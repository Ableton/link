// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/MeasurementEndpointV4.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/discovery/Payload.hpp>

namespace ableton
{
namespace link
{

// A state type for peers. PeerState stores the normal NodeState plus
// additional information (the remote endpoint at which to find its
// ping/pong measurement server).

struct PeerState
{
  using NodeId = NodeId;

  friend NodeId ident(const PeerState& state)
  {
    return ident(state.nodeState);
  }

  friend SessionId sessionId(const PeerState& state)
  {
    return state.nodeState.sessionId;
  }

  friend Timeline timeline(const PeerState& state)
  {
    return state.nodeState.timeline;
  }

  friend bool operator==(const PeerState& lhs, const PeerState& rhs)
  {
    return lhs.nodeState == rhs.nodeState && lhs.endpoint == rhs.endpoint;
  }

  friend auto toPayload(const PeerState& state) ->
    decltype(
      std::declval<NodeState::Payload>() +
      discovery::makePayload(MeasurementEndpointV4{{}}))
  {
    return toPayload(state.nodeState) +
      discovery::makePayload(MeasurementEndpointV4{state.endpoint});
  }

  template <typename It, typename LogT>
  static PeerState fromPayload(NodeId id, It begin, It end, LogT log)
  {
    using namespace std;
    auto peerState = PeerState{NodeState::fromPayload(move(id), begin, end, log), {}};

    discovery::parsePayload<MeasurementEndpointV4>(
      move(begin), move(end), move(log),
      [&peerState](MeasurementEndpointV4 me4) {
        peerState.endpoint = move(me4.ep);
      });
    return peerState;
  }

  NodeState nodeState;
  asio::ip::udp::endpoint endpoint;
};

} // namespace link
} // namespace ableton
