// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/Controller.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link/platform/Winsocket.hpp>
#include <ableton/util/AsioService.hpp>
#include <ableton/util/Log.hpp>
#include <platforms/windows/Clock.hpp>

namespace platforms
{
namespace windows
{

using Platform = ableton::link::platform::Winsocket;
using Log = ableton::util::NullLog;
using IoService = std::unique_ptr<ableton::util::AsioService>;

template <typename PeerCountCallback, typename TempoCallback>
using Link = ableton::link::Controller<
  PeerCountCallback,
  TempoCallback,
  Clock,
  Platform,
  Log,
  IoService>;

} // windows
} // platforms
