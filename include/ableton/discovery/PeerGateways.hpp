// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/InterfaceScanner.hpp>
#include <map>

namespace ableton
{
namespace link
{
namespace discovery
{

// GatewayFactory must have an operator()(IoRef, asio::ip::address)
// that constructs a new PeerGateway on a given interface address.
template <typename GatewayFactory, typename Io, typename Platform, typename Log>
struct PeerGateways
{
  using IoType = typename util::Injected<Io>::type;
  using LogType = typename util::Injected<Log>::type;
  using Gateway = typename std::result_of<
    GatewayFactory(IoType&, asio::ip::address)>::type;
  using GatewayMap = std::map<asio::ip::address, Gateway>;

  PeerGateways(
    const unsigned rescanPeriod,
    GatewayFactory factory,
    util::Injected<Io> io,
    util::Injected<Platform> platform,
    util::Injected<Log> log)
    : mIo(std::move(io))
    , mLog(std::move(log))
    , mpScannerCallback(std::make_shared<Callback>(std::move(factory), *mIo, *mLog))
    , mpScanner(std::make_shared<Scanner>(
        rescanPeriod, util::injectShared(mpScannerCallback), std::move(platform),
        util::injectVal(mIo->makeTimer()), mLog))
  {
  }

  ~PeerGateways()
  {
    // Release the callback in the io thread so that gateway cleanup
    // doesn't happen in the client thread
    mIo->post(Deleter{*this});
  }

  PeerGateways(const PeerGateways&) = delete;
  PeerGateways& operator=(const PeerGateways&) = delete;

  PeerGateways(PeerGateways&&) = delete;
  PeerGateways& operator=(PeerGateways&&) = delete;

  void enable(const bool bEnable)
  {
    auto pCallback = mpScannerCallback;
    auto pScanner = mpScanner;

    mIo->post([pCallback, pScanner, bEnable] {
        pCallback->mGateways.clear();
        pScanner->enable(bEnable);
      });
  }

  template <typename Handler>
  void withGatewaysAsync(Handler handler)
  {
    auto pCallback = mpScannerCallback;
    mIo->post([pCallback, handler] {
        handler(pCallback->mGateways.begin(), pCallback->mGateways.end());
      });
  }

  // If a gateway has become non-responsive or is throwing exceptions,
  // this method can be invoked to either fix it or discard it.
  void repairGateway(const asio::ip::address& gatewayAddr)
  {
    auto pCallback = mpScannerCallback;
    auto pScanner = mpScanner;
    mIo->post([pCallback, pScanner, gatewayAddr] {
        if (pCallback->mGateways.erase(gatewayAddr))
        {
          // If we erased a gateway, rescan again immediately so that
          // we will re-initialize it if it's still present
          pScanner->scan();
        }
      });
  }

private:

  struct Callback
  {
    Callback(GatewayFactory factory, IoType& io, LogType& log)
      : mFactory(std::move(factory))
      , mIo(io)
      , mLog(log)
    {
    }

    template <typename AddrRange>
    void operator()(const AddrRange& range)
    {
      using namespace std;
      // Get the set of current addresses.
      vector<asio::ip::address> curAddrs;
      curAddrs.reserve(mGateways.size());
      transform(
        std::begin(mGateways), std::end(mGateways), back_inserter(curAddrs),
        [](const typename GatewayMap::value_type& vt) {
         return vt.first;
        });

      // Now use set_difference to determine the set of addresses that
      // are new and the set of cur addresses that are no longer there
      vector<asio::ip::address> newAddrs;
      set_difference(
        std::begin(range), std::end(range),
        std::begin(curAddrs), std::end(curAddrs),
        back_inserter(newAddrs));

      vector<asio::ip::address> staleAddrs;
      set_difference(
        std::begin(curAddrs), std::end(curAddrs),
        std::begin(range), std::end(range),
        back_inserter(staleAddrs));

      // Remove the stale addresses
      for (const auto& addr : staleAddrs)
      {
        mGateways.erase(addr);
      }

      // Add the new addresses
      for (const auto& addr : newAddrs)
      {
        try
        {
          // Only handle v4 for now
          if (addr.is_v4())
          {
            info(mLog) << "initializing peer gateway on interface " << addr;
            mGateways.emplace(addr, mFactory(mIo, addr.to_v4()));
          }
        }
        catch (const runtime_error& e)
        {
          warning(mLog) << "failed to init gateway on interface " << addr <<
            " reason: " << e.what();
        }
      }
    }

    GatewayFactory mFactory;
    IoType& mIo;
    LogType& mLog;
    GatewayMap mGateways;
  };

  using Scanner = InterfaceScanner<
    std::shared_ptr<Callback>, Platform, typename IoType::Timer, Log>;

  struct Deleter
  {
    Deleter(PeerGateways& gateways)
      : mpScannerCallback(std::move(gateways.mpScannerCallback))
      , mpScanner(std::move(gateways.mpScanner))
    {
    }

    void operator()()
    {
      mpScanner.reset();
      mpScannerCallback.reset();
    }

    std::shared_ptr<Callback> mpScannerCallback;
    std::shared_ptr<Scanner> mpScanner;
  };

  util::Injected<Io> mIo;
  util::Injected<Log> mLog;
  std::shared_ptr<Callback> mpScannerCallback;
  std::shared_ptr<Scanner> mpScanner;
};

// Factory function
template <typename GatewayFactory, typename Io, typename Platform, typename Log>
std::unique_ptr<PeerGateways<GatewayFactory, Io, Platform, Log>> makePeerGateways(
  const unsigned rescanPeriod,
  GatewayFactory factory,
  util::Injected<Io> io,
  util::Injected<Platform> platform,
  util::Injected<Log> log)
{
  using std::move;
  using Gateways = PeerGateways<GatewayFactory, Io, Platform, Log>;
  return std::unique_ptr<Gateways>{new Gateways{
      rescanPeriod, move(factory), move(io), move(platform), move(log)}};
}

} // namespace discovery
} // namespace link
} // namespace ableton
