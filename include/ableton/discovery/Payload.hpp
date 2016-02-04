// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/NetworkByteStreamSerializable.hpp>
#include <functional>
#include <unordered_map>

namespace ableton
{
namespace link
{
namespace discovery
{

struct PayloadEntryHeader
{
  using Key = std::uint32_t;
  using Size = std::uint32_t;

  Key key;
  Size size;

  friend Size sizeInByteStream(const PayloadEntryHeader& header)
  {
    return sizeInByteStream(header.key) + sizeInByteStream(header.size);
  }

  template <typename It>
  friend It toNetworkByteStream(const PayloadEntryHeader& header, It out)
  {
    return toNetworkByteStream(
      header.size, toNetworkByteStream(header.key, std::move(out)));
  }

  template <typename It>
  static std::pair<PayloadEntryHeader, It> fromNetworkByteStream(It begin, const It end)
  {
    using namespace std;
    Key key;
    Size size;
    tie(key, begin) = Deserialize<Key>::fromNetworkByteStream(begin, end);
    tie(size, begin) = Deserialize<Size>::fromNetworkByteStream(begin, end);
    return make_pair(PayloadEntryHeader{move(key), move(size)}, move(begin));
   }
};

template <typename EntryType>
struct PayloadEntry
{
  PayloadEntry(EntryType entryVal)
    : value(std::move(entryVal))
  {
    header = {EntryType::key, sizeInByteStream(value)};
  }

  PayloadEntryHeader header;
  EntryType value;

  friend std::uint32_t sizeInByteStream(const PayloadEntry& entry)
  {
    return sizeInByteStream(entry.header) + sizeInByteStream(entry.value);
  }

  template <typename It>
  friend It toNetworkByteStream(const PayloadEntry& entry, It out)
  {
    return toNetworkByteStream(
      entry.value, toNetworkByteStream(entry.header, std::move(out)));
  }
};

namespace detail
{

template <typename It, typename Log>
using HandlerMap =
  std::unordered_map<typename PayloadEntryHeader::Key, std::function<void (It, It, Log)>>;

// Given an index of handlers and a byte range, parse the bytes as a
// sequence of payload entries and invoke the appropriate handler for
// each entry type. Entries that are encountered that do not have a
// corresponding handler in the map are ignored. Throws
// std::runtime_error if parsing fails for any entry. Note that if an
// exception is thrown, some of the handlers may have already been called.
template <typename It, typename Log>
void parseByteStream(HandlerMap<It, Log>& map, It bsBegin, const It bsEnd, Log log)
{
  using namespace std;

  while (bsBegin < bsEnd)
  {
    // Try to parse an entry header at this location in the byte stream
    PayloadEntryHeader header;
    It valueBegin;
    tie(header, valueBegin) =
      Deserialize<PayloadEntryHeader>::fromNetworkByteStream(bsBegin, bsEnd);

    // Ensure that the reported size of the entry does not exceed the
    // length of the byte stream
    It valueEnd = valueBegin + header.size;
    if (bsEnd < valueEnd)
    {
      throw range_error("Partial payload entry with key: " + to_string(header.key));
    }

    // The next entry will start at the end of this one
    bsBegin = valueEnd;

    // Use the appropriate handler for this entry, if available
    auto handlerIt = map.find(header.key);
    if (handlerIt == end(map))
    {
      debug(log) << "Ignored unknown payload entry with key: " << header.key;
    }
    else
    {
      handlerIt->second(move(valueBegin), move(valueEnd), log);
    }
  }
}

} // detail


// Payload encoding
template <typename... Entries>
struct Payload;

template <typename First, typename Rest>
struct Payload<First, Rest>
{
  Payload(First first, Rest rest)
    : mFirst(std::move(first))
    , mRest(std::move(rest))
  {
  }

  Payload(PayloadEntry<First> first, Rest rest)
    : mFirst(std::move(first))
    , mRest(std::move(rest))
  {
  }

