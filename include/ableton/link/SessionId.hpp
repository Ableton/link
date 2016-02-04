// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/NodeId.hpp>

namespace ableton
{
namespace link
{

// SessionIds occupy the same value space as NodeIds and are
// identified by their founding node.
using SessionId = NodeId;

// A payload entry indicating membership in a particular session
struct SessionMembership
{
  enum { key = 'sess' };

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const SessionMembership& sm)
  {
    return discovery::sizeInByteStream(sm.sessionId);
  }

  template <typename It>
  friend It toNetworkByteStream(const SessionMembership& sm, It out)
  {
    return discovery::toNetworkByteStream(sm.sessionId, std::move(out));
  }

  template <typename It>
  static std::pair<SessionMembership, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto idRes = SessionId::fromNetworkByteStream(move(begin), move(end));
    return make_pair(SessionMembership{move(idRes.first)}, move(idRes.second));
  }

  SessionId sessionId;
};

} // namespace link
} // namespace ableton
