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
#include <ableton/link/ClientSessionTimelines.hpp>
#include <ableton/link/Gateway.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/Peers.hpp>
#include <ableton/link/SessionState.hpp>
#include <ableton/link/Sessions.hpp>
#include <ableton/link/StartStopState.hpp>
#include <ableton/util/TripleBuffer.hpp>

#include <chrono>
#include <condition_variable>
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

template <typename Clock>
inline SessionState initSessionState(const Tempo tempo, const Clock& clock)
{
  using namespace std::chrono;
  return {clampTempo(Timeline{tempo, Beats{0.}, microseconds{0}}),
          StartStopState{false, Beats{0.}, microseconds{0}},
          std::nullopt,
          initXForm(clock)};
}

inline ClientState initClientState(const SessionState& sessionState, SessionId sessionId)
{
  const auto hostTime = sessionState.ghostXForm.ghostToHost(std::chrono::microseconds{0});
  return {
    Timeline{sessionState.timeline.tempo, sessionState.timeline.beatOrigin, hostTime},
    std::move(sessionId),
    ClientStartStopState{sessionState.startStopState.isPlaying, hostTime, hostTime},
    std::nullopt};
}

inline RtClientState initRtClientState(const ClientState& clientState)
{
  using namespace std::chrono;
  return {clientState.timeline,
          clientState.timelineSessionId,
          clientState.startStopState,
          clientState.arrangementTimeline,
          microseconds{0},
          microseconds{0},
          microseconds{0}};
}

// The timespan in which local modifications to the timeline will be
// preferred over any modifications coming from the network.
const auto kLocalModGracePeriod = std::chrono::milliseconds(1000);
const auto kRtHandlerFallbackPeriod = kLocalModGracePeriod / 2;

inline ClientStartStopState selectPreferredStartStopState(
  const ClientStartStopState currentStartStopState,
  const ClientStartStopState startStopState)
{
  return startStopState.timestamp > currentStartStopState.timestamp
           ? startStopState
           : currentStartStopState;
}

inline bool startStopCarriesArrangement(
  const std::optional<ArrangementTimeline>& arrangementTimeline,
  const StartStopState& startStopState)
{
  return arrangementTimeline
         && arrangementTimeline->timestamp >= startStopState.timestamp;
}

inline ClientStartStopState mapStartStopStateFromSessionToClient(
  const StartStopState& sessionStartStopState,
  const Timeline& sessionTimeline,
  const GhostXForm& xForm)
{
  const auto time =
    xForm.ghostToHost(sessionTimeline.fromBeats(sessionStartStopState.beats));
  const auto timestamp = xForm.ghostToHost(sessionStartStopState.timestamp);
  return ClientStartStopState{sessionStartStopState.isPlaying, time, timestamp};
}

inline StartStopState mapStartStopStateFromClientToSession(
  const ClientStartStopState& clientStartStopState,
  const Timeline& sessionTimeline,
  const GhostXForm& xForm)
{
  const auto sessionBeats =
    sessionTimeline.toBeats(xForm.hostToGhost(clientStartStopState.time));
  const auto timestamp = xForm.hostToGhost(clientStartStopState.timestamp);
  return StartStopState{clientStartStopState.isPlaying, sessionBeats, timestamp};
}

inline ArrangementTimeline mapArrangementTimelineFromClientToSession(
  const ClientArrangementTimeline& client,
  const Timeline& sessionTimeline,
  const Timeline& clientTimeline,
  const GhostXForm& xForm)
{
  const auto ghostTimestamp = xForm.hostToGhost(client.timestamp);
  const auto magnitudeOffset =
    sessionTimeline.toBeats(ghostTimestamp) - clientTimeline.toBeats(client.timestamp);
  const auto launchBeats = sessionTimeline.toBeats(xForm.hostToGhost(client.launchTime));
  return {client.beatOrigin + magnitudeOffset, launchBeats, ghostTimestamp};
}

