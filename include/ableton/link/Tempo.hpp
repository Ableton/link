// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/Beats.hpp>
#include <chrono>

namespace ableton
{
namespace link
{

struct Tempo : std::tuple<std::chrono::microseconds>
{
  Tempo() = default;

  // Beats per minute
  explicit Tempo(const double bpm)
    : std::tuple<std::chrono::microseconds>(
	    std::chrono::microseconds(std::llround(1e6 / (bpm / 60))))
  {
  }

  // Beats / microseconds
  Tempo(const Beats beats, const std::chrono::microseconds micros)
    : std::tuple<std::chrono::microseconds>(
       std::chrono::microseconds(std::llround(micros.count() / floating(beats))))
  {
  }

  // Microseconds / Beat
  explicit Tempo(std::chrono::microseconds microsPerBeat)
    : std::tuple<std::chrono::microseconds>(std::move(microsPerBeat))
  {
  }

  friend double bpm(const Tempo tempo)
  {
    return 60. / (microsPerBeat(tempo).count() / 1e6);
  }

  friend std::chrono::microseconds microsPerBeat(const Tempo tempo)
  {
    return std::get<0>(tempo);
  }

  friend bool operator<(const Tempo& lhs, const Tempo& rhs)
  {
    return bpm(lhs) < bpm(rhs);
  }

  friend bool operator>(const Tempo& lhs, const Tempo& rhs)
  {
    return bpm(lhs) > bpm(rhs);
  }

  friend bool operator<=(const Tempo& lhs, const Tempo& rhs)
  {
    return bpm(lhs) <= bpm(rhs);
  }

  friend bool operator>=(const Tempo& lhs, const Tempo& rhs)
  {
    return bpm(lhs) >= bpm(rhs);
  }

  // Given the tempo, convert a time to a beat value
  friend Beats microsToBeats(const Tempo tempo, const std::chrono::microseconds micros)
  {
    return Beats{micros.count() / static_cast<double>(microsPerBeat(tempo).count())};
  }

  // Given the tempo, convert a beat to a time value
  friend std::chrono::microseconds beatsToMicros(const Tempo tempo, const Beats beats)
  {
    return std::chrono::microseconds{
      std::llround(floating(beats) * microsPerBeat(tempo).count())};
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const Tempo tempo)
  {
    return discovery::sizeInByteStream(microsPerBeat(tempo));
  }

  template <typename It>
  friend It toNetworkByteStream(const Tempo tempo, It out)
  {
    return discovery::toNetworkByteStream(
      microsPerBeat(tempo), std::move(out));
  }

  template <typename It>
  static std::pair<Tempo, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = discovery::Deserialize<std::chrono::microseconds>::
      fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(Tempo{std::move(result.first)}, std::move(result.second));
  }
};

} // namespace link
} // namespace ableton
