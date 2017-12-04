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
  microseconds micros() const
  {
    return microseconds{1};
  }
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

using MockController =
  Controller<PeerCountCallback, TempoCallback, MockClock, MockIoContext>;

} // anon namespace

TEST_CASE("Controller | ConstructOptimistically", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, MockClock{},
    util::injectVal(MockIoContext{}));

  CHECK(!controller.isEnabled());
  CHECK(!controller.isStartStopSyncEnabled());
  CHECK(0 == controller.numPeers());
  const auto tl = controller.sessionState().timeline;
  CHECK(Tempo{100.0} == tl.tempo);
}

TEST_CASE("Controller | ConstructWithInvalidTempo", "[Controller]")
{
  MockController controllerLowTempo(Tempo{1.0}, [](std::size_t) {}, [](Tempo) {},
    MockClock{}, util::injectVal(MockIoContext{}));
  const auto tlLow = controllerLowTempo.sessionState().timeline;
  CHECK(Tempo{20.0} == tlLow.tempo);

  MockController controllerHighTempo(Tempo{100000.0}, [](std::size_t) {}, [](Tempo) {},
    MockClock{}, util::injectVal(MockIoContext{}));
  const auto tlHigh = controllerHighTempo.sessionState().timeline;
  CHECK(Tempo{999.0} == tlHigh.tempo);
}

TEST_CASE("Controller | EnableDisable", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, MockClock{},
    util::injectVal(MockIoContext{}));

  controller.enable(true);
  CHECK(controller.isEnabled());
  controller.enable(false);
  CHECK(!controller.isEnabled());
}

TEST_CASE("Controller | EnableDisableStartStopSync", "[Controller]")
{
  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, MockClock{},
    util::injectVal(MockIoContext{}));

  controller.enableStartStopSync(true);
  CHECK(controller.isStartStopSyncEnabled());
  controller.enableStartStopSync(false);
  CHECK(!controller.isStartStopSyncEnabled());
}

TEST_CASE("Controller | SetAndGetSessionStateThreadSafe", "[Controller]")
{
  using namespace std::chrono;

  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, MockClock{},
    util::injectVal(MockIoContext{}));

  auto expectedTimeline = Timeline{Tempo{60.}, Beats{0.}, microseconds{0}};
  auto expectedStartStopState = StartStopState{true, microseconds{1}};
  auto expectedSessionState = SessionState{expectedTimeline, expectedStartStopState};
  controller.setSessionState(expectedSessionState, microseconds{0});
  auto sessionState = controller.sessionState();
  CHECK(expectedSessionState == sessionState);

  // Set session state with an outdated StartStopState
  const auto outdatedStartStopState = StartStopState{false, microseconds{1}};
  controller.setSessionState(
    SessionState{expectedTimeline, outdatedStartStopState}, microseconds{0});
  sessionState = controller.sessionState();
  CHECK(expectedSessionState == sessionState);

  // Set session state with a new StartStopState
  expectedTimeline = Timeline{Tempo{80.}, Beats{1.}, microseconds{6}};
  expectedStartStopState = StartStopState{false, microseconds{7}};
  expectedSessionState = SessionState{expectedTimeline, expectedStartStopState};
  controller.setSessionState(expectedSessionState, microseconds{0});
  sessionState = controller.sessionState();
  CHECK(expectedSessionState == sessionState);
}

TEST_CASE("Controller | SetAndGetSessionStateRealtimeSafe", "[Controller]")
{
  using namespace std::chrono;

  MockController controller(Tempo{100.0}, [](std::size_t) {}, [](Tempo) {}, MockClock{},
    util::injectVal(MockIoContext{}));

  auto expectedTimeline = Timeline{Tempo{110.}, Beats{0.}, microseconds{0}};
  auto expectedStartStopState = StartStopState{true, microseconds{1}};
  auto expectedSessionState = SessionState{expectedTimeline, expectedStartStopState};
  controller.setSessionStateRtSafe(expectedSessionState, microseconds{0});
  auto sessionState = controller.sessionState();
  CHECK(expectedSessionState == sessionState);

  // Set session state with an outdated StartStopState
  const auto outdatedStartStopState = StartStopState{false, microseconds{0}};
  controller.setSessionStateRtSafe(
    SessionState{expectedTimeline, outdatedStartStopState}, microseconds{0});
  sessionState = controller.sessionStateRtSafe();
  CHECK(expectedSessionState == sessionState);

  // Set session state with a new StartStopState
  expectedTimeline = Timeline{Tempo{90.}, Beats{1.4}, microseconds{5}};
  expectedStartStopState = StartStopState{false, microseconds{6}};
  expectedSessionState = SessionState{expectedTimeline, expectedStartStopState};
  controller.setSessionStateRtSafe(expectedSessionState, microseconds{0});
  sessionState = controller.sessionStateRtSafe();
  CHECK(expectedSessionState == sessionState);
}

} // namespace link
} // namespace ableton
