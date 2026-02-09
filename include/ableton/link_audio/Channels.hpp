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

#include <ableton/discovery/AsioTypes.hpp>
#include <ableton/link_audio/ChannelAnnouncements.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/PeerAnnouncement.hpp>
#include <ableton/util/Injected.hpp>
#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename IoContext, typename Callback>
class Channels
{
  // non-movable private implementation type
  struct Impl;

public:
  struct Channel
  {
    std::string name;
    Id id;
    std::string peerName;
    Id peerId;
    Id sessionId;
  };

  struct ChannelInfo
  {
    Channel channel;
    discovery::IpAddress gatewayAddr;
  };

  Channels(util::Injected<IoContext> io, Callback callback)
    : mpImpl(std::make_shared<Impl>(std::move(io), std::move(callback)))
  {
  }

  struct GatewayObserver
  {
    using GatewayObserverAnnouncement = PeerAnnouncement;
    using GatewayObserverNodeId = link::NodeId;

    GatewayObserver(std::shared_ptr<Impl> pImpl, discovery::IpAddress addr)
      : mpImpl(std::move(pImpl))
      , mAddr(std::move(addr))
    {
    }

    GatewayObserver(const GatewayObserver&) = delete;

    GatewayObserver(GatewayObserver&& rhs)
      : mpImpl(std::move(rhs.mpImpl))
      , mAddr(std::move(rhs.mAddr))
    {
    }

    ~GatewayObserver()
    {
      // Check to handle the moved from case
      if (mpImpl)
      {
        mpImpl->gatewayClosed(mAddr);
      }
    }

    // model the observer concept from Channels
    template <typename Announcement>
    friend void sawAnnouncement(GatewayObserver& observer, Announcement announcement)
    {
      auto pImpl = observer.mpImpl;
      auto addr = observer.mAddr;
      assert(pImpl);
      pImpl->sawAnnouncementOnGateway(std::move(announcement), std::move(addr));
    }

    template <typename It>
    friend void channelsLeft(GatewayObserver& observer, It channelsBegin, It channelsEnd)
    {
      auto pImpl = observer.mpImpl;
      auto addr = observer.mAddr;
      pImpl->channelsLeftGateway(std::move(addr), channelsBegin, channelsEnd);
    }

    std::shared_ptr<Impl> mpImpl;
    discovery::IpAddress mAddr;
  };

  // Factory function for the gateway observer
  friend GatewayObserver makeGatewayObserver(Channels& channels,
                                             discovery::IpAddress addr)
  {
    return GatewayObserver{channels.mpImpl, std::move(addr)};
  }

private:
  struct Impl
  {
    Impl(util::Injected<IoContext> io, Callback callback)
      : mIo(std::move(io))
      , mCallback(std::move(callback))
    {
    }

    template <typename Announcement>
    void sawAnnouncementOnGateway(Announcement incoming, discovery::IpAddress gatewayAddr)
    {
      using namespace std;

      const auto peerSession = incoming.announcement.sessionId;
      const auto nodeId = incoming.announcement.nodeId;
      const auto peerInfo = incoming.announcement.peerInfo;
      const auto peerAudioChannels = incoming.announcement.channels.channels;
      bool didChannelsChange = false;

      for (const auto& peerChannel : peerAudioChannels)
      {
        const auto channelInfo = ChannelInfo{
          {peerChannel.name, peerChannel.id, peerInfo.name, nodeId, peerSession},
          gatewayAddr};

        const auto idRange = equal_range(begin(mChannels),
                                         end(mChannels),
                                         channelInfo,
                                         [](const auto& a, const auto& b)
                                         { return a.channel.id < b.channel.id; });

        if (idRange.first == idRange.second)
        {
          // This channel is not currently known on any gateway
          didChannelsChange = true;
          mChannels.insert(idRange.first, std::move(channelInfo));
        }
        else
        {
          // was it on this gateway?
          const auto addrRange = equal_range(idRange.first,
                                             idRange.second,
                                             channelInfo,
                                             [](const auto& a, const auto& b)
                                             { return a.gatewayAddr < b.gatewayAddr; });

          if (addrRange.first == addrRange.second)
          {
            // First time on this gateway, add it
            mChannels.insert(addrRange.first, std::move(channelInfo));
          }
          else
          {
            // We have an entry for this channel on this gateway, update it
            // callback if the name of the first entry changed

            auto cmp = [](const auto& lhs, const auto& rhs)
            {
              return std::tie(lhs.name, lhs.sessionId, lhs.peerName, lhs.peerId)
                     == std::tie(rhs.name, rhs.sessionId, rhs.peerName, rhs.peerId);
            };
            const auto firstChannelChanged =
              addrRange.first == idRange.first
              && !cmp(addrRange.first->channel, channelInfo.channel);
            didChannelsChange = didChannelsChange || firstChannelChanged;
            *addrRange.first = std::move(channelInfo);
          }
        }
      }

      // Invoke callbacks outside the critical section
      if (didChannelsChange)
      {
        mCallback();
      }
    }

    template <typename It>
    void channelsLeftGateway(const discovery::IpAddress& gatewayAddr,
                             It channelsBegin,
                             It channelsEnd)
    {
      using namespace std;

      auto channelsChanged = false;

      for (auto byeIt = channelsBegin; byeIt != channelsEnd; ++byeIt)
      {
        auto bye = *byeIt;

        auto it = remove_if(
          begin(mChannels),
          end(mChannels),
          [&](const auto& info)
          { return info.gatewayAddr == gatewayAddr && bye == info.channel.id; });
        channelsChanged = it != end(mChannels);
        mChannels.erase(it, end(mChannels));
      }

      if (channelsChanged)
      {
        mCallback();
      }
    }

    void gatewayClosed(const discovery::IpAddress& gatewayAddr)
    {
      using namespace std;
      auto it =
        remove_if(begin(mChannels),
                  end(mChannels),
                  [&](const auto& info) { return info.gatewayAddr == gatewayAddr; });

      const auto channelsChanged = it != end(mChannels);

      mChannels.erase(it, end(mChannels));

      if (channelsChanged)
      {
        mCallback();
      }
    }

    util::Injected<IoContext> mIo;
    Callback mCallback;
    std::vector<ChannelInfo> mChannels;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
