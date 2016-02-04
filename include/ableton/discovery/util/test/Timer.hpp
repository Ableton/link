// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <functional>
#include <chrono>

namespace ableton
{
namespace util
{
namespace test
{

struct Timer
{
  using ErrorCode = int;
  using TimePoint = std::chrono::system_clock::time_point;

  // Initialize timer with an arbitrary large value to simulate the
  // time_since_epoch of a real clock.
  Timer()
    : mNow{std::chrono::milliseconds{123456789}}
  {
  }

  void expires_at(std::chrono::system_clock::time_point t)
  {
    cancel();
    mFireAt = std::move(t);
  }

  template <typename T, typename Rep>
  void expires_from_now(std::chrono::duration<T, Rep> duration)
  {
    cancel();
    mFireAt = now() + duration;
  }

  ErrorCode cancel()
  {
    if (mHandler)
    {
      mHandler(1); // call existing handler with truthy error code
    }
    mHandler = nullptr;
    return 0;
  }

  template <typename Handler>
  void async_wait(Handler handler)
  {
    mHandler = [handler](ErrorCode ec) {
      handler(ec);
    };
  }

  std::chrono::system_clock::time_point now() const
  {
    return mNow;
  }

  template <typename T, typename Rep>
  void advance(std::chrono::duration<T, Rep> duration)
  {
    mNow += duration;
    if (mHandler && mFireAt < mNow)
    {
      mHandler(0);
      mHandler = nullptr;
    }
  }

  std::function<void (ErrorCode)> mHandler;
  std::chrono::system_clock::time_point mFireAt;
  std::chrono::system_clock::time_point mNow;
};

} // namespace test
} // namespace util
} // namespace ableton