  template <typename RhsFirst, typename RhsRest>
  using PayloadSum =
    Payload<First, typename Rest::template PayloadSum<RhsFirst, RhsRest>>;

  // Concatenate payloads together into a single payload
  template <typename RhsFirst, typename RhsRest>
  friend PayloadSum<RhsFirst, RhsRest> operator+(
    Payload lhs,
    Payload<RhsFirst, RhsRest> rhs)
  {
    return {std::move(lhs.mFirst), std::move(lhs.mRest) + std::move(rhs)};
  }

  friend std::size_t sizeInByteStream(const Payload& payload)
  {
    return sizeInByteStream(payload.mFirst) + sizeInByteStream(payload.mRest);
  }

  template <typename It>
  friend It toNetworkByteStream(const Payload& payload, It streamIt)
  {
    return toNetworkByteStream(
      payload.mRest,
      toNetworkByteStream(payload.mFirst, std::move(streamIt)));
  }

  PayloadEntry<First> mFirst;
  Rest mRest;
};

template <>
struct Payload<>
{
  template <typename RhsFirst, typename RhsRest>
  using PayloadSum = Payload<RhsFirst, RhsRest>;

  template <typename RhsFirst, typename RhsRest>
  friend PayloadSum<RhsFirst, RhsRest> operator+(
    Payload,
    Payload<RhsFirst, RhsRest> rhs)
  {
    return rhs;
  }

  friend std::size_t sizeInByteStream(const Payload&)
  {
    return 0;
  }

  template <typename It>
  friend It toNetworkByteStream(const Payload&, It streamIt)
  {
    return streamIt;
  }
};

template <typename... Entries>
struct PayloadBuilder;

// Payload factory function
template <typename... Entries>
auto makePayload(Entries... entries) ->
  decltype(PayloadBuilder<Entries...>{}(std::move(entries)...))
{
  return PayloadBuilder<Entries...>{}(std::move(entries)...);
}

template <typename First, typename... Rest>
struct PayloadBuilder<First, Rest...>
{
  auto operator()(First first, Rest... rest) ->
    Payload<First, decltype(makePayload(std::move(rest)...))>
  {
    return {std::move(first), makePayload(std::move(rest)...)};
  }
};

template <>
struct PayloadBuilder<>
{
  Payload<> operator()()
  {
    return {};
  }
};

// Parse payloads to values
template <typename... Entries>
struct ParsePayload;

template <typename First, typename... Rest>
struct ParsePayload<First, Rest...>
{
  template <typename It, typename Log, typename... Handlers>
  static void parse(It begin, It end, Log log, Handlers... handlers)
  {
    detail::HandlerMap<It, Log> map;
    collectHandlers(map, std::move(handlers)...);
    detail::parseByteStream(map, std::move(begin), std::move(end), std::move(log));
  }

  template <typename It, typename Log, typename FirstHandler, typename... RestHandlers>
  static void collectHandlers(
    detail::HandlerMap<It, Log>& map,
    FirstHandler handler,
    RestHandlers... rest)
  {
    using namespace std;
    map[First::key] = [handler] (const It begin, const It end, Log log) {
      const auto res = First::fromNetworkByteStream(begin, end);
      if (res.second != end)
      {
        warning(log) << "Parsing payload entry " << First::key <<
          " did not consume the expected number of bytes. " <<
          " Expected: " << distance(begin, end) <<
          ", Actual: " << distance(begin, res.second);
      }
      handler(res.first);
    };

    ParsePayload<Rest...>::collectHandlers(map, std::move(rest)...);
  }
};

template <>
struct ParsePayload<>
{
  template <typename It, typename Log>
  static void collectHandlers(detail::HandlerMap<It, Log>&)
  {
  }
};

template <typename... Entries, typename It, typename Log, typename... Handlers>
void parsePayload(It begin, It end, Log log, Handlers... handlers)
{
  ParsePayload<Entries...>::parse(
    std::move(begin), std::move(end), std::move(log), std::move(handlers)...);
}

} // namespace discovery
} // namespace link
} // namespace ableton
