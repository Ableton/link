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

#include <ableton/link_audio/Buffer.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/Queue.hpp>
#include <ableton/util/Locked.hpp>

#include <atomic>
#include <optional>

namespace ableton
{
namespace link_audio
{

static constexpr auto kSourceQueueSize = std::chrono::milliseconds(10000);

struct Source
{
  using Callback = std::function<void(BufferCallbackHandle<Buffer<int16_t>>)>;

  Source(Id id, Callback callback)
    : mId(std::move(id))
    , mCallback(std::move(callback))
  {
  }

  const Id& id() const { return mId; }


  void setCallback(Callback newCallback)
  {
    mCallback.update([newCallback_ = std::move(newCallback)](auto& callback)
                     { callback = std::move(newCallback_); });
  }

  void callback(BufferCallbackHandle<Buffer<int16_t>> buffer)
  {
    mCallback.update([&](auto& callback) { callback(buffer); });
  }

private:
  Id mId;
  util::Locked<Callback> mCallback;
};

} // namespace link_audio
} // namespace ableton
