
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

#include <ableton/discovery/AsioTypes.hpp>
#include <ableton/link_audio/ChannelAnnouncements.hpp>
#include <ableton/link_audio/Channels.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/PeerAnnouncement.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("Channels")
{
  struct Callback
  {
    void operator()() { ++mNumCalls; }
    size_t mNumCalls = 0;
  };

  struct Input
  {
    Id ident() const { return announcement.ident(); }

    PeerAnnouncement announcement;
    double networkQuality;
    std::shared_ptr<int> pInterface;
    discovery::UdpEndpoint from;
    int ttl;
  };

  using Random = ableton::platforms::stl::Random;
  using IoContext = test::serial_io::Context;
  using TestChannels = Channels<IoContext, std::reference_wrapper<Callback>, int>;
  using Channel = TestChannels::Channel;

  auto checkChannel = [](const Input& test, const std::vector<Channel>& channels)
  {
    for (const auto& channel : test.announcement.channels.channels)
    {
      const auto expected = Channel{channel.name,
                                    channel.id,
                                    test.announcement.peerInfo.name,
                                    test.announcement.nodeId,
                                    test.announcement.sessionId};

      CHECK(std::find_if(channels.begin(),
                         channels.end(),
                         [&](const auto& c) { return expected == c; })
            != channels.end());
    }
  };

  const auto sessionId = Id::random<Random>();
  const auto foo = Input{{Id::random<Random>(),
                          sessionId,
                          {"foo"},
                          {{ChannelAnnouncement{{"fooChannel"}, Id::random<Random>()}}}},
                         100.,
                         {},
                         {discovery::makeAddress("1.1.1.1"), 1},
                         2};

  const auto bar = Input{{Id::random<Random>(),
                          sessionId,
                          {"bar"},
                          {{ChannelAnnouncement{{"barChannel"}, Id::random<Random>()}}}},
                         {},
                         {},
                         {},
                         5};

  const auto gateway1 = discovery::makeAddress("123.123.123.123");
  const auto gateway2 = discovery::makeAddress("210.210.210.210");

  auto callback = Callback{};
  test::serial_io::Fixture io;

  auto channels = TestChannels(util::injectVal(io.makeIoContext()), std::ref(callback));

  SECTION("EmptyChannelsAfterInit")
  {
    CHECK(0 == callback.mNumCalls);
  }

  SECTION("AddChannel")
  {
    auto observer = makeGatewayObserver(channels, gateway1);

    sawAnnouncement(observer, foo);

    CHECK(1 == callback.mNumCalls);

    SECTION("UniqueChannels")
    {
      const auto uniqueChannels =
        channels.uniqueSessionChannels(foo.announcement.sessionId);

      CHECK(1 == uniqueChannels.size());
      checkChannel(foo, uniqueChannels);

      SECTION("GetSendHandler")
      {
        const auto handler =
          channels.channelSendHandler(foo.announcement.channels.channels[0].id);
        CHECK(handler.has_value());
        CHECK(foo.from == handler->endpoint());

        SECTION("GetSendHandlerForBestConnection")
        {
          auto fasterFoo = foo;
          fasterFoo.networkQuality = foo.networkQuality * 2;
          const auto expectedSendHandler =
            TestChannels::SendHandler({discovery::makeAddress("3.3.3.3"), 3333}, {});
          fasterFoo.from = expectedSendHandler.endpoint();
          auto observer2 = makeGatewayObserver(channels, gateway2);
          sawAnnouncement(observer2, fasterFoo);
          CHECK(2 == channels.sessionChannels(sessionId).size());
          CHECK(1 == channels.uniqueSessionChannels(sessionId).size());
          auto sendHandler =
            channels.channelSendHandler(foo.announcement.channels.channels[0].id);
          CHECK(sendHandler.has_value());
          CHECK(expectedSendHandler.endpoint() == sendHandler->endpoint());
        }
      }
    }

    SECTION("ReAddChannelWithChangedPeerId")
    {
      auto changedFoo = foo;
      changedFoo.announcement.nodeId = Id::random<Random>();
      sawAnnouncement(observer, changedFoo);

      SECTION("GetSendHandler")
      {
        const auto handler = channels.peerSendHandler(changedFoo.announcement.nodeId);
        CHECK(handler.has_value());
        CHECK(foo.from == handler->endpoint());
      }

      SECTION("GetSendHandlerWithUnknownChannelId")
      {
        const auto handler = channels.channelSendHandler(Id::random<Random>());
        CHECK(!handler.has_value());
      }
    }

    SECTION("UniqueChannelsWithUnknownSessionId")
    {
      const auto uniqueChannels = channels.uniqueSessionChannels(Id::random<Random>());

      CHECK(0 == uniqueChannels.size());
    }

    SECTION("RemoveChannel")
    {
      auto byes = std::vector<Id>{foo.announcement.channels.channels[0].id};
      channelsLeft(observer, begin(byes), end(byes));

      const auto uniqueChannels =
        channels.uniqueSessionChannels(foo.announcement.sessionId);

      CHECK(2 == callback.mNumCalls);
      CHECK(0 == uniqueChannels.size());
    }

    SECTION("AddSecondPeer")
    {
      sawAnnouncement(observer, bar);

      auto uniqueChannels = channels.uniqueSessionChannels(sessionId);

      CHECK(2 == uniqueChannels.size());
      checkChannel(foo, uniqueChannels);
      checkChannel(bar, uniqueChannels);
    }

    SECTION("AddSecondChannel")
    {
      auto observer2 = makeGatewayObserver(channels, gateway2);
      sawAnnouncement(observer2, foo);

      const auto uniqueChannels = channels.uniqueSessionChannels(sessionId);

      CHECK(1 == uniqueChannels.size());
      checkChannel(foo, uniqueChannels);
    }

    SECTION("Timeout")
    {
      io.advanceTime(std::chrono::seconds(5));

      const auto uniqueChannels = channels.uniqueSessionChannels(sessionId);

      CHECK(2 == callback.mNumCalls);
      CHECK(0 == uniqueChannels.size());
    }
  }
}

} // namespace link_audio
} // namespace ableton
