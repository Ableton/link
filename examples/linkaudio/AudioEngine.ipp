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

// Make sure to define this before <cmath> is included on Windows
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
  , mTimeAtLastClick{}
  , mIsPlaying(false)
  , mLinkAudioRenderer(mLink, mSampleRate)
{
  if (!mOutputLatency.is_lock_free())
  {
    std::cout << "WARNING: AudioEngine::mOutputLatency is not lock free!" << std::endl;
  }
}

template <typename Link>
void AudioEngine<Link>::startPlaying()
{
  mEngineDataSignals.requestStart = true;
}

template <typename Link>
void AudioEngine<Link>::stopPlaying()
{
  mEngineDataSignals.requestStop = true;
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
  return sessionState.beatAtTime(mLink.clock().micros(), mEngineDataSignals.quantum);
}

template <typename Link>
void AudioEngine<Link>::setTempo(double tempo)
{
  mEngineDataSignals.requestedTempo = tempo;
}

template <typename Link>
double AudioEngine<Link>::quantum() const
{
  return mEngineDataSignals.quantum;
}

template <typename Link>
void AudioEngine<Link>::setQuantum(double quantum)
{
  mEngineDataSignals.quantum = quantum;
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
bool AudioEngine<Link>::isArrangementSyncEnabled() const
{
  return mLink.isArrangementSyncEnabled();
}

template <typename Link>
void AudioEngine<Link>::setArrangementSyncEnabled(const bool enabled)
{
  mLink.enableArrangementSync(enabled);
}

template <typename Link>
void AudioEngine<Link>::requestBeatTimeJump()
{
  mEngineDataSignals.requestBeatTimeJump = true;
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

  engineData.requestedTempo = mEngineDataSignals.requestedTempo.exchange(0.);
  engineData.requestStart = mEngineDataSignals.requestStart.exchange(false);
  engineData.requestStop = mEngineDataSignals.requestStop.exchange(false);
  engineData.startStopSyncOn = mEngineDataSignals.startStopSyncOn.load();
  engineData.quantum = mEngineDataSignals.quantum.load();
  engineData.requestBeatTimeJump = mEngineDataSignals.requestBeatTimeJump.exchange(false);

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

  const auto arrangementSyncOn = mLink.isArrangementSyncEnabled();

  for (std::size_t i = 0; i < numSamples; ++i)
  {
    double amplitude = 0.;
    // Compute the host time for this sample and the last.
    const auto hostTime =
      beginHostTime + microseconds(llround(static_cast<double>(i) * microsPerSample));
    const auto lastSampleHostTime = hostTime - microseconds(llround(microsPerSample));

    // Only make sound once transport has started.
    // With arrangement sync the shared launch point is timeForArrangementIsPlaying(): the
    // beat is
    // absolute, so deferral is driven by that quantized launch time (the same deferred
    // start behaviour as start/stop sync). Without it, timeForIsPlaying() is the raw
    // start time and the in-phase start beat is derived via beatAtTime() >= 0
    // (quantization lives in the beat mapping); negative beats before beat 0 are the
    // count-in.
    const auto launchStarted = arrangementSyncOn
                                 ? hostTime >= sessionState.timeForArrangementIsPlaying()
                                 : (hostTime >= sessionState.timeForIsPlaying()
                                    && sessionState.beatAtTime(hostTime, quantum) >= 0.);
    if (launchStarted)
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
    sessionState.requestBeatAtStartPlayingTime(0., engineData.quantum);
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

  if (engineData.requestBeatTimeJump)
  {
    // Non-quantized jump of the shared arrangement timeline to beat 0. This is a no-op
    // unless arrangement sync is enabled, so without it the local timeline is untouched.
    sessionState.setArrangementBeatAtTime(0., hostTime);
  }

  // Timeline modifications are complete, commit the results
  mLink.commitAudioSessionState(sessionState);

  if (mIsPlaying)
  {
    // As long as the engine is playing, generate metronome clicks in
    // the buffer at the appropriate beats.
    renderMetronomeIntoBuffer(sessionState, engineData.quantum, hostTime, numSamples);
  }

  mLinkAudioRenderer(mBuffers[0].data(),
                     mBuffers[1].data(),
                     mBuffers[0].size(),
                     sessionState,
                     mSampleRate,
                     hostTime,
                     engineData.quantum);
}

} // namespace linkaudio
} // namespace ableton
