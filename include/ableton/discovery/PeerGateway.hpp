// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/UdpMessenger.hpp>
#include <ableton/discovery/asio/AsioService.hpp>
#include <ableton/discovery/v1/Messages.hpp>
#include <ableton/discovery/util/Unwrap.hpp>
#include <ableton/discovery/util/SafeAsyncHandler.hpp>
#include <memory>

namespace ableton
{
namespace link
{
namespace discovery
{

template <typename Messenger, typename PeerObserver, typename Timer, typename Log>
struct PeerGateway
{
  // The peer types are defined by the observer but must match with those
  // used by the Messenger
  using ObserverT = util::member_type_of<PeerObserver>;
  using NodeState = typename ObserverT::NodeState;
  using NodeId = typename ObserverT::NodeId;
  using TimerError = typename util::Injected<Timer>::type::ErrorCode;

  PeerGateway(
    Messenger messenger,
    PeerObserver observer,
    util::Injected<Timer> timer,
    Log log)
    : mpImpl(
        new Impl(
          std::move(messenger), std::move(observer), std::move(timer), std::move(log)))
  {
    mpImpl->listen();
  }

  PeerGateway(const PeerGateway&) = delete;
  PeerGateway& operator=(const PeerGateway&) = delete;

  PeerGateway(PeerGateway&& rhs)
    : mpImpl(std::move(rhs.mpImpl))
  {
  }

  friend void broadcastState(PeerGateway& gateway)
  {
    broadcastState(gateway.mpImpl->mMessenger);
  }

private:
  using PeerTimeout = std::pair<std::chrono::system_clock::time_point, NodeId>;
  using PeerTimeouts = std::vector<PeerTimeout>;

  struct Impl : std::enable_shared_from_this<Impl>
  {
    Impl(Messenger messenger, PeerObserver observer, util::Injected<Timer> timer, Log log)
      : mMessenger(std::move(messenger))
      , mObserver(std::move(observer))
      , mPruneTimer(std::move(timer))
      , mLog(std::move(log))
    {
    }

    void listen()
    {
      receive(mMessenger, util::makeAsyncSafe(this->shared_from_this()));
    }

    // Operators for handling incoming messages
    void operator()(const PeerState<NodeState>& msg)
    {
      onPeerState(msg.peerState, msg.ttl);
      listen();
    }

    void operator()(const ByeBye<NodeId>& msg)
    {
      onByeBye(msg.peerId);
      listen();
    }

    void onPeerState(const NodeState& nodeState, const int ttl)
    {
      using namespace std;
      const auto peerId = ident(nodeState);
      const auto existing = findPeer(peerId);
      if (existing != end(mPeerTimeouts))
      {
        // If the peer is already present in our timeout list, remove it
        // as it will be re-inserted below.
        mPeerTimeouts.erase(existing);
      }

      auto newTo = make_pair(mPruneTimer->now() + std::chrono::seconds(ttl), peerId);
      mPeerTimeouts.insert(
        upper_bound(begin(mPeerTimeouts), end(mPeerTimeouts), newTo, TimeoutCompare{}),
        move(newTo));

      sawPeer(mObserver, nodeState);
      scheduleNextPruning();
    }

    void onByeBye(const NodeId& peerId)
    {
      const auto it = findPeer(peerId);
      if (it != mPeerTimeouts.end())
      {
        peerLeft(mObserver, it->second);
        mPeerTimeouts.erase(it);
      }
    }

    void pruneExpiredPeers()
    {
      using namespace std;

      const auto test = make_pair(mPruneTimer->now(), NodeId{});
      debug(mLog) << "pruning peers @ " << test.first.time_since_epoch().count();

      const auto endExpired =
        lower_bound(begin(mPeerTimeouts), end(mPeerTimeouts), test, TimeoutCompare{});

      for_each(begin(mPeerTimeouts), endExpired, [this](const PeerTimeout& pto) {
          info(mLog) << "pruning peer " << pto.second;
          peerTimedOut(mObserver, pto.second);
        });
      mPeerTimeouts.erase(begin(mPeerTimeouts), endExpired);
      scheduleNextPruning();
    }

    void scheduleNextPruning()
    {
      // Find the next peer to expire and set the timer based on it
      if (!mPeerTimeouts.empty())
      {
        // Add a second of padding to the timer to avoid over-eager timeouts
        const auto t = mPeerTimeouts.front().first + std::chrono::seconds(1);

        debug(mLog) <<
          "scheduling next pruning for " << t.time_since_epoch().count() <<
          " because of peer " << mPeerTimeouts.front().second;

        mPruneTimer->expires_at(t);
        mPruneTimer->async_wait([this](const TimerError e) {
            if (!e)
            {
              pruneExpiredPeers();
            }
          });
      }
    }

    struct TimeoutCompare
    {
      bool operator()(const PeerTimeout& lhs, const PeerTimeout& rhs) const
      {
        return lhs.first < rhs.first;
      }
    };

    typename PeerTimeouts::iterator findPeer(const NodeId& peerId)
    {
      return std::find_if(
        begin(mPeerTimeouts), end(mPeerTimeouts),
        [&peerId](const PeerTimeout& pto) { return pto.second == peerId; });
    }

    Messenger mMessenger;
    PeerObserver mObserver;
    util::Injected<Timer> mPruneTimer;
    Log mLog;
    PeerTimeouts mPeerTimeouts; // Invariant: sorted by time_point
  };

  std::shared_ptr<Impl> mpImpl;
};

template <typename Messenger, typename PeerObserver, typename Timer, typename Log>
PeerGateway<Messenger, PeerObserver, Timer, Log> makePeerGateway(
  Messenger messenger,
  PeerObserver observer,
  util::Injected<Timer> timer,
  Log log)
{
  return {std::move(messenger), std::move(observer), std::move(timer), std::move(log)};
}

// IpV4 gateway types
template <typename StateQuery, typename Log>
using IpV4Messenger =
  UdpMessenger<IpV4Interface<v1::kMaxMessageSize>, StateQuery, util::AsioTimer, Log>;

template <typename PeerObserver, typename StateQuery, typename Log>
using IpV4Gateway =
  PeerGateway<IpV4Messenger<StateQuery, Log>, PeerObserver, util::AsioTimer, Log>;

// Factory function to make a PeerGateway bound to an ip v4 interface
// with the given address.
template <typename PeerObserver, typename StateQuery, typename Log>
IpV4Gateway<PeerObserver, StateQuery, Log> makeIpV4Gateway(
  util::AsioService& io,
  const asio::ip::address_v4& addr,
  PeerObserver observer,
  StateQuery query,
  Log log)
{
  using namespace std;
  using namespace util;

  const uint8_t ttl = 5;
  const uint8_t ttlRatio = 20;
  auto iface = IpV4Interface<v1::kMaxMessageSize>(io, addr);

  auto messenger = makeUdpMessenger(
    move(iface), move(query), injectVal(io.makeTimer()), log, ttl, ttlRatio);
  return {move(messenger), move(observer), injectVal(io.makeTimer()), move(log)};
}

} // namespace discovery
} // namespace link
} // namespace ableton
