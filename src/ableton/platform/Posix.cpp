// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/platform/Posix.hpp>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>

namespace
{

// Utility for making v4 or v6 ip addresses from raw bytes in network byte-order
template <typename BoostAddrType>
BoostAddrType makeAddress(const char* pAddr)
{
  using namespace std;
  typename BoostAddrType::bytes_type bytes;
  copy(pAddr, pAddr + bytes.size(), begin(bytes));
  return BoostAddrType{bytes};
}

// RAII type to make [get,free]ifaddrs function pairs exception safe
struct GetIfAddrs
{
  GetIfAddrs()
  {
    if (getifaddrs(&interfaces)) // returns 0 on success
    {
      interfaces = NULL;
    }
  }
  ~GetIfAddrs() { if (interfaces) freeifaddrs(interfaces); }

  // RAII must not copy
  GetIfAddrs(GetIfAddrs&) = delete;
  GetIfAddrs& operator=(GetIfAddrs&) = delete;

  template <typename Function>
  void withIfAddrs(Function f) { if (interfaces) f(*interfaces); }

private:
  struct ifaddrs* interfaces = NULL;
};

} // anonymous namespace

namespace ableton
{
namespace link
{
namespace platform
{

std::vector<asio::ip::address> Posix::scanIpIfAddrs()
{
  std::vector<asio::ip::address> addrs;

  GetIfAddrs getIfAddrs;
  getIfAddrs.withIfAddrs(
    [&](const struct ifaddrs& interfaces)
    {
      const struct ifaddrs* interface;
      for (interface = &interfaces; interface; interface = interface->ifa_next)
      {
        auto addr = reinterpret_cast<const struct sockaddr_in*>(interface->ifa_addr);
        if (addr && interface->ifa_flags & IFF_UP)
        {
          if (addr->sin_family == AF_INET)
          {
            auto bytes = reinterpret_cast<const char*>(&addr->sin_addr);
            addrs.emplace_back(makeAddress<asio::ip::address_v4>(bytes));
          }
          else if (addr->sin_family == AF_INET6)
          {
            auto addr6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
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
