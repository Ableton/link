// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Timeline.hpp>
#include <ableton/test/CatchWrapper.hpp>

namespace ableton
{
namespace link
{

TEST_CASE("Timeline | TimeToBeats", "[Timeline]")
{
  const auto tl = Timeline{Tempo{60.}, Beats{-1.}, std::chrono::microseconds{1000000}};
  CHECK(Beats{2.5} == toBeats(tl, std::chrono::microseconds{4500000}));
}

TEST_CASE("Timeline | BeatsToTime", "[Timeline]")
{
  const auto tl = Timeline{Tempo{60.}, Beats{-1.}, std::chrono::microseconds{1000000}};
  CHECK(std::chrono::microseconds{5200000} == fromBeats(tl, Beats{3.2}));
}

TEST_CASE("Timeline | RoundtripByteStreamEncoding", "[Timeline]")
{
  const auto tl = Timeline{Tempo{120.}, Beats{5.5}, std::chrono::microseconds{12558940}};
  std::vector<std::uint8_t> bytes(sizeInByteStream(tl));
  const auto end = toNetworkByteStream(tl, begin(bytes));
  const auto result = Timeline::fromNetworkByteStream(begin(bytes), end);
  CHECK(tl == result.first);
}

} // namespace link
} // namespace ableton
