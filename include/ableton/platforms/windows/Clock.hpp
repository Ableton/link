// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <chrono>
#include <cstdint>

namespace platforms
{
namespace windows
{

struct Clock
{
  using Ticks = std::int64_t;
  using Micros = std::chrono::microseconds;

  Clock()
  {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    mTicksToMicros = 1.0e6 / frequency.QuadPart;
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
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
  }

  friend std::chrono::microseconds micros(const Clock& clock)
  {
    return ticksToMicros(clock, ticks(clock));
  }

  double mTicksToMicros;
};

} // windows
} // platforms
