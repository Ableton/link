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

#include <ableton/link_audio/BeatTimeMapping.hpp>
#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/Queue.hpp>
#include <ableton/util/Locked.hpp>
#include <atomic>
#include <string>

#pragma once

namespace ableton
{
namespace link_audio
{

struct Sink
{
  Sink(std::string name, size_t maxNumSamples, Id id)
    : mName{std::move(name)}
    , mId{std::move(id)}
    , mQueue(128, {static_cast<uint32_t>(maxNumSamples)})
  {
  }

  void setName(std::string name)
  {
    mName.update([name = std::move(name)](auto& n) { n = std::move(name); });
    mNameIsUpToDate.clear();
  }

  std::string name() const { return mName.read(); }

  bool nameChanged() { return !mNameIsUpToDate.test_and_set(); }

  const Id& id() const { return mId; }

  Buffer<int16_t>* retainBuffer()
  {
    auto queueWriter = mQueue.writer();

    if (queueWriter.numRetainedSlots() > 0)
    {
      return nullptr;
    }

    if (!queueWriter.retainSlot())
    {
      return nullptr;
    }

    return queueWriter[0];
  }

  void releaseAndCommitBuffer(const link::Timeline& timeline,
                              const Id& sessionId,
                              double beatsAtBufferBegin,
                              double quantum,
                              size_t numFrames,
                              size_t numChannels,
                              uint32_t sampleRate)
  {
    auto queueWriter = mQueue.writer();

    if (queueWriter.numRetainedSlots() > 0)
    {
      auto pBuffer = queueWriter[0];
      const auto beginBeats =
        globalBeatAtBeat(timeline, link::Beats{beatsAtBufferBegin}, link::Beats{quantum});
      pBuffer->mBeginBeats = beginBeats;
      pBuffer->mTempo = timeline.tempo;
      pBuffer->mNumChannels = static_cast<uint32_t>(numChannels);
      pBuffer->mSampleRate = sampleRate;
      pBuffer->mNumFrames = static_cast<uint32_t>(numFrames);
      pBuffer->mSessionId = sessionId;
      queueWriter.releaseSlot();
    }
  }

  void releaseBuffer()
  {
    auto queueWriter = mQueue.writer();

    assert(queueWriter.numRetainedSlots() > 0);

    if (queueWriter.numRetainedSlots() > 0)
    {
      queueWriter[0]->mBeginBeats = link::Beats{0.};
      queueWriter[0]->mTempo = link::Tempo{0};
      queueWriter.releaseSlot();
    }
  }

  Buffer<int16_t>* buffer()
  {
    auto queueWriter = mQueue.writer();
    return queueWriter.numRetainedSlots() > 0 ? queueWriter[0] : nullptr;
  }

  Queue<Buffer<int16_t>>::Reader reader() { return mQueue.reader(); }

private:
  util::Locked<std::string> mName;
  std::atomic_flag mNameIsUpToDate;
  Id mId;
  Queue<Buffer<int16_t>> mQueue;
};

} // namespace link_audio
} // namespace ableton
