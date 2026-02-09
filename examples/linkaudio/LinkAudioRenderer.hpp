/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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

#if defined(LINK_AUDIO)

#include <ableton/LinkAudio.hpp>
#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Queue.hpp>
#include <ableton/util/FloatIntConversion.hpp>

namespace ableton
{
namespace linkaudio
{

namespace
{

template <typename T>
T cubicInterpolate(const std::array<T, 4>& p, double t)
{
  double a = -0.5 * static_cast<double>(p[0]) + 1.5 * static_cast<double>(p[1])
             - 1.5 * static_cast<double>(p[2]) + 0.5 * static_cast<double>(p[3]);
  double b = static_cast<double>(p[0]) - 2.5 * static_cast<double>(p[1])
             + 2.0 * static_cast<double>(p[2]) - 0.5 * static_cast<double>(p[3]);
  double c = -0.5 * static_cast<double>(p[0]) + 0.5 * static_cast<double>(p[2]);
  double d = static_cast<double>(p[1]);
  return static_cast<T>(a * t * t * t + b * t * t + c * t + d);
}

template <typename T>
T linearInterpolate(T value, T inMin, T inMax, T outMin, T outMax)
{
  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

} // namespace

template <typename Link>
class LinkAudioRenderer
{
  struct Buffer
  {
    std::array<double, 512> mSamples; // Should at least hold max network buffer size
    LinkAudioSource::BufferHandle::Info mInfo;
  };
  using Queue = link_audio::Queue<Buffer>;

public:
  LinkAudioRenderer(Link& link, double& sampleRate)
    : mLink(link)
    , mSink(mLink, "A Sink", 4096)
    , mSampleRate(sampleRate)
  {
    auto queue = Queue(2048, {});
    mpQueueWriter = std::make_shared<typename Queue::Writer>(std::move(queue.writer()));
    mpQueueReader = std::make_shared<typename Queue::Reader>(std::move(queue.reader()));
  }

  ~LinkAudioRenderer() { mpSource.reset(); }

  void send(double* pSamples,
            size_t numFrames,
            typename Link::SessionState sessionState,
            double sampleRate,
            const std::chrono::microseconds hostTime,
            double quantum)
  {
    // We can only send audio if the sink can provide a buffer to write to
    auto buffer = LinkAudioSink::BufferHandle(mSink);
    if (buffer)
    {
      // The sink expects 16 bit integers so the doubles have to be converted
      for (auto i = 0u; i < numFrames; ++i)
      {
        buffer.samples[i] = ableton::util::floatToInt16(pSamples[i]);
      }

      // The buffer is commited to Link Audio with the required timing information
      const auto beatsAtBufferBegin = sessionState.beatAtTime(hostTime, quantum);
      buffer.commit(sessionState,
                    beatsAtBufferBegin,
                    quantum,
                    static_cast<uint32_t>(numFrames),
                    1, // mono
                    static_cast<uint32_t>(sampleRate));
    }
  }

  void receive(double* pRightSamples,
               size_t numFrames,
               typename Link::SessionState sessionState,
               double sampleRate,
               const std::chrono::microseconds hostTime,
               double quantum)
  {
    // Get all slots from the queue
    while (mpQueueReader->retainSlot())
    {
    }

    // Calculate the beat range we want to render to the output buffer
    constexpr auto kLatencyInBeats = 4;
    const auto targetBeatsAtBufferBegin =
      sessionState.beatAtTime(hostTime, quantum) - kLatencyInBeats;
    const auto targetBeatsAtBufferEnd =
      sessionState.beatAtTime(
        hostTime
          + std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::duration<double>((double(numFrames)) / sampleRate)),
        quantum)
      - kLatencyInBeats;

    // Drop slots that are too old while we are not rendering
    while (!moStartReadPos && mpQueueReader->numRetainedSlots() > 0)
    {
      if ((*mpQueueReader)[0]->mInfo.endBeats(sessionState, quantum)
          < targetBeatsAtBufferBegin)
      {
        mpQueueReader->releaseSlot();
      }
      else
      {
        break;
      }
    }