inline ClientArrangementTimeline mapArrangementTimelineFromSessionToClient(
  const ArrangementTimeline& session,
  const Timeline& sessionTimeline,
  const Timeline& clientTimeline,
  const GhostXForm& xForm)
{
  const auto hostTimestamp = xForm.ghostToHost(session.timestamp);
  const auto magnitudeOffset =
    sessionTimeline.toBeats(session.timestamp) - clientTimeline.toBeats(hostTimestamp);
  const auto launchTime =
    xForm.ghostToHost(sessionTimeline.fromBeats(session.launchBeats));
  return {session.beatOrigin - magnitudeOffset, launchTime, hostTimestamp};
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
          typename Random,
          typename IoContext,
          typename SessionController>
class Controller
{
public:
  Controller(Tempo tempo,
             PeerCountCallback peerCallback,
             TempoCallback tempoCallback,
             StartStopStateCallback startStopStateCallback,
             Clock clock)
    : mTempoCallback(std::move(tempoCallback))
    , mStartStopStateCallback(std::move(startStopStateCallback))
    , mClock(std::move(clock))
    , mNodeId(NodeId::random<Random>())
    , mSessionId(mNodeId)
    , mSessionState(detail::initSessionState(tempo, mClock))
    , mClientState(detail::initClientState(mSessionState, mSessionId))
    , mLastIsPlayingForStartStopStateCallback(false)
    , mRtClientState(detail::initRtClientState(mClientState.get()))
    , mHasPendingRtClientStates(false)
    , mpSessionController{static_cast<SessionController*>(this)}
    , mSessionPeerCounter(*this, std::move(peerCallback))
    , mEnabled(false)
    , mStartStopSyncEnabled(false)
    , mArrangementSyncEnabled(false)
    , mIo(IoContext{UdpSendExceptionHandler{this}})
    , mPeers(util::injectRef(*mIo),
             SessionMembershipCallback{this},
             SessionTimelineCallback{*this},
             SessionStartStopStateCallback{*this},
             SawAudioEndpointCallback{this})
    , mSessions(
        {mSessionId, mSessionState.timeline, {mSessionState.ghostXForm, mClock.micros()}},
        util::injectRef(mPeers),
        MeasurePeer{this},
        JoinSessionCallback{this},
        util::injectRef(*mIo),
        mClock)
    , mDiscovery(std::make_pair(NodeState{mNodeId,
                                          mSessionId,
                                          mSessionState.timeline,
                                          mSessionState.startStopState,
                                          mSessionState.arrangementTimeline},
                                mSessionState.ghostXForm),
                 util::injectVal(GatewayFactory{this}),
                 util::injectRef(*mIo))
    , mRtClientStateSetter(*this)
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
      mRtClientStateSetter.invoke();
    }
  }

  bool isEnabled() const { return mEnabled; }

  void enableStartStopSync(const bool bEnable)
  {
    if (!bEnable)
    {
      enableArrangementSync(false);
    }
    mStartStopSyncEnabled = bEnable;
  }

  bool isStartStopSyncEnabled() const { return mStartStopSyncEnabled; }

  void enableArrangementSync(const bool bEnable)
  {
    if (bEnable)
    {
      mStartStopSyncEnabled = true;
    }

    if (mArrangementSyncEnabled && !bEnable)
    {
      mClientState.invalidateArrangement();
    }
    mArrangementSyncEnabled = bEnable;
  }

  bool isArrangementSyncEnabled() const { return mArrangementSyncEnabled; }

  std::size_t numPeers() const { return mSessionPeerCounter.mSessionPeerCount; }

  // Get the current Link client state. Thread-safe but may block, so
  // it cannot be used from audio thread.
  ClientState clientState() const { return mClientState.resolve(mClientState.get()); }

  // Set the client state to be used, starting at the given time.
  // Thread-safe but may block, so it cannot be used from audio thread.
  void setClientState(IncomingClientState newClientState)
  {
    mClientState.update(
      [&](ClientState& clientState)
      {
        if (newClientState.timeline)
        {
          *newClientState.timeline = clampTempo(*newClientState.timeline);
          clientState.timeline = *newClientState.timeline;
        }
        if (newClientState.startStopState)
        {
          // Prevent updating client start stop state with an outdated start stop state
          *newClientState.startStopState = detail::selectPreferredStartStopState(
            clientState.startStopState, *newClientState.startStopState);
          clientState.startStopState = *newClientState.startStopState;
        }
        applyArrangement(clientState, newClientState);
      });
    mIo->async([this, newClientState] { handleClientState(newClientState); });
  }

  // Non-blocking client state access for a realtime context. NOT
  // thread-safe. Must not be called from multiple threads
  // concurrently and must not be called concurrently with setClientStateRtSafe.
  ClientState clientStateRtSafe() const
  {
    // Respect the session state guard and the client state guard but don't
    // block on them. If we can't access one or both because of concurrent modification
    // we fall back to our cached version of the timeline and/or start stop state.

    if (!mHasPendingRtClientStates)
    {
      const auto now = mClock.micros();
      const auto timelineGracePeriodOver =
        now - mRtClientState.timelineTimestamp > detail::kLocalModGracePeriod;
      const auto startStopStateGracePeriodOver =
        now - mRtClientState.startStopStateTimestamp > detail::kLocalModGracePeriod;
      const auto arrangementTimelineGracePeriodOver =
        now - mRtClientState.arrangementTimelineTimestamp > detail::kLocalModGracePeriod;

      if (timelineGracePeriodOver || startStopStateGracePeriodOver
          || arrangementTimelineGracePeriodOver)
      {
        const auto clientState = mClientState.getRt();
        const auto shouldMirrorArrangement = [&]
        {
          const auto isClientArrangementValid =
            mClientState.isArrangementValid(clientState);
          const auto isClientArrangementStale =
            mClientState.isArrangementStale(clientState);
          const auto isRtArrangementStale =
            mClientState.isArrangementStale(mRtClientState);
          const auto mustRtArrangementFollowTimeline =
            isRtArrangementStale && !isClientArrangementValid;
          const auto isArrangementRefreshCoupled =
            isClientArrangementStale || mustRtArrangementFollowTimeline;
          const auto isArrangementRefreshDueAndIndependent =
            arrangementTimelineGracePeriodOver && !isArrangementRefreshCoupled;
          return (timelineGracePeriodOver && isArrangementRefreshCoupled)
                 || isArrangementRefreshDueAndIndependent;
        }();

        if (timelineGracePeriodOver)
        {
          mRtClientState.timeline = clientState.timeline;
          mRtClientState.timelineSessionId = clientState.timelineSessionId;
        }

        if (startStopStateGracePeriodOver)
        {
          mRtClientState.startStopState = clientState.startStopState;
        }

        if (shouldMirrorArrangement)
        {
          mClientState.mirrorArrangement(mRtClientState, clientState);
        }
      }
    }

    return mClientState.resolve(mRtClientState);
  }

  // should only be called from the audio thread
  void setClientStateRtSafe(IncomingClientState newClientState)
  {
    if (!newClientState.timeline && !newClientState.startStopState
        && !newClientState.arrangementTimeline)
    {
      return;
    }

    if (newClientState.timeline)
    {
      *newClientState.timeline = clampTempo(*newClientState.timeline);
    }

    if (newClientState.startStopState)
    {
      // Prevent updating client start stop state with an outdated start stop state
      *newClientState.startStopState = detail::selectPreferredStartStopState(
        mRtClientState.startStopState, *newClientState.startStopState);
    }

    // This flag ensures that mRtClientState is only updated after all incoming
    // client states were processed
    mHasPendingRtClientStates = true;
    mRtClientStateSetter.push(newClientState);
    const auto now = mClock.micros();
    // Cache the new timeline and StartStopState for serving back to the client
    if (newClientState.timeline)
    {
      // Cache the new timeline and StartStopState for serving back to the client
      mRtClientState.timeline = *newClientState.timeline;
      mRtClientState.timelineTimestamp = makeRtTimestamp(now);
    }
    if (newClientState.startStopState)
    {
      mRtClientState.startStopState = *newClientState.startStopState;
      mRtClientState.startStopStateTimestamp = makeRtTimestamp(now);
    }
    if (newClientState.arrangementTimeline)
    {
      mClientState.stampArrangement(mRtClientState, *newClientState.arrangementTimeline);
      mRtClientState.arrangementTimelineTimestamp = makeRtTimestamp(now);
    }
    else if (newClientState.timeline && mClientState.isArrangementStale(mRtClientState))
    {
      // New timeline arrives session-relative (client read it via resolve()). The
      // stale arrangement must be cleared so resolve() doesn't fold it again.
      mRtClientState.arrangementTimeline = std::nullopt;
    }
  }

