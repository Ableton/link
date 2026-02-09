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
#include <ableton/link_audio/ChannelRequests.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/PCMCodec.hpp>
#include <ableton/link_audio/Source.hpp>
#include <ableton/link_audio/v1/Messages.hpp>
#include <ableton/util/Injected.hpp>
#include <optional>
#include <string>

namespace ableton
{
namespace link_audio
{

template <typename GetSender, typename GetNodeId, typename IoContext>
struct SourceProcessor
{
  using Timer = typename util::Injected<IoContext>::type::Timer;
  static constexpr uint8_t kTtl = 5;

  SourceProcessor(util::Injected<IoContext> io,
                  std::shared_ptr<Source> pSource,
                  util::Injected<GetSender> getSender,
                  util::Injected<GetNodeId> getNodeId)
    : mpImpl(std::make_shared<Impl>(
        std::move(io), pSource, std::move(getSender), std::move(getNodeId)))
  {
    mpImpl->sendAudioRequest();
  }

  ~SourceProcessor()
  {
    if (mpImpl != nullptr)
    {
      mpImpl->sendChannelStopRequest();
    }
  }

  bool process() { return mpImpl->process(); }

  void receiveAudioBuffer(const AudioBuffer& buffer)
  {
    mpImpl->receiveAudioBuffer(buffer);
  }

  const Id& id() const { return mpImpl->id(); }

  struct Impl : public std::enable_shared_from_this<Impl>
  {
    Impl(util::Injected<IoContext> io,
         std::shared_ptr<Source> pSource,
         util::Injected<GetSender> getSender,
         util::Injected<GetNodeId> getNodeId)
      : mTimer(io->makeTimer())
      , mpSource(pSource)
      , mGetSender(std::move(getSender))
      , mGetNodeId(std::move(getNodeId))
      , mBuffer(4096 * 2)
      , mDecoder(util::injectVal(Callback{this}), 4096)
    {
    }

    template <typename Payload>
    void sendMessage(const Payload& payload,
                     const v1::MessageType messageType,
                     const uint8_t ttl)
    {
      v1::MessageBuffer buffer;
      const auto messageBegin = buffer.begin();
      const auto messageEnd = v1::detail::encodeMessage(
        (*mGetNodeId)(), ttl, messageType, payload, messageBegin);
      const auto numBytes = static_cast<size_t>(std::distance(messageBegin, messageEnd));
      auto oSender = mGetSender->forChannel(mpSource->id());
      if (oSender)
      {
        try
        {
          (*oSender)(buffer.data(), numBytes);
        }
        catch (const std::runtime_error&)
        {
        }
      }
    }

    void sendAudioRequest()
    {
      mTimer.expires_from_now(std::chrono::seconds(kTtl));
      mTimer.async_wait(
        [this](const auto e)
        {
          if (!e)
          {
            sendAudioRequest();
          }
        });

      const auto request = ChannelRequest{(*mGetNodeId)(), mpSource->id()};
      sendMessage(toPayload(request), v1::kChannelRequest, kTtl);
    }

    void sendChannelStopRequest()
    {
      const auto stopRequest = ChannelStopRequest{(*mGetNodeId)(), mpSource->id()};
      sendMessage(toPayload(stopRequest), v1::kStopChannelRequest, 0);
    }

    bool process() { return mpSource.use_count() > 1; }

    void receiveAudioBuffer(const AudioBuffer& buffer) { mDecoder(buffer); }

    const Id& id() const { return mpSource->id(); }

  private:
    struct Callback
    {
      void operator()(BufferCallbackHandle<Buffer<int16_t>> buffer)
      {
        pImpl->mpSource->callback(buffer);
      }

      Impl* pImpl;
    };

    Timer mTimer;
    std::shared_ptr<Source> mpSource;
    util::Injected<GetSender> mGetSender;
    util::Injected<GetNodeId> mGetNodeId;
    Buffer<int16_t> mBuffer;
    PCMDecoder<int16_t, Callback> mDecoder;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
