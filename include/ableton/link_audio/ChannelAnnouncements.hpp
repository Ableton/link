/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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
#include <ableton/link_audio/Id.hpp>

namespace ableton
{
namespace link_audio
{

struct ChannelAnnouncement
{
  std::string name;
  Id id;

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const ChannelAnnouncement& channel)
  {
    return discovery::sizeInByteStream(channel.name)
           + discovery::sizeInByteStream(channel.id);
  }

  template <typename It>
  friend It toNetworkByteStream(const ChannelAnnouncement& channel, It out)
  {
    return discovery::toNetworkByteStream(
      channel.id, discovery::toNetworkByteStream(channel.name, std::move(out)));
  }

  template <typename It>
  static std::pair<ChannelAnnouncement, It> fromNetworkByteStream(It begin, It end)
  {
    auto [name, nameEnd] =
      discovery::Deserialize<std::string>::fromNetworkByteStream(begin, end);
    auto [id, idEnd] = discovery::Deserialize<Id>::fromNetworkByteStream(nameEnd, end);
    return std::make_pair(ChannelAnnouncement{std::move(name), std::move(id)}, idEnd);
  }

  friend bool operator==(const ChannelAnnouncement& lhs, const ChannelAnnouncement& rhs)
  {
    return std::tie(lhs.name, lhs.id) == std::tie(rhs.name, rhs.id);
  }
};
;


struct ChannelAnnouncements
{
  static constexpr std::int32_t key = 'auca';
  static_assert(key == 0x61756361, "Unexpected byte order");

  std::vector<ChannelAnnouncement> channels;

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const ChannelAnnouncements& announcements)
  {
    return discovery::sizeInByteStream(announcements.channels);
  }

  template <typename It>
  friend It toNetworkByteStream(const ChannelAnnouncements& announcements, It out)
  {
    return discovery::toNetworkByteStream(announcements.channels, std::move(out));
  }

  template <typename It>
  static std::pair<ChannelAnnouncements, It> fromNetworkByteStream(It begin, It end)
  {
    auto [channels, announcementsEnd] =
      discovery::Deserialize<std::vector<ChannelAnnouncement>>::fromNetworkByteStream(
        std::move(begin), end);
    return std::make_pair(ChannelAnnouncements{channels}, announcementsEnd);
  }

  friend bool operator==(const ChannelAnnouncements& lhs, const ChannelAnnouncements& rhs)
  {
    return lhs.channels == rhs.channels;
  }

  friend bool operator!=(const ChannelAnnouncements& lhs, const ChannelAnnouncements& rhs)
  {
    return !(lhs == rhs);
  }
};

struct ChannelBye
{
  Id id;

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const ChannelBye& bye)
  {
    return discovery::sizeInByteStream(bye.id);
  }

  template <typename It>
  friend It toNetworkByteStream(const ChannelBye& bye, It out)
  {
    return discovery::toNetworkByteStream(bye.id, std::move(out));
  }

  template <typename It>
  static std::pair<ChannelBye, It> fromNetworkByteStream(It begin, It end)
  {
    auto [bye, byeEnd] =
      discovery::Deserialize<Id>::fromNetworkByteStream(std::move(begin), end);
    return std::make_pair(ChannelBye{bye}, byeEnd);
  }

  friend bool operator==(const ChannelBye& lhs, const ChannelBye& rhs)
  {
    return lhs.id == rhs.id;
  }

  friend bool operator!=(const ChannelBye& lhs, const ChannelBye& rhs)
  {
    return !(lhs == rhs);
  }
};

struct ChannelByes
{
  static constexpr std::int32_t key = 'aucb';
  static_assert(key == 0x61756362, "Unexpected byte order");

  std::vector<ChannelBye> byes;

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const ChannelByes& channels)
  {
    return discovery::sizeInByteStream(channels.byes);
  }

  template <typename It>
  friend It toNetworkByteStream(const ChannelByes& channels, It out)
  {
    return discovery::toNetworkByteStream(channels.byes, std::move(out));
  }

  template <typename It>
  static std::pair<ChannelByes, It> fromNetworkByteStream(It begin, It end)
  {
    auto [byes, byesEnd] =
      discovery::Deserialize<std::vector<ChannelBye>>::fromNetworkByteStream(
        std::move(begin), end);
    return std::make_pair(ChannelByes{byes}, byesEnd);
  }

  friend bool operator==(const ChannelByes& lhs, const ChannelByes& rhs)
  {
    return lhs.byes == rhs.byes;
  }

  friend bool operator!=(const ChannelByes& lhs, const ChannelByes& rhs)
  {
    return !(lhs == rhs);
  }
};

} // namespace link_audio
} // namespace ableton
