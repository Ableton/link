/* Copyright 2026, Ableton AG, Berlin. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/link/Beats.hpp>
#include <chrono>
#include <tuple>

namespace ableton
{
namespace link
{

struct ArrangementTimeline
{
  static const std::int32_t key = 'artl';
  static_assert(key == 0x6172746c, "Unexpected byte order");

  using ArrangementTimelineTuple = std::tuple<Beats, Beats, std::chrono::microseconds>;

  ArrangementTimeline() = default;

  ArrangementTimeline(const Beats abeatOrigin,
                      const Beats aLaunchBeats,
                      const std::chrono::microseconds aTimestamp)
    : beatOrigin(abeatOrigin)
    , launchBeats(aLaunchBeats)
    , timestamp(aTimestamp)
  {
  }

  friend bool operator==(const ArrangementTimeline& lhs, const ArrangementTimeline& rhs)
  {
    return std::tie(lhs.beatOrigin, lhs.launchBeats, lhs.timestamp)
           == std::tie(rhs.beatOrigin, rhs.launchBeats, rhs.timestamp);
  }

  friend bool operator!=(const ArrangementTimeline& lhs, const ArrangementTimeline& rhs)
  {
    return !(lhs == rhs);
  }

  friend std::uint32_t sizeInByteStream(const ArrangementTimeline& timeline)
  {
    return discovery::sizeInByteStream(timeline.asTuple());
  }

  template <typename It>
  friend It toNetworkByteStream(const ArrangementTimeline& timeline, It out)
  {
    return discovery::toNetworkByteStream(timeline.asTuple(), std::move(out));
  }

  template <typename It>
  static std::pair<ArrangementTimeline, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace discovery;
    auto result = Deserialize<ArrangementTimelineTuple>::fromNetworkByteStream(
      std::move(begin), std::move(end));
    return std::make_pair(
      ArrangementTimeline{
        std::get<0>(result.first), std::get<1>(result.first), std::get<2>(result.first)},
      std::move(result.second));
  }

  Beats beatOrigin{0.};
  Beats launchBeats{0.};
  std::chrono::microseconds timestamp{0};

private:
  ArrangementTimelineTuple asTuple() const
  {
    return std::make_tuple(beatOrigin, launchBeats, timestamp);
  }
};

struct ClientArrangementTimeline
{
  ClientArrangementTimeline() = default;

  ClientArrangementTimeline(const Beats abeatOrigin,
                            const std::chrono::microseconds aLaunchTime,
                            const std::chrono::microseconds aTimestamp)
    : beatOrigin(abeatOrigin)
    , launchTime(aLaunchTime)
    , timestamp(aTimestamp)
  {
  }

  friend bool operator==(const ClientArrangementTimeline& lhs,
                         const ClientArrangementTimeline& rhs)
  {
    return std::tie(lhs.beatOrigin, lhs.launchTime, lhs.timestamp)
           == std::tie(rhs.beatOrigin, rhs.launchTime, rhs.timestamp);
  }

  friend bool operator!=(const ClientArrangementTimeline& lhs,
                         const ClientArrangementTimeline& rhs)
  {
    return !(lhs == rhs);
  }

  Beats beatOrigin{0.};
  std::chrono::microseconds launchTime{0};
  std::chrono::microseconds timestamp{0};
};

// API (host-time domain) representation. No timestamp, so capture/commit diffs
// don't spuriously fire; a jump is detected via a changed beatOrigin.
struct ApiArrangementTimeline
{
  ApiArrangementTimeline() = default;

  ApiArrangementTimeline(const Beats abeatOrigin,
                         const std::chrono::microseconds aLaunchTime)
    : beatOrigin(abeatOrigin)
    , launchTime(aLaunchTime)
  {
  }

  friend bool operator==(const ApiArrangementTimeline& lhs,
                         const ApiArrangementTimeline& rhs)
  {
    return std::tie(lhs.beatOrigin, lhs.launchTime)
           == std::tie(rhs.beatOrigin, rhs.launchTime);
  }

  friend bool operator!=(const ApiArrangementTimeline& lhs,
                         const ApiArrangementTimeline& rhs)
  {
    return !(lhs == rhs);
  }

  Beats beatOrigin{0.};
  std::chrono::microseconds launchTime{0};
};

} // namespace link
} // namespace ableton
