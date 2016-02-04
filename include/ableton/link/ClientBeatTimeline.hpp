// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/Beats.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/Timeline.hpp>
#include <atomic>
#include <chrono>

namespace ableton
{
namespace link
{

struct ClientBeatTimeline
{
  ClientBeatTimeline(Timeline tl, GhostXForm xform)
    : mClientOffset(INT64_C(0))
    , mTimeline(std::move(tl))
    , mXForm(std::move(xform))
  {
    mLock.clear();
  }

  friend Tempo tempo(const ClientBeatTimeline& clientTl)
  {
    SpinLock lock(clientTl);
    return clientTl.mTimeline.tempo;
  }

  friend Beats hostToBeats(
    const ClientBeatTimeline& clientTl,
    const std::chrono::microseconds host)
  {
    SpinLock lock(clientTl);
    return clientTl.hostToSessionBeats(host) + clientTl.mClientOffset;
  }

  friend std::chrono::microseconds beatsToHost(
    const ClientBeatTimeline& clientTl,
    const Beats beats)
  {
    SpinLock lock(clientTl);
    return ghostToHost(
      clientTl.mXForm, fromBeats(clientTl.mTimeline, beats - clientTl.mClientOffset));
  }

  friend Beats resetBeats(
    ClientBeatTimeline& clientTl,
    const Beats beats,
    const std::chrono::microseconds host,
    const Beats quantum)
  {
    SpinLock lock(clientTl);
    const auto curBeats = clientTl.hostToSessionBeats(host);
    const auto phaseMatchedBeats = clientTl.phaseMatch(curBeats, beats, quantum);
    clientTl.mClientOffset = beats - phaseMatchedBeats;
    return beats - (phaseMatchedBeats - curBeats);
  }

  friend Timeline forceBeats(
    ClientBeatTimeline& clientTl,
    const Tempo tempo,
    const Beats beats,
    const std::chrono::microseconds host,
    const Beats quantum)
  {
    SpinLock lock(clientTl);
    const auto ghost = hostToGhost(clientTl.mXForm, host);
    const auto curBeats = toBeats(clientTl.mTimeline, ghost);
    const auto phaseMatchedBeats = clientTl.phaseMatch(
      curBeats - Beats{0.5 * floating(quantum)}, beats, quantum);
    clientTl.mClientOffset = beats - phaseMatchedBeats;
    // Create the timeline representing the given arguments
    auto newTl = Timeline{tempo, phaseMatchedBeats, ghost};
    // and adjust as necessary to make curBeats the beat origin
    newTl.timeOrigin = fromBeats(newTl, curBeats);
    newTl.beatOrigin = curBeats;
    clientTl.mTimeline = newTl;
    return newTl;
  }

  friend Beats phase(
    ClientBeatTimeline& clientTl,
    const Beats beats,
    const Beats quantum)
  {
    if (quantum == Beats{INT64_C(0)})
    {
      return Beats{INT64_C(0)};
    }
    else
    {
      SpinLock lock(clientTl);
      return clientTl.phase(beats - clientTl.mClientOffset, quantum);
    }
  }

  // Update the tempo, returning a new version of the timeline with the
  // new tempo incorporated.
  friend Timeline updateTempo(
    ClientBeatTimeline& clientTl,
    const Tempo newTempo,
    const std::chrono::microseconds atHostTime)
  {
    SpinLock lock(clientTl);
    const auto ghostTime = hostToGhost(clientTl.mXForm, atHostTime);
    clientTl.mTimeline = {newTempo, toBeats(clientTl.mTimeline, ghostTime), ghostTime};
    return clientTl.mTimeline;
  }

  friend void updateTimeline(ClientBeatTimeline& clientTl, const Timeline tl)
  {
    SpinLock lock(clientTl);
    clientTl.mTimeline = tl;
  }

  friend Timeline updateOrigin(
    ClientBeatTimeline& clientTl,
    const Beats beats,
    const std::chrono::microseconds host)
  {
    SpinLock lock(clientTl);
    clientTl.mClientOffset = Beats{INT64_C(0)};
    clientTl.mTimeline.beatOrigin = beats;
    clientTl.mTimeline.timeOrigin = hostToGhost(clientTl.mXForm, host);
    return clientTl.mTimeline;
  }

  friend Timeline reset(
    ClientBeatTimeline& clientTl,
    const std::chrono::microseconds host,
    const GhostXForm xform)
  {
    SpinLock lock(clientTl);
    clientTl.mTimeline.beatOrigin =
      clientTl.hostToSessionBeats(host) + clientTl.mClientOffset;
    clientTl.mClientOffset = Beats{INT64_C(0)};
    clientTl.mXForm = xform;
    clientTl.mTimeline.timeOrigin = hostToGhost(xform, host);
    return clientTl.mTimeline;
  }

  // Switch to a new session defined by the given (timeline, xform)
  // pair. The switch should be considered to have happened at the
  // given host time and should be phase matched according to the
  // given quantum. The client's beat time value will jump forward by
  // at most a quantum as a result of this call.
  friend void updateSession(
    ClientBeatTimeline& clientTl,
    const Timeline tl,
    const GhostXForm xform,
    const std::chrono::microseconds atHostTime,
    const Beats quantum)
  {
    SpinLock lock(clientTl);
    const auto oldBeats = clientTl.hostToSessionBeats(atHostTime);
    clientTl.mTimeline = tl;
    clientTl.mXForm = xform;
    const auto newBeats = clientTl.hostToSessionBeats(atHostTime);
    // phase match to the nearest quantum when joining a session (not
    // always the next one). This may move backwards by at most half a
    // quantum. This ensures that we have to make the minimal beat
    // time jump to match the session.
    clientTl.mClientOffset =
      clientTl.phaseMatch(oldBeats - Beats{0.5 * floating(quantum)}, newBeats, quantum) +
      clientTl.mClientOffset - newBeats;
  }

private:

  Beats hostToSessionBeats(const std::chrono::microseconds host) const
  {
    return toBeats(mTimeline, hostToGhost(mXForm, host));
  }

  // Return the least value greater than x that matches the phase of
  // target with respect to the given quantum. If the given quantum
  // quantum is not greater than 0, x is returned.
  Beats phaseMatch(const Beats x, const Beats target, const Beats quantum) const
  {
    if (quantum > Beats{INT64_C(0)})
    {
      const auto desiredPhase = phase(target, quantum);
      const auto xPhase = phase(x, quantum);
      const auto phaseDiff = (desiredPhase - xPhase + quantum) % quantum;
      return x + phaseDiff;
    }
    else
    {
      return x;
    }
  }

  Beats phase(const Beats x, const Beats quantum) const
  {
    // Handle negative values by doing the computation relative to an
    // origin that is on the nearest quantum boundary less than -(abs(x))
    const auto quantumMicros = microBeats(quantum);
    const auto quantumBins = (llabs(microBeats(x)) + quantumMicros) / quantumMicros;
    const std::int64_t quantumBeats{quantumBins * quantumMicros};
    return (x + Beats{quantumBeats}) % quantum;
  }

  struct SpinLock
  {
    SpinLock(const ClientBeatTimeline& cbtl)
      : mCbtl(cbtl)
    {
      while(mCbtl.mLock.test_and_set());
    }

    ~SpinLock()
    {
      mCbtl.mLock.clear();
    }

    const ClientBeatTimeline& mCbtl;
  };

  Beats mClientOffset;
  Timeline mTimeline;
  GhostXForm mXForm;
  mutable std::atomic_flag mLock;
};

} // namespace link
} // namespace ableton
