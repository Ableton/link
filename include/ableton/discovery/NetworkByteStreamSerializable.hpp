// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <type_traits>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

#if PLATFORM_MACOSX
#include <ableton/link/platform/Darwin.hpp>
#elif PLATFORM_LINUX
#include <ableton/link/platform/Linux.hpp>
#elif PLATFORM_WINDOWS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#endif

namespace ableton
{
namespace link
{
namespace discovery
{

// Concept: NetworkByteStreamSerializable
//
// A type that can be encoded to a stream of bytes and decoded from a
// stream of bytes in network byte order. The following type is for
// documentation purposes only.

struct NetworkByteStreamSerializable
{
  friend std::uint32_t sizeInByteStream(const NetworkByteStreamSerializable&);

  // The byte stream pointed to by 'out' must have sufficient space to
  // hold this object, as defined by sizeInByteStream.
  template <typename It>
  friend It toNetworkByteStream(const NetworkByteStreamSerializable&, It out);
};

// Deserialization aspect of the concept. Outside of the demonstration
// type above because clients must specify the type
// explicitly. Default implementation just defers to a class static
// method on T. For types that can't provide such a method, specialize
// this template.
template <typename T>
struct Deserialize
{
  // Throws std::runtime_exception if parsing the type from the given
  // byte range fails. Returns a pair of the correctly parsed value
  // and an iterator to the next byte to parse.
  template <typename It>
  static std::pair<T, It> fromNetworkByteStream(It begin, It end)
  {
    return T::fromNetworkByteStream(std::move(begin), std::move(end));
  }
};


// Default size implementation. Works for primitive types.

template <typename T>
std::uint32_t sizeInByteStream(T)
{
  return sizeof(T);
}

namespace detail
{

// utilities for implementing concept for primitive types

template <typename T, typename It>
It copyToByteStream(T t, It out)
{
  using namespace std;
  return copy_n(
    reinterpret_cast<typename iterator_traits<It>::pointer>(&t), sizeof(t), out);
}

template <typename T, typename It>
std::pair<T, It> copyFromByteStream(It begin, const It end)
{
  using namespace std;
  using ItDiff = typename iterator_traits<It>::difference_type;

  if (distance(begin, end) < static_cast<ItDiff>(sizeof (T)))
  {
    throw range_error("Parsing type from byte stream failed");
  }
  else
  {
    T t;
    const auto n = sizeof(t);
    copy_n(begin, n, reinterpret_cast<uint8_t*>(&t));
    return make_pair(t, begin + n);
  }
}

} // detail


// Model the concept for unsigned integral types

// uint8_t
template <typename It>
It toNetworkByteStream(const uint8_t byte, It out)
{
  return detail::copyToByteStream(byte, std::move(out));
}

template <>
struct Deserialize<uint8_t>
{
  template <typename It>
  static std::pair<uint8_t, It> fromNetworkByteStream(It begin, It end)
  {
    return detail::copyFromByteStream<uint8_t>(std::move(begin), std::move(end));
  }
};

// uint16_t
template <typename It>
It toNetworkByteStream(uint16_t s, It out)
{
  return detail::copyToByteStream(htons(s), std::move(out));
}

template <>
struct Deserialize<uint16_t>
{
  template <typename It>
  static std::pair<uint16_t, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = detail::copyFromByteStream<uint16_t>(std::move(begin), std::move(end));
    result.first = ntohs(result.first);
    return result;
  }
};

// uint32_t
template <typename It>
It toNetworkByteStream(uint32_t l, It out)
{
  return detail::copyToByteStream(htonl(l), std::move(out));
}

template <>
struct Deserialize<uint32_t>
{
  template <typename It>
  static std::pair<uint32_t, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = detail::copyFromByteStream<uint32_t>(std::move(begin), std::move(end));
    result.first = ntohl(result.first);
    return result;
  }
};

// int32_t in terms of uint32_t
template <typename It>
It toNetworkByteStream(int32_t l, It out)
{
  return toNetworkByteStream(reinterpret_cast<const uint32_t&>(l), std::move(out));
}

template <>
struct Deserialize<int32_t>
{
  template <typename It>
  static std::pair<int32_t, It> fromNetworkByteStream(It begin, It end)
  {
    auto result =
      Deserialize<uint32_t>::fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(reinterpret_cast<const int32_t&>(result.first), result.second);
  }
};

// uint64_t
template <typename It>
It toNetworkByteStream(uint64_t ll, It out)
{
  return detail::copyToByteStream(htonll(ll), std::move(out));
}

template <>
struct Deserialize<uint64_t>
{
  template <typename It>
  static std::pair<uint64_t, It> fromNetworkByteStream(It begin, It end)
  {
    auto result = detail::copyFromByteStream<uint64_t>(std::move(begin), std::move(end));
    result.first = ntohll(result.first);
    return result;
  }
};

// int64_t in terms of uint64_t
template <typename It>
It toNetworkByteStream(int64_t ll, It out)
{
  return toNetworkByteStream(reinterpret_cast<const uint64_t&>(ll), std::move(out));
}

template <>
struct Deserialize<int64_t>
{
  template <typename It>
  static std::pair<int64_t, It> fromNetworkByteStream(It begin, It end)
  {
    auto result =
      Deserialize<uint64_t>::fromNetworkByteStream(std::move(begin), std::move(end));
    return std::make_pair(reinterpret_cast<const int64_t&>(result.first), result.second);
  }
};

// overloads for std::chrono durations
template <typename Rep, typename Ratio>
std::uint32_t sizeInByteStream(const std::chrono::duration<Rep, Ratio> dur)
{
  return sizeInByteStream(dur.count());
}

template <typename Rep, typename Ratio, typename It>
It toNetworkByteStream(const std::chrono::duration<Rep, Ratio> dur, It out)
{
  return toNetworkByteStream(dur.count(), std::move(out));
}

template <typename Rep, typename Ratio>
struct Deserialize<std::chrono::duration<Rep, Ratio>>
{
  template <typename It>
  static std::pair<std::chrono::duration<Rep, Ratio>, It> fromNetworkByteStream(
    It begin,
    It end)
  {
    using namespace std;
    auto result = Deserialize<Rep>::fromNetworkByteStream(move(begin), move(end));
    return make_pair(std::chrono::duration<Rep, Ratio>{result.first}, result.second);
  }
};

namespace detail
{

// Generic serialize/deserialize utilities for containers

template <typename Container>
std::uint32_t containerSizeInByteStream(const Container& container)
{
  std::uint32_t totalSize = 0;
  for (const auto& val : container)
  {
    totalSize += sizeInByteStream(val);
  }
  return totalSize;
}

template <typename Container, typename It>
It containerToNetworkByteStream(const Container& container, It out)
{
  for (const auto& val : container)
  {
    out = toNetworkByteStream(val, out);
  }
  return out;
}

template <typename T, typename BytesIt, typename InsertIt>
BytesIt deserializeContainer(
  BytesIt bytesBegin,
  const BytesIt bytesEnd,
  InsertIt contBegin,
  const std::uint32_t maxElements)
{
  using namespace std;
  std::uint32_t numElements = 0;
  while (bytesBegin < bytesEnd && numElements < maxElements)
  {
    T newVal;
    tie(newVal, bytesBegin) = Deserialize<T>::fromNetworkByteStream(bytesBegin, bytesEnd);
    *contBegin++ = newVal;
    ++numElements;
  }
  return bytesBegin;
}

} // detail

// Need specific overloads for each container type, but use above
// utilities for common implementation

// array
template <typename T, std::size_t Size>
std::uint32_t sizeInByteStream(const std::array<T, Size>& arr)
{
  return detail::containerSizeInByteStream(arr);
}

template <typename T, std::size_t Size, typename It>
It toNetworkByteStream(const std::array<T, Size>& arr, It out)
{
  return detail::containerToNetworkByteStream(arr, std::move(out));
}

template <typename T, std::size_t Size>
struct Deserialize<std::array<T, Size>>
{
  template <typename It>
  static std::pair<std::array<T, Size>, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    array<T, Size> result{};
    auto resultIt =
      detail::deserializeContainer<T>(move(begin), move(end), move(result.begin()), Size);
    return make_pair(move(result), move(resultIt));
  }
};

