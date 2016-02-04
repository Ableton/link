// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/PeerGateway.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <ableton/discovery/util/test/Timer.hpp>

namespace ableton
{
namespace link
{
namespace discovery
{
namespace
{

struct TestNodeState : std::tuple<std::string, double>
{
  TestNodeState(std::string ident, double tempo)
    : std::tuple<std::string, double>(std::move(ident), std::move(tempo))
  {
  }

  friend std::string ident(const TestNodeState& state)
  {
    return std::get<0>(state);
  }
};

struct TestMessenger
{
  template <typename Handler>
  friend void receive(TestMessenger& m, Handler handler)
  {
    m.receivePeerState = [handler](const PeerState<TestNodeState>& msg) {
      handler(msg);
    };

    m.receiveByeBye = [handler](const ByeBye<std::string>& msg) {
      handler(msg);
    };
  }

  std::function<void (const PeerState<TestNodeState>&)> receivePeerState;
  std::function<void (const ByeBye<std::string>&)> receiveByeBye;
};

struct TestObserver
{
  using NodeState = TestNodeState;
  using NodeId = std::string;

  friend void sawPeer(TestObserver& observer, const NodeState& peer)
  {
    observer.mPeersSeen.push_back(peer);
  }

  friend void peerLeft(TestObserver& observer, const NodeId& id)
  {
    observer.mPeersLeft.push_back(id);
  }

  friend void peerTimedOut(TestObserver& observer, const NodeId& id)
  {
    observer.mPeersTimedOut.push_back(id);
  }

  std::vector<NodeState> mPeersSeen;
  std::vector<NodeId> mPeersLeft;
  std::vector<NodeId> mPeersTimedOut;
};

void expectPeersSeen(std::vector<TestNodeState> expected, const TestObserver& observer)
{
  CHECK(expected == observer.mPeersSeen);
}

void expectPeersLeft(std::vector<std::string> expected, const TestObserver& observer)
{
  CHECK(expected == observer.mPeersLeft);
}

void expectPeersTimedOut(std::vector<std::string> expected, const TestObserver& observer)
{
  CHECK(expected == observer.mPeersTimedOut);
}

// Test peers
const auto peerA = TestNodeState{"peerA", 120};
const auto peerB = TestNodeState{"peerB", 150};

} // anonymous namespace

TEST_CASE("PeerGateway | NoActivity", "[PeerGateway]")
{
  TestObserver observer;
  util::test::Timer timer;
  auto listener = makePeerGateway(
    TestMessenger{}, std::ref(observer), util::injectRef(timer), util::NullLog{});
  timer.advance(std::chrono::seconds(10));

  // Without any outside interaction but the passage of time, our
  // listener should not have seen any peers.
  CHECK(observer.mPeersSeen.empty());
  CHECK(observer.mPeersLeft.empty());
  CHECK(observer.mPeersTimedOut.empty());
}

TEST_CASE("PeerGateway | ReceivedPeerState", "[PeerGateway]")
{
  TestObserver observer;
  TestMessenger messenger;
  auto listener = makePeerGateway(
    std::ref(messenger), std::ref(observer), util::injectVal(util::test::Timer{}),
    util::NullLog{});

  messenger.receivePeerState({peerA, 5});

  expectPeersSeen({peerA}, observer);
}

TEST_CASE("PeerGateway | TwoPeersOneLeaves", "[PeerGateway]")
{
  TestObserver observer;
  TestMessenger messenger;
  auto listener = makePeerGateway(
    std::ref(messenger), std::ref(observer), util::injectVal(util::test::Timer{}),
    util::NullLog{});

  messenger.receivePeerState({peerA, 5});
  messenger.receivePeerState({peerB, 5});
  messenger.receiveByeBye({ident(peerA)});

  expectPeersSeen({peerA, peerB}, observer);
  expectPeersLeft({ident(peerA)}, observer);
}

TEST_CASE("PeerGateway | TwoPeersOneTimesOut", "[PeerGateway]")
{
  TestObserver observer;
  TestMessenger messenger;
  util::test::Timer timer;
  auto listener = makePeerGateway(
    std::ref(messenger), std::ref(observer), util::injectRef(timer), util::NullLog{});

  messenger.receivePeerState({peerA, 5});
  messenger.receivePeerState({peerB, 10});

  timer.advance(std::chrono::seconds(3));
  expectPeersTimedOut({}, observer);
  timer.advance(std::chrono::seconds(4));
  expectPeersTimedOut({ident(peerA)}, observer);
}

TEST_CASE("PeerGateway | PeerTimesOutAndIsSeenAgain", "[PeerGateway]")
{
  TestObserver observer;
  TestMessenger messenger;
  util::test::Timer timer;
  auto listener = makePeerGateway(
    std::ref(messenger), std::ref(observer), util::injectRef(timer), util::NullLog{});

  messenger.receivePeerState({peerA, 5});
  timer.advance(std::chrono::seconds(7));

  expectPeersTimedOut({ident(peerA)}, observer);

  messenger.receivePeerState({peerA, 5});
  timer.advance(std::chrono::seconds(3));

  expectPeersSeen({peerA, peerA}, observer);
  expectPeersTimedOut({ident(peerA)}, observer);
}

} // namespace discovery
} // namespace link
} // namespace ableton
