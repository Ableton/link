/* Copyright 2016, Ableton AG, Berlin. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */

#pragma once

#include <ableton/discovery/Service.hpp>
#include <ableton/link/CircularFifo.hpp>
#include <ableton/link/ClientSessionTimelines.hpp>
#include <ableton/link/Gateway.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/Peers.hpp>
#include <ableton/link/SessionState.hpp>
#include <ableton/link/Sessions.hpp>
#include <ableton/link/StartStopState.hpp>
#include <mutex>

namespace ableton
{
namespace link
{
namespace detail
{

template <typename Clock>
GhostXForm initXForm(const Clock& clock)
{
  // Make the current time map to a ghost time of 0 with ghost time
  // increasing at the same rate as clock time
  return {1.0, -clock.micros()};
}

// The timespan in which local modifications to the timeline will be
// preferred over any modifications coming from the network.
const auto kLocalModGracePeriod = std::chrono::milliseconds(1000);
const auto kRtHandlerFallbackPeriod = kLocalModGracePeriod / 2;

inline StartStopState selectPreferredStartStopState(
  const StartStopState currentStartStopState, const StartStopState startStopState)
{
  return startStopState.time > currentStartStopState.time ? startStopState
                                                          : currentStartStopState;
}

} // namespace detail

// function types corresponding to the Controller callback type params
using PeerCountCallback = std::function<void(std::size_t)>;
using TempoCallback = std::function<void(ableton::link::Tempo)>;
using StartStopStateCallback = std::function<void(bool)>;


// The main Link controller
template <typename PeerCountCallback,
  typename TempoCallback,
  typename StartStopStateCallback,
  typename Clock,
  typename IoContext>
class Controller
{
public:
  Controller(Tempo tempo,
    PeerCountCallback peerCallback,
    TempoCallback tempoCallback,
    StartStopStateCallback startStopStateCallback,
    Clock clock,
    util::Injected<IoContext> io)
    : mTempoCallback(std::move(tempoCallback))
    , mStartStopStateCallback(std::move(startStopStateCallback))
    , mClock(std::move(clock))
    , mNodeId(NodeId::random())
    , mSessionId(mNodeId)
    , mGhostXForm(detail::initXForm(mClock))
    , mSessionTimeline(clampTempo({tempo, Beats{0.}, std::chrono::microseconds{0}}))
    , mClientTimeline({mSessionTimeline.tempo, Beats{0.},
        mGhostXForm.ghostToHost(std::chrono::microseconds{0})})
    , mClientStartStopState()
    , mRtClientTimeline(mClientTimeline)
    , mRtClientTimelineTimestamp(0)
    , mSessionStartStopState()
    , mRtClientStartStopState()
    , mRtClientStartStopStateTimestamp(0)
    , mSessionPeerCounter(*this, std::move(peerCallback))
    , mEnabled(false)
    , mStartStopSyncEnabled(false)
    , mIo(std::move(io))
    , mRtSessionStateSetter(*this)
    , mPeers(util::injectRef(*mIo),
        std::ref(mSessionPeerCounter),
        SessionTimelineCallback{*this},
        SessionStartStopStateCallback{*this})
    , mSessions({mSessionId, mSessionTimeline, {mGhostXForm, mClock.micros()}},
        util::injectRef(mPeers),
        MeasurePeer{*this},
        JoinSessionCallback{*this},
        util::injectRef(*mIo),
        mClock)
    , mDiscovery(
        std::make_pair(NodeState{mNodeId, mSessionId, mSessionTimeline, StartStopState{}},
          mGhostXForm),
        GatewayFactory{*this},
        util::injectVal(mIo->clone(UdpSendExceptionHandler{*this})))
  {
  }

  Controller(const Controller&) = delete;
  Controller(Controller&&) = delete;

  Controller& operator=(const Controller&) = delete;
  Controller& operator=(Controller&&) = delete;

  void enable(const bool bEnable)
  {
    const bool bWasEnabled = mEnabled.exchange(bEnable);
    if (bWasEnabled != bEnable)
    {
      mIo->async([this, bEnable] {
        if (bEnable)
        {
          // Always reset when first enabling to avoid hijacking
          // tempo in existing sessions
          resetState();
        }
        mDiscovery.enable(bEnable);
      });
    }
  }

