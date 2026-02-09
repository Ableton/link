/*! @file LinkAudio.hpp
 *  @copyright 2025, Ableton AG, Berlin. All rights reserved.
 *  @brief Library for cross-device shared tempo, quantized beat grid, and sharing audio
 *
 *  @license:
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
#include <optional>
#include <string>
#include <vector>

namespace ableton
{

/*! @class LinkAudio and BasicLinkAudio
 *  @brief LinkAudio provides the Link functionality plus audio sharing.
 *  To enable audio sharing include this header and replace the Link class with the
 *  LinkAudio and Link should not be used simultaneously.
 *
 *  @discussion
 *  LinkAudio provides the same functionality as Link. See Link.hpp for the respective
 *  documentation. On top of that, LinkAudio allows to announce and discover audio
 *  channels in a Link session. When sending and receiving audio, the Link beat time grid
 *  and quantum can be used to align audio from different peers.
 *
 *  Channels in a Link session are represented as sinks on the sending side and sources on
 *  the receiving side.
 *  Sinks only send audio if at least one corresponding source is present in the session.
 *  Audio buffers are interleaved and samples are represented as 16-bit signed integers.
 */

template <typename Clock>
class BasicLinkAudio : public BasicLink<Clock>
{
public:
  /*! @brief Construct with an initial tempo and local peer name for identification in the
   *  Link session.
   */
  BasicLinkAudio(double bpm, std::string name);

  ~BasicLinkAudio();

  /*! @brief Is audio currently enabled?
   *  Thread-safe: yes
   *  Realtime-safe: yes
   */
  bool isLinkAudioEnabled() const;

  /*! @brief Enable or disable audio.
   *  Thread-safe: yes
   *  Realtime-safe: no
   */
  void enableLinkAudio(bool bEnable);

  /*! @brief Change the local peer name for identification in the Link session.
   *  Thread-safe: yes
   *  Realtime-safe: no
   */
  void setPeerName(std::string name);

  /*! @brief Register a callback to be notified when the set of available
   *  audio channels changes. This will be called when channels are discovered or
   *  disappeared and when names change.
   *  Thread-safe: yes
   *  Realtime-safe: no
   *
   *  @discussion The callback is invoked on a Link-managed thread.
   *  @param callback The callback signature is: void ()
   */
  template <typename Callback>
  void setChannelsChangedCallback(Callback callback);

  /*! @struct Channel
   *  @brief Describes an audio channel available in the Link session.
   *  @discussion
   *  The unique identifiers for channels and peers are persistent for the lifetime of a
   *  channel. Names may change over time and are meant for display purposes.
   */
  struct Channel
  {
    ChannelId id;         /*!< Unique identifier for the channel. */
    std::string name;     /*!< Name of the channel. */
    PeerId peerId;        /*!< Identifier of the peer providing the channel. */
    std::string peerName; /*!< Name of the peer providing the channel. */
  };

  /*! @brief Get the list of currently available audio channels in the session.
   *  Thread-safe: yes
   *  Realtime-safe: yes
   */
  std::vector<Channel> channels() const;

  /*! @brief Call a function on the Link thread.
   *  Thread-safe: yes
   *  Realtime-safe: no
   *
   *  @discussion The function will be called on the Link thread, which is managed by the
   *  LinkAudio instance. This is useful for setting the thread priority for example.
   *  @param func The function signature is: void ()
   */
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

/*! @class LinkAudioSink
 *  @brief Announces an audio channel to the Link session.
 *
 *  @discussion
 *  The announced channel is visible to other peers for the lifetime of the sink.
 *  The sink can be used to write audio samples, which will be sent to peers that have
 *  respective sources.
 */
class LinkAudioSink
{
public:
  /*! @brief Construct a LinkAudioSink with a LinkAudio instance, a name of the audio
   *  channel, and the maximum buffer size in samples. This buffer size should account for
   *  the number of channels times the number of samples per channel in one audio
   *  callback.
   */
  template <typename LinkAudio>
  LinkAudioSink(LinkAudio& link, std::string name, size_t maxNumSamples);

  LinkAudioSink(const LinkAudioSink&) = default;
  LinkAudioSink& operator=(const LinkAudioSink&) = default;
  LinkAudioSink(LinkAudioSink&&) = default;
  LinkAudioSink& operator=(LinkAudioSink&&) = default;

  /*! @brief Get the name of the channel.
   *  Thread-safe: no
   *  Realtime-safe: no
   */
  std::string name() const;

  /*! @brief Set the name of this channel.
   *  Thread-safe: no
   *  Realtime-safe: no
   */
  void setName(std::string name);

  /*! @brief Request a maximum buffer size for future buffers.
   *  Thread-safe: yes
   *  Realtime-safe: yes
   *
   *  @discussion Increase the number of samples retained buffer handles can hold. If the
   *  requested number of samples is smaller than the current maximum number of samples
   *  this is a no-op.
   */
  void requestMaxNumSamples(size_t numSamples);

  /*! @brief Get the current maximum number of samples a buffer handle can hold.
   *  Thread-safe: yes
   *  Realtime-safe: yes
   */
  size_t maxNumSamples() const;

