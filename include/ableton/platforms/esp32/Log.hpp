/* Copyright 2025, All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <esp_log.h>
#include <sstream>
#include <string>

namespace ableton
{
namespace platforms
{
namespace esp32
{

// ESP-IDF logging implementation for Ableton Link
struct EspLog
{
  enum class Level
  {
    Debug,
    Info,
    Warning,
    Error
  };

  // Tag used for all Link logging
  //
  // All Link logging should go through this tag, since ESP-IDF has a lot of
  // internal logging and this helps to manage being able to filter.
  //
  // ESP-IDF has no wildcard filtering support, so for channels we prepend the
  // channel name to the message text, since channels can be created based on
  // dynamic data, like IP addresses.
  static constexpr const char* kTag = "ableton_link";

  EspLog() = default;

  explicit EspLog(std::string context)
    : mContext(std::move(context))
  {
  }

  struct EspLogStream
  {
    EspLogStream(Level level, const std::string& context)
      : mLevel(level)
      , mContext(context)
    {
    }

    EspLogStream(EspLogStream&& rhs)
      : mLevel(rhs.mLevel)
      , mContext(rhs.mContext)
      , mStream(std::move(rhs.mStream))
    {
      rhs.mMoved = true;
    }

    ~EspLogStream()
    {
      if (!mMoved)
      {
        std::string msg = mStream.str();
        // Prepend context to message if present
        if (!mContext.empty())
        {
          msg = "[" + mContext + "] " + msg;
        }
        switch (mLevel)
        {
        case Level::Debug:
          ESP_LOGD(kTag, "%s", msg.c_str());
          break;
        case Level::Info:
          ESP_LOGI(kTag, "%s", msg.c_str());
          break;
        case Level::Warning:
          ESP_LOGW(kTag, "%s", msg.c_str());
          break;
        case Level::Error:
          ESP_LOGE(kTag, "%s", msg.c_str());
          break;
        }
      }
    }

    template <typename T>
    EspLogStream& operator<<(const T& rhs)
    {
      mStream << rhs;
      return *this;
    }

    Level mLevel;
    const std::string& mContext;
    std::ostringstream mStream;
    bool mMoved = false;
  };

  friend EspLogStream debug(const EspLog& log) { return {Level::Debug, log.mContext}; }

  friend EspLogStream info(const EspLog& log) { return {Level::Info, log.mContext}; }

  friend EspLogStream warning(const EspLog& log)
  {
    return {Level::Warning, log.mContext};
  }

  friend EspLogStream error(const EspLog& log) { return {Level::Error, log.mContext}; }

  friend EspLog channel(const EspLog& log, const std::string& channelName)
  {
    auto newContext =
      log.mContext.empty() ? channelName : log.mContext + "::" + channelName;
    return EspLog{std::move(newContext)};
  }

  std::string mContext;
};

} // namespace esp32
} // namespace platforms
} // namespace ableton
