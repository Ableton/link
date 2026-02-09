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

// Make sure to define this before <cmath> is included for
// Wihttps://github.com/AbletonAppDev/link/pull/664ndows
#ifdef LINK_PLATFORM_WINDOWS
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <iostream>

namespace ableton
{
namespace linkaudio
{

template <typename Link>
AudioEngine<Link>::AudioEngine(Link& link)
  : mLink(link)
  , mSampleRate(44100.)
  , mOutputLatency(std::chrono::microseconds{0})
  , mSharedEngineData({0., false, false, 4., false})
  , mLockfreeEngineData(mSharedEngineData)
  , mTimeAtLastClick{}
  , mIsPlaying(false)
{
  if (!mOutputLatency.is_lock_free())
  {
    std::cout << "WARNING: AudioEngine::mOutputLatency is not lock free!" << std::endl;
  }
}

template <typename Link>
void AudioEngine<Link>::startPlaying()
{
  std::lock_guard<std::mutex> lock(mEngineDataGuard);
  mSharedEngineData.requestStart = true;
}

template <typename Link>
void AudioEngine<Link>::stopPlaying()
{
  std::lock_guard<std::mutex> lock(mEngineDataGuard);
  mSharedEngineData.requestStop = true;
}

template <typename Link>
bool AudioEngine<Link>::isPlaying() const
{
  return mLink.captureAppSessionState().isPlaying();
}

template <typename Link>
double AudioEngine<Link>::beatTime() const
{
  const auto sessionState = mLink.captureAppSessionState();
  return sessionState.beatAtTime(mLink.clock().micros(), mSharedEngineData.quantum);
}

template <typename Link>
void AudioEngine<Link>::setTempo(double tempo)
{
  std::lock_guard<std::mutex> lock(mEngineDataGuard);
  mSharedEngineData.requestedTempo = tempo;
}

template <typename Link>
double AudioEngine<Link>::quantum() const
{
  return mSharedEngineData.quantum;
}

template <typename Link>
void AudioEngine<Link>::setQuantum(double quantum)
{
  std::lock_guard<std::mutex> lock(mEngineDataGuard);
  mSharedEngineData.quantum = quantum;
}

template <typename Link>
bool AudioEngine<Link>::isStartStopSyncEnabled() const
{
  return mLink.isStartStopSyncEnabled();
}

template <typename Link>
void AudioEngine<Link>::setStartStopSyncEnabled(const bool enabled)
{
  mLink.enableStartStopSync(enabled);
}

template <typename Link>
void AudioEngine<Link>::setNumFrames(std::size_t size)
{
  mBuffers[0] = std::vector<double>(size, 0.);
  mBuffers[1] = std::vector<double>(size, 0.);
}

template <typename Link>
void AudioEngine<Link>::setSampleRate(double sampleRate)
{
  mSampleRate = sampleRate;
}

template <typename Link>
typename AudioEngine<Link>::EngineData AudioEngine<Link>::pullEngineData()
{
  auto engineData = EngineData{};
  if (mEngineDataGuard.try_lock())
  {
    engineData.requestedTempo = mSharedEngineData.requestedTempo;
    mSharedEngineData.requestedTempo = 0;
    engineData.requestStart = mSharedEngineData.requestStart;
    mSharedEngineData.requestStart = false;
    engineData.requestStop = mSharedEngineData.requestStop;
    mSharedEngineData.requestStop = false;

    mLockfreeEngineData.quantum = mSharedEngineData.quantum;
    mLockfreeEngineData.startStopSyncOn = mSharedEngineData.startStopSyncOn;

    mEngineDataGuard.unlock();
  }
  engineData.quantum = mLockfreeEngineData.quantum;

  return engineData;
}

template <typename Link>
void AudioEngine<Link>::renderMetronomeIntoBuffer(
  const typename Link::SessionState sessionState,
  const double quantum,
  const std::chrono::microseconds beginHostTime,
  const std::size_t numSamples)
{
  using namespace std::chrono;

  // Metronome frequencies
  static const double highTone = 1567.98;
  static const double lowTone = 1108.73;
  // 100ms click duration
  static const auto clickDuration = duration<double>{0.1};

  // The number of microseconds that elapse between samples
  const auto microsPerSample = 1e6 / mSampleRate;

  for (std::size_t i = 0; i < numSamples; ++i)
  {
    double amplitude = 0.;
    // Compute the host time for this sample and the last.
    const auto hostTime =
      beginHostTime + microseconds(llround(static_cast<double>(i) * microsPerSample));
    const auto lastSampleHostTime = hostTime - microseconds(llround(microsPerSample));

    // Only make sound for positive beat magnitudes. Negative beat
    // magnitudes are count-in beats.
    if (sessionState.beatAtTime(hostTime, quantum) >= 0.)
    {
      // If the phase wraps around between the last sample and the
      // current one with respect to a 1 beat quantum, then a click
      // should occur.
      if (sessionState.phaseAtTime(hostTime, 1)
          < sessionState.phaseAtTime(lastSampleHostTime, 1))
      {
        mTimeAtLastClick = hostTime;
      }

      const auto secondsAfterClick =
        duration_cast<duration<double>>(hostTime - mTimeAtLastClick);

      // If we're within the click duration of the last beat, render
      // the click tone into this sample
      if (secondsAfterClick < clickDuration)
      {
        // If the phase of the last beat with respect to the current
        // quantum was zero, then it was at a quantum boundary and we
        // want to use the high tone. For other beats within the
        // quantum, use the low tone.
        const auto freq =
          floor(sessionState.phaseAtTime(hostTime, quantum)) == 0 ? highTone : lowTone;

        // Simple cosine synth
        amplitude = cos(2 * M_PI * secondsAfterClick.count() * freq)
                    * (1 - sin(5 * M_PI * secondsAfterClick.count()));
      }
    }
    mBuffers[0][i] = amplitude;
  }
}

template <typename Link>
void AudioEngine<Link>::audioCallback(const std::chrono::microseconds hostTime,
                                      const std::size_t numSamples)
{
  const auto engineData = pullEngineData();

  auto sessionState = mLink.captureAudioSessionState();

  // Clear the buffer
  for (auto& buffer : mBuffers)
  {
    std::fill(buffer.begin(), buffer.end(), 0);
  }

  if (engineData.requestStart)
  {
    sessionState.setIsPlaying(true, hostTime);
  }

  if (engineData.requestStop)
  {
    sessionState.setIsPlaying(false, hostTime);
  }

  if (!mIsPlaying && sessionState.isPlaying())
  {
    // Reset the timeline so that beat 0 corresponds to the time when transport starts
    sessionState.requestBeatAtStartPlayingTime(0, engineData.quantum);
    mIsPlaying = true;
  }
  else if (mIsPlaying && !sessionState.isPlaying())
  {
    mIsPlaying = false;
  }

  if (engineData.requestedTempo > 0)
  {
    // Set the newly requested tempo from the beginning of this buffer
    sessionState.setTempo(engineData.requestedTempo, hostTime);
  }

  // Timeline modifications are complete, commit the results
  mLink.commitAudioSessionState(sessionState);

  if (mIsPlaying)
  {
    // As long as the engine is playing, generate metronome clicks in
    // the buffer at the appropriate beats.
    renderMetronomeIntoBuffer(sessionState, engineData.quantum, hostTime, numSamples);
  }

  std::copy(mBuffers[0].begin(), mBuffers[0].end(), mBuffers[1].begin());
}

} // namespace linkaudio
} // namespace ableton