    // Return early if there is no audio buffer to read from
    if (mpQueueReader->numRetainedSlots() == 0)
    {
      moLastFrameIdx = std::nullopt;
      moStartReadPos = std::nullopt;
      return;
    }

    // Return early if the next buffer is too new and we are not rendering
    if (!moStartReadPos
        && (*mpQueueReader)[0]->mInfo.beginBeats(sessionState, quantum)
             > targetBeatsAtBufferBegin)
    {
      moLastFrameIdx = std::nullopt;
      moStartReadPos = std::nullopt;
      return;
    }

    // Initialize start read position if not set
    if (!moStartReadPos)
    {
      const auto& info = (*mpQueueReader)[0]->mInfo;
      const auto startBufferBegin = *info.beginBeats(sessionState, quantum);
      const auto startBufferEnd = *info.endBeats(sessionState, quantum);

      // Calculate initial position from target beat
      moStartReadPos = linearInterpolate(targetBeatsAtBufferBegin,
                                         startBufferBegin,
                                         startBufferEnd,
                                         0.0,
                                         double(info.numFrames));
    }

    const auto startFramePos = *moStartReadPos;

    // Loop through queue to find end beat and collect total frames
    auto totalFrames = 0.0;
    auto foundEnd = false;

    for (auto i = 0u; i < mpQueueReader->numRetainedSlots(); ++i)
    {
      const auto& info = (*mpQueueReader)[i]->mInfo;
      const auto bufferBegin = *info.beginBeats(sessionState, quantum);
      const auto bufferEnd = *info.endBeats(sessionState, quantum);

      if (targetBeatsAtBufferEnd >= bufferBegin && targetBeatsAtBufferEnd < bufferEnd)
      {
        const auto targetBeatsFrame = linearInterpolate(
          targetBeatsAtBufferEnd, bufferBegin, bufferEnd, 0.0, double(info.numFrames));

        totalFrames += targetBeatsFrame;
        foundEnd = true;
        break;
      }
      else
      {
        totalFrames += double(info.numFrames);
      }
    }

    // We don't have enough frames buffered
    if (!foundEnd)
    {
      moLastFrameIdx = std::nullopt;
      moStartReadPos = std::nullopt;
      return;
    }

    // Take the frames we have already rendered into account
    totalFrames -= startFramePos;

    // Beat time jump, we can't continue rendering
    if (totalFrames <= 0.0)
    {
      moLastFrameIdx = std::nullopt;
      moStartReadPos = std::nullopt;
      return;
    }

    // The increment is how many source frames to advance per output frame
    // This automatically handles both tempo changes and sample rate differences by
    // re-pitching the audio
    const auto frameIncrement = totalFrames / double(numFrames);
    auto readPos = startFramePos;

    // Helper to get sample at index, handling buffer boundaries
    auto getSample = [&](size_t idx) -> double
    {
      size_t bufferIdx = 0;
      size_t currentIdx = idx;

      while (bufferIdx < mpQueueReader->numRetainedSlots())
      {
        auto& currentBuffer = *((*mpQueueReader)[bufferIdx]);
        if (currentIdx < currentBuffer.mInfo.numFrames)
        {
          return currentBuffer.mSamples[currentIdx];
        }
        currentIdx -= currentBuffer.mInfo.numFrames;
        ++bufferIdx;
      }

      return 0.0;
    };

