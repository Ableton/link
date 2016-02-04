// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/PayloadEntries.hpp>
#include <ableton/link/PingResponder.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/discovery/Payload.hpp>
#include <ableton/discovery/test/Socket.hpp>
#include <ableton/link/v1/Messages.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <ableton/discovery/util/test/IoService.hpp>
#include <array>

namespace ableton
{
namespace link
{
namespace test
{

struct RpFixture
{
  RpFixture()
    : mAddress(asio::ip::address_v4::from_string("127.0.0.1"))
    , mResponder(
        mAddress, mStateQuery, mGhostXFormQuery, util::injectVal(util::test::IoService{}),
        MockClock{}, util::NullLog{})
  {
  }

  test::Socket responderSocket()
  {
    return socket(mResponder);
  }

  std::size_t numSentMessages()
  {
    return responderSocket().sentMessages.size();
  }

  struct MockClock
  {
    friend std::chrono::microseconds micros(const MockClock&)
    {
      return std::chrono::microseconds{4};
    }
  };

  struct GhostXFormQueryMock
  {
    GhostXForm operator()() const
    {
      return {1.0, std::chrono::microseconds{0}};
    }
  };

  struct NodeStateQueryMock
  {
    NodeStateQueryMock()
    {
      mState.sessionId = NodeId::random();
    }

    NodeState operator()()
    {
      return mState;
    }
    NodeState mState;
  };

  asio::ip::address_v4 mAddress =
    asio::ip::address_v4::from_string("127.0.0.1");
  NodeStateQueryMock mStateQuery;
  GhostXFormQueryMock mGhostXFormQuery;

  PingResponder<
    NodeStateQueryMock,
    GhostXFormQueryMock,
    util::test::IoService,
    MockClock,
    test::Socket,
    util::NullLog> mResponder;
};


TEST_CASE("PingResponder | ReplyToPing", "[PingResponder]")
{
  using std::chrono::microseconds;

  RpFixture fixture;

  // Construct and send Ping Message. Check if Responder sent back a Message
  const auto payload = discovery::makePayload(
    HostTime{microseconds(2)}, PrevGHostTime{microseconds(1)});

  v1::MessageBuffer buffer;
  const auto msgBegin = std::begin(buffer);
  const auto msgEnd = v1::pingMessage(payload, msgBegin);
  const auto endpoint = asio::ip::udp::endpoint(fixture.mAddress, 8888);

  fixture.responderSocket().incomingMessage(endpoint, msgBegin, msgEnd);

  CHECK(1 == fixture.numSentMessages());

  // Check Responder's message
  const auto messageBuffer = fixture.responderSocket().sentMessages[0].first;
  const auto result = v1::parseMessageHeader(begin(messageBuffer), end(messageBuffer));
  const auto& hdr = result.first;

  std::chrono::microseconds ghostTime{0};
  std::chrono::microseconds prevGHostTime{0};
  std::chrono::microseconds hostTime{0};
  discovery::parsePayload<GHostTime, PrevGHostTime, HostTime>(
    result.second, std::end(messageBuffer), util::NullLog{},
    [&ghostTime](GHostTime gt) {
      ghostTime = std::move(gt.time);
    },
    [&prevGHostTime](PrevGHostTime gt) {
      prevGHostTime = std::move(gt.time);
    },
    [&hostTime](HostTime ht) {
      hostTime = std::move(ht.time);
    }
  );

  CHECK(v1::kPong == hdr.messageType);
  CHECK(std::chrono::microseconds{2} == hostTime);
  CHECK(std::chrono::microseconds{1} == prevGHostTime);
  CHECK(std::chrono::microseconds{4} == ghostTime);
}

TEST_CASE("PingResponder | PingSizeExceeding", "[PingResponder]")
{
  using std::chrono::microseconds;

  RpFixture fixture;

  const auto ht = HostTime{microseconds(2)};
  const auto payload = discovery::makePayload(ht, ht, ht);

  v1::MessageBuffer buffer;
  const auto msgBegin = std::begin(buffer);
  const auto msgEnd = v1::pingMessage(payload, msgBegin);

  const auto endpoint = asio::ip::udp::endpoint(fixture.mAddress, 8888);

  fixture.responderSocket().incomingMessage(endpoint, msgBegin, msgEnd);

  CHECK(0 == fixture.numSentMessages());
}

} // namespace test
} // namespace link
} // namespace ableton
