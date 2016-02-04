// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/Controller.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link/platform/Posix.hpp>
#include <ableton/util/AsioService.hpp>
#include <ableton/util/Log.hpp>
#include <platforms/darwin/Clock.hpp>
#include <chrono>
#include <functional>

namespace platforms
{
namespace darwin
{

using Platform = ableton::link::platform::Posix;
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

} // darwin
} // platforms