  /*! @struct BufferHandle
   *  @brief Handle to a buffer for writing audio samples.
   */
  struct BufferHandle
  {
    /*! @brief Retain a buffer for writing audio samples. Only on BufferHandle can be
     *  retained at a time. A BufferHandle should never outlive the LinkAudioSink it was
     *  created from.
     *  Thread-safe: no
     *  Realtime-safe: yes
     */
    BufferHandle(LinkAudioSink&);

    BufferHandle(const BufferHandle&) = delete;
    BufferHandle(BufferHandle&& other) = delete;
    BufferHandle& operator=(const BufferHandle&) = delete;
    BufferHandle& operator=(BufferHandle&& other) = delete;

    ~BufferHandle();

    /*! @brief Check if the handle is valid. Make sure to check this before
     *  using the handle. The handle may be invalid if no corresponding source exists or
     *  no buffer is available.
     *
     *  Thread-safe: no
     *  Realtime-safe: yes
     */
    operator bool() const;
    int16_t* samples;     /*!< Pointer to the buffer for writing samples. */
    size_t maxNumSamples; /*!< Maximum number of samples that can be written. */


    /*! @brief Commit the buffer after writing samples. After this the buffer the object
     *  should not be used anymore.
     *  @param sessionState The current Link session state.
     *  @param beatsAtBufferBegin Beat at the start of the buffer.
     *  @param quantum Quantum value for beat mapping.
     *  @param numFrames Number of frames written. numFrames * numChannels may not exceed
     *  maxNumSamples.
     *  @param numChannels Number of channels. Can be 1 for mono or 2 for stereo.
     *  @param sampleRate Sample rate in Hz.
     *  @return True if the buffer was successfully committed.
     *
     *  Thread-safe: no
     *  Realtime-safe: yes
     *
     *  @discussion The Link session state, the quantum, and the beats at buffer begin
     *  must be same as used for rendering the audio locally. Changes to the Link session
     *  state should always be made before rendering and eventually writing the buffer.
     */
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

/*! @class LinkAudioSource
 *  @brief Receives audio streams from other peers in the Link session.
 *
 *  @discussion
 *  LinkAudioSource allows an application to subscribe to an audio channel
 *  published by another peer, receiving buffers as they become available.
 */
class LinkAudioSource
{
public:
  struct BufferHandle;

  /*! @brief Construct a LinkAudioSource to receive audio for a specifc channel.
   *  @param link The LinkAudio instance.
   *  @param id The ID of the channel to be received.
   *  @param callback Callback invoked on a Link-managed thread when a buffer is received.
   *  The callback signature is: void (BufferHandle bufferHandle)
   */
  template <typename LinkAudio, typename Callback>
  LinkAudioSource(LinkAudio& link, ChannelId id, Callback callback);

  LinkAudioSource(const LinkAudioSource&) = default;
  LinkAudioSource& operator=(const LinkAudioSource&) = default;
  LinkAudioSource(LinkAudioSource&&) = default;
  LinkAudioSource& operator=(LinkAudioSource&&) = default;

  ~LinkAudioSource();

  /*! @brief Get the channel ID of the corresponding source.
   *  Thread-safe: yes
   *  Realtime-safe: yes
   */
  ChannelId id() const;

  /*! @struct BufferHandle
   *  @brief Handle to a buffer containing received audio samples.
   */
  struct BufferHandle
  {
    /*! @struct Info
     *  @brief Metadata about the received audio buffer.
     */
    struct Info
    {
      size_t numChannels;     /*!< Number of channels in the buffer. */
      size_t numFrames;       /*!< Number of frames in the buffer. */
      uint32_t sampleRate;    /*!< Sample rate in Hz. */
      uint64_t count;         /*!< Sequence number of the buffer. */
      double sessionBeatTime; /*!< Use beginBeats() to map this to local beat time. */
      double tempo;           /*!< Tempo in beats per minute. */
      SessionId sessionId;    /*!< ID of the session the buffer belongs to. */

      /*! @brief Map the beat time at the begin of the buffer to the local Link session
       *  state.
       *  @param sessionState The current Link session state.
       *  @param quantum Quantum value for beat mapping.
       *  @return Local beat time at buffer begin. Returns nullopt if the buffer
       *  originates from a different Link Session.
       */
      template <typename SessionState>
      std::optional<double> beginBeats(const SessionState&, double quantum) const;

      /*! @brief Map the beat time at the end of the buffer to the local Link session
       *  state.
       *  @param sessionState The current Link session state.
       *  @param quantum Quantum value for beat mapping.
       *  @return Local beat time at buffer end. Returns nullopt if the buffer
       *  originates from a different Link Session.
       */
      template <typename SessionState>
      std::optional<double> endBeats(const SessionState&, double quantum) const;
    };

    int16_t* samples; /*!< Pointer to the buffer of received samples. */
    Info info;        /*!< Information about the buffer. */
  };

private:
  std::shared_ptr<link_audio::Source> mpImpl;
};

} // namespace ableton

#include <ableton/LinkAudio.ipp>