// vector
template <typename T, typename Alloc>
std::uint32_t sizeInByteStream(const std::vector<T, Alloc>& vec)
{
  return detail::containerSizeInByteStream(vec);
}

template <typename T, typename Alloc, typename It>
It toNetworkByteStream(const std::vector<T, Alloc>& vec, It out)
{
  return detail::containerToNetworkByteStream(vec, std::move(out));
}

template <typename T, typename Alloc>
struct Deserialize<std::vector<T, Alloc>>
{
  template <typename It>
  static std::pair<std::vector<T, Alloc>, It> fromNetworkByteStream(
    It bytesBegin, It bytesEnd)
  {
    using namespace std;
    vector<T, Alloc> result;
    // Use the number of bytes remaining in the stream as the upper
    // bound on the number of elements that could be deserialized
    // since we don't have a better heuristic.
    auto resultIt = detail::deserializeContainer<T>(
      move(bytesBegin), move(bytesEnd),
      back_inserter(result), static_cast<uint32_t>(distance(bytesBegin, bytesEnd)));

    return make_pair(move(result), move(resultIt));
  }
};

// 3-tuple
template <typename X, typename Y, typename Z>
std::uint32_t sizeInByteStream(const std::tuple<X, Y, Z>& tup)
{
  return sizeInByteStream(std::get<0>(tup)) +
    sizeInByteStream(std::get<1>(tup)) +
    sizeInByteStream(std::get<2>(tup));
}

template <typename X, typename Y, typename Z, typename It>
It toNetworkByteStream(const std::tuple<X, Y, Z>& tup, It out)
{
  return toNetworkByteStream(
    std::get<2>(tup),
    toNetworkByteStream(
      std::get<1>(tup),
      toNetworkByteStream(
        std::get<0>(tup),
        std::move(out))));
}

template <typename X, typename Y, typename Z>
struct Deserialize<std::tuple<X, Y, Z>>
{
  template <typename It>
  static std::pair<std::tuple<X, Y, Z>, It> fromNetworkByteStream(It begin, It end)
  {
    using namespace std;
    auto xres = Deserialize<X>::fromNetworkByteStream(begin, end);
    auto yres = Deserialize<Y>::fromNetworkByteStream(xres.second, end);
    auto zres = Deserialize<Z>::fromNetworkByteStream(yres.second, end);
    return make_pair(
      std::tuple<X, Y, Z>{move(xres.first), move(yres.first), move(zres.first)},
      move(zres.second));
  }
};

} // namespace discovery
} // namespace link
} // namespace ableton
