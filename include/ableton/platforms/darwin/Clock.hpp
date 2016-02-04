// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <mach/mach_time.h>
#include <chrono>

namespace platforms
{
namespace darwin
{

struct Clock
{
  using Ticks = std::uint64_t;
  using Micros = std::chrono::microseconds;

  Clock()
  {
    mach_timebase_info_data_t timeInfo;
    mach_timebase_info(&timeInfo);
    // numer / denom gives nanoseconds, we want microseconds
    mTicksToMicros = timeInfo.numer / (timeInfo.denom * 1000.);
  }

  friend Micros ticksToMicros(const Clock& clock, const Ticks ticks)
  {
    return Micros{std::llround(clock.mTicksToMicros * ticks)};
  }

  friend Ticks microsToTicks(const Clock& clock, const Micros micros)
  {
    return static_cast<Ticks>(micros.count() / clock.mTicksToMicros);
  }

  friend Ticks ticks(const Clock&)
  {
    return mach_absolute_time();
  }

  friend std::chrono::microseconds micros(const Clock& clock)
  {
    return ticksToMicros(clock, ticks(clock));
  }

  double mTicksToMicros;
};

} // darwin
} // platforms
