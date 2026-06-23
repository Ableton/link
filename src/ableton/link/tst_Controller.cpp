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

#include <ableton/link/SessionController.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/util/Log.hpp>
#include <ableton/util/test/Timer.hpp>

namespace ableton
{
namespace link
{

using namespace std::chrono;

static bool operator==(const IncomingClientState& lhs, const ClientState& rhs)
{
  return static_cast<bool>(lhs.timeline) && static_cast<bool>(lhs.startStopState)
         && std::tie(*lhs.timeline, *lhs.startStopState)
              == std::tie(rhs.timeline, rhs.startStopState);
}

namespace
{

struct MockClock
{
  template <typename T, typename Rep>
  void advance(std::chrono::duration<T, Rep> duration)
  {
    now() += duration;
  }

  microseconds micros() const { return now(); }

private:
  static microseconds& now()
  {
    static microseconds now{1};
    return now;
  }
};

struct MockIoContext
{
  MockIoContext() {}

  template <typename ExceptionHandler>
  MockIoContext(ExceptionHandler)
  {
  }

  template <std::size_t BufferSize>
  struct Socket
  {
    std::size_t send(const uint8_t* const,
                     const size_t numBytes,
                     const discovery::UdpEndpoint&)
    {
      return numBytes;
    }

    template <typename Handler>
    void receive(Handler)
    {
    }

    discovery::UdpEndpoint endpoint() const { return {}; }
  };

  void stop() {}

  template <std::size_t BufferSize>
  Socket<BufferSize> openUnicastSocket(const discovery::IpAddress&)
  {
    return {};
  }

  template <std::size_t BufferSize>
  Socket<BufferSize> openMulticastSocket(const discovery::IpAddress&)
  {
    return {};
  }

  std::vector<discovery::IpAddress> scanNetworkInterfaces() { return {}; }

  using Timer = util::test::Timer;

  Timer makeTimer() { return {}; }

  template <typename Callback, typename Duration>
  struct LockFreeCallbackDispatcher
  {
    LockFreeCallbackDispatcher(Callback callback, Duration)
      : mCallback(std::move(callback))
    {
    }

    void start() {}

    void stop() {}

    void invoke() { mCallback(); }

    Callback mCallback;
  };

  using Log = util::NullLog;

  Log log() const { return {}; }

  template <typename Handler>
  void async(Handler handler) const
  {
    handler();
  }
};

using MockController = SessionController<PeerCountCallback,
                                         TempoCallback,
                                         StartStopStateCallback,
                                         MockClock,
                                         platforms::stl::Random,
                                         MockIoContext>;

const auto kAnyTime = std::chrono::microseconds{6};

struct TempoClientCallback
{
  void operator()(const Tempo bpm) { tempos.push_back(bpm); }

  std::vector<Tempo> tempos;
};

struct StartStopStateClientCallback
{
  void operator()(const bool isPlaying) { startStopStates.push_back(isPlaying); }

