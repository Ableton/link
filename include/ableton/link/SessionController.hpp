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

#include <ableton/link/Controller.hpp>

#include <optional>

namespace ableton
{
namespace link
{

template <typename PeerCountCallback,
          typename TempoCallback,
          typename StartStopStateCallback,
          typename Clock,
          typename Random,
          typename IoContext>
class SessionController : public Controller<PeerCountCallback,
                                            TempoCallback,
                                            StartStopStateCallback,
                                            Clock,
                                            Random,
                                            IoContext,
                                            SessionController<PeerCountCallback,
                                                              TempoCallback,
                                                              StartStopStateCallback,
                                                              Clock,
                                                              Random,
                                                              IoContext>>
{
public:
  using LinkController = Controller<PeerCountCallback,
                                    TempoCallback,
                                    StartStopStateCallback,
                                    Clock,
                                    Random,
                                    IoContext,
                                    SessionController<PeerCountCallback,
                                                      TempoCallback,
                                                      StartStopStateCallback,
                                                      Clock,
                                                      Random,
                                                      IoContext>>;

  SessionController(Tempo tempo,
                    PeerCountCallback peerCallback,
                    TempoCallback tempoCallback,
                    StartStopStateCallback startStopStateCallback,
                    Clock clock)
    : LinkController(tempo, peerCallback, tempoCallback, startStopStateCallback, clock)
  {
    this->mRtClientStateSetter.start();
  }

  ~SessionController() { this->mpSessionController = nullptr; }

  void sessionMembershipCallback() { this->mSessionPeerCounter(); }

  void joinSessionCallback(const Session& session) { this->joinSession(session); }

  template <typename Peer, typename Handler>
  void measurePeerCallback(Peer peer, Handler handler)
  {
    this->measurePeer(std::move(peer), std::move(handler));
  }

  void updateDiscoveryCallback() { this->updateDiscovery(); }

  void gatewaysChangedCallback() {}

  void updateRtStatesCallback()
  {
    // Process the pending client states first to make sure we don't push one
    // after we have joined a running session when enabling
    this->mRtClientStateSetter.processPendingClientStates();
    this->mRtClientStateSetter.updateEnabled();
  }

  void sawAudioEndpointCallback(NodeId,
                                std::optional<discovery::UdpEndpoint>,
                                discovery::IpAddress)
  {
  }
};

} // namespace link
} // namespace ableton
