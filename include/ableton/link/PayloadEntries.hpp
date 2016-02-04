// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <cstdint>
#include <cmath>


namespace ableton
{
namespace link
{

struct HostTime
{
  enum { key = '__ht' };

  HostTime() = default;

  HostTime(const std::chrono::microseconds tm)
    : time(tm)
  {
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const HostTime& sht)
  {
    return discovery::sizeInByteStream(std::move(sht.time));
  }

  template <typename It>
  friend It toNetworkByteStream(const HostTime& sht, It out)
  {
    return discovery::toNetworkByteStream(std::move(sht.time), std::move(out));
  }

  template <typename It>
  static std::pair<HostTime, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto result = discovery::Deserialize<chrono::microseconds>::
      fromNetworkByteStream(move(begin), move(end));
    return make_pair(HostTime{move(result.first)}, move(result.second));
  }

  std::chrono::microseconds time;
};

struct GHostTime : HostTime
{
  enum { key = '__gt' };

  GHostTime() = default;

  GHostTime(const std::chrono::microseconds tm)
    : time(tm)
  {
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const GHostTime& dgt)
  {
    return discovery::sizeInByteStream(std::move(dgt.time));
  }

  template <typename It>
  friend It toNetworkByteStream(const GHostTime& dgt, It out)
  {
    return discovery::toNetworkByteStream(std::move(dgt.time), std::move(out));
  }

  template <typename It>
  static std::pair<GHostTime, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto result = discovery::Deserialize<chrono::microseconds>::
      fromNetworkByteStream(move(begin), move(end));
    return make_pair(GHostTime{move(result.first)}, move(result.second));
  }

  std::chrono::microseconds time;
};

struct PrevGHostTime
{
  enum { key = '_pgt' };

  PrevGHostTime() = default;

  PrevGHostTime(const std::chrono::microseconds tm)
    : time(tm)
  {
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const PrevGHostTime& dgt)
  {
    return discovery::sizeInByteStream(std::move(dgt.time));
  }

  template <typename It>
  friend It toNetworkByteStream(const PrevGHostTime& pdgt, It out)
  {
    return discovery::toNetworkByteStream(std::move(pdgt.time), std::move(out));
  }

  template <typename It>
  static std::pair<PrevGHostTime, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto result = discovery::Deserialize<chrono::microseconds>::
      fromNetworkByteStream(move(begin), move(end));
    return make_pair(PrevGHostTime{move(result.first)}, move(result.second));
  }

  std::chrono::microseconds time;
};

} // namespace link
} // namespace ableton
