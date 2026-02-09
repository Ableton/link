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

#include <ableton/discovery/Payload.hpp>
#include <ableton/link/NodeId.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link_audio/PeerInfo.hpp>

namespace ableton
{
namespace link_audio
{

struct PeerAnnouncement
{
  using IdType = link::NodeId;

  using Payload = decltype(discovery::makePayload(link::SessionMembership{}, PeerInfo{}));

  link::NodeId ident() const { return nodeId; }

  friend bool operator==(const PeerAnnouncement& lhs, const PeerAnnouncement& rhs)
  {
    return std::tie(lhs.nodeId, lhs.sessionId, lhs.peerInfo)
           == std::tie(rhs.nodeId, rhs.sessionId, rhs.peerInfo);
  }

  friend Payload toPayload(const PeerAnnouncement& announcement)
  {
    return discovery::makePayload(
      link::SessionMembership{announcement.sessionId}, announcement.peerInfo);
  }

  template <typename It>
  static PeerAnnouncement fromPayload(link::NodeId nodeId, It begin, It end)
  {
    using namespace std;
    auto announcement = PeerAnnouncement{std::move(nodeId), {}, {}};
    discovery::parsePayload<link::SessionMembership, PeerInfo>(
      std::move(begin),
      std::move(end),
      [&announcement](link::SessionMembership membership)
      { announcement.sessionId = std::move(membership.sessionId); },
      [&announcement](PeerInfo pi) { announcement.peerInfo = std::move(pi); });
    return announcement;
  }

  link::NodeId nodeId;
  link::SessionId sessionId;
  PeerInfo peerInfo;
};

} // namespace link_audio
} // namespace ableton
