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
#include <ableton/link/ArrangementTimeline.hpp>
#include <ableton/link/NodeId.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link/StartStopState.hpp>
#include <ableton/link/Timeline.hpp>

#include <chrono>
#include <optional>

namespace ableton
{
namespace link
{

struct NodeState
{
  struct OptionalArrangementTimelineEntry
  {
    static const std::int32_t key = ArrangementTimeline::key;

    std::optional<ArrangementTimeline> value;

    friend std::uint32_t sizeInByteStream(const OptionalArrangementTimelineEntry& entry)
    {
      return entry.value ? sizeInByteStream(*entry.value) : 0;
    }

    template <typename It>
    friend It toNetworkByteStream(const OptionalArrangementTimelineEntry& entry, It out)
    {
      return entry.value ? toNetworkByteStream(*entry.value, std::move(out)) : out;
    }

    template <typename It>
    static std::pair<OptionalArrangementTimelineEntry, It> fromNetworkByteStream(It begin,
                                                                                 It end)
    {
      auto result = ArrangementTimeline::fromNetworkByteStream(std::move(begin), end);
      return std::make_pair(OptionalArrangementTimelineEntry{std::move(result.first)},
                            std::move(result.second));
    }
  };

  using Payload = decltype(discovery::makePayload(Timeline{},
                                                  SessionMembership{},
                                                  StartStopState{},
                                                  OptionalArrangementTimelineEntry{}));

  NodeId ident() const { return nodeId; }

  friend bool operator==(const NodeState& lhs, const NodeState& rhs)
  {
    return std::tie(lhs.nodeId,
                    lhs.sessionId,
                    lhs.timeline,
                    lhs.startStopState,
                    lhs.arrangementTimeline)
           == std::tie(rhs.nodeId,
                       rhs.sessionId,
                       rhs.timeline,
                       rhs.startStopState,
                       rhs.arrangementTimeline);
  }

  friend Payload toPayload(const NodeState& state)
  {
    return discovery::makePayload(
      state.timeline,
      SessionMembership{state.sessionId},
      state.startStopState,
      OptionalArrangementTimelineEntry{state.arrangementTimeline});
  }

  template <typename It>
  static NodeState fromPayload(NodeId nodeId, It begin, It end)
  {
    using namespace std;
    auto nodeState = NodeState{std::move(nodeId), {}, {}, {}, std::nullopt};
    discovery::parsePayload<Timeline,
                            SessionMembership,
                            StartStopState,
                            OptionalArrangementTimelineEntry>(
      std::move(begin),
      std::move(end),
      [&nodeState](Timeline tl) { nodeState.timeline = std::move(tl); },
      [&nodeState](SessionMembership membership)
      { nodeState.sessionId = std::move(membership.sessionId); },
      [&nodeState](StartStopState ststst)
      { nodeState.startStopState = std::move(ststst); },
      [&nodeState](OptionalArrangementTimelineEntry entry)
      { nodeState.arrangementTimeline = std::move(entry.value); });
    return nodeState;
  }

  NodeId nodeId;
  SessionId sessionId;
  Timeline timeline;
  StartStopState startStopState;
  std::optional<ArrangementTimeline> arrangementTimeline;
};

} // namespace link
} // namespace ableton
