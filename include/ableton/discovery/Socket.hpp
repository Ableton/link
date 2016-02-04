// Copyright: 2016, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioService.hpp>
#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/util/SafeAsyncHandler.hpp>
#include <array>
#include <cassert>

namespace ableton
{
namespace link
{

template <std::size_t MaxPacketSize>
struct Socket
{
  Socket(util::AsioService& io)
    : mImpl(io.mService, asio::ip::udp::v4())
  {
  }

  ~Socket()
  {
    // Ignore error codes in shutdown and close as the socket may
    // have already been forcibly closed
    asio::error_code ec;
    mImpl.shutdown(asio::ip::udp::socket::shutdown_both, ec);
    mImpl.close(ec);
  }

  friend std::size_t send(
    const std::shared_ptr<Socket>& socket,
    const uint8_t* const pData,
    const size_t numBytes,
    const asio::ip::udp::endpoint& to)
  {
    assert(numBytes < MaxPacketSize);
    return socket->mImpl.send_to(asio::buffer(pData, numBytes), to);
  }

  template <typename Handler>
  friend void receive(const std::shared_ptr<Socket>& socket, Handler handler)
  {
    socket->mHandler = std::move(handler);
    socket->mImpl.async_receive_from(
      asio::buffer(socket->mReceiveBuffer, MaxPacketSize),
      socket->mSenderEndpoint,
      util::makeAsyncSafe(socket));
  }

  void operator()(const asio::error_code& error, const std::size_t numBytes)
  {
    if (!error && numBytes > 0 && numBytes <= MaxPacketSize)
    {
      const auto bufBegin = begin(mReceiveBuffer);
      mHandler(mSenderEndpoint, bufBegin, bufBegin + static_cast<ptrdiff_t>(numBytes));
    }
  }

  asio::ip::udp::socket mImpl;
  asio::ip::udp::endpoint mSenderEndpoint;
  using Buffer = std::array<uint8_t, MaxPacketSize>;
  Buffer mReceiveBuffer;
  using ByteIt = typename Buffer::const_iterator;
  std::function<void (const asio::ip::udp::endpoint&, ByteIt, ByteIt)> mHandler;
};

// Configure an asio socket for receiving multicast messages
template <std::size_t MaxPacketSize>
void configureMulticastSocket(
  Socket<MaxPacketSize>& socket,
  const asio::ip::address_v4& addr,
  const asio::ip::udp::endpoint& multicastEndpoint)
{
  socket.mImpl.set_option(asio::ip::udp::socket::reuse_address(true));
  // ???
  socket.mImpl.set_option(asio::socket_base::broadcast(!addr.is_loopback()));
  // ???
  socket.mImpl.set_option(
    asio::ip::multicast::enable_loopback(addr.is_loopback()));
  socket.mImpl.set_option(asio::ip::multicast::outbound_interface(addr));
  // Is from_string("0.0.0.0") best approach?
  socket.mImpl.bind(
    {asio::ip::address::from_string("0.0.0.0"), multicastEndpoint.port()});
  socket.mImpl.set_option(
    asio::ip::multicast::join_group(multicastEndpoint.address().to_v4(), addr));
}

// Configure an asio socket for receiving unicast messages
template <std::size_t MaxPacketSize>
void configureUnicastSocket(
  Socket<MaxPacketSize>& socket,
  const asio::ip::address_v4& addr)
{
  // ??? really necessary?
  socket.mImpl.set_option(
    asio::ip::multicast::enable_loopback(addr.is_loopback()));
  socket.mImpl.set_option(asio::ip::multicast::outbound_interface(addr));
  socket.mImpl.bind(asio::ip::udp::endpoint{addr, 0});
}

} // namespace link
} // namespace ableton
