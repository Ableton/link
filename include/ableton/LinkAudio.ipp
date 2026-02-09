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

inline LinkAudioSink::BufferHandle::BufferHandle(LinkAudioSink& sink)
  : samples(nullptr)
  , maxNumSamples(0)
  , mpSink(sink.mpImpl)
  , mpBuffer(nullptr)
{
  if (auto pBuffer = sink.mpImpl->retainBuffer())
  {
    samples = pBuffer->mSamples.data();
    maxNumSamples = pBuffer->mSamples.size();
    mpBuffer = pBuffer;
  }
}

inline LinkAudioSink::BufferHandle::~BufferHandle()
{
  // BufferHandle should not outlive the LinkAudioSink it was created from
  assert(mpSink.lock());

  if (mpBuffer)
  {
    if (auto pSink = mpSink.lock())
    {
      pSink->releaseBuffer();
    }
  }
}

inline LinkAudioSink::BufferHandle::operator bool() const
{
  return mpBuffer != nullptr;
}

template <typename SessionState>
inline bool LinkAudioSink::BufferHandle::commit(const SessionState& sessionState,
                                                const double beatsAtBufferBegin,
                                                const double quantum,
                                                const size_t numFrames,
                                                const size_t numChannels,
                                                const uint32_t sampleRate)
{
  const auto result = static_cast<bool>(*this) && (numChannels == 1 || numChannels == 2)
                      && maxNumSamples >= numFrames * numChannels;

  if (result && mpBuffer)
  {
    if (auto pSink = mpSink.lock())
    {
      pSink->releaseAndCommitBuffer(detail::linkApiState(sessionState).timeline,
                                    detail::linkApiState(sessionState).timelineSessionId,
                                    beatsAtBufferBegin,
                                    quantum,
                                    numFrames,
                                    numChannels,
                                    sampleRate);
    }
  }

  mpBuffer = nullptr;

  return result;
}

template <typename SessionState>
inline std::optional<double> LinkAudioSource::BufferHandle::Info::beginBeats(
  const SessionState& sessionState, const double quantum) const
{
  const auto& state = detail::linkApiState(sessionState);
  if (state.timelineSessionId != sessionId)
  {
    return std::nullopt;
  }

  return link_audio::beatAtGlobalBeat(
           state.timeline, link::Beats{sessionBeatTime}, link::Beats{quantum})
    .floating();
}


template <typename SessionState>
inline std::optional<double> LinkAudioSource::BufferHandle::Info::endBeats(
  const SessionState& sessionState, const double quantum) const
{
  const auto& state = detail::linkApiState(sessionState);
  if (state.timelineSessionId != sessionId)
  {
    return std::nullopt;
  }

  const auto durationSeconds =
    static_cast<double>(numFrames) / static_cast<double>(sampleRate);
  const auto beatsPerSecond = tempo / 60.0;
  const auto endBeats = link::Beats{sessionBeatTime + durationSeconds * beatsPerSecond};
  return link_audio::beatAtGlobalBeat(state.timeline, endBeats, link::Beats{quantum})
    .floating();
}


template <typename LinkAudio, typename Callback>
inline LinkAudioSource::LinkAudioSource(LinkAudio& link, ChannelId id, Callback callback)
  : mpImpl{link.mController.addSource(
      std::move(id),
      [callback](auto handle)
      {
        auto info = LinkAudioSource::BufferHandle::Info{};
        info.numChannels = handle.mBuffer.mNumChannels;
        info.numFrames = handle.mBuffer.mNumFrames;
        info.sampleRate = handle.mBuffer.mSampleRate;
        info.count = handle.mBuffer.mCount;
        info.sessionBeatTime = handle.mBuffer.mBeginBeats.floating();
        info.tempo = handle.mBuffer.mTempo.bpm();
        info.sessionId = handle.mBuffer.mSessionId;

        callback(LinkAudioSource::BufferHandle{handle.mpSamples, info});
      })}
{
}

inline LinkAudioSource::~LinkAudioSource()
{
  mpImpl->setCallback([](auto) {});
}

inline void LinkAudioSink::requestMaxNumSamples(size_t numSamples)
{
  mpImpl->requestMaxNumSamples(numSamples);
}

inline size_t LinkAudioSink::maxNumSamples() const
{
  return mpImpl->maxNumSamples();
}

inline ChannelId LinkAudioSource::id() const
{
  return mpImpl->id();
}

} // namespace ableton