  bool isEnabled() const
  {
    return mEnabled;
  }

  void enableStartStopSync(const bool bEnable)
  {
    mStartStopSyncEnabled = bEnable;
  }

  bool isStartStopSyncEnabled() const
  {
    return mStartStopSyncEnabled;
  }

  std::size_t numPeers() const
  {
    return mSessionPeerCounter.mSessionPeerCount;
  }

  // Get the current Link session state. Thread-safe but may block, so
  // it cannot be used from audio thread.
  SessionState sessionState() const
  {
    std::lock_guard<std::mutex> lock(mClientSessionStateGuard);
    return SessionState{mClientTimeline, mClientStartStopState};
  }

  // Set the session state to be used, starting at the given
  // time. Thread-safe but may block, so it cannot be used from audio thread.
  void setSessionState(IncomingSessionState newSessionState)
  {
    {
      std::lock_guard<std::mutex> lock(mClientSessionStateGuard);
      if (newSessionState.timeline)
      {
        *newSessionState.timeline = clampTempo(*newSessionState.timeline);
        mClientTimeline = *newSessionState.timeline;
      }
      if (newSessionState.startStopState)
      {
        // Prevent updating client start stop state with an outdated start stop state
        *newSessionState.startStopState = detail::selectPreferredStartStopState(
          mClientStartStopState, *newSessionState.startStopState);
        mClientStartStopState = *newSessionState.startStopState;
      }
    }

    mIo->async(
      [this, newSessionState] { handleSessionStateFromClient(newSessionState); });
  }

  // Non-blocking session state access for a realtime context. NOT
  // thread-safe. Must not be called from multiple threads
  // concurrently and must not be called concurrently with setSessionStateRtSafe.
  SessionState sessionStateRtSafe() const
  {
    // Respect the session timing guard and the client session state guard but don't
    // block on them. If we can't access one or both because of concurrent modification
    // we fall back to our cached version of the timeline and/or start stop state.

    if (!mRtSessionStateSetter.hasPendingSessionStates())
    {
      const auto now = mClock.micros();
      if (now - mRtClientTimelineTimestamp > detail::kLocalModGracePeriod)
      {
        if (mSessionTimingGuard.try_lock())
        {
          const auto clientTimeline = updateClientTimelineFromSession(
            mRtClientTimeline, mSessionTimeline, now, mGhostXForm);

          mSessionTimingGuard.unlock();

          if (clientTimeline != mRtClientTimeline)
          {
            mRtClientTimeline = clientTimeline;
          }
        }
      }

      if (now - mRtClientStartStopStateTimestamp > detail::kLocalModGracePeriod)
      {
        if (mClientSessionStateGuard.try_lock())
        {
          const auto startStopState = mClientStartStopState;

          mClientSessionStateGuard.unlock();

          if (startStopState != mRtClientStartStopState)
          {
            mRtClientStartStopState = startStopState;
          }
        }
      }
    }

    return {mRtClientTimeline, mRtClientStartStopState};
  }

  // should only be called from the audio thread
  void setSessionStateRtSafe(IncomingSessionState newSessionState)
  {
    if (newSessionState.timeline)
    {
      *newSessionState.timeline = clampTempo(*newSessionState.timeline);
    }

    if (newSessionState.startStopState)
    {
      // Prevent updating client start stop state with an outdated start stop state
      *newSessionState.startStopState = detail::selectPreferredStartStopState(
        mRtClientStartStopState, *newSessionState.startStopState);
    }

    // This will fail in case the Fifo in the RtSessionStateSetter is full. This indicates
    // a very high rate of calls to the setter. In this case we ignore one value because
    // we expect the setter to be called again soon.
    if (mRtSessionStateSetter.tryPush(newSessionState))
    {
      const auto now = mClock.micros();
      // Cache the new timeline and StartStopState for serving back to the client
      if (newSessionState.timeline)
      {
        // Cache the new timeline and StartStopState for serving back to the client
        mRtClientTimeline = *newSessionState.timeline;
        mRtClientTimelineTimestamp = makeRtTimestamp(now);
      }
      if (newSessionState.startStopState)
      {
        mRtClientStartStopState = *newSessionState.startStopState;
        mRtClientStartStopStateTimestamp = makeRtTimestamp(now);
      }
    }
  }

private:
  std::chrono::microseconds makeRtTimestamp(const std::chrono::microseconds now) const
  {
    return isEnabled() ? now : std::chrono::microseconds(0);
  }

