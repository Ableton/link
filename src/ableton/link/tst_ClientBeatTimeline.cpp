// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/ClientBeatTimeline.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{
namespace
{

using std::chrono::microseconds;

const auto timeline1 = Timeline{Tempo{60.}, Beats{-1.}, microseconds{1500000}};
const auto timeline2 = Timeline{Tempo{30.}, Beats{2.}, microseconds{-2000000}};

const auto xform1 = GhostXForm{1.0, microseconds{10000000}};
const auto xform2 = GhostXForm{2.0, microseconds{-1000000}};

} // unnamed

TEST_CASE("ClientBeatTimeline | HostBeatRoundtrips", "[ClientBeatTimeline]")
{
  ClientBeatTimeline cbtl{timeline1, xform1};
  const auto testBeats = Beats{5.5};
  const auto testHost = microseconds{123456789};
  CHECK(testBeats == hostToBeats(cbtl, (beatsToHost(cbtl, testBeats))));
  CHECK(testHost == beatsToHost(cbtl, (hostToBeats(cbtl, testHost))));
}

TEST_CASE("ClientBeatTimeline | ResetBeats", "[ClientBeatTimeline]")
{
  ClientBeatTimeline cbtl{timeline1, xform1};
  // Request to remap beat 6.2 onto host time 13.
  CHECK(Beats{4.5} == resetBeats(cbtl, Beats{6.2}, microseconds{5000000}, Beats{4.}));
  // There's now a client offset in play, test that it's respected
  CHECK(Beats{6.2} == hostToBeats(cbtl, microseconds{6700000}));
  CHECK(microseconds{6700000} == beatsToHost(cbtl, Beats{6.2}));
}

TEST_CASE("ClientBeatTimeline | Phase", "[ClientBeatTimeline]")
{
  ClientBeatTimeline cbtl{timeline1, xform1};
  CHECK(Beats{0.5} == resetBeats(cbtl, Beats{1.}, microseconds{5000000}, Beats{1.}));
  // Beat 1 is now mapped to 13 on the underlying timeline
  CHECK(Beats{0.} == phase(cbtl, Beats{1.}, Beats{0.}));
  CHECK(Beats{0.} == phase(cbtl, Beats{1.}, Beats{1.}));
  CHECK(Beats{1.} == phase(cbtl, Beats{1.}, Beats{2.}));
  CHECK(Beats{1.} == phase(cbtl, Beats{1.}, Beats{3.}));
  CHECK(Beats{1.} == phase(cbtl, Beats{1.}, Beats{4.}));
  CHECK(Beats{3.} == phase(cbtl, Beats{1.}, Beats{5.}));
  CHECK(Beats{1.} == phase(cbtl, Beats{1.}, Beats{6.}));
  CHECK(Beats{6.} == phase(cbtl, Beats{1.}, Beats{7.}));
  CHECK(Beats{5.} == phase(cbtl, Beats{1.}, Beats{8.}));
}

TEST_CASE("ClientBeatTimeline | UpdateSession", "[ClientBeatTimeline]")
{
  ClientBeatTimeline cbtl{timeline1, xform1};
  // Request to remap beat 6.2 onto host time 3 in order to introduce
  // an offset
  const auto quantum = Beats{4.};
  resetBeats(cbtl, Beats{6.2}, microseconds{5000000}, quantum);
  const auto updateSessionTime = microseconds{7000000};
  const auto beatReference = hostToBeats(cbtl, updateSessionTime);
  // Now update the session and validate that the client's timeline
  // has not been adjusted by more than it should
  updateSession(cbtl, timeline2, xform2, updateSessionTime, quantum);
  CHECK(quantum > hostToBeats(cbtl, updateSessionTime) - beatReference);
}

TEST_CASE("ClientBeatTimeline | ForceBeats", "[ClientBeatTimeline]")
{
  ClientBeatTimeline cbtl{timeline1, xform1};
  const auto quantum = Beats{4.};
  // Remap 2 onto session beat time 14 to introduce an offset
  resetBeats(cbtl, Beats{2.}, microseconds{5000000}, quantum);
  // Now force a mapping onto beat 5 at the same host time value and
  // verify that this maps to 13 (not 17, it should take the closest
  // phase matched value)
  forceBeats(cbtl, Tempo{120.}, Beats{5.}, microseconds{5000000}, quantum);
  CHECK(Beats{5.} == hostToBeats(cbtl, microseconds{5000000}));
  CHECK(Beats{5.} == phase(cbtl, Beats{5.}, Beats{8.}));
  // Now force 0, which should map to 12
  forceBeats(cbtl, Tempo{240.}, Beats{0.}, microseconds{5000000}, quantum);
  CHECK(Beats{0.} == hostToBeats(cbtl, microseconds{5000000}));
  CHECK(Beats{12.} == phase(cbtl, Beats{0.}, Beats{13.}));
}

} // namespace link
} // namespace ableton
