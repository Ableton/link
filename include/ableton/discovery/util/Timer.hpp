// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>

namespace ableton
{
namespace util
{

// Concept: Timer
//
// Based on the boost::asio::system_timer interface, slightly
// simplified and transformed to use friend functions and with
// the addition of a now() function and an ErrorCode member type.
// Declaration is for documentation purposes only.

struct Timer
{
  using ErrorCode = int;

  friend void expires_at(Timer&, std::chrono::system_clock::time_point);

  friend void expires_from_now(Timer&, std::chrono::seconds);

  // If result is truthy then an error occured during cancellation
  friend ErrorCode cancel(Timer&);

  // Handler takes a single argument of type ErrorCode. If ErrorCode
  // is truthy, an error has occurred and the handler should not run.
  template <typename Handler>
  friend void async_wait(Timer&, Handler);

  // The current time
  friend std::chrono::system_clock::time_point now(const Timer&);
};

} // namespace util
} // namespace ableton
