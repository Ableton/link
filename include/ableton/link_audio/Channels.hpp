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
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename IoContext, typename Callback, typename Interface>
class Channels
{
  // non-movable private implementation type
  struct Impl;

public:
  struct SendHandler
  {
    SendHandler(discovery::UdpEndpoint endpoint, std::shared_ptr<Interface> interface)
      : mEndpoint(endpoint)
      , mpInterface(interface)
    {
    }

    std::size_t operator()(const uint8_t* const pData, const size_t numBytes)
    {
      if (auto interface = mpInterface.lock())
      {
        try
        {
          return interface->send(pData, numBytes, mEndpoint);
        }
        catch (const std::runtime_error&)
        {
        }
      }
      return 0;
    }

    discovery::UdpEndpoint endpoint() const { return mEndpoint; }

  private:
    discovery::UdpEndpoint mEndpoint;
    std::weak_ptr<Interface> mpInterface;
  };

  struct Channel
  {
    std::string name;
    Id id;
    std::string peerName;
    Id peerId;
    Id sessionId;

    friend bool operator==(const Channel& lhs, const Channel& rhs)
    {
      return std::tie(lhs.name, lhs.id, lhs.peerName, lhs.peerId, lhs.sessionId)
             == std::tie(rhs.name, rhs.id, rhs.peerName, rhs.peerId, rhs.sessionId);
    }
  };

  struct ChannelInfo
  {
    Channel channel;
    discovery::IpAddress gatewayAddr;
  };

  struct ChannelInfoCompare
  {
    bool operator()(const ChannelInfo& lhs, const ChannelInfo& rhs) const
    {
      if (lhs.channel.sessionId == rhs.channel.sessionId)
      {
        if (lhs.channel.peerId == rhs.channel.peerId)
        {
          if (lhs.channel.id == rhs.channel.id)
          {
            return lhs.gatewayAddr < rhs.gatewayAddr;
          }
          return lhs.channel.channelId < rhs.channel.channelId;
        }
        return lhs.channel.peerId < rhs.channel.peerId;
      }
      return lhs.channel.sessionId < rhs.channel.sessionId;
    }
  };

  Channels(util::Injected<IoContext> io, Callback callback)
    : mpImpl(std::make_shared<Impl>(std::move(io), std::move(callback)))
  {
  }

  std::vector<Channel> sessionChannels(const link::SessionId& sessionId) const
  {
    using namespace std;
    vector<Channel> result;
    auto& channelsVec = mpImpl->mChannels;
    for (const auto& channel : mpImpl->mChannels)
    {
      if (channel.channel.sessionId == sessionId)
      {
        result.push_back(channel.channel);
      }
    }
    return result;
  }

  std::vector<Channel> uniqueSessionChannels(const link::SessionId& sessionId) const
  {
    auto channels = sessionChannels(sessionId);
    auto it = std::unique(begin(channels),
                          end(channels),
                          [](const auto& a, const auto& b) { return a.id == b.id; });
    return {begin(channels), it};
  }

  std::optional<SendHandler> peerSendHandler(const Id peerId) const
  {
    auto it = mpImpl->mPeerSendHandlers.find(peerId);
    return it != mpImpl->mPeerSendHandlers.end() ? std::optional{it->second.handler}
                                                 : std::nullopt;
  }