protected:
  SessionState sessionState() const
  {
    std::lock_guard<std::mutex> lock(mSessionStateGuard);
    return mSessionState;
  }

  std::chrono::microseconds makeRtTimestamp(const std::chrono::microseconds now) const
  {
    return isEnabled() ? now : std::chrono::microseconds(0);
  }

  void invokeStartStopStateCallbackIfChanged()
  {
    bool shouldInvokeCallback = false;

    mClientState.update(
      [&](ClientState& clientState)
      {
        shouldInvokeCallback =
          mLastIsPlayingForStartStopStateCallback != clientState.startStopState.isPlaying;
        mLastIsPlayingForStartStopStateCallback = clientState.startStopState.isPlaying;
      });

    if (shouldInvokeCallback)
    {
      mStartStopStateCallback(mLastIsPlayingForStartStopStateCallback);
    }
  }

  void updateDiscovery()
  {
    // Push the change to the discovery service
    std::lock_guard<std::mutex> lock(mSessionStateGuard);
    mDiscovery.updateNodeState(
      std::make_pair(NodeState{mNodeId,
                               mSessionId,
                               mSessionState.timeline,
                               mSessionState.startStopState,
                               mSessionState.arrangementTimeline},
                     mSessionState.ghostXForm));
  }

  void updateSessionTiming(Timeline newTimeline, const GhostXForm newXForm)
  {
    // Clamp the session tempo because it may slightly overshoot (999 bpm is
    // transferred as 60606 us/beat and received as 999.000999... bpm).
    newTimeline = clampTempo(newTimeline);
    const auto oldTimeline = mSessionState.timeline;
    const auto oldXForm = mSessionState.ghostXForm;

    if (oldTimeline != newTimeline || oldXForm != newXForm)
    {
      {
        std::lock_guard<std::mutex> lock(mSessionStateGuard);
        mSessionState.timeline = newTimeline;
        mSessionState.ghostXForm = newXForm;
      }

      // Update the client timeline and start stop state based on the new session timing
      mClientState.update(
        [&](ClientState& clientState)
        {
          clientState.timeline =
            updateClientTimelineFromSession(clientState.timeline,
                                            mSessionState.timeline,
                                            mClock.micros(),
                                            mSessionState.ghostXForm);
          clientState.timelineSessionId = mSessionId;

          if (mStartStopSyncEnabled && mSessionState.startStopState != StartStopState{}
              && (!mArrangementSyncEnabled
                  || detail::startStopCarriesArrangement(
                    mSessionState.arrangementTimeline, mSessionState.startStopState)))
          {
            std::lock_guard<std::mutex> startStopStateLock(mSessionStateGuard);
            clientState.startStopState =
              detail::mapStartStopStateFromSessionToClient(mSessionState.startStopState,
                                                           mSessionState.timeline,
                                                           mSessionState.ghostXForm);
          }

          if (mClientState.isArrangementValid(clientState)
              && mSessionState.arrangementTimeline)
          {
            std::lock_guard<std::mutex> arrangementLock(mSessionStateGuard);
            stampArrangementFromSession(clientState, *mSessionState.arrangementTimeline);
          }
        });

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
    updateSessionTiming(mSessions.sawSessionTimeline(std::move(id), std::move(timeline)),
                        mSessionState.ghostXForm);
    mpSessionController->updateDiscoveryCallback();
  }

  void resetSessionStartStopState() { mSessionState.startStopState = StartStopState{}; }

  void handleStartStopStateFromSession(
    SessionId sessionId,
    StartStopState startStopState,
    std::optional<ArrangementTimeline> arrangementTimeline)
  {
    debug(mIo->log()) << "Received start stop state. isPlaying: "
                      << startStopState.isPlaying
                      << ", beats: " << startStopState.beats.floating()
                      << ", time: " << startStopState.timestamp.count()
                      << ", hasArrangement: " << arrangementTimeline.has_value()
                      << " for session: " << sessionId;

    if (sessionId != mSessionId)
    {
      return;
    }

    const auto isNewStartStopState =
      startStopState.timestamp > mSessionState.startStopState.timestamp;
    const auto isCurrentStartStopState = startStopState == mSessionState.startStopState;

    // Transport propagation is asymmetric: a start-stop-only peer must NOT change the
    // transport of a peer that has arrangement sync enabled. Start-stop from arrangement
    // propagates to start stop only peers.
    const auto adoptStartStopToClient =
      (isNewStartStopState
       || (isCurrentStartStopState && arrangementTimeline
           && arrangementTimeline->timestamp == startStopState.timestamp))
      && mStartStopSyncEnabled
      && (!mArrangementSyncEnabled
          || detail::startStopCarriesArrangement(arrangementTimeline, startStopState));

    const auto isNewArrangementTimeline =
      arrangementTimeline
      && (!mSessionState.arrangementTimeline
          || arrangementTimeline->timestamp
               > mSessionState.arrangementTimeline->timestamp);

    if (isNewStartStopState)
    {
      mSessionState.startStopState = startStopState;
    }

    if (isNewArrangementTimeline)
    {
      std::lock_guard<std::mutex> lock(mSessionStateGuard);
      mSessionState.arrangementTimeline = arrangementTimeline;
    }

    if (isNewStartStopState || isNewArrangementTimeline)
    {
      // Always relay so even a client that doesn't have the feature enabled can function
      // as a relay and forward the change to the rest of the session.
      mpSessionController->updateDiscoveryCallback();

      mClientState.update(
        [&](ClientState& clientState)
        {
          if (adoptStartStopToClient)
          {
            clientState.startStopState = detail::mapStartStopStateFromSessionToClient(
              startStopState, mSessionState.timeline, mSessionState.ghostXForm);
          }
          if (isNewArrangementTimeline && mArrangementSyncEnabled)
          {
            stampArrangementFromSession(clientState, *arrangementTimeline);
          }
        });

      if (adoptStartStopToClient)
      {
        invokeStartStopStateCallbackIfChanged();
      }
    }
  }

  void resetSessionArrangementTimeline()
  {
    mSessionState.arrangementTimeline = std::nullopt;
  }

  void reanchorSessionArrangementTimeline()
  {
    mClientState.update(
      [&](ClientState& clientState)
      {
        if (clientState.arrangementTimeline)
        {
          clientState.timeline.beatOrigin =
            clientState.timeline.beatOrigin - clientState.arrangementTimeline->beatOrigin;
          clientState.arrangementTimeline = std::nullopt;
        }
      });
    resetSessionArrangementTimeline();
  }

  void handleClientState(const IncomingClientState clientState)
  {
    auto mustUpdateDiscovery = false;

    if (clientState.timeline)
    {
      auto sessionTimeline =
        updateSessionTimelineFromClient(mSessionState.timeline,
                                        *clientState.timeline,
                                        clientState.timelineTimestamp,
                                        mSessionState.ghostXForm);

      mSessions.resetTimeline(sessionTimeline);
      mPeers.setSessionTimeline(mSessionId, sessionTimeline);
      updateSessionTiming(std::move(sessionTimeline), mSessionState.ghostXForm);

      mustUpdateDiscovery = true;
    }

    if (mStartStopSyncEnabled && clientState.startStopState)
    {
      // Prevent updating with an outdated start stop state
      const auto newGhostTime =
        mSessionState.ghostXForm.hostToGhost(clientState.startStopState->timestamp);
      if (newGhostTime > mSessionState.startStopState.timestamp)
      {
        mClientState.update(
          [&](ClientState& currentClientState)
          {
            mSessionState.startStopState =
              detail::mapStartStopStateFromClientToSession(*clientState.startStopState,
                                                           mSessionState.timeline,
                                                           mSessionState.ghostXForm);
            currentClientState.startStopState = *clientState.startStopState;
          });

        // Transport propagation to arrangement peers is asymmetric: they adopt a
        // start/stop only when it carries an arrangement with the same timestamp.
        // If we committed a start/stop without a respective client arrangement,
        // refresh the timestamp of our lingering session arrangement so the broadcast
        // still carries it and other arrangement peers follow the change.
        if (mArrangementSyncEnabled && !clientState.arrangementTimeline
            && mSessionState.arrangementTimeline)
        {
          std::lock_guard<std::mutex> lock(mSessionStateGuard);
          mSessionState.arrangementTimeline->timestamp =
            mSessionState.startStopState.timestamp;
        }

        mustUpdateDiscovery = true;
      }
    }

    if (mArrangementSyncEnabled && clientState.arrangementTimeline)
    {
      const auto clientTimeline = mClientState.get().timeline;
      const auto mapped = detail::mapArrangementTimelineFromClientToSession(
        *clientState.arrangementTimeline,
        mSessionState.timeline,
        clientTimeline,
        mSessionState.ghostXForm);
      // Prevent updating with an outdated arrangement timeline
      if (!mSessionState.arrangementTimeline
          || mapped.timestamp > mSessionState.arrangementTimeline->timestamp)
      {
        {
          std::lock_guard<std::mutex> lock(mSessionStateGuard);
          mSessionState.arrangementTimeline = mapped;
        }
        mustUpdateDiscovery = true;
      }
    }

    if (mustUpdateDiscovery)
    {
      mpSessionController->updateDiscoveryCallback();
    }

    invokeStartStopStateCallbackIfChanged();
  }

  void stampArrangementFromSession(ClientState& clientState,
                                   const ArrangementTimeline& sessionArrangement)
  {
    mClientState.stampArrangement(
      clientState,
      detail::mapArrangementTimelineFromSessionToClient(sessionArrangement,
                                                        mSessionState.timeline,
                                                        clientState.timeline,
                                                        mSessionState.ghostXForm));
  }

  // Apply an incoming arrangement to the stored state, folding a stale origin into the
  // timeline before stamping — or clearing it when only a new timeline arrives — so
  // resolve() never folds the same origin twice.
  void applyArrangement(ClientState& clientState,
                        const IncomingClientState& newClientState)
  {
    if (newClientState.arrangementTimeline)
    {
      if (mClientState.isArrangementStale(clientState))
      {
        clientState.timeline.beatOrigin =
          clientState.timeline.beatOrigin - clientState.arrangementTimeline->beatOrigin;
      }
      mClientState.stampArrangement(clientState, *newClientState.arrangementTimeline);
    }
    else if (newClientState.timeline && mClientState.isArrangementStale(clientState))
    {
      clientState.arrangementTimeline = std::nullopt;
    }
  }

  void handleRtClientState(IncomingClientState clientState)
  {
    mClientState.update(
      [&](ClientState& currentClientState)
      {
        if (clientState.timeline)
        {
          currentClientState.timeline = *clientState.timeline;
        }
        if (clientState.startStopState)
        {
          // Prevent updating client start stop state with an outdated start stop state
          *clientState.startStopState = detail::selectPreferredStartStopState(
            currentClientState.startStopState, *clientState.startStopState);
          currentClientState.startStopState = *clientState.startStopState;
        }
        applyArrangement(currentClientState, clientState);
      });

    handleClientState(clientState);
    mHasPendingRtClientStates = false;
  }

  void joinSession(const Session& session)
  {
    const bool sessionIdChanged = mSessionId != session.sessionId;
    mSessionId = session.sessionId;

    // Prevent passing the state of the previous session to the new one.
    if (sessionIdChanged)
    {
      mRtClientStateSetter.processPendingClientStates();
      resetSessionStartStopState();
    }

    updateSessionTiming(session.timeline, session.measurement.xform);

    if (sessionIdChanged)
    {
      reanchorSessionArrangementTimeline();
    }

    mpSessionController->updateDiscoveryCallback();

    if (sessionIdChanged)
    {
      debug(mIo->log()) << "Joining session " << session.sessionId << " with tempo "
                        << session.timeline.tempo.bpm();
      mpSessionController->sessionMembershipCallback();
    }
  }

  void resetState()
  {
    mNodeId = NodeId::random<Random>();
    mSessionId = mNodeId;

    const auto xform = detail::initXForm(mClock);
    const auto hostTime = -xform.intercept;
    // When creating the new timeline, make it continuous by finding
    // the beat on the old session timeline corresponding to the
    // current host time and mapping it to the new ghost time
    // representation of the current host time.
    const auto newTl = Timeline{
      mSessionState.timeline.tempo,
      mSessionState.timeline.toBeats(mSessionState.ghostXForm.hostToGhost(hostTime)),
      xform.hostToGhost(hostTime)};

    resetSessionStartStopState();

    updateSessionTiming(newTl, xform);
    reanchorSessionArrangementTimeline();
    mpSessionController->updateDiscoveryCallback();

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

  struct RtClientStateSetter
  {
    using CallbackDispatcher =
      typename IoContext::template LockFreeCallbackDispatcher<std::function<void()>,
                                                              std::chrono::milliseconds>;

    RtClientStateSetter(Controller& controller)
      : mController(controller)
      , mCallbackDispatcher(
          [this]
          {
            mController.mIo->async(
              [this]() { mController.mpSessionController->updateRtStatesCallback(); });
          },
          detail::kRtHandlerFallbackPeriod)
    {
    }

    void start() { mCallbackDispatcher.start(); }

    void stop() { mCallbackDispatcher.stop(); }

    void invoke() { mCallbackDispatcher.invoke(); }

    void push(const IncomingClientState clientState)
    {
      if (clientState.timeline)
      {
        mTimelineBuffer.write(
          std::make_pair(clientState.timelineTimestamp, *clientState.timeline));
      }

      if (clientState.startStopState)
      {
        mStartStopStateBuffer.write(*clientState.startStopState);
      }

      if (clientState.arrangementTimeline)
      {
        mArrangementTimelineBuffer.write(*clientState.arrangementTimeline);
      }

      if (clientState.timeline || clientState.startStopState
          || clientState.arrangementTimeline)
      {
        mCallbackDispatcher.invoke();
      }
    }

    void processPendingClientStates()
    {
      const auto clientState = buildMergedPendingClientState();
      mController.handleRtClientState(clientState);
    }

    void updateEnabled()
    {
      if (mController.mEnabled && !mController.mDiscovery.isEnabled())
      {
        // Always reset when first enabling to avoid hijacking tempo in existing sessions
        mController.resetState();
        mController.mDiscovery.enable(true);
      }
      else if (!mController.mEnabled && mController.mDiscovery.isEnabled())
      {
        mController.mDiscovery.enable(false);
      }
    }

  private:
    IncomingClientState buildMergedPendingClientState()
    {
      auto clientState = IncomingClientState{};
      if (auto tl = mTimelineBuffer.readNew())
      {
        clientState.timelineTimestamp = (*tl).first;
        clientState.timeline = OptionalTimeline{(*tl).second};
      }
      if (auto sss = mStartStopStateBuffer.readNew())
      {
        clientState.startStopState = sss;
      }
      if (auto atl = mArrangementTimelineBuffer.readNew())
      {
        clientState.arrangementTimeline = atl;
      }
      return clientState;
    }

    Controller& mController;
    // Use separate TripleBuffers for the Timeline, StartStopState and Arrangement so we
    // read the latest set value from either optional.
    util::TripleBuffer<std::pair<std::chrono::microseconds, Timeline>> mTimelineBuffer;
    util::TripleBuffer<ClientStartStopState> mStartStopStateBuffer;
    util::TripleBuffer<ClientArrangementTimeline> mArrangementTimelineBuffer;
    CallbackDispatcher mCallbackDispatcher;
  };

  struct SessionStartStopStateCallback
  {
    void operator()(SessionId sessionId,
                    StartStopState startStopState,
                    std::optional<ArrangementTimeline> arrangementTimeline)
    {
      mController.handleStartStopStateFromSession(
        sessionId, std::move(startStopState), std::move(arrangementTimeline));
    }

    Controller& mController;
  };

  struct SawAudioEndpointCallback
  {
    void operator()(NodeId peerId,
                    std::optional<discovery::UdpEndpoint> endpoint,
                    discovery::IpAddress gateway)
    {
      mpController->mpSessionController->sawAudioEndpointCallback(
        peerId, endpoint, gateway);
    }

    Controller* mpController;
  };

  struct SessionMembershipCallback
  {
    void operator()() { mpController->mpSessionController->sessionMembershipCallback(); }

    Controller* mpController;
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
          mController.mIo->async([this] { mController.resetState(); });
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
      mpController->mpSessionController->measurePeerCallback(
        std::move(peer), std::move(handler));
    }

    Controller* mpController;
  };

  template <typename Peer, typename Handler>
  void measurePeer(Peer peer, Handler handler)
  {
    mDiscovery.withGateways(
      [peer, handler](auto begin, const auto end)
      {
        const auto addr = peer.second;
        const auto it =
          std::find_if(begin, end, [&addr](const auto& vt) { return vt.first == addr; });
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

  struct JoinSessionCallback
  {
    void operator()(Session session)
    {
      mpController->mpSessionController->joinSessionCallback(std::move(session));
    }

    Controller* mpController;
  };

  void shutdown()
  {
    if (mIoStopped)
    {
      return;
    }

    std::mutex mutex;
    std::condition_variable condition;

    this->mIo->async(
      [this, &mutex, &condition]()
      {
        mRtClientStateSetter.stop();

        mEnabled = false;
        mDiscovery.enable(false);

        std::unique_lock<std::mutex> lock(mutex);
        mIoStopped = true;
        condition.notify_one();
      });

    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [&]() { return mIoStopped.load(); });

    this->mIo->stop();
  }

  using IoType = typename util::Injected<IoContext>::type;

  using ControllerPeers = Peers<IoType&,
                                SessionMembershipCallback,
                                SessionTimelineCallback,
                                SessionStartStopStateCallback,
                                SawAudioEndpointCallback>;

  using ControllerGateway =
    Gateway<typename ControllerPeers::GatewayObserver, Clock, IoType&>;
  using GatewayPtr = std::shared_ptr<ControllerGateway>;

  struct GatewayFactory
  {
    GatewayPtr operator()(std::pair<NodeState, GhostXForm> state,
                          util::Injected<IoType&> io,
                          const discovery::IpAddress& addr)
    {
      return GatewayPtr{new ControllerGateway{
        std::move(io),
        addr,
        util::injectVal(makeGatewayObserver(mpController->mPeers, addr)),
        std::move(state.first),
        std::move(state.second),
        mpController->mClock}};
    }

    void gatewaysChanged()
    {
      mpController->mpSessionController->gatewaysChangedCallback();
    }

    Controller* mpController;
  };

  struct UdpSendExceptionHandler
  {
    using Exception = discovery::UdpSendException;

    void operator()(const Exception exception)
    {
      mpController->mIo->async(
        [this, exception]
        { mpController->mDiscovery.repairGateway(exception.interfaceAddr); });
    }

    Controller* mpController;
  };

  TempoCallback mTempoCallback;
  StartStopStateCallback mStartStopStateCallback;
  Clock mClock;
  NodeId mNodeId;
  SessionId mSessionId;

  mutable std::mutex mSessionStateGuard;
  SessionState mSessionState;

  ControllerClientState mClientState;
  bool mLastIsPlayingForStartStopStateCallback;

  mutable RtClientState mRtClientState;
  std::atomic<bool> mHasPendingRtClientStates;

  SessionController* mpSessionController;

  SessionPeerCounter mSessionPeerCounter;

  std::atomic<bool> mEnabled;

  std::atomic<bool> mStartStopSyncEnabled;

  std::atomic<bool> mArrangementSyncEnabled;

  util::Injected<IoContext> mIo;
  std::atomic<bool> mIoStopped{false};

  ControllerPeers mPeers;

  using ControllerSessions = Sessions<ControllerPeers&,
                                      MeasurePeer,
                                      JoinSessionCallback,
                                      typename util::Injected<IoContext>::type&,
                                      Clock>;
  ControllerSessions mSessions;

  using Discovery = discovery::Service<std::pair<NodeState, GhostXForm>,
                                       typename util::Injected<GatewayFactory>::type,
                                       typename util::Injected<IoContext>::type&>;
  Discovery mDiscovery;

  RtClientStateSetter mRtClientStateSetter;
};

} // namespace link
} // namespace ableton
