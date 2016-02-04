// Copyright: 2016, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/LinearRegression.hpp>
#include <vector>

namespace ableton
{
namespace link
{

template <class T>
class HostTimeUtility
{
  static const std::size_t kNumPoints = 512;
  using Points = std::vector<std::pair<double, double>>;
  using PointIt = typename Points::iterator;

public:
  HostTimeUtility()
    : mIndex(0)
  {
    mPoints.reserve(kNumPoints);
  }

  ~HostTimeUtility() = default;

  void reset()
  {
    mIndex = 0;
    mPoints.clear();
  }

  double sampleTimeToHostTime(const double sampleTime)
  {
    const auto htMicros = static_cast<double>(micros(mHostTimeSampler).count());
    const auto point = std::make_pair(sampleTime, htMicros);

    if (mPoints.size() < kNumPoints)
    {
      mPoints.push_back(point);
    }
    else
    {
      mPoints[mIndex] = point;
    }
    mIndex = (mIndex + 1) % kNumPoints;

    const auto result =
      linearRegression(mPoints.begin(), mPoints.end());

    return (result.first * sampleTime) + result.second;
  }

private:
  std::size_t mIndex;
  Points mPoints;
  T mHostTimeSampler;
};

} // namespace link
} // namespace ableton
