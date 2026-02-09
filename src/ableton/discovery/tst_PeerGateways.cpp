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

#include <ableton/discovery/PeerGateways.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>


namespace ableton
{
namespace discovery
{
namespace
{

struct Gateway
{
  IpAddress addr;
};

struct NodeState
{
};

struct Factory
{
  template <typename IoContext>
  Gateway operator()(NodeState, util::Injected<IoContext>, const IpAddress& addr)
  {
    return {addr};
  }

  void gatewaysChanged() { ++changedCount; }

  size_t changedCount{0};
};


template <typename Gateways>
void expectGateways(Gateways& gateways,
                    test::serial_io::Fixture& io,
                    std::vector<IpAddress> addrs)

{
  using namespace std;
  using GatewayIt = typename Gateways::GatewayMap::iterator;
  bool bTested = false;

  gateways.withGateways(
    [addrs, &bTested](GatewayIt begin, const GatewayIt end)
    {
      bTested = true;
      REQUIRE(static_cast<size_t>(distance(begin, end)) == addrs.size());
      std::size_t i = 0;
      for (; begin != end; ++begin)
      {
        CHECK(begin->first == addrs[i++]);
      }
    });
  io.flush();
  CHECK(bTested);
}

} // anonymous namespace

TEST_CASE("PeerGateways")
{
  const IpAddress addr1 = makeAddress("192.192.192.1");
  const IpAddress addr2 = makeAddress("192.192.192.2");

  test::serial_io::Fixture io;
  auto factory = Factory{};
  auto pGateways = makePeerGateways(std::chrono::seconds(2),
                                    NodeState{},
                                    util::injectRef(factory),
                                    util::injectVal(io.makeIoContext()));

  SECTION("CallGatewaysChangedOnEnable")
  {
    pGateways->enable(true);
    CHECK(factory.changedCount == 1);

    pGateways->enable(false);
    CHECK(factory.changedCount == 2);
  }

  SECTION("EmptyIfNoInterfaces")
  {
    pGateways->enable(true);
    expectGateways(*pGateways, io, {});
    CHECK(factory.changedCount == 1);
  }

  SECTION("MatchesAfterInitialScan")
  {
    io.setNetworkInterfaces({addr1, addr2});
    pGateways->enable(true);
    expectGateways(*pGateways, io, {addr1, addr2});
    CHECK(factory.changedCount == 2);
  }

  SECTION("GatewayAppears")
  {
    io.setNetworkInterfaces({addr1});
    pGateways->enable(true);
    expectGateways(*pGateways, io, {addr1});
    CHECK(factory.changedCount == 2);

    io.setNetworkInterfaces({addr1, addr2});
    io.advanceTime(std::chrono::seconds(3));
    expectGateways(*pGateways, io, {addr1, addr2});
    CHECK(factory.changedCount == 3);
  }

  SECTION("GatewayDisappears")
  {
    io.setNetworkInterfaces({addr1, addr2});
    pGateways->enable(true);
    expectGateways(*pGateways, io, {addr1, addr2});
    CHECK(factory.changedCount == 2);

    io.setNetworkInterfaces({addr1});
    io.advanceTime(std::chrono::seconds(3));
    expectGateways(*pGateways, io, {addr1});
    CHECK(factory.changedCount == 3);
  }

  SECTION("GatewayChangesAddress")
  {
    io.setNetworkInterfaces({addr1});
    pGateways->enable(true);
    expectGateways(*pGateways, io, {addr1});
    CHECK(factory.changedCount == 2);

    io.setNetworkInterfaces({addr2});
    io.advanceTime(std::chrono::seconds(3));
    expectGateways(*pGateways, io, {addr2});
    CHECK(factory.changedCount == 3);
  }
}

} // namespace discovery
} // namespace ableton
