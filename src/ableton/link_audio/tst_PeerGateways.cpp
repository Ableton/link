/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/discovery/AsioTypes.hpp>
#include <ableton/link_audio/PeerGateways.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>
#include <memory>


namespace ableton
{
namespace link_audio
{
namespace
{

struct Gateway
{
  void updateAnnouncement(int a) { announcement = a; }

  discovery::IpAddress address;
  int announcement{0};
};

struct Factory
{
  std::shared_ptr<Gateway> operator()(const discovery::IpAddress& addr)
  {
    return std::make_shared<Gateway>(Gateway{addr});
  }
};


template <typename Gateways>
void expectGateways(Gateways& gateways, std::vector<Gateway> expectedGateways)

{
  bool bTested = false;

  gateways.withGateways(
    [&, expectedGateways](auto begin, const auto end)
    {
      bTested = true;
      REQUIRE(static_cast<size_t>(distance(begin, end)) == expectedGateways.size());
      std::size_t i = 0;
      for (; begin != end; ++begin)
      {
        CHECK(begin->first == expectedGateways[i].address);
        CHECK(begin->second->announcement == expectedGateways[i].announcement);
        ++i;
      }
    });
  CHECK(bTested);
}

using TestGateways = PeerGateways<Factory&, ableton::test::serial_io::Context>;

} // anonymous namespace

TEST_CASE("PeerGateways")
{
  const discovery::IpAddress addr1 = discovery::makeAddress("192.192.192.1");
  const discovery::IpAddress addr2 = discovery::makeAddress("192.192.192.2");

  test::serial_io::Fixture io;
  auto factory = Factory{};
  auto gateways =
    TestGateways(util::injectRef(factory), util::injectVal(io.makeIoContext()));

  SECTION("GatewayAppears")
  {
    gateways.updateGateways({addr1});
    expectGateways(gateways, {{addr1, 0}});

    gateways.updateGateways({addr1, addr2});
    expectGateways(gateways, {{addr1, 0}, {addr2, 0}});
  }

  SECTION("GatewayDisappears")
  {
    gateways.updateGateways({addr1, addr2});
    expectGateways(gateways, {{addr1, 0}, {addr2, 0}});

    gateways.updateGateways({addr1});
    expectGateways(gateways, {{addr1, 0}});
  }

  SECTION("GatewayChangesAddress")
  {
    gateways.updateGateways({addr1});
    expectGateways(gateways, {{addr1, 0}});

    gateways.updateGateways({addr2});
    expectGateways(gateways, {{addr2, 0}});
  }

  SECTION("UpdateAnnouncement")
  {
    gateways.updateGateways({addr1, addr2});
    expectGateways(gateways, {{addr1, 0}, {addr2, 0}});

    gateways.updateAnnouncement(42);
    expectGateways(gateways, {{addr1, 42}, {addr2, 42}});
  }
}

} // namespace link_audio
} // namespace ableton
