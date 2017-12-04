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

inline Link::Link(const double bpm)
  : mPeerCountCallback([](std::size_t) {})
  , mTempoCallback([](link::Tempo) {})
  , mStartStopCallback([](bool) {})
  , mClock{}
  , mController(link::Tempo(bpm),
      [this](const std::size_t peers) {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mPeerCountCallback(peers);
      },
      [this](const link::Tempo tempo) {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mTempoCallback(tempo);
      },
      [this](const bool isPlaying) {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mStartStopCallback(isPlaying);
      },
      mClock,
      util::injectVal(link::platform::IoContext{}))
{
}

inline bool Link::isEnabled() const
{
  return mController.isEnabled();
}

inline void Link::enable(const bool bEnable)
{
  mController.enable(bEnable);
}

inline bool Link::isStartStopSyncEnabled() const
{
  return mController.isStartStopSyncEnabled();
}

inline void Link::enableStartStopSync(bool bEnable)
{
  mController.enableStartStopSync(bEnable);
}

inline std::size_t Link::numPeers() const
{
  return mController.numPeers();
}

template <typename Callback>
void Link::setNumPeersCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mPeerCountCallback = [callback](const std::size_t numPeers) { callback(numPeers); };
}

template <typename Callback>
void Link::setTempoCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mTempoCallback = [callback](const link::Tempo tempo) { callback(tempo.bpm()); };
}

template <typename Callback>
void Link::setStartStopCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mStartStopCallback = callback;
}

inline Link::Clock Link::clock() const
{
  return mClock;
}

inline Link::SessionState Link::captureAudioSessionState() const
{
  return Link::SessionState{mController.sessionStateRtSafe(), numPeers() > 0};
}

inline void Link::commitAudioSessionState(const Link::SessionState state)
{
  if (state.mOriginalSessionState != state.mSessionState)
  {
    mController.setSessionStateRtSafe({state.mSessionState.timeline,
      state.mSessionState.startStopState, mClock.micros()});
  }
}

inline Link::SessionState Link::captureAppSessionState() const
{
  return Link::SessionState{mController.sessionState(), numPeers() > 0};
}

inline void Link::commitAppSessionState(const Link::SessionState state)
{
  if (state.mOriginalSessionState != state.mSessionState)
  {
    mController.setSessionState({state.mSessionState.timeline,
      state.mSessionState.startStopState, mClock.micros()});
  }
}

// Link::SessionState

inline Link::SessionState::SessionState(
  const link::SessionState sessionState, const bool bRespectQuantum)
  : mOriginalSessionState(sessionState)
  , mSessionState(sessionState)
  , mbRespectQuantum(bRespectQuantum)
{
}

inline double Link::SessionState::tempo() const
{
  return mSessionState.timeline.tempo.bpm();
}

inline void Link::SessionState::setTempo(
  const double bpm, const std::chrono::microseconds atTime)
{
  const auto desiredTl = link::clampTempo(
    link::Timeline{link::Tempo(bpm), mSessionState.timeline.toBeats(atTime), atTime});
  mSessionState.timeline.tempo = desiredTl.tempo;
  mSessionState.timeline.timeOrigin =
    desiredTl.fromBeats(mSessionState.timeline.beatOrigin);
}

inline double Link::SessionState::beatAtTime(
  const std::chrono::microseconds time, const double quantum) const
{
  return link::toPhaseEncodedBeats(mSessionState.timeline, time, link::Beats{quantum})
    .floating();
}

inline double Link::SessionState::phaseAtTime(
  const std::chrono::microseconds time, const double quantum) const
{
  return link::phase(link::Beats{beatAtTime(time, quantum)}, link::Beats{quantum})
    .floating();
}

inline std::chrono::microseconds Link::SessionState::timeAtBeat(
  const double beat, const double quantum) const
{
  return link::fromPhaseEncodedBeats(
    mSessionState.timeline, link::Beats{beat}, link::Beats{quantum});
}

inline void Link::SessionState::requestBeatAtTime(
  const double beat, std::chrono::microseconds time, const double quantum)
{
  if (mbRespectQuantum)
  {
    time = timeAtBeat(link::nextPhaseMatch(link::Beats{beatAtTime(time, quantum)},
                        link::Beats{beat}, link::Beats{quantum})
                        .floating(),
      quantum);
  }
  forceBeatAtTime(beat, time, quantum);
}

inline void Link::SessionState::forceBeatAtTime(
  const double beat, const std::chrono::microseconds time, const double quantum)
{
  // There are two components to the beat adjustment: a phase shift
  // and a beat magnitude adjustment.
  const auto curBeatAtTime = link::Beats{beatAtTime(time, quantum)};
  const auto closestInPhase =
    link::closestPhaseMatch(curBeatAtTime, link::Beats{beat}, link::Beats{quantum});
  mSessionState.timeline =
    shiftClientTimeline(mSessionState.timeline, closestInPhase - curBeatAtTime);
  // Now adjust the magnitude
  mSessionState.timeline.beatOrigin =
    mSessionState.timeline.beatOrigin + (link::Beats{beat} - closestInPhase);
}

inline void Link::SessionState::setIsPlaying(
  const bool isPlaying, const std::chrono::microseconds time)
{
  mSessionState.startStopState = {isPlaying, time};
}

inline bool Link::SessionState::isPlaying() const
{
  return mSessionState.startStopState.isPlaying;
}

inline std::chrono::microseconds Link::SessionState::timeForIsPlaying() const
{
  return mSessionState.startStopState.time;
}

inline void Link::SessionState::requestBeatAtStartPlayingTime(
  const double beat, const double quantum)
{
  if (isPlaying())
  {
    requestBeatAtTime(beat, mSessionState.startStopState.time, quantum);
  }
}

inline void Link::SessionState::setIsPlayingAndRequestBeatAtTime(
  bool isPlaying, std::chrono::microseconds time, double beat, double quantum)
{
  mSessionState.startStopState = {isPlaying, time};
  requestBeatAtStartPlayingTime(beat, quantum);
}

} // ableton
