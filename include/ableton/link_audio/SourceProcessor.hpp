/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */

#pragma once

#include <ableton/link_audio/ChannelAnnouncements.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/Source.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/util/Injected.hpp>
#include <optional>
#include <string>

namespace ableton
{
namespace link_audio
{

template <typename IoContext>
struct SourceProcessor
{
  using Timer = typename util::Injected<IoContext>::type::Timer;
  static constexpr uint8_t kTtl = 5;

  SourceProcessor(util::Injected<IoContext> io, std::shared_ptr<Source> pSource)
    : mpImpl(std::make_shared<Impl>(std::move(io), pSource))
  {
  }

  bool process() { return mpImpl->process(); }

  const Id& id() const { return mpImpl->id(); }

  struct Impl : public std::enable_shared_from_this<Impl>
  {
    Impl(util::Injected<IoContext> io, std::shared_ptr<Source> pSource)
      : mTimer(io->makeTimer())
      , mpSource(pSource)
    {
    }

    bool process() { return mpSource.use_count() > 1; }

    const Id& id() const { return mpSource->id(); }

  private:
    Timer mTimer;
    std::shared_ptr<Source> mpSource;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
