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
#include <vector>

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

  struct Channel
  {
    ChannelId id;
    std::string name;
    PeerId peerId;
    std::string peerName;
  };

  std::vector<Channel> channels() const;

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
  friend class LinkAudioSource;
};

class LinkAudioSink
{
public:
  template <typename LinkAudio>
  LinkAudioSink(LinkAudio& link, std::string name, size_t maxNumSamples);

  LinkAudioSink(const LinkAudioSink&) = default;
  LinkAudioSink& operator=(const LinkAudioSink&) = default;
  LinkAudioSink(LinkAudioSink&&) = default;
  LinkAudioSink& operator=(LinkAudioSink&&) = default;

  std::string name() const;

  void setName(std::string name);

  struct BufferHandle
  {
    BufferHandle(LinkAudioSink&);

    BufferHandle(const BufferHandle&) = delete;
    BufferHandle(BufferHandle&& other) = delete;
    BufferHandle& operator=(const BufferHandle&) = delete;
    BufferHandle& operator=(BufferHandle&& other) = delete;

    ~BufferHandle();

    operator bool() const;
    int16_t* samples;
    size_t maxNumSamples;
    template <typename SessionState>
    bool commit(const SessionState&,
                double beatsAtBufferBegin,
                double quantum,
                size_t numFrames,
                size_t numChannels,
                uint32_t sampleRate);

  private:
    std::weak_ptr<link_audio::Sink> mpSink;
    link_audio::Buffer<int16_t>* mpBuffer;
  };

private:
  friend struct BufferHandle;
  std::shared_ptr<link_audio::Sink> mpImpl;
};

class LinkAudioSource
{
public:
  template <typename LinkAudio>
  LinkAudioSource(LinkAudio& link, ChannelId id);

  LinkAudioSource(const LinkAudioSource&) = default;
  LinkAudioSource& operator=(const LinkAudioSource&) = default;
  LinkAudioSource(LinkAudioSource&&) = default;
  LinkAudioSource& operator=(LinkAudioSource&&) = default;

  ChannelId id() const;

private:
  std::shared_ptr<link_audio::Source> mpImpl;
};

} // namespace ableton

#include <ableton/LinkAudio.ipp>
