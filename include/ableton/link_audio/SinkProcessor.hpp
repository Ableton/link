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

#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/Sink.hpp>
#include <ableton/util/Injected.hpp>
#include <memory>
#include <string>

namespace ableton
{
namespace link_audio
{

struct SinkProcessor
{
  SinkProcessor(std::shared_ptr<Sink> pSink)
    : mpImpl(std::make_shared<Impl>(pSink))
  {
  }

  bool process() { return mpImpl->process(); }

  const Id& id() const { return mpImpl->id(); }

  std::string name() const { return mpImpl->name(); }

  bool nameChanged() { return mpImpl->nameChanged(); }

  struct Impl : public std::enable_shared_from_this<Impl>
  {
    Impl(std::shared_ptr<Sink> pSink)
      : mpSink(pSink)
    {
    }

    bool process()
    {
      if (mpSink.use_count() <= 1)
      {
        return false;
      }

      return true;
    }

    const Id& id() const { return mpSink->id(); }

    std::string name() const { return mpSink->name(); }

    bool nameChanged() { return mpSink->nameChanged(); }

  private:
    std::shared_ptr<Sink> mpSink;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
