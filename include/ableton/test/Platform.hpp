// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{
namespace platform
{
namespace test
{

struct Platform
{
  using AddrSets = std::vector<std::vector<asio::ip::address>>;

  Platform(AddrSets addrSets)
    : mAddrSets(std::move(addrSets))
    , mNextAddrSet(0)
  {
  }

  std::vector<asio::ip::address> scanIpIfAddrs()
  {
    if (mNextAddrSet == mAddrSets.size())
    {
      return {};
    }
    else
    {
      return mAddrSets[mNextAddrSet++];
    }
  }

private:
  AddrSets mAddrSets;
  std::size_t mNextAddrSet;
};

} // namespace test
} // namespace platform
} // namespace link
} // namespace ableton
