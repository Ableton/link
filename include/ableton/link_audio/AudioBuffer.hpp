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

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/link/Beats.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <algorithm>
#include <array>
#include <numeric>
#include <tuple>
#include <vector>

namespace ableton
{
namespace link_audio
{

using Beats = link::Beats;
using Tempo = link::Tempo;

enum Codec : uint8_t
{
  kInvalid = 0,
  kPCM_i16 = 1,
};
struct AudioBuffer
{
  static constexpr std::int32_t key = '_abu';
  static_assert(key == 0x5f616275, "Unexpected byte order");

  static constexpr std::uint32_t kNonAudioBytes = 50;
  static constexpr uint32_t kMaxAudioBytes = v1::kMaxPayloadSize - kNonAudioBytes;

  struct Chunk
  {
    friend std::uint32_t sizeInByteStream(const Chunk& chunk)
    {
      return discovery::sizeInByteStream(chunk.count)
             + discovery::sizeInByteStream(chunk.numFrames)
             + sizeInByteStream(chunk.beginBeats) + sizeInByteStream(chunk.tempo);
    }

    template <typename It>
    friend It toNetworkByteStream(const Chunk& chunk, It out)
    {
      out = toNetworkByteStream(
        chunk.tempo,
        toNetworkByteStream(
          chunk.beginBeats,
          discovery::toNetworkByteStream(
            chunk.numFrames, discovery::toNetworkByteStream(chunk.count, out))));
      return out;
    }

    template <typename It>
    static std::pair<Chunk, It> fromNetworkByteStream(It begin, It end)
    {
      using namespace std;

      auto [count, countEnd] =
        discovery::Deserialize<uint64_t>::fromNetworkByteStream(begin, end);
      auto [numFrames, numFramesEnd] =
        discovery::Deserialize<uint16_t>::fromNetworkByteStream(countEnd, end);
      auto [beginBeats, beginBeatsEnd] =
        discovery::Deserialize<Beats>::fromNetworkByteStream(numFramesEnd, end);
      auto [tempo, tempoEnd] =
        discovery::Deserialize<Tempo>::fromNetworkByteStream(beginBeatsEnd, end);
      return make_pair(Chunk{count, numFrames, beginBeats, tempo}, tempoEnd);
    }

    friend bool operator==(const Chunk& lhs, const Chunk& rhs)
    {
      return std::tie(lhs.count, lhs.numFrames, lhs.beginBeats, lhs.tempo)
             == std::tie(rhs.count, rhs.numFrames, rhs.beginBeats, rhs.tempo);
    }

    uint64_t count;
    uint16_t numFrames;
    Beats beginBeats;
    Tempo tempo;
  };

  using Chunks = std::vector<Chunk>;
  using Bytes = std::array<uint8_t, kMaxAudioBytes>;

  uint32_t numFrames() const
  {
    return std::accumulate(chunks.begin(),
                           chunks.end(),
                           0u,
                           [](uint32_t sum, const Chunk& chunk)
                           { return sum + chunk.numFrames; });
  }

  // Model the NetworkByteStreamSerializable concept
  friend std::uint32_t sizeInByteStream(const AudioBuffer& buffer)
  {
    return discovery::sizeInByteStream(buffer.channelId)
           + discovery::sizeInByteStream(buffer.sessionId)
           + discovery::sizeInByteStream(buffer.chunks)
           + discovery::sizeInByteStream(static_cast<uint8_t>(buffer.codec))
           + discovery::sizeInByteStream(static_cast<uint32_t>(buffer.sampleRate))
           + discovery::sizeInByteStream(static_cast<int8_t>(buffer.numChannels))
           + discovery::sizeInByteStream(static_cast<int16_t>(buffer.numBytes))
           + static_cast<uint32_t>(buffer.numBytes);
  }

