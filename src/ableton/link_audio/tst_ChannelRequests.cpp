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

#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("ChannelRequest | RoundtripByteStreamEncoding", "[ChannelRequests]")
{
  using Random = ableton::platforms::stl::Random;

  const auto request = ChannelRequest{Id::random<Random>(), Id::random<Random>()};

  auto payload = toPayload(request);

  std::vector<std::uint8_t> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  const auto result = ChannelRequest::fromPayload(request.peerId, bytes.begin(), end);
  CHECK(request == result);
}

TEST_CASE("ChannelStopRequest | RoundtripByteStreamEncoding", "[ChannelRequests]")
{
  using Random = ableton::platforms::stl::Random;

  const auto request = ChannelStopRequest{Id::random<Random>(), Id::random<Random>()};

  auto payload = toPayload(request);

  std::vector<std::uint8_t> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  const auto result = ChannelStopRequest::fromPayload(request.peerId, bytes.begin(), end);
  CHECK(request == result);
}

} // namespace link_audio
} // namespace ableton
