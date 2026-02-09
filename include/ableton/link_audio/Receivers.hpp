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

#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/util/Injected.hpp>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename GetSender, typename IoContext>
class Receivers
{
  using Timer = typename util::Injected<IoContext>::type::Timer;
  using TimePoint = typename Timer::TimePoint;
  using OptionalSendHandler =
    decltype(std::declval<GetSender>().forChannel(std::declval<Id>()));

  struct Receiver
  {
    OptionalSendHandler sendHandler;
    ChannelRequest request;
    TimePoint lastRequest;
  };

public:
  Receivers(util::Injected<IoContext> io, util::Injected<GetSender> getSender)
    : mpImpl(std::make_shared<Impl>(std::move(io), std::move(getSender)))
  {
  }

  void receiveChannelRequest(ChannelRequest request, uint8_t ttl)
  {
    mpImpl->receive(std::move(request), ttl);
  }

  void receiveChannelRequest(ChannelStopRequest stopRequest, uint8_t ttl)
  {
    mpImpl->receive(std::move(stopRequest), ttl);
  }

  void operator()(const uint8_t* const pData, const size_t numBytes)
  {
    (*mpImpl)(pData, numBytes);
  }

  bool empty() const { return mpImpl->empty(); }

private:
  struct Impl
  {
    Impl(util::Injected<IoContext> io, util::Injected<GetSender> getSender)
      : mPruneTimer(io->makeTimer())
      , mGetSender(std::move(getSender))
    {
    }

    void receive(ChannelRequest request, uint8_t ttl)
    {
      const auto it =
        std::find_if(mReceivers.begin(),
                     mReceivers.end(),
                     [&](auto& r) { return r.request.peerId == request.peerId; });
      if (it != mReceivers.end())
      {
        mReceivers.erase(it);
      }

      const auto now = mPruneTimer.now();
      auto receiver = Receiver{
        mGetSender->forPeer(request.peerId), request, now + std::chrono::seconds(ttl)};

      mReceivers.insert(upper_bound(mReceivers.begin(),
                                    mReceivers.end(),
                                    receiver,
                                    [](const auto& r1, const auto& r2)
                                    { return r1.lastRequest < r2.lastRequest; }),
                        std::move(receiver));

      scheduleNextPruning();
    }

    void receive(ChannelStopRequest stopRequest, uint8_t)
    {
      const auto it = std::find_if(mReceivers.begin(),
                                   mReceivers.end(),
                                   [&](const auto& r)
                                   { return r.request.peerId == stopRequest.peerId; });
      if (it != mReceivers.end())
      {
        mReceivers.erase(it);
      };
    }

    void pruneExpiredReceivers()
    {
      const auto test = Receiver{{}, {}, mPruneTimer.now()};

      const auto endExpired = std::lower_bound(
        mReceivers.begin(),
        mReceivers.end(),
        test,
        [](const auto& r1, const auto& r2) { return r1.lastRequest < r2.lastRequest; });

      mReceivers.erase(mReceivers.begin(), endExpired);
      scheduleNextPruning();
    }

    void scheduleNextPruning()
    {
      if (!mReceivers.empty())
      {
        const auto t = mReceivers.front().lastRequest + std::chrono::seconds(1);

        mPruneTimer.expires_at(t);
        mPruneTimer.async_wait(
          [this](const auto e)
          {
            if (!e)
            {
              pruneExpiredReceivers();
            }
          });
      }
    }

    void operator()(const uint8_t* const pData, const size_t numBytes)
    {
      for (auto& receiver : mReceivers)
      {
        if (auto& sendHandler = receiver.sendHandler)
        {
          (*sendHandler)(pData, numBytes);
        }
      }
    }

    bool empty() const { return mReceivers.empty(); }

    Timer mPruneTimer;
    util::Injected<GetSender> mGetSender;
    std::vector<Receiver> mReceivers; // Invariant: sorted by time_point
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