  std::vector<bool> startStopStates;
};

template <typename SetClientStateFunctionT, typename GetClientStateFunctionT>
void testSetAndGetClientState(SetClientStateFunctionT setClientState,
                              GetClientStateFunctionT getClientState)
{
  using namespace std::chrono;

  auto clock = MockClock{};
  MockController controller(
    Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, clock);

  clock.advance(microseconds{1});
  const auto initialTimeline =
    std::optional<Timeline>{Timeline{Tempo{60.}, Beats{0.}, kAnyTime}};
  const auto initialStartStopState = std::optional<ClientStartStopState>{
    ClientStartStopState{false, kAnyTime, clock.micros()}};
  const auto initialClientState =
    IncomingClientState{initialTimeline, initialStartStopState, {}, clock.micros()};

  setClientState(controller, initialClientState);

  SECTION("Client state is correct after initial set")
  {
    CHECK(initialClientState == getClientState(controller));
  }

  SECTION("Set outdated start stop state (timestamp unchanged)")
  {
    // Set client state with a StartStopState having the same timestamp as the current
    // StartStopState - don't advance clock
    const auto outdatedStartStopState = std::optional<ClientStartStopState>{
      ClientStartStopState{false, kAnyTime, clock.micros()}};
    setClientState(
      controller,
      IncomingClientState{
        std::optional<Timeline>{}, outdatedStartStopState, {}, clock.micros()});
    CHECK(initialClientState == getClientState(controller));
  }

  clock.advance(microseconds{1});

  SECTION("Set outdated start stop state (timestamp in past)")
  {
    const auto outdatedStartStopState = std::optional<ClientStartStopState>{
      ClientStartStopState{false, kAnyTime, microseconds{0}}};
    setClientState(
      controller,
      IncomingClientState{
        std::optional<Timeline>{}, outdatedStartStopState, {}, clock.micros()});
    CHECK(initialClientState == getClientState(controller));
  }

  SECTION("Set empty client state")
  {
    setClientState(controller,
                   IncomingClientState{std::optional<Timeline>{},
                                       std::optional<ClientStartStopState>{},
                                       {},
                                       clock.micros()});
    CHECK(initialClientState == getClientState(controller));
  }

  SECTION("Set client state with new Timeline and StartStopState")
  {
    const auto expectedTimeline =
      std::optional<Timeline>{Timeline{Tempo{80.}, Beats{1.}, kAnyTime}};
    const auto expectedStartStopState = std::optional<ClientStartStopState>{
      ClientStartStopState{false, kAnyTime, clock.micros()}};
    const auto expectedClientState =
      IncomingClientState{expectedTimeline, expectedStartStopState, {}, clock.micros()};
    setClientState(controller, expectedClientState);
    CHECK(expectedClientState == getClientState(controller));
  }
}


template <typename SetClientStateFunctionT>
void testCallbackInvocation(SetClientStateFunctionT setClientState)
{
  using namespace std::chrono;

  auto clock = MockClock{};
  auto tempoCallback = TempoClientCallback{};
  auto startStopStateCallback = StartStopStateClientCallback{};
  MockController controller(
    Tempo{100.0},
    [](std::size_t) {},
    std::ref(tempoCallback),
    std::ref(startStopStateCallback),
    clock);

  clock.advance(microseconds{1});

  const auto initialTempo = Tempo{50.};
  const auto initialIsPlaying = true;

  const auto initialTimeline =
    std::optional<Timeline>{Timeline{initialTempo, Beats{0.}, kAnyTime}};
  const auto initialStartStopState = std::optional<ClientStartStopState>{
    ClientStartStopState{initialIsPlaying, kAnyTime, clock.micros()}};
  setClientState(
    controller, {initialTimeline, initialStartStopState, {}, clock.micros()});

  SECTION("Callbacks are called when setting new client state")
  {
    CHECK(std::vector<Tempo>{initialTempo} == tempoCallback.tempos);
    CHECK(std::vector<bool>{initialIsPlaying} == startStopStateCallback.startStopStates);

    clock.advance(microseconds{1});
    tempoCallback.tempos = {};
    startStopStateCallback.startStopStates = {};

    SECTION("Callbacks mustn't be called if Tempo and isPlaying don't change")
    {
      const auto timeline =
        std::optional<Timeline>{Timeline{initialTempo, Beats{1.}, kAnyTime}};
      const auto startStopState = std::optional<ClientStartStopState>{
        ClientStartStopState{initialIsPlaying, kAnyTime, clock.micros()}};
      setClientState(controller, {timeline, startStopState, {}, clock.micros()});
      CHECK(tempoCallback.tempos.empty());
      CHECK(startStopStateCallback.startStopStates.empty());
    }
  }
}

} // namespace

TEST_CASE("Controller")
{
  SECTION("ConstructOptimistically")
  {
    MockController controller(
      Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});

    CHECK(!controller.isEnabled());
    CHECK(!controller.isStartStopSyncEnabled());
    CHECK(0 == controller.numPeers());
    const auto tl = controller.clientState().timeline;
    CHECK(Tempo{100.0} == tl.tempo);
  }

