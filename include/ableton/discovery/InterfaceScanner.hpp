// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/util/Injected.hpp>
#include <ableton/discovery/util/Unwrap.hpp>
#include <chrono>
#include <vector>

namespace ableton
{
namespace link
{

// Callback takes a range of asio::ip:address which is
// guaranteed to be sorted and unique
template <typename Callback, typename Platform, typename Timer, typename Log>
struct InterfaceScanner
{
  InterfaceScanner(
    const unsigned period,
    util::Injected<Callback> callback,
    util::Injected<Platform> platform,
    util::Injected<Timer> timer,
    util::Injected<Log> log)
    : mPeriod(period)
    , mCallback(std::move(callback))
    , mPlatform(std::move(platform))
    , mTimer(std::move(timer))
    , mLog(std::move(log))
  {
  }

  void enable(const bool bEnable)
  {
    if (bEnable)
    {
      scan();
    }
    else
    {
      mTimer->cancel();
    }
  }

  void scan()
  {
    using namespace std;
    debug(*mLog) << "Scanning network interfaces";
    // Rescan the hardware for available network interface addresses
    vector<asio::ip::address> addrs = mPlatform->scanIpIfAddrs();
    // Sort and unique them to guarantee consistent comparison
    sort(begin(addrs), end(addrs));
    addrs.erase(unique(begin(addrs), end(addrs)), end(addrs));
    // Pass them to the callback
    (*mCallback)(std::move(addrs));
    // setup the next scanning
    mTimer->expires_from_now(chrono::seconds(mPeriod));
    using ErrorCode = typename util::Injected<Timer>::type::ErrorCode;
    mTimer->async_wait([this](const ErrorCode e) {
        if (!e)
        {
          scan();
        }
      });
  }

private:
  const unsigned mPeriod;
  util::Injected<Callback> mCallback;
  util::Injected<Platform> mPlatform;
  util::Injected<Timer> mTimer;
  util::Injected<Log> mLog;
};

// Factory function
template <typename Callback, typename Platform, typename Timer, typename Log>
InterfaceScanner<Callback, Platform, Timer, Log> makeInterfaceScanner(
  const unsigned period,
  util::Injected<Callback> callback,
  util::Injected<Platform> platform,
  util::Injected<Timer> timer,
  util::Injected<Log> log)
{
  using namespace std;
  return {period, move(callback), move(platform), move(timer), move(log)};
}

} // namespace link
} // namespace ableton
