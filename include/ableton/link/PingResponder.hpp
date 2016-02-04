// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/PayloadEntries.hpp>
#include <ableton/link/v1/Messages.hpp>
#include <ableton/discovery/util/Injected.hpp>
#include <ableton/discovery/util/SafeAsyncHandler.hpp>
#include <memory>
#include <thread>
#include <chrono>

namespace ableton
{
namespace link
{

template <
  typename NodeStateQuery,
  typename GhostXFormQuery,
  typename Io,
  typename Clock,
  typename Socket,
  typename Log
>
struct PingResponder
{
  PingResponder(
    asio::ip::address_v4 address,
    NodeStateQuery stateQuery,
    GhostXFormQuery ghostXFormQuery,
    util::Injected<Io> io,
    Clock clock,
    Log log)
    : mIo(std::move(io))
    , mpImpl(std::make_shared<Impl>(
      *mIo,
      std::move(address),
      std::move(stateQuery),
      std::move(ghostXFormQuery),
      std::move(clock),
      std::move(log)))
  {
    mpImpl->listen();
  }

  PingResponder(const PingResponder&) = delete;
  PingResponder(PingResponder&&) = delete;

  ~PingResponder()
  {
    // post the release of the impl object into the io service so that
    // it happens in the same thread as its handlers
    auto pImpl = mpImpl;
    mIo->post([pImpl]() mutable {
        pImpl.reset();
      });
  }

  friend asio::ip::udp::endpoint endpoint(const PingResponder& pr)
  {
    return pr.mpImpl->mpSocket->mImpl.local_endpoint();
  }

  friend asio::ip::address address(const PingResponder& pr)
  {
    return endpoint(pr).address();
  }

  friend Socket socket(const PingResponder& pr)
  {
    return *pr.mpImpl->mpSocket;
  }

private:
  struct Impl : std::enable_shared_from_this<Impl>
  {
    Impl(
      typename util::Injected<Io>::type& io,
      asio::ip::address_v4 address,
      NodeStateQuery stateQuery,
      GhostXFormQuery ghostXFormQuery,
      Clock clock,
      Log log)
        : mStateQuery(std::move(stateQuery))
        , mGhostXFormQuery(std::move(ghostXFormQuery))
        , mClock(std::move(clock))
        , mLog(std::move(log))
        , mpSocket(new Socket(io))
    {
      configureUnicastSocket(*mpSocket, address);
    }

    void listen()
    {
      receive(mpSocket, util::makeAsyncSafe(this->shared_from_this()));
    }

    // Operator to handle incoming messages on the interface
    template <typename It>
    void operator()(
      const asio::ip::udp::endpoint& from,
      const It begin,
      const It end)
    {
      using namespace discovery;

      // Decode Ping Message
      const auto result = link::v1::parseMessageHeader(begin, end);
      const auto& header = result.first;
      const auto payloadBegin = result.second;

      // Check Payload size
      const auto payloadSize =
        static_cast<std::size_t>(std::distance(payloadBegin, end));
      const auto maxPayloadSize =
        sizeInByteStream(makePayload(HostTime{}, PrevGHostTime{}));
      if (header.messageType == link::v1::kPing && payloadSize <= maxPayloadSize)
      {
        debug(mLog) << "Received Ping message from " << from;

        try
        {
          reply(std::move(payloadBegin), std::move(end), from);
        }
        catch (const std::runtime_error& err)
        {
          info(mLog) << "Failed to send Pong to " << from << ". Reason: " << err.what();
        }
      }
      else
      {
        info(mLog) << "Received invalid Message from " << from << ".";
      }
      listen();
    }

    template <typename It>
    void reply(It begin, It end, const asio::ip::udp::endpoint& to)
    {
      using namespace discovery;

      // Encode Pong Message
      const auto id = SessionMembership{mStateQuery().sessionId};
      const auto currentGt = GHostTime{hostToGhost(mGhostXFormQuery(), micros(mClock))};
      const auto pongPayload = makePayload(id, currentGt);

      link::v1::MessageBuffer pongBuffer;
      const auto pongMsgBegin = std::begin(pongBuffer);
      auto pongMsgEnd = link::v1::pongMessage(pongPayload, pongMsgBegin);
      // Append ping payload to pong message.
      pongMsgEnd = std::copy(begin, end, pongMsgEnd);

      const auto numBytes =
        static_cast<std::size_t>(std::distance(pongMsgBegin, pongMsgEnd));
      send(mpSocket, pongBuffer.data(), numBytes, to);
    }

    NodeStateQuery mStateQuery;
    GhostXFormQuery mGhostXFormQuery;
    Clock mClock;
    Log mLog;
    std::shared_ptr<Socket> mpSocket;
  };

  util::Injected<Io> mIo;
  std::shared_ptr<Impl> mpImpl;
};

} // namespace link
} // namespace ableton
