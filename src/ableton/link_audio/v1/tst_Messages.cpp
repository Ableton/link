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

#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>

namespace ableton
{
namespace link_audio
{
namespace v1
{

TEST_CASE("ParseEmptyBuffer", "[Messages]")
{
  const std::array<uint8_t, 0> buffer{};
  auto result = parseMessageHeader(begin(buffer), end(buffer));
  CHECK(kInvalid == result.first.messageType);
  CHECK(begin(buffer) == result.second);
}

TEST_CASE("ParseTruncatedMessageHeader", "[Messages]")
{
  const std::array<uint8_t, 10> truncated = {
    {'c', 'h', 'n', 'n', 'l', 's', 'v', 1, 'x', 'y'}};
  const auto result = parseMessageHeader(begin(truncated), end(truncated));
  CHECK(kInvalid == result.first.messageType);
  const auto consumedBytes = (begin(truncated) != result.second);
  CHECK_FALSE(consumedBytes);
}

TEST_CASE("MissingProtocolHeader", "[Messages]")
{
  // Buffer should be large enough to proceed but shouldn't match the protocol header
  const std::array<uint8_t, 32> zeros{};
  auto result = parseMessageHeader(begin(zeros), end(zeros));
  CHECK(kInvalid == result.first.messageType);
  CHECK(begin(zeros) == result.second);
}

TEST_CASE("RoundtripAudioBufferNoPayload", "[Messages]")
{
  using Random = ableton::platforms::stl::Random;

  std::array<uint8_t, kMaxMessageSize> buffer{};
  const auto nodeId = link::NodeId::random<Random>();
  const auto endMessage =
    audioBufferMessage(nodeId, discovery::makePayload(), begin(buffer));
  const auto result = parseMessageHeader(begin(buffer), endMessage);
  CHECK(endMessage == result.second);
  CHECK(kAudioBuffer == result.first.messageType);
  CHECK(0 == result.first.ttl);
  CHECK(nodeId == result.first.ident);
}


} // namespace v1
} // namespace link_audio
} // namespace ableton
