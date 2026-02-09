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
#include <algorithm>
#include <map>
#include <memory>
#include <type_traits>
#include <vector>

namespace ableton
{
namespace link_audio
{

template <typename GatewayFactory, typename IoContext>
struct PeerGateways
{
  using FactoryType = typename util::Injected<GatewayFactory>::type;
  using GatewayPtr = std::invoke_result_t<FactoryType, const discovery::IpAddress&>;
  using GatewayMap = std::map<discovery::IpAddress, GatewayPtr>;

  PeerGateways(util::Injected<GatewayFactory> factory, util::Injected<IoContext> io)
    : mIo(std::move(io))
    , mFactory(std::move(factory))
  {
  }

  template <typename Handler>
  void forEachGateway(Handler handler)
  {
    for (auto it = mGateways.begin(); it != mGateways.end(); ++it)
    {
      handler(it->second);
    }
  }

  template <typename Handler>
  void withGateways(Handler handler)
  {
    handler(mGateways.begin(), mGateways.end());
  }

  void clear()
  {
    mGateways.clear();
    mFactory->gatewaysChanged();
  }

  template <typename Announcement>
  void updateAnnouncement(const Announcement& announcement)
  {
    for (const auto& gateway : mGateways)
    {
      gateway.second->updateAnnouncement(announcement);
    }
  }

  void updateGateways(std::vector<discovery::IpAddress> linkGateways)
  {
    using namespace std;

    vector<discovery::IpAddress> curAddrs;
    curAddrs.reserve(mGateways.size());
    transform(mGateways.begin(),
              mGateways.end(),
              back_inserter(curAddrs),
              [](const auto& gateway) { return gateway.first; });

    vector<discovery::IpAddress> staleAddrs;
    set_difference(curAddrs.begin(),
                   curAddrs.end(),
                   linkGateways.begin(),
                   linkGateways.end(),
                   back_inserter(staleAddrs));

    for (const auto& addr : staleAddrs)
    {
      mGateways.erase(addr);
    }

    vector<discovery::IpAddress> newAddrs;
    set_difference(linkGateways.begin(),
                   linkGateways.end(),
                   curAddrs.begin(),
                   curAddrs.end(),
                   back_inserter(newAddrs));

    for (const auto& addr : newAddrs)
    {
      try
      {
        info(mIo->log()) << "initializing audio gateway on interface " << addr;
        mGateways.emplace(addr, (*mFactory)(addr));
      }
      catch (const std::runtime_error& e)
      {
        warning(mIo->log()) << "failed to init audio gateway on interface " << addr
                            << " reason: " << e.what();
      }
    }

    if (!staleAddrs.empty() || !newAddrs.empty())
    {
      mFactory->gatewaysChanged();
    }
  }

private:
  util::Injected<IoContext> mIo;
  GatewayMap mGateways;
  util::Injected<GatewayFactory> mFactory;
};

} // namespace link_audio
} // namespace ableton
