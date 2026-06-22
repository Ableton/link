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
                                  const bool bRespectQuantum = false,
                                  const link::Beats arrangementOrigin = link::Beats{0.},
                                  const bool bArrangementSyncEnabled = false,
                                  const bool bArrangementActive = false)
{
  const auto tl =
    link::Timeline{link::Tempo{bpm}, link::Beats{0.}, std::chrono::microseconds{0}};
  const auto arrangement =
    bArrangementActive
      ? link::OptionalApiArrangementTimeline{link::ApiArrangementTimeline{
          arrangementOrigin, std::chrono::microseconds{0}}}
      : link::OptionalApiArrangementTimeline{};
  return LinkSessionState{
    {tl, {}, {}, arrangement}, bRespectQuantum, bArrangementSyncEnabled};
}

} // namespace

TEST_CASE("SessionState")
{
  using namespace std::chrono;
  using namespace ableton::link;

  const auto bArrangementSyncEnabled = GENERATE(false, true);
  const auto arrangementOrigin = Beats{GENERATE(0., 16.5, -123.75)};

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
      const auto expectedPhase = beat - std::floor(beat / quantum) * quantum;
      CHECK(phase == Approx(expectedPhase).margin(1e-6));
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
      const auto tl = link::Timeline{
        link::Tempo{tempo}, link::Beats{12345678.}, std::chrono::microseconds{1234567}};
      auto sessionState =
        LinkSessionState({{tl, {}, {}, {}}, false, bArrangementSyncEnabled});
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

  SECTION("ArrangementSyncReportsRawSessionBeatOffsetByOrigin")
  {
    auto state = makeSessionState(120., false, arrangementOrigin, true, /*active*/ true);
    state.setIsPlaying(true, microseconds{0});
    const auto tl = link::Timeline{link::Tempo{120.}, link::Beats{0.}, microseconds{0}};
    const auto quantum = 4.;
    for (auto us = 0; us < 4000000; us += 250000)
    {
      const auto time = microseconds{us};
      CHECK(state.beatAtTime(time, quantum)
            == Approx((tl.toBeats(time) - arrangementOrigin).floating()).margin(1e-6));
    }
  }

  SECTION("StoppedArrangementKeepsMagnitudeButAlignsPhaseToSession")
  {
    const auto quantum = 4.;
    auto state = makeSessionState(120., false, arrangementOrigin, true, /*active*/ true);
    auto sessionPeer = makeSessionState(120., false, arrangementOrigin, false);
    const auto tl = link::Timeline{link::Tempo{120.}, link::Beats{0.}, microseconds{0}};
    for (auto us = 0; us < 4000000; us += 250000)
    {
      const auto time = microseconds{us};
      const auto arrangementBeat = (tl.toBeats(time) - arrangementOrigin).floating();
      CHECK(std::abs(state.beatAtTime(time, quantum) - arrangementBeat)
            <= quantum / 2. + 1e-6);
      CHECK(state.phaseAtTime(time, quantum)
            == Approx(sessionPeer.phaseAtTime(time, quantum)).margin(1e-6));
    }
  }

  SECTION("StoppedArrangementTimeAtBeatIsInverseOfBeatAtTime")
  {
    const auto quantum = 4.;
    auto state = makeSessionState(120., false, arrangementOrigin, true, /*active*/ true);

    for (auto beat = -16.; beat <= 16.; beat += 0.5)
    {
      const auto time = state.timeAtBeat(beat, quantum);
      CHECK(state.beatAtTime(time, quantum) == Approx(beat).margin(1e-6));
    }
  }

  SECTION("EnablingArrangementSyncWithoutActiveOriginDoesNotMoveTheBeat")
  {
    const auto quantum = 4.;
    auto enabledInactive =
      makeSessionState(120., false, arrangementOrigin, true, /*active*/ false);
    auto disabled = makeSessionState(120., false, arrangementOrigin, false);
    for (auto us = 0; us < 4000000; us += 250000)
    {
      const auto time = microseconds{us};
      CHECK(enabledInactive.beatAtTime(time, quantum)
            == Approx(disabled.beatAtTime(time, quantum)).margin(1e-6));
    }
  }

  SECTION("ArrangementJumpIsIgnoredByReadsWhenSyncDisabled")
  {
    auto state = makeSessionState(120., false, arrangementOrigin, false);
    const auto quantum = 4.;
    const auto time = microseconds{2000000};
    const auto before = state.beatAtTime(time, quantum);
    state.setArrangementBeatAtTime(99., time);
    CHECK(state.beatAtTime(time, quantum) == Approx(before).margin(1e-6));
  }

  SECTION("SetArrangementBeatAtTimeMapsExactlyAndIsNonQuantized")
  {
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    state.setIsPlaying(true, microseconds{0});
    const auto quantum = 4.;

    const auto time = microseconds{5000000};
    state.setArrangementBeatAtTime(33.25, time);
    CHECK(state.beatAtTime(time, quantum) == Approx(33.25).margin(1e-6));

    const auto jumpTime = microseconds{6000000};
    state.setArrangementBeatAtTime(-7.5, jumpTime);
    CHECK(state.beatAtTime(jumpTime, quantum) == Approx(-7.5).margin(1e-6));
  }

  SECTION("ForceRemapsTimelineRegardlessOfArrangementSync")
  {
    const auto quantum = 4.;
    const auto time = microseconds{5000000};
    const auto beat = 37.3;

    // forceBeatAtTime always remaps the local timeline, independent of whether
    // arrangement sync is enabled or whether we are connected to peers (bRespectQuantum).
    const auto plainForce = [&](const bool bRespectQuantum, const bool bArrangementSync)
    {
      auto state =
        makeSessionState(120., bRespectQuantum, arrangementOrigin, bArrangementSync);
      state.forceBeatAtTime(beat, time, quantum);
      return state.beatAtTime(time, quantum);
    };

    const auto reference = plainForce(false, false);
    CHECK(reference == Approx(beat).margin(1e-6));
    CHECK(plainForce(false, true) == Approx(reference).margin(1e-6));
    CHECK(plainForce(true, false) == Approx(reference).margin(1e-6));
    CHECK(plainForce(true, true) == Approx(reference).margin(1e-6));
  }

  SECTION("ForceMovesSharedArrangementBeatForPeers")
  {
    // A force while arrangement sync is enabled moves the shared arrangement beat so that
    // arrangement peers follow to the same position, without restarting transport.
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    const auto quantum = 4.;

    const auto startTime = microseconds{3030000};
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, 0., quantum);
    const auto launch = state.timeForArrangementIsPlaying();

    const auto jumpTime = microseconds{9000000};
    state.forceBeatAtTime(50., jumpTime, quantum);

    CHECK(state.beatAtTime(jumpTime, quantum) == Approx(50.).margin(1e-3));
    CHECK(state.timeForArrangementIsPlaying() == launch);
  }

  SECTION("QuantizedRestartResetsArrangementLaunchTime")
  {
    // A quantized start while a (stopped) arrangement already exists must move the launch
    // time to the new start point, not keep the previous one. setArrangementBeatAtTime
    // preserves the existing launch time (jump semantics), so requestBeatAtTime resets
    // it.
    auto state = makeSessionState(120., true, arrangementOrigin, true, /*active*/ true);
    const auto quantum = 4.;
    REQUIRE(state.timeForArrangementIsPlaying() == microseconds{0});

    const auto startTime = microseconds{3030000};
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, 0., quantum);

    CHECK(state.timeForArrangementIsPlaying() >= startTime);
  }

  SECTION("ArrangementStartIsQuantizedToSessionPhase")
  {
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    const auto beat = 8.;
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, beat, quantum);

    const auto mappedTime = state.timeAtBeat(beat, quantum);
    CHECK(mappedTime >= startTime);
    CHECK(state.beatAtTime(mappedTime, quantum) == Approx(beat).margin(1e-6));
    CHECK(state.phaseAtTime(mappedTime, quantum) == Approx(0.).margin(1e-6));
  }

  SECTION("ArrangementStartBroadcastsRawStartTime")
  {
    // An arrangement sync start quantizes the beat mapping but leaves the start time
    // raw: timeForIsPlaying() is what peers receive, and receivers do their own
    // quantization. Broadcasting a pre-quantized time would make a start-stop follower
    // quantize twice.
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    const auto beat = 8.;
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, beat, quantum);

    CHECK(state.timeForIsPlaying() == startTime);
    CHECK(state.beatAtTime(state.timeForIsPlaying(), quantum) <= Approx(beat));
  }

  SECTION("TimeForArrangementIsTheQuantizedLaunchWhileTimeForIsPlayingIsRaw")
  {
    // timeForIsPlaying() keeps start/stop-sync semantics (the raw start-command time),
    // while timeForArrangementIsPlaying() carries the quantized launch: the shared time
    // at which this peer, and same-quantum arrangement peers, actually begin.
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    const auto beat = 8.;
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, beat, quantum);

    CHECK(state.timeForIsPlaying() == startTime);

    const auto launch = state.timeForArrangementIsPlaying();
    CHECK(launch >= startTime);
    CHECK(launch == state.timeAtBeat(beat, quantum));
    CHECK(state.beatAtTime(launch, quantum) == Approx(beat).margin(1e-6));
    CHECK(state.phaseAtTime(launch, quantum) == Approx(0.).margin(1e-6));
  }

  SECTION("ArrangementJumpPreservesTheLaunchTime")
  {
    // A non-quantized jump only moves the origin; it must not restart transport, so the
    // launch time is preserved.
    auto state = makeSessionState(120., true, arrangementOrigin, true);
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    state.setIsPlayingAndRequestBeatAtTime(true, startTime, 8., quantum);
    const auto launch = state.timeForArrangementIsPlaying();

    state.setArrangementBeatAtTime(40., microseconds{9000000});
    CHECK(state.timeForArrangementIsPlaying() == launch);
  }

  SECTION("StartStopSyncFollowerQuantizesRawStartToOwnQuantum")
  {
    // A (arrangement sync) starts and broadcasts the RAW start time. A start-stop sync
    // follower B receives that raw time and quantizes it to its OWN quantum, starting in
    // phase within one quantum — never double-quantized a full quantum out.
    const auto tempo = 120.;
    const auto beatDurUs = 60. / tempo * 1e6;
    const auto tl = link::Timeline{link::Tempo{tempo}, link::Beats{0.}, microseconds{0}};

    const auto rawStart = microseconds{3030000};
    auto a = LinkSessionState{{tl, {}, {}, {}}, true, true};
    a.setIsPlayingAndRequestBeatAtTime(true, rawStart, 33., 4.);
    const auto broadcast = a.timeForIsPlaying();
    CHECK(broadcast == rawStart);

    // Follower quantums including values different from A's.
    const auto followerQuantum = GENERATE(2., 3., 4., 6.);
    auto b = LinkSessionState{{tl, {}, {true, broadcast}, {}}, true, false};
    b.requestBeatAtStartPlayingTime(0., followerQuantum);

    const auto startB = b.timeAtBeat(0., followerQuantum);
    CHECK(startB >= broadcast);
    CHECK(static_cast<double>((startB - broadcast).count())
          < followerQuantum * beatDurUs);
    CHECK(b.phaseAtTime(startB, followerQuantum) == Approx(0.).margin(1e-6));
  }

  SECTION("FollowedStartAdoptsTheControllerProvidedArrangementOrigin")
  {
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    const auto tl = link::Timeline{link::Tempo{120.}, link::Beats{0.}, microseconds{0}};
    const auto staleOrigin =
      link::ApiArrangementTimeline{link::Beats{1.7}, microseconds{1000000}};
    auto state = LinkSessionState{{tl, {}, {true, startTime}, staleOrigin}, true, true};
    const auto before = state;

    state.requestBeatAtStartPlayingTime(0., quantum);

    CHECK(state == before);
  }

  SECTION("RemoteArrangementStartIsFollowedWithoutReQuantizing")
  {
    const auto quantum = 4.;
    const auto startTime = microseconds{3030000};
    const auto tl = link::Timeline{link::Tempo{120.}, link::Beats{0.}, microseconds{0}};
    const auto anchoredOrigin = link::ApiArrangementTimeline{link::Beats{1.7}, startTime};
    auto state =
      LinkSessionState{{tl, {}, {true, startTime}, anchoredOrigin}, true, true};
    const auto before = state;

    state.requestBeatAtStartPlayingTime(0., quantum);

    CHECK(state == before);
  }
}

