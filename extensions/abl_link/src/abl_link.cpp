/* Copyright 2021, Ableton AG, Berlin. All rights reserved.
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

#include <abl_link.h>
#include <ableton/LinkAudio.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
struct abl_link_audio_channel_id toChannelId(const ableton::link_audio::Id &id)
{
  struct abl_link_audio_channel_id out{};
  std::copy(id.begin(), id.end(), out.bytes);
  return out;
}

struct abl_link_audio_peer_id toPeerId(const ableton::link_audio::Id &id)
{
  struct abl_link_audio_peer_id out{};
  std::copy(id.begin(), id.end(), out.bytes);
  return out;
}

struct abl_link_audio_session_id toSessionId(const ableton::link_audio::Id &id)
{
  struct abl_link_audio_session_id out{};
  std::copy(id.begin(), id.end(), out.bytes);
  return out;
}

ableton::link_audio::Id toCppChannelId(const struct abl_link_audio_channel_id &id)
{
  ableton::link_audio::Id out{};
  std::copy(id.bytes, id.bytes + sizeof(id.bytes), out.begin());
  return out;
}

ableton::link_audio::Id toCppSessionId(const struct abl_link_audio_session_id &id)
{
  ableton::link_audio::Id out{};
  std::copy(id.bytes, id.bytes + sizeof(id.bytes), out.begin());
  return out;
}

char *copyCString(const std::string &value)
{
  auto *buffer = static_cast<char *>(std::malloc(value.size() + 1));
  if (buffer)
  {
    std::memcpy(buffer, value.c_str(), value.size() + 1);
  }
  return buffer;
}

size_t copyStringToBuffer(const std::string &src, char *buffer, size_t buffer_size)
{
  if (buffer && buffer_size > 0)
  {
    const auto toCopy = std::min(buffer_size - 1, src.size());
    std::memcpy(buffer, src.c_str(), toCopy);
    buffer[toCopy] = '\0';
  }
  return src.size();
}
} // namespace

extern "C"
{
  struct abl_link abl_link_create(double bpm)
  {
    return {reinterpret_cast<void *>(new ableton::LinkAudio(bpm, std::string{}))};
  }

  void abl_link_destroy(struct abl_link link)
  {
    delete reinterpret_cast<ableton::LinkAudio *>(link.impl);
  }

  bool abl_link_is_enabled(struct abl_link link)
  {
    return reinterpret_cast<ableton::LinkAudio *>(link.impl)->isEnabled();
  }

  void abl_link_enable(struct abl_link link, bool enabled)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->enable(enabled);
  }

  bool abl_link_is_start_stop_sync_enabled(struct abl_link link)
  {
    return reinterpret_cast<ableton::LinkAudio *>(link.impl)->isStartStopSyncEnabled();
  }

  void abl_link_enable_start_stop_sync(struct abl_link link, bool enabled)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->enableStartStopSync(enabled);
  }

  uint64_t abl_link_num_peers(struct abl_link link)
  {
    return reinterpret_cast<ableton::LinkAudio *>(link.impl)->numPeers();
  }

  void abl_link_set_num_peers_callback(struct abl_link link,
    void (*callback)(uint64_t num_peers, void *context),
    void *context)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->setNumPeersCallback(
      [callback, context](std::size_t numPeers)
      { (*callback)(static_cast<uint64_t>(numPeers), context); });
  }

  void abl_link_set_tempo_callback(
    struct abl_link link, void (*callback)(double tempo, void *context), void *context)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->setTempoCallback(
      [callback, context](double tempo) { (*callback)(tempo, context); });
  }

  void abl_link_set_start_stop_callback(
    struct abl_link link, void (*callback)(bool isPlaying, void *context), void *context)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->setStartStopCallback(
      [callback, context](bool isPlaying) { (*callback)(isPlaying, context); });
  }

  int64_t abl_link_clock_micros(struct abl_link link)
  {
    return reinterpret_cast<ableton::LinkAudio *>(link.impl)->clock().micros().count();
  }

  abl_link_session_state abl_link_create_session_state(void)
  {
    return abl_link_session_state{reinterpret_cast<void *>(
      new ableton::Link::SessionState{ableton::link::ApiState{}, {}})};
  }

  void abl_link_destroy_session_state(abl_link_session_state session_state)
  {
    delete reinterpret_cast<ableton::Link::SessionState *>(session_state.impl);
  }

  void abl_link_capture_app_session_state(
    struct abl_link link, abl_link_session_state session_state)
  {
    *reinterpret_cast<ableton::Link::SessionState *>(session_state.impl) =
      reinterpret_cast<ableton::LinkAudio *>(link.impl)->captureAppSessionState();
  }

  void abl_link_commit_app_session_state(
    struct abl_link link, abl_link_session_state session_state)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->commitAppSessionState(
      *reinterpret_cast<ableton::Link::SessionState *>(session_state.impl));
  }

  void abl_link_capture_audio_session_state(
    struct abl_link link, abl_link_session_state session_state)
  {
    *reinterpret_cast<ableton::Link::SessionState *>(session_state.impl) =
      reinterpret_cast<ableton::LinkAudio *>(link.impl)->captureAudioSessionState();
  }

  void abl_link_commit_audio_session_state(
    struct abl_link link, abl_link_session_state session_state)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->commitAudioSessionState(
      *reinterpret_cast<ableton::Link::SessionState *>(session_state.impl));
  }

  double abl_link_tempo(abl_link_session_state session_state)
  {
    return reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)->tempo();
  }

  void abl_link_set_tempo(
    abl_link_session_state session_state, double bpm, int64_t at_time)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->setTempo(bpm, std::chrono::microseconds{at_time});
  }

  double abl_link_beat_at_time(
    abl_link_session_state session_state, int64_t time, double quantum)
  {
    auto micros = std::chrono::microseconds{time};
    return reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->beatAtTime(micros, quantum);
  }

  double abl_link_phase_at_time(
    abl_link_session_state session_state, int64_t time, double quantum)
  {
    return reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->phaseAtTime(std::chrono::microseconds{time}, quantum);
  }

  int64_t abl_link_time_at_beat(
    abl_link_session_state session_state, double beat, double quantum)
  {
    return reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->timeAtBeat(beat, quantum)
      .count();
  }

  void abl_link_request_beat_at_time(
    abl_link_session_state session_state, double beat, int64_t time, double quantum)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->requestBeatAtTime(beat, std::chrono::microseconds{time}, quantum);
  }

  void abl_link_force_beat_at_time(
    abl_link_session_state session_state, double beat, int64_t time, double quantum)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->forceBeatAtTime(beat, std::chrono::microseconds{time}, quantum);
  }

  void abl_link_set_is_playing(
    abl_link_session_state session_state, bool is_playing, int64_t time)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->setIsPlaying(is_playing, std::chrono::microseconds(time));
  }

  bool abl_link_is_playing(abl_link_session_state session_state)
  {
    return reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->isPlaying();
  }

  int64_t abl_link_time_for_is_playing(abl_link_session_state session_state)
  {
    return static_cast<int64_t>(
      reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
        ->timeForIsPlaying()
        .count());
  }

  void abl_link_request_beat_at_start_playing_time(
    abl_link_session_state session_state, double beat, double quantum)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->requestBeatAtStartPlayingTime(beat, quantum);
  }

  void abl_link_set_is_playing_and_request_beat_at_time(
    abl_link_session_state session_state,
    bool is_playing,
    int64_t time,
    double beat,
    double quantum)
  {
    reinterpret_cast<ableton::Link::SessionState *>(session_state.impl)
      ->setIsPlayingAndRequestBeatAtTime(
        is_playing, std::chrono::microseconds{time}, beat, quantum);
  }

  bool abl_link_audio_is_link_audio_enabled(struct abl_link link)
  {
    return reinterpret_cast<ableton::LinkAudio *>(link.impl)->isLinkAudioEnabled();
  }

  void abl_link_audio_enable_link_audio(struct abl_link link, bool enabled)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->enableLinkAudio(enabled);
  }

  size_t abl_link_audio_peer_name(struct abl_link link, char *buffer, size_t buffer_size)
  {
    return copyStringToBuffer(
      reinterpret_cast<ableton::LinkAudio *>(link.impl)->peerName(), buffer, buffer_size);
  }

  void abl_link_audio_set_peer_name(struct abl_link link, const char *name)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->setPeerName(name);
  }

  struct abl_link_audio_channel_list abl_link_audio_get_channels(struct abl_link link)
  {
    struct abl_link_audio_channel_list result{};
    const auto channels = reinterpret_cast<ableton::LinkAudio *>(link.impl)->channels();
    result.count = channels.size();
    if (result.count == 0)
    {
      return result;
    }

    result.channels = static_cast<struct abl_link_audio_channel *>(
      std::calloc(result.count, sizeof(struct abl_link_audio_channel)));
    if (!result.channels)
    {
      result.count = 0;
      return result;
    }

    for (size_t i = 0; i < result.count; ++i)
    {
      const auto &channel = channels[i];
      result.channels[i].id = toChannelId(channel.id);
      result.channels[i].peer_id = toPeerId(channel.peerId);
      result.channels[i].name = copyCString(channel.name);
      result.channels[i].peer_name = copyCString(channel.peerName);
    }

    return result;
  }

  void abl_link_audio_free_channel_list(struct abl_link_audio_channel_list list)
  {
    if (!list.channels)
    {
      return;
    }
    for (size_t i = 0; i < list.count; ++i)
    {
      std::free(const_cast<char *>(list.channels[i].name));
      std::free(const_cast<char *>(list.channels[i].peer_name));
    }
    std::free(list.channels);
  }

  void abl_link_audio_set_channels_changed_callback(
    struct abl_link link, void (*callback)(void *context), void *context)
  {
    reinterpret_cast<ableton::LinkAudio *>(link.impl)->setChannelsChangedCallback(
      [callback, context]()
      {
        if (callback)
        {
          (*callback)(context);
        }
      });
  }

  struct abl_link_audio_sink abl_link_audio_sink_create(
    struct abl_link link, const char *name, size_t max_num_samples)
  {
    return {reinterpret_cast<void *>(new ableton::LinkAudioSink(
      *reinterpret_cast<ableton::LinkAudio *>(link.impl), name, max_num_samples))};
  }

  void abl_link_audio_sink_destroy(struct abl_link_audio_sink sink)
  {
    delete reinterpret_cast<ableton::LinkAudioSink *>(sink.impl);
  }

  size_t abl_link_audio_sink_name(
    struct abl_link_audio_sink sink, char *buffer, size_t buffer_size)
  {
    return copyStringToBuffer(
      reinterpret_cast<ableton::LinkAudioSink *>(sink.impl)->name(), buffer, buffer_size);
  }

  void abl_link_audio_sink_set_name(struct abl_link_audio_sink sink, const char *name)
  {
    reinterpret_cast<ableton::LinkAudioSink *>(sink.impl)->setName(name);
  }

  void abl_link_audio_sink_request_max_num_samples(
    struct abl_link_audio_sink sink, size_t num_samples)
  {
    reinterpret_cast<ableton::LinkAudioSink *>(sink.impl)->requestMaxNumSamples(
      num_samples);
  }

  size_t abl_link_audio_sink_max_num_samples(struct abl_link_audio_sink sink)
  {
    return reinterpret_cast<ableton::LinkAudioSink *>(sink.impl)->maxNumSamples();
  }

  struct abl_link_audio_sink_buffer_handle abl_link_audio_sink_retain_buffer(
    struct abl_link_audio_sink sink)
  {
    struct abl_link_audio_sink_buffer_handle result{};
    auto *cppSink = reinterpret_cast<ableton::LinkAudioSink *>(sink.impl);
    auto *cppHandle = new ableton::LinkAudioSink::BufferHandle(*cppSink);
    result.impl = cppHandle;
    result.samples = cppHandle->samples;
    result.max_num_samples = cppHandle->maxNumSamples;
    return result;
  }

  bool abl_link_audio_sink_buffer_is_valid(
    const struct abl_link_audio_sink_buffer_handle *handle)
  {
    if (!handle || !handle->impl)
    {
      return false;
    }
    const auto *cppHandle =
      reinterpret_cast<const ableton::LinkAudioSink::BufferHandle *>(handle->impl);
    return static_cast<bool>(*cppHandle);
  }

  bool abl_link_audio_sink_buffer_commit(struct abl_link_audio_sink_buffer_handle *handle,
    abl_link_session_state session_state,
    double beats_at_buffer_begin,
    double quantum,
    size_t num_frames,
    size_t num_channels,
    uint32_t sample_rate)
  {
    if (!handle || !handle->impl)
    {
      return false;
    }
    auto *cppHandle =
      reinterpret_cast<ableton::LinkAudioSink::BufferHandle *>(handle->impl);
    const auto *state =
      reinterpret_cast<ableton::Link::SessionState *>(session_state.impl);
    const auto result = cppHandle->commit(
      *state, beats_at_buffer_begin, quantum, num_frames, num_channels, sample_rate);
    delete cppHandle;
    handle->impl = nullptr;
    handle->samples = nullptr;
    handle->max_num_samples = 0;
    return result;
  }

  void abl_link_audio_sink_buffer_release(
    struct abl_link_audio_sink_buffer_handle *handle)
  {
    if (!handle || !handle->impl)
    {
      return;
    }
    delete reinterpret_cast<ableton::LinkAudioSink::BufferHandle *>(handle->impl);
    handle->impl = nullptr;
    handle->samples = nullptr;
    handle->max_num_samples = 0;
  }

  bool abl_link_audio_source_buffer_info_begin_beats(
    const struct abl_link_audio_source_buffer_info *info,
    abl_link_session_state session_state,
    double quantum,
    double *out_beats)
  {
    if (!info || !out_beats || !session_state.impl)
    {
      return false;
    }
    if (info->sample_rate == 0)
    {
      return false;
    }
    auto cppInfo = ableton::LinkAudioSource::BufferHandle::Info{};
    cppInfo.numChannels = info->num_channels;
    cppInfo.numFrames = info->num_frames;
    cppInfo.sampleRate = info->sample_rate;
    cppInfo.count = info->count;
    cppInfo.sessionBeatTime = info->session_beat_time;
    cppInfo.tempo = info->tempo;
    cppInfo.sessionId = toCppSessionId(info->session_id);

    const auto *state =
      reinterpret_cast<const ableton::Link::SessionState *>(session_state.impl);
    const auto beats = cppInfo.beginBeats(*state, quantum);
    if (!beats)
    {
      return false;
    }
    *out_beats = *beats;
    return true;
  }

  bool abl_link_audio_source_buffer_info_end_beats(
    const struct abl_link_audio_source_buffer_info *info,
    abl_link_session_state session_state,
    double quantum,
    double *out_beats)
  {
    if (!info || !out_beats || !session_state.impl)
    {
      return false;
    }
    if (info->sample_rate == 0)
    {
      return false;
    }
    auto cppInfo = ableton::LinkAudioSource::BufferHandle::Info{};
    cppInfo.numChannels = info->num_channels;
    cppInfo.numFrames = info->num_frames;
    cppInfo.sampleRate = info->sample_rate;
    cppInfo.count = info->count;
    cppInfo.sessionBeatTime = info->session_beat_time;
    cppInfo.tempo = info->tempo;
    cppInfo.sessionId = toCppSessionId(info->session_id);

    const auto *state =
      reinterpret_cast<const ableton::Link::SessionState *>(session_state.impl);
    const auto beats = cppInfo.endBeats(*state, quantum);
    if (!beats)
    {
      return false;
    }
    *out_beats = *beats;
    return true;
  }

  struct abl_link_audio_source abl_link_audio_source_create(struct abl_link link,
    struct abl_link_audio_channel_id channel_id,
    void (*callback)(const struct abl_link_audio_source_buffer *buffer, void *context),
    void *context)
  {
    auto *cppLink = reinterpret_cast<ableton::LinkAudio *>(link.impl);
    const auto id = toCppChannelId(channel_id);
    return {reinterpret_cast<void *>(new ableton::LinkAudioSource(*cppLink, id,
      [callback, context](ableton::LinkAudioSource::BufferHandle handle)
      {
        if (!callback)
        {
          return;
        }
        struct abl_link_audio_source_buffer buffer{};
        buffer.samples = handle.samples;
        buffer.info.num_channels = handle.info.numChannels;
        buffer.info.num_frames = handle.info.numFrames;
        buffer.info.sample_rate = handle.info.sampleRate;
        buffer.info.count = handle.info.count;
        buffer.info.session_beat_time = handle.info.sessionBeatTime;
        buffer.info.tempo = handle.info.tempo;
        buffer.info.session_id = toSessionId(handle.info.sessionId);
        callback(&buffer, context);
      }))};
  }

  void abl_link_audio_source_destroy(struct abl_link_audio_source source)
  {
    delete reinterpret_cast<ableton::LinkAudioSource *>(source.impl);
  }

  struct abl_link_audio_channel_id abl_link_audio_source_id(
    struct abl_link_audio_source source)
  {
    if (!source.impl)
    {
      struct abl_link_audio_channel_id empty{};
      return empty;
    }
    return toChannelId(reinterpret_cast<ableton::LinkAudioSource *>(source.impl)->id());
  }
}
