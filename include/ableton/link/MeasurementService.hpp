// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/Socket.hpp>
#include <ableton/discovery/asio/AsioService.hpp>
#include <ableton/link/GhostXForm.hpp>
#include <ableton/link/Kalman.hpp>
#include <ableton/link/LinearRegression.hpp>
#include <ableton/link/Measurement.hpp>
#include <ableton/link/NodeState.hpp>
#include <ableton/link/PeerState.hpp>
#include <ableton/link/PingResponder.hpp>
#include <ableton/link/SessionId.hpp>
#include <ableton/link/v1/Messages.hpp>
#include <ableton/discovery/util/Log.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace ableton
{
namespace link
{

template <
  typename NodeStateQuery,
  typename GhostXFormQuery,
  typename Clock,
  typename Log>
struct MeasurementService
{
  using Point = std::pair<double, double>;

  using Measurement = Measurement<
    util::AsioService,
    Clock,
    link::Socket<v1::kMaxMessageSize>,
    Log>;

  using PingResponder = PingResponder<
    NodeStateQuery,
    GhostXFormQuery,
    util::AsioService&,
    Clock,
    link::Socket<v1::kMaxMessageSize>,
    Log>;

  static const std::size_t kNumberThreads = 1;

  MeasurementService(
    asio::ip::address_v4 address,
    NodeStateQuery stateQuery,
    GhostXFormQuery ghostXFormQuery,
    Clock clock,
    Log log)
    : mClock(std::move(clock))
    , mLog(std::move(log))
    , mPingResponder(
        std::move(address), std::move(stateQuery), std::move(ghostXFormQuery),
        util::injectRef(mIo), mClock, mLog)
  {
  }

  MeasurementService(const MeasurementService&) = delete;
  MeasurementService(MeasurementService&&) = delete;

  ~MeasurementService()
  {
    // Clear the measurement map in the io service so that whatever
    // cleanup code executes in response to the destruction of the
    // measurement objects still have access to the io service
    mIo.post([this] {
        mMeasurementMap.clear();
      });
  }

  friend asio::ip::udp::endpoint endpoint(const MeasurementService& ms)
  {
    return endpoint(ms.mPingResponder);
  }

  // Measure the peer and invoke the handler with a GhostXForm
  template <typename Handler>
  void measurePeer(const PeerState& state, const Handler handler)
  {
    using namespace std;

    mIo.post([this, state, handler] {
        const auto nodeId = state.nodeState.nodeId;
        auto addr = endpoint(mPingResponder).address().to_v4();
        auto callback = CompletionCallback<Handler>{*this, nodeId, handler};

        try
        {
          mMeasurementMap[nodeId] =
            Measurement{state, move(callback), move(addr), mClock, mLog};
        }
        catch (const runtime_error& err)
        {
          info(mLog) << "Failed to measure. Reason: " << err.what();
          handler(GhostXForm{});
        }
      });
  }

  static GhostXForm filter(
    std::vector<Point>::const_iterator begin,
    std::vector<Point>::const_iterator end)
  {
    using namespace std;
    using std::chrono::microseconds;

    Kalman<5> kalman;
    for (auto it = begin; it != end; ++it)
    {
      kalman.iterate(it->second - it->first);
    }

    return GhostXForm{1, microseconds(llround(kalman.getValue()))};
  }

private:

  template <typename Handler>
  struct CompletionCallback
  {
    void operator()(const std::vector<Point> data)
    {
      using namespace std;
      using std::chrono::microseconds;

      // Post this to the measurement service's io service so that we
      // don't delete the measurement object in its stack. Capture all
      // needed data separately from this, since this object may be
      // gone by the time the block gets executed.
      auto nodeId = mNodeId;
      auto handler = mHandler;
      auto& measurementMap = mService.mMeasurementMap;
      mService.mIo.post([nodeId, handler, &measurementMap, data] {
          const auto it = measurementMap.find(nodeId);
          if (it != measurementMap.end())
          {
            if (data.empty())
            {
              handler(GhostXForm{});
            }
            else
            {
              handler(MeasurementService::filter(begin(data), end(data)));
            }
            measurementMap.erase(it);
          }
        });
    }

    MeasurementService& mService;
    NodeId mNodeId;
    Handler mHandler;
  };

  // Make sure the measurement map outlives the io service so that the rest of
  // the members are guaranteed to be valid when any final handlers
  // are begin run.
  using MeasurementMap = std::map<NodeId, Measurement>;
  MeasurementMap mMeasurementMap;
  Clock mClock;
  Log mLog;
  util::AsioService mIo;
  PingResponder mPingResponder;
};

} // namespace link
} // namespace ableton
