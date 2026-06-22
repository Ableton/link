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

#include <ableton/link/Phase.hpp>

namespace ableton
{
namespace detail
{

template <typename Clock>
inline typename BasicLink<Clock>::SessionState toSessionState(
  const link::ClientState& state,
  const bool isConnected,
  const bool isArrangementSyncEnabled)
{
  const auto arrangementTimeline =
    state.arrangementTimeline
      ? link::OptionalApiArrangementTimeline{link::ApiArrangementTimeline{
          state.arrangementTimeline->beatOrigin, state.arrangementTimeline->launchTime}}
      : link::OptionalApiArrangementTimeline{};
  return {{state.timeline,
           state.timelineSessionId,
           {state.startStopState.isPlaying, state.startStopState.time},
           arrangementTimeline},
          isConnected,
          isArrangementSyncEnabled};
}

inline link::IncomingClientState toIncomingClientState(
  const link::ApiState& state,
  const link::ApiState& originalState,
  const std::chrono::microseconds timestamp,
  const bool isArrangementSyncEnabled)
{
  const auto timeline = originalState.timeline != state.timeline
                          ? link::OptionalTimeline{state.timeline}
                          : link::OptionalTimeline{};
  const auto startStopChanged = originalState.startStopState != state.startStopState;
  const auto startStopState =
    startStopChanged ? link::OptionalClientStartStopState{{state.startStopState.isPlaying,
                                                           state.startStopState.time,
                                                           timestamp}}
                     : link::OptionalClientStartStopState{};
  const auto arrangementChanged =
    originalState.arrangementTimeline != state.arrangementTimeline;
  auto arrangementTimeline =
    arrangementChanged && state.arrangementTimeline
      ? link::OptionalClientArrangementTimeline{link::ClientArrangementTimeline{
          state.arrangementTimeline->beatOrigin,
          state.arrangementTimeline->launchTime,
          timestamp}}
      : link::OptionalClientArrangementTimeline{};
  if (startStopChanged && state.arrangementTimeline)
  {
    arrangementTimeline =
      link::ClientArrangementTimeline{state.arrangementTimeline->beatOrigin,
                                      state.arrangementTimeline->launchTime,
                                      timestamp};
  }
  else if (startStopChanged && !state.startStopState.isPlaying
           && isArrangementSyncEnabled)
  {
    // We are stopping with arrangement sync enabled but have no arrangement of our own
    // — e.g. we enabled arrangement sync after joining an already-playing session, so we
    // never received one. Synthesize an arrangement that matches the current client
    // timeline (origin 0: the arrangement beat tracks the timeline beat, a no-op for us)
    // so the stop still carries an arrangement and other arrangement peers follow it.
    // The Controller remaps it into the session domain like any other client
    // arrangement; if our client timeline carries a folded arrangement offset the remap
    // reconstructs the shared origin, otherwise the arrangement stays continuous.
    arrangementTimeline = link::ClientArrangementTimeline{
      link::Beats{0.}, state.startStopState.time, timestamp};
  }
  return {timeline, startStopState, arrangementTimeline, timestamp};
}

} // namespace detail

template <typename Clock>
inline BasicLink<Clock>::BasicLink(const double bpm)
  : mController(
      link::Tempo(bpm),
      [this](const std::size_t peers)
      {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mPeerCountCallback(peers);
      },
      [this](const link::Tempo tempo)
      {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mTempoCallback(tempo);
      },
      [this](const bool isPlaying)
      {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mStartStopCallback(isPlaying);
      },
      mClock)
{
}

template <typename Clock>
inline bool BasicLink<Clock>::isEnabled() const
{
  return mController.isEnabled();
}

template <typename Clock>
inline void BasicLink<Clock>::enable(const bool bEnable)
{
  mController.enable(bEnable);
}

template <typename Clock>
inline bool BasicLink<Clock>::isStartStopSyncEnabled() const
{
  return mController.isStartStopSyncEnabled();
}

template <typename Clock>
inline void BasicLink<Clock>::enableStartStopSync(bool bEnable)
{
  mController.enableStartStopSync(bEnable);
}

template <typename Clock>
inline bool BasicLink<Clock>::isArrangementSyncEnabled() const
{
  return mController.isArrangementSyncEnabled();
}

template <typename Clock>
inline void BasicLink<Clock>::enableArrangementSync(bool bEnable)
{
  mController.enableArrangementSync(bEnable);
}

template <typename Clock>
inline std::size_t BasicLink<Clock>::numPeers() const
{
  return mController.numPeers();
}

template <typename Clock>
template <typename Callback>
void BasicLink<Clock>::setNumPeersCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mPeerCountCallback = [callback](const std::size_t numPeers) { callback(numPeers); };
}

template <typename Clock>
template <typename Callback>
void BasicLink<Clock>::setTempoCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mTempoCallback = [callback](const link::Tempo tempo) { callback(tempo.bpm()); };
}

