// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/asio/AsioWrapper.hpp>
#include <vector>

namespace ableton
{
namespace link
{
namespace platform
{

// Platform API declaration

// Type flag for the Posix platform API
struct Posix
{
  // Scan active network interfaces and return corresponding addresses
  // for all ip-based interfaces.
  std::vector<asio::ip::address> scanIpIfAddrs();
};

} // namespace platform
} // namespace link
} // namespace ableton

