// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Controller.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/discovery/util/test/Timer.hpp>

namespace ableton
{
namespace link
{

using namespace std::chrono;

struct MockClock
{
  using Ticks = std::int64_t;

  friend Ticks ticks(const MockClock&)
  {
    return 1;
  }

  friend microseconds micros(const MockClock&)
  {
    return microseconds{1};
  }
};

struct MockPlatform
{
  using Addresses = std::vector<asio::ip::address>;
  Addresses scanIpIfAddrs()
  {
    return Addresses{};
  }
};

struct MockIoService
{
  using Timer = util::test::Timer;

  MockIoService() = default;
  ~MockIoService() = default;

  Timer makeTimer()
  {
    return Timer{};
  }

  template <typename Handler>
  void post(Handler handler)
  {
    handler();
  }
};

using Log = ableton::util::NullLog;
using MockController = Controller<PeerCountCallback, TempoCallback, MockClock,
  MockPlatform, Log, MockIoService>;

TEST_CASE("Controller | ConstructOptimistically", "[Controller]")
{
  MockController controller(
    Tempo{100.0},
    Beats{5.0},
    [](std::size_t) {},
    [](Tempo) {},
    MockClock{},
    MockPlatform{},
    Log{},
    MockIoService{});

  CHECK(!controller.isEnabled());
  CHECK(0 == controller.numPeers());
  CHECK(Beats{5.0} == controller.quantum());
  CHECK(Tempo{100.0} == tempo(controller));
}

TEST_CASE("Controller | ConstructWithInvalidTempo", "[Controller]")
{
  MockController controllerLowTempo(
    Tempo{1.0},
    Beats{5.0},
    [](std::size_t) {},
    [](Tempo) {},
    MockClock{},
    MockPlatform{},
    Log{},
    MockIoService{});
  CHECK(Tempo{20.0} == tempo(controllerLowTempo));

  MockController controllerHighTempo(
    Tempo{100000.0},
    Beats{5.0},
    [](std::size_t) {},
    [](Tempo) {},
    MockClock{},
    MockPlatform{},
    Log{},
    MockIoService{});
  CHECK(Tempo{999.0} == tempo(controllerHighTempo));
}

TEST_CASE("Controller | SetTempo", "[Controller]")
{
  Tempo broadcastedTempo;
  MockController controller(
    Tempo{100.0},
    Beats{5.0},
    [](std::size_t) {},
    [&](Tempo tempo) { broadcastedTempo = tempo; },
    MockClock{},
    MockPlatform{},
    Log{},
    MockIoService{});

  const Tempo newTempo{123.0};
  controller.setTempo(newTempo, std::chrono::milliseconds{0});

  CHECK(newTempo == tempo(controller));
  // The TempoCallback should also have been reached
  CHECK(newTempo == broadcastedTempo);
}

} // namespace link
} // namespace ableton
