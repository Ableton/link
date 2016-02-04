// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/PeerGateways.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/Platform.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <ableton/discovery/util/test/IoService.hpp>

namespace ableton
{
namespace link
{
namespace discovery
{
namespace
{

struct Gateway
{
  asio::ip::address addr;
};

struct Factory
{
  Gateway operator()(util::test::IoService&, const asio::ip::address& addr)
  {
    return {addr};
  }
};

const asio::ip::address addr1 =
  asio::ip::address::from_string("192.192.192.1");

const asio::ip::address addr2 =
  asio::ip::address::from_string("192.192.192.2");

template <typename Gateways, typename T, typename Rep>
void expectGatewaysAsync(
  Gateways& gateways,
  std::vector<asio::ip::address> addrs,
  util::test::IoService& io,
  std::chrono::duration<T, Rep> after)
{
  using namespace std;
  using GatewayIt = typename Gateways::GatewayMap::iterator;
  bool bTested = false;

  // Advance the io service for side effects first
  io.advance(after);
  // Then test in the mock io thread by advancing 0 seconds
  gateways.withGatewaysAsync([addrs, &bTested] (GatewayIt begin, const GatewayIt end) {
      bTested = true;
      REQUIRE(static_cast<size_t>(distance(begin, end)) == addrs.size());
      std::size_t i = 0;
      for (; begin != end; ++begin)
      {
        CHECK(begin->first == addrs[i++]);
      }
    });
  io.advance(std::chrono::seconds(0));
  CHECK(bTested);
}

} // anonymous namespace

TEST_CASE("PeerGateways | EmptyIfNoInterfaces", "[PeerGateways]")
{
  util::test::IoService io;
  auto pGateways = makePeerGateways(
    2, Factory{}, util::injectRef(io), util::injectVal(platform::test::Platform{{}}),
    util::injectVal(util::NullLog{}));
  pGateways->enable(true);
  expectGatewaysAsync(*pGateways, {}, io, std::chrono::seconds(0));
}

TEST_CASE("PeerGateways | MatchesAfterInitialScan", "[PeerGateways]")
{
  util::test::IoService io;
  auto platform = platform::test::Platform{ platform::test::Platform::AddrSets{ { addr1, addr2 } } };
  auto pGateways = makePeerGateways(
    2, Factory{}, util::injectRef(io), util::injectVal(platform),
    util::injectVal(util::NullLog{}));
  pGateways->enable(true);
  expectGatewaysAsync(*pGateways, {addr1, addr2}, io, std::chrono::seconds(0));
}

TEST_CASE("PeerGateways | GatewayAppears", "[PeerGateways]")
{
  util::test::IoService io;
  auto platform = platform::test::Platform{ { { addr1 }, { addr1, addr2 } } };
  auto pGateways = makePeerGateways(
    2, Factory{}, util::injectRef(io), util::injectVal(platform),
    util::injectVal(util::NullLog{}));
  pGateways->enable(true);
  expectGatewaysAsync(*pGateways, {addr1}, io, std::chrono::seconds(0));
  expectGatewaysAsync(*pGateways, {addr1, addr2}, io, std::chrono::seconds(3));
}

TEST_CASE("PeerGateways | GatewayDisappears", "[PeerGateways]")
{
  util::test::IoService io;
  auto platform = platform::test::Platform{ { { addr1, addr2 }, { addr1 } } };
  auto pGateways = makePeerGateways(
    2, Factory{}, util::injectRef(io), util::injectVal(platform),
    util::injectVal(util::NullLog{}));
  pGateways->enable(true);
  expectGatewaysAsync(*pGateways, {addr1, addr2}, io, std::chrono::seconds(0));
  expectGatewaysAsync(*pGateways, {addr1}, io, std::chrono::seconds(3));
}

TEST_CASE("PeerGateways | PeerGateways", "[PeerGateways]")
{
  util::test::IoService io;
  auto platform = platform::test::Platform{ { { addr1 }, { addr2 } } };
  auto pGateways = makePeerGateways(
    2, Factory{}, util::injectRef(io), util::injectVal(platform),
    util::injectVal(util::NullLog{}));
  pGateways->enable(true);
  expectGatewaysAsync(*pGateways, {addr1}, io, std::chrono::seconds(0));
  expectGatewaysAsync(*pGateways, {addr2}, io, std::chrono::seconds(3));
}

} // namespace discovery
} // namespace link
} // namespace ableton
