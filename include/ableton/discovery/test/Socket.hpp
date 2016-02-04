// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <ableton/discovery/util/test/IoService.hpp>

namespace ableton
{
namespace link
{
namespace test
{

struct Socket
{
  Socket(util::test::IoService&)
  {
  }

  friend void configureUnicastSocket(Socket&, const asio::ip::address_v4&) {}

  friend std::size_t send(
    const std::shared_ptr<Socket>& socket,
    const uint8_t* const pData,
    const size_t numBytes,
    const asio::ip::udp::endpoint& to)
  {
    socket->sentMessages.push_back(
      std::make_pair(std::vector<uint8_t>{pData, pData + numBytes}, to));
    return numBytes;
  }

  template <typename Handler>
  friend void receive(const std::shared_ptr<Socket>& socket, Handler handler)
  {
    socket->mCallback = [handler](const asio::ip::udp::endpoint& from,
                                 const std::vector<uint8_t>& buffer) {
      handler(from, begin(buffer), end(buffer));
    };
  }

  template <typename It>
  void incomingMessage(
    const asio::ip::udp::endpoint& from,
    It messageBegin,
    It messageEnd)
  {
    std::vector<uint8_t> buffer{messageBegin, messageEnd};
    mCallback(from, buffer);
  }

  friend asio::ip::udp::endpoint endpoint(Socket&)
  {
    return asio::ip::udp::endpoint({}, 0);
  }

  using SentMessage = std::pair<std::vector<uint8_t>, asio::ip::udp::endpoint>;
  std::vector<SentMessage> sentMessages;

private:
  using ReceiveCallback = std::function<void (
    const asio::ip::udp::endpoint&,
    const std::vector<uint8_t>&)>;
  ReceiveCallback mCallback;
};

} // namespace test
} // namespace link
} // namespace ableton
