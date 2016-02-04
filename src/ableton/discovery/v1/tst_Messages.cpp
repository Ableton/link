// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/v1/Messages.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>

namespace ableton
{
namespace link
{
namespace discovery
{
namespace v1
{
namespace
{

// For testing just use a single byte identifier
using NodeId = uint8_t;

} // unnamed

TEST_CASE("ParseEmptyBuffer", "[Messages]")
{
  std::array<char, 0> buffer;
  auto result = parseMessageHeader<NodeId>(begin(buffer), end(buffer));
  CHECK(kInvalid == result.first.messageType);
  CHECK(begin(buffer) == result.second);
}

TEST_CASE("ParseTruncatedMessageHeader", "[Messages]")
{
  std::array<char, 10> truncated = {{'_', 'a', 's', 'd', 'p', '_', 'v', 1, 'x', 'y'}};
  auto result = parseMessageHeader<NodeId>(begin(truncated), end(truncated));
  CHECK(kInvalid == result.first.messageType);
  CHECK(begin(truncated) == result.second);
}

TEST_CASE("MissingProtocolHeader", "[Messages]")
{
  // Buffer should be large enough to proceed but shouldn't match the
  // protocol header
  std::array<char, 32> zeros{};
  auto result = parseMessageHeader<NodeId>(begin(zeros), end(zeros));
  CHECK(kInvalid == result.first.messageType);
  CHECK(begin(zeros) == result.second);
}

TEST_CASE("RoundtripAliveNoPayload", "[Messages]")
{
  std::array<char, 32> buffer;
  const uint8_t ident = 1;
  auto endAlive = aliveMessage(ident, 5, makePayload(), begin(buffer));
  auto result = parseMessageHeader<NodeId>(begin(buffer), endAlive);
  CHECK(endAlive == result.second);
  CHECK(kAlive == result.first.messageType);
  CHECK(5 == result.first.ttl);
  CHECK(ident == result.first.ident);
}

} // namespace v1
} // namespace discovery
} // namespace link
} // namespace ableton
