// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link/Timeline.hpp>

namespace ableton
{
namespace link
{

struct SessionMeasurement
{
  GhostXForm xform;
  std::chrono::microseconds timestamp;
};

struct Session
{
  SessionId sessionId;
  Timeline timeline;
  SessionMeasurement measurement;
};

template <
  typename Peers,
  typename MeasurePeer,
  typename JoinSessionCallback,
  typename Io,
  typename Clock,
  typename Log>
struct Sessions
{
  using Timer = typename util::Injected<Io>::type::Timer;

  Sessions(
    Session init,
    Peers peers,
    MeasurePeer measure,
    JoinSessionCallback join,
    util::Injected<Io> io,
    Clock clock,
    util::Injected<Log> log)
    : mPeers(std::move(peers))
    , mMeasure(std::move(measure))
    , mCallback(std::move(join))
    , mCurrent(std::move(init))
    , mIo(std::move(io))
    , mTimer(mIo->makeTimer())
    , mClock(std::move(clock))
    , mLog(std::move(log))
  {
  }

  void resetSession(Session session)
  {
    mCurrent = std::move(session);
    mOtherSessions.clear();
  }

  void resetTimeline(Timeline timeline)
  {
    mCurrent.timeline = std::move(timeline);
  }

  // Consider the observed session/timeline pair and return a possibly
  // new timeline that should be used going forward.
  Timeline sawSessionTimeline(SessionId sid, Timeline timeline)
  {
    using namespace std;
    if (sid == mCurrent.sessionId)
    {
      // matches our current session, update the timeline if necessary
      updateTimeline(mCurrent, move(timeline));
    }
    else
    {
      auto session = Session{move(sid), move(timeline), {}};
      const auto range = equal_range(
        begin(mOtherSessions), end(mOtherSessions), session, SessionIdComp{});
      if (range.first == range.second)
      {
        // brand new session, insert it into our list of known
        // sessions and launch a measurement
        launchSessionMeasurement(session);
        mOtherSessions.insert(range.first, move(session));
      }
      else
      {
        // we've seen this session before, update its timeline if necessary
        updateTimeline(*range.first, move(timeline));
      }
    }
    return mCurrent.timeline;
  }

private:

  void launchSessionMeasurement(Session& session)
  {
    using namespace std;
    auto peers = sessionPeers(mPeers, session.sessionId);
    if (!peers.empty())
    {
      // first criteria: always prefer the founding peer
      const auto it = find_if(begin(peers), end(peers), [&session](const Peer& peer) {
          return session.sessionId == ident(peer.first);
        });
      // TODO: second criteria should be degree. We don't have that
      // represented yet so just use the first peer for now
      auto peer = it == end(peers) ? peers.front() : *it;
      // mark that a session is in progress by clearing out the
      // session's timestamp
      session.measurement.timestamp = {};
      mMeasure(move(peer), MeasurementResultsHandler{*this, session.sessionId});
    }
  }

  void handleSuccessfulMeasurement(const SessionId& id, GhostXForm xform)
  {
    using namespace std;

    debug(*mLog) << "Session " << id << " measurement completed with result " <<
      "(" << xform.slope << ", " << xform.intercept.count() << ")";

    auto measurement = SessionMeasurement{move(xform), micros(mClock)};

    if (mCurrent.sessionId == id)
    {
      mCurrent.measurement = move(measurement);
      mCallback(mCurrent);
    }
    else
    {
      const auto range = equal_range(
        begin(mOtherSessions), end(mOtherSessions),
        Session{id, {}, {}}, SessionIdComp{});

      if (range.first != range.second)
      {
        const auto SESSION_EPS = chrono::microseconds{500000};
        // should we join this session?
        const auto hostTime = micros(mClock);
        const auto curGhost = hostToGhost(mCurrent.measurement.xform, hostTime);
        const auto newGhost = hostToGhost(measurement.xform, hostTime);
        // update the measurement for the session entry
        range.first->measurement = move(measurement);
        // If session times too close - fall back to session id order
        const auto ghostDiff = newGhost - curGhost;
        if (ghostDiff > SESSION_EPS ||
            (abs(ghostDiff.count()) < SESSION_EPS.count() && id < mCurrent.sessionId))
        {
          // The new session wins, switch over to it
          auto current = mCurrent;
          mCurrent = move(*range.first);
          mOtherSessions.erase(range.first);
          // Put the old current session back into our list of known
          // sessions so that we won't re-measure it
          const auto it = upper_bound(
            begin(mOtherSessions), end(mOtherSessions), current, SessionIdComp{});
          mOtherSessions.insert(it, move(current));
          // And notify that we have a new session and make sure that
          // we remeasure it periodically.
          mCallback(mCurrent);
          scheduleRemeasurement();
        }
      }
    }
  }

