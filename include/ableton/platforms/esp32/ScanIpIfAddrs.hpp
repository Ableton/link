/* Copyright 2020, Ableton AG, Berlin and 2019, Mathias Bredholt, Torso Electronics,
 * Copenhagen. All rights reserved.
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

#include <ableton/discovery/AsioTypes.hpp>
#include <arpa/inet.h>
#include <esp_idf_version.h>
#include <esp_netif.h>
#include <net/if.h>
#include <vector>

// esp_netif_next_unsafe() was introduced in ESP-IDF v5.5 as a rename of
// esp_netif_next() to make the thread-unsafety explicit.
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
#define esp_netif_next_unsafe esp_netif_next
#endif

namespace ableton
{
namespace platforms
{
namespace esp32
{

// ESP32 implementation of ip interface address scanner.
// Uses esp_netif_tcpip_exec() to iterate network interfaces safely from
// the TCPIP thread, where the interface list is guaranteed stable.
// esp_netif_tcpip_exec() is available in ESP-IDF v5.3 and later.
struct ScanIpIfAddrs
{
  std::vector<discovery::IpAddress> operator()()
  {
    std::vector<discovery::IpAddress> addrs;
    esp_netif_tcpip_exec(
      [](void* ctx) -> esp_err_t
      {
        auto& out = *static_cast<std::vector<discovery::IpAddress>*>(ctx);
        // Get first network interface
        esp_netif_t* esp_netif = esp_netif_next_unsafe(NULL);
        while (esp_netif)
        {
          // Check if interface is active
          if (esp_netif_is_netif_up(esp_netif))
          {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(esp_netif, &ip_info);
            out.emplace_back(::asio::ip::address_v4(ntohl(ip_info.ip.addr)));
          }
          // Get next network interface
          esp_netif = esp_netif_next_unsafe(esp_netif);
        }
        return ESP_OK;
      },
      &addrs);
    return addrs;
  }
};

} // namespace esp32
} // namespace platforms
} // namespace ableton
