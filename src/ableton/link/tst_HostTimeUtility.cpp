// Copyright: 2016, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/HostTimeUtility.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <chrono>

namespace ableton
{
namespace link
{

struct MockClock
{
  MockClock()
    : mTime(std::chrono::microseconds(0))
  {}

  friend std::chrono::microseconds micros(MockClock& clock)
  {
    const auto current = clock.mTime;
    clock.mTime = clock.mTime + std::chrono::microseconds(1);
    return current;
  }

private:
  std::chrono::microseconds mTime;
};

using Utility = ableton::link::HostTimeUtility<MockClock>;

TEST_CASE("HostTimeUtility | OneValue", "[HostTimeUtility]")
{
  Utility utility;
  const auto ht = utility.sampleTimeToHostTime(5);
  CHECK(0 == ht);
}

TEST_CASE("HostTimeUtility | MultipleValues", "[HostTimeUtility]")
{
  Utility utility;
  const auto numValues = 600;
  auto ht = 0.0;

  for (int i = 0; i <= numValues; ++i)
  {
    ht = utility.sampleTimeToHostTime(i);
  }

  CHECK(numValues == ht);
}

TEST_CASE("HostTimeUtility | Reset", "[HostTimeUtility]")
{
  Utility utility;
  auto ht = utility.sampleTimeToHostTime(0);
  ht = utility.sampleTimeToHostTime(-23);
  ht = utility.sampleTimeToHostTime(40);
  REQUIRE(2 != ht);

  utility.reset();
  ht = utility.sampleTimeToHostTime(0);
  CHECK(3 == ht);
}

} // namespace link
} // namespace ableton
