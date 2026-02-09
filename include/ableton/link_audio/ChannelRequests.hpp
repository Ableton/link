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

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/discovery/Payload.hpp>
#include <ableton/link_audio/ChannelId.hpp>

namespace ableton
{
namespace link_audio
{

struct ChannelRequest
{
  using Payload = decltype(discovery::makePayload(ChannelId{}));

  friend bool operator==(const ChannelRequest& lhs, const ChannelRequest& rhs)
  {
    return std::tie(lhs.peerId, lhs.channelId) == std::tie(rhs.peerId, rhs.channelId);
  }

  friend Payload toPayload(const ChannelRequest& request)
  {
    return discovery::makePayload(ChannelId{request.channelId});
  }

  template <typename It>
  static ChannelRequest fromPayload(Id peerId, It begin, It end)
  {
    using namespace std;
    auto request = ChannelRequest{std::move(peerId)};
    discovery::parsePayload<ChannelId>(std::move(begin),
                                       std::move(end),
                                       [&request](ChannelId cid)
                                       { request.channelId = std::move(cid.id); });
    return request;
  }

  Id peerId;
  Id channelId;
};

struct ChannelStopRequest
{
  using Payload = decltype(discovery::makePayload(ChannelId{}));

  friend bool operator==(const ChannelStopRequest& lhs, const ChannelStopRequest& rhs)
  {
    return std::tie(lhs.peerId, lhs.channelId) == std::tie(rhs.peerId, rhs.channelId);
  }

  friend Payload toPayload(const ChannelStopRequest& request)
  {
    return discovery::makePayload(ChannelId{request.channelId});
  }

  template <typename It>
  static ChannelStopRequest fromPayload(Id peerId, It begin, It end)
  {
    using namespace std;
    auto request = ChannelStopRequest{std::move(peerId), {}};
    discovery::parsePayload<ChannelId>(std::move(begin),
                                       std::move(end),
                                       [&request](ChannelId cid)
                                       { request.channelId = std::move(cid.id); });
    return request;
  }

  Id peerId;
  Id channelId;
};

} // namespace link_audio
} // namespace ableton
