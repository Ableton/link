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
    , mSink(mLink, "A Sink")
  {
  }

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

  Link& mLink;
  LinkAudioSink mSink;
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
