// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/Payload.hpp>
#include <ableton/discovery/test/PayloadEntries.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <array>

namespace ableton
{
namespace link
{
namespace discovery
{

TEST_CASE("Payload | EmptyPayload", "[Payload]")
{
  CHECK(0 == sizeInByteStream(makePayload()));
  CHECK(nullptr == toNetworkByteStream(makePayload(), nullptr));
}

TEST_CASE("Payload | FixedSizeEntryPayloadSize", "[Payload]")
{
  CHECK(12 == sizeInByteStream(makePayload(test::Foo{})));
}

TEST_CASE("Payload | SingleEntryPayloadEncoding", "[Payload]")
{
  const auto payload = makePayload(test::Foo{-1});
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  // Should have filled the buffer with the payload
  CHECK(bytes.size() == static_cast<size_t>(end - begin(bytes)));
  // Should have encoded the value 1 after the payload entry header
  // as an unsigned in network byte order
  const auto uresult = ntohl(reinterpret_cast<const std::uint32_t&>(*(begin(bytes) + 8)));
  CHECK(-1 == reinterpret_cast<const std::int32_t&>(uresult));
}

TEST_CASE("Payload | DoubleEntryPayloadSize", "[Payload]")
{
  CHECK(44 == sizeInByteStream(makePayload(test::Foo{}, test::Bar{{0, 1, 2}})));
}

TEST_CASE("Payload | DoubleEntryPayloadEncoding", "[Payload]")
{
  const auto payload = makePayload(test::Foo{1}, test::Bar{{0, 1, 2}});
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  // Should have filled the buffer with the payload
  CHECK(bytes.size() == static_cast<size_t>(end - begin(bytes)));
  // Should have encoded the value 1 after the first payload entry header
  // and 0,1,2 after the second payload entry header in network byte
  // order
  CHECK(1 == ntohl(reinterpret_cast<const std::uint32_t&>(*(begin(bytes) + 8))));
  CHECK(0 == ntohll(reinterpret_cast<const std::uint64_t&>(*(begin(bytes) + 20))));
  CHECK(1 == ntohll(reinterpret_cast<const std::uint64_t&>(*(begin(bytes) + 28))));
  CHECK(2 == ntohll(reinterpret_cast<const std::uint64_t&>(*(begin(bytes) + 36))));
}

TEST_CASE("Payload | RoundtripSingleEntry", "[Payload]")
{
  const auto expected = test::Foo{1};
  const auto payload = makePayload(expected);
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  test::Foo actual{};
  parsePayload<test::Foo>(
    begin(bytes), end, util::NullLog{},
    [&actual] (const test::Foo& foo) {
      actual = foo;
    });

  CHECK(expected.fooVal == actual.fooVal);
}

TEST_CASE("Payload | RoundtripDoubleEntry", "[Payload]")
{
  const auto expectedFoo = test::Foo{1};
  const auto expectedBar = test::Bar{{0, 1, 2}};
  const auto payload = makePayload(expectedBar, expectedFoo);
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  test::Foo actualFoo{};
  test::Bar actualBar{};
  parsePayload<test::Foo, test::Bar>(
    begin(bytes), end, util::NullLog{},
    [&actualFoo] (const test::Foo& foo) {
      actualFoo = foo;
    },
    [&actualBar] (const test::Bar& bar) {
      actualBar = bar;
    });

  CHECK(expectedFoo.fooVal == actualFoo.fooVal);
  CHECK(expectedBar.barVals == actualBar.barVals);
}

TEST_CASE("Payload | ParseSubset", "[Payload]")
{
  // Encode two payload entries
  const auto expectedFoo = test::Foo{1};
  const auto expectedBar = test::Bar{{0, 1, 2}};
  const auto payload = makePayload(expectedFoo, expectedBar);
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  // Only decode one of them
  test::Bar actualBar{};
  parsePayload<test::Bar>(
    begin(bytes), end, util::NullLog{},
    [&actualBar] (const test::Bar& bar) {
      actualBar = bar;
    });

  CHECK(expectedBar.barVals == actualBar.barVals);
}

TEST_CASE("Payload | ParseTruncatedEntry", "[Payload]")
{
  const auto expectedFoo = test::Foo{1};
  const auto expectedBar = test::Bar{{0, 1, 2}};
  const auto payload = makePayload(expectedBar, expectedFoo);
  std::vector<char> bytes(sizeInByteStream(payload));
  const auto end = toNetworkByteStream(payload, begin(bytes));

  test::Foo actualFoo{};
  test::Bar actualBar{};

  REQUIRE_THROWS_AS(
  (  // We truncate the buffer by one byte
    parsePayload<test::Foo, test::Bar>(
      begin(bytes), end - 1, util::NullLog{},
      [&actualFoo] (const test::Foo& foo) {
        actualFoo = foo;
      },
      [&actualBar] (const test::Bar& bar) {
        actualBar = bar;
      })
  ), std::runtime_error);

  // We expect that bar should be properly parsed but foo not
  CHECK(0 == actualFoo.fooVal);
  CHECK(expectedBar.barVals == actualBar.barVals);
}

TEST_CASE("Payload | AddPayloads", "[Payload]")
{
  // The sum of a foo payload and a bar payload should be equal in
  // every way to a foobar payload
  const auto foo = test::Foo{1};
  const auto bar = test::Bar{{0, 1, 2}};
  const auto fooBarPayload = makePayload(foo, bar);
  const auto sumPayload = makePayload(foo) + makePayload(bar);

  REQUIRE(sizeInByteStream(fooBarPayload) == sizeInByteStream(sumPayload));

  std::vector<char> fooBarBytes(sizeInByteStream(fooBarPayload));
  std::vector<char> sumBytes(sizeInByteStream(sumPayload));

  const auto fooBarEnd = toNetworkByteStream(fooBarPayload, begin(fooBarBytes));
  toNetworkByteStream(sumPayload, begin(sumBytes));

  CHECK(std::equal(begin(fooBarBytes), fooBarEnd, begin(sumBytes)));
}

} // namespace discovery
} // namespace link
} // namespace ableton
