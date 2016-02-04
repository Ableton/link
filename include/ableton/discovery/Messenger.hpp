// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

namespace ableton
{
namespace link
{
namespace discovery
{

// Message types used in the Ableton service discovery protocol. There
// are two logical messages: a state dump and a bye bye.
//
// A state dump provides all relevant information about the peer's
// current state as well as a Time To Live (TTL) value that indicates
// how many seconds this state should be considered valid.
//
// The bye bye indicates that the sender is leaving the session.

template <typename NodeState>
struct PeerState
{
  NodeState peerState;
  int ttl;
};

template <typename NodeId>
struct ByeBye
{
  NodeId peerId;
};

// Concept: Messenger
//
// This type is purely for documentation purposes, concrete
// implementations of the concept will be used in real code. The
// messenger concept abstracts the details of communication over a
// given transport type and exposes a logical message passing
// interface to clients. For example, models of this concept may
// communicate over ip networks using UDP packets or over USB or
// Bluetooth channels.

struct Messenger
{
  // Broadcast the current state of the system to all peers. May throw
  // std::runtime_error if assembling a broadcast message fails or if
  // there is an error at the transport layer.
  friend void broadcastState(Messenger& m);

  // Asynchronous receive function for incoming messages from peers. Will
  // return immediately and the handler will be invoked when a message
  // is received. Handler must have operator() overloads for PeerState and
  // ByeBye messages.
  template <typename Handler>
  friend void receive(Messenger& m, Handler handler);
};

} // namespace discovery
} // namespace link
} // namespace ableton
