// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/link/PayloadEntries.hpp>
#include <ableton/link/PeerState.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/discovery/Payload.hpp>
#include <ableton/discovery/Socket.hpp>
#include <ableton/discovery/asio/AsioService.hpp>
#include <ableton/link/v1/Messages.hpp>
#include <ableton/discovery/util/Injected.hpp>
#include <chrono>
#include <memory>
#include <mutex>


namespace ableton
{
namespace link
{

template<typename IoService, typename Clock, typename Socket, typename Log>
struct Measurement
{
  using Point = std::pair<double, double>;
  using Callback = std::function<void(std::vector<Point>)>;
  using Micros = std::chrono::microseconds;
  using Timer = typename IoService::Timer;

  static const std::size_t kNumberDataPoints = 100;
  static const std::size_t kNumberMeasurements = 5;

  Measurement() = default;

  Measurement(
    const PeerState& state,
    Callback callback,
    asio::ip::address_v4 address,
    Clock clock,
    Log log)
    : mpIo(new IoService{})
    , mpImpl(
      std::make_shared<Impl>(
        *mpIo,
        std::move(state),
        std::move(callback),
        std::move(address),
        std::move(clock),
        std::move(log)
      ))
  {
    mpImpl->listen();
  }

  Measurement(Measurement&& rhs)
    : mpIo(std::move(rhs.mpIo))
    , mpImpl(std::move(rhs.mpImpl))
  {
  }

  ~Measurement()
  {
    postImplDestruction();
  }

  Measurement& operator=(Measurement&& rhs)
  {
    postImplDestruction();
    mpIo = std::move(rhs.mpIo);
    mpImpl = std::move(rhs.mpImpl);
    return *this;
  }

  void postImplDestruction()
  {
    // Post destruction of the impl object into the io thread if valid
    if (mpIo)
    {
      mpIo->post(ImplDeleter{*this});
    }
  }

  struct Impl : std::enable_shared_from_this<Impl>
  {
    Impl(
      IoService& io,
      const PeerState& state,
      Callback callback,
      asio::ip::address_v4 address,
      Clock clock,
      Log log)
    : mpSocket(std::make_shared<Socket>(io))
    , mSessionId(state.nodeState.sessionId)
    , mEndpoint(state.endpoint)
    , mCallback(std::move(callback))
    , mClock(std::move(clock))
    , mTimer(util::injectVal(io.makeTimer()))
    , mMeasurementsStarted(0)
    , mLog(std::move(log))
    , mSuccess(false)
    {
      configureUnicastSocket(*mpSocket, address);

      const auto ht = HostTime{micros(mClock)};
      sendPing(mEndpoint, discovery::makePayload(ht));
      resetTimer();
    }

    void resetTimer()
    {
      mTimer->cancel();
      mTimer->expires_from_now(std::chrono::milliseconds(50));
      mTimer->async_wait([this](const typename Timer::ErrorCode e){
        if (!e)
        {
          if (mMeasurementsStarted < kNumberMeasurements)
          {
            const auto ht = HostTime{micros(mClock)};
            sendPing(mEndpoint, discovery::makePayload(ht));
            ++mMeasurementsStarted;
            resetTimer();
          }
          else
          {
            fail();
          }
        }
      });
    }

    void listen()
    {
      receive(mpSocket, util::makeAsyncSafe(this->shared_from_this()));
    }

    // Operator to handle incoming messages on the interface
    template <typename It>
    void operator()(
      const asio::ip::udp::endpoint& from,
      const It messageBegin,
      const It messageEnd)
    {
      using namespace std;
      const auto result = v1::parseMessageHeader(messageBegin, messageEnd);
      const auto& header = result.first;
      const auto payloadBegin = result.second;

      if(header.messageType == v1::kPong)
      {
        debug(mLog) << "Received Pong message from " << from;

        // parse for all entries
        SessionId sessionId{};
        std::chrono::microseconds ghostTime{0};
        std::chrono::microseconds prevGHostTime{0};
        std::chrono::microseconds prevHostTime{0};
        discovery::parsePayload<
          SessionMembership, GHostTime, PrevGHostTime, HostTime>(
            payloadBegin, messageEnd, mLog,
            [&sessionId] (const SessionMembership& sms) {
              sessionId = sms.sessionId;
            },
            [&ghostTime](GHostTime gt) {
              ghostTime = std::move(gt.time);
            },
            [&prevGHostTime](PrevGHostTime gt) {
              prevGHostTime = std::move(gt.time);
            },
            [&prevHostTime](HostTime ht) {
              prevHostTime = std::move(ht.time);
            }
          );

        if (mSessionId == sessionId)
        {
          const auto hostTime = micros(mClock);

          const auto payload =
            discovery::makePayload(HostTime{hostTime}, PrevGHostTime{ghostTime});

          sendPing(from, payload);
          listen();

          if (prevGHostTime != Micros{0})
          {
            mData.push_back(std::make_pair(
              static_cast<double>((hostTime + prevHostTime).count()) * 0.5,
              static_cast<double>(ghostTime.count())));
            mData.push_back(std::make_pair(
              static_cast<double>(prevHostTime.count()),
              static_cast<double>((ghostTime + prevGHostTime).count()) * 0.5));
          }

          if (mData.size() > kNumberDataPoints)
          {
            finish();
          }
          else
          {
            resetTimer();
          }
        }
        else
        {
          fail();
        }
      }
      else
      {
        debug(mLog) << "Received invalid message from " << from;
      }
    }

    template <typename Payload>
    void sendPing(asio::ip::udp::endpoint to, const Payload& payload)
    {
      v1::MessageBuffer buffer;
      const auto msgBegin = std::begin(buffer);
      const auto msgEnd = v1::pingMessage(payload, msgBegin);
      const auto numBytes = static_cast<size_t>(std::distance(msgBegin, msgEnd));

      try
      {
        send(mpSocket, buffer.data(), numBytes, to);
      }
      catch (const std::runtime_error& err)
      {
        info(mLog) << "Failed to send Ping to " << to.address().to_string() <<
          ". Reason: " << err.what();
      }
    }

    void finish()
    {
      mTimer->cancel();
      mCallback(std::move(mData));
      mData = {};
      mSuccess = true;
      debug(mLog) << "Measuring " << mEndpoint << " done.";
    }

    void fail()
    {
      mCallback(std::vector<Point>{});
      mData = {};
      debug(mLog) << "Measuring " << mEndpoint << " failed.";
    }

    std::shared_ptr<Socket> mpSocket;
    SessionId mSessionId;
    asio::ip::udp::endpoint mEndpoint;
    std::vector<std::pair<double, double>> mData;
    Callback mCallback;
    Clock mClock;
    util::Injected<typename IoService::Timer> mTimer;
    std::size_t mMeasurementsStarted;
    Log mLog;
    bool mSuccess;
  };

  struct ImplDeleter
  {
    ImplDeleter(Measurement& measurement)
      : mpImpl(std::move(measurement.mpImpl))
    {
    }

    void operator()()
    {
      // Notify callback that the measurement has failed if it did
      // not succeed before destruction
      if (!mpImpl->mSuccess)
      {
        mpImpl->fail();
      }
      mpImpl.reset();
    }

    std::shared_ptr<Impl> mpImpl;
  };

  std::unique_ptr<IoService> mpIo;
  std::shared_ptr<Impl> mpImpl;
};

} // namespace link
} // namespace ableton
