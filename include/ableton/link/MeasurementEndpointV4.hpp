// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/NetworkByteStreamSerializable.hpp>

namespace ableton
{
namespace link
{

struct MeasurementEndpointV4
{
  enum { key = 'mep4' };

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const MeasurementEndpointV4 mep)
  {
    return
      discovery::sizeInByteStream(
        static_cast<std::uint32_t>(mep.ep.address().to_v4().to_ulong())) +
      discovery::sizeInByteStream(mep.ep.port());
  }

  template <typename It>
  friend It toNetworkByteStream(const MeasurementEndpointV4 mep, It out)
  {
    return discovery::toNetworkByteStream(mep.ep.port(),
      discovery::toNetworkByteStream(
        static_cast<std::uint32_t>(mep.ep.address().to_v4().to_ulong()),
        std::move(out)));
  }

  template <typename It>
  static std::pair<MeasurementEndpointV4, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto addrRes = discovery::Deserialize<std::uint32_t>::
      fromNetworkByteStream(move(begin), end);
    auto portRes = discovery::Deserialize<std::uint16_t>::
      fromNetworkByteStream(move(addrRes.second), end);
    return make_pair(
      MeasurementEndpointV4{
        {asio::ip::address_v4{move(addrRes.first)}, move(portRes.first)}},
      move(portRes.second));
  }

  asio::ip::udp::endpoint ep;
};

} // namespace link
} // namespace ableton
