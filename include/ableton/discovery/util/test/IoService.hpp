// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/util/test/Timer.hpp>

namespace ableton
{
namespace util
{
namespace test
{

struct IoService
{
  // Wrapper around the internal util::test::Timer in the list
  struct Timer
  {
    using ErrorCode = test::Timer::ErrorCode;
    using TimePoint = test::Timer::TimePoint;

    Timer(util::test::Timer* pTimer)
      : mpTimer(pTimer)
    {
    }

    void expires_at(std::chrono::system_clock::time_point t)
    {
      mpTimer->expires_at(t);
    }

    template <typename T, typename Rep>
    void expires_from_now(std::chrono::duration<T, Rep> duration)
    {
      mpTimer->expires_from_now(duration);
    }

    ErrorCode cancel()
    {
      return mpTimer->cancel();
    }

    template <typename Handler>
    void async_wait(Handler handler)
    {
      mpTimer->async_wait(std::move(handler));
    }

    TimePoint now() const
    {
      return mpTimer->now();
    }

    util::test::Timer* mpTimer;
  };

  IoService() = default;

  Timer makeTimer()
  {
    mTimers.emplace_back();
    return Timer{&mTimers.back()};
  }

  template <typename Handler>
  void post(Handler handler)
  {
    mHandlers.emplace_back(std::move(handler));
  }

  template <typename T, typename Rep>
  void advance(std::chrono::duration<T, Rep> duration)
  {
    runHandlers();

    for (auto& timer : mTimers)
    {
      timer.advance(duration);
    }
  }

  void runHandlers()
  {
    for (auto& handler : mHandlers)
    {
      handler();
    }
    mHandlers.clear();
  }

  std::vector<std::function<void ()>> mHandlers;
  std::vector<util::test::Timer> mTimers;

};

} // namespace test
} // namespace util
} // namespace ableton
