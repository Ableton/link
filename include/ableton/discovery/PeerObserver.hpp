// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

namespace ableton
{
namespace link
{
namespace discovery
{

// Concept: PeerObserver
//
// Represents a type that accepts notifications about the comings and
// goings of peers on a particular gateway. This type is for
// documentation purposes only.
template <typename NodeStateT, typename NodeId>
struct PeerObserver
{
  using NodeState = NodeStateT;

  friend void sawPeer(PeerObserver& observer, const NodeState& peer);

  friend void peerLeft(PeerObserver& observer, const NodeId& id);

  friend void peerTimedOut(PeerObserver& observer, const NodeId& id);
};

} // namespace discovery
} // namespace link
} // namespace ableton
