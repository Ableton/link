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

#include <ableton/link/NodeId.hpp>
#include <ableton/link/Timeline.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/BeatTimeMapping.hpp>
#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/link_audio/Sink.hpp>
#include <ableton/link_audio/SinkProcessor.hpp>
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

TEST_CASE("SinkProcessor")
{
  using IoContext = test::serial_io::Context;

  // Decodes every sent audio buffer message and records its chunks
  struct GetSender
  {
    struct SendHandler
    {
      void operator()(const uint8_t* pData, const size_t numBytes)
      {
        const auto end = pData + numBytes;
        const auto payloadBegin = v1::parseMessageHeader(pData, end).second;
        auto buffer = AudioBuffer{};
        AudioBuffer::fromNetworkByteStream(buffer, payloadBegin, end);
        pChunks->insert(pChunks->end(), buffer.chunks.begin(), buffer.chunks.end());
      }

      AudioBuffer::Chunks* pChunks;
    };

    std::optional<SendHandler> forPeer(const Id&) { return SendHandler{&chunks}; }
    std::optional<SendHandler> forChannel(const Id&) { return SendHandler{&chunks}; }

    AudioBuffer::Chunks chunks;
  };

  struct GetNodeId
  {
    const link::NodeId& operator()() const { return nodeId; }
    link::NodeId nodeId = link::NodeId::random<platforms::stl::Random>();
  };

  constexpr auto kNumChannels = 2u;
  constexpr auto kSampleRate = 44100u;
  constexpr auto kQuantum = 4.;
  // A packet holds Encoder::kMaxAudioBytes / (kNumChannels * sizeof(int16_t)) = 125
  // stereo frames, so committing this many frames flushes at least one packet.
  constexpr auto kNumFrames = 128u;
  constexpr auto kMaxNumSamples = kNumFrames * kNumChannels;

  const auto sinkId = Id::random<platforms::stl::Random>();
  const auto peerId = Id::random<platforms::stl::Random>();
  const auto sessionId = Id::random<platforms::stl::Random>();
  const auto timeline =
    link::Timeline{link::Tempo{120.}, link::Beats{0.}, std::chrono::microseconds{0}};

  test::serial_io::Fixture fixture;
  auto io = fixture.makeIoContext();
  GetSender sender;
  auto pSink = std::make_shared<Sink>(std::string{"sink"}, kMaxNumSamples, sinkId);
  auto processor = SinkProcessor<GetSender&, GetNodeId, IoContext&>(
    util::injectRef(io), pSink, util::injectRef(sender), util::injectVal(GetNodeId{}));

  const auto commitBuffer = [&](const double beats, const size_t numFrames)
  {
    auto* pBuffer = pSink->retainBuffer();
    REQUIRE(pBuffer != nullptr);
    REQUIRE(pBuffer->mSamples.size() >= numFrames * kNumChannels);
    pSink->releaseAndCommitBuffer(
      timeline, sessionId, beats, kQuantum, numFrames, kNumChannels, kSampleRate);
  };

  SECTION("No buffer is available without a connected receiver")
  {
    CHECK(pSink->retainBuffer() == nullptr);
    CHECK(processor.process());
    CHECK(sender.chunks.empty());
  }

  processor.receiveChannelRequest(ChannelRequest{peerId, sinkId}, 5);

  SECTION("Committed buffers are sent")
  {
    commitBuffer(1., kNumFrames);
    CHECK(processor.process());

    REQUIRE(sender.chunks.size() == 1);
    CHECK(sender.chunks[0].beginBeats
          == globalBeatAtBeat(timeline, link::Beats{1.}, link::Beats{kQuantum}));
    CHECK(sender.chunks[0].tempo == timeline.tempo);
  }

  SECTION("Growing the max num samples does not resend stale buffers")
  {
    commitBuffer(1., kNumFrames);
    CHECK(processor.process());
    REQUIRE(sender.chunks.size() == 1);
    const auto staleBeats = sender.chunks[0].beginBeats;

    pSink->requestMaxNumSamples(2 * kMaxNumSamples);
    CHECK(pSink->maxNumSamples() == 2 * kMaxNumSamples);

    // Every queued slot is undersized. The writer skips all slots it can reach and
    // hands them to the processor to be resized. The slot that was just sent sits at
    // the reader's edge and is only reached and skipped on the next pass. Skipped slots
    // still hold their old samples and timing, none of them must be sent again.
    CHECK(pSink->retainBuffer() == nullptr);
    CHECK(processor.process());
    CHECK(sender.chunks.size() == 1);

    commitBuffer(2., 2 * kNumFrames);
    CHECK(processor.process());
    REQUIRE(sender.chunks.size() > 1);
    for (auto it = sender.chunks.begin() + 1; it != sender.chunks.end(); ++it)
    {
      CHECK(it->beginBeats != staleBeats);
    }
  }

  SECTION("Shrinking the max num samples is a no-op")
  {
    pSink->requestMaxNumSamples(kMaxNumSamples / 2);
    CHECK(pSink->maxNumSamples() == kMaxNumSamples);

    commitBuffer(1., kNumFrames);
    CHECK(processor.process());
    CHECK(sender.chunks.size() == 1);
  }
}

} // namespace link_audio
} // namespace ableton