  SECTION("ConstructWithInvalidTempo")
  {
    MockController controllerLowTempo(
      Tempo{1.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});
    const auto tlLow = controllerLowTempo.clientState().timeline;
    CHECK(Tempo{20.0} == tlLow.tempo);

    MockController controllerHighTempo(
      Tempo{100000.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});
    const auto tlHigh = controllerHighTempo.clientState().timeline;
    CHECK(Tempo{999.0} == tlHigh.tempo);
  }

  SECTION("EnableDisable")
  {
    MockController controller(
      Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});

    controller.enable(true);
    CHECK(controller.isEnabled());
    controller.enable(false);
    CHECK(!controller.isEnabled());
  }

  SECTION("EnableDisableStartStopSync")
  {
    MockController controller(
      Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});

    controller.enableStartStopSync(true);
    CHECK(controller.isStartStopSyncEnabled());
    controller.enableStartStopSync(false);
    CHECK(!controller.isStartStopSyncEnabled());
  }

  SECTION("EnableArrangementSyncEnablesStartStopSync")
  {
    MockController controller(
      Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, MockClock{});

    controller.enableArrangementSync(true);
    CHECK(controller.isStartStopSyncEnabled());
    CHECK(controller.isArrangementSyncEnabled());

    controller.enableStartStopSync(false);
    CHECK(!controller.isStartStopSyncEnabled());
    CHECK(!controller.isArrangementSyncEnabled());
  }

  SECTION("SetAndGetClientStateThreadSafe")
  {
    testSetAndGetClientState(
      [](MockController& controller, IncomingClientState clientState)
      { controller.setClientState(clientState); },
      [](MockController& controller) { return controller.clientState(); });
  }

  SECTION("SetAndGetClientStateRealtimeSafe")
  {
    testSetAndGetClientState(
      [](MockController& controller, IncomingClientState clientState)
      { controller.setClientStateRtSafe(clientState); },
      [](MockController& controller) { return controller.clientStateRtSafe(); });
  }

  SECTION("SetClientStateRealtimeSafeAndGetItThreadSafe")
  {
    testSetAndGetClientState(
      [](MockController& controller, IncomingClientState clientState)
      { controller.setClientStateRtSafe(clientState); },
      [](MockController& controller) { return controller.clientState(); });
  }

  SECTION("SetClientStateThreadSafeAndGetItRealtimeSafe")
  {
    testSetAndGetClientState(
      [](MockController& controller, IncomingClientState clientState)
      { controller.setClientState(clientState); },
      [](MockController& controller)
      {
        MockClock{}.advance(seconds{2});
        return controller.clientStateRtSafe();
      });
  }

  SECTION("ArrangementTimelinePersistsAcrossRecapture")
  {
    using namespace std::chrono;
    auto clock = MockClock{};
    MockController controller(
      Tempo{120.0}, [](std::size_t) {}, [](Tempo) {}, [](bool) {}, clock);
    clock.advance(microseconds{1});

    const auto origin = Beats{42.5};
    const auto arrangement =
      std::optional<ClientArrangementTimeline>{{origin, clock.micros(), clock.micros()}};
    const auto incoming = IncomingClientState{std::optional<Timeline>{},
                                              std::optional<ClientStartStopState>{},
                                              arrangement,
                                              clock.micros()};

    SECTION("Rt-safe")
    {
      controller.setClientStateRtSafe(incoming);
      REQUIRE(controller.clientStateRtSafe().arrangementTimeline);
      REQUIRE(controller.clientState().arrangementTimeline);
      CHECK(origin == controller.clientStateRtSafe().arrangementTimeline->beatOrigin);
      CHECK(origin == controller.clientState().arrangementTimeline->beatOrigin);
    }

    SECTION("App")
    {
      controller.setClientState(incoming);
      REQUIRE(controller.clientState().arrangementTimeline);
      CHECK(origin == controller.clientState().arrangementTimeline->beatOrigin);
      clock.advance(seconds{2}); // Longer than grace period
      REQUIRE(controller.clientStateRtSafe().arrangementTimeline);
      CHECK(origin == controller.clientStateRtSafe().arrangementTimeline->beatOrigin);
    }
  }

  SECTION("CallbacksCalledBySettingClientStateThreadSafe")
  {
    testCallbackInvocation([](MockController& controller, IncomingClientState clientState)
                           { controller.setClientState(clientState); });
  }

  SECTION("CallbacksCalledBySettingClientStateRealtimeSafe")
  {
    testCallbackInvocation([](MockController& controller, IncomingClientState clientState)
                           { controller.setClientStateRtSafe(clientState); });
  }

  SECTION("GetClientStateRtSafeGracePeriod")
  {
    using namespace std::chrono;

    auto clock = MockClock{};
    auto tempoCallback = TempoClientCallback{};
    auto startStopStateCallback = StartStopStateClientCallback{};
    MockController controller(
      Tempo{100.0},
      [](std::size_t) {},
      std::ref(tempoCallback),
      std::ref(startStopStateCallback),
      clock);
    controller.enable(true);

    clock.advance(microseconds{1});
    const auto initialTimeline =
      std::optional<Timeline>{Timeline{Tempo{50.}, Beats{0.}, clock.micros()}};
    const auto initialStartStopState = std::optional<ClientStartStopState>{
      ClientStartStopState{true, kAnyTime, clock.micros()}};
    const auto initialState =
      IncomingClientState{initialTimeline, initialStartStopState, {}, clock.micros()};

    controller.setClientStateRtSafe(
      {initialTimeline, initialStartStopState, {}, clock.micros()});
    REQUIRE(initialState == controller.clientState());
    REQUIRE(initialState == controller.clientStateRtSafe());

    clock.advance(microseconds{1});
    const auto newTimeline =
      std::optional<Timeline>{Timeline{Tempo{70.}, Beats{1.}, clock.micros()}};
    const auto newStartStopState = std::optional<ClientStartStopState>{
      ClientStartStopState{false, kAnyTime, clock.micros()}};
    const auto newState =
      IncomingClientState{newTimeline, newStartStopState, {}, clock.micros()};

    controller.setClientState({newTimeline, newStartStopState, {}, clock.micros()});
    clock.advance(milliseconds{500});
    CHECK(newState == controller.clientState());
    CHECK(initialState == controller.clientStateRtSafe());

    clock.advance(milliseconds{500});
    CHECK(newState == controller.clientState());
    CHECK(newState == controller.clientStateRtSafe());
  }
}

