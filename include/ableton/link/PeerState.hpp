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

#pragma once

#include <ableton/discovery/Payload.hpp>
#include <ableton/link/EndpointV4.hpp>
#include <ableton/link/EndpointV6.hpp>
#include <ableton/link/NodeState.hpp>

namespace ableton
{
namespace link
{

// A state type for peers. PeerState stores the normal NodeState plus
// additional information (the remote endpoint at which to find its
// ping/pong measurement server).

struct PeerState
{
  static constexpr std::int32_t kMep4Key = 'mep4';
  using MeasurementEndpointV4 = EndpointV4<kMep4Key>;
  static_assert(MeasurementEndpointV4::key == 0x6d657034, "Unexpected byte order");

  static constexpr std::int32_t kMep6Key = 'mep6';
  using MeasurementEndpointV6 = EndpointV6<kMep6Key>;
  static_assert(MeasurementEndpointV6::key == 0x6d657036, "Unexpected byte order");


  using IdType = NodeId;

  IdType ident() const { return nodeState.ident(); }

  SessionId sessionId() const { return nodeState.sessionId; }

  Timeline timeline() const { return nodeState.timeline; }

  StartStopState startStopState() const { return nodeState.startStopState; }

  friend bool operator==(const PeerState& lhs, const PeerState& rhs)
  {
    return lhs.nodeState == rhs.nodeState
           && lhs.measurementEndpoint == rhs.measurementEndpoint;
  }

  friend auto toPayload(const PeerState& state)
    -> decltype(std::declval<NodeState::Payload>()
                + discovery::makePayload(MeasurementEndpointV4{{}})
                + discovery::makePayload(MeasurementEndpointV6{{}}))
  {
    // This implements a switch if either an IPv4 or IPv6 endpoint is serialized.
    // MeasurementEndpoints that contain an endpoint that does not match the IP protocol
    // version return a sizeInByteStream() of zero and won't be serialized.
    return toPayload(state.nodeState)
           + discovery::makePayload(MeasurementEndpointV4{state.measurementEndpoint})
           + discovery::makePayload(MeasurementEndpointV6{state.measurementEndpoint});
  }

  template <typename It>
  static PeerState fromPayload(NodeId id, It begin, It end)
  {
    using namespace std;
    auto peerState = PeerState{NodeState::fromPayload(std::move(id), begin, end), {}};

    discovery::parsePayload<MeasurementEndpointV4, MeasurementEndpointV6>(
      std::move(begin),
      std::move(end),
      [&peerState](MeasurementEndpointV4 me4)
      { peerState.measurementEndpoint = std::move(me4.ep); },
      [&peerState](MeasurementEndpointV6 me6)
      { peerState.measurementEndpoint = std::move(me6.ep); });
    return peerState;
  }

  NodeState nodeState;
  discovery::UdpEndpoint measurementEndpoint;
};

} // namespace link
} // namespace ableton
