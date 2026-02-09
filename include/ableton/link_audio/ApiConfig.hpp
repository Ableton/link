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

#ifndef LINK_API_CONTROLLER

#include <ableton/platforms/Config.hpp>

#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/SessionController.hpp>
#include <ableton/link_audio/Sink.hpp>

namespace ableton
{
namespace link
{

template <typename Clock>
using ApiController = ableton::link_audio::SessionController<PeerCountCallback,
                                                             TempoCallback,
                                                             StartStopStateCallback,
                                                             Clock,
                                                             platform::Random,
                                                             platform::IoContext>;

} // namespace link

using ChannelId = link_audio::Id;
using PeerId = link_audio::Id;

} // namespace ableton

#define LINK_API_CONTROLLER YES

#endif

#include <ableton/Link.hpp>
