/* Copyright 2026, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/link/ArrangementTimeline.hpp>
#include <ableton/link/Timeline.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{

TEST_CASE("ArrangementTimeline | RoundtripByteStreamEncoding")
{
  using std::chrono::microseconds;
  const auto originalTimeline =
    ArrangementTimeline{Beats{123.456789}, Beats{42.5}, microseconds{987654}};
  std::vector<std::uint8_t> bytes(sizeInByteStream(originalTimeline));
  const auto serializedEndIter = toNetworkByteStream(originalTimeline, begin(bytes));
  const auto deserialized =
    ArrangementTimeline::fromNetworkByteStream(begin(bytes), serializedEndIter);
  CHECK(originalTimeline == deserialized.first);
}

} // namespace link
} // namespace ableton
