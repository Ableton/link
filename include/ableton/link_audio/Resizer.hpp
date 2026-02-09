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
 *  If you would like to incorporate Link into a proprietary software
 * application, please contact <link-devs@ableton.com>.
 */

#pragma once

#include <ableton/link/Beats.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/util/Injected.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <tuple>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename SampleFormat, typename Successor, size_t KMaxNumBytes>
struct Resizer
{
  static constexpr uint32_t kSampleFormatSize =
    static_cast<uint32_t>(sizeof(SampleFormat));
  static constexpr uint32_t kMaxNumSamples = KMaxNumBytes / kSampleFormatSize;

  Resizer(util::Injected<Successor> successor)
    : mSuccessor(std::move(successor))
  {
  }

  void operator()(const SampleFormat* samples,
                  uint32_t numFrames,
                  uint32_t numChannels,
                  uint32_t sampleRate,
                  link::Beats beginBeats,
                  link::Tempo tempo,
                  Id sessionId)
  {
    if (mCachedFrames != 0
        && (numChannels != mNumChannels || sampleRate != mSampleRate
            || sessionId != mSessionId))
    {
      (*mSuccessor)(mCache.data(), mChunks, mNumChannels, mSampleRate, mSessionId);
      mCachedFrames = 0;
      mChunks.clear();
    }

    if (mCachedFrames == 0)
    {
      mSampleRate = sampleRate;
      mNumChannels = numChannels;
      mSessionId = sessionId;
      assert(mChunks.empty());
      newChunk(beginBeats, tempo);
    }
    else if (tempo != mChunks.back().tempo && beginBeats != chunkEndBeats(mChunks.back()))
    {
      newChunk(beginBeats, tempo);
    }

    for (auto frame = 0u; frame < numFrames; ++frame)
    {
      for (auto channel = 0u; channel < mNumChannels; ++channel)
      {
        mCache[mNumChannels * mCachedFrames + channel] =
          samples[mNumChannels * frame + channel];
      }
      ++mCachedFrames;
      ++mChunks.back().numFrames;


      if (mCachedFrames >= ((kMaxNumSamples / mNumChannels)))
      {
        (*mSuccessor)(mCache.data(), mChunks, mNumChannels, mSampleRate, mSessionId);
        mCachedFrames = 0;
        const auto nextChunkBeginBeats = chunkEndBeats(mChunks.back());
        mChunks.clear();

        // Always create a new chunk when needed - don't lose data
        if (frame + 1 < numFrames) // Only if there are more frames to process
        {
          newChunk(nextChunkBeginBeats, tempo);
        }
      }
    }
  }

private:
  link::Beats chunkEndBeats(const AudioBuffer::Chunk& chunk)
  {
    const auto& [count, numFrames, beginBeats, tempo] = chunk;
    const auto secondsPerBeat = 60.0 / tempo.bpm();
    const auto rangeDuration =
      static_cast<double>(numFrames) / static_cast<double>(mSampleRate);
    return beginBeats + link::Beats{rangeDuration / secondsPerBeat};
  }

  uint32_t mCachedFrames = 0;
  uint32_t mAvailableFrames = 0;

  void updateAvailableFrames()
  {
    const auto availableBytes =
      kMaxNumSamples * kSampleFormatSize - discovery::sizeInByteStream(mChunks);
    const auto bytesPerFrame = mNumChannels * kSampleFormatSize;
    const auto offsetBytes = availableBytes % bytesPerFrame;
    mAvailableFrames = (availableBytes - offsetBytes) / bytesPerFrame;
  }

  void newChunk(Beats beats, Tempo tempo)
  {
    mChunks.emplace_back(AudioBuffer::Chunk{++mCount, 0, beats, tempo});
    updateAvailableFrames();
  }

  std::array<SampleFormat, kMaxNumSamples> mCache;
  util::Injected<Successor> mSuccessor;
  uint32_t mNumChannels = 0u;
  uint32_t mSampleRate = 0u;
  Id mSessionId;

  AudioBuffer::Chunks mChunks;
  uint64_t mCount = 0;
};

} // namespace link_audio
} // namespace ableton
