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

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/link/NodeId.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/link_audio/Source.hpp>
#include <ableton/link_audio/SourceProcessor.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("SourceProcessor")
{
  using IoContext = test::serial_io::Context;

  // Decodes every sent channel request message
  struct Message
  {
    v1::MessageHeader header;
    Id channelId;
  };

  struct GetSender
  {
    struct SendHandler
    {
      void operator()(const uint8_t* pData, const size_t numBytes)
      {
        const auto end = pData + numBytes;
        const auto [header, payloadBegin] = v1::parseMessageHeader(pData, end);
        auto channelId = Id{};
        if (header.messageType == v1::kChannelRequest)
        {
          channelId = ChannelRequest::fromPayload(Id{}, payloadBegin, end).channelId;
        }
        else if (header.messageType == v1::kStopChannelRequest)
        {
          channelId = ChannelStopRequest::fromPayload(Id{}, payloadBegin, end).channelId;
        }
        pMessages->push_back({header, channelId});
      }

      std::vector<Message>* pMessages;
    };

    std::optional<SendHandler> forChannel(const Id&)
    {
      if (hasSender)
      {
        return SendHandler{&messages};
      }
      return std::nullopt;
    }

    std::vector<Message> messages;
    bool hasSender = true;
  };

  struct GetNodeId
  {
    const link::NodeId& operator()() const { return nodeId; }
    link::NodeId nodeId = link::NodeId::random<platforms::stl::Random>();
  };

  using Processor = SourceProcessor<GetSender&, GetNodeId, IoContext&>;

  // Records every buffer delivered to the source callback
  struct Received
  {
    Buffer<int16_t> buffer{0};
    std::vector<int16_t> samples;
  };

  const auto sourceId = Id::random<platforms::stl::Random>();
  const auto sessionId = Id::random<platforms::stl::Random>();
  const auto nodeId = GetNodeId{};

  test::serial_io::Fixture fixture;
  auto io = fixture.makeIoContext();
  GetSender sender;
  std::vector<Received> received;
  auto pSource = std::make_shared<Source>(
    sourceId,
    [&](BufferCallbackHandle<Buffer<int16_t>> handle)
    {
      const auto numSamples = handle.mBuffer.mNumFrames * handle.mBuffer.mNumChannels;
      received.push_back(
        {handle.mBuffer, {handle.mpSamples, handle.mpSamples + numSamples}});
    });

  const auto makeProcessor = [&]
  {
    return std::make_unique<Processor>(
      util::injectRef(io), pSource, util::injectRef(sender), util::injectVal(nodeId));
  };

  const auto checkRequest =
    [&](const Message& message, const v1::MessageType messageType, const uint8_t ttl)
  {
    CHECK(message.header.messageType == messageType);
    CHECK(message.header.ttl == ttl);
    CHECK(message.header.ident == nodeId.nodeId);
    CHECK(message.channelId == sourceId);
  };

  SECTION("Requests the channel on construction")
  {
    const auto pProcessor = makeProcessor();

    REQUIRE(sender.messages.size() == 1);
    checkRequest(sender.messages[0], v1::kChannelRequest, Processor::kTtl);
  }

  SECTION("Repeats the channel request every ttl")
  {
    const auto pProcessor = makeProcessor();
    REQUIRE(sender.messages.size() == 1);

    fixture.advanceTime(std::chrono::seconds(Processor::kTtl) - std::chrono::seconds(1));
    CHECK(sender.messages.size() == 1);

    fixture.advanceTime(std::chrono::seconds(1));
    REQUIRE(sender.messages.size() == 2);
    checkRequest(sender.messages[1], v1::kChannelRequest, Processor::kTtl);

    fixture.advanceTime(std::chrono::seconds(Processor::kTtl));
    REQUIRE(sender.messages.size() == 3);
    checkRequest(sender.messages[2], v1::kChannelRequest, Processor::kTtl);
  }

  SECTION("Sends a stop request on destruction and stops requesting")
  {
    auto pProcessor = makeProcessor();
    REQUIRE(sender.messages.size() == 1);

    pProcessor.reset();
    REQUIRE(sender.messages.size() == 2);
    checkRequest(sender.messages[1], v1::kStopChannelRequest, 0);

    fixture.advanceTime(std::chrono::seconds(2 * Processor::kTtl));
    CHECK(sender.messages.size() == 2);
  }

  SECTION("Runs while the API holds the source")
  {
    const auto pProcessor = makeProcessor();
    CHECK(pProcessor->process());
    CHECK(pProcessor->id() == sourceId);

    pSource.reset();
    CHECK(!pProcessor->process());
  }

  SECTION("Works without a sender for the channel")
  {
    sender.hasSender = false;
    auto pProcessor = makeProcessor();
    CHECK(sender.messages.empty());

    fixture.advanceTime(std::chrono::seconds(Processor::kTtl));
    pProcessor.reset();
    CHECK(sender.messages.empty());
  }

  SECTION("Delivers decoded audio buffers chunk by chunk to the source")
  {
    const auto pProcessor = makeProcessor();

    auto audio = AudioBuffer{};
    audio.channelId = sourceId;
    audio.sessionId = sessionId;
    audio.codec = Codec::kPCM_i16;
    audio.sampleRate = 48000;
    audio.numChannels = 2;
    audio.chunks = {AudioBuffer::Chunk{7, 4, link::Beats{1.}, link::Tempo{120.}},
                    AudioBuffer::Chunk{8, 2, link::Beats{1.5}, link::Tempo{100.}}};

    const auto numSamples = audio.numFrames() * audio.numChannels;
    auto it = audio.bytes.begin();
    for (auto sample = 0u; sample < numSamples; ++sample)
    {
      it = discovery::toNetworkByteStream(static_cast<int16_t>(sample + 1), it);
    }
    audio.numBytes = static_cast<uint16_t>(std::distance(audio.bytes.begin(), it));

    pProcessor->receiveAudioBuffer(audio);

    REQUIRE(received.size() == 2);
    for (auto i = 0u; i < received.size(); ++i)
    {
      const auto& chunk = audio.chunks[i];
      const auto& buffer = received[i].buffer;
      CHECK(buffer.mNumFrames == chunk.numFrames);
      CHECK(buffer.mNumChannels == audio.numChannels);
      CHECK(buffer.mSampleRate == audio.sampleRate);
      CHECK(buffer.mBeginBeats == chunk.beginBeats);
      CHECK(buffer.mTempo == chunk.tempo);
      CHECK(buffer.mCount == chunk.count);
      CHECK(buffer.mSessionId == sessionId);
    }
    CHECK(received[0].samples == std::vector<int16_t>{1, 2, 3, 4, 5, 6, 7, 8});
    CHECK(received[1].samples == std::vector<int16_t>{9, 10, 11, 12});
  }
}

} // namespace link_audio
} // namespace ableton
