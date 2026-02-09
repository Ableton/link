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

#include <ableton/discovery/AsioTypes.hpp>
#include <ableton/util/Injected.hpp>
#include <memory>
#include <stdexcept>

namespace ableton
{
namespace link_audio
{

template <typename IoContext, std::size_t MaxPacketSize>
class UnicastIpInterface
{
public:
  using Socket = typename util::Injected<IoContext>::type::template Socket<MaxPacketSize>;

  UnicastIpInterface(util::Injected<IoContext> io, const discovery::IpAddress& addr)
    : mSocket(io->template openUnicastSocket<MaxPacketSize>(addr))
  {
  }

  UnicastIpInterface(const UnicastIpInterface&) = delete;
  UnicastIpInterface& operator=(const UnicastIpInterface&) = delete;

  UnicastIpInterface(UnicastIpInterface&& rhs)
    : mSocket(std::move(rhs.mSocket))
  {
  }

  std::size_t send(const uint8_t* const pData,
                   const size_t numBytes,
                   const discovery::UdpEndpoint& to)
  {
    return mSocket.send(pData, numBytes, to);
  }

  template <typename Handler>
  void receive(Handler handler)
  {
    mSocket.receive(SocketReceiver<Handler>(std::move(handler)));
  }

  discovery::UdpEndpoint endpoint() const { return mSocket.endpoint(); }

private:
  template <typename Handler>
  struct SocketReceiver
  {
    SocketReceiver(Handler handler)
      : mHandler(std::move(handler))
    {
    }

    template <typename It>
    void operator()(const discovery::UdpEndpoint& from,
                    const It messageBegin,
                    const It messageEnd)
    {
      mHandler(from, messageBegin, messageEnd);
    }

    Handler mHandler;
  };

  Socket mSocket;
};

template <std::size_t MaxPacketSize, typename IoContext>
std::shared_ptr<UnicastIpInterface<IoContext, MaxPacketSize>>
makeSharedUnicastIpInterface(util::Injected<IoContext> io,
                             const discovery::IpAddress& addr)
{
  return std::make_shared<UnicastIpInterface<IoContext, MaxPacketSize>>(
    std::move(io), addr);
}

} // namespace link_audio
} // namespace ableton