  void scheduleRemeasurement()
  {
    // set a timer to re-measure the active session after a period
    mTimer.expires_from_now(std::chrono::microseconds{30000000});
    mTimer.async_wait([this](const typename Timer::ErrorCode e) {
        if (!e)
        {
          launchSessionMeasurement(mCurrent);
          scheduleRemeasurement();
        }
      });
  }

  void handleFailedMeasurement(const SessionId& id)
  {
    using namespace std;

    debug(*mLog) << "Session " << id << " measurement failed.";

    // if we failed to measure for our current session, schedule a
    // retry in the future. Otherwise, remove the session from our set
    // of known sessions (if it is seen again it will be measured as
    // if new).
    if (mCurrent.sessionId == id)
    {
      scheduleRemeasurement();
    }
    else
    {
      const auto range = equal_range(
        begin(mOtherSessions), end(mOtherSessions), Session{id, {}, {}}, SessionIdComp{});
      if (range.first != range.second)
      {
        mOtherSessions.erase(range.first);
        forgetSession(mPeers, id);
      }
    }
  }

  void updateTimeline(Session& session, Timeline timeline)
  {
    // We use beat origin magnitude as prioritization between sessions.
    if (timeline.beatOrigin > session.timeline.beatOrigin)
    {
      debug(*mLog) << "Adopting peer timeline (" << bpm(timeline.tempo) <<
        ", " << floating(timeline.beatOrigin) <<
        ", " << timeline.timeOrigin.count() << ")";

      session.timeline = std::move(timeline);
    }
    else
    {
      debug(*mLog) << "Rejecting peer timeline with beat origin: " <<
        floating(timeline.beatOrigin) << ". Current timeline beat origin: " <<
        floating(session.timeline.beatOrigin);
    }
  }

  struct MeasurementResultsHandler
  {
    void operator()(GhostXForm xform) const
    {
      Sessions& sessions = mSessions;
      const SessionId& sessionId = mSessionId;
      if (xform == GhostXForm{})
      {
        mSessions.mIo->post([&sessions, sessionId] {
            sessions.handleFailedMeasurement(std::move(sessionId));
          });
      }
      else
      {
        mSessions.mIo->post([&sessions, sessionId, xform] {
            sessions.handleSuccessfulMeasurement(std::move(sessionId), std::move(xform));
          });
      }
    }

    Sessions& mSessions;
    SessionId mSessionId;
  };

  struct SessionIdComp
  {
    bool operator()(const Session& lhs, const Session& rhs) const
    {
      return lhs.sessionId < rhs.sessionId;
    }
  };

  using PeersT = typename util::unwrap<Peers>::type;
  using Peer = typename PeersT::Peer;
  Peers mPeers;
  MeasurePeer mMeasure;
  JoinSessionCallback mCallback;
  Session mCurrent;
  util::Injected<Io> mIo;
  Timer mTimer;
  Clock mClock;
  util::Injected<Log> mLog;
  std::vector<Session> mOtherSessions; // sorted/unique by session id
};

template <
  typename Peers,
  typename MeasurePeer,
  typename JoinSessionCallback,
  typename Io,
  typename Clock,
  typename Log>
Sessions<Peers, MeasurePeer, JoinSessionCallback, Io, Clock, Log> makeSessions(
  Session init,
  Peers peers,
  MeasurePeer measure,
  JoinSessionCallback join,
  util::Injected<Io> io,
  Clock clock,
  Log log)
{
  using namespace std;
  return {
    move(init), move(peers), move(measure), move(join), move(io), move(clock), move(log)
  };
}

} // namespace link
} // namespace ableton
