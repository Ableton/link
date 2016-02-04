// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/link/Beats.hpp>
#include <ableton/link/Tempo.hpp>
#include <cstdint>
#include <cmath>
#include <tuple>

namespace ableton
{
namespace link
{

// A tuple of (tempo, beats, time), with integral units
// based on microseconds. This type establishes a bijection between
// beats and wall time, given a valid tempo. It also serves as a
// payload entry.

struct Timeline
{
  enum { key = 'tmln' };

  friend Beats toBeats(const Timeline tl, const std::chrono::microseconds time)
  {
    return tl.beatOrigin + microsToBeats(tl.tempo, time - tl.timeOrigin);
  }

  friend std::chrono::microseconds fromBeats(const Timeline tl, const Beats beats)
  {
    return tl.timeOrigin + beatsToMicros(tl.tempo, beats - tl.beatOrigin);
  }

  friend bool operator==(const Timeline& lhs, const Timeline& rhs)
  {
    return
      std::tie(lhs.tempo, lhs.beatOrigin, lhs.timeOrigin) ==
      std::tie(rhs.tempo, rhs.beatOrigin, rhs.timeOrigin);
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const Timeline& tl)
  {
    return discovery::sizeInByteStream(
      std::tie(tl.tempo, tl.beatOrigin, tl.timeOrigin));
  }

  template <typename It>
  friend It toNetworkByteStream(const Timeline& tl, It out)
  {
    return discovery::toNetworkByteStream(
      std::tie(tl.tempo, tl.beatOrigin, tl.timeOrigin), std::move(out));
  }

  template <typename It>
  static std::pair<Timeline, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    using namespace discovery;
    Timeline timeline;
    auto result = Deserialize<tuple<Tempo, Beats, chrono::microseconds>>::
      fromNetworkByteStream(move(begin), move(end));
    tie(timeline.tempo, timeline.beatOrigin, timeline.timeOrigin) = move(result.first);
    return make_pair(move(timeline), move(result.second));
  }

  Tempo tempo;
  Beats beatOrigin;
  std::chrono::microseconds timeOrigin;
};

} // namespace link
} // namespace ableton
