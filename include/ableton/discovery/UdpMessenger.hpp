// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/IpV4Interface.hpp>
#include <ableton/discovery/Messenger.hpp>
#include <ableton/discovery/v1/Messages.hpp>
#include <ableton/discovery/util/Injected.hpp>
#include <ableton/discovery/util/SafeAsyncHandler.hpp>
#include <ableton/discovery/util/Unwrap.hpp>
#include <algorithm>
#include <memory>

namespace ableton
{
namespace link
{
namespace discovery
{

// An exception thrown when sending a udp message fails. Stores the
// interface through which the sending failed.
struct UdpSendException : std::runtime_error
{
  UdpSendException(const std::runtime_error& e, asio::ip::address ifAddr)
    : std::runtime_error(e.what())
    , interfaceAddr(std::move(ifAddr))
  {
  }

  asio::ip::address interfaceAddr;
};

// Throws UdpSendException
template <typename Interface, typename NodeId, typename Payload>
void sendUdpMessage(
  Interface& iface,
  NodeId from,
  const uint8_t ttl,
  const v1::MessageType messageType,
  const Payload& payload,
  const asio::ip::udp::endpoint& to)
{
  using namespace std;
  v1::MessageBuffer buffer;
  const auto messageBegin = begin(buffer);
  const auto messageEnd =
    v1::detail::encodeMessage(move(from), ttl, messageType, payload, messageBegin);
  const auto numBytes = static_cast<size_t>(distance(messageBegin, messageEnd));
  try
  {
    send(iface, buffer.data(), numBytes, to);
  }
  catch (const std::runtime_error& err)
  {
    throw UdpSendException{err, endpoint(iface).address()};
  }
}

// UdpMessenger uses a "shared_ptr pImpl" pattern to make it moveable
// and to support safe async handler callbacks when receiving messages
// on the given interface.
template <typename Interface, typename StateQuery, typename Timer, typename Log>
struct UdpMessenger
{
  using NodeState = typename std::decay<decltype(std::declval<StateQuery>()())>::type;
  using NodeId = typename NodeState::NodeId;
  using TimerError = typename util::Injected<Timer>::type::ErrorCode;
  using TimePoint = typename util::Injected<Timer>::type::TimePoint;

  UdpMessenger(
    Interface iface,
    StateQuery stateQuery,
    util::Injected<Timer> timer,
    Log log,
    const uint8_t ttl,
    const uint8_t ttlRatio)
    : mpImpl(std::make_shared<Impl>(
        std::move(iface), std::move(stateQuery), std::move(timer), std::move(log), ttl,
        ttlRatio))
  {
    // We need to always listen for incoming traffic in order to
    // respond to peer state broadcasts
    mpImpl->listen(MulticastTag{});
    mpImpl->listen(UnicastTag{});
    mpImpl->broadcastState();
  }

  UdpMessenger(const UdpMessenger&) = delete;
  UdpMessenger& operator=(const UdpMessenger&) = delete;

  UdpMessenger(UdpMessenger&& rhs)
    : mpImpl(std::move(rhs.mpImpl))
  {
  }

  ~UdpMessenger()
  {
    if (mpImpl != nullptr)
    {
      try
      {
        mpImpl->sendByeBye();
      }
      catch (const UdpSendException& err)
      {
        debug(mpImpl->mLog) << "Failed to send bye bye message: " << err.what();
      }
    }
  }

  // throws on failure
  friend void broadcastState(UdpMessenger& m)
  {
    m.mpImpl->broadcastState();
  }

  template <typename Handler>
  friend void receive(UdpMessenger& m, Handler handler)
  {
    m.mpImpl->setReceiveHandler(std::move(handler));
  }

private:

  struct Impl : std::enable_shared_from_this<Impl>
  {
    Impl(
      Interface iface,
      StateQuery stateQuery,
      util::Injected<Timer> timer,
      Log log,
      const uint8_t ttl,
      const uint8_t ttlRatio)
      : mInterface(std::move(iface))
      , mStateQuery(std::move(stateQuery))
      , mTimer(std::move(timer))
      , mLastBroadcastTime{}
      , mLog(std::move(log))
      , mTtl(ttl)
      , mTtlRatio(ttlRatio)
      , mPeerStateHandler([](PeerState<NodeState>) { })
      , mByeByeHandler([](ByeBye<NodeId>) { })
    {
    }

    template <typename Handler>
    void setReceiveHandler(Handler handler)
    {
      mPeerStateHandler = [handler](PeerState<NodeState> peerState) {
        handler(std::move(peerState));
      };

      mByeByeHandler = [handler](ByeBye<NodeId> byeBye) {
        handler(std::move(byeBye));
      };
    }

    void sendByeBye()
    {
      sendUdpMessage(
        mInterface, ident(mStateQuery()), 0, v1::kByeBye, makePayload(),
        multicastEndpoint());
    }

