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

#define LINK_AUDIO YES

#include <ableton/link_audio/ApiConfig.hpp>

namespace ableton
{

template <typename Clock>
class BasicLinkAudio : public BasicLink<Clock>
{
public:
  BasicLinkAudio(double bpm);

private:
  using Controller = ableton::link::ApiController<Clock>;
};

class LinkAudio : public BasicLinkAudio<link::platform::Clock>
{
public:
  using Clock = link::platform::Clock;

  LinkAudio(double bpm)
    : BasicLinkAudio<Clock>(bpm)
  {
  }
};

} // namespace ableton

#include <ableton/LinkAudio.ipp>
