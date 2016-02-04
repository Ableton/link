// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>

namespace platforms
{

struct Clock
{
  using Ticks = std::uint64_t;

  Clock()
  {
    mStartTime = std::chrono::high_resolution_clock::now();
  }

  friend std::chrono::microseconds micros(const Clock& clock)
  {
    using namespace std::chrono;
    return duration_cast<microseconds>(high_resolution_clock::now() - clock.mStartTime);
  }

  std::chrono::high_resolution_clock::time_point mStartTime;
};

} // platforms
