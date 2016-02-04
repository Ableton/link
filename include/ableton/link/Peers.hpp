// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/PeerState.hpp>
#include <ableton/discovery/util/Injected.hpp>
#include <cassert>

namespace ableton
{
namespace link
{

// SessionMembershipCallback is invoked when any change to session
// membership occurs (when any peer joins or leaves a session)
//
// SessionTimelineCallback is invoked with a session id and a timeline
// whenever a new combination of these values is seen

template <
  typename Io,
  typename SessionMembershipCallback,
  typename SessionTimelineCallback>
class Peers
{
  // non-moveable private implementation type
  struct Impl;

public:
  using Peer = std::pair<PeerState, asio::ip::address>;

  Peers(
    util::Injected<Io> io,
    SessionMembershipCallback membership,
    SessionTimelineCallback timeline)
    : mpImpl(std::make_shared<Impl>(
               std::move(io), std::move(membership), std::move(timeline)))
  {
  }

  // The set of peers for a given session, ordered by (peerId, addr).
  // The result will possibly contain multiple entries for the same
  // peer if it is visible through multiple gateways.
  friend std::vector<Peer> sessionPeers(const Peers& peers, const SessionId& sid)
  {
    using namespace std;
    vector<Peer> result;
    auto& peerVec = peers.mpImpl->mPeers;
    copy_if(begin(peerVec), end(peerVec), back_inserter(result), SessionMemberPred{sid});
    return result;
  }

  // Number of individual for a given session.
  friend std::size_t uniqueSessionPeerCount(const Peers& peers, const SessionId& sid)
  {
    using namespace std;
    auto peerVec = sessionPeers(peers, sid);
    auto last = unique(begin(peerVec), end(peerVec), [](const Peer &a, const Peer &b){
        return ident(a.first) == ident(b.first);
    });
    return static_cast<size_t>(distance(begin(peerVec), last));
  }

  // Purge all cached peers that are members of the given session
  friend void forgetSession(Peers& peers, const SessionId& sid)
  {
    using namespace std;
    auto& peerVec = peers.mpImpl->mPeers;
    peerVec.erase(
      remove_if(begin(peerVec), end(peerVec), SessionMemberPred{sid}),
      end(peerVec));
  }

  friend void resetPeers(Peers& peers)
  {
    peers.mpImpl->mPeers.clear();
  }

  // Observer type that monitors peer discovery on a particular
  // gateway and relays the information to a Peers instance.
  // Models the PeerObserver concept from the discovery module.
  struct GatewayObserver
  {
    using NodeState = PeerState;
    using NodeId = NodeId;

    GatewayObserver(std::shared_ptr<Impl> pImpl, asio::ip::address addr)
      : mpImpl(std::move(pImpl))
      , mAddr(std::move(addr))
    {
    }
    GatewayObserver(const GatewayObserver&) = delete;

    GatewayObserver(GatewayObserver&& rhs)
      : mpImpl(std::move(rhs.mpImpl))
      , mAddr(std::move(rhs.mAddr))
    {
    }

    ~GatewayObserver()
    {
      // Check to handle the moved from case
      if (mpImpl)
      {
        auto& io = *mpImpl->mIo;
        io.post(Deleter{*this});
      }
    }

    // model the PeerObserver concept from discovery
    friend void sawPeer(GatewayObserver& observer, const PeerState& state)
    {
      auto pImpl = observer.mpImpl;
      auto addr = observer.mAddr;
      assert(pImpl);
      pImpl->mIo->post([pImpl, addr, state] {
          pImpl->sawPeerOnGateway(std::move(state), std::move(addr));
        });
    }

    friend void peerLeft(GatewayObserver& observer, const NodeId& id)
    {
      auto pImpl = observer.mpImpl;
      auto addr = observer.mAddr;
      pImpl->mIo->post([pImpl, addr, id] {
          pImpl->peerLeftGateway(std::move(id), std::move(addr));
        });
    }

    friend void peerTimedOut(GatewayObserver& observer, const NodeId& id)
    {
      auto pImpl = observer.mpImpl;
      auto addr = observer.mAddr;
      pImpl->mIo->post([pImpl, addr, id] {
          pImpl->peerLeftGateway(std::move(id), std::move(addr));
        });
    }

    struct Deleter
    {
      Deleter(GatewayObserver& observer)
        : mpImpl(std::move(observer.mpImpl))
        , mAddr(std::move(observer.mAddr))
      {
      }

      void operator()()
      {
        mpImpl->gatewayClosed(mAddr);
      }

      std::shared_ptr<Impl> mpImpl;
      asio::ip::address mAddr;
    };

    std::shared_ptr<Impl> mpImpl;
    asio::ip::address mAddr;
  };

