// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/Payload.hpp>
#include <ableton/link/NodeId.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link/Timeline.hpp>

namespace ableton
{
namespace link
{

struct NodeState
{
  using Payload =
    decltype(discovery::makePayload(Timeline{}, SessionMembership{}));

  friend NodeId ident(const NodeState& state)
  {
    return state.nodeId;
  }

  friend bool operator==(const NodeState& lhs, const NodeState& rhs)
  {
    return
      std::tie(lhs.nodeId, lhs.sessionId, lhs.timeline) ==
      std::tie(rhs.nodeId, rhs.sessionId, rhs.timeline);
  }

  friend Payload toPayload(const NodeState& state)
  {
    return discovery::makePayload(
      state.timeline, SessionMembership{state.sessionId});
  }

  template <typename It, typename Log>
  static NodeState fromPayload(NodeId id, It begin, It end, Log log)
  {
    using namespace std;
    auto state = NodeState{move(id), {}, {}};
    discovery::parsePayload<Timeline, SessionMembership>(
      move(begin), move(end), move(log),
      [&state](Timeline tl) {
        state.timeline = move(tl);
      },
      [&state](SessionMembership sm) {
        state.sessionId = move(sm.sessionId);
      });
    return state;
  }

  NodeId nodeId;
  SessionId sessionId;
  Timeline timeline;
};

} // namespace link
} // namespace ableton
