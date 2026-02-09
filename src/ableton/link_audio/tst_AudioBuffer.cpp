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

namespace ableton
{
namespace link_audio
{

TEST_CASE("AudioBuffer")
{
  using Random = ableton::platforms::stl::Random;

  SECTION("ValidBuffer")
  {
    auto buffer =
      AudioBuffer{Id::random<Random>(),
                  Id::random<Random>(),
                  std::vector<AudioBuffer::Chunk>{{9977, 2, Beats{23.}, Tempo(120.)}},
                  Codec::kPCM_i16,
                  44100,
                  4,
                  16,
                  {{1, 2, 3, 4, 5, 6, 7, 8}}};
    const auto size = sizeInByteStream(buffer);
    auto bytes = std::vector<uint8_t>(size);

    CHECK(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));

    auto deserialized = AudioBuffer{};
    auto deserializedEnd =
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end());

    CHECK(bytes.end() == deserializedEnd);
    CHECK(buffer == deserialized);
  }

  SECTION("InvalidNumBytes")
  {
    auto buffer = AudioBuffer{Id::random<Random>(),
                              Id::random<Random>(),
                              {},
                              Codec::kPCM_i16,
                              44100,
                              2,
                              1,
                              {{1, 2, 3, 4}}};
    auto deserialized = AudioBuffer{};
    auto bytes = std::vector<uint8_t>(sizeInByteStream(buffer));
    CHECK(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));
    CHECK_THROWS(
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()));
  }

  SECTION("InvalidNumFrames")
  {
    auto buffer =
      AudioBuffer{Id::random<Random>(),
                  Id::random<Random>(),
                  std::vector<AudioBuffer::Chunk>{{346, 222, Beats{23.}, Tempo(120.)}},
                  Codec::kPCM_i16,
                  44100,
                  2,
                  2,
                  {{1, 2, 3, 4}}};
    auto deserialized = AudioBuffer{};
    auto bytes = std::vector<uint8_t>(sizeInByteStream(buffer));
    CHECK(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));
    CHECK_THROWS(
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()));
  }

  SECTION("EmptyChunks")
  {
    auto buffer = AudioBuffer{Id::random<Random>(),
                              Id::random<Random>(),
                              std::vector<AudioBuffer::Chunk>{},
                              Codec::kPCM_i16,
                              44100,
                              2,
                              0,
                              {{}}};
    auto deserialized = AudioBuffer{};
    auto bytes = std::vector<uint8_t>(sizeInByteStream(buffer));
    CHECK(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));
    CHECK_THROWS_WITH(
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end()),
      "Invalid audio buffer: no chunks.");
  }


  SECTION("MultipleChunks")
  {
    auto buffer = AudioBuffer{
      Id::random<Random>(),
      Id::random<Random>(),
      std::vector<AudioBuffer::Chunk>{{12345, 5, Beats{0.0}, Tempo(120.0)},
                                      {12346, 2, Beats{2.5}, Tempo(160.0)},
                                      {12347, 3, Beats{5.2}, Tempo(100.0)}},
      Codec::kPCM_i16,
      48000,
      2,
      40,
      {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}}};

    const auto size = sizeInByteStream(buffer);
    auto bytes = std::vector<uint8_t>(size);

    CHECK(bytes.end() == toNetworkByteStream(buffer, bytes.begin()));

    auto deserialized = AudioBuffer{};
    auto deserializedEnd =
      AudioBuffer::fromNetworkByteStream(deserialized, bytes.begin(), bytes.end());

    CHECK(bytes.end() == deserializedEnd);
    CHECK(buffer == deserialized);
  }
}

} // namespace link_audio
} // namespace ableton
