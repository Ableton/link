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
#include <ableton/discovery/UdpMessenger.hpp>
#include <ableton/discovery/UnicastIpInterface.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/util/Injected.hpp>
#include <ableton/util/SafeAsyncHandler.hpp>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>

namespace ableton
{
namespace link_audio
{

// Throws UdpSendException
template <typename Interface, typename NodeId, typename Payload>
void sendLinkAudioUdpMessage(Interface& iface,
                             NodeId from,
                             const uint8_t ttl,
                             const v1::MessageType messageType,
                             const Payload& payload,
                             const discovery::UdpEndpoint& to)
{
  using namespace std;
  v1::MessageBuffer buffer;
  const auto messageBegin = begin(buffer);
  const auto messageEnd =
    v1::detail::encodeMessage(std::move(from), ttl, messageType, payload, messageBegin);
  const auto numBytes = static_cast<size_t>(distance(messageBegin, messageEnd));
  try
  {
    iface.send(buffer.data(), numBytes, to);
  }
  catch (const std::runtime_error& err)
  {
    throw discovery::UdpSendException{err, iface.endpoint().address()};
  }
}

// UdpMessenger uses a "shared_ptr pImpl" pattern to make it movable
// and to support safe async handler callbacks when receiving messages
// on the given interface.
template <typename Interface, typename Observer, typename IoContext>
class UdpMessenger
{
public:
  using ObserverType = typename util::Injected<Observer>::type;
  using Announcement = typename ObserverType::GatewayObserverAnnouncement;
  using NodeId = typename ObserverType::GatewayObserverNodeId;
  using Timer = typename util::Injected<IoContext>::type::Timer;
  using TimerError = typename Timer::ErrorCode;
  using TimePoint = typename Timer::TimePoint;

  struct ExtendedAnnouncement
  {
    link::NodeId ident() const { return announcement.ident(); }

    Announcement announcement;
  };

  UdpMessenger(util::Injected<Interface> iface,
               Announcement announcement,
               util::Injected<IoContext> io,
               const uint8_t ttl,
               const uint8_t ttlRatio,
               util::Injected<Observer> observer)
    : mpImpl(std::make_shared<Impl>(std::move(iface),
                                    std::move(announcement),
                                    std::move(io),
                                    ttl,
                                    ttlRatio,
                                    std::move(observer)))
  {
    // We need to always listen for incoming traffic in order to
    // respond to announcement broadcasts
    mpImpl->listen();
    mpImpl->broadcastAnnouncement();
  }

  UdpMessenger(const UdpMessenger&) = delete;
  UdpMessenger& operator=(const UdpMessenger&) = delete;

  UdpMessenger(UdpMessenger&& rhs)
    : mpImpl(std::move(rhs.mpImpl))
  {
  }

  void updateAnnouncement(Announcement announcement)
  {
    mpImpl->updateAnnouncement(std::move(announcement));
  }

  discovery::UdpEndpoint endpoint() const { return mpImpl->mpInterface->endpoint(); }

private:
  struct Receiver
  {
    link::NodeId id;
    discovery::UdpEndpoint endpoint;
  };

  struct Impl : std::enable_shared_from_this<Impl>
  {
    Impl(util::Injected<Interface> iface,
         Announcement announcement,
         util::Injected<IoContext> io,
         const uint8_t ttl,
         const uint8_t ttlRatio,
         util::Injected<Observer> observer)
      : mIo(std::move(io))
      , mpInterface(std::move(iface))
      , mTimer(mIo->makeTimer())
      , mLastBroadcastTime{}
      , mTtl(ttl)
      , mTtlRatio(ttlRatio)
      , mObserver(std::move(observer))
    {
      updateAnnouncement(std::move(announcement));
    }

    void updateAnnouncement(Announcement announcement)
    {
      mAnnouncements = {Announcement{
        announcement.nodeId, announcement.sessionId, announcement.peerInfo, {}}};

      for (const auto& channel : announcement.channels.channels)
      {
        auto addedSize = sizeInByteStream(channel);
        if (discovery::sizeInByteStream(mAnnouncements.back().channels.channels)
              + addedSize
            > v1::kMaxPayloadSize)
        {
          mAnnouncements.push_back(Announcement{
            announcement.nodeId, announcement.sessionId, announcement.peerInfo, {}});
        }
        mAnnouncements.back().channels.channels.push_back(channel);
      }
    }

