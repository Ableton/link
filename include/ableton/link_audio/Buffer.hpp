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

#include <ableton/link/Beats.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link_audio/Id.hpp>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename SampleFormat>
struct Buffer
{
  using SampleFormatType = SampleFormat;
  using Samples = std::vector<SampleFormat>;

  Buffer(uint32_t numSamples)
    : mSamples(numSamples)
  {
  }

  uint32_t mSampleRate;
  uint32_t mNumChannels;
  uint32_t mNumFrames;
  Samples mSamples;
  link::Beats mBeginBeats;
  link::Tempo mTempo;
  uint64_t mCount;
  Id mSessionId;
};

template <typename Buffer>
struct BufferCallbackHandle
{
  BufferCallbackHandle(const Buffer& buffer, typename Buffer::SampleFormatType* pSamples)
    : mBuffer(buffer)
    , mpSamples(pSamples)
  {
  }

  const Buffer& mBuffer;
  typename Buffer::SampleFormatType* mpSamples;
};

} // namespace link_audio
} // namespace ableton
