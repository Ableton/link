/* Copyright 2026, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/platforms/asio/AsioWrapper.hpp>
#include <ableton/platforms/asio/Socket.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <chrono>
#include <cstdint>

namespace ableton
{
namespace platforms
{
namespace asio
{

TEST_CASE("Socket")
{
  SECTION("KeepsReceivingAfterSendingToAClosedPort")
  {
    ::asio::io_context io;
    const auto loopback = ::asio::ip::address_v4::loopback();

    // An endpoint that nobody listens on any more
    auto closedEndpoint = discovery::UdpEndpoint{};
    {
      auto probe = ::asio::ip::udp::socket{io, discovery::UdpEndpoint{loopback, 0}};
      closedEndpoint = probe.local_endpoint();
    }

    auto socket = Socket<64>{io, ::asio::ip::udp::v4()};
    socket.mpImpl->mSocket.bind(discovery::UdpEndpoint{loopback, 0});

    auto numReceived = 0;
    socket.receive([&numReceived](const discovery::UdpEndpoint&, auto, auto)
                   { ++numReceived; });

    // On Windows, the ICMP port unreachable this provokes is reported as an error on the
    // socket's next receive
    const std::uint8_t byte = 42;
    socket.send(&byte, 1, closedEndpoint);
    io.run_for(std::chrono::milliseconds{100});

    auto sender = ::asio::ip::udp::socket{io, discovery::UdpEndpoint{loopback, 0}};
    sender.send_to(::asio::buffer(&byte, 1), socket.endpoint());
    io.restart();
    io.run_for(std::chrono::milliseconds{500});

    CHECK(1 == numReceived);
  }
}

} // namespace asio
} // namespace platforms
} // namespace ableton
