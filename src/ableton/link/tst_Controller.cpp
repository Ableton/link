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

#include <ableton/link/Controller.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/util/test/Timer.hpp>

namespace ableton
{
namespace link
{

using namespace std::chrono;

namespace
{

struct MockClock
{
  void advance()
  {
    time += microseconds{1};
  }

  microseconds micros() const
  {
    return time;
  }

  microseconds time{1};
};

struct MockIoContext
{
  template <std::size_t BufferSize>
  struct Socket
  {
    std::size_t send(
      const uint8_t* const, const size_t numBytes, const asio::ip::udp::endpoint&)
    {
      return numBytes;
    }

    template <typename Handler>
    void receive(Handler)
    {
    }

    asio::ip::udp::endpoint endpoint() const
    {
      return {};
    }
  };

  template <std::size_t BufferSize>
  Socket<BufferSize> openUnicastSocket(const asio::ip::address_v4&)
  {
    return {};
  }

  template <std::size_t BufferSize>
  Socket<BufferSize> openMulticastSocket(const asio::ip::address_v4&)
  {
    return {};
  }

  std::vector<asio::ip::address> scanNetworkInterfaces()
  {
    return {};
  }

  using Timer = util::test::Timer;

  Timer makeTimer()
  {
    return {};
  }

  template <typename Callback, typename Duration>
  struct LockFreeCallbackDispatcher
  {
    LockFreeCallbackDispatcher(Callback callback, Duration)
      : mCallback(std::move(callback))
    {
    }

    void invoke()
    {
      mCallback();
    }

    Callback mCallback;
  };

  using Log = util::NullLog;

  Log log() const
  {
    return {};
  }

  template <typename Handler>
  void async(Handler handler) const
  {
    handler();
  }

  MockIoContext clone() const
  {
    return {};
  }

  template <typename ExceptionHandler>
  MockIoContext clone(ExceptionHandler) const
  {
    return {};
  }
};

using MockController = Controller<PeerCountCallback,
  TempoCallback,
  StartStopStateCallback,
  MockClock,
  MockIoContext>;

const auto kAnyBeatTime = Beats{5.};

const auto kAnyTime = std::chrono::microseconds{6};

struct TempoClientCallback
{
  void operator()(const Tempo bpm)
  {
    tempos.push_back(bpm);
  }

  std::vector<Tempo> tempos;
};

struct StartStopStateClientCallback
{
  void operator()(const bool isPlaying)
  {
    startStopStates.push_back(isPlaying);
  }

  std::vector<bool> startStopStates;
};

void expectClientStateEquals(
  const IncomingClientState& expectedState, const ClientState& state)
{
  CHECK(std::tie(*expectedState.timeline, *expectedState.startStopState)
        == std::tie(state.timeline, state.startStopState));
}

} // anon namespace

TEST_CASE("Controller | ConstructOptimistically", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {},
    MockClock{}, util::injectVal(MockIoContext{}));

  CHECK(!controller.isEnabled());
  CHECK(!controller.isStartStopSyncEnabled());
  CHECK(0 == controller.numPeers());
  const auto tl = controller.clientState().timeline;
  CHECK(Tempo{100.0} == tl.tempo);
}

TEST_CASE("Controller | ConstructWithInvalidTempo", "[Controller]")
{
  MockController controllerLowTempo(Tempo{1.0}, [](std::size_t) {}, [](Tempo) {},
    [](bool) {}, MockClock{}, util::injectVal(MockIoContext{}));
  const auto tlLow = controllerLowTempo.clientState().timeline;
  CHECK(Tempo{20.0} == tlLow.tempo);

  MockController controllerHighTempo(Tempo{100000.0}, [](std::size_t) {}, [](Tempo) {},
    [](bool) {}, MockClock{}, util::injectVal(MockIoContext{}));
  const auto tlHigh = controllerHighTempo.clientState().timeline;
  CHECK(Tempo{999.0} == tlHigh.tempo);
}

TEST_CASE("Controller | EnableDisable", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {},
    MockClock{}, util::injectVal(MockIoContext{}));

  controller.enable(true);
  CHECK(controller.isEnabled());
  controller.enable(false);
  CHECK(!controller.isEnabled());
}

TEST_CASE("Controller | EnableDisableStartStopSync", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {},
    MockClock{}, util::injectVal(MockIoContext{}));

  controller.enableStartStopSync(true);
  CHECK(controller.isStartStopSyncEnabled());
  controller.enableStartStopSync(false);
  CHECK(!controller.isStartStopSyncEnabled());
}

