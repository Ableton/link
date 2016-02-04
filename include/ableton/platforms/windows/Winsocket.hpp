// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <vector>

namespace ableton
{
namespace link
{
namespace platform
{

// Platform API declaration

// Type flag for the Windows platform socket API
struct Winsocket
{
  // Scan active network interfaces and return corresponding addresses
  // for all ip-based interfaces.
  std::vector<asio::ip::address> scanIpIfAddrs();
};

} // namespace platform
} // namespace link
} // namespace ableton