  // Factory function for the gateway observer
  friend GatewayObserver makeGatewayObserver(Peers& peers, asio::ip::address addr)
  {
    return GatewayObserver{peers.mpImpl, std::move(addr)};
  }

private:
  struct Impl
  {
    Impl(
      util::Injected<Io> io,
      SessionMembershipCallback membership,
      SessionTimelineCallback timeline)
      : mIo(std::move(io))
      , mSessionMembershipCallback(std::move(membership))
      , mSessionTimelineCallback(std::move(timeline))
    {
    }

    void sawPeerOnGateway(PeerState peerState, asio::ip::address gatewayAddr)
    {
      using namespace std;

      const auto peerSession = sessionId(peerState);
      const auto peerTimeline = timeline(peerState);
      bool isNewSessionTimeline = false;
      bool didSessionMembershipChange = false;
      {
        isNewSessionTimeline = !sessionTimelineExists(peerSession, peerTimeline);

        auto peer = make_pair(move(peerState), move(gatewayAddr));
        const auto idRange = equal_range(begin(mPeers), end(mPeers), peer, PeerIdComp{});

        if (idRange.first == idRange.second)
        {
          // This peer is not currently known on any gateway
          didSessionMembershipChange = true;
          mPeers.insert(move(idRange.first), move(peer));
        }
        else
        {
          // We've seen this peer before... does it have a new session?
          didSessionMembershipChange =
            all_of(idRange.first, idRange.second, [&peerSession](const Peer& test) {
                return sessionId(test.first) != peerSession;
              });

          // was it on this gateway?
          const auto addrRange =
            equal_range(idRange.first, idRange.second, peer, AddrComp{});

          if (addrRange.first == addrRange.second)
          {
            // First time on this gateway, add it
            mPeers.insert(move(addrRange.first), move(peer));
          }
          else
          {
            // We have an entry for this peer on this gateway, update it
            *addrRange.first = move(peer);
          }
        }
      } // end lock

      // Invoke callbacks outside the critical section
      if (isNewSessionTimeline)
      {
        mSessionTimelineCallback(peerSession, peerTimeline);
      }

      if (didSessionMembershipChange)
      {
        mSessionMembershipCallback();
      }
    }

    void peerLeftGateway(
      const NodeId& nodeId,
      const asio::ip::address& gatewayAddr)
    {
      using namespace std;

      bool didSessionMembershipChange = false;
      {
        auto it = find_if(begin(mPeers), end(mPeers), [&](const Peer& peer) {
            return ident(peer.first) == nodeId && peer.second == gatewayAddr;
          });

        if (it != end(mPeers))
        {
          mPeers.erase(move(it));
          didSessionMembershipChange = true;
        }
      } // end lock

      if (didSessionMembershipChange)
      {
        mSessionMembershipCallback();
      }
    }

    void gatewayClosed(const asio::ip::address& gatewayAddr)
    {
      using namespace std;

      {
        mPeers.erase(
          remove_if(begin(mPeers), end(mPeers), [&gatewayAddr](const Peer& peer) {
              return peer.second == gatewayAddr;
            }),
          end(mPeers));
      } // end lock

      mSessionMembershipCallback();
    }

    bool sessionTimelineExists(const SessionId& session, const Timeline& tl)
    {
      using namespace std;
      return find_if(begin(mPeers), end(mPeers), [&](const Peer& peer) {
          return sessionId(peer.first) == session && timeline(peer.first) == tl;
        }) != end(mPeers);
    }

    struct PeerIdComp
    {
      bool operator()(const Peer& lhs, const Peer& rhs) const
      {
        return ident(lhs.first) < ident(rhs.first);
      }
    };

    struct AddrComp
    {
      bool operator()(const Peer& lhs, const Peer& rhs) const
      {
        return lhs.second < rhs.second;
      }
    };

    util::Injected<Io> mIo;
    SessionMembershipCallback mSessionMembershipCallback;
    SessionTimelineCallback mSessionTimelineCallback;
    std::vector<Peer> mPeers; // sorted by peerId, unique by (peerId, addr)
  };

  struct SessionMemberPred
  {
    bool operator()(const Peer& peer) const
    {
      return sessionId(peer.first) == sid;
    }

    const SessionId& sid;
  };

  std::shared_ptr<Impl> mpImpl;
};

template <
  typename Io,
  typename SessionMembershipCallback,
  typename SessionTimelineCallback>
Peers<Io, SessionMembershipCallback, SessionTimelineCallback> makePeers(
  util::Injected<Io> io,
  SessionMembershipCallback membershipCallback,
  SessionTimelineCallback timelineCallback)
{
  return {std::move(io), std::move(membershipCallback), std::move(timelineCallback)};
}

} // namespace link
} // namespace ableton