  void updateDiscovery()
  {
    // Push the change to the discovery service
    mDiscovery.updateNodeState(std::make_pair(
      NodeState{mNodeId, mSessionId, mSessionTimeline, mSessionStartStopState},
      mGhostXForm));
  }

  void updateSessionTiming(const Timeline newTimeline, const GhostXForm newXForm)
  {
    const auto oldTimeline = mSessionTimeline;
    const auto oldXForm = mGhostXForm;

    if (oldTimeline != newTimeline || oldXForm != newXForm)
    {
      {
        std::lock_guard<std::mutex> lock(mSessionTimingGuard);
        mSessionTimeline = newTimeline;
        mGhostXForm = newXForm;
      }

      // Update the client timeline based on the new session timing data
      {
        std::lock_guard<std::mutex> lock(mClientSessionStateGuard);
        mClientTimeline = updateClientTimelineFromSession(
          mClientTimeline, mSessionTimeline, mClock.micros(), mGhostXForm);
      }

      if (oldTimeline.tempo != newTimeline.tempo)
      {
        mTempoCallback(newTimeline.tempo);
      }
    }
  }

  void handleTimelineFromSession(SessionId id, Timeline timeline)
  {
    debug(mIo->log()) << "Received timeline with tempo: " << timeline.tempo.bpm()
                      << " for session: " << id;
    updateSessionTiming(
      mSessions.sawSessionTimeline(std::move(id), std::move(timeline)), mGhostXForm);
    updateDiscovery();
  }

  void resetSessionStartStopState()
  {
    mSessionStartStopState = StartStopState{};
  }

  void handleStartStopStateFromSession(SessionId sessionId, StartStopState startStopState)
  {
    debug(mIo->log()) << "Received start stop state. isPlaying: "
                      << startStopState.isPlaying
                      << ", time: " << startStopState.time.count()
                      << " for session: " << sessionId;
    if (sessionId == mSessionId && startStopState.time > mSessionStartStopState.time)
    {
      mSessionStartStopState = startStopState;

      // Always propagate the session start stop state so even a client that doesn't have
      // the feature enabled can function as a relay.
      updateDiscovery();

      if (mStartStopSyncEnabled)
      {
        {
          std::lock_guard<std::mutex> lock(mClientSessionStateGuard);
          mClientStartStopState = StartStopState{
            startStopState.isPlaying, mGhostXForm.ghostToHost(startStopState.time)};
        }
        mStartStopStateCallback(startStopState.isPlaying);
      }
    }
  }

  void handleSessionStateFromClient(const IncomingSessionState sessionState)
  {
    auto mustUpdateDiscovery = false;

    if (sessionState.timeline)
    {
      auto sessionTimeline = updateSessionTimelineFromClient(
        mSessionTimeline, *sessionState.timeline, sessionState.timestamp, mGhostXForm);

      mSessions.resetTimeline(sessionTimeline);
      mPeers.setSessionTimeline(mSessionId, sessionTimeline);
      updateSessionTiming(std::move(sessionTimeline), mGhostXForm);

      mustUpdateDiscovery = true;
    }

    if (mStartStopSyncEnabled && sessionState.startStopState)
    {
      // Prevent updating the session start stop state with an outdated start stop state
      const auto newGhostTime =
        mGhostXForm.hostToGhost(sessionState.startStopState->time);
      if (newGhostTime > mSessionStartStopState.time)
      {
        mSessionStartStopState =
          StartStopState{sessionState.startStopState->isPlaying, newGhostTime};

        mustUpdateDiscovery = true;
      }
    }

    if (mustUpdateDiscovery)
    {
      updateDiscovery();
    }

    if (sessionState.startStopState)
    {
      mStartStopStateCallback(sessionState.startStopState->isPlaying);
    }
  }

