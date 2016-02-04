// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <utility>
#include <cstdint>

namespace ableton
{
namespace link
{
namespace discovery
{
namespace test
{

// Test payload entries

// A fixed-size entry type
struct Foo
{
  enum { key = '_foo' };

  std::int32_t fooVal;

  friend std::uint32_t sizeInByteStream(const Foo& foo)
  {
    // Namespace qualification is needed to avoid ambiguous function definitions
    return discovery::sizeInByteStream(foo.fooVal);
  }

  template <typename It>
  friend It toNetworkByteStream(const Foo& foo, It out)
  {
    return discovery::toNetworkByteStream(foo.fooVal, std::move(out));
  }

  template <typename It>
  static std::pair<Foo, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = Deserialize<decltype(fooVal)>::
      fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(Foo{std::move(result.first)}, std::move(result.second));
  }
};

// A variable-size entry type
struct Bar
{
  enum { key = '_bar' };

  std::vector<std::uint64_t> barVals;

  friend std::uint32_t sizeInByteStream(const Bar& bar)
  {
    return discovery::sizeInByteStream(bar.barVals);
  }

  template <typename It>
  friend It toNetworkByteStream(const Bar& bar, It out)
  {
    return discovery::toNetworkByteStream(bar.barVals, out);
  }

  template <typename It>
  static std::pair<Bar, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = Deserialize<decltype(barVals)>::
      fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(Bar{std::move(result.first)}, std::move(result.second));
  }
};

} // namespace test
} // namespace discovery
} // namespace link
} // namespace ableton
