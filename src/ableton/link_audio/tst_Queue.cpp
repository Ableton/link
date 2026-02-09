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

#include <ableton/link_audio/Queue.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>
#include <thread>

namespace ableton
{
namespace link_audio
{

TEST_CASE("Queue")
{
  struct BigValue
  {
    BigValue(const uint32_t s)
      : seed{s}
    {
      // Generate random values to take some time and test read/write consistency.
      std::minstd_rand generator{s};
      std::generate(values.begin(), values.end(), generator);
    }

    uint32_t seed;                   // Seed that identifies this value
    std::array<uint32_t, 40> values; // Seed-derived data larger than a cache line
  };

  SECTION("0Slots")
  {
    auto queue = Queue<size_t>(0, {});
    auto writer = queue.writer();
    auto reader = queue.reader();

    CHECK(!writer.retainSlot());
    CHECK(0 == writer.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(!reader.retainSlot());
    CHECK(0 == reader.numRetainedSlots());
    CHECK(0 == reader.numQueuedSlots());
    CHECK(nullptr == reader[0]);
  }

  SECTION("1Slot")
  {
    auto queue = Queue<size_t>(1, {});
    auto writer = queue.writer();
    auto reader = queue.reader();

    CHECK(0 == writer.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(writer.retainSlot());
    CHECK(1 == writer.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(!writer.retainSlot());
    *writer[0] = 1;
    CHECK(nullptr == writer[1]);
    CHECK(0 == reader.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(!reader.retainSlot());
    writer.releaseSlot();
    CHECK(nullptr == writer[0]);
    CHECK(0 == writer.numRetainedSlots());
    CHECK(1 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(1 == reader.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(!reader.retainSlot());
    CHECK(!writer.retainSlot());
    CHECK(1 == *reader[0]);
    CHECK(nullptr == reader[1]);
    reader.releaseSlot();
    CHECK(writer.retainSlot());
    CHECK(nullptr == reader[0]);
  }

  SECTION("Wrap")
  {
    auto queue = Queue<size_t>(6, {});
    auto writer = queue.writer();
    auto reader = queue.reader();

    CHECK(0 == writer.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(writer.retainSlot());
    CHECK(1 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(2 == writer.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    *writer[0] = 1;
    *writer[1] = 2;

    CHECK(!reader.retainSlot());
    writer.releaseSlot();
    CHECK(1 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(1 == reader.numRetainedSlots());
    CHECK(!reader.retainSlot());
    writer.releaseSlot();
    CHECK(1 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(2 == reader.numRetainedSlots());
    CHECK(1 == *reader[0]);
    CHECK(2 == *reader[1]);
    reader.releaseSlot();
    reader.releaseSlot();
    reader.releaseSlot();

    CHECK(0 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(1 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(2 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(3 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(4 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(5 == writer.numRetainedSlots());
    CHECK(writer.retainSlot());
    CHECK(6 == writer.numRetainedSlots());
    CHECK(!writer.retainSlot());
    CHECK(6 == writer.numRetainedSlots());

    *writer[0] = 9;
    *writer[1] = 8;
    *writer[2] = 7;
    *writer[3] = 6;
    *writer[4] = 5;
    *writer[5] = 4;

    CHECK(0 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(5 == writer.numRetainedSlots());
    CHECK(1 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(4 == writer.numRetainedSlots());
    CHECK(2 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(3 == writer.numRetainedSlots());
    CHECK(3 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(2 == writer.numRetainedSlots());
    CHECK(4 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(1 == writer.numRetainedSlots());
    CHECK(5 == writer.numQueuedSlots());
    writer.releaseSlot();
    CHECK(0 == writer.numRetainedSlots());
    CHECK(6 == writer.numQueuedSlots());

    CHECK(0 == reader.numRetainedSlots());
    CHECK(reader.retainSlot());
    CHECK(1 == reader.numRetainedSlots());
    CHECK(5 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(2 == reader.numRetainedSlots());
    CHECK(4 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(3 == reader.numRetainedSlots());
    CHECK(3 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(4 == reader.numRetainedSlots());
    CHECK(2 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(5 == reader.numRetainedSlots());
    CHECK(1 == writer.numQueuedSlots());
    CHECK(reader.retainSlot());
    CHECK(6 == reader.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());
    CHECK(!reader.retainSlot());
    CHECK(6 == reader.numRetainedSlots());
    CHECK(0 == writer.numQueuedSlots());

    CHECK(9 == *reader[0]);
    CHECK(8 == *reader[1]);
    CHECK(7 == *reader[2]);
    CHECK(6 == *reader[3]);
    CHECK(5 == *reader[4]);
    CHECK(4 == *reader[5]);

    reader.releaseSlot();
    CHECK(5 == reader.numRetainedSlots());
    reader.releaseSlot();
    CHECK(4 == reader.numRetainedSlots());
    reader.releaseSlot();
    CHECK(3 == reader.numRetainedSlots());
    reader.releaseSlot();
    CHECK(2 == reader.numRetainedSlots());
    reader.releaseSlot();
    CHECK(1 == reader.numRetainedSlots());
    reader.releaseSlot();
    CHECK(0 == reader.numRetainedSlots());
  }

  SECTION("Threaded")
  {
    const auto kNumTestOps = 1u << 18u;

    auto queue = Queue<BigValue>(kNumTestOps / 100, BigValue{0});
    auto writer = queue.writer();
    auto reader = queue.reader();

    std::thread writerThread{[&, writer_ = std::move(writer)]() mutable
                             {
                               for (auto i = 0u; i < kNumTestOps; ++i)
                               {
                                 if (writer_.retainSlot())
                                 {
                                   *writer_[0] = i;
                                   writer_.releaseSlot();
                                 }
                               }
                             }};

    std::thread readerThread{
      [&, reader_ = std::move(reader)]() mutable
      {
        auto prevValueSeed = 0u;
        for (auto i = 0u; i < kNumTestOps; ++i)
        {
          reader_.retainSlot();
          if (1 == reader_.numRetainedSlots())
          {
            const auto thisValue = *reader_[0];
            reader_.releaseSlot();

            CHECK(thisValue.seed >= prevValueSeed);
            CHECK(thisValue.values == BigValue{thisValue.seed}.values);

            prevValueSeed = thisValue.seed;
          }
        }
      }};

    writerThread.join();
    readerThread.join();
  }
}

} // namespace link_audio
} // namespace ableton
