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

#include <ableton/link/Beats.hpp>
#include <ableton/link_audio/AudioBuffer.hpp>
#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Encoder.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <algorithm>
#include <vector>

namespace ableton
{
namespace link_audio
{

TEST_CASE("Encoder")
{

  using SampleFormat = int16_t;
  using Samples = std::vector<SampleFormat>;
  using InputBuffer = Buffer<SampleFormat>;

  struct Sender
  {
    void operator()(const AudioBuffer& buffer)
    {
      std::copy(
        buffer.chunks.begin(), buffer.chunks.end(), std::back_inserter(sentChunks));

      for (auto it = buffer.bytes.begin(); it != buffer.bytes.begin() + buffer.numBytes;)
      {
        auto [value, iter] = discovery::Deserialize<SampleFormat>::fromNetworkByteStream(
          it, buffer.bytes.begin() + buffer.numBytes);
        it = iter;
        sent.push_back(value);
      }
    }

    Samples sent;
    std::vector<AudioBuffer::Chunk> sentChunks;
  };

  auto buildSamples = [](uint32_t frameSize, uint32_t audio)
  {
    auto samples = Samples(frameSize * audio);
    for (auto i = 0u; i < frameSize; ++i)
    {
      for (auto j = 0u; j < audio; ++j)
      {
        samples[i * audio + j] = static_cast<SampleFormat>(i);
      }
    }
    return samples;
  };

  auto buildInputBuffer = [](const Samples& samples,
                             uint32_t sampleRate,
                             uint32_t numChannels,
                             Beats beginBeats,
                             Tempo tempo)
  {
    auto inputBuffer = InputBuffer(static_cast<uint32_t>(samples.size()));
    inputBuffer.mSamples = samples;
    inputBuffer.mNumFrames = static_cast<uint32_t>(samples.size() / numChannels);
    inputBuffer.mNumChannels = numChannels;
    inputBuffer.mSampleRate = sampleRate;
    inputBuffer.mBeginBeats = beginBeats;
    inputBuffer.mTempo = tempo;
    return inputBuffer;
  };


  using TestEncoder = Encoder<Sender&, SampleFormat>;

  auto process = [](Sender& sender, InputBuffer inputBuffer)
  {
    auto encoder = TestEncoder(util::injectRef(sender), {});

    encoder(inputBuffer);
  };

  const auto kEngineSampleRate = 44100;
  const auto kMaxNumMonoSamplesPerBuffer = TestEncoder::kMaxAudioBytes / 2;
  const auto kMonoSamples = buildSamples(kMaxNumMonoSamplesPerBuffer, 1);
  const auto kMaxNumStereoSamplesPerBuffer = kMaxNumMonoSamplesPerBuffer / 2;
  const auto kStereoSamples = buildSamples(kMaxNumStereoSamplesPerBuffer, 2);
  const auto kBeginBeats = Beats{0.2};
  const auto kTempo = Tempo{120.0};
  const auto kMonoInputBuffer =
    buildInputBuffer(kMonoSamples, kEngineSampleRate, 1, kBeginBeats, kTempo);
  const auto kStereoInputBuffer = buildInputBuffer(
    kStereoSamples, kEngineSampleRate, 2, Beats{kBeginBeats}, Tempo{kTempo});

  auto sender = Sender{};

  SECTION("NoProcessingMono")
  {
    process(sender, kMonoInputBuffer);

    REQUIRE(1 == sender.sentChunks.size());
    CHECK(kMonoSamples == sender.sent);
    const auto& [count, numFrames, beginBeats, tempo] = sender.sentChunks.front();
    CHECK(kBeginBeats == beginBeats);
    CHECK(kTempo == tempo);
  }

  SECTION("NoProcessingStereo")
  {
    process(sender, kStereoInputBuffer);

    REQUIRE(1 == sender.sentChunks.size());
    CHECK(kStereoSamples == sender.sent);
    const auto& [count, numFrames, beginBeats, tempo] = sender.sentChunks.front();
    CHECK(kBeginBeats == beginBeats);
    CHECK(kTempo == tempo);
  }

  SECTION("BigMonoBuffer")
  {
    // Input Buffer that is bigger than kMaxNumberOfNetworkBytes
    const auto kSampleRate = 32000;
    auto samples = buildSamples(TestEncoder::kMaxAudioBytes, 1);
    const auto inputBuffer =
      buildInputBuffer(samples, kSampleRate, 1, kBeginBeats, kTempo);

    process(sender, inputBuffer);

    CHECK(2 == sender.sentChunks.size());

    CHECK(kBeginBeats == sender.sentChunks[0].beginBeats);
    CHECK(kTempo == sender.sentChunks[0].tempo);
    CHECK(TestEncoder::kMaxAudioBytes / sizeof(SampleFormat)
          == sender.sentChunks[0].numFrames);

    CHECK(kBeginBeats < sender.sentChunks[1].beginBeats);
    CHECK(kTempo == sender.sentChunks[1].tempo);
    CHECK(TestEncoder::kMaxAudioBytes / sizeof(SampleFormat)
          == sender.sentChunks[1].numFrames);

    CHECK(samples == sender.sent);
  }
}

} // namespace link_audio
} // namespace ableton