    void broadcastState()
    {
      using namespace std::chrono;

      const auto minBroadcastPeriod = milliseconds{50};
      const auto nominalBroadcastPeriod = milliseconds(mTtl * 1000 / mTtlRatio);
      const auto timeSinceLastBroadcast =
        duration_cast<milliseconds>(mTimer->now() - mLastBroadcastTime);

      // The rate is limited to maxBroadcastRate to prevent flooding the network.
      const auto delay = minBroadcastPeriod - timeSinceLastBroadcast;

      // Schedule the next broadcast before we actually send the
      // message so that if sending throws an exception we are still
      // scheduled to try again. We want to keep trying at our
      // interval as long as this instance is alive.
      mTimer->expires_from_now(delay > milliseconds{0} ? delay : nominalBroadcastPeriod);
      mTimer->async_wait([this](const TimerError e) {
          if (!e)
          {
            broadcastState();
          }
        });

      // If we're not delaying, broadcast now
      if (delay < milliseconds{1})
      {
        debug(mLog) << "Broadcasting state";
        sendPeerState(v1::kAlive, multicastEndpoint());
      }
    }

    void sendPeerState(
      const v1::MessageType messageType,
      const asio::ip::udp::endpoint& to)
    {
      auto state = mStateQuery();
      auto thisId = ident(state);
      sendUdpMessage(
        mInterface, std::move(thisId), mTtl, messageType, toPayload(std::move(state)), to);
      mLastBroadcastTime = mTimer->now();
    }

    void sendResponse(const asio::ip::udp::endpoint& to)
    {
      sendPeerState(v1::kResponse, to);
    }

    template <typename Tag>
    void listen(Tag tag)
    {
      receive(mInterface, util::makeAsyncSafe(this->shared_from_this()), tag);
    }

    template <typename Tag, typename It>
    void operator()(
      Tag tag,
      const asio::ip::udp::endpoint& from,
      const It messageBegin,
      const It messageEnd)
    {
      auto result = v1::parseMessageHeader<NodeId>(messageBegin, messageEnd);

      const auto& header = result.first;
      // Ignore messages from self and other groups
      if (header.ident != ident(mStateQuery()) && header.groupId == 0)
      {
        debug(mLog) << "Received message type " <<
          static_cast<int>(header.messageType) << " from peer " << header.ident;

        switch (header.messageType)
        {
        case v1::kAlive:
          sendResponse(from);
          receivePeerState(std::move(result.first), result.second, messageEnd);
          break;
        case v1::kResponse:
          receivePeerState(std::move(result.first), result.second, messageEnd);
          break;
        case v1::kByeBye:
          receiveByeBye(std::move(result.first.ident));
          break;
        default:
          info(mLog) << "Unknown message received of type: " << header.messageType;
        }
      }
      listen(tag);
    }

    template <typename It>
    void receivePeerState(
      v1::MessageHeader<NodeId> header,
      It payloadBegin,
      It payloadEnd)
    {
      try
      {
        auto state = NodeState::fromPayload(
          std::move(header.ident), std::move(payloadBegin), std::move(payloadEnd), mLog);

        // Handlers must only be called once
        auto handler = std::move(mPeerStateHandler);
        mPeerStateHandler = [](PeerState<NodeState>) { };
        handler(PeerState<NodeState>{std::move(state), header.ttl});
      }
      catch (const std::runtime_error& err)
      {
        info(mLog) << "Ignoring peer state message: " << err.what();
      }
    }

    void receiveByeBye(NodeId nodeId)
    {
      // Handlers must only be called once
      auto byeByeHandler = std::move(mByeByeHandler);
      mByeByeHandler = [](ByeBye<NodeId>) { };
      byeByeHandler(ByeBye<NodeId>{std::move(nodeId)});
    }

    Interface mInterface;
    StateQuery mStateQuery;
    util::Injected<Timer> mTimer;
    TimePoint mLastBroadcastTime;
    Log mLog;
    uint8_t mTtl;
    uint8_t mTtlRatio;
    std::function<void (PeerState<NodeState>)> mPeerStateHandler;
    std::function<void (ByeBye<NodeId>)> mByeByeHandler;
  };

  std::shared_ptr<Impl> mpImpl;
};

// Factory function
template <typename Interface, typename StateQuery, typename Timer, typename Log>
UdpMessenger<Interface, StateQuery, Timer, Log> makeUdpMessenger(
  Interface iface,
  StateQuery query,
  util::Injected<Timer> timer,
  Log log,
  const uint8_t ttl,
  const uint8_t ttlRatio)
{
  return UdpMessenger<Interface, StateQuery, Timer, Log>
    {std::move(iface), std::move(query), std::move(timer), std::move(log), ttl, ttlRatio};
}

} // namespace discovery
} // namespace link
} // namespace ableton
