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

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/Buffer.hpp>
#include <ableton/util/Injected.hpp>
#include <memory>

namespace ableton
{
namespace link_audio
{

template <typename SampleFormat, typename Sender>
struct PCMEncoder
{
  PCMEncoder(util::Injected<Sender> sender, Id channelId)
    : mSender(std::move(sender))
  {
    mOutputBuffer.channelId = channelId;
  }

  void operator()(const SampleFormat* samples,
                  const AudioBuffer::Chunks& chunks,
                  const uint32_t numChannels,
                  const uint32_t sampleRate,
                  const Id sessionId)
  {
    mOutputBuffer.chunks = chunks;
    mOutputBuffer.codec = Codec::kPCM_i16;
    mOutputBuffer.sampleRate = sampleRate;
    mOutputBuffer.numChannels = numChannels;
    mOutputBuffer.sessionId = sessionId;

    auto it = mOutputBuffer.bytes.begin();
    const auto numSamples = mOutputBuffer.numFrames() * mOutputBuffer.numChannels;
    assert(numSamples * sizeof(int16_t) <= AudioBuffer::kMaxAudioBytes);
    for (auto sample = 0u; sample < numSamples; ++sample)
    {
      it = discovery::toNetworkByteStream(samples[sample], it);
    }
    mOutputBuffer.numBytes =
      static_cast<uint32_t>(std::distance(mOutputBuffer.bytes.begin(), it));

    (*mSender)(mOutputBuffer);
  }

  AudioBuffer mOutputBuffer;
  util::Injected<Sender> mSender;
};

template <typename SampleFormat, typename Successor>
struct PCMDecoder
{
  PCMDecoder(util::Injected<Successor> successor, size_t cacheSize)
    : mBuffer(cacheSize)
    , mSuccessor(std::move(successor))
  {
  }

  void operator()(const AudioBuffer& input)
  {
    auto it = input.bytes.begin();
    auto numSamples = 0u;
    auto inputEnd = input.bytes.begin() + input.numBytes;
    while (it != inputEnd)
    {
      auto [sample, next] =
        discovery::Deserialize<SampleFormat>::fromNetworkByteStream(it, inputEnd);
      mBuffer.mSamples[numSamples] = sample;
      ++numSamples;
      it = next;
    }

    auto pSamples = mBuffer.mSamples.data();

    for (const auto& [count, numFrames, beginBeats, tempo] : input.chunks)
    {
      mBuffer.mNumFrames = numFrames;
      mBuffer.mNumChannels = input.numChannels;
      mBuffer.mSampleRate = input.sampleRate;
      mBuffer.mBeginBeats = beginBeats;
      mBuffer.mTempo = tempo;
      mBuffer.mCount = count;
      mBuffer.mSessionId = input.sessionId;

      (*mSuccessor)(BufferCallbackHandle<Buffer<SampleFormat>>{mBuffer, pSamples});
      pSamples += numFrames * input.numChannels;
    }
  }

  Buffer<SampleFormat> mBuffer;
  util::Injected<Successor> mSuccessor;
};

} // namespace link_audio
} // namespace ableton
