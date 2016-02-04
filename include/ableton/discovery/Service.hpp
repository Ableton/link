// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/PeerGateways.hpp>
#include <ableton/discovery/asio/AsioTimer.hpp>
// Temporary -- shouldn't depend directly on udp - need to generalize
#include <ableton/discovery/UdpMessenger.hpp>

namespace ableton
{
namespace link
{
namespace discovery
{

template <typename GatewayFactory, typename Platform, typename Log>
struct Service
{
  using PeerGateways = PeerGateways<GatewayFactory, util::AsioService&, Platform, Log>;

  Service(
    GatewayFactory factory,
    util::Injected<Platform> platform,
    util::Injected<Log> log)
    : mIo(ExceptionHandler{*this})
    , mGateways(5u, std::move(factory), util::injectRef(mIo), std::move(platform), log)
    , mLog(std::move(log))
  {
  }

  void enable(const bool bEnable)
  {
    mGateways.enable(bEnable);
  }

  // Asynchronously operate on the current set of peer gateways. The
  // handler will be invoked on a thread managed by the service.
  template <typename Handler>
  void withGatewaysAsync(Handler handler)
  {
    mGateways.withGatewaysAsync(std::move(handler));
  }

private:

  struct ExceptionHandler
  {
    // Depend directly on this Udp-specific exception for now, but
    // this should be generalized.
    using Exception = UdpSendException;

    void operator()(const Exception& exception)
    {
      mService.mGateways.repairGateway(exception.interfaceAddr);
    }

    Service& mService;
  };

  util::AsioService mIo;
  PeerGateways mGateways;
  util::Injected<Log> mLog;
};

} // namespace discovery
} // namespace link
} // namespace ableton
