#pragma once

#include <ableton/discovery/Socket.hpp>
#include <ableton/discovery/asio/AsioService.hpp>

namespace ableton
{
namespace link
{
namespace discovery
{

// The IPv4 multicast address and port used by the protocol
inline asio::ip::udp::endpoint multicastEndpoint()
{
  return {asio::ip::address::from_string("224.76.78.75"), 20808};
}

// Type tags for dispatching between unicast and multicast packets
struct MulticastTag {};
struct UnicastTag {};

template <std::size_t MaxPacketSize>
struct IpV4Interface
{
  IpV4Interface(util::AsioService& io, const asio::ip::address_v4& addr)
    : mpMulticastReceiveSocket(std::make_shared<Socket<MaxPacketSize>>(io))
    , mpSendSocket(std::make_shared<Socket<MaxPacketSize>>(io))
  {
    configureMulticastSocket(*mpMulticastReceiveSocket, addr, multicastEndpoint());
    configureUnicastSocket(*mpSendSocket, addr);
  }

  friend std::size_t send(
    IpV4Interface& iface,
    const uint8_t* const pData,
    const size_t numBytes,
    const asio::ip::udp::endpoint& to)
  {
    return send(iface.mpSendSocket, pData, numBytes, to);
  }

  template <typename Handler>
  friend void receive(IpV4Interface& iface, Handler handler, UnicastTag)
  {
    receive(iface.mpSendSocket, SocketReceiver<UnicastTag, Handler>{std::move(handler)});
  }

  template <typename Handler>
  friend void receive(IpV4Interface& iface, Handler handler, MulticastTag)
  {
    receive(
      iface.mpMulticastReceiveSocket,
      SocketReceiver<MulticastTag, Handler>(std::move(handler)));
  }

  friend asio::ip::udp::endpoint endpoint(IpV4Interface& iface)
  {
    return iface.mpSendSocket->mImpl.local_endpoint();
  }

private:

  template <typename Tag, typename Handler>
  struct SocketReceiver
  {
    SocketReceiver(Handler handler)
      : mHandler(std::move(handler))
    {
    }

    template <typename It>
    void operator()(
      const asio::ip::udp::endpoint& from,
      const It messageBegin,
      const It messageEnd)
    {
      mHandler(Tag{}, from, messageBegin, messageEnd);
    }

    Handler mHandler;
  };

  // We use shared_ptr for sockets because they are not movable and
  // because it's necessary for compatibility with util::makeAsyncSafe
  using SocketPtr = std::shared_ptr<Socket<MaxPacketSize>>;
  SocketPtr mpMulticastReceiveSocket;
  SocketPtr mpSendSocket;
};

} // namespace discovery
} // namespace link
} // namespace ableton
