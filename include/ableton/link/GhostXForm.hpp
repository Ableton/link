// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <cmath>
#include <chrono>

namespace ableton
{
namespace link
{

using std::chrono::microseconds;

struct GhostXForm
{
  friend microseconds hostToGhost(const GhostXForm xform, const microseconds hostTime)
  {
    return microseconds{llround(xform.slope * hostTime.count())} + xform.intercept;
  }

  friend microseconds ghostToHost(const GhostXForm xform, const microseconds ghostTime)
  {
    return microseconds{llround((ghostTime - xform.intercept).count() / xform.slope)};
  }

  friend bool operator==(const GhostXForm lhs, const GhostXForm rhs)
  {
    return lhs.slope == rhs.slope && lhs.intercept == rhs.intercept;
  }

  double slope;
  microseconds intercept;
};

} // namespace link
} // namespace ableton
