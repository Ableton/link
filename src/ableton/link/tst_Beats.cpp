// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Beats.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{

TEST_CASE("Beats | ConstructFromFloating", "[Beats]")
{
  const auto beats = Beats{0.5};
  CHECK(500000 == microBeats(beats));
  CHECK(0.5 == floating(beats));
}

TEST_CASE("Beats | ConstructFromMicros", "[Beats]")
{
  const auto beats = Beats{INT64_C(100000)};
  CHECK(100000 == microBeats(beats));
  CHECK(0.1 == floating(beats));
}

TEST_CASE("Beats | Addition", "[Beats]")
{
  const auto beat1 = Beats{0.5};
  const auto beat2 = Beats{INT64_C(200000)};
  const auto beat3 = Beats{0.1};
  CHECK(beat1 == beat2 + beat2 + beat3);
}

TEST_CASE("Beats | Subtraction", "[Beats]")
{
  const auto beat1 = Beats{0.5};
  const auto beat2 = Beats{INT64_C(200000)};
  const auto beat3 = Beats{0.1};
  CHECK(beat3 == beat1 - beat2 - beat2);
}

TEST_CASE("Beats | Modulo", "[Beats]")
{
  const auto beat1 = Beats{0.1};
  const auto beat2 = Beats{0.5};
  const auto beat3 = Beats{0.6};
  CHECK(beat1 == beat3 % beat2);
}

TEST_CASE("Beats | SizeInByteStream", "[Beats]")
{
  Beats beats{0.5};
  CHECK(8 == sizeInByteStream(beats));
}

TEST_CASE("Beats | RoundtripByteStreamEncoding", "[Beats]")
{
  Beats beats{0.5};
  std::vector<std::uint8_t> bytes(sizeInByteStream(beats));
  const auto end = toNetworkByteStream(beats, begin(bytes));
  const auto result = Beats::fromNetworkByteStream(begin(bytes), end);
  CHECK(beats == result.first);
}

} // namespace link
} // namespace ableton