template <typename Clock>
template <typename Callback>
void BasicLink<Clock>::setStartStopCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mStartStopCallback = callback;
}

template <typename Clock>
inline Clock BasicLink<Clock>::clock() const
{
  return mClock;
}

template <typename Clock>
inline typename BasicLink<Clock>::SessionState BasicLink<
  Clock>::captureAudioSessionState() const
{
  return detail::toSessionState<Clock>(mController.clientStateRtSafe(),
                                       numPeers() > 0,
                                       mController.isArrangementSyncEnabled());
}

template <typename Clock>
inline void BasicLink<Clock>::commitAudioSessionState(
  const typename BasicLink<Clock>::SessionState state)
{
  mController.setClientStateRtSafe(detail::toIncomingClientState(
    state.mState, state.mOriginalState, mClock.micros(), state.mbArrangementSyncEnabled));
}

template <typename Clock>
inline typename BasicLink<Clock>::SessionState BasicLink<Clock>::captureAppSessionState()
  const
{
  return detail::toSessionState<Clock>(
    mController.clientState(), numPeers() > 0, mController.isArrangementSyncEnabled());
}

template <typename Clock>
inline void BasicLink<Clock>::commitAppSessionState(
  const typename BasicLink<Clock>::SessionState state)
{
  mController.setClientState(detail::toIncomingClientState(
    state.mState, state.mOriginalState, mClock.micros(), state.mbArrangementSyncEnabled));
}

// Link::SessionState

template <typename Clock>
inline BasicLink<Clock>::SessionState::SessionState(const link::ApiState state,
                                                    const bool bRespectQuantum,
                                                    const bool bArrangementSyncEnabled)
  : mOriginalState(state)
  , mState(state)
  , mbRespectQuantum(bRespectQuantum)
  , mbArrangementSyncEnabled(bArrangementSyncEnabled)
{
}

template <typename Clock>
inline bool BasicLink<Clock>::SessionState::operator==(const SessionState& other) const
{
  return std::tie(mOriginalState, mState, mbRespectQuantum, mbArrangementSyncEnabled)
         == std::tie(other.mOriginalState,
                     other.mState,
                     other.mbRespectQuantum,
                     other.mbArrangementSyncEnabled);
}

template <typename Clock>
inline bool BasicLink<Clock>::SessionState::operator!=(const SessionState& other) const
{
  return !(operator==(other));
}