    for (auto frame = 0u; frame < numFrames; ++frame)
    {
      const auto framePos = readPos + frame * frameIncrement;
      const auto frameIdx = static_cast<size_t>(std::floor(framePos));
      const auto t = framePos - std::floor(framePos);

      // Update the interpolator cache
      while (!moLastFrameIdx || (moLastFrameIdx && frameIdx > *moLastFrameIdx))
      {
        mReceiverSampleCache[3] = mReceiverSampleCache[2];
        mReceiverSampleCache[2] = mReceiverSampleCache[1];
        mReceiverSampleCache[1] = mReceiverSampleCache[0];
        mReceiverSampleCache[0] = (frameIdx > 0) ? getSample(frameIdx - 1) : getSample(0);
        moLastFrameIdx = moLastFrameIdx ? (*moLastFrameIdx + 1) : frameIdx;
      }

      // Write the output frame
      pRightSamples[frame] = cubicInterpolate(mReceiverSampleCache, t);

      // Drop buffer if we've moved past it
      const auto& currentInfo = (*mpQueueReader)[0]->mInfo;
      if (frameIdx >= currentInfo.numFrames)
      {
        readPos -= double(currentInfo.numFrames);
        moLastFrameIdx = frameIdx - currentInfo.numFrames;
        mpQueueReader->releaseSlot();
      }
    }

    // Update the read position for the next buffer
    *moStartReadPos = readPos + double(numFrames) * frameIncrement;
  }

  void operator()(double* pLeftSamples,
                  double* pRightSamples,
                  size_t numFrames,
                  typename Link::SessionState sessionState,
                  double sampleRate,
                  const std::chrono::microseconds hostTime,
                  double quantum)
  {
    // Send the left channel to Link
    send(pLeftSamples, numFrames, sessionState, sampleRate, hostTime, quantum);

    // Write the received audio to the right channel
    receive(pRightSamples, numFrames, sessionState, sampleRate, hostTime, quantum);
  }


  bool hasSource() const { return mpSource != nullptr; }

  void removeSource()
  {
    if (mpSource)
    {
      mpSource.reset();

      while (mpQueueReader->retainSlot())
      {
      }

      while (mpQueueReader->numRetainedSlots() > 0)
      {
        mpQueueReader->releaseSlot();
      }

      moLastFrameIdx = std::nullopt;
      moStartReadPos = std::nullopt;
    }
  }

  void createSource(const ChannelId& channelId)
  {
    mpSource = std::make_unique<LinkAudioSource>(
      mLink,
      channelId,
      [this](ableton::LinkAudioSource::BufferHandle bufferHandle)
      { onSourceBuffer(bufferHandle); });
  }

  void onSourceBuffer(const LinkAudioSource::BufferHandle bufferHandle)
  {
    // When we receive a buffer from Link, we prime it for the audio rendering callback.
    // This runs on the main Link thread, so we should not block it for too long.
    if (mpQueueWriter->retainSlot())
    {
      auto& buffer = *((*mpQueueWriter)[0]);
      buffer.mInfo = bufferHandle.info;
      buffer.mInfo.numChannels = 1;

      for (auto i = 0u; i < bufferHandle.info.numFrames; ++i)
      {
        buffer.mSamples[i] = util::int16ToFloat<double>(
          bufferHandle.samples[i * bufferHandle.info.numChannels]);
      }

      mpQueueWriter->releaseSlot();
    }
  }

  Link& mLink;
  LinkAudioSink mSink;
  std::unique_ptr<LinkAudioSource> mpSource;
  double& mSampleRate;

  std::optional<double> moStartReadPos;

private:
  std::shared_ptr<typename Queue::Writer> mpQueueWriter;
  std::shared_ptr<typename Queue::Reader> mpQueueReader;

  std::array<double, 4> mReceiverSampleCache = {{0.0, 0.0, 0.0, 0.0}};
  std::optional<size_t> moLastFrameIdx = std::nullopt;
};

} // namespace linkaudio
} // namespace ableton

#else

namespace ableton
{
namespace linkaudio
{

template <typename Link>
class LinkAudioRenderer
{
public:
  LinkAudioRenderer(Link&, double&) {}

  // In case we don't support LinkAudio we just copy the left output buffer to the right
  void operator()(double* pLeftSamples,
                  double* pRightSamples,
                  size_t numFrames,
                  typename Link::SessionState,
                  double,
                  const std::chrono::microseconds,
                  double)
  {
    std::copy_n(pLeftSamples, numFrames, pRightSamples);
  }
};

} // namespace linkaudio
} // namespace ableton

#endif
