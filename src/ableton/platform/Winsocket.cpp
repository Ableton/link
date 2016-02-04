// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/platforms/windows/Winsocket.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MAX_TRIES 3 // MSFT recommendation
#define WORKING_BUFFER_SIZE 15000 // MSFT recommendation

namespace
{

// Utility for making v4 or v6 ip addresses from raw bytes in network byte-order
template <typename BoostAddrType>
BoostAddrType makeAddress(const char* pAddr)
{
  using namespace std;
  typename BoostAddrType::bytes_type bytes;
  copy(pAddr, pAddr + bytes.size(), begin(bytes));
  return BoostAddrType{ bytes };
}

// RAII type to make [get,free]ifaddrs function pairs exception safe
struct GetIfAddrs
{
  GetIfAddrs()
  {
    DWORD adapter_addrs_buffer_size = WORKING_BUFFER_SIZE;
    for (int i = 0; i < MAX_TRIES; i++)
    {
      adapter_addrs = (IP_ADAPTER_ADDRESSES*)malloc(adapter_addrs_buffer_size);
      assert(adapter_addrs);

      DWORD error = ::GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER |
        GAA_FLAG_SKIP_FRIENDLY_NAME,
        NULL,
        adapter_addrs,
        &adapter_addrs_buffer_size);

      if (error == ERROR_SUCCESS)
      {
        break;
      }
      // if buffer too small, use new buffer size in next iteration
      if (error == ERROR_BUFFER_OVERFLOW)
      {
        free(adapter_addrs);
        adapter_addrs = NULL;
        continue;
      }
    }
  }
  ~GetIfAddrs() { if (adapter_addrs) free(adapter_addrs); }

  // RAII must not copy
  GetIfAddrs(GetIfAddrs&) = delete;
  GetIfAddrs& operator=(GetIfAddrs&) = delete;

  template <typename Function>
  void withIfAddrs(Function f) { if (adapter_addrs) f(*adapter_addrs); }

private:
  IP_ADAPTER_ADDRESSES* adapter_addrs;
  IP_ADAPTER_ADDRESSES* adapter;
};

} // anonymous namespace

namespace ableton
{
namespace link
{
namespace platform
{

std::vector<asio::ip::address> Winsocket::scanIpIfAddrs()
{
  std::vector<asio::ip::address> addrs;

  GetIfAddrs getIfAddrs;
  getIfAddrs.withIfAddrs(
    [&](const IP_ADAPTER_ADDRESSES& interfaces)
  {
    const IP_ADAPTER_ADDRESSES* networkInterface;
    for (networkInterface = &interfaces; networkInterface; networkInterface =
      networkInterface->Next)
    {
      for (
        IP_ADAPTER_UNICAST_ADDRESS* address = networkInterface->FirstUnicastAddress;
        NULL != address;
        address = address->Next)
      {
        auto family = address->Address.lpSockaddr->sa_family;
        if (AF_INET == family)
        {
          // IPv4
          SOCKADDR_IN* addr4 = reinterpret_cast<SOCKADDR_IN*>(
            address->Address.lpSockaddr);
          auto bytes = reinterpret_cast<const char*>(&addr4->sin_addr);
          addrs.emplace_back(makeAddress<asio::ip::address_v4>(bytes));
        }
        else if (AF_INET6 == family)
        {
          SOCKADDR_IN6* addr6 = reinterpret_cast<SOCKADDR_IN6*>(
            address->Address.lpSockaddr);
          auto bytes = reinterpret_cast<const char*>(&addr6->sin6_addr);
          addrs.emplace_back(makeAddress<asio::ip::address_v6>(bytes));
        }
      }
    }
  });
  return addrs;
}

} // namespace platform
} // namespace link
} // namespace ableton
