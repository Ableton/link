/* Copyright 2017, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/link/ArrangementTimeline.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link/StartStopState.hpp>
#include <ableton/link/Timeline.hpp>
#include <ableton/util/TripleBuffer.hpp>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

namespace ableton
{
namespace link
{

using OptionalTimeline = std::optional<Timeline>;
using OptionalStartStopState = std::optional<StartStopState>;
using OptionalClientStartStopState = std::optional<ClientStartStopState>;
using OptionalArrangementTimeline = std::optional<ArrangementTimeline>;
using OptionalClientArrangementTimeline = std::optional<ClientArrangementTimeline>;
using OptionalApiArrangementTimeline = std::optional<ApiArrangementTimeline>;

struct SessionState
{
  Timeline timeline;
  StartStopState startStopState;
  OptionalArrangementTimeline arrangementTimeline;
  GhostXForm ghostXForm;
};

struct ClientState
{
  friend bool operator==(const ClientState& lhs, const ClientState& rhs)
  {
    return std::tie(lhs.timeline, lhs.startStopState, lhs.arrangementTimeline)
           == std::tie(rhs.timeline, rhs.startStopState, rhs.arrangementTimeline);
  }

  friend bool operator!=(const ClientState& lhs, const ClientState& rhs)
  {
    return !(lhs == rhs);
  }

  Timeline timeline;
  SessionId timelineSessionId;
  ClientStartStopState startStopState;
  OptionalClientArrangementTimeline arrangementTimeline;
  std::int64_t arrangementGenerationStamp{0};
};

struct RtClientState
{
  Timeline timeline;
  SessionId timelineSessionId;
  ClientStartStopState startStopState;
  OptionalClientArrangementTimeline arrangementTimeline;
  std::chrono::microseconds timelineTimestamp;
  std::chrono::microseconds startStopStateTimestamp;
  std::chrono::microseconds arrangementTimelineTimestamp;
  std::int64_t arrangementGenerationStamp{0};
};

struct ControllerClientState
{
  ControllerClientState(ClientState state)
    : mState(state)
    , mRtState(state)
  {
  }

  template <typename Fn>
  void update(Fn fn)
  {
    std::unique_lock<std::mutex> lock(mMutex);
    fn(mState);
    mRtState.write(mState);
  }

  ClientState get() const
  {
    std::unique_lock<std::mutex> lock(mMutex);
    return mState;
  }

  ClientState getRt() const { return mRtState.read(); }

  void invalidateArrangement() { mArrangementGeneration.fetch_add(1); }

  template <typename TState>
  void stampArrangement(TState& state, const ClientArrangementTimeline& atl) const
  {
    state.arrangementTimeline = atl;
    state.arrangementGenerationStamp = mArrangementGeneration;
  }

  static void mirrorArrangement(RtClientState& to, const ClientState& from)
  {
    to.arrangementTimeline = from.arrangementTimeline;
    to.arrangementGenerationStamp = from.arrangementGenerationStamp;
  }

  template <typename TState>
  bool isArrangementValid(const TState& state) const
  {
    return state.arrangementTimeline
           && state.arrangementGenerationStamp == mArrangementGeneration;
  }

  template <typename TState>
  bool isArrangementStale(const TState& state) const
  {
    return state.arrangementTimeline && !isArrangementValid(state);
  }

  template <typename TState>
  ClientState resolve(const TState& state) const
  {
    auto result = ClientState{state.timeline,
                              state.timelineSessionId,
                              state.startStopState,
                              state.arrangementTimeline,
                              state.arrangementGenerationStamp};
    if (result.arrangementTimeline && !isArrangementValid(result))
    {
      result.timeline.beatOrigin =
        result.timeline.beatOrigin - result.arrangementTimeline->beatOrigin;
      result.arrangementTimeline = std::nullopt;
    }
    return result;
  }

private:
  mutable std::mutex mMutex;
  ClientState mState;
  mutable util::TripleBuffer<ClientState> mRtState;
  std::atomic<std::int64_t> mArrangementGeneration{0};
};

struct IncomingClientState
{
  OptionalTimeline timeline;
  OptionalClientStartStopState startStopState;
  OptionalClientArrangementTimeline arrangementTimeline;
  std::chrono::microseconds timelineTimestamp;
};

struct ApiState
{
  friend bool operator==(const ApiState& lhs, const ApiState& rhs)
  {
    return std::tie(lhs.timeline,
                    lhs.timelineSessionId,
                    lhs.startStopState,
                    lhs.arrangementTimeline)
           == std::tie(rhs.timeline,
                       rhs.timelineSessionId,
                       rhs.startStopState,
                       rhs.arrangementTimeline);
  }

  friend bool operator!=(const ApiState& lhs, const ApiState& rhs)
  {
    return !(lhs == rhs);
  }

  Timeline timeline;
  SessionId timelineSessionId;
  ApiStartStopState startStopState;
  OptionalApiArrangementTimeline arrangementTimeline;
};

} // namespace link


namespace detail
{
template <typename SessionState>
const link::ApiState& linkApiState(const SessionState& sessionState)
{
  return sessionState.mState;
}

} // namespace detail

} // namespace ableton