TEST_CASE("Controller | SetAndGetClientStateThreadSafe", "[Controller]")
{
  using namespace std::chrono;

  auto clock = MockClock{};
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {},
    clock, util::injectVal(MockIoContext{}));

  clock.advance();
  const auto initialTimeline =
    Optional<Timeline>{Timeline{Tempo{60.}, Beats{0.}, kAnyTime}};
  const auto initialStartStopState =
    Optional<StartStopState>{StartStopState{true, kAnyBeatTime, clock.micros()}};
  const auto initialClientState =
    IncomingClientState{initialTimeline, initialStartStopState, clock.micros()};

  controller.setClientState(initialClientState);

  SECTION("Client state is correct after initial set")
  {
    expectClientStateEquals(initialClientState, controller.clientState());
  }

  SECTION("Set outdated start stop state (timestamp unchanged)")
  {
    // Set client state with a StartStopState having the same timestamp as the current
    // StartStopState - don't advance clock
    const auto outdatedStartStopState =
      Optional<StartStopState>{StartStopState{false, kAnyBeatTime, clock.micros()}};
    controller.setClientState(
      IncomingClientState{Optional<Timeline>{}, outdatedStartStopState, clock.micros()});
    expectClientStateEquals(initialClientState, controller.clientState());
  }

  clock.advance();

  SECTION("Set outdated start stop state (timestamp in past)")
  {
    const auto outdatedStartStopState =
      Optional<StartStopState>{StartStopState{false, kAnyBeatTime, microseconds{0}}};

    controller.setClientState(
      IncomingClientState{Optional<Timeline>{}, outdatedStartStopState, clock.micros()});
    expectClientStateEquals(initialClientState, controller.clientState());
  }

  SECTION("Set empty client state")
  {
    controller.setClientState(IncomingClientState{
      Optional<Timeline>{}, Optional<StartStopState>{}, clock.micros()});
    expectClientStateEquals(initialClientState, controller.clientState());
  }

  SECTION("Set client state with new Timeline and StartStopState")
  {
    const auto expectedTimeline =
      Optional<Timeline>{Timeline{Tempo{80.}, Beats{1.}, kAnyTime}};
    const auto expectedStartStopState =
      Optional<StartStopState>{StartStopState{false, kAnyBeatTime, clock.micros()}};
    const auto expectedClientState =
      IncomingClientState{expectedTimeline, expectedStartStopState, clock.micros()};
    controller.setClientState(expectedClientState);
    expectClientStateEquals(expectedClientState, controller.clientState());
  }
}

TEST_CASE("Controller | SetAndGetClientStateRealtimeSafe", "[Controller]")
{
  using namespace std::chrono;

  auto clock = MockClock{};
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {},
    clock, util::injectVal(MockIoContext{}));

  clock.advance();
  auto expectedTimeline = Optional<Timeline>{Timeline{Tempo{110.}, Beats{0.}, kAnyTime}};
  auto expectedStartStopState =
    Optional<StartStopState>{StartStopState{true, kAnyBeatTime, clock.micros()}};
  auto expectedClientState =
    IncomingClientState{expectedTimeline, expectedStartStopState, clock.micros()};
  controller.setClientStateRtSafe(expectedClientState);
  auto clientState = controller.clientState();
  expectClientStateEquals(expectedClientState, clientState);

  // Set client state with a StartStopState having the same timestamp as the current
  // StartStopState - don't advance clock
  auto outdatedStartStopState =
    Optional<StartStopState>{StartStopState{false, kAnyBeatTime, clock.micros()}};
  controller.setClientStateRtSafe(
    IncomingClientState{Optional<Timeline>{}, outdatedStartStopState, clock.micros()});
  clientState = controller.clientStateRtSafe();
  expectClientStateEquals(expectedClientState, clientState);

  // Set client state with an outdated StartStopState
  clock.advance();
  outdatedStartStopState =
    Optional<StartStopState>{StartStopState{false, kAnyBeatTime, microseconds{0}}};
  controller.setClientStateRtSafe(
    IncomingClientState{Optional<Timeline>{}, outdatedStartStopState, clock.micros()});
  clientState = controller.clientStateRtSafe();
  expectClientStateEquals(expectedClientState, clientState);

  // Set client state without Timeline and StartStopState
  clock.advance();
  controller.setClientStateRtSafe(IncomingClientState{
    Optional<Timeline>{}, Optional<StartStopState>{}, clock.micros()});
  clientState = controller.clientStateRtSafe();
  expectClientStateEquals(expectedClientState, clientState);

  // Set client state with a new StartStopState
  clock.advance();
  expectedTimeline = Optional<Timeline>{Timeline{Tempo{90.}, Beats{1.4}, kAnyTime}};
  expectedStartStopState =
    Optional<StartStopState>{StartStopState{false, kAnyBeatTime, clock.micros()}};
  expectedClientState =
    IncomingClientState{expectedTimeline, expectedStartStopState, clock.micros()};
  controller.setClientStateRtSafe(expectedClientState);
  clientState = controller.clientStateRtSafe();
  expectClientStateEquals(expectedClientState, clientState);
}

