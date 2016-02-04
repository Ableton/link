// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/Service.hpp>
#include <ableton/discovery/asio/AsioService.hpp>
#include <ableton/link/ClientBeatTimeline.hpp>
#include <ableton/link/Gateway.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/Peers.hpp>
#include <ableton/link/Sessions.hpp>
#include <ableton/discovery/util/PooledHandlerService.hpp>

namespace ableton
{
namespace link
{
namespace detail
{

static const double kMinTempo = 20.0;
static const double kMaxTempo = 999.0;

inline NodeState initState(Tempo tempo)
{
  auto id = NodeId::random();
  return {id, id, {std::move(tempo), Beats{0.}, std::chrono::microseconds{0}}};
}

template <typename Clock>
inline GhostXForm initXForm(const Clock& clock)
{
  // Make the current time map to a ghost time of 0 with ghost time
  // increasing at the same rate as clock time
  return {1.0, -micros(clock)};
}

inline Tempo clampTempo(const Tempo tempo)
{
  return (std::min)((std::max)(tempo, Tempo{kMinTempo}), Tempo{kMaxTempo});
}

} // detail

// function types corresponding to the Controller callback type params
using PeerCountCallback = std::function<void (std::size_t)>;
using TempoCallback = std::function<void (ableton::link::Tempo)>;

// The main Link controller
template <
  typename PeerCountCallback,
  typename TempoCallback,
  typename Clock,
  typename Platform,
  typename Log,
  typename IoService>
struct Controller
{
  using Ticks = typename Clock::Ticks;

