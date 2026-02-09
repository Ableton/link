#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <numeric>

namespace ableton
{
namespace link_audio
{

struct NetworkMetricsFilter
{
  struct NetworkMetrics
  {
    double speed;
    double jitter;

    double quality() const { return speed / (1.0 + jitter); }

    bool operator<(const NetworkMetrics& other) const
    {
      return quality() < other.quality();
    }
  };

  NetworkMetricsFilter()
    : mCurrentIndex(0)
    , mCount(0)
  {
  }

  void operator()(std::chrono::microseconds time)
  {
    mPingPongTimes[mCurrentIndex] = time;
    mCurrentIndex = (mCurrentIndex + 1) % kMaxSize;
    if (mCount < kMaxSize)
    {
      ++mCount;
    }
  }

  NetworkMetrics metrics() const
  {
    if (mCount == 0)
    {
      return {0.0, 0.0};
    }

    double sum =
      std::accumulate(mPingPongTimes.begin(),
                      mPingPongTimes.begin() + mCount,
                      0.0,
                      [](auto acc, const auto& time) { return acc + time.count(); });
    double avgRTT = sum / mCount;

    double speed = avgRTT != 0.0 ? 1e6 / avgRTT : 0.0;

    double variance = 0.0;
    for (size_t i = 0; i < mCount; ++i)
    {
      double diff = mPingPongTimes[i].count() - avgRTT;
      variance += diff * diff;
    }
    variance /= mCount;
    double jitter = std::sqrt(variance);

    return {speed, (1e4 - 1e4 * mCount / kMaxSize) + jitter};
  }

  double quality() const { return metrics().quality(); }

private:
  static constexpr size_t kMaxSize = 10;
  std::array<std::chrono::microseconds, kMaxSize> mPingPongTimes;
  size_t mCurrentIndex;
  size_t mCount;
};

} // namespace link_audio
} // namespace ableton
