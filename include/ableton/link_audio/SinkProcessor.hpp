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

#include <ableton/link_audio/Encoder.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/Receivers.hpp>
#include <ableton/link_audio/Sink.hpp>
#include <ableton/util/Injected.hpp>
#include <memory>
#include <string>

namespace ableton
{
namespace link_audio
{

template <typename GetSender, typename GetNodeId, typename IoContext>
struct SinkProcessor
{
  SinkProcessor(util::Injected<IoContext> io,
                std::shared_ptr<Sink> pSink,
                util::Injected<GetSender> getSender,
                util::Injected<GetNodeId> getNodeId)
    : mpImpl(std::make_shared<Impl>(
        std::move(io), pSink, std::move(getSender), std::move(getNodeId)))
  {
  }

  bool process() { return mpImpl->process(); }

  const Id& id() const { return mpImpl->id(); }

  std::string name() const { return mpImpl->name(); }

  bool nameChanged() { return mpImpl->nameChanged(); }

  template <typename Request>
  void receiveChannelRequest(Request request, uint8_t ttl)
  {
    mpImpl->receiveChannelRequest(std::move(request), ttl);
  }

  struct Impl : public std::enable_shared_from_this<Impl>
  {
    struct Sender
    {
      void operator()(const AudioBuffer& buffer)
      {
        auto end =
          v1::audioBufferMessage((*mpImpl->mGetNodeId)(), buffer, mBuffer.begin());
        try
        {
          mpImpl->mReceivers(mBuffer.data(), std::distance(mBuffer.begin(), end));
        }
        catch (const std::runtime_error& err)
        {
          debug(mpImpl->mIo->log()) << "Failed to send message: " << err.what();
        }
      }

      Impl* mpImpl;
      std::array<uint8_t, v1::kMaxMessageSize> mBuffer{};
    };

    Impl(util::Injected<IoContext> io,
         std::shared_ptr<Sink> pSink,
         util::Injected<GetSender> getSender,
         util::Injected<GetNodeId> getNodeId)
      : mIo(std::move(io))
      , mpSink(pSink)
      , mQueueReader(pSink->reader())
      , mEncoder(util::injectVal(Sender{this}), mpSink->id())
      , mReceivers(util::injectRef(*mIo), std::move(getSender))
      , mGetNodeId(std::move(getNodeId))
    {
    }

    bool process()
    {
      if (mpSink.use_count() <= 1)
      {
        return false;
      }

      while (mQueueReader.retainSlot())
      {
        if (!mReceivers.empty() && mQueueReader[0]->mTempo > link::Tempo{0})
        {
          mEncoder(*mQueueReader[0]);
        }
        if (mQueueReader[0]->mSamples.size() < mpSink->maxNumSamples())
        {
          mQueueReader[0]->mSamples.resize(mpSink->maxNumSamples());
        }
        mQueueReader.releaseSlot();
      }

      return true;
    }

    const Id& id() const { return mpSink->id(); }

    std::string name() const { return mpSink->name(); }

    bool nameChanged() { return mpSink->nameChanged(); }

    template <typename Request>
    void receiveChannelRequest(Request request, uint8_t ttl)
    {
      mReceivers.receiveChannelRequest(std::move(request), ttl);
    }

  private:
    util::Injected<IoContext> mIo;
    std::shared_ptr<Sink> mpSink;
    Queue<Buffer<int16_t>>::Reader mQueueReader;
    Encoder<Sender, int16_t> mEncoder;
    Receivers<GetSender, IoContext> mReceivers;
    util::Injected<GetNodeId> mGetNodeId;
  };

  std::shared_ptr<Impl> mpImpl;
};

} // namespace link_audio
} // namespace ableton
