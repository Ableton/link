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

#include <ableton/link/NodeId.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/MainProcessor.hpp>
#include <ableton/link_audio/Sink.hpp>
#include <ableton/link_audio/Source.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/test/serial_io/Fixture.hpp>

#include <atomic>
#include <optional>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("MainProcessor")
{
  using IoContext = test::serial_io::Context;

  struct ChannelsChangedCounter
  {
    void operator()() { ++numCalls; }
    size_t numCalls = 0;
  };

  struct TestGetSender
  {
    using SendHandler = std::function<void(const uint8_t*, size_t)>;

    std::optional<SendHandler> forChannel(const Id&)
    {
      return SendHandler{[this](const uint8_t*, size_t) { ++numSends; }};
    }

    size_t numSends = 0;
  };

  struct TestGetNodeId
  {
    const link::NodeId& operator()() const { return nodeId; }
    link::NodeId nodeId = link::NodeId::random<platforms::stl::Random>();
  };

  test::serial_io::Fixture fixture;
  auto io = util::injectVal(fixture.makeIoContext());

  ChannelsChangedCounter channelsChanged;
  using Processor = MainProcessor<ChannelsChangedCounter&,
                                  TestGetSender,
                                  TestGetNodeId,
                                  platforms::stl::Random,
                                  IoContext>;
  Processor processor(std::move(io), util::injectRef(channelsChanged));

  SECTION("Announcements reflect added sinks")
  {
    auto sinkId = Id::random<platforms::stl::Random>();
    auto pSink = std::make_shared<Sink>(std::string{"sinkA"}, 256, sinkId);

    TestGetSender sender;
    TestGetNodeId getNode;

    processor.addSink(pSink, util::injectVal(sender), util::injectVal(getNode));

    const auto ann = processor.channelAnnouncements();
    REQUIRE(ann.channels.size() == 1);
    CHECK(ann.channels[0].name == "sinkA");
    CHECK(ann.channels[0].id == sinkId);
  }

  SECTION("ChannelsChanged fires on sink name change")
  {
    auto pSink = std::make_shared<Sink>(
      std::string{"sinkB"}, 128, Id::random<platforms::stl::Random>());
    processor.addSink(
      pSink, util::injectVal(TestGetSender{}), util::injectVal(TestGetNodeId{}));


    pSink->setName("New Name");
    fixture.advanceTime(Processor::kProcessTimerPeriod);

    CHECK(channelsChanged.numCalls >= 1);
  }

  SECTION("ChannelsChanged fires on sink name change")
  {
    auto pSink = std::make_shared<Sink>(
      std::string{"sinkB"}, 128, Id::random<platforms::stl::Random>());
    processor.addSink(
      pSink, util::injectVal(TestGetSender{}), util::injectVal(TestGetNodeId{}));

    pSink->setName("New Name");
    fixture.advanceTime(Processor::kProcessTimerPeriod);

    CHECK(channelsChanged.numCalls >= 1);
  }

  SECTION("ChannelsChanged fires API removal of sink")
  {
    auto pSink = std::make_shared<Sink>(
      std::string{"sinkB"}, 128, Id::random<platforms::stl::Random>());
    processor.addSink(
      pSink, util::injectVal(TestGetSender{}), util::injectVal(TestGetNodeId{}));

    // Release the shared_ptr to the source that would be held by the API
    pSink.reset();
    fixture.advanceTime(Processor::kProcessTimerPeriod);

    CHECK(channelsChanged.numCalls >= 1);
  }

  SECTION("Source removed when API releases shared_ptr")
  {
    size_t numCallbacks = 0;
    auto sourceId = Id::random<platforms::stl::Random>();
    auto sessionId = Id::random<platforms::stl::Random>();
    auto pSource = std::make_shared<Source>(
      sourceId, [&](BufferCallbackHandle<Buffer<int16_t>>) { ++numCallbacks; });

    processor.addSource(
      pSource, util::injectVal(TestGetSender{}), util::injectVal(TestGetNodeId{}));

    fixture.advanceTime(Processor::kProcessTimerPeriod);

    // Attempt to deliver audio to that source id; callback should not run
    AudioBuffer audio;
    audio.channelId = sourceId;
    audio.sessionId = sessionId;
    audio.codec = Codec::kPCM_i16;
    audio.sampleRate = 44100;
    audio.numChannels = 2;
    audio.chunks = {AudioBuffer::Chunk{1, 4, link::Beats{0.0}, link::Tempo{120.0}}};
    audio.numBytes =
      static_cast<uint16_t>(audio.numFrames() * audio.numChannels * sizeof(int16_t));
    v1::MessageBuffer message;
    auto endIt =
      v1::audioBufferMessage(TestGetNodeId{}.operator()(), audio, message.begin());
    auto payloadBegin = v1::parseMessageHeader(message.begin(), endIt).second;
    processor.receiveAudioBuffer(payloadBegin, endIt);

    // Send calback has been called
    CHECK(numCallbacks == 1);

    // Release the shared_ptr to the source that would be held by the API
    pSource.reset();
    fixture.advanceTime(Processor::kProcessTimerPeriod);

    processor.receiveAudioBuffer(payloadBegin, endIt);

    // Send calback has not been called again
    CHECK(numCallbacks == 1);
  }
}

} // namespace link_audio
} // namespace ableton
