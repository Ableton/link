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
#include <ableton/link/Controller.hpp>
#include <ableton/link_audio/Channels.hpp>
#include <ableton/link_audio/Id.hpp>
#include <ableton/link_audio/MainProcessor.hpp>
#include <ableton/link_audio/PeerGateways.hpp>
#include <ableton/link_audio/PeerInfo.hpp>
#include <ableton/link_audio/UdpMessenger.hpp>
#include <ableton/util/Injected.hpp>
#include <ableton/util/Locked.hpp>
#include <algorithm>
#include <atomic>
#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace ableton
{
namespace link_audio
{

using ChannelsChangedCallback = std::function<void()>;
using CallOnThreadFunction = std::function<void()>;

template <typename PeerCountCallback,
          typename TempoCallback,
          typename StartStopStateCallback,
          typename Clock,
          typename Random,
          typename IoContext,
          typename SessionController>
class Controller : public link::Controller<PeerCountCallback,
                                           TempoCallback,
                                           StartStopStateCallback,
                                           Clock,
                                           Random,
                                           IoContext,
                                           SessionController>
{
public:
  using LinkController = link::Controller<PeerCountCallback,
                                          TempoCallback,
                                          StartStopStateCallback,
                                          Clock,
                                          Random,
                                          IoContext,
                                          SessionController>;

  Controller(link::Tempo tempo,
             link::PeerCountCallback peerCallback,
             link::TempoCallback tempoCallback,
             link::StartStopStateCallback startStopStateCallback,
             Clock clock)
    : LinkController(tempo, peerCallback, tempoCallback, startStopStateCallback, clock)
    , mChannelsChangedCallback{[]() {}}
    , mApiChannels({})
    , mIsLinkAudioEnabled(false)
    , mWasLinkAudioEnabled(false)
    , mPeerInfo{}
    , mChannels(util::injectRef(*(this->mIo)), ChannelsChanged{this})
    , mProcessor{util::injectRef(*(this->mIo)), util::injectVal(ChannelsCallback{this})}
    , mGateways{util::injectVal(GatewayFactory{this}), util::injectRef(*(this->mIo))}
  {
  }

  void enableLinkAudio(bool enabled)
  {
    mIsLinkAudioEnabled = enabled;
    this->mRtClientStateSetter.invoke();
  }

  bool isLinkAudioEnabled() const { return mIsLinkAudioEnabled; }

  void setName(std::string name)
  {
    this->mIo->async(
      [this, name = std::move(name)]()
      {
        mPeerInfo.name = std::move(name);
        if (this->mpSessionController)
        {
          this->mpSessionController->updateDiscoveryCallback();
        }
      });
  }

  void callOnLinkThread(CallOnThreadFunction func)
  {
    this->mIo->async([func = std::move(func)]() { func(); });
  }

  SharedSink addSink(std::string name, size_t maxNumSamples)
  {
    auto id = Id::random<Random>();
    auto sink = std::make_shared<Sink>(name, maxNumSamples, id);

    this->mIo->async(
      [this, sink]()
      {
        this->mProcessor.addSink(
          sink, util::injectVal(GetSender{this}), util::injectVal(GetNodeId{this}));
        if (this->mpSessionController)
        {
          this->mpSessionController->updateDiscoveryCallback();
        }
      });

    return sink;
  }

  SharedSource addSource(Id channelId, Source::Callback callback)
  {
    auto source = std::make_shared<Source>(std::move(channelId), std::move(callback));

    this->mIo->async(
      [this, source]()
      {
        mProcessor.addSource(
          source, util::injectVal(GetSender{this}), util::injectVal(GetNodeId{this}));
      });

    return source;
  }

  void setChannelsChangedCallback(ChannelsChangedCallback callback)
  {
    this->mIo->async([&, callback = std::move(callback)]()
                     { mChannelsChangedCallback = callback; });
  }

  auto channels() const { return mApiChannels.read(); }

protected:
  void updateIsLinkAudioEnabled()
  {
    if (mIsLinkAudioEnabled && !mWasLinkAudioEnabled && this->mpSessionController)
    {
      mWasLinkAudioEnabled = true;
      this->mpSessionController->gatewaysChangedCallback();
    }
    else if (!mIsLinkAudioEnabled && mWasLinkAudioEnabled)
    {
      mWasLinkAudioEnabled = false;
      mGateways.clear();
      updateAudioEndpoints({});
    }
  }

  void updateAudioDiscovery()
  {
    mGateways.updateAnnouncement(PeerAnnouncement{
      this->mNodeId, this->mSessionId, mPeerInfo, mProcessor.channelAnnouncements()});
  }

  void updateLinkAudio()
  {
    auto peers = std::set<link::NodeId>();

    for (const auto& sessionPeer : this->mPeers.sessionPeers(this->mSessionId))
    {
      peers.insert(sessionPeer.first.nodeState.ident());
    }

    mGateways.updateSessionPeers(begin(peers), end(peers));

    updateApiChannels();
  }

  void sawLinkAudioEndpoint(link::NodeId peerId,
                            std::optional<discovery::UdpEndpoint> endpoint,
                            discovery::IpAddress gateway)
  {
    mGateways.sawLinkAudioEndpoint(peerId, endpoint, gateway);
  }

  void updateLinkAudioGateways()
  {
    auto gateways = std::vector<discovery::IpAddress>();
    this->mDiscovery.withGateways(
      [&](auto begin, auto end)
      { std::for_each(begin, end, [&](auto& it) { gateways.push_back(it.first); }); });

    if (mIsLinkAudioEnabled)
    {
      mGateways.updateGateways(std::move(gateways));
      updateLinkAudio();
    }
    else
    {
      mGateways.clear();
    }
  }

  void updateAudioEndpoints(std::vector<discovery::UdpEndpoint> endpoints)
  {
    this->mDiscovery.withGateways(
      [&](auto begin, auto end)
      {
        std::for_each(begin,
                      end,
                      [&](auto& gateway)
                      {
                        auto it = std::find_if(endpoints.begin(),
                                               endpoints.end(),
                                               [&](const auto& ep)
                                               { return ep.address() == gateway.first; });
                        if (it != endpoints.end())
                        {
                          gateway.second->updateAudioEndpoint(*it);
                        }
                        else
                        {
                          gateway.second->updateAudioEndpoint(std::nullopt);
                        }
                      });
      });
    this->updateDiscovery();
  }

  void updateApiChannels()
  {
    auto channelsChanged = false;
    auto currentChannels = mChannels.uniqueSessionChannels(this->mSessionId);

    std::sort(currentChannels.begin(),
              currentChannels.end(),
              [](const auto& a, const auto& b)
              {
                if (a.peerName == b.peerName)
                {
                  return a.name < b.name;
                }
                return a.peerName < b.peerName;
              });

    mApiChannels.update(
      [&](auto& apiChannels)
      {
        if (apiChannels != currentChannels)
        {
          apiChannels = currentChannels;
          channelsChanged = true;
        }
      });

    if (channelsChanged)
    {
      mChannelsChangedCallback();
    }
  }

  struct ChannelsChanged
  {
    void operator()() { mpController->updateApiChannels(); }

    Controller* mpController;
  };


  using Interface = MessengerInterface<typename util::Injected<IoContext>::type&>;
  using ControllerChannels = Channels<IoContext&, ChannelsChanged, Interface>;
  using SendHandler = typename ControllerChannels::SendHandler;

  struct GetSender
  {
    std::optional<SendHandler> forChannel(const Id& id)
    {
      return mpController->mChannels.channelSendHandler(id);
    }

    std::optional<SendHandler> forPeer(const Id& id)
    {
      return mpController->mChannels.peerSendHandler(id);
    }

    Controller* mpController;
  };

  struct GetNodeId
  {
    const link::NodeId& operator()() const { return mpController->mNodeId; }

    Controller* mpController;
  };

  struct ChannelsCallback
  {
    void operator()()
    {
      if (mpController && mpController->mpSessionController)
      {
        mpController->mpSessionController->updateDiscoveryCallback();
      }
    }

    Controller* mpController;
  };

  using ControllerMainProcessor =
    MainProcessor<ChannelsCallback, GetSender, GetNodeId, Random, IoContext&>;
  using ControllerMessengerPtr =
    MessengerPtr<ControllerMainProcessor&,
                 typename ControllerChannels::GatewayObserver,
                 typename util::Injected<IoContext>::type&>;

  struct GatewayFactory
  {
    ControllerMessengerPtr operator()(const discovery::IpAddress& addr)
    {
      return makeMessengerPtr(
        util::injectRef(mpController->mProcessor),
        util::injectRef(*(mpController->mIo)),
        addr,
        util::injectVal(makeGatewayObserver(mpController->mChannels, addr)),
        PeerAnnouncement{mpController->mNodeId,
                         mpController->mSessionId,
                         mpController->mPeerInfo,
                         mpController->mProcessor.channelAnnouncements()});
    }

    void gatewaysChanged()
    {
      std::vector<discovery::UdpEndpoint> endpoints;
      mpController->mGateways.withGateways(
        [&](auto begin, auto end)
        {
          endpoints.reserve(std::distance(begin, end));
          std::transform(begin,
                         end,
                         std::back_inserter(endpoints),
                         [](auto gateway) { return gateway.second->endpoint(); });
        });
      mpController->updateAudioEndpoints(std::move(endpoints));
    }

    Controller* mpController;
  };

  void stopAudio()
  {
    mIsLinkAudioEnabled = false;
    mGateways.clear();
    mProcessor.stop();
  }

  ChannelsChangedCallback mChannelsChangedCallback;
  util::Locked<std::vector<typename ControllerChannels::Channel>> mApiChannels;
  std::atomic_bool mIsLinkAudioEnabled;
  bool mWasLinkAudioEnabled;
  PeerInfo mPeerInfo;
  ControllerChannels mChannels;
  ControllerMainProcessor mProcessor;
  PeerGateways<GatewayFactory, IoContext&> mGateways;
};

} // namespace link_audio
} // namespace ableton
