// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <cstdint>
#include <tuple>
#include <cmath>

namespace ableton
{
namespace link
{

struct Beats : std::tuple<std::int64_t>
{
  Beats() = default;

  explicit Beats(const double beats)
    : std::tuple<std::int64_t>(std::llround(beats * 1e6))
  {
  }

  explicit Beats(const std::int64_t microBeats)
    : std::tuple<std::int64_t>(microBeats)
  {
  }

  friend double floating(const Beats beats)
  {
    return microBeats(beats) / 1e6;
  }

  friend std::int64_t microBeats(const Beats beats)
  {
    return std::get<0>(beats);
  }

  friend Beats operator+(const Beats lhs, const Beats rhs)
  {
    return Beats{microBeats(lhs) + microBeats(rhs)};
  }

  friend Beats operator-(const Beats lhs, const Beats rhs)
  {
    return Beats{microBeats(lhs) - microBeats(rhs)};
  }

  friend Beats operator%(const Beats lhs, const Beats rhs)
  {
    return Beats{microBeats(lhs) % microBeats(rhs)};
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const Beats beats)
  {
    return discovery::sizeInByteStream(microBeats(beats));
  }

  template <typename It>
  friend It toNetworkByteStream(const Beats beats, It out)
  {
    return discovery::toNetworkByteStream(
      microBeats(beats), std::move(out));
  }

  template <typename It>
  static std::pair<Beats, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = discovery::Deserialize<std::int64_t>::
      fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(Beats{result.first}, std::move(result.second));
  }
};

} // namespace link
} // namespace ableton
