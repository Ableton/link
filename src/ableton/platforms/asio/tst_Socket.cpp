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

#include <ableton/platforms/asio/Socket.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>
#include <chrono>
#include <vector>

namespace ableton
{
namespace platforms
{
namespace asio
{
namespace
{

constexpr std::size_t kMaxPacketSize = 16;
using TestSocket = Socket<kMaxPacketSize>;

TestSocket makeLoopbackSocket(::asio::io_context& io)
{
  auto socket = TestSocket{io, ::asio::ip::udp::v4()};
  socket.mpImpl->mSocket.bind({::asio::ip::address_v4::loopback(), 0});
  return socket;
}

} // namespace

TEST_CASE("Socket")
{
  ::asio::io_context io;
  auto receiver = makeLoopbackSocket(io);
  auto sender = makeLoopbackSocket(io);

  std::vector<std::vector<uint8_t>> received;
  receiver.receive([&received](const discovery::UdpEndpoint&,
                               TestSocket::Impl::ByteIt begin,
                               TestSocket::Impl::ByteIt end)
                   { received.emplace_back(begin, end); });

  const std::array<uint8_t, 3> payload = {{1, 2, 3}};

  SECTION("ReceivesDatagram")
  {
    sender.send(payload.data(), payload.size(), receiver.endpoint());

    io.run_for(std::chrono::milliseconds(100));

    REQUIRE(1 == received.size());
    CHECK(std::vector<uint8_t>(begin(payload), end(payload)) == received.front());
  }

  SECTION("KeepsListeningAfterEmptyDatagram")
  {
    sender.send(payload.data(), 0, receiver.endpoint());
    sender.send(payload.data(), payload.size(), receiver.endpoint());

    io.run_for(std::chrono::milliseconds(100));

    REQUIRE(1 == received.size());
    CHECK(std::vector<uint8_t>(begin(payload), end(payload)) == received.front());
  }

  SECTION("KeepsListeningAfterTransientError")
  {
    // Sending to a port nobody listens on provokes an ICMP port unreachable. On
    // Windows this fails the pending receive on the sending socket with
    // WSAECONNRESET, without invoking the handler.
    const auto unreachable = [&io]
    {
      auto socket = makeLoopbackSocket(io);
      return socket.endpoint();
    }();
    receiver.send(payload.data(), payload.size(), unreachable);

    io.run_for(std::chrono::milliseconds(100));
    sender.send(payload.data(), payload.size(), receiver.endpoint());

    io.run_for(std::chrono::milliseconds(100));

    REQUIRE(1 == received.size());
    CHECK(std::vector<uint8_t>(begin(payload), end(payload)) == received.front());
  }

  SECTION("StopsListeningWhenClosed")
  {
    receiver.mpImpl->mSocket.close();

    io.run_for(std::chrono::milliseconds(100));

    CHECK(io.stopped());
    CHECK(received.empty());
  }
}

} // namespace asio
} // namespace platforms
} // namespace ableton