  void handleRtSessionState(IncomingSessionState sessionState)
  {
    {
      std::lock_guard<std::mutex> lock(mClientSessionStateGuard);
      if (sessionState.timeline)
      {
        mClientTimeline = *sessionState.timeline;
      }
      if (sessionState.startStopState)
      {
        // Prevent updating client start stop state with an outdated start stop state
        *sessionState.startStopState = detail::selectPreferredStartStopState(
          mClientStartStopState, *sessionState.startStopState);
        mClientStartStopState = *sessionState.startStopState;
      }
    }

    handleSessionStateFromClient(sessionState);
  }

  void joinSession(const Session& session)
  {
    const bool sessionIdChanged = mSessionId != session.sessionId;
    mSessionId = session.sessionId;

    // Prevent passing the start stop state of the previous session to the new one.
    if (sessionIdChanged)
    {
      resetSessionStartStopState();
    }

    updateSessionTiming(session.timeline, session.measurement.xform);
    updateDiscovery();

    if (sessionIdChanged)
    {
      debug(mIo->log()) << "Joining session " << session.sessionId << " with tempo "
                        << session.timeline.tempo.bpm();
      mSessionPeerCounter();
    }
  }

  void resetState()
  {
    mNodeId = NodeId::random();
    mSessionId = mNodeId;

    const auto xform = detail::initXForm(mClock);
    const auto hostTime = -xform.intercept;
    // When creating the new timeline, make it continuous by finding
    // the beat on the old session timeline corresponding to the
    // current host time and mapping it to the new ghost time
    // representation of the current host time.
    const auto newTl = Timeline{mSessionTimeline.tempo,
      mSessionTimeline.toBeats(mGhostXForm.hostToGhost(hostTime)),
      xform.hostToGhost(hostTime)};

    resetSessionStartStopState();

    updateSessionTiming(newTl, xform);
    updateDiscovery();

    mSessions.resetSession({mNodeId, newTl, {xform, hostTime}});
    mPeers.resetPeers();
  }

  struct SessionTimelineCallback
  {
    void operator()(SessionId id, Timeline timeline)
    {
      mController.handleTimelineFromSession(std::move(id), std::move(timeline));
    }

    Controller& mController;
  };

  struct RtSessionStateSetter
  {
    using CallbackDispatcher =
      typename IoContext::template LockFreeCallbackDispatcher<std::function<void()>,
        std::chrono::milliseconds>;

    RtSessionStateSetter(Controller& controller)
      : mController(controller)
      , mHasPendingSessionStates(false)
      , mCallbackDispatcher(
          [this] { processPendingSessionStates(); }, detail::kRtHandlerFallbackPeriod)
    {
    }

    bool tryPush(const IncomingSessionState sessionState)
    {
      mHasPendingSessionStates = true;
      const auto success = mSessionStateFifo.push(sessionState);
      if (success)
      {
        mCallbackDispatcher.invoke();
      }
      return success;
    }

    bool hasPendingSessionStates() const
    {
      return mHasPendingSessionStates;
    }

  private:
    void processPendingSessionStates()
    {
      auto result = mSessionStateFifo.clearAndPopLast();

      if (result)
      {
        const auto sessionState = std::move(*result);
        mController.mIo->async([this, sessionState]() {
          mController.handleRtSessionState(sessionState);
          mHasPendingSessionStates = false;
        });
      }
    }

    Controller& mController;
    // Assuming a wake up time of one ms for the threads owned by the CallbackDispatcher
    // and the ioService, buffering 16 session states allows to set eight session states
    // per ms.
    static const std::size_t kBufferSize = 16;
    CircularFifo<IncomingSessionState, kBufferSize> mSessionStateFifo;
    std::atomic<bool> mHasPendingSessionStates;
    CallbackDispatcher mCallbackDispatcher;
  };

  struct SessionStartStopStateCallback
  {
    void operator()(SessionId sessionId, StartStopState startStopState)
    {
      mController.handleStartStopStateFromSession(sessionId, startStopState);
    }

    Controller& mController;
  };

  struct SessionPeerCounter
  {
    SessionPeerCounter(Controller& controller, PeerCountCallback callback)
      : mController(controller)
      , mCallback(std::move(callback))
      , mSessionPeerCount(0)
    {
    }

