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

#define LINK_AUDIO YES

#include <ableton/link_audio/ApiConfig.hpp>

#include <memory>
#include <string>

namespace ableton
{

template <typename Clock>
class BasicLinkAudio : public BasicLink<Clock>
{
public:
  BasicLinkAudio(double bpm, std::string name);

  ~BasicLinkAudio();

  bool isLinkAudioEnabled() const;
  void enableLinkAudio(bool bEnable);

  void setPeerName(std::string name);

  template <typename Callback>
  void setChannelsChangedCallback(Callback callback);

  template <typename Function>
  void callOnLinkThread(Function func);

private:
  using Controller = ableton::link::ApiController<Clock>;

  link_audio::ChannelsChangedCallback mChannelsChangedCallback = []() {};
};

class LinkAudio : public BasicLinkAudio<link::platform::Clock>
{
public:
  using Clock = link::platform::Clock;

  LinkAudio(double bpm, std::string name)
    : BasicLinkAudio<Clock>(bpm, name)
  {
  }

private:
  friend class LinkAudioSink;
};

class LinkAudioSink
{
public:
  template <typename LinkAudio>
  LinkAudioSink(LinkAudio& link, std::string name);

  LinkAudioSink(const LinkAudioSink&) = default;
  LinkAudioSink& operator=(const LinkAudioSink&) = default;
  LinkAudioSink(LinkAudioSink&&) = default;
  LinkAudioSink& operator=(LinkAudioSink&&) = default;

  std::string name() const;
  void setName(std::string name);

private:
  std::shared_ptr<link_audio::Sink> mpImpl;
};

} // namespace ableton

#include <ableton/LinkAudio.ipp>
