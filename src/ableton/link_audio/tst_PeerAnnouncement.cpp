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

#include <ableton/link_audio/PeerAnnouncement.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("PeerAnnouncement")
{
  SECTION("Equality")
  {
    using Random = ableton::platforms::stl::Random;

    const auto nodeId1 = link::NodeId::random<Random>();
    const auto nodeId2 = link::NodeId::random<Random>();
    const auto sessionId = link::SessionId::random<Random>();
    const auto peerInfo = PeerInfo{"TestPeer"};
    const auto channelId = Id::random<Random>();
    const auto channels =
      ChannelAnnouncements{{ChannelAnnouncement{"Channel1", channelId}}};

    const auto announcement1 = PeerAnnouncement{nodeId1, sessionId, peerInfo, channels};
    const auto announcement2 = PeerAnnouncement{nodeId1, sessionId, peerInfo, channels};
    const auto announcement3 = PeerAnnouncement{nodeId2, sessionId, peerInfo, channels};

    CHECK(announcement1 == announcement2);
    CHECK(!(announcement1 == announcement3));
  }

  SECTION("Roundtrip")
  {
    using Random = ableton::platforms::stl::Random;

    const auto nodeId = link::NodeId::random<Random>();
    const auto sessionId = link::SessionId::random<Random>();
    const auto peerInfo = PeerInfo{"TestPeer"};
    const auto channelId1 = Id::random<Random>();
    const auto channelId2 = Id::random<Random>();
    const auto channels =
      ChannelAnnouncements{{ChannelAnnouncement{"Channel1", channelId1},
                            ChannelAnnouncement{"Channel2", channelId2}}};

    const auto announcement = PeerAnnouncement{nodeId, sessionId, peerInfo, channels};

    auto payload = toPayload(announcement);
    std::vector<uint8_t> byteStream(sizeInByteStream(payload));
    const auto end = toNetworkByteStream(payload, byteStream.begin());
    CHECK(byteStream.end() == end);

    const auto deserialized =
      PeerAnnouncement::fromPayload(nodeId, byteStream.begin(), byteStream.end());

    CHECK(announcement == deserialized);
    CHECK(deserialized.nodeId == nodeId);
    CHECK(deserialized.sessionId == sessionId);
    CHECK(deserialized.peerInfo == peerInfo);
    CHECK(deserialized.channels == channels);
  }
}

} // namespace link_audio
} // namespace ableton
