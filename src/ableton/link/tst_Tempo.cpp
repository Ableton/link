// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Tempo.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{

TEST_CASE("Tempo | ConstructFromBpm", "[Tempo]")
{
  const auto tempo = Tempo{120.};
  CHECK(120. == bpm(tempo));
  CHECK(std::chrono::microseconds{500000} == microsPerBeat(tempo));
}

TEST_CASE("Tempo | ConstructFromMicros", "[Tempo]")
{
  const auto tempo = Tempo{std::chrono::microseconds{500000}};
  CHECK(120. == bpm(tempo));
  CHECK(std::chrono::microseconds{500000} == microsPerBeat(tempo));
}

TEST_CASE("Tempo | ConstructFromRatio", "[Tempo]")
{
  const auto tempo = Tempo{Beats{2.}, std::chrono::microseconds{500000}};
  CHECK(240. == bpm(tempo));
  CHECK(std::chrono::microseconds{250000} == microsPerBeat(tempo));
}

TEST_CASE("Tempo | MicrosToBeats", "[Tempo]")
{
  const auto tempo = Tempo{120.};
  CHECK(Beats{2.} == microsToBeats(tempo, std::chrono::microseconds{1000000}));
}

TEST_CASE("Tempo | BeatsToMicros", "[Tempo]")
{
  const auto tempo = Tempo{120.};
  CHECK(std::chrono::microseconds{1000000} == beatsToMicros(tempo, Beats{2.}));
}

TEST_CASE("Tempo | ComparisonLT", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{200.};
  CHECK(tempo1 < tempo2);
}

TEST_CASE("Tempo | ComparisonGT", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{200.};
  CHECK(tempo2 > tempo1);
}


TEST_CASE("Tempo | ComparisonLE", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{200.};
  CHECK(tempo1 <= tempo2);
  CHECK(tempo2 <= tempo2);
}

TEST_CASE("Tempo | ComparisonGE", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{200.};
  CHECK(tempo2 >= tempo1);
  CHECK(tempo2 >= tempo2);
}

TEST_CASE("Tempo | ComparisonEQ", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{100.};
  CHECK(tempo1 == tempo2);
}

TEST_CASE("Tempo | ComparisonNE", "[Tempo]")
{
  const auto tempo1 = Tempo{100.};
  const auto tempo2 = Tempo{200.};
  CHECK(tempo1 != tempo2);
}

TEST_CASE("Tempo | RoundtripByteStreamEncoding", "[Tempo]")
{
  const auto tempo = Tempo{120.};
  std::vector<std::uint8_t> bytes(sizeInByteStream(tempo));
  const auto end = toNetworkByteStream(tempo, begin(bytes));
  const auto result = Tempo::fromNetworkByteStream(begin(bytes), end);
  CHECK(tempo == result.first);
}

} // namespace link
} // namespace ableton
