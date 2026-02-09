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

#include <ableton/link_audio/Receivers.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>
#include <optional>

namespace ableton
{
namespace link_audio
{

TEST_CASE("Receivers")
{
  auto id = Id{{{0, 0, 0, 0, 0, 0, 0, 0}}};
  auto id1 = Id{{{0, 0, 0, 0, 0, 0, 0, 1}}};
  auto id2 = Id{{{0, 0, 0, 0, 0, 0, 0, 2}}};

  static size_t numSendCalls = 0;

  struct SendHandler
  {
    void operator()(const uint8_t*, size_t) { ++numSendCalls; }

    Id peerId;
  };

  struct GetSender
  {
    std::optional<SendHandler> forPeer(const Id& id)
    {
      if (auto it = mSendHandlers.find(id); it != mSendHandlers.end())
      {
        return it->second;
      }
      return std::nullopt;
    }

    std::optional<SendHandler> forChannel(const Id& id)
    {
      if (auto it = mSendHandlers.find(id); it != mSendHandlers.end())
      {
        return it->second;
      }
      return std::nullopt;
    }

    std::map<Id, SendHandler> mSendHandlers;
  };

  auto getSender = GetSender{};

  test::serial_io::Fixture io;

  auto receivers = Receivers<GetSender&, test::serial_io::Context>(
    util::injectVal(io.makeIoContext()), util::injectRef(getSender));

  CHECK(receivers.empty());
  CHECK(0 == numSendCalls);

  SECTION("ReceiveChannelRequest")
  {
    getSender.mSendHandlers[id1] = SendHandler{id};
    receivers.receiveChannelRequest(ChannelRequest{id1, id}, 10);
    CHECK(!receivers.empty());
    receivers(nullptr, 0);
    CHECK(1 == numSendCalls);
    numSendCalls = 0;

    SECTION("Second Request")
    {
      io.advanceTime(std::chrono::seconds(5));

      getSender.mSendHandlers[id2] = SendHandler{id};
      receivers.receiveChannelRequest(ChannelRequest{id2, id}, 10);
      receivers(nullptr, 0);
      CHECK(2 == numSendCalls);
      numSendCalls = 0;
    }

    SECTION("ReceiveAudioStopRequest")
    {
      auto stopRequest = ChannelStopRequest{id1, id};
      receivers.receiveChannelRequest(stopRequest, 10);

      CHECK(receivers.empty());
    }

    SECTION("PruneExpiredReceivers")
    {
      io.advanceTime(std::chrono::seconds(11));
      CHECK(receivers.empty());
    }
  }
}

} // namespace link_audio
} // namespace ableton
