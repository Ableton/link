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

template <typename Link>
class LinkAudioRenderer
{
public:
  LinkAudioRenderer(Link& link)
    : mLink(link)
    , mSink(mLink, "A Sink", 4096)
  {
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

    std::copy_n(pLeftSamples, numFrames, pRightSamples);
  }

  void toggleSource()
  {
    if (mpSource)
    {
      mpSource.reset();
    }
    else
    {
      const auto channels = mLink.channels();
      if (!channels.empty())
      {
        mpSource = std::make_unique<LinkAudioSource>(mLink, channels.front().id);
      }
    }
  }

  Link& mLink;
  LinkAudioSink mSink;
  std::unique_ptr<LinkAudioSource> mpSource;
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
  LinkAudioRenderer(Link&) {}

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
