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

#include <ableton/link_audio/Resizer.hpp>
#include <ableton/platforms/stl/Random.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/util/Injected.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace ableton
{
namespace link_audio
{
namespace
{

using SampleFormat = int16_t;
using Samples = std::vector<SampleFormat>;
using BeatTimes = std::vector<link::Beats>;

template <uint32_t kNumChannels>
struct Successor
{
  void operator()(const SampleFormat* samples,
                  const AudioBuffer::Chunks& chunks,
                  const uint32_t numChannels,
                  const uint32_t /*sampleRate*/,
                  const Id /*sessionId*/)

  {
    auto samplesBegin = samples;
    for (const auto& chunk : chunks)
    {
      const auto& [count, numFrames, beginBeats, tempo] = chunk;
      std::copy_n(
        samplesBegin, numFrames * numChannels, std::back_inserter(receivedSamples));
      samplesBegin += numFrames * numChannels;
    }
    receivedChunks.insert(receivedChunks.end(), chunks.begin(), chunks.end());
  }

  void checkMonotonic() const
  {
    for (auto it = receivedChunks.begin() + 1; it != receivedChunks.end(); ++it)
    {
      const auto& chunk = *it;
      const auto& prevChunk = *(it - 1);
      CHECK(prevChunk.count < chunk.count);
      CHECK(prevChunk.numFrames == chunk.numFrames);
      CHECK(prevChunk.beginBeats < chunk.beginBeats);
      CHECK(prevChunk.tempo == chunk.tempo);
    }
  }

  std::vector<std::pair<link::Tempo, Samples>> collectTempoRanges() const
  {
    std::vector<std::pair<link::Tempo, Samples>> ranges = {
      std::make_pair(receivedChunks.front().tempo, Samples{})};

    auto samplesBegin = receivedSamples.begin();

    for (const auto& [count, numFrames, beginBeats, tempo] : receivedChunks)
    {
      if (tempo != ranges.back().first)
      {
        ranges.emplace_back(tempo, Samples{});
      }

      const auto numSamples = numFrames * kNumChannels;
      ranges.back().second.insert(
        ranges.back().second.end(), samplesBegin, samplesBegin + numSamples);
      samplesBegin += numSamples;
    }

    return ranges;
  }

  Samples receivedSamples;
  AudioBuffer::Chunks receivedChunks;
};

} // namespace

TEST_CASE("Resizer")
{
  constexpr auto sampleRate = 100u;

  SECTION("InputTimingChanges")
  {
    constexpr auto numChannels = 2u;
    auto successor = Successor<numChannels>{};
    auto resizer =
      Resizer<SampleFormat, Successor<numChannels>&, 512>(util::injectRef(successor));
    auto samples = Samples(128);
    std::iota(samples.begin(), samples.end(), SampleFormat{0});

    const auto firstBegin = Beats{10.0};
    const auto firstTempo = Tempo{120.0};
    auto checkTiming = [&](Beats nextBegin, Tempo nextTempo)
    {
      resizer(samples.data(), 32, numChannels, sampleRate, firstBegin, firstTempo, {});
      resizer(samples.data() + 64, 32, numChannels, sampleRate, nextBegin, nextTempo, {});
      resizer(samples.data(), 0, numChannels, sampleRate + 1, {}, {}, {});

      CHECK(successor.receivedSamples == samples);
      REQUIRE(successor.receivedChunks.size() == 2);
      CHECK(successor.receivedChunks[0].numFrames == 32);
      CHECK(successor.receivedChunks[0].beginBeats == firstBegin);
      CHECK(successor.receivedChunks[0].tempo == firstTempo);
      CHECK(successor.receivedChunks[1].numFrames == 32);
      CHECK(successor.receivedChunks[1].beginBeats == nextBegin);
      CHECK(successor.receivedChunks[1].tempo == nextTempo);
    };

    SECTION("ForwardGapAtUnchangedTempo")
    {
      checkTiming(Beats{12.64}, firstTempo);
    }
    SECTION("BackwardJumpAtUnchangedTempo")
    {
      checkTiming(Beats{8.64}, firstTempo);
    }
    SECTION("TempoChangeAtContinuousBeatTime")
    {
      // The first 32 frames at 100 Hz and 120 BPM end at beat 10.64.
      checkTiming(Beats{10.64}, Tempo{90.0});
    }
  }

  SECTION("BeatTimingRoundingTolerance")
  {
    constexpr auto numChannels = 1u;
    constexpr auto testSampleRate = 44100u;
    constexpr auto numFrames = 128u;
    auto successor = Successor<numChannels>{};
    auto resizer =
      Resizer<SampleFormat, Successor<numChannels>&, 1024>(util::injectRef(successor));
    auto samples = Samples(2 * numFrames, SampleFormat{1});

    const auto beginBeats = link::Beats{10.0};
    const auto tempo = link::Tempo{120.0};

    // Calc beat time from tempo
    const auto microsPerBeat = tempo.microsPerBeat().count();
    const auto hostTimeDelta =
      std::llround(static_cast<double>(numFrames) * 1e6 / testSampleRate);
    const auto nextBeginBeats = beginBeats
                                + link::Beats{static_cast<double>(hostTimeDelta)
                                              / static_cast<double>(microsPerBeat)};

    resizer(
      samples.data(), numFrames, numChannels, testSampleRate, beginBeats, tempo, {});
    resizer(samples.data() + numFrames,
            numFrames,
            numChannels,
            testSampleRate,
            nextBeginBeats,
            tempo,
            {});
    resizer(
      samples.data(), 0, numChannels, testSampleRate + 1, {}, {}, {}); // force flush

    REQUIRE(successor.receivedChunks.size() == 1);
    CHECK(successor.receivedChunks[0].numFrames == 2 * numFrames);
    CHECK(successor.receivedSamples == samples);
  }

  SECTION("Monotonic")
  {
    platforms::stl::Random random;
    const auto kNumIncomingSamples = 24000;
    auto samples = Samples(kNumIncomingSamples);
    std::generate(samples.begin(), samples.end(), [&] { return random(); });

    auto resize = [&](auto& resizer, size_t numBuffers, size_t numChannels)
    {
      const auto samplesPerBuffer = samples.size() / numBuffers;
      const auto framesPerBuffer = samplesPerBuffer / numChannels;
      for (auto i = 0u; i < numBuffers; ++i)
      {
        const auto beginBeats = link::Beats{static_cast<double>(i * framesPerBuffer)};

        const auto tempo = link::Tempo{60.0 * sampleRate}; // 1 beat per frame

        resizer(samples.data() + (i * samplesPerBuffer),
                static_cast<uint32_t>(framesPerBuffer),
                static_cast<uint32_t>(numChannels),
                sampleRate,
                beginBeats,
                tempo,
                {});
      }
    };

    SECTION("Mono")
    {
      const auto numChannels = 1u;
      auto successor = Successor<numChannels>{};

      SECTION("BigInputBuffers")
      {
        const auto numBuffers = 3u;
        const auto requiredCacheBuffers = 8u;
        const auto cacheBufferSizeInSamples = kNumIncomingSamples / requiredCacheBuffers;
        const auto cacheBufferSizeInBytes =
          cacheBufferSizeInSamples * sizeof(SampleFormat);

        auto resizer =
          Resizer<SampleFormat, Successor<numChannels>&, cacheBufferSizeInBytes>(
            util::injectRef(successor));

        resize(resizer, numBuffers, numChannels);

        CHECK(samples == successor.receivedSamples);
        CHECK(successor.receivedChunks.size() == requiredCacheBuffers);

        successor.checkMonotonic();
      }

      SECTION("SmallInputBuffers")
      {
        const auto numBuffers = 8u;
        const auto requiredCacheBuffers = 3u;
        const auto cacheBufferSizeInSamples = kNumIncomingSamples / requiredCacheBuffers;
        const auto cacheBufferSizeInBytes =
          cacheBufferSizeInSamples * sizeof(SampleFormat);

        auto resizer =
          Resizer<SampleFormat, Successor<numChannels>&, cacheBufferSizeInBytes>(
            util::injectRef(successor));

        resize(resizer, numBuffers, numChannels);

        CHECK(samples == successor.receivedSamples);
        CHECK(successor.receivedChunks.size() == requiredCacheBuffers);
        successor.checkMonotonic();
      }
    }

    SECTION("Stereo")
    {
      const auto numChannels = 2u;
      auto successor = Successor<numChannels>{};

      SECTION("BigInputBuffers")
      {
        const auto numBuffers = 6u;
        const auto requiredCacheBuffers = 3u;
        const auto cacheBufferSizeInSamples = kNumIncomingSamples / requiredCacheBuffers;
        const auto cacheBufferSizeInBytes =
          cacheBufferSizeInSamples * sizeof(SampleFormat);

        auto resizer =
          Resizer<SampleFormat, Successor<numChannels>&, cacheBufferSizeInBytes>(
            util::injectRef(successor));

        resize(resizer, numBuffers, numChannels);

        CHECK(samples == successor.receivedSamples);
        CHECK(successor.receivedChunks.size() == requiredCacheBuffers);
        successor.checkMonotonic();
      }

      SECTION("SmallInputBuffers")
      {
        const auto numBuffers = 3u;
        const auto requiredCacheBuffers = 6u;
        const auto cacheBufferSizeInSamples = kNumIncomingSamples / requiredCacheBuffers;
        const auto cacheBufferSizeInBytes =
          cacheBufferSizeInSamples * sizeof(SampleFormat);

        auto resizer =
          Resizer<SampleFormat, Successor<numChannels>&, cacheBufferSizeInBytes>(
            util::injectRef(successor));

        resize(resizer, numBuffers, numChannels);

        CHECK(samples == successor.receivedSamples);
        CHECK(successor.receivedChunks.size() == requiredCacheBuffers);
        successor.checkMonotonic();
      }
    }

    SECTION("TempoChanges")
    {
      const auto numChannels = 2u;
      auto successor = Successor<numChannels>{};

      auto resizer =
        Resizer<SampleFormat, Successor<numChannels>&, 512>(util::injectRef(successor));

      auto ranges =
        std::vector<std::pair<size_t, link::Tempo>>{{4000, link::Tempo{60.0}},
                                                    {8000, link::Tempo{90.0}},
                                                    {12000, link::Tempo{120.0}},
                                                    {16000, link::Tempo{150.0}},
                                                    {20000, link::Tempo{180.0}},
                                                    {24000, link::Tempo{210.0}}};

      for (const auto& [numSamples, tempo] : ranges)
      {
        const auto beginBeats = link::Beats{
          static_cast<double>(successor.receivedSamples.size() / numChannels)};

        resizer(samples.data(),
                static_cast<uint32_t>(numSamples / numChannels),
                static_cast<uint32_t>(numChannels),
                sampleRate,
                beginBeats,
                tempo,
                {});
      }

      // flush by changing session ID
      resizer(samples.data(), 0, numChannels + 1, sampleRate, {}, {}, Id{});

      const auto received = successor.collectTempoRanges();
      CHECK(received.size() == ranges.size());

      for (auto i = 0u; i < ranges.size(); ++i)
      {
        CHECK(received[i].first == ranges[i].second);
        CHECK(received[i].second.size() == ranges[i].first);
        CHECK(
          received[i].second
          == std::vector(samples.begin(),
                         samples.begin() + static_cast<std::ptrdiff_t>(ranges[i].first)));
      }
    }
  }
}

} // namespace link_audio
} // namespace ableton
