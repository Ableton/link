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

#include <ableton/link/NodeId.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <sstream>

namespace ableton
{
namespace link
{

TEST_CASE("NodeId")
{
  SECTION("StreamState")
  {
    NodeId nodeId{};
    nodeId.fill(0xAB);

    std::ostringstream stream;

    // Set up some non-default stream state
    stream << std::dec << std::setfill('X') << std::setw(5) << 42;
    // After setw, width resets to 0, but fill and flags should persist

    // Capture stream state before outputting NodeId
    const auto flagsBefore = stream.flags();
    const auto fillBefore = stream.fill();

    // Output the NodeId
    stream << nodeId;

    // Check stream state after outputting NodeId
    const auto flagsAfter = stream.flags();
    const auto fillAfter = stream.fill();

    CHECK(flagsBefore == flagsAfter);
    CHECK(fillBefore == fillAfter);
  }


  SECTION("StreamOutputFormat")
  {
    NodeId nodeId{};
    nodeId.fill(0x00);
    nodeId[0] = 0x01;
    nodeId[7] = 0xFF;

    std::ostringstream stream;
    stream << nodeId;

    CHECK(stream.str() == "0x01000000000000ff");
  }

  SECTION("StreamStateVisual")
  {
    NodeId nodeId{};
    nodeId.fill(0xAB);

    std::ostringstream stream;

    // Output something before NodeId with specific formatting
    stream << std::setfill('_') << std::setw(5) << 42;
    CHECK(stream.str() == "___42");

    // Output the NodeId
    stream << " " << nodeId << " ";

    // Now output something after - it should use decimal and default fill
    // If stream state is corrupted, this will be in hex and/or use '0' fill
    stream << std::setw(5) << 15;

    // Expected: "___42 0xabababababababab ___15"
    // If corrupted: "___42 0xabababababababab 0000f" (hex + '0' fill)
    CHECK(stream.str() == "___42 0xabababababababab ___15");
  }
}

} // namespace link
} // namespace ableton
