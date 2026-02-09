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
#include <ableton/link_audio/PCMCodec.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>

namespace ableton
{
namespace link_audio
{
namespace
{

using SampleFormat = int16_t;
using Samples = std::vector<SampleFormat>;

struct Sender
{
  void operator()(const AudioBuffer& buffer_) { buffer = buffer_; }

  AudioBuffer buffer;
};

struct Successor
{
  template <typename Buffer>
  void operator()(BufferCallbackHandle<Buffer> handle)
  {
    inputFrames = handle.mBuffer.mNumFrames;
    beginBeats = handle.mBuffer.mBeginBeats;
    tempo = handle.mBuffer.mTempo;

    cache.resize(handle.mBuffer.mNumFrames * handle.mBuffer.mNumChannels);
    std::copy_n(handle.mpSamples,
                handle.mBuffer.mNumFrames * handle.mBuffer.mNumChannels,
                cache.begin());
  }

  size_t inputFrames;
  Beats beginBeats;
  Tempo tempo;
  Samples cache;
};

using TestEncoder = PCMEncoder<SampleFormat, Sender&>;
using TestDecoder = PCMDecoder<SampleFormat, Successor&>;

} // namespace

TEST_CASE("PCMCodec")
{
  const auto bufferSize = 32u;
  auto input = Samples(bufferSize);
  platforms::stl::Random random;
  std::generate(input.begin(), input.end(), [&] { return random(); });

  auto successor = Successor{};

  SECTION("Roundtrip")
  {
    const auto beginBeats = Beats{44.};
    const auto tempo = Tempo{120.};

    auto sender = Sender{};
    auto encoder = TestEncoder(util::injectRef(sender), {});
    auto decoder = TestDecoder(util::injectRef(successor), 512);
    const AudioBuffer::Chunks chunks = {
      AudioBuffer::Chunk{1u, bufferSize, beginBeats, tempo}};

    encoder(input.data(), chunks, 1, 48000, Id{});
    CHECK(input.size() * sizeof(SampleFormat) == sender.buffer.numBytes);

    decoder(sender.buffer);
    CHECK(bufferSize == successor.inputFrames);
    CHECK(input == successor.cache);
    CHECK(beginBeats == successor.beginBeats);
    CHECK(tempo == successor.tempo);
  }
}

} // namespace link_audio
} // namespace ableton