    void broadcastAnnouncement()
    {
      using namespace std::chrono;

      const auto minBroadcastPeriod = milliseconds{50};
      const auto nominalBroadcastPeriod = milliseconds(mTtl * 1000 / mTtlRatio);
      const auto timeSinceLastBroadcast =
        duration_cast<milliseconds>(mTimer.now() - mLastBroadcastTime);

      // The rate is limited to maxBroadcastRate to prevent flooding the network.
      const auto delay = minBroadcastPeriod - timeSinceLastBroadcast;

      // Schedule the next broadcast before we actually send the
      // message so that if sending throws an exception we are still
      // scheduled to try again. We want to keep trying at our
      // interval as long as this instance is alive.
      mTimer.expires_from_now(delay > milliseconds{0} ? delay : nominalBroadcastPeriod);
      mTimer.async_wait(
        [this](const TimerError e)
        {
          if (!e)
          {
            broadcastAnnouncement();
          }
        });

      // // If we're not delaying, broadcast now
      if (delay < milliseconds{1})
      {
        debug(mIo->log()) << "Broadcasting Announcement";
        sendAnnouncement();
      }
    }

    void sendAnnouncement()
    {
      for (auto& receiver : mReceivers)
      {
        try
        {
          for (const auto& announcement : mAnnouncements)
          {
            sendLinkAudioUdpMessage(*mpInterface,
                                    announcement.ident(),
                                    mTtl,
                                    v1::kPeerAnnouncement,
                                    toPayload(announcement),
                                    receiver.endpoint);
          }
        }
        catch (const discovery::UdpSendException&)
        {
        }
      }
      mLastBroadcastTime = mTimer.now();
    }

    void listen() { mpInterface->receive(util::makeAsyncSafe(this->shared_from_this())); }

    template <typename It>
    void operator()(const discovery::UdpEndpoint& from,
                    const It messageBegin,
                    const It messageEnd)
    {
      auto result = v1::parseMessageHeader(messageBegin, messageEnd);

      const auto& header = result.first;
      // Ignore messages from self and other groups
      if (header.ident != mAnnouncements.front().ident() && header.groupId == 0)
      {
        debug(mIo->log()) << "Received message type "
                          << static_cast<int>(header.messageType) << " from peer "
                          << header.ident;

        switch (header.messageType)
        {
        case v1::kPeerAnnouncement:
          receiveAnnouncement(std::move(result.first), result.second, messageEnd, from);
          break;
        default:
          info(mIo->log()) << "Unknown message received of type: " << header.messageType;
        }
      }
      listen();
    }

    template <typename It>
    void receiveAnnouncement(v1::MessageHeader header,
                             It payloadBegin,
                             It payloadEnd,
                             discovery::UdpEndpoint from)
    {
      const auto it =
        std::find_if(mReceivers.begin(),
                     mReceivers.end(),
                     [&](const auto& receiver) { return receiver.endpoint == from; });

      if (it != mReceivers.end())
      {
        try
        {
          auto announcement = Announcement::fromPayload(
            std::move(header.ident), std::move(payloadBegin), std::move(payloadEnd));

          sawAnnouncement(*mObserver, ExtendedAnnouncement{std::move(announcement)});
        }
        catch (const std::runtime_error& err)
        {
          info(mIo->log()) << "Ignoring peer announcement message: " << err.what();
        }
      }
    }

    util::Injected<IoContext> mIo;
    util::Injected<Interface> mpInterface;
    std::vector<Announcement> mAnnouncements;
    Timer mTimer;
    TimePoint mLastBroadcastTime;
    uint8_t mTtl;
    uint8_t mTtlRatio;
    util::Injected<Observer> mObserver;
    std::vector<Receiver> mReceivers;
  };

  std::shared_ptr<Impl> mpImpl;
};

template <typename IoContext>
using MessengerInterface =
  UnicastIpInterface<typename util::Injected<IoContext>::type&, v1::kMaxMessageSize>;

template <typename IoContext>
using MessengerInterfacePtr = std::shared_ptr<MessengerInterface<IoContext>>;
template <typename Observer, typename IoContext>
using MessengerPtr =
  std::shared_ptr<UdpMessenger<MessengerInterfacePtr<IoContext>, Observer, IoContext>>;

// Factory function
template <typename Announcement, typename IoContext, typename Observer>
MessengerPtr<Observer, IoContext> makeMessengerPtr(util::Injected<IoContext> io,
                                                   const discovery::IpAddress& addr,
                                                   util::Injected<Observer> observer,
                                                   Announcement announcement)
{
  const uint8_t ttl = 5;
  const uint8_t ttlRatio = 20;

  MessengerInterfacePtr<IoContext> iface =
    makeSharedUnicastIpInterface<v1::kMaxMessageSize>(util::injectRef(*io), addr);

  return std::make_shared<
    UdpMessenger<MessengerInterfacePtr<IoContext>, Observer, IoContext>>(
    util::injectShared(iface),
    std::move(announcement),
    std::move(io),
    ttl,
    ttlRatio,
    std::move(observer));
}

} // namespace link_audio
} // namespace ableton
