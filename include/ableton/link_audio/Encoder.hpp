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

#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/PCMCodec.hpp>
#include <ableton/link_audio/Resizer.hpp>
#include <ableton/util/Injected.hpp>
#include <array>
#include <memory>
#include <stdexcept>

namespace ableton
{
namespace link_audio
{

template <typename Sender, typename SampleFormat>
struct Encoder
{
  // TODO: Find the best size for audio buffer messages
  // For now we take RFC 791 as a reference. Nodes must be able to process IP messages of
  // at least 576 bytes.
  static constexpr uint32_t kMaxAudioBytes =
    576 - v1::kHeaderSize - AudioBuffer::kNonAudioBytes;
  static_assert(kMaxAudioBytes <= v1::kMaxPayloadSize);

  Encoder(util::Injected<Sender> sender, Id channelId)
    : mProcessor(
        util::injectVal(PCMEncoder<SampleFormat, Sender>(std::move(sender), channelId)))
  {
  }

  void operator()(const Buffer<SampleFormat>& input)
  {
    mProcessor(input.mSamples.data(),
               input.mNumFrames,
               input.mNumChannels,
               input.mSampleRate,
               input.mBeginBeats,
               input.mTempo,
               input.mSessionId);
  }

private:
  Resizer<SampleFormat, PCMEncoder<SampleFormat, Sender>, kMaxAudioBytes> mProcessor;
};

} // namespace link_audio
} // namespace ableton
