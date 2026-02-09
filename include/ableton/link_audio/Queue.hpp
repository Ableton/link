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

#include <atomic>
#include <memory>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename T>
struct Queue
{
private:
  struct Impl;

public:
  struct Writer
  {
    using SlotType = T;

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&&) = default;
    Writer& operator=(Writer&&) = default;

    bool retainSlot()
    {
      return mpQueue->retainSlot(mpQueue->mWriterEnd, mpQueue->mReaderBegin);
    }

    void releaseSlot()
    {
      mpQueue->releaseSlot(mpQueue->mWriterBegin, mpQueue->mWriterEnd);
    }

    size_t numRetainedSlots() const
    {
      return mpQueue->numRetainedSlots(mpQueue->mWriterBegin, mpQueue->mWriterEnd);
    }

    size_t numQueuedSlots() const { return mpQueue->numQueuedSlots(); }

    T* operator[](size_t i) const
    {
      if (i < numRetainedSlots())
      {
        i += mpQueue->mWriterBegin.load();
        return mpQueue->mData[i % mpQueue->mData.size()].get();
      }
      return nullptr;
    }

    size_t numSlots() const { return mpQueue->numSlots(); }

  private:
    friend Queue;

    Writer(std::shared_ptr<Impl> pQueue)
      : mpQueue(pQueue)
    {
    }

    std::shared_ptr<Impl> mpQueue;
  };

  struct Reader
  {
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    Reader(Reader&&) = default;
    Reader& operator=(Reader&&) = default;

    bool retainSlot()
    {
      return mpQueue->retainSlot(mpQueue->mReaderEnd, mpQueue->mWriterBegin);
    }

    void releaseSlot()
    {
      mpQueue->releaseSlot(mpQueue->mReaderBegin, mpQueue->mReaderEnd);
    }

    size_t numRetainedSlots() const
    {
      return mpQueue->numRetainedSlots(mpQueue->mReaderBegin, mpQueue->mReaderEnd);
    }

    size_t numQueuedSlots() const { return mpQueue->numQueuedSlots(); }

    T* operator[](size_t i) const
    {
      if (i < numRetainedSlots())
      {
        i += mpQueue->mReaderBegin.load() + 1;
        return mpQueue->mData[i % mpQueue->mData.size()].get();
      }
      return nullptr;
    }

    size_t numSlots() const { return mpQueue->numSlots(); }

  private:
    friend Queue;

    Reader(std::shared_ptr<Impl> pQueue)
      : mpQueue(pQueue)
    {
    }

    std::shared_ptr<Impl> mpQueue;
  };

  Queue(size_t numRetainedSlots, const T& t)
    : mpImpl{std::make_shared<Impl>(numRetainedSlots, t)}
  {
  }

  Writer writer() const { return {mpImpl}; }

  Reader reader() const { return {mpImpl}; }

private:
  struct Impl
  {
    Impl(size_t numRetainedSlots, const T& t)
      : mData(numRetainedSlots + 2)
      , mWriterBegin(1)
      , mWriterEnd(1)
      , mReaderBegin(0)
      , mReaderEnd(0)
    {
      for (auto& slot : mData)
      {
        slot = std::make_unique<T>(t);
      }
    }

    inline size_t nextIndex(const size_t index) const
    {
      return (index + 1) % mData.size();
    }

    inline bool retainSlot(std::atomic_size_t& aEnd, std::atomic_size_t& aOtherBegin)
    {
      const auto nextEnd = nextIndex(aEnd.load());
      const auto otherBegin = aOtherBegin.load();
      if (nextEnd != otherBegin)
      {
        aEnd.store(nextEnd);
        return true;
      }
      return false;
    }

    inline void releaseSlot(std::atomic_size_t& aBegin, std::atomic_size_t& aEnd)
    {
      const auto begin = aBegin.load();
      const auto end = aEnd.load();
      if (begin != end)
      {
        aBegin.store(nextIndex(begin));
      }
    }

    inline size_t numRetainedSlots(const std::atomic_size_t& aBegin,
                                   const std::atomic_size_t& aEnd) const
    {
      const auto begin = aBegin.load();
      const auto end = aEnd.load();
      return begin > end ? end + mData.size() - begin : end - begin;
    }

    inline size_t numQueuedSlots() const
    {
      const auto writerBegin = mWriterBegin.load();
      const auto readerEnd = mReaderEnd.load();
      return writerBegin > readerEnd ? writerBegin - readerEnd - 1
                                     : writerBegin + mData.size() - readerEnd - 1;
    }

    size_t numSlots() const { return mData.size() - 2; }

    std::vector<std::unique_ptr<T>> mData;
    std::atomic_size_t mWriterBegin;
    std::atomic_size_t mWriterEnd;
    std::atomic_size_t mReaderBegin;
    std::atomic_size_t mReaderEnd;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
