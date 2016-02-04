// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/util/Log.hpp>

namespace ableton
{
namespace link
{
namespace discovery
{
namespace test
{

struct Interface
{
  friend void send(
    Interface& iface,
    const uint8_t* const bytes,
    const size_t numBytes,
    const asio::ip::udp::endpoint& endpoint)
  {
    iface.sentMessages.push_back(
      std::make_pair(std::vector<uint8_t>{bytes, bytes + numBytes}, endpoint));
  }

  template <typename Callback, typename Tag>
  friend void receive(Interface& iface, Callback callback, Tag tag)
  {
    iface.mCallback = [callback, tag](
      const asio::ip::udp::endpoint& from,
      const std::vector<uint8_t>& buffer) {
        callback(tag, from, begin(buffer), end(buffer));
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

  friend asio::ip::udp::endpoint endpoint(Interface&)
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
} // namespace discovery
} // namespace link
} // namespace ableton
