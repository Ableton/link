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

#include <ableton/link_audio/ChannelId.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("ChannelId | RoundtripByteStreamEncoding", "[ChannelId]")
{
  using Random = ableton::platforms::stl::Random;

  const auto cid = ChannelId{Id::random<Random>()};

  auto byteStream = std::vector<uint8_t>(sizeInByteStream(cid));
  const auto serializedEnd = toNetworkByteStream(cid, byteStream.begin());
  CHECK(byteStream.end() == serializedEnd);

  const auto [deserialized, deserializedEnd] =
    ChannelId::fromNetworkByteStream(byteStream.begin(), byteStream.end());
  CHECK(cid == deserialized);
  CHECK(byteStream.end() == deserializedEnd);
}

} // namespace link_audio
} // namespace ableton
