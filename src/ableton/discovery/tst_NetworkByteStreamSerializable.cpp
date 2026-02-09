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

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace discovery
{

TEST_CASE("NetworkByteStreamSerializable")
{
  SECTION("DurationRoundtripByteStreamEncoding")
  {
    using namespace std;
    const auto expectedDuration = chrono::microseconds{42};
    const auto size = sizeInByteStream(expectedDuration);
    CHECK(8 == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedDuration, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized = Deserialize<chrono::microseconds>::fromNetworkByteStream(
      begin(byteStream), end(byteStream));
    CHECK(expectedDuration == deserialized.first);
  }

  SECTION("StringRoundtripByteStreamEncoding")
  {
    using namespace std;
    const auto expectedString = string{"Oh, wie schoen ist Panama!"};
    const auto size = sizeInByteStream(expectedString);
    CHECK(4 + expectedString.size() == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedString, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized =
      Deserialize<string>::fromNetworkByteStream(begin(byteStream), end(byteStream));
    CHECK(expectedString == deserialized.first);
  }

  SECTION("ArrayRoundtripByteStreamEncoding")
  {
    using namespace std;
    using Array = array<int64_t, 3>;
    const auto expectedArray = Array{{0, -1, 2}};
    const auto size = sizeInByteStream(expectedArray);
    CHECK(24 == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedArray, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized =
      Deserialize<Array>::fromNetworkByteStream(begin(byteStream), end(byteStream));
    CHECK(expectedArray == deserialized.first);
  }

  SECTION("VectorRoundtripByteStreamEncoding")
  {
    using namespace std;
    using Vector = vector<uint16_t>;
    const auto expectedVector = Vector{0, 1, 2, 3};
    const auto size = sizeInByteStream(expectedVector);
    CHECK(12 == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedVector, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized =
      Deserialize<Vector>::fromNetworkByteStream(begin(byteStream), end(byteStream));
    CHECK(expectedVector == deserialized.first);
  }

  SECTION("2-TupleRoundtripByteStreamEncoding")
  {
    using namespace std;
    using Tuple = tuple<uint32_t, bool>;
    const Tuple expectedTuple = make_tuple(42, false);
    const auto size = sizeInByteStream(expectedTuple);
    CHECK(5 == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedTuple, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized =
      Deserialize<Tuple>::fromNetworkByteStream(begin(byteStream), end(byteStream));
    CHECK(expectedTuple == deserialized.first);
  }

  SECTION("3-TupleRoundtripByteStreamEncoding")
  {
    using namespace std;
    using Tuple = tuple<bool, int64_t, uint32_t>;
    const Tuple expectedTuple = make_tuple(false, -42, 42);
    const auto size = sizeInByteStream(expectedTuple);
    CHECK(13 == size);

    auto byteStream = vector<uint8_t>(size);
    const auto serializedEndIt = toNetworkByteStream(expectedTuple, begin(byteStream));
    CHECK(byteStream.size()
          == static_cast<size_t>(distance(begin(byteStream), serializedEndIt)));

    const auto deserialized =
      Deserialize<Tuple>::fromNetworkByteStream(begin(byteStream), end(byteStream));
    CHECK(expectedTuple == deserialized.first);
  }
}

} // namespace discovery
} // namespace ableton