template <typename Clock>
inline double BasicLink<Clock>::SessionState::tempo() const
{
  return mState.timeline.tempo.bpm();
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::setTempo(
  const double bpm, const std::chrono::microseconds atTime)
{
  const auto desiredTl = link::clampTempo(
    link::Timeline{link::Tempo(bpm), mState.timeline.toBeats(atTime), atTime});
  mState.timeline.tempo = desiredTl.tempo;
  mState.timeline.timeOrigin = desiredTl.fromBeats(mState.timeline.beatOrigin);
}

template <typename Clock>
inline double BasicLink<Clock>::SessionState::beatAtTime(
  const std::chrono::microseconds time, const double quantum) const
{
  if (mbArrangementSyncEnabled && mState.arrangementTimeline)
  {
    // Arrangement sync shares an absolute beat allowing non-quantized jumps
    const auto arrangementBeat =
      mState.timeline.toBeats(time) - mState.arrangementTimeline->beatOrigin;
    if (isPlaying())
    {
      return arrangementBeat.floating();
    }

    // While stopped we keep the arrangement beat's magnitude but realign its phase
    const auto sessionBeat =
      link::toPhaseEncodedBeats(mState.timeline, time, link::Beats{quantum});
    return link::closestPhaseMatch(arrangementBeat, sessionBeat, link::Beats{quantum})
      .floating();
  }

  return link::toPhaseEncodedBeats(mState.timeline, time, link::Beats{quantum})
    .floating();
}

template <typename Clock>
inline double BasicLink<Clock>::SessionState::phaseAtTime(
  const std::chrono::microseconds time, const double quantum) const
{
  return link::phase(link::Beats{beatAtTime(time, quantum)}, link::Beats{quantum})
    .floating();
}

template <typename Clock>
inline std::chrono::microseconds BasicLink<Clock>::SessionState::timeAtBeat(
  const double beat, const double quantum) const
{
  if (mbArrangementSyncEnabled && mState.arrangementTimeline)
  {
    const auto arrangementSessionBeat =
      link::Beats{beat} + mState.arrangementTimeline->beatOrigin;
    if (isPlaying())
    {
      return mState.timeline.fromBeats(arrangementSessionBeat);
    }

    const auto sessionPhaseTarget = link::Beats{beat} + mState.timeline.beatOrigin;
    return mState.timeline.fromBeats(link::closestPhaseMatch(
      arrangementSessionBeat, sessionPhaseTarget, link::Beats{quantum}));
  }
  return link::fromPhaseEncodedBeats(
    mState.timeline, link::Beats{beat}, link::Beats{quantum});
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::requestBeatAtTime(
  const double beat, std::chrono::microseconds time, const double quantum)
{
  if (mbRespectQuantum)
  {
    if (mbArrangementSyncEnabled)
    {
      // Ignore request when playing
      if (mOriginalState.startStopState.isPlaying)
      {
        return;
      }

      // Quantized start
      const auto sessionBeat =
        link::toPhaseEncodedBeats(mState.timeline, time, link::Beats{quantum});
      const auto quantizedBeat =
        link::nextPhaseMatch(sessionBeat, link::Beats{beat}, link::Beats{quantum});
      time =
        link::fromPhaseEncodedBeats(mState.timeline, quantizedBeat, link::Beats{quantum});

      setArrangementBeatAtTime(beat, time);
      mState.arrangementTimeline->launchTime = time;
      return;
    }
    else
    {
      time = timeAtBeat(
        link::nextPhaseMatch(
          link::Beats{beatAtTime(time, quantum)}, link::Beats{beat}, link::Beats{quantum})
          .floating(),
        quantum);
    }
  }
  forceBeatAtTime(beat, time, quantum);
}

inline void forceBeatAtTimeImpl(link::Timeline& timeline,
                                const link::Beats beat,
                                const std::chrono::microseconds time,
                                const link::Beats quantum)
{
  // There are two components to the beat adjustment: a phase shift
  // and a beat magnitude adjustment.
  const auto curBeatAtTime = link::toPhaseEncodedBeats(timeline, time, quantum);
  const auto closestInPhase = link::closestPhaseMatch(curBeatAtTime, beat, quantum);
  timeline = shiftClientTimeline(timeline, closestInPhase - curBeatAtTime);
  // Now adjust the magnitude
  timeline.beatOrigin = timeline.beatOrigin + beat - closestInPhase;
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::forceBeatAtTime(
  const double beat, std::chrono::microseconds time, const double quantum)
{
  // forceBeatAtTime always rudely re-maps the local timeline, regardless of arrangement
  // sync.
  forceBeatAtTimeImpl(mState.timeline, link::Beats{beat}, time, link::Beats{quantum});

  // Due to quantization errors the resulting BeatTime at 'time' after forcing can be
  // bigger than 'beat' which then violates intended behavior of the API and can lead
  // i.e. to missing a downbeat. Thus we have to shift the timeline forwards. This checks
  // the plain timeline we just forced; while an arrangement is active beatAtTime would
  // instead read through the arrangement origin.
  if (link::toPhaseEncodedBeats(mState.timeline, time, link::Beats{quantum})
      > link::Beats{beat})
  {
    forceBeatAtTimeImpl(mState.timeline, link::Beats{beat}, ++time, link::Beats{quantum});
  }

  setArrangementBeatAtTime(beat, time);
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::setIsPlaying(
  const bool isPlaying, const std::chrono::microseconds time)
{
  mState.startStopState = {isPlaying, time};
}

template <typename Clock>
inline bool BasicLink<Clock>::SessionState::isPlaying() const
{
  return mState.startStopState.isPlaying;
}

template <typename Clock>
inline std::chrono::microseconds BasicLink<Clock>::SessionState::timeForIsPlaying() const
{
  return mState.startStopState.time;
}

template <typename Clock>
inline std::chrono::microseconds BasicLink<
  Clock>::SessionState::timeForArrangementIsPlaying() const
{
  return mState.arrangementTimeline ? mState.arrangementTimeline->launchTime
                                    : timeForIsPlaying();
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::requestBeatAtStartPlayingTime(
  const double beat, const double quantum)
{
  if (isPlaying())
  {
    requestBeatAtTime(beat, mState.startStopState.time, quantum);
  }
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::setIsPlayingAndRequestBeatAtTime(
  bool isPlaying, std::chrono::microseconds time, double beat, double quantum)
{
  mState.startStopState = {isPlaying, time};
  requestBeatAtStartPlayingTime(beat, quantum);
}

template <typename Clock>
inline void BasicLink<Clock>::SessionState::setArrangementBeatAtTime(
  const double beat, const std::chrono::microseconds time)
{
  if (mbArrangementSyncEnabled)
  {
    // Allows non-quantized jump. A jump only moves the origin; it must not restart
    // transport, so the existing launch point is preserved.
    const auto launchTime =
      mState.arrangementTimeline ? mState.arrangementTimeline->launchTime : time;
    mState.arrangementTimeline = link::ApiArrangementTimeline{
      mState.timeline.toBeats(time) - link::Beats{beat}, launchTime};
  }
}

} // namespace ableton
