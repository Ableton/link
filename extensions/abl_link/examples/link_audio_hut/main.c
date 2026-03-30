/* Copyright 2026, Ableton AG, Berlin. All rights reserved.
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

#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum
{
  kSampleRate = 48000,
  kNumFrames = 4800
};

typedef struct app_state
{
  struct abl_link link;
  struct abl_link_audio_sink sink;
  struct abl_link_audio_sink ping_pong_sink;
  struct abl_link_audio_source source;
  struct abl_link_audio_channel_id source_channel_id;
  abl_link_session_state session_state;
  abl_link_session_state ping_pong_session_state;
  atomic_bool channels_dirty;
  atomic_uint_fast64_t received_buffers;
  double quantum;
  double phase;
} app_state;

static volatile sig_atomic_t g_stop = 0;

static bool id_equal(
  struct abl_link_audio_channel_id lhs, struct abl_link_audio_channel_id rhs)
{
  return memcmp(lhs.bytes, rhs.bytes, sizeof(lhs.bytes)) == 0;
}

static bool is_zero_id(struct abl_link_audio_channel_id value)
{
  const struct abl_link_audio_channel_id zero = {0};
  return id_equal(value, zero);
}

static void on_sigint(int signum)
{
  (void)signum;
  g_stop = 1;
}

static void on_channels_changed(void *context)
{
  app_state *state = (app_state *)context;
  atomic_store(&state->channels_dirty, true);
}

static void on_source_buffer(
  const struct abl_link_audio_source_buffer *buffer, void *context)
{
  app_state *state = (app_state *)context;

  atomic_fetch_add(&state->received_buffers, 1);

  struct abl_link_audio_sink_buffer_handle handle =
    abl_link_audio_sink_retain_buffer(state->ping_pong_sink);
  if (!abl_link_audio_sink_buffer_is_valid(&handle))
  {
    abl_link_audio_sink_buffer_release(&handle);
    return;
  }

  const size_t num_frames = buffer->info.num_frames;
  const size_t num_channels = buffer->info.num_channels;
  const size_t total_samples = num_frames * num_channels;

  if (handle.max_num_samples < total_samples)
  {
    abl_link_audio_sink_buffer_release(&handle);
    abl_link_audio_sink_request_max_num_samples(state->ping_pong_sink, total_samples);
    return;
  }

  abl_link_capture_audio_session_state(state->link, state->ping_pong_session_state);

  double begin_beats;
  if (!abl_link_audio_source_buffer_info_begin_beats(
        &buffer->info, state->ping_pong_session_state, state->quantum, &begin_beats))
  {
    abl_link_audio_sink_buffer_release(&handle);
    return;
  }

  memcpy(handle.samples, buffer->samples, total_samples * sizeof(int16_t));
  abl_link_audio_sink_buffer_commit(&handle, state->ping_pong_session_state, begin_beats,
    state->quantum, num_frames, num_channels, buffer->info.sample_rate);
}

static void update_channels(app_state *state)
{
  const struct abl_link_audio_channel_list list =
    abl_link_audio_get_channels(state->link);
  printf("\n\nLinkAudio channels (%zu):\n", list.count);
  for (size_t i = 0; i < list.count; ++i)
  {
    printf("  %zu: %s | %s\n", i, list.channels[i].peer_name, list.channels[i].name);
  }

  // Receive from the first channel
  if (list.count > 0)
  {
    const struct abl_link_audio_channel_id first_channel = list.channels[0].id;
    if (!id_equal(state->source_channel_id, first_channel))
    {
      if (!is_zero_id(state->source_channel_id))
      {
        abl_link_audio_source_destroy(state->source);
        state->source = (struct abl_link_audio_source){NULL};
      }
      state->source =
        abl_link_audio_source_create(state->link, first_channel, on_source_buffer, state);
      state->source_channel_id = first_channel;
    }
  }
  else if (!is_zero_id(state->source_channel_id))
  {
    abl_link_audio_source_destroy(state->source);
    state->source = (struct abl_link_audio_source){NULL};
    state->source_channel_id = (struct abl_link_audio_channel_id){0};
  }

  abl_link_audio_free_channel_list(list);
}

static void process_audio(app_state *state, int64_t host_time)
{
  const double step = (2.0 * M_PI * 440.0) / (double)kSampleRate;
  abl_link_capture_audio_session_state(state->link, state->session_state);
  const double beats_at_begin =
    abl_link_beat_at_time(state->session_state, host_time, state->quantum);

  struct abl_link_audio_sink_buffer_handle handle =
    abl_link_audio_sink_retain_buffer(state->sink);
  if (abl_link_audio_sink_buffer_is_valid(&handle))
  {
    size_t num_frames = kNumFrames;
    if (num_frames > handle.max_num_samples)
    {
      num_frames = handle.max_num_samples;
    }
    for (size_t i = 0; i < num_frames; ++i)
    {
      const double sample = sin(state->phase) * 0.2;
      state->phase += step;
      if (state->phase >= 2.0 * M_PI)
      {
        state->phase -= 2.0 * M_PI;
      }
      handle.samples[i] = (int16_t)(sample * 32767.0);
    }
    abl_link_audio_sink_buffer_commit(&handle, state->session_state, beats_at_begin,
      state->quantum, num_frames, 1, kSampleRate);
  }
  else
  {
    abl_link_audio_sink_buffer_release(&handle);
  }
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    printf("Usage: link_audio_hut_c <name>\n");
    return 1;
  }

  app_state state = {
    .link = abl_link_create(120.0),
    .sink = {NULL},
    .ping_pong_sink = {NULL},
    .source = {NULL},
    .source_channel_id = {0},
    .session_state = abl_link_create_session_state(),
    .ping_pong_session_state = abl_link_create_session_state(),
    .channels_dirty = true,
    .received_buffers = 0,
    .quantum = 4.0,
    .phase = 0.0,
  };

  signal(SIGINT, on_sigint);

  // init link audio
  abl_link_audio_set_peer_name(state.link, argv[1]);
  abl_link_enable(state.link, true);
  abl_link_audio_enable_link_audio(state.link, true);
  abl_link_audio_set_channels_changed_callback(state.link, on_channels_changed, &state);

  // init sinks
  state.sink = abl_link_audio_sink_create(state.link, "Sink", kNumFrames);
  state.ping_pong_sink =
    abl_link_audio_sink_create(state.link, "Ping-Pong Sink", kNumFrames * 2);

  // init playing state
  abl_link_capture_app_session_state(state.link, state.session_state);
  const int64_t now = abl_link_clock_micros(state.link);
  abl_link_set_is_playing_and_request_beat_at_time(
    state.session_state, true, now, 0.0, state.quantum);
  abl_link_commit_app_session_state(state.link, state.session_state);
  const int64_t buffer_duration_micros =
    (int64_t)((double)kNumFrames * 1000000.0 / (double)kSampleRate);
  int64_t next_send_time_micros = abl_link_clock_micros(state.link);

  while (true)
  {
    if (g_stop)
    {
      break;
    }

    // wait before processing audio
    const int64_t current_time_micros = abl_link_clock_micros(state.link);
    if (current_time_micros < next_send_time_micros)
    {
      continue;
    }

    process_audio(&state, current_time_micros);

    if (atomic_exchange(&state.channels_dirty, false))
    {
      update_channels(&state);
    }

    abl_link_capture_app_session_state(state.link, state.session_state);

    printf("\r");
    const bool has_source = !is_zero_id(state.source_channel_id);
    printf("Link %s [audio %s] peers %" PRIu64
           " tempo %.2f beat %.2f source %s recv %" PRIuFAST64,
      abl_link_is_enabled(state.link) ? "on" : "off",
      abl_link_audio_is_link_audio_enabled(state.link) ? "on" : "off",
      abl_link_num_peers(state.link), abl_link_tempo(state.session_state),
      abl_link_beat_at_time(state.session_state, current_time_micros, state.quantum),
      has_source ? "on" : "off", atomic_load(&state.received_buffers));
    fflush(stdout);
    next_send_time_micros += buffer_duration_micros;
    if (current_time_micros > next_send_time_micros)
    {
      next_send_time_micros = current_time_micros + buffer_duration_micros;
    }
  }

  printf("\nShutting down...\n");
  if (!is_zero_id(state.source_channel_id))
  {
    abl_link_audio_source_destroy(state.source);
  }
  abl_link_audio_sink_destroy(state.ping_pong_sink);
  abl_link_audio_sink_destroy(state.sink);
  abl_link_destroy_session_state(state.ping_pong_session_state);
  abl_link_destroy_session_state(state.session_state);
  abl_link_destroy(state.link);
  return 0;
}
