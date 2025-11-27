/* Copyright 2016, Ableton AG, Berlin. All rights reserved.
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <string>

namespace ableton
{
namespace link
{

using NodeIdArray = std::array<std::uint8_t, 8>;

struct NodeId : NodeIdArray
{
  NodeId() = default;

  NodeId(NodeIdArray rhs)
    : NodeIdArray(std::move(rhs))
  {
  }

  template <typename Random>
  static NodeId random()
  {
    using namespace std;
    NodeId nodeId;

    Random random;
    generate(nodeId.begin(), nodeId.end(), [&] { return random(); });

    return nodeId;
  }

  friend std::ostream& operator<<(std::ostream& stream, const NodeId& id)
  {
    const auto flags = stream.flags();
    const auto fill = stream.fill();

    stream << "0x" << std::hex << std::setfill('0');
    for (const auto byte : id)
    {
      stream << std::setw(2) << static_cast<int>(byte);
    }

    stream.flags(flags);
    stream.fill(fill);
    return stream;
  }

  template <typename It>
  friend It toNetworkByteStream(const NodeId& nodeId, It out)
  {
    return discovery::toNetworkByteStream(nodeId, std::move(out));
  }

  template <typename It>
  static std::pair<NodeId, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto result = discovery::Deserialize<NodeIdArray>::fromNetworkByteStream(
      std::move(begin), std::move(end));
    return make_pair(NodeId(std::move(result.first)), std::move(result.second));
  }
};

} // namespace link
} // namespace ableton
