// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/Controller.hpp>
#include <ableton/link/Tempo.hpp>
#include <ableton/link/platform/Posix.hpp>
#include <ableton/util/AsioService.hpp>
#include <ableton/util/Log.hpp>
#include <chrono>
#include <functional>
#include <platforms/stl/Clock.hpp>

namespace platforms
{
// Not a political statement here. :) The problem is that one of the dependencies defines
// linux=1 in the preprocessor, so "namespace linux" would throw a compiler error.
namespace gnu_linux
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

} // gnu_linux
} // platforms

