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

#include <string>

namespace ableton
{

template <typename Clock>
inline BasicLinkAudio<Clock>::BasicLinkAudio(const double bpm, std::string name)
  : BasicLink<Clock>(bpm)
{
  setPeerName(name);
  this->mController.setChannelsChangedCallback(
    [&]()
    {
      std::lock_guard<std::mutex> lock(this->mCallbackMutex);
      mChannelsChangedCallback();
    });
}

template <typename Clock>
inline BasicLinkAudio<Clock>::~BasicLinkAudio()
{
  this->mController.setChannelsChangedCallback([]() {});
}

template <typename Clock>
inline bool BasicLinkAudio<Clock>::isLinkAudioEnabled() const
{
  return this->mController.isLinkAudioEnabled();
}

template <typename Clock>
inline void BasicLinkAudio<Clock>::enableLinkAudio(const bool bEnable)
{
  this->mController.enableLinkAudio(bEnable);
}

template <typename Clock>
inline void BasicLinkAudio<Clock>::setPeerName(std::string name)
{
  this->mController.setName(std::move(name));
}

template <typename Clock>
template <typename Callback>
inline void BasicLinkAudio<Clock>::setChannelsChangedCallback(Callback callback)
{
  std::lock_guard<std::mutex> lock(this->mCallbackMutex);
  mChannelsChangedCallback = callback;
}

template <typename Clock>
inline std::vector<typename BasicLinkAudio<Clock>::Channel> BasicLinkAudio<
  Clock>::channels() const
{
  auto channels = this->mController.channels();
  auto result = std::vector<typename BasicLinkAudio<Clock>::Channel>{};
  result.reserve(channels.size());
  for (auto& channel : channels)
  {
    result.push_back(typename BasicLinkAudio<Clock>::Channel{
      channel.id, channel.name, channel.peerId, channel.peerName});
  }
  return result;
}

template <typename Clock>
template <typename Function>
inline void BasicLinkAudio<Clock>::callOnLinkThread(Function func)
{
  this->mController.callOnLinkThread(std::move(func));
}

template <typename LinkAudio>
inline LinkAudioSink::LinkAudioSink(LinkAudio& link,
                                    std::string name,
                                    size_t maxNumSamples)
  : mpImpl{link.mController.addSink(std::move(name), maxNumSamples)}
{
}

inline std::string LinkAudioSink::name() const
{
  return mpImpl->name();
}

inline void LinkAudioSink::setName(std::string name)
{
  mpImpl->setName(std::move(name));
}

template <typename LinkAudio>
inline LinkAudioSource::LinkAudioSource(LinkAudio& link, ChannelId id)
  : mpImpl{link.mController.addSource(std::move(id))}
{
}

inline ChannelId LinkAudioSource::id() const
{
  return mpImpl->id();
}

} // namespace ableton
