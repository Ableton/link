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
#include <cstdint>
#include <string>
#include <tuple>

namespace ableton
{
namespace link_audio
{

struct PeerInfo
{
  static const std::int32_t key = '__pi';
  static_assert(key == 0x5f5f7069, "Unexpected byte order");

  friend bool operator==(const PeerInfo& lhs, const PeerInfo& rhs)
  {
    return std::tie(lhs.name) == std::tie(rhs.name);
  }

  friend bool operator!=(const PeerInfo& lhs, const PeerInfo& rhs)
  {
    return !(lhs == rhs);
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const PeerInfo& tl)
  {
    return discovery::sizeInByteStream(tl.name);
  }

  template <typename It>
  friend It toNetworkByteStream(const PeerInfo& tl, It out)
  {
    return discovery::toNetworkByteStream(tl.name, std::move(out));
  }

  template <typename It>
  static std::pair<PeerInfo, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    using namespace discovery;
    PeerInfo peerInfo;
    auto result =
      Deserialize<std::string>::fromNetworkByteStream(std::move(begin), std::move(end));
    peerInfo.name = std::move(result.first);
    return make_pair(std::move(peerInfo), std::move(result.second));
  }

  std::string name;
};

} // namespace link_audio
} // namespace ableton
