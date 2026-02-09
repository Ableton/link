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

#include <ableton/discovery/test/Interface.hpp>
#include <ableton/discovery/test/PayloadEntries.hpp>
#include <ableton/link/PayloadEntries.hpp>
#include <ableton/link_audio/ChannelAnnouncements.hpp>
#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/link_audio/PeerAnnouncement.hpp>
#include <ableton/link_audio/UdpMessenger.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ableton
{
namespace link_audio
{
namespace
{

struct TestHandler;
struct TestObserver;

using Context = test::serial_io::Context;
using SharedInterface = std::shared_ptr<discovery::test::Interface>;
using TestMessenger =
  UdpMessenger<TestHandler&, discovery::test::Interface, TestObserver&, Context>;
using Random = ableton::platforms::stl::Random;

constexpr auto kTtl = 5;
constexpr auto kTtlRatio = 20;

struct TestObserver
{
  using GatewayObserverAnnouncement = PeerAnnouncement;
  using GatewayObserverNodeId = link::NodeId;

  template <typename Announcement>
  friend void sawAnnouncement(TestObserver& observer, Announcement announcement)
  {
    observer.announcements.push_back(std::move(announcement));
  }

  template <typename It>
  friend void channelsLeft(TestObserver& observer, It begin, It end)
  {
    observer.channelByes.emplace_back(begin, end);
  }

  std::vector<TestMessenger::ExtendedAnnouncement> announcements;
  std::vector<std::vector<link::NodeId>> channelByes;
};

struct TestHandler
{
  void receiveChannelRequest(ChannelRequest request, uint8_t ttl)
  {
    channelRequests.push_back({std::move(request), ttl});
  }

  void receiveChannelRequest(ChannelStopRequest request, uint8_t ttl)
  {
    channelStopRequests.push_back({std::move(request), ttl});
  }

  template <typename It>
  void receiveAudioBuffer(It, It)
  {
    ++audioBufferCallsCount;
  }

  std::vector<std::pair<ChannelRequest, uint8_t>> channelRequests;
  std::vector<std::pair<ChannelStopRequest, uint8_t>> channelStopRequests;
  size_t audioBufferCallsCount = 0u;
};

PeerAnnouncement makePeerAnnouncement(link::NodeId nodeId,
                                      link::SessionId sessionId,
                                      std::uint32_t numChannels)
{
  auto announcement = PeerAnnouncement{nodeId, sessionId, {"TestPeer"}, {}};

  for (auto i = 0u; i < numChannels; ++i)
  {
    announcement.channels.channels.push_back(
      {"Channel_" + std::to_string(i), Id::random<Random>()});
  }

  return announcement;
}

template <v1::MessageType MessageType>
size_t sentMessagesCount(SharedInterface pIface)
{
  return static_cast<size_t>(std::count_if(pIface->sentMessages.begin(),
                                           pIface->sentMessages.end(),
                                           [](const auto& sentMessage)
                                           {
                                             auto [header, _] = v1::parseMessageHeader(
                                               begin(sentMessage.first),
                                               end(sentMessage.first));
                                             return header.messageType == MessageType;
                                           }));
}

template <typename T, typename It>
std::optional<T> parseMessageBuffer(It begin, It end)
{
  std::optional<T> oResult = std::nullopt;
  discovery::parsePayload<T>(begin, end, [&](const T& r) { oResult = r; });
  return oResult;
}

struct MessengerWrapper
{
  MessengerWrapper(TestMessenger messenger)
    : mMessenger(std::move(messenger))
  {
  }

  TestMessenger mMessenger;
};

} // namespace

TEST_CASE("UdpMessenger")
{
  const auto sessionId = Id::random<Random>();
  const auto localId = Id::random<Random>();
  const auto peerId = Id::random<Random>();
  const auto peerEndpoint =
    discovery::UdpEndpoint{discovery::makeAddress("123.123.234.234"), 1900};

  ::ableton::test::serial_io::Fixture io;
  auto pIface = std::make_shared<discovery::test::Interface>(
    discovery::UdpEndpoint{discovery::makeAddress("123.123.234.42"), 1234});
  TestObserver observer;
  TestHandler handler;

  auto makeTestMessenger = [&](auto announcement)
  {
    return TestMessenger(util::injectRef(handler),
                         pIface,
                         announcement,
                         util::injectVal(io.makeIoContext()),
                         kTtl,
                         kTtlRatio,
                         util::injectRef(observer));
  };

  auto receiveMessage = [&](auto messageType, auto payload)
  {
    v1::MessageBuffer buffer;
    const auto messageEnd = v1::detail::encodeMessage(
      peerId, kTtl, messageType, std::move(payload), begin(buffer));
    pIface->incomingMessage(peerEndpoint, begin(buffer), messageEnd);
  };

  const auto kAnnouncement = makePeerAnnouncement(localId, sessionId, 3);
  auto messenger = makeTestMessenger(kAnnouncement);

  SECTION("ConstructionDoesNotBroadcastWithoutReceivers")
  {
    CHECK(0 == pIface->sentMessages.size());
  }

  SECTION("BroadcastAfterAddingReceiver")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    io.advanceTime(std::chrono::milliseconds(260));

    REQUIRE(1 == pIface->sentMessages.size());
    const auto& [messageBuffer, sentTo] = pIface->sentMessages[0];
    CHECK(peerEndpoint == sentTo);

    auto [header, payloadBegin] =
      v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));
    CHECK(v1::kPeerAnnouncement == header.messageType);
    CHECK(kAnnouncement.ident() == header.ident);

    // Verify that payload contains a ping
    const auto oHostTime =
      parseMessageBuffer<link::HostTime>(payloadBegin, messageBuffer.end());
    CHECK(oHostTime.has_value());

    // Verify announcement
    const auto receivedAnnouncement =
      PeerAnnouncement::fromPayload(header.ident, payloadBegin, messageBuffer.end());
    CHECK(kAnnouncement == receivedAnnouncement);
  }

  SECTION("SendByeByeOnDestruction")
  {
    auto announcement = makePeerAnnouncement(localId, sessionId, 2);
    {
      auto messenger2 = makeTestMessenger(announcement);
      messenger2.sawLinkAudioEndpoint(peerId, peerEndpoint);
      io.advanceTime(std::chrono::milliseconds(260));
      pIface->sentMessages.clear();
    }

    CHECK(pIface->sentMessages.size() == 1);

    const auto& [messageBuffer, sentTo] = pIface->sentMessages[0];
    auto [header, payloadBegin] =
      v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));

    CHECK(v1::kChannelByes == header.messageType);
    CHECK(peerEndpoint == sentTo);

    const auto oReceivedByes =
      parseMessageBuffer<ChannelByes>(payloadBegin, messageBuffer.end());
    REQUIRE(oReceivedByes.has_value());
    CHECK(2 == oReceivedByes->byes.size());
    CHECK(announcement.channels.channels[0].id == oReceivedByes->byes[0].id);
    CHECK(announcement.channels.channels[1].id == oReceivedByes->byes[1].id);
  }

  SECTION("DestructorWithoutReceiversSendsNothing")
  {
    {
      auto messenger2 = makeTestMessenger(makePeerAnnouncement(localId, sessionId, 2));
    }

    CHECK(0 == pIface->sentMessages.size());
  }

  SECTION("MovingMessengerDoesntSendByeBye")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    const auto wrapper = MessengerWrapper(std::move(messenger));

    CHECK(0 == sentMessagesCount<v1::kChannelByes>(pIface));
  }

  SECTION("AnnouncementSplitting")
  {
    const auto numChannels = 60;
    const auto announcement = makePeerAnnouncement(localId, sessionId, numChannels);
    messenger.updateAnnouncement(announcement);

    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    io.advanceTime(std::chrono::milliseconds(260));

    CHECK(sentMessagesCount<v1::kPeerAnnouncement>(pIface) > 1);

    // Verify first message contains ping
    SECTION("FirstMessageContainsPing")
    {
      for (auto i = 0u; i < pIface->sentMessages.size(); ++i)
      {
        const auto& [messageBuffer, _] = pIface->sentMessages[i];
        auto [header, payloadBegin] =
          v1::parseMessageHeader(messageBuffer.begin(), messageBuffer.end());

        REQUIRE(v1::kPeerAnnouncement == header.messageType);
        const auto oHostTime =
          parseMessageBuffer<link::HostTime>(payloadBegin, messageBuffer.end());

        if (i == 0)
        {
          CHECK(oHostTime.has_value());
        }
        else
        {
          CHECK_FALSE(oHostTime.has_value());
        }
      }
    }

    SECTION("AllChannelsAnnounced")
    {
      auto channelIndex = 0u;
      for (const auto& [messageBuffer, _] : pIface->sentMessages)
      {
        auto [header, payloadBegin] =
          v1::parseMessageHeader(messageBuffer.begin(), messageBuffer.end());
        REQUIRE(header.messageType == v1::kPeerAnnouncement);
        CHECK(announcement.ident() == header.ident);

        auto receivedAnnouncement =
          PeerAnnouncement::fromPayload(header.ident, payloadBegin, messageBuffer.end());
        CHECK(announcement.sessionId == receivedAnnouncement.sessionId);

        for (const auto& channel : receivedAnnouncement.channels.channels)
        {
          CHECK(channel.id == announcement.channels.channels[channelIndex].id);
          ++channelIndex;
        }
      }
      CHECK(channelIndex == numChannels);
    }
  }

  SECTION("SawLinkAudioEndpointAddsReceiver")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    pIface->sentMessages.clear();
    io.advanceTime(std::chrono::milliseconds(260));

    const auto messageCount =
      std::count_if(pIface->sentMessages.begin(),
                    pIface->sentMessages.end(),
                    [&peerEndpoint](const auto& e) { return e.second == peerEndpoint; });
    CHECK(1 == messageCount);
  }

  SECTION("SawLinkAudioEndpointDuplicateIgnored")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    pIface->sentMessages.clear();
    io.advanceTime(std::chrono::milliseconds(260));

    const auto messageCount = std::count_if(pIface->sentMessages.begin(),
                                            pIface->sentMessages.end(),
                                            [&peerEndpoint](const auto& msg)
                                            { return msg.second == peerEndpoint; });
    CHECK(1 == messageCount);
  }

  SECTION("SawLinkAudioEndpointRemovesReceiver")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    messenger.sawLinkAudioEndpoint(peerId, std::nullopt);

    pIface->sentMessages.clear();
    io.advanceTime(std::chrono::milliseconds(260));

    const auto messageCount = std::count_if(pIface->sentMessages.begin(),
                                            pIface->sentMessages.end(),
                                            [&peerEndpoint](const auto& msg)
                                            { return msg.second == peerEndpoint; });

    CHECK(0 == messageCount);
  }

  SECTION("PruneChannelsEndpoints")
  {
    const auto peer2Id = Id::random<Random>();
    const auto peer2Endpoint =
      discovery::UdpEndpoint{discovery::makeAddress("123.123.234.235"), 1901};

    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    messenger.sawLinkAudioEndpoint(peer2Id, peer2Endpoint);

    std::vector<link::NodeId> peersToKeep = {peerId};
    messenger.pruneChannelsEndpoints(peersToKeep.begin(), peersToKeep.end());

    pIface->sentMessages.clear();
    io.advanceTime(std::chrono::milliseconds(260));

    REQUIRE(1 == pIface->sentMessages.size());
    CHECK(pIface->sentMessages[0].second == peerEndpoint);
  }

  SECTION("ReceivePingAndSendPong")
  {
    const auto pingTime = link::HostTime{std::chrono::microseconds{123456}};
    receiveMessage(v1::kPeerAnnouncement,
                   toPayload(makePeerAnnouncement(peerId, sessionId, 1))
                     + discovery::makePayload(pingTime));

    CHECK(1 == pIface->sentMessages.size());

    const auto [messageBuffer, sentTo] = pIface->sentMessages[0];
    CHECK(sentTo == peerEndpoint);

    auto [header, payloadBegin] =
      v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));
    CHECK(header.messageType == v1::kPong);

    const auto oPongTime =
      parseMessageBuffer<link::HostTime>(payloadBegin, messageBuffer.end());
    REQUIRE(oPongTime.has_value());
    CHECK(pingTime.time == oPongTime->time);
  }

  SECTION("ReceiveAnnouncementWithoutPingDoesNotSendPong")
  {
    receiveMessage(
      v1::kPeerAnnouncement, toPayload(makePeerAnnouncement(peerId, sessionId, 1)));

    // Should not respond with pong
    CHECK(0 == pIface->sentMessages.size());
  }

  SECTION("ReceivePongUpdatesMetrics")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    receiveMessage(
      v1::kPeerAnnouncement, toPayload(makePeerAnnouncement(peerId, sessionId, 1)));
    CHECK(1 == observer.announcements.size());
    CHECK(0.0 == observer.announcements[0].networkQuality);

    // Send announcement (with ping)
    pIface->sentMessages.clear();
    io.advanceTime(std::chrono::milliseconds(260));

    // Find the ping time from sent message
    REQUIRE(1 == pIface->sentMessages.size());
    const auto [messageBuffer, sentTo] = pIface->sentMessages[0];
    auto [header, payloadBegin] =
      v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));
    REQUIRE(header.messageType == v1::kPeerAnnouncement);
    const auto oPingTime =
      parseMessageBuffer<link::HostTime>(payloadBegin, messageBuffer.end());
    REQUIRE(oPingTime.has_value());

    // receive pong with ping time
    io.advanceTime(std::chrono::milliseconds(60));
    receiveMessage(v1::kPong, discovery::makePayload(*oPingTime));

    // Receive announcement to trigger metrics update
    receiveMessage(
      v1::kPeerAnnouncement, toPayload(makePeerAnnouncement(peerId, sessionId, 1)));
    CHECK(2 == observer.announcements.size());
    CHECK(observer.announcements[0].networkQuality
          < observer.announcements[1].networkQuality);
  }

  SECTION("ReceiveAnnouncementFromKnownReceiver")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    observer.announcements.clear();
    const auto receivedAnnouncement = makePeerAnnouncement(peerId, sessionId, 2);
    receiveMessage(v1::kPeerAnnouncement, toPayload(receivedAnnouncement));

    REQUIRE(1 == observer.announcements.size());
    CHECK(receivedAnnouncement == observer.announcements[0].announcement);
    CHECK(peerEndpoint == observer.announcements[0].from);
    CHECK(pIface->endpoint() == observer.announcements[0].pInterface->endpoint());
    CHECK(5 == observer.announcements[0].ttl);
    CHECK(0.0 == observer.announcements[0].networkQuality);
  }

  SECTION("ReceiveMessageFromUnknownReceiverIgnored")
  {
    receiveMessage(
      v1::kPeerAnnouncement, toPayload(makePeerAnnouncement(peerId, sessionId, 2)));

    CHECK(0 == observer.announcements.size());
  }

  SECTION("ReceiveChannelByes")
  {
    const auto expectedByes =
      ChannelByes{{{Id::random<Random>()}, {Id::random<Random>()}}};

    observer.channelByes.clear();
    receiveMessage(v1::kChannelByes, discovery::makePayload(expectedByes));
    REQUIRE(1 == observer.channelByes.size());
    CHECK(expectedByes.byes[0].id == observer.channelByes[0][0]);
    CHECK(expectedByes.byes[1].id == observer.channelByes[0][1]);
  }

  SECTION("ReceiveChannelRequest")
  {
    handler.channelRequests.clear();
    const auto receivedRequest = ChannelRequest{peerId, Id::random<Random>()};
    receiveMessage(v1::kChannelRequest, toPayload(receivedRequest));

    REQUIRE(1 == handler.channelRequests.size());
    CHECK(receivedRequest == handler.channelRequests[0].first);
    CHECK(kTtl == handler.channelRequests[0].second);
  }

  SECTION("ReceiveChannelStopRequest")
  {
    handler.channelStopRequests.clear();
    const auto receivedRequest = ChannelStopRequest{peerId, Id::random<Random>()};
    receiveMessage(v1::kStopChannelRequest, toPayload(receivedRequest));

    REQUIRE(1 == handler.channelStopRequests.size());
    CHECK(receivedRequest == handler.channelStopRequests[0].first);
    CHECK(kTtl == handler.channelStopRequests[0].second);
  }

  SECTION("ReceiveAudioBuffer")
  {
    receiveMessage(v1::kAudioBuffer, discovery::makePayload());

    CHECK(1 == handler.audioBufferCallsCount);
  }

  SECTION("IgnoreMessageFromSelf")
  {
    v1::MessageBuffer buffer;
    const auto messageEnd = v1::detail::encodeMessage(
      kAnnouncement.ident(),
      kTtl,
      v1::kPeerAnnouncement,
      toPayload(makePeerAnnouncement(kAnnouncement.ident(), kAnnouncement.sessionId, 1)),
      begin(buffer));
    pIface->incomingMessage(peerEndpoint, begin(buffer), messageEnd);

    CHECK(0 == observer.announcements.size());
  }

  SECTION("UpdateAnnouncementSendsByesForRemovedChannels")
  {
    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);

    pIface->sentMessages.clear();
    auto updatedAnnouncement = kAnnouncement;
    updatedAnnouncement.channels =
      ChannelAnnouncements{{kAnnouncement.channels.channels[0]}};
    messenger.updateAnnouncement(updatedAnnouncement);

    REQUIRE(1 == pIface->sentMessages.size());
    const auto& sentMessage = pIface->sentMessages[0].first;
    auto [header, payloadBegin] =
      v1::parseMessageHeader(sentMessage.begin(), sentMessage.end());
    REQUIRE(v1::kChannelByes == header.messageType);
    const auto oChannelByes =
      parseMessageBuffer<ChannelByes>(payloadBegin, sentMessage.end());
    REQUIRE(oChannelByes.has_value());
    REQUIRE(2 == oChannelByes->byes.size());

    CHECK(kAnnouncement.channels.channels[1].id == oChannelByes->byes[0].id);
    CHECK(kAnnouncement.channels.channels[2].id == oChannelByes->byes[1].id);
  }

  SECTION("ChannelByeSplitting")
  {
    const auto numChannels = 200;
    auto announcement = makePeerAnnouncement(localId, sessionId, numChannels);
    messenger.updateAnnouncement(announcement);

    messenger.sawLinkAudioEndpoint(peerId, peerEndpoint);
    pIface->sentMessages.clear();

    // Update with announcement that has all channels removed
    auto emptyAnnouncement =
      makePeerAnnouncement(announcement.nodeId, announcement.sessionId, 0);
    messenger.updateAnnouncement(emptyAnnouncement);

    CHECK(pIface->sentMessages.size() > 1);
    size_t numberOfByes = 0;
    for (const auto& [messageBuffer, _] : pIface->sentMessages)
    {
      auto [header, payloadBegin] =
        v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));
      CHECK(header.messageType == v1::kChannelByes);
      const auto oChannelByes =
        parseMessageBuffer<ChannelByes>(payloadBegin, messageBuffer.end());
      REQUIRE(oChannelByes.has_value());
      numberOfByes += oChannelByes->byes.size();
    }
    CHECK(numChannels == numberOfByes);
  }
}

} // namespace link_audio
} // namespace ableton