TEST_CASE("Controller | CallbacksCalledBySettingClientStateThreadSafe", "[Controller]")
{
  using namespace std::chrono;

  auto clock = MockClock{};
  auto tempoCallback = TempoClientCallback{};
  auto startStopStateCallback = StartStopStateClientCallback{};
  MockController controller(Tempo{100.0}, [](std::size_t) {}, std::ref(tempoCallback),
    std::ref(startStopStateCallback), clock, util::injectVal(MockIoContext{}));

  clock.advance();
  const auto expectedTempo = Tempo{50.};
  auto timeline = Optional<Timeline>{Timeline{expectedTempo, Beats{0.}, kAnyTime}};
  const auto expectedIsPlaying = true;
  auto startStopState = Optional<StartStopState>{
    StartStopState{expectedIsPlaying, kAnyBeatTime, clock.micros()}};
  controller.setClientState({timeline, startStopState, clock.micros()});
  CHECK(std::vector<Tempo>{expectedTempo} == tempoCallback.tempos);
  CHECK(std::vector<bool>{expectedIsPlaying} == startStopStateCallback.startStopStates);

  // Callbacks mustn't be called if Tempo and isPlaying don't change
  clock.advance();
  tempoCallback.tempos = {};
  startStopStateCallback.startStopStates = {};
  timeline = Optional<Timeline>{Timeline{expectedTempo, Beats{1.}, kAnyTime}};
  startStopState = Optional<StartStopState>{
    StartStopState{expectedIsPlaying, kAnyBeatTime, clock.micros()}};
  controller.setClientState({timeline, startStopState, clock.micros()});
  CHECK(tempoCallback.tempos.empty());
  CHECK(startStopStateCallback.startStopStates.empty());
}

TEST_CASE("Controller | CallbacksCalledBySettingClientStateRealtimeSafe", "[Controller]")
{
  using namespace std::chrono;

  auto clock = MockClock{};
  auto tempoCallback = TempoClientCallback{};
  auto startStopStateCallback = StartStopStateClientCallback{};
  MockController controller(Tempo{100.0}, [](std::size_t) {}, std::ref(tempoCallback),
    std::ref(startStopStateCallback), clock, util::injectVal(MockIoContext{}));

  clock.advance();
  const auto expectedTempo = Tempo{130.};
  auto timeline = Optional<Timeline>{Timeline{expectedTempo, Beats{0.}, kAnyTime}};
  const auto expectedIsPlaying = true;
  auto startStopState = Optional<StartStopState>{
    StartStopState{expectedIsPlaying, kAnyBeatTime, clock.micros()}};
  controller.setClientStateRtSafe({timeline, startStopState, clock.micros()});
  CHECK(std::vector<Tempo>{expectedTempo} == tempoCallback.tempos);
  CHECK(std::vector<bool>{expectedIsPlaying} == startStopStateCallback.startStopStates);

  // Callbacks mustn't be called if Tempo and isPlaying don't change
  clock.advance();
  tempoCallback.tempos = {};
  startStopStateCallback.startStopStates = {};
  timeline = Optional<Timeline>{Timeline{expectedTempo, Beats{1.}, kAnyTime}};
  startStopState = Optional<StartStopState>{
    StartStopState{expectedIsPlaying, kAnyBeatTime, clock.micros()}};
  controller.setClientStateRtSafe({timeline, startStopState, clock.micros()});
  CHECK(tempoCallback.tempos.empty());
  CHECK(startStopStateCallback.startStopStates.empty());
}

} // namespace link
} // namespace ableton
