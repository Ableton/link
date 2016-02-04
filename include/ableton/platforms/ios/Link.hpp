// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <platforms/darwin/Link.hpp>

using PeerCountCallback = ableton::link::PeerCountCallback;
using TempoCallback = ableton::link::TempoCallback;

using Link = platforms::darwin::Link<PeerCountCallback, TempoCallback>;

using Clock = platforms::darwin::Clock;
using Platform = platforms::darwin::Platform;
using Log = platforms::darwin::Log;
using IoService = platforms::darwin::IoService;
