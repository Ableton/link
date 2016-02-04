// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/Kalman.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <vector>
#include <array>

namespace ableton
{
namespace link
{

TEST_CASE("Kalman | Check1", "[Kalman]")
{
  int peerTimeDiff = 0;
  Kalman<16> filter;

  for (int i = 0; i < 5; ++i)
  {
    filter.iterate(peerTimeDiff);
  }

  CHECK(peerTimeDiff == filter.getValue());

  filter.iterate(100);

  CHECK(peerTimeDiff != filter.getValue());
}


TEST_CASE("Kalman | Check2", "[Kalman]")
{
  double peerTimeDiff = 3e11;
  Kalman<5> filter;

  for (int i = 0; i < 15; ++i)
  {
    filter.iterate(peerTimeDiff);
  }

  CHECK(peerTimeDiff == filter.getValue());

  for (int i = 0; i<15; ++i)
  {
    filter.iterate(11);
  }

  CHECK(peerTimeDiff != filter.getValue());
}

} // namespace link
} // namespace ableton
