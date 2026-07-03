/* Copyright 2026, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/link/SessionState.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{

namespace
{

ControllerClientState makeController(Beats beatOrigin)
{
  const auto state =
    ClientState{Timeline{Tempo{120.}, beatOrigin, std::chrono::microseconds{0}},
                SessionId{},
                ClientStartStopState{},
                std::nullopt,
                0};
  return ControllerClientState{state};
}

} // namespace

TEST_CASE("ControllerClientState | ResolvePreservesStateWhenArrangementValid")
{
  auto ccs = makeController(Beats{0.});
  const auto atl = ClientArrangementTimeline{
    Beats{4.}, std::chrono::microseconds{0}, std::chrono::microseconds{0}};
  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });

  const auto state = ccs.get();
  REQUIRE(state.arrangementTimeline);

  const auto resolved = ccs.resolve(state);
  CHECK(resolved.arrangementTimeline);
  CHECK(resolved.timeline.beatOrigin == state.timeline.beatOrigin);
}

TEST_CASE("ControllerClientState | ResolveDropsStaleArrangementAndAdjustsBeatOrigin")
{
  auto ccs = makeController(Beats{0.});
  const auto atlBeatOrigin = Beats{4.};
  const auto atl = ClientArrangementTimeline{
    atlBeatOrigin, std::chrono::microseconds{0}, std::chrono::microseconds{0}};

  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });

  const auto timelineBeatOrigin = Beats{10.};
  ccs.update([&](ClientState& s) { s.timeline.beatOrigin = timelineBeatOrigin; });

  ccs.invalidateArrangement();

  const auto state = ccs.get();
  REQUIRE(state.arrangementTimeline);
  CHECK(ccs.isArrangementStale(state));

  const auto resolved = ccs.resolve(state);
  CHECK(!resolved.arrangementTimeline);
  CHECK(resolved.timeline.beatOrigin == timelineBeatOrigin - atlBeatOrigin);
}

TEST_CASE("ControllerClientState | ResolvePassesThroughStateWithNoArrangement")
{
  auto ccs = makeController(Beats{7.});
  const auto state = ccs.get();
  CHECK(!state.arrangementTimeline);

  const auto resolved = ccs.resolve(state);
  CHECK(!resolved.arrangementTimeline);
  CHECK(resolved.timeline.beatOrigin == Beats{7.});
}

TEST_CASE("ControllerClientState | IsArrangementValidAfterStamp")
{
  auto ccs = makeController(Beats{0.});
  const auto atl = ClientArrangementTimeline{
    Beats{2.}, std::chrono::microseconds{0}, std::chrono::microseconds{0}};
  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });

  const auto state = ccs.get();
  CHECK(ccs.isArrangementValid(state));
  CHECK(!ccs.isArrangementStale(state));
}

TEST_CASE("ControllerClientState | IsArrangementStaleAfterInvalidate")
{
  auto ccs = makeController(Beats{0.});
  const auto atl = ClientArrangementTimeline{
    Beats{2.}, std::chrono::microseconds{0}, std::chrono::microseconds{0}};
  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });
  ccs.invalidateArrangement();

  const auto state = ccs.get();
  CHECK(!ccs.isArrangementValid(state));
  CHECK(ccs.isArrangementStale(state));
}

TEST_CASE("ControllerClientState | IsArrangementValidFalseWithNoArrangement")
{
  auto ccs = makeController(Beats{0.});
  const auto state = ccs.get();
  CHECK(!state.arrangementTimeline);
  CHECK(!ccs.isArrangementValid(state));
  CHECK(!ccs.isArrangementStale(state));
}

TEST_CASE("ControllerClientState | MultipleInvalidationsAccumulate")
{
  auto ccs = makeController(Beats{0.});
  const auto atl = ClientArrangementTimeline{
    Beats{3.}, std::chrono::microseconds{0}, std::chrono::microseconds{0}};
  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });
  ccs.invalidateArrangement();
  ccs.invalidateArrangement();

  const auto state = ccs.get();
  CHECK(ccs.isArrangementStale(state));

  ccs.update([&](ClientState& s) { ccs.stampArrangement(s, atl); });
  CHECK(ccs.isArrangementValid(ccs.get()));
}

} // namespace link
} // namespace ableton