  template <typename It>
  friend It toNetworkByteStream(const AudioBuffer& buffer, It out)
  {
    assert(buffer.codec != Codec::kInvalid);

    out = discovery::toNetworkByteStream(
      static_cast<uint16_t>(buffer.numBytes),
      discovery::toNetworkByteStream(
        static_cast<uint8_t>(buffer.numChannels),
        discovery::toNetworkByteStream(
          static_cast<uint32_t>(buffer.sampleRate),
          discovery::toNetworkByteStream(
            static_cast<uint8_t>(buffer.codec),
            discovery::toNetworkByteStream(
              buffer.chunks,
              discovery::toNetworkByteStream(
                buffer.sessionId,
                discovery::toNetworkByteStream(buffer.channelId, out)))))));

    return std::copy_n(buffer.bytes.begin(), buffer.numBytes, out);
  }

  template <typename It>
  static It fromNetworkByteStream(AudioBuffer& audioBuffer, It begin, It end)
  {
    using namespace std;

    auto [channelId, channelIdEnd] =
      discovery::Deserialize<Id>::fromNetworkByteStream(begin, end);
    audioBuffer.channelId = channelId;

    auto [sessionId, sessionIdEnd] =
      discovery::Deserialize<Id>::fromNetworkByteStream(channelIdEnd, end);
    audioBuffer.sessionId = sessionId;

    auto [chunks, chunksEnd] =
      discovery::Deserialize<Chunks>::fromNetworkByteStream(sessionIdEnd, end);
    audioBuffer.chunks = chunks;

    if (chunks.empty())
    {
      throw runtime_error("Invalid audio buffer: no chunks.");
    }

    auto [codec, codecEnd] =
      discovery::Deserialize<uint8_t>::fromNetworkByteStream(chunksEnd, end);
    audioBuffer.codec = static_cast<Codec>(codec);

    if (codec == Codec::kInvalid)
    {
      throw runtime_error("Invalid codec.");
    }

    auto [sampleRate, sampleRateEnd] =
      discovery::Deserialize<uint32_t>::fromNetworkByteStream(codecEnd, end);
    audioBuffer.sampleRate = sampleRate;

    auto [numChannels, numChannelsEnd] =
      discovery::Deserialize<uint8_t>::fromNetworkByteStream(sampleRateEnd, end);
    audioBuffer.numChannels = numChannels;

    auto [numBytes, numBytesEnd] =
      discovery::Deserialize<uint16_t>::fromNetworkByteStream(numChannelsEnd, end);
    audioBuffer.numBytes = numBytes;

    if (codec == Codec::kPCM_i16
        && audioBuffer.numFrames() * numChannels * sizeof(int16_t) != numBytes)
    {
      throw range_error("Byte count / frame count mismatch.");
    }

    if (std::distance(numBytesEnd, end) > numBytes || numBytesEnd + numBytes > end)
    {
      throw range_error("Invalid byte count.");
    }

    std::copy_n(numBytesEnd, numBytes, audioBuffer.bytes.begin());

    return numBytesEnd + numBytes;
  }

  friend bool operator==(const AudioBuffer& lhs, const AudioBuffer& rhs)
  {
    return std::tie(lhs.channelId,
                    lhs.sessionId,
                    lhs.sampleRate,
                    lhs.chunks,
                    lhs.numChannels,
                    lhs.numBytes)
             == std::tie(rhs.channelId,
                         rhs.sessionId,
                         rhs.sampleRate,
                         rhs.chunks,
                         rhs.numChannels,
                         rhs.numBytes)
           && std::equal(lhs.bytes.begin(),
                         lhs.bytes.begin() + lhs.numBytes,
                         begin(rhs.bytes),
                         begin(rhs.bytes) + rhs.numBytes);
  }

  friend bool operator!=(const AudioBuffer& lhs, const AudioBuffer& rhs)
  {
    return !(lhs == rhs);
  }

  Id channelId;
  Id sessionId;
  Chunks chunks;
  Codec codec;
  uint32_t sampleRate;
  uint8_t numChannels;
  uint16_t numBytes;
  Bytes bytes;
};

} // namespace link_audio
} // namespace ableton
