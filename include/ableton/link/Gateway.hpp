// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/MeasurementService.hpp>
#include <ableton/discovery/PeerGateway.hpp>

namespace ableton
{
namespace link
{

template <
  typename PeerObserver,
  typename StateQuery,
  typename GhostXFormQuery,
  typename Clock,
  typename Log>
struct Gateway
{
  Gateway(
    util::AsioService& io,
    asio::ip::address_v4 addr,
    PeerObserver observer,
    StateQuery stateQuery,
    GhostXFormQuery ghostXFormQuery,
    Clock clock,
    Log log)
    : mMeasurement(addr, stateQuery, std::move(ghostXFormQuery), std::move(clock), log)
    , mPeerGateway(
        discovery::makeIpV4Gateway(
          io, std::move(addr), std::move(observer),
          PeerStateQuery{{endpoint(mMeasurement)}, std::move(stateQuery)},
          std::move(log)))
  {
  }

  Gateway(const Gateway& rhs) = delete;
  Gateway& operator=(const Gateway& rhs) = delete;

  Gateway(Gateway&& rhs)
    : mMeasurement(std::move(rhs.mMeasurement))
    , mPeerGateway(std::move(rhs.mPeerGateway))
  {
  }

  Gateway& operator=(Gateway&& rhs)
  {
    mMeasurement = std::move(rhs.mMeasurement);
    mPeerGateway = std::move(rhs.mPeerGateway);
    return *this;
  }

  friend void broadcastState(Gateway& gateway)
  {
    broadcastState(gateway.mPeerGateway);
  }

  template <typename Handler>
  friend void measurePeer(Gateway& gateway, const PeerState& peer, Handler handler)
  {
    gateway.mMeasurement.measurePeer(peer, std::move(handler));
  }

private:

  struct PeerStateQuery
  {
    PeerState operator()()
    {
      return {mQuery(), mEndpoint};
    }

    asio::ip::udp::endpoint mEndpoint;
    StateQuery mQuery;
  };

  MeasurementService<StateQuery, GhostXFormQuery, Clock, Log> mMeasurement;
  discovery::IpV4Gateway<PeerObserver, PeerStateQuery, Log> mPeerGateway;
};

} // namespace link
} // namespace ableton
