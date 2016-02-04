// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/InterfaceScanner.hpp>
#include <ableton/link/platform/test/Platform.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/util/Log.hpp>
#include <ableton/util/test/Timer.hpp>

namespace ableton
{
namespace link
{
namespace
{

struct TestCallback
{
  template <typename AddrRange>
  void operator()(AddrRange addrs)
  {
    addrRanges.emplace_back(begin(addrs), end(addrs));
  }

  std::vector<std::vector<asio::ip::address>> addrRanges;
};

// some test addresses
const asio::ip::address addr1 =
  asio::ip::address::from_string("123.123.123.1");
const asio::ip::address addr2 =
  asio::ip::address::from_string("123.123.123.2");

} // anonymous namespace

TEST_CASE("InterfaceScanner | NoInterfacesThenOne", "[InterfaceScanner]")
{
  auto callback = TestCallback{};
  auto timer = util::test::Timer{};
  auto platform = platform::test::Platform{{
      {}, {addr1}
    }};
  {
    auto scanner = makeInterfaceScanner(
      2u, util::injectRef(callback), util::injectVal(std::move(platform)),
      util::injectRef(timer), util::injectVal(util::NullLog{}));
    scanner.enable(true);
    CHECK(1 == callback.addrRanges.size());
    timer.advance(std::chrono::seconds(3));
  }
  REQUIRE(2 == callback.addrRanges.size());
  CHECK(0 == callback.addrRanges[0].size());
  REQUIRE(1 == callback.addrRanges[1].size());
  CHECK(addr1 == callback.addrRanges[1].front());
}

TEST_CASE("InterfaceScanner | InterfaceGoesAway", "[InterfaceScanner]")
{
  auto callback = TestCallback{};
  auto timer = util::test::Timer{};
  auto platform = platform::test::Platform{ {
      {addr1}, {}
    }};
  {
    auto scanner = makeInterfaceScanner(
      2u, util::injectRef(callback), util::injectVal(std::move(platform)),
      util::injectRef(timer), util::injectVal(util::NullLog{}));
    scanner.enable(true);
    timer.advance(std::chrono::seconds(3));
  }
  REQUIRE(2 == callback.addrRanges.size());
  REQUIRE(1 == callback.addrRanges[0].size());
  CHECK(addr1 == callback.addrRanges[0].front());
  CHECK(0 == callback.addrRanges[1].size());
}

} // namespace link
} // namespace ableton
