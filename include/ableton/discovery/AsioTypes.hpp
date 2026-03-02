/* Copyright 2023, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/platforms/asio/AsioWrapper.hpp>
#include <ostream>
#include <variant>

namespace ableton
{
namespace discovery
{

using IpAddress = LINK_ASIO_NAMESPACE::ip::address;
using IpAddressV4 = LINK_ASIO_NAMESPACE::ip::address_v4;
using IpAddressV6 = LINK_ASIO_NAMESPACE::ip::address_v6;
using NetworkV4 = LINK_ASIO_NAMESPACE::ip::network_v4;
using UdpSocket = LINK_ASIO_NAMESPACE::ip::udp::socket;
using UdpEndpoint = LINK_ASIO_NAMESPACE::ip::udp::endpoint;

// An interface address is either a v4 network (address + prefix length)
// or a bare v6 address. This bundles the prefix information that was
// previously obtained via a separate prefixLengthForAddress() call.
// Implemented as a struct wrapping std::variant because ASIO's NetworkV4
// does not provide operator<, which std::variant comparison requires.
struct InterfaceAddress
{
  std::variant<NetworkV4, IpAddressV6> value;

  InterfaceAddress() = default;
  InterfaceAddress(NetworkV4 net)
    : value(std::move(net))
  {
  }
  InterfaceAddress(IpAddressV6 addr)
    : value(std::move(addr))
  {
  }

  friend bool operator==(const InterfaceAddress& a, const InterfaceAddress& b)
  {
    return a.value == b.value;
  }
  friend bool operator!=(const InterfaceAddress& a, const InterfaceAddress& b)
  {
    return !(a == b);
  }
  friend bool operator<(const InterfaceAddress& a, const InterfaceAddress& b)
  {
    if (a.value.index() != b.value.index())
    {
      return a.value.index() < b.value.index();
    }
    if (auto* lhs = std::get_if<NetworkV4>(&a.value))
    {
      const auto& rhs = std::get<NetworkV4>(b.value);
      if (lhs->address() != rhs.address())
      {
        return lhs->address() < rhs.address();
      }
      return lhs->prefix_length() < rhs.prefix_length();
    }
    return std::get<IpAddressV6>(a.value) < std::get<IpAddressV6>(b.value);
  }
  friend std::ostream& operator<<(std::ostream& os, const InterfaceAddress& ifAddr)
  {
    std::visit([&os](const auto& addr) { os << addr; }, ifAddr.value);
    return os;
  }
};

// Extract the IpAddress from an InterfaceAddress
inline IpAddress toIpAddress(const InterfaceAddress& ifAddr)
{
  return std::visit(
    [](const auto& addr) -> IpAddress
    {
      if constexpr (std::is_same_v<std::decay_t<decltype(addr)>, NetworkV4>)
      {
        return addr.address();
      }
      else
      {
        return addr;
      }
    },
    ifAddr.value);
}

template <typename... Args>
inline NetworkV4 makeNetworkV4(Args&&... args)
{
  return LINK_ASIO_NAMESPACE::ip::make_network_v4(std::forward<Args>(args)...);
}

template <typename... Args>
inline IpAddress makeAddress(Args&&... args)
{
  return LINK_ASIO_NAMESPACE::ip::make_address(std::forward<Args>(args)...);
}

template <typename AsioAddrType>
AsioAddrType makeAddressFromBytes(const char* pAddr)
{
  using namespace std;
  typename AsioAddrType::bytes_type bytes;
  copy(pAddr, pAddr + bytes.size(), begin(bytes));
  return AsioAddrType{bytes};
}

template <typename AsioAddrType>
AsioAddrType makeAddressFromBytes(const char* pAddr, uint32_t scopeId)
{
  using namespace std;
  typename AsioAddrType::bytes_type bytes;
  copy(pAddr, pAddr + bytes.size(), begin(bytes));
  return AsioAddrType{bytes, scopeId};
}

} // namespace discovery
} // namespace ableton
