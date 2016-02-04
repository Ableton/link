// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Peers.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/discovery/util/test/IoService.hpp>

namespace ableton
{
namespace link
{
namespace
{

struct SessionMembershipCallback
{
  void operator()()
  {
    ++calls;
  }

  std::size_t calls = 0;
};

struct SessionTimelineCallback
{
  void operator()(const SessionId& session, const Timeline& tl)
  {
    sessionTimelines.push_back(std::make_pair(session, tl));
  }

  std::vector<std::pair<SessionId, Timeline>> sessionTimelines;
};

const auto fooPeer = PeerState{
  {NodeId::random(),
   NodeId::random(),
   Timeline{Tempo{60.}, Beats{1.}, std::chrono::microseconds{1234}}},
  {}
};

const auto barPeer = PeerState{
  {NodeId::random(),
   NodeId::random(),
   Timeline{Tempo{120.}, Beats{10.}, std::chrono::microseconds{500}}},
  {}
};

const auto bazPeer = PeerState{
  {NodeId::random(),
   NodeId::random(),
   Timeline{Tempo{100.}, Beats{4.}, std::chrono::microseconds{100}}},
  {}
};

const auto gateway1 = asio::ip::address::from_string("123.123.123.123");
const auto gateway2 = asio::ip::address::from_string("210.210.210.210");

using PeerVector = std::vector<
  typename Peers<
    util::test::IoService&, SessionMembershipCallback, SessionTimelineCallback>::Peer>;

void expectPeers(PeerVector expected, PeerVector actual)
{
  CHECK(expected == actual);
}

void expectSessionTimelines(
  std::vector<std::pair<SessionId, Timeline>> expected,
  SessionTimelineCallback callback)
{
  CHECK(expected == callback.sessionTimelines);
}

}

TEST_CASE("Peers | EmptySessionPeersAfterInit", "[Peers]")
{
  auto io = util::test::IoService{};
  auto peers = makePeers(
    util::injectRef(io), SessionMembershipCallback{}, SessionTimelineCallback{});
  expectPeers({}, sessionPeers(peers, sessionId(fooPeer)));
}

TEST_CASE("Peers | AddAndFindPeer", "[Peers]")
{
  auto membership = SessionMembershipCallback{};
  auto sessions = SessionTimelineCallback{};
  auto io = util::test::IoService{};
  auto peers = makePeers(util::injectRef(io), std::ref(membership), std::ref(sessions));
  auto observer = makeGatewayObserver(peers, gateway1);

  sawPeer(observer, fooPeer);
  io.runHandlers();

  expectPeers({{fooPeer, gateway1}}, sessionPeers(peers, sessionId(fooPeer)));
  CHECK(1u == membership.calls);
  expectSessionTimelines({make_pair(sessionId(fooPeer), timeline(fooPeer))}, sessions);
}

TEST_CASE("Peers | AddAndRemovePeer", "[Peers]")
{
  auto membership = SessionMembershipCallback{};
  auto io = util::test::IoService{};
  auto peers = makePeers(
    util::injectRef(io), std::ref(membership), SessionTimelineCallback{});
  auto observer = makeGatewayObserver(peers, gateway1);

  sawPeer(observer, fooPeer);
  peerLeft(observer, ident(fooPeer));
  io.runHandlers();

  expectPeers({}, sessionPeers(peers, sessionId(fooPeer)));
  CHECK(2u == membership.calls);
}

TEST_CASE("Peers | AddTwoPeersRemoveOne", "[Peers]")
{
  auto membership = SessionMembershipCallback{};
  auto sessions = SessionTimelineCallback{};
  auto io = util::test::IoService{};
  auto peers = makePeers(util::injectRef(io), std::ref(membership), std::ref(sessions));
  auto observer = makeGatewayObserver(peers, gateway1);
  sawPeer(observer, fooPeer);
  sawPeer(observer, barPeer);
  peerLeft(observer, ident(fooPeer));
  io.runHandlers();

  expectPeers({}, sessionPeers(peers, sessionId(fooPeer)));
  expectPeers({{barPeer, gateway1}}, sessionPeers(peers, sessionId(barPeer)));
  CHECK(3u == membership.calls);
}

TEST_CASE("Peers | AddThreePeersTwoOnSameGateway", "[Peers]")
{
  auto membership = SessionMembershipCallback{};
  auto sessions = SessionTimelineCallback{};
  auto io = util::test::IoService{};
  auto peers = makePeers(util::injectRef(io), std::ref(membership), std::ref(sessions));
  auto observer1 = makeGatewayObserver(peers, gateway1);
  auto observer2 = makeGatewayObserver(peers, gateway2);

  sawPeer(observer1, fooPeer);
  sawPeer(observer2, fooPeer);
  sawPeer(observer1, barPeer);
  sawPeer(observer1, bazPeer);
  io.runHandlers();

  expectPeers(
    {{fooPeer, gateway1}, {fooPeer, gateway2}}, sessionPeers(peers, sessionId(fooPeer)));
  CHECK(3 == membership.calls);
}

TEST_CASE("Peers | CloseGateway", "[Peers]")
{
  auto membership = SessionMembershipCallback{};
  auto sessions = SessionTimelineCallback{};
  auto io = util::test::IoService{};
  auto peers = makePeers(util::injectRef(io), std::ref(membership), std::ref(sessions));
  auto observer1 = makeGatewayObserver(peers, gateway1);
  {
    // The observer will close the gateway when it goes out of scope
    auto observer2 = makeGatewayObserver(peers, gateway2);
    sawPeer(observer2, fooPeer);
    sawPeer(observer2, barPeer);
    sawPeer(observer1, fooPeer);
    sawPeer(observer2, bazPeer);
  }
  io.runHandlers();

  expectPeers({{fooPeer, gateway1}}, sessionPeers(peers, sessionId(fooPeer)));
  CHECK(4 == membership.calls);
}

} // namespace link
} // namespace ableton