TEST_CASE("ToIncomingClientState")
{
  using namespace std::chrono;
  using namespace ableton::link;

  const auto tl = Timeline{Tempo{120.}, Beats{0.}, microseconds{0}};
  const auto origin = Beats{16.};
  const auto launchTime = microseconds{1000};
  const auto arrangement =
    OptionalApiArrangementTimeline{ApiArrangementTimeline{origin, launchTime}};
  const auto commit = microseconds{5000};

  SECTION("StartStopChangeCarriesTheReStampedArrangement")
  {
    const auto playing = ApiState{tl, {}, {true, microseconds{2000}}, arrangement};
    const auto stopped = ApiState{tl, {}, {false, microseconds{2000}}, arrangement};

    const auto incoming =
      ableton::detail::toIncomingClientState(stopped, playing, commit, true);

    REQUIRE(incoming.startStopState);
    CHECK(!incoming.startStopState->isPlaying);
    REQUIRE(incoming.arrangementTimeline);
    CHECK(incoming.arrangementTimeline->beatOrigin == origin);
    CHECK(incoming.arrangementTimeline->launchTime == launchTime);
    CHECK(incoming.arrangementTimeline->timestamp == commit);
  }

  SECTION("ArrangementIsNotResentWithoutAStartStopChange")
  {
    const auto before = ApiState{tl, {}, {true, microseconds{2000}}, arrangement};
    const auto tl2 = Timeline{Tempo{130.}, Beats{0.}, microseconds{0}};
    const auto after = ApiState{tl2, {}, {true, microseconds{2000}}, arrangement};

    const auto incoming =
      ableton::detail::toIncomingClientState(after, before, commit, true);

    CHECK(!incoming.startStopState);
    CHECK(!incoming.arrangementTimeline);
  }

  SECTION("StartStopChangeWithoutArrangementSyncSendsNoArrangement")
  {
    // A start/stop-only peer (arrangement sync disabled) never carries an arrangement.
    const auto playing = ApiState{tl, {}, {true, microseconds{2000}}, {}};
    const auto stopped = ApiState{tl, {}, {false, microseconds{2000}}, {}};

    const auto incoming =
      ableton::detail::toIncomingClientState(stopped, playing, commit, false);

    REQUIRE(incoming.startStopState);
    CHECK(!incoming.arrangementTimeline);
  }

  SECTION("StopWithArrangementSyncButNoArrangementSynthesizesContinuousArrangement")
  {
    // Arrangement sync is enabled but we have no arrangement of our own (e.g. we enabled
    // it after joining an already-playing session). Stopping synthesizes a continuous
    // arrangement (origin 0) so the stop carries one and other arrangement peers follow.
    const auto playing = ApiState{tl, {}, {true, microseconds{2000}}, {}};
    const auto stopped = ApiState{tl, {}, {false, microseconds{2000}}, {}};

    const auto incoming =
      ableton::detail::toIncomingClientState(stopped, playing, commit, true);

    REQUIRE(incoming.startStopState);
    CHECK(!incoming.startStopState->isPlaying);
    REQUIRE(incoming.arrangementTimeline);
    CHECK(incoming.arrangementTimeline->beatOrigin == Beats{0.});
    CHECK(incoming.arrangementTimeline->timestamp == commit);
  }

  SECTION("StartWithArrangementSyncButNoArrangementSendsNoArrangement")
  {
    // Synthesis is for stops only: a start without an arrangement of our own must not
    // force other peers' arrangement position to zero.
    const auto stopped = ApiState{tl, {}, {false, microseconds{2000}}, {}};
    const auto playing = ApiState{tl, {}, {true, microseconds{2000}}, {}};

    const auto incoming =
      ableton::detail::toIncomingClientState(playing, stopped, commit, true);

    REQUIRE(incoming.startStopState);
    CHECK(incoming.startStopState->isPlaying);
    CHECK(!incoming.arrangementTimeline);
  }
}

} // namespace ableton
