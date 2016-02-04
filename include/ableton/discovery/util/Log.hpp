// Copyright: 2014, Ableton AG, Berlin, all rights reserved

#pragma once

#include <iostream>
#include <string>

namespace ableton
{
namespace util
{

// Concept: Log
// Requirements:
//  - copyable
//  - selectors for debug, info, warning, and error streams
//  - channel function that provides new log object tagged with the
//    given channel name


// Null object for the Log concept
struct NullLog
{
  template <typename T>
  friend NullLog& operator<<(NullLog& log, const T&)
  {
    return log;
  }

  friend NullLog& debug(NullLog& log)
  {
    return log;
  }

  friend NullLog& info(NullLog& log)
  {
    return log;
  }

  friend NullLog& warning(NullLog& log)
  {
    return log;
  }

  friend NullLog& error(NullLog& log)
  {
    return log;
  }

  friend NullLog channel(const NullLog&, std::string)
  {
    return {};
  }
};

// std streams-based log
struct StdLog
{
  StdLog(std::string channelName = {})
    : mChannelName(std::move(channelName))
  {
  }

  // Stream type used by std log to prepend the channel name to log messages
  struct StdLogStream
  {
    StdLogStream(std::ostream& ioStream, const std::string& channelName)
      : mpIoStream(&ioStream)
      , mChannelName(channelName)
    {
      ioStream << "[" << mChannelName << "] ";
    }

    StdLogStream(StdLogStream&& rhs)
      : mpIoStream(rhs.mpIoStream)
      , mChannelName(rhs.mChannelName)
    {
      rhs.mpIoStream = nullptr;
    }

    ~StdLogStream()
    {
      if (mpIoStream)
      {
        (*mpIoStream) << "\n";
      }
    }

    template <typename T>
    std::ostream& operator<<(const T& rhs)
    {
      (*mpIoStream) << rhs;
      return *mpIoStream;
    }

    std::ostream* mpIoStream;
    const std::string& mChannelName;
  };

  friend StdLogStream debug(const StdLog& log)
  {
    return {std::clog, log.mChannelName};
  }

  friend StdLogStream info(const StdLog& log)
  {
    return {std::clog, log.mChannelName};
  }

  friend StdLogStream warning(const StdLog& log)
  {
    return {std::clog, log.mChannelName};
  }

  friend StdLogStream error(const StdLog& log)
  {
    return {std::cerr, log.mChannelName};
  }

  friend StdLog channel(const StdLog& log, const std::string& channelName)
  {
    auto compositeName =
      log.mChannelName.empty() ? channelName : log.mChannelName + "::" + channelName;
    return {std::move(compositeName)};
  }

  std::string mChannelName;
};

// Log adapter that adds timestamps
template <typename Log>
struct Timestamped
{
  Timestamped() = default;

  Timestamped(Log log)
    : mLog(std::move(log))
  {
  }

  Log mLog;

  friend decltype(debug(std::declval<Log>())) debug(const Timestamped& log)
  {
    auto stream = debug(log.mLog);
    log.logTimestamp(stream);
    return stream;
  }

  friend decltype(info(std::declval<Log>())) info(const Timestamped& log)
  {
    auto stream = info(log.mLog);
    log.logTimestamp(stream);
    return stream;
  }

  friend decltype(warning(std::declval<Log>())) warning(const Timestamped& log)
  {
    auto stream = warning(log.mLog);
    log.logTimestamp(stream);
    return stream;
  }

  friend decltype(error(std::declval<Log>())) error(const Timestamped& log)
  {
    auto stream = error(log.mLog);
    log.logTimestamp(stream);
    return stream;
  }

  friend Timestamped channel(const Timestamped& log, const std::string& channelName)
  {
    return {channel(log.mLog, channelName)};
  }

  template <typename Stream>
  void logTimestamp(Stream& stream) const
  {
    using namespace std::chrono;
    stream << "|"
           << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
           << "ms| ";
  }

  std::int64_t now() const
  {

  }
};

} // namespace util
} // namespace ableton
