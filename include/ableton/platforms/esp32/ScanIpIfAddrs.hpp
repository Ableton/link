/* Copyright 2019, Mathias Bredholt, Torso Electronics, Copenhagen. All rights reserved.
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
 */

#pragma once

#include <ableton/platforms/asio/AsioWrapper.hpp>
#include <arpa/inet.h>
#include <net/if.h>
#include <vector>

#include "tcpip_adapter.h"

namespace ableton
{
namespace platforms
{
namespace esp32
{

// ESP32 implementation of ip interface address scanner
struct ScanIpIfAddrs
{
  std::vector<::asio::ip::address> operator()()
  {
    std::vector<::asio::ip::address> addrs;
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    addrs.emplace_back(::asio::ip::address_v4(ntohl(ip_info.ip.addr)));
    return addrs;
  }
};

} // namespace esp32
} // namespace platforms
} // namespace ableton