TEST_CASE("ArrangementTimelineSessionMapping")
{
  using namespace std::chrono;

  const auto sessionTimeline = Timeline{Tempo{120.}, Beats{0.}, microseconds{0}};

  SECTION("ClientToSessionRoundTripIsIdentity")
  {
    const auto clientTimeline = Timeline{Tempo{120.}, Beats{7.}, microseconds{3000}};
    const auto xForm = GhostXForm{1.0, microseconds{250}};
    const auto client =
      ClientArrangementTimeline{Beats{42.5}, microseconds{1500}, microseconds{2000}};

    const auto session = detail::mapArrangementTimelineFromClientToSession(
      client, sessionTimeline, clientTimeline, xForm);
    const auto back = detail::mapArrangementTimelineFromSessionToClient(
      session, sessionTimeline, clientTimeline, xForm);

    CHECK(client.beatOrigin.floating()
          == Approx(back.beatOrigin.floating()).margin(1e-6));
    CHECK(client.timestamp == back.timestamp);
  }

  SECTION("AbsoluteArrangementBeatIsSharedAcrossPeers")
  {
    const auto clientA = Timeline{Tempo{120.}, Beats{10.}, microseconds{0}};
    const auto clientB = Timeline{Tempo{120.}, Beats{-3.}, microseconds{0}};
    const auto xFormA = GhostXForm{1.0, microseconds{100}};
    const auto xFormB = GhostXForm{1.0, microseconds{-50}};

    const auto hostA = microseconds{500000};
    const auto arrangementBeat = 8.0;

    const auto originA = clientA.toBeats(hostA) - Beats{arrangementBeat};
    const auto session = detail::mapArrangementTimelineFromClientToSession(
      {originA, hostA, hostA}, sessionTimeline, clientA, xFormA);

    const auto clientArrangementB = detail::mapArrangementTimelineFromSessionToClient(
      session, sessionTimeline, clientB, xFormB);

    const auto ghost = xFormA.hostToGhost(hostA);
    const auto hostB = xFormB.ghostToHost(ghost);
    const auto beatOnB =
      (clientB.toBeats(hostB) - clientArrangementB.beatOrigin).floating();

    CHECK(beatOnB == Approx(arrangementBeat).margin(1e-6));
  }

  SECTION("AdoptingSharedArrangementGivesSameBeatAcrossPeers")
  {
    const auto clientA = Timeline{Tempo{120.}, Beats{10.3}, microseconds{0}};
    const auto clientB = Timeline{Tempo{120.}, Beats{2.7}, microseconds{0}};
    const auto xForm = GhostXForm{1.0, microseconds{0}};
    const auto t = microseconds{1234567};

    CHECK(clientA.toBeats(t).floating()
          != Approx(clientB.toBeats(t).floating()).margin(1e-6));

    const auto sessionZero =
      ArrangementTimeline{Beats{0.}, Beats{0.}, xForm.hostToGhost(t)};
    const auto a = detail::mapArrangementTimelineFromSessionToClient(
      sessionZero, sessionTimeline, clientA, xForm);
    const auto b = detail::mapArrangementTimelineFromSessionToClient(
      sessionZero, sessionTimeline, clientB, xForm);
    const auto beatA = (clientA.toBeats(t) - a.beatOrigin).floating();
    const auto beatB = (clientB.toBeats(t) - b.beatOrigin).floating();
    CHECK(beatA == Approx(beatB).margin(1e-6));
  }

  SECTION("SharedArrangementStaysSyncedAcrossTempoChange")
  {
    const auto xForm = GhostXForm{1.0, microseconds{0}}; // ghost == host
    const auto clientA0 = Timeline{Tempo{120.}, Beats{10.}, microseconds{0}};
    const auto clientB0 = Timeline{Tempo{120.}, Beats{2.}, microseconds{0}};
    const auto shared = ArrangementTimeline{Beats{17.}, Beats{17.}, microseconds{500000}};

    const auto beatAt =
      [&](const Timeline& session, const Timeline& client, const microseconds host)
    {
      const auto ca =
        detail::mapArrangementTimelineFromSessionToClient(shared, session, client, xForm);
      return (client.toBeats(host) - ca.beatOrigin).floating();
    };

    const auto host = microseconds{3000000};
    CHECK(beatAt(sessionTimeline, clientA0, host)
          == Approx(beatAt(sessionTimeline, clientB0, host)).margin(1e-6));

    const auto sessionNew = Timeline{Tempo{130.}, Beats{50.}, microseconds{0}};
    const auto clientA1 =
      updateClientTimelineFromSession(clientA0, sessionNew, microseconds{2500000}, xForm);
    const auto clientB1 =
      updateClientTimelineFromSession(clientB0, sessionNew, microseconds{2600000}, xForm);

    CHECK(beatAt(sessionNew, clientA1, host)
          == Approx(beatAt(sessionNew, clientB1, host)).margin(1e-6));
  }
}

} // namespace link
} // namespace ableton