    void operator()()
    {
      const auto count =
        mController.mPeers.uniqueSessionPeerCount(mController.mSessionId);
      const auto oldCount = mSessionPeerCount.exchange(count);
      if (oldCount != count)
      {
        if (count == 0)
        {
          // When the count goes down to zero, completely reset the
          // state, effectively founding a new session
          mController.resetState();
        }
        mCallback(count);
      }
    }

    Controller& mController;
    PeerCountCallback mCallback;
    std::atomic<std::size_t> mSessionPeerCount;
  };

  struct MeasurePeer
  {
    template <typename Peer, typename Handler>
    void operator()(Peer peer, Handler handler)
    {
      using It = typename Discovery::ServicePeerGateways::GatewayMap::iterator;
      using ValueType = typename Discovery::ServicePeerGateways::GatewayMap::value_type;
      mController.mDiscovery.withGatewaysAsync([peer, handler](It begin, const It end) {
        const auto addr = peer.second;
        const auto it = std::find_if(
          begin, end, [&addr](const ValueType& vt) { return vt.first == addr; });
        if (it != end)
        {
          it->second->measurePeer(std::move(peer.first), std::move(handler));
        }
        else
        {
          // invoke the handler with an empty result if we couldn't
          // find the peer's gateway
          handler(GhostXForm{});
        }
      });
    }

    Controller& mController;
  };

  struct JoinSessionCallback
  {
    void operator()(Session session)
    {
      mController.joinSession(std::move(session));
    }

    Controller& mController;
  };

  using IoType = typename util::Injected<IoContext>::type;

  using ControllerPeers = Peers<IoType&,
    std::reference_wrapper<SessionPeerCounter>,
    SessionTimelineCallback,
    SessionStartStopStateCallback>;

  using ControllerGateway =
    Gateway<typename ControllerPeers::GatewayObserver, Clock, IoType&>;
  using GatewayPtr = std::shared_ptr<ControllerGateway>;

  struct GatewayFactory
  {
    GatewayPtr operator()(std::pair<NodeState, GhostXForm> state,
      util::Injected<IoType&> io,
      const asio::ip::address& addr)
    {
      if (addr.is_v4())
      {
        return GatewayPtr{new ControllerGateway{std::move(io), addr.to_v4(),
          util::injectVal(makeGatewayObserver(mController.mPeers, addr)),
          std::move(state.first), std::move(state.second), mController.mClock}};
      }
      else
      {
        throw std::runtime_error("Could not create peer gateway on non-ipV4 address");
      }
    }

    Controller& mController;
  };

  struct UdpSendExceptionHandler
  {
    using Exception = discovery::UdpSendException;

    void operator()(const Exception& exception)
    {
      mController.mDiscovery.repairGateway(exception.interfaceAddr);
    }

    Controller& mController;
  };

  TempoCallback mTempoCallback;
  StartStopStateCallback mStartStopStateCallback;
  Clock mClock;
  NodeId mNodeId;
  SessionId mSessionId;

  // Mutex that controls access to mGhostXForm and mSessionTimeline
  mutable std::mutex mSessionTimingGuard;
  GhostXForm mGhostXForm;
  Timeline mSessionTimeline;

  mutable std::mutex mClientSessionStateGuard;
  Timeline mClientTimeline;
  StartStopState mClientStartStopState;

  mutable Timeline mRtClientTimeline;
  std::chrono::microseconds mRtClientTimelineTimestamp;

  StartStopState mSessionStartStopState;

  mutable StartStopState mRtClientStartStopState;
  std::chrono::microseconds mRtClientStartStopStateTimestamp;

  SessionPeerCounter mSessionPeerCounter;

  std::atomic<bool> mEnabled;

  std::atomic<bool> mStartStopSyncEnabled;

  util::Injected<IoContext> mIo;

  RtSessionStateSetter mRtSessionStateSetter;

  ControllerPeers mPeers;

  using ControllerSessions = Sessions<ControllerPeers&,
    MeasurePeer,
    JoinSessionCallback,
    typename util::Injected<IoContext>::type&,
    Clock>;
  ControllerSessions mSessions;

  using Discovery =
    discovery::Service<std::pair<NodeState, GhostXForm>, GatewayFactory, IoContext>;
  Discovery mDiscovery;
};

} // namespace link
} // namespace ableton
