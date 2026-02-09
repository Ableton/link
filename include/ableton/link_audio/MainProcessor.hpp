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
#include <ableton/link_audio/Sink.hpp>
#include <ableton/link_audio/SinkProcessor.hpp>
#include <ableton/link_audio/Source.hpp>
#include <ableton/link_audio/SourceProcessor.hpp>
#include <ableton/util/Injected.hpp>
#include <ableton/util/SafeAsyncHandler.hpp>

namespace ableton
{
namespace link_audio
{

using SharedSink = std::shared_ptr<Sink>;
using SharedSource = std::shared_ptr<Source>;

template <typename ChannelsChangedCallback,
          typename GetSender,
          typename GetNodeId,
          typename Random,
          typename IoContext>
struct MainProcessor
{
  using IoType = typename util::Injected<IoContext>::type;

  static constexpr auto kProcessTimerPeriod = std::chrono::milliseconds(1);

  MainProcessor(util::Injected<IoContext> io,
                util::Injected<ChannelsChangedCallback> callback)
    : mpImpl(std::make_shared<Impl>(std::move(io), std::move(callback)))
  {
  }

  ~MainProcessor() { stop(); }

  void stop() { mpImpl->stop(); }

  void addSink(SharedSink pSink,
               util::Injected<GetSender> getSender,
               util::Injected<GetNodeId> getNodeId)
  {
    mpImpl->addSink(pSink, std::move(getSender), std::move(getNodeId));
  }

  void addSource(SharedSource pSource,
                 util::Injected<GetSender> getSender,
                 util::Injected<GetNodeId> getNodeId)
  {
    mpImpl->addSource(pSource, std::move(getSender), std::move(getNodeId));
  }

  ChannelAnnouncements channelAnnouncements() const
  {
    return mpImpl->channelAnnouncements();
  }

  template <typename Request>
  void receiveChannelRequest(Request request, uint8_t ttl)
  {
    mpImpl->receiveChannelRequest(std::move(request), ttl);
  }

  template <typename It>
  void receiveAudioBuffer(It begin, It end)
  {
    mpImpl->receiveAudioBuffer(begin, end);
  }

private:
  struct Impl : std::enable_shared_from_this<Impl>
  {
    using Timer = typename util::Injected<IoContext>::type::Timer;
    using MainSinkProcessor = SinkProcessor<GetSender, GetNodeId, IoContext&>;
    using MainSourceProcessor = SourceProcessor<GetSender, GetNodeId, IoContext&>;

    Impl(util::Injected<IoContext> io, util::Injected<ChannelsChangedCallback> callback)
      : mIo{std::move(io)}
      , mChannelsChangedCallback{std::move(callback)}
      , mProcessTimer{mIo->makeTimer()}
    {
    }

    void addSink(SharedSink pSink,
                 util::Injected<GetSender> getSender,
                 util::Injected<GetNodeId> getNodeId)
    {
      auto processor = std::make_unique<MainSinkProcessor>(
        util::injectRef(*mIo), pSink, std::move(getSender), std::move(getNodeId));
      mSinks.push_back(std::move(processor));
      start();
    }

    void addSource(SharedSource pSource,
                   util::Injected<GetSender> getSender,
                   util::Injected<GetNodeId> getNodeId)
    {
      auto pProcessor = std::make_unique<MainSourceProcessor>(
        util::injectRef(*mIo), pSource, std::move(getSender), std::move(getNodeId));
      mSources.push_back(std::move(pProcessor));
      start();
    }

    void start() { (*this)(); }

    void stop() { mProcessTimer.cancel(); }

    void operator()()
    {
      processSinks();
      processSources();
      if (!mSinks.empty() || !mSources.empty())
      {
        mProcessTimer.expires_from_now(kProcessTimerPeriod);
        mProcessTimer.async_wait(
          [this](const auto e)
          {
            if (!e)
            {
              (*this)();
            }
          });
      }
    }

    void processSinks()
    {
      auto channelsChanged = false;

      for (auto it = mSinks.begin(); it != mSinks.end();)
      {
        if ((*it)->nameChanged())
        {
          channelsChanged = true;
        }

        if ((*it)->process())
        {
          ++it;
        }
        else
        {
          it = mSinks.erase(it);
          channelsChanged = true;
        }
      }

      if (channelsChanged)
      {
        (*mChannelsChangedCallback)();
      }
    }

    void processSources()
    {
      auto channelsChanged = false;

      for (auto it = mSources.begin(); it != mSources.end();)
      {
        if ((*it)->process())
        {
          ++it;
        }
        else
        {
          it = mSources.erase(it);
        }
      }
    }

    ChannelAnnouncements channelAnnouncements() const
    {
      auto announcements = ChannelAnnouncements{};
      for (auto& sink : mSinks)
      {
        announcements.channels.emplace_back(
          ChannelAnnouncement{sink->name(), sink->id()});
      }
      return announcements;
    }

    template <typename Request>
    void receiveChannelRequest(Request request, uint8_t ttl)
    {
      auto it =
        std::find_if(mSinks.begin(),
                     mSinks.end(),
                     [&](const auto& pSink) { return request.channelId == pSink->id(); });
      if (it != mSinks.end())
      {
        it->get()->receiveChannelRequest(std::move(request), ttl);
      }
    }

    template <typename It>
    void receiveAudioBuffer(It begin, It end)
    {
      static auto audioBuffer = AudioBuffer{};
      AudioBuffer::fromNetworkByteStream(audioBuffer, begin, end);

      auto it = std::find_if(mSources.begin(),
                             mSources.end(),
                             [&](const auto& pSource)
                             { return audioBuffer.channelId == pSource->id(); });
      if (it != mSources.end())
      {
        it->get()->receiveAudioBuffer(audioBuffer);
      }
    }

    util::Injected<IoContext> mIo;
    util::Injected<ChannelsChangedCallback> mChannelsChangedCallback;
    std::vector<std::unique_ptr<MainSinkProcessor>> mSinks;
    std::vector<std::unique_ptr<MainSourceProcessor>> mSources;
    Timer mProcessTimer;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