  Controller(
    Tempo tempo,
    Beats quantum,
    PeerCountCallback peerCallback,
    TempoCallback tempoCallback,
    Clock clock,
    util::Injected<Platform> platform,
    util::Injected<Log> log,
    util::Injected<IoService> ioService)
    : mLog(std::move(log))
    , mTempoCallback(std::move(tempoCallback))
    , mClock(std::move(clock))
    , mState(detail::initState(detail::clampTempo(std::move(tempo))))
    , mGhostXForm(detail::initXForm(mClock))
    , mClientBeatTimeline(mState.timeline, mGhostXForm)
    , mSessionPeerCounter(*this, std::move(peerCallback))
    , mEnabled(false)
    , mQuantum(microBeats(quantum))
    , mIo(std::move(ioService), mLog)
    , mPeers(
        util::injectRef(*mIo.mIo), std::ref(mSessionPeerCounter),
        SessionTimelineCallback{*this})
    , mSessions(
        {ident(mState), mState.timeline, {mGhostXForm, micros(mClock)}}, std::ref(mPeers),
        MeasurePeer{*this}, JoinSessionCallback{*this}, util::injectRef(*mIo.mIo),
        mClock, mLog)
    , mDiscovery(
        GatewayFactory{*this}, std::move(platform),
        util::injectVal(channel(*mLog, "discovery")))
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
      mIo.post([this, bEnable] {
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

  std::size_t numPeers() const
  {
    return mSessionPeerCounter.mSessionPeerCount;
  }

  Beats quantum() const
  {
    return Beats{mQuantum};
  }

  void setQuantum(const Beats quantum)
  {
    mQuantum = microBeats(quantum);
  }

  friend Tempo tempo(const Controller& controller)
  {
    return tempo(controller.mClientBeatTimeline);
  }

  void setTempo(const Tempo newTempo, std::chrono::microseconds atTime)
  {
    // Restrict tempo modifications to the range [now, now + 1 second)
    const auto now = micros(mClock);
    atTime = (std::max)(now, atTime);

    if (atTime < now + std::chrono::seconds(1))
    {
      const auto newTl =
        updateTempo(mClientBeatTimeline, detail::clampTempo(newTempo), atTime);

      mIo.post([this, newTl] {
          mMutex.lock();
          mState.timeline = newTl;
          mMutex.unlock();

          mSessions.resetTimeline(newTl);
          broadcastStateChange();
          mTempoCallback(newTl.tempo);
        });
    }
  }

  friend Beats timeToBeats(
    const Controller& controller,
    const std::chrono::microseconds time)
  {
    return hostToBeats(controller.mClientBeatTimeline, time);
  }

  friend std::chrono::microseconds beatsToTime(
    const Controller& controller,
    const Beats beats)
  {
    return beatsToHost(controller.mClientBeatTimeline, beats);
  }

  friend Beats resetBeats(
    Controller& controller,
    const Beats beats,
    const std::chrono::microseconds atTime)
  {
    if (controller.isEnabled() && controller.numPeers() > 0)
    {
      return resetBeats(
        controller.mClientBeatTimeline, beats, atTime, Beats{controller.mQuantum});
    }
    else
    {
      // When no peers are connected, we are free to modify the
      // timeline and avoid quantization delay
      const auto newTl = updateOrigin(controller.mClientBeatTimeline, beats, atTime);
      controller.mIo.post([&controller, newTl] {
        controller.mMutex.lock();
        controller.mState.timeline = newTl;
        controller.mMutex.unlock();

        controller.mSessions.resetTimeline(newTl);
        controller.broadcastStateChange();
      });

      return beats;
    }
  }

  friend void forceBeats(
    Controller& controller,
    const Tempo tempo,
    const Beats beats,
    const std::chrono::microseconds atTime)
  {
    controller.postTimeline(
      forceBeats(
        controller.mClientBeatTimeline, detail::clampTempo(tempo), beats, atTime,
        Beats{controller.mQuantum}));
  }

  friend Beats phase(
    Controller& controller,
    const Beats beats,
    const Beats quantum)
  {
    return phase(controller.mClientBeatTimeline, beats, quantum);
  }

private:
  // TODO: Use for resetBeats and setTempo as well. Don't do it now in
  // order to minimize any changes during release process
  void postTimeline(const Timeline tl)
  {
    mIo.post([this, tl] {
        mMutex.lock();
        const bool sameTempo = mState.timeline.tempo == tl.tempo;
        mState.timeline = tl;
        mMutex.unlock();

        mSessions.resetTimeline(tl);
        broadcastStateChange();
        if (!sameTempo)
        {
          mTempoCallback(tl.tempo);
        }
      });
  }

  void broadcastStateChange()
  {
    using It = typename Discovery::PeerGateways::GatewayMap::iterator;
    mDiscovery.withGatewaysAsync([this](It begin, const It end) {
        for (; begin != end; ++begin)
        {
          try
          {
            broadcastState(*begin->second);
          }
          catch (const std::runtime_error& err)
          {
            info(*mLog) << "State broadcast failed on gateway: " << err.what();
          }
        }
      });
  }

  void handleSessionTimeline(SessionId id, Timeline timeline)
  {
    using namespace std;
    using namespace std::rel_ops;

    debug(*mLog) << "Received timeline with tempo: " << bpm(timeline.tempo) <<
      " for session: " << id;

    auto newTimeline = mSessions.sawSessionTimeline(move(id), move(timeline));
    // Unconditionally update the timeline to the new version since we
    // have to acquire the lock anyway
    mMutex.lock();
    const auto oldTimeline = mState.timeline;
    mState.timeline = newTimeline;
    mMutex.unlock();

    // Notify peers and clients if the timeline was changed
    if (newTimeline != oldTimeline)
    {
      updateTimeline(mClientBeatTimeline, newTimeline);
      broadcastStateChange();
      if (newTimeline.tempo != oldTimeline.tempo)
      {
        mTempoCallback(move(newTimeline.tempo));
      }
    }
  }

  void joinSession(const Session& session)
  {
    using namespace std;
    using namespace std::chrono;

    mMutex.lock();
    bool sessionIdChanged = mState.sessionId != session.sessionId;
    mState.sessionId = session.sessionId;
    bool tempoChanged = mState.timeline.tempo != session.timeline.tempo;
    mState.timeline = session.timeline;
    mGhostXForm = session.measurement.xform;
    mMutex.unlock();

    updateSession(
      mClientBeatTimeline, session.timeline, session.measurement.xform, micros(mClock),
      Beats{mQuantum});

    if (sessionIdChanged)
    {
      debug(*mLog) << "Joining session " << session.sessionId <<
        " with tempo " << bpm(session.timeline.tempo);
      broadcastStateChange();
      mSessionPeerCounter();
    }

    if (tempoChanged)
    {
      mTempoCallback(session.timeline.tempo);
    }
  }

  void resetState()
  {
    const auto newId = NodeId::random();
    const auto xform = detail::initXForm(mClock);
    const auto hostTime = -xform.intercept;
    const auto newTl = reset(mClientBeatTimeline, hostTime, xform);
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mState = {newId, newId, newTl};
      mGhostXForm = xform;
    }
    mSessions.resetSession({newId, newTl, {xform, hostTime}});
    resetPeers(mPeers);
  }

  struct SessionTimelineCallback
  {
    void operator()(SessionId id, Timeline timeline)
    {
      mController.handleSessionTimeline(std::move(id), std::move(timeline));
    }

    Controller& mController;
  };

  struct StateQuery
  {
    NodeState operator()()
    {
      std::lock_guard<std::mutex> lock(mController.mMutex);
      return mController.mState;
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
      const auto state = StateQuery{mController}();
      const auto count = uniqueSessionPeerCount(mController.mPeers, state.sessionId);
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

  struct GhostXFormQuery
  {
    GhostXForm operator()()
    {
      std::lock_guard<std::mutex> lock(mController.mMutex);
      return mController.mGhostXForm;
    }

    Controller& mController;
  };

  struct MeasurePeer
  {
    template <typename Peer, typename Handler>
    void operator()(Peer peer, Handler handler)
    {
      using It = typename Discovery::PeerGateways::GatewayMap::iterator;
      using ValueType = typename Discovery::PeerGateways::GatewayMap::value_type;
      mController.mDiscovery.withGatewaysAsync([peer, handler](It begin, const It end) {
          const auto addr = peer.second;
          const auto it = std::find_if(begin, end, [&addr](const ValueType& vt) {
              return vt.first == addr;
            });
          if (it != end)
          {
            measurePeer(*it->second, std::move(peer.first), std::move(handler));
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

  using PooledIoService = util::PooledHandlerService<IoService, Log>;

  using Peers = Peers<
    typename util::Injected<IoService>::type&, std::reference_wrapper<SessionPeerCounter>,
    SessionTimelineCallback>;

  using Gateway =
    Gateway<typename Peers::GatewayObserver, StateQuery, GhostXFormQuery, Clock, Log>;
  using GatewayPtr = std::shared_ptr<Gateway>;

  struct GatewayFactory
  {
    GatewayPtr operator()(util::AsioService& io, const asio::ip::address& addr)
    {
      if (addr.is_v4())
      {
        return GatewayPtr{
          new Gateway{
            io, addr.to_v4(),
            makeGatewayObserver(mController.mPeers, addr),
            StateQuery{mController},
            GhostXFormQuery{mController},
            mController.mClock,
            channel(*mController.mLog, "gateway@" + addr.to_string())
          }
        };
      }
      else
      {
        throw std::runtime_error("Could not create peer gateway on non-ipV4 address");
      }
    }

    Controller& mController;
  };

  util::Injected<Log> mLog;

  std::mutex mMutex;
  TempoCallback mTempoCallback;
  Clock mClock;
  NodeState mState;
  GhostXForm mGhostXForm;
  ClientBeatTimeline mClientBeatTimeline;
  SessionPeerCounter mSessionPeerCounter;

  std::atomic<bool> mEnabled;
  std::atomic<std::int64_t> mQuantum;

  PooledIoService mIo;

  Peers mPeers;

  using Sessions = Sessions<
    std::reference_wrapper<Peers>, MeasurePeer, JoinSessionCallback,
    typename util::Injected<IoService>::type&, Clock, Log>;
  Sessions mSessions;

  using Discovery = discovery::Service<GatewayFactory, Platform, Log>;
  Discovery mDiscovery;
};

} // namespace link
} // namespace ableton
