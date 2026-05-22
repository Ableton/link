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

#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <cstring>
#include <limits>

namespace ableton
{
namespace link_audio
{
namespace
{

using Random = ableton::platforms::stl::Random;

AudioBuffer makeBuffer(
  std::vector<AudioBuffer::Chunk> chunks = {{9977, 2, Beats{23.}, Tempo(120.)}},
  uint8_t numChannels = 2,
  uint32_t sampleRate = 44100,
  uint16_t numBytes = 8,
  AudioBuffer::Bytes bytes = {{1, 2, 3, 4, 5, 6, 7, 8}},
  Codec codec = Codec::kPCM_i16)
{
  return AudioBuffer{Id::random<Random>(),
                     Id::random<Random>(),
                     std::move(chunks),
                     codec,
                     sampleRate,
                     numChannels,
                     numBytes,
                     bytes};
}

std::vector<uint8_t> serialize(const AudioBuffer& buffer)
{
  auto bytes = std::vector<uint8_t>(sizeInByteStream(buffer));
  REQUIRE(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));
  return bytes;
}

void checkRoundTrip(const AudioBuffer& buffer)
{
  const auto bytes = serialize(buffer);
  auto deserialized = AudioBuffer{};
  const auto deserializedEnd =
    AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end());
  CHECK(bytes.end() == deserializedEnd);
  CHECK(buffer == deserialized);
}

void checkThrowsWith(const AudioBuffer& buffer, const char* msg)
{
  const auto bytes = serialize(buffer);
  auto deserialized = AudioBuffer{};
  CHECK_THROWS_WITH(
    AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()), msg);
}

void checkThrows(const AudioBuffer& buffer)
{
  const auto bytes = serialize(buffer);
  auto deserialized = AudioBuffer{};
  CHECK_THROWS(
    AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()));
}

} // namespace

TEST_CASE("AudioBuffer")
{
  SECTION("ValidBuffer")
  {
    checkRoundTrip(makeBuffer());
  }

  SECTION("InvalidNumBytes")
  {
    checkThrows(makeBuffer({}, 2, 44100, 1, {{1, 2, 3, 4}}));
  }

  SECTION("InvalidNumFrames")
  {
    checkThrows(
      makeBuffer({{346, 222, Beats{23.}, Tempo(120.)}}, 2, 44100, 2, {{1, 2, 3, 4}}));
  }

  SECTION("InvalidSampleRate")
  {
    checkThrowsWith(
      makeBuffer({{9977, 2, Beats{23.}, Tempo(120.)}}, 2, 0), "Invalid sample rate.");
  }

  SECTION("EmptyChunks")
  {
    checkThrowsWith(
      makeBuffer({}, 2, 44100, 0, {{}}), "Invalid audio buffer: no chunks.");
  }

  SECTION("ZeroNumChannels")
  {
    checkThrowsWith(makeBuffer({{9977, 0, Beats{23.}, Tempo(120.)}}, 0, 44100, 0, {{}}),
                    "Invalid channel count.");
  }

  SECTION("TooManyChannels")
  {
    checkThrowsWith(makeBuffer({{9977, 2, Beats{23.}, Tempo(120.)}},
                               3,
                               44100,
                               12,
                               {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}}),
                    "Invalid channel count.");
  }

  SECTION("UnknownCodec")
  {
    const auto buffer = makeBuffer();
    auto bytes = serialize(buffer);
    const auto codecOffset = discovery::sizeInByteStream(buffer.channelId)
                             + discovery::sizeInByteStream(buffer.sessionId)
                             + discovery::sizeInByteStream(buffer.chunks);
    bytes[codecOffset] = 42;

    auto deserialized = AudioBuffer{};
    CHECK_THROWS_WITH(
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()),
      "Unknown codec.");
  }

  SECTION("NumBytesExceedsMax")
  {
    const auto buffer = makeBuffer();
    const auto headerSize = sizeInByteStream(buffer) - buffer.numBytes;
    const auto oversized = static_cast<uint16_t>(AudioBuffer::kMaxAudioBytes + 1);
    auto bytes = std::vector<uint8_t>(headerSize + oversized, 0u);
    toNetworkByteStream(buffer, bytes.begin());

    const auto numBytesOffset = discovery::sizeInByteStream(buffer.channelId)
                                + discovery::sizeInByteStream(buffer.sessionId)
                                + discovery::sizeInByteStream(buffer.chunks)
                                + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t);
    const auto oversizedBE = htons(oversized);
    std::memcpy(bytes.data() + numBytesOffset, &oversizedBE, sizeof(uint16_t));

    auto deserialized = AudioBuffer{};
    CHECK_THROWS_WITH(
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()),
      "Byte count exceeds maximum.");
  }

  SECTION("InvalidTempoZeroMicrosPerBeat")
  {
    checkThrowsWith(
      makeBuffer({{9977, 2, Beats{23.}, Tempo(std::chrono::microseconds{0})}}),
      "Invalid tempo.");
  }

  SECTION("InvalidTempoNaN")
  {
    checkThrowsWith(
      makeBuffer(
        {{9977, 2, Beats{23.}, Tempo(std::numeric_limits<double>::quiet_NaN())}}),
      "Invalid tempo.");
  }

  SECTION("MultipleChunks")
  {
    checkRoundTrip(makeBuffer(
      {{12345, 5, Beats{0.0}, Tempo(120.0)},
       {12346, 2, Beats{2.5}, Tempo(160.0)},
       {12347, 3, Beats{5.2}, Tempo(100.0)}},
      2,
      48000,
      40,
      {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}}));
  }
}

} // namespace link_audio
} // namespace ableton
