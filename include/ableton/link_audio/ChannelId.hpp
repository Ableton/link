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
#include <ableton/link_audio/Id.hpp>

namespace ableton
{
namespace link_audio
{

struct ChannelId
{
  static const std::int32_t key = 'chid';
  static_assert(key == 0x63686964, "Unexpected byte order");

  friend bool operator==(const ChannelId& lhs, const ChannelId& rhs)
  {
    return lhs.id == rhs.id;
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const ChannelId& cid)
  {
    return discovery::sizeInByteStream(cid.id);
  }

  template <typename It>
  friend It toNetworkByteStream(const ChannelId& cid, It out)
  {
    return discovery::toNetworkByteStream(cid.id, std::move(out));
  }

  template <typename It>
  static std::pair<ChannelId, It> fromNetworkByteStream(It begin, It end)
  {
    auto [result, itEnd] = discovery::Deserialize<Id>::fromNetworkByteStream(begin, end);
    return std::make_pair(ChannelId{std::move(result)}, itEnd);
  }

  Id id;
};

} // namespace link_audio
} // namespace ableton
