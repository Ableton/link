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

#include <ableton/link/Timeline.hpp>
#include <ableton/link_audio/BeatTimeMapping.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link_audio
{

TEST_CASE("BeatTimeMapping")
{
  const auto timeline = link::Timeline{
    link::Tempo{120.}, link::Beats{5.5678}, std::chrono::microseconds{12558940}};

  SECTION("Roundtrip")
  {
    const auto beats = link::Beats{234.567};
    const auto quantum = link::Beats{4.};
    const auto globalBeats = globalBeatAtBeat(timeline, beats, quantum);
    const auto localBeats = beatAtGlobalBeat(timeline, globalBeats, quantum);
    CHECK(beats.floating() == Approx(localBeats.floating()));
  }
}

} // namespace link_audio
} // namespace ableton
