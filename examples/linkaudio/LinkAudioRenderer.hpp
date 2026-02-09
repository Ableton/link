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
    std::array<int16_t, 2048> mSamples; // TODO size
    LinkAudioSource::BufferHandle::Info mInfo;
  };
  using Queue = link_audio::Queue<Buffer>;

public:
  LinkAudioRenderer(Link& link, double& sampleRate)
    : mLink(link)
    , mSink(mLink, "A Sink", 4096)
    , mSampleRate(sampleRate)
  {
    auto queue = Queue(2084, {});
    mpQueueWriter = std::make_shared<typename Queue::Writer>(std::move(queue.writer()));
    mpQueueReader = std::make_shared<typename Queue::Reader>(std::move(queue.reader()));
  }

  ~LinkAudioRenderer() { mpSource.reset(); }

  void operator()(double* pLeftSamples,
                  double* pRightSamples,
                  size_t numFrames,
                  typename Link::SessionState sessionState,
                  double sampleRate,
                  const std::chrono::microseconds hostTime,
                  double quantum)
  {
    // Send the left channel to Link
    // We can only send audo if the sink can provide a buffer to write to
    auto buffer = LinkAudioSink::BufferHandle(mSink);
    if (buffer)
    {
      // The sink expects 16 bit integers so the doubles have to be converted
      for (auto i = 0u; i < numFrames; ++i)
      {
        buffer.samples[i] = ableton::util::floatToInt16(pLeftSamples[i]);
      }

      // The buffer is commited to Link Audio with the required timing information
      const auto beatsAtBufferBegin = sessionState.beatAtTime(hostTime, quantum);
      buffer.commit(sessionState,
                    beatsAtBufferBegin,
                    quantum,
                    static_cast<uint32_t>(numFrames),
                    1,
                    static_cast<uint32_t>(sampleRate));
    }

    // Receive audio to the right channel. This is a simplified example that does not
    // handle tempo changes or clock drift.
    {
      // Get all slots from the queue
      while (mpQueueReader->retainSlot())
      {
      }

      // Calc the beat position we want to play back
      constexpr auto kLatencyInBeats = 4;
      const auto targetBeatsAtBufferBegin =
        sessionState.beatAtTime(hostTime, quantum) - kLatencyInBeats;

      // Drop slots that are too old
      while (mpQueueReader->numRetainedSlots() > 0)
      {
        if ((*mpQueueReader)[0]->mInfo.endBeats(sessionState, quantum)
            < targetBeatsAtBufferBegin)
        {
          mpQueueReader->releaseSlot();
          sourceReadFrame = std::nullopt;
        }
        else
        {
          break;
        }
      }

      // Return early if there is no audio buffer to read from
      if (mpQueueReader->numRetainedSlots() == 0)
      {
        return;
      }

      // Return early if the next buffer is too new
      if ((*mpQueueReader)[0]->mInfo.beginBeats(sessionState, quantum)
          > targetBeatsAtBufferBegin)
      {
        return;
      }

      // Calc the frame in the buffer that corresponds to the target beat position if we
      // are not rendering from a buffer already
      if (!sourceReadFrame)
      {
        sourceReadFrame = static_cast<size_t>(linearInterpolate(
          targetBeatsAtBufferBegin,
          *(*mpQueueReader)[0]->mInfo.beginBeats(sessionState, quantum),
          *(*mpQueueReader)[0]->mInfo.endBeats(sessionState, quantum),
          0.0,
          static_cast<double>(((*mpQueueReader)[0])->mInfo.numFrames - 1)));
      }

      // Fill the output buffer with audio from the queue
      auto frame = 0u;
      while (frame < numFrames)
      {
        // The incoming audio is formated as 16 bit integers so we have to convert it
        auto& sourceBuffer = *((*mpQueueReader)[0]);
        while (frame < numFrames && *sourceReadFrame < sourceBuffer.mInfo.numFrames)
        {
          pRightSamples[frame] = util::int16ToFloat<double>(
            sourceBuffer.mSamples[*sourceReadFrame * sourceBuffer.mInfo.numChannels]);
          ++frame;
          ++(*sourceReadFrame);
        }

        if (*sourceReadFrame == sourceBuffer.mInfo.numFrames)
        {
          // We have reached the end of the current buffer, move to the next one
          mpQueueReader->releaseSlot();
          *sourceReadFrame = 0;
          if (mpQueueReader->numRetainedSlots() == 0)
          {
            // No more buffers available, abort
            sourceReadFrame = std::nullopt;
            return;
          }
        }
      }
    }
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
    if (mpQueueWriter->retainSlot())
    {
      auto& buffer = *((*mpQueueWriter)[0]);
      buffer.mInfo = bufferHandle.info;
      buffer.mInfo.numChannels = 1;
      buffer.mInfo.numFrames = 0;
      buffer.mInfo.sampleRate = static_cast<uint32_t>(mSampleRate);

      { // Rudimentarily deal with different sample rates
        const auto ratio =
          static_cast<double>(bufferHandle.info.sampleRate) / mSampleRate;
        while (mInterpolatorPos < static_cast<double>(bufferHandle.info.numFrames))
        {
          const double weight = mInterpolatorPos - floor(mInterpolatorPos);
          buffer.mSamples[buffer.mInfo.numFrames] =
            cubicInterpolate(mInterpolatorCache, weight);
          ++buffer.mInfo.numFrames;

          if (floor(mInterpolatorPos) != floor(mInterpolatorPos + ratio))
          {
            mInterpolatorCache[0] = mInterpolatorCache[1];
            mInterpolatorCache[1] = mInterpolatorCache[2];
            mInterpolatorCache[2] = mInterpolatorCache[3];
            mInterpolatorCache[3] =
              bufferHandle.samples[static_cast<size_t>(floor(mInterpolatorPos))
                                   * bufferHandle.info.numChannels];
          }
          mInterpolatorPos += ratio;
        }

        mInterpolatorPos = std::fmod(mInterpolatorPos, 1);
      }

      mpQueueWriter->releaseSlot();
    }
  }

  Link& mLink;
  LinkAudioSink mSink;
  std::unique_ptr<LinkAudioSource> mpSource;
  double& mSampleRate;

  std::optional<size_t> sourceReadFrame;

private:
  std::shared_ptr<typename Queue::Writer> mpQueueWriter;
  std::shared_ptr<typename Queue::Reader> mpQueueReader;

  double mInterpolatorPos = 0.;
  std::array<int16_t, 4> mInterpolatorCache;
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
