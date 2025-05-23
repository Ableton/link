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

#include <ableton/discovery/AsioTypes.hpp>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <map>
#include <net/if.h>
#include <string>
#include <vector>

namespace ableton
{
namespace platforms
{
namespace posix
{
namespace detail
{

// RAII type to make [get,free]ifaddrs function pairs exception safe
class GetIfAddrs
{
public:
  GetIfAddrs()
  {
    if (getifaddrs(&interfaces)) // returns 0 on success
    {
      interfaces = NULL;
    }
  }
  ~GetIfAddrs()
  {
    if (interfaces)
      freeifaddrs(interfaces);
  }

  // RAII must not copy
  GetIfAddrs(GetIfAddrs&) = delete;
  GetIfAddrs& operator=(GetIfAddrs&) = delete;

  template <typename Function>
  void withIfAddrs(Function f)
  {
    if (interfaces)
      f(*interfaces);
  }

private:
  struct ifaddrs* interfaces = NULL;
};

} // namespace detail


// Posix implementation of ip interface address scanner
struct ScanIpIfAddrs
{
  // Scan active network interfaces and return corresponding addresses
  // for all ip-based interfaces.
  std::vector<discovery::IpAddress> operator()()
  {
    std::vector<discovery::IpAddress> addrs;
    std::map<std::string, discovery::IpAddress> IpInterfaceNames;

    detail::GetIfAddrs getIfAddrs;
    getIfAddrs.withIfAddrs([&addrs, &IpInterfaceNames](const struct ifaddrs& interfaces) {
      const struct ifaddrs* interface;
      for (interface = &interfaces; interface; interface = interface->ifa_next)
      {
        const auto addr =
          reinterpret_cast<const struct sockaddr_in*>(interface->ifa_addr);
        if (addr && interface->ifa_flags & IFF_RUNNING && addr->sin_family == AF_INET)
        {
          const auto bytes = reinterpret_cast<const char*>(&addr->sin_addr);
          const auto address =
            discovery::makeAddressFromBytes<discovery::IpAddressV4>(bytes);
          addrs.emplace_back(address);
          IpInterfaceNames.insert(std::make_pair(interface->ifa_name, address));
        }
      }
    });

    getIfAddrs.withIfAddrs([&addrs, &IpInterfaceNames](const struct ifaddrs& interfaces) {
      const struct ifaddrs* interface;
      for (interface = &interfaces; interface; interface = interface->ifa_next)
      {
        const auto addr =
          reinterpret_cast<const struct sockaddr_in*>(interface->ifa_addr);
        if (IpInterfaceNames.find(interface->ifa_name) != IpInterfaceNames.end() && addr
            && interface->ifa_flags & IFF_RUNNING && addr->sin_family == AF_INET6)
        {
          const auto addr6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
          const auto bytes = reinterpret_cast<const char*>(&addr6->sin6_addr);
          const auto scopeId = addr6->sin6_scope_id;
          const auto address =
            discovery::makeAddressFromBytes<discovery::IpAddressV6>(bytes, scopeId);
          if (!address.is_loopback() && address.is_link_local())
          {
            addrs.emplace_back(address);
          }
        }
      }
    });

    return addrs;
  };
};

} // namespace posix
} // namespace platforms
} // namespace ableton
