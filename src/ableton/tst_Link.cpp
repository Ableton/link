/* Copyright 2023, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/Link.hpp>
#include <ableton/platforms/stl/Clock.hpp>
#include <ableton/test/CatchWrapper.hpp>

#include <chrono>
#include <cmath>

namespace ableton
{
namespace
{

using LinkSessionState = ableton::BasicLink<platforms::stl::Clock>::SessionState;

LinkSessionState makeSessionState(const double bpm = 120.,
                                  const bool bRespectQuantum = false)
{
  const auto tl =
    link::Timeline{link::Tempo{bpm}, link::Beats{0.}, std::chrono::microseconds{0}};
  return LinkSessionState{{tl, {}, {}}, bRespectQuantum};
}

} // namespace

TEST_CASE("SessionState")
{
  using namespace std::chrono;
  using namespace ableton::link;

  SECTION("Equality")
  {
    const auto state = makeSessionState();
    CHECK(state == state);
    CHECK_FALSE(state != state);

    auto other = state;
    other.setTempo(140., microseconds{0});
    CHECK(state != other);
    CHECK_FALSE(state == other);
  }

  SECTION("Tempo")
  {
    auto state = makeSessionState(120.);
    CHECK(state.tempo() == Approx(120.));

    const auto atTime = microseconds{1000000};
    const auto quantum = 4.;
    const auto beatBefore = state.beatAtTime(atTime, quantum);
    state.setTempo(60., atTime);

    CHECK(state.tempo() == Approx(60.));
    CHECK(state.beatAtTime(atTime, quantum) == Approx(beatBefore));
  }

  SECTION("BeatAtTimeIsInverseOfTimeAtBeat")
  {
    const auto state = makeSessionState(130.);
    const auto quantum = 4.;
    for (auto beat = 0.; beat < 16.; beat += 0.5)
    {
      const auto time = state.timeAtBeat(beat, quantum);
      CHECK(state.beatAtTime(time, quantum) == Approx(beat).margin(1e-6));
    }
  }

  SECTION("PhaseAtTimeIsInQuantumRange")
  {
    const auto state = makeSessionState(120.);
    const auto quantum = 4.;
    for (auto us = 0; us < 4000000; us += 100000)
    {
      const auto time = microseconds{us};
      const auto phase = state.phaseAtTime(time, quantum);
      CHECK(phase >= 0.);
      CHECK(phase < quantum);
      const auto beat = state.beatAtTime(time, quantum);
      CHECK(phase == Approx(std::fmod(beat, quantum)).margin(1e-6));
    }
  }

  SECTION("RequestBeatAtTimeWithoutQuantizationMapsExactly")
  {
    auto state = makeSessionState(120., false);
    const auto beat = 8.;
    const auto time = microseconds{5000000};
    const auto quantum = 4.;
    state.requestBeatAtTime(beat, time, quantum);
    CHECK(state.beatAtTime(time, quantum) == Approx(beat).margin(1e-6));
  }

  SECTION("RequestBeatAtTimeWithQuantizationMatchesPhase")
  {
    auto state = makeSessionState(120., true);
    const auto beat = 0.;
    const auto time = microseconds{5000000};
    const auto quantum = 4.;
    state.requestBeatAtTime(beat, time, quantum);
    CHECK(state.phaseAtTime(state.timeAtBeat(beat, quantum), quantum)
          == Approx(beat).margin(1e-6));
  }

  SECTION("ForceBeatTime")
  {
    const auto beats = 0.;
    const auto time = microseconds{23456789};
    const auto quantum = 4.;

    for (auto tempo = 20.; tempo < 999.; tempo += 0.8)
    {
      const auto tl = Timeline{Tempo{tempo}, Beats{12345678.}, microseconds{1234567}};
      auto sessionState = LinkSessionState({{tl, {}, {}}, false});
      sessionState.forceBeatAtTime(beats, time, quantum);

      CHECK(beats >= sessionState.beatAtTime(time, quantum));
    }
  }

  SECTION("IsPlaying")
  {
    auto state = makeSessionState();
    CHECK_FALSE(state.isPlaying());

    const auto time = microseconds{777000};
    state.setIsPlaying(true, time);
    CHECK(state.isPlaying());
    CHECK(state.timeForIsPlaying() == time);

    state.setIsPlaying(false, microseconds{888000});
    CHECK_FALSE(state.isPlaying());
    CHECK(state.timeForIsPlaying() == microseconds{888000});
  }

  SECTION("RequestBeatAtStartPlayingTimeIsNoOpWhenStopped")
  {
    auto state = makeSessionState();
    const auto quantum = 4.;
    const auto before = state;
    state.requestBeatAtStartPlayingTime(2., quantum);
    CHECK(state == before);
  }

  SECTION("RequestBeatAtStartPlayingTimeMapsWhenPlaying")
  {
    auto state = makeSessionState(120., false);
    const auto quantum = 4.;
    const auto startTime = microseconds{3000000};
    state.setIsPlaying(true, startTime);
    state.requestBeatAtStartPlayingTime(7., quantum);
    CHECK(state.beatAtTime(startTime, quantum) == Approx(7.).margin(1e-6));
  }

  SECTION("SetIsPlayingAndRequestBeatAtTime")
  {
    auto state = makeSessionState(120., false);
    const auto quantum = 4.;
    const auto startTime = microseconds{2500000};
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, 14., quantum);

    CHECK(state.isPlaying());
    CHECK(state.timeForIsPlaying() == startTime);
    CHECK(state.beatAtTime(startTime, quantum) == Approx(14.).margin(1e-6));
  }
}

} // namespace ableton
