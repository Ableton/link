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
#include <ableton/link/Phase.hpp>
#include <ableton/link/Timeline.hpp>

namespace ableton
{
namespace link_audio
{

static inline link::Beats globalBeatAtBeat(const link::Timeline& timeline,
                                           link::Beats targetBeat,
                                           link::Beats quantum)
{
  const auto beatsDiff =
    link::toPhaseEncodedBeats(timeline, timeline.timeOrigin, quantum);
  return targetBeat - beatsDiff;
}

static inline link::Beats beatAtGlobalBeat(const link::Timeline& timeline,
                                           link::Beats globalBeat,
                                           link::Beats quantum)
{
  const auto beatsDiff =
    link::toPhaseEncodedBeats(timeline, timeline.timeOrigin, quantum);
  return globalBeat + beatsDiff;
}

} // namespace link_audio
} // namespace ableton