  std::optional<SendHandler> channelSendHandler(const Id channelId) const
  {
    const auto& channels = mpImpl->mChannels;
    const auto it =
      std::find_if(begin(channels),
                   end(channels),
                   [&](const auto& info) { return info.channel.id == channelId; });
    return it != end(channels) ? peerSendHandler(it->channel.peerId) : std::nullopt;
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
  using Timer = typename util::Injected<IoContext>::type::Timer;
  using TimerError = typename Timer::ErrorCode;
  using TimePoint = typename Timer::TimePoint;

  struct TimeoutCompare
  {
    bool operator()(const std::tuple<TimePoint, discovery::IpAddress, Id>& lhs,
                    const std::tuple<TimePoint, discovery::IpAddress, Id>& rhs) const
    {
      return std::get<TimePoint>(lhs) < std::get<TimePoint>(rhs);
    }
  };

  struct Impl
  {
    Impl(util::Injected<IoContext> io, Callback callback)
      : mIo(std::move(io))
      , mCallback(std::move(callback))
      , mPruneTimer(mIo->makeTimer())
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
      const auto ttl = incoming.ttl;
      auto sendHandler = SendHandler(incoming.from, incoming.pInterface);
      const auto networkQuality = incoming.networkQuality;
      bool didChannelsChange = false;

      for (const auto& peerChannel : peerAudioChannels)
      {
        const auto channelInfo = ChannelInfo{
          {peerChannel.name, peerChannel.id, peerInfo.name, nodeId, peerSession},
          gatewayAddr};

        {
          const auto timeout =
            std::find_if(begin(mChannelTimeouts),
                         end(mChannelTimeouts),
                         [&](const auto& timeout)
                         {
                           return std::get<discovery::IpAddress>(timeout) == gatewayAddr
                                  && std::get<Id>(timeout) == channelInfo.channel.id;
                         });
          if (timeout != end(mChannelTimeouts))
          {
            mChannelTimeouts.erase(timeout);
          }

          auto newTo = std::make_tuple(mPruneTimer.now() + std::chrono::seconds(ttl),
                                       gatewayAddr,
                                       channelInfo.channel.id);
          mChannelTimeouts.insert(
            upper_bound(
              begin(mChannelTimeouts), end(mChannelTimeouts), newTo, TimeoutCompare{}),
            newTo);
        }

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

        if (mPeerSendHandlers.count(nodeId) == 0)
        {
          mPeerSendHandlers.emplace(nodeId, PeerSendHandler{sendHandler, networkQuality});
        }
        else if (mPeerSendHandlers.at(nodeId).networkQuality < networkQuality)
        {
          mPeerSendHandlers.insert_or_assign(
            nodeId, PeerSendHandler{sendHandler, networkQuality});
        }
      }

      // Invoke callbacks outside the critical section
      if (didChannelsChange)
      {
        mCallback();
      }

      scheduleNextPruning();
    }

    void pruneSendHandlers()
    {
      using namespace std;
      for (auto it = begin(mPeerSendHandlers); it != end(mPeerSendHandlers);)
      {
        if (none_of(begin(mChannels),
                    end(mChannels),
                    [&](const auto& info) { return info.channel.peerId == it->first; }))
        {
          it = mPeerSendHandlers.erase(it);
        }
        else
        {
          ++it;
        }
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

        mChannelTimeouts.erase(remove_if(begin(mChannelTimeouts),
                                         end(mChannelTimeouts),
                                         [&](const auto& timeout)
                                         {
                                           return std::get<discovery::IpAddress>(timeout)
                                                    == gatewayAddr
                                                  && std::get<Id>(timeout) == bye;
                                         }),
                               end(mChannelTimeouts));
      }

      scheduleNextPruning();

      if (channelsChanged)
      {
        pruneSendHandlers();
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

      mChannelTimeouts.erase(
        std::remove_if(
          begin(mChannelTimeouts),
          end(mChannelTimeouts),
          [&](const auto& timeout)
          { return std::get<discovery::IpAddress>(timeout) == gatewayAddr; }),
        end(mChannelTimeouts));

      scheduleNextPruning();

      if (channelsChanged)
      {
        pruneSendHandlers();
        mCallback();
      }
    }

    void pruneExpiredChannels()
    {
      using namespace std;

      const auto test = make_tuple(mPruneTimer.now(), discovery::IpAddress{}, Id{});

      const auto endExpired = lower_bound(
        begin(mChannelTimeouts), end(mChannelTimeouts), test, TimeoutCompare{});

      auto channelsChanged = false;
      if (endExpired != begin(mChannelTimeouts))
      {
        channelsChanged = true;

        for_each(
          begin(mChannelTimeouts),
          endExpired,
          [&](const auto& timeout)
          {
            const auto& id = std::get<Id>(timeout);
            const auto& gatewayAddr = std::get<discovery::IpAddress>(timeout);

            auto it = remove_if(
              begin(mChannels),
              end(mChannels),
              [&](const auto& info)
              { return info.channel.id == id && info.gatewayAddr == gatewayAddr; });

            mChannels.erase(it, end(mChannels));
          });
      }

      mChannelTimeouts.erase(begin(mChannelTimeouts), endExpired);

      scheduleNextPruning();

      if (channelsChanged)
      {
        pruneSendHandlers();
        mCallback();
      }
    }

    void scheduleNextPruning()
    {
      if (!mChannelTimeouts.empty())
      {
        // Add a second of padding to the timer to avoid over-eager timeouts
        const auto t =
          std::get<TimePoint>(mChannelTimeouts.front()) + std::chrono::seconds(1);
        mPruneTimer.expires_at(t);
        mPruneTimer.async_wait(
          [this](const TimerError e)
          {
            if (!e)
            {
              pruneExpiredChannels();
            }
          });
      }
    }

    util::Injected<IoContext> mIo;
    Callback mCallback;
    std::vector<ChannelInfo> mChannels;

    struct PeerSendHandler
    {
      SendHandler handler;
      double networkQuality;
    };
    std::map<Id, PeerSendHandler> mPeerSendHandlers;

    using ChannelTimeout = std::tuple<TimePoint, discovery::IpAddress, Id>;
    using ChannelTimeouts = std::vector<ChannelTimeout>;
    ChannelTimeouts mChannelTimeouts;
    Timer mPruneTimer;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
