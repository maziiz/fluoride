/******************************************************************************
 *
 *  Copyright 2018 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#pragma once
#include <base/callback_forward.h>
#include <hardware/bt_hearing_aid.h>
#include <future>
constexpr uint16_t HA_INTERVAL_10_MS = 10;
constexpr uint16_t HA_INTERVAL_20_MS = 20;
// Masks for checking capability support
constexpr uint8_t CAPABILITY_SIDE = 0x01;
constexpr uint8_t CAPABILITY_BINAURAL = 0x02;
constexpr uint8_t CAPABILITY_RESERVED = 0xFC;
/** Implementations of HearingAid will also implement this interface */
class HearingAidAudioReceiver {
 public:
  virtual ~HearingAidAudioReceiver() = default;
  virtual void OnAudioDataReady(const std::vector<uint8_t>& data) = 0;
  virtual void OnAudioSuspend(std::promise<void> do_suspend_promise) = 0;
  virtual void OnAudioResume(std::promise<void> do_resume_promise) = 0;
};
struct AudioStats {
  size_t packet_flush_count;
  size_t packet_send_count;
  size_t frame_flush_count;
  size_t frame_send_count;
  AudioStats() { Reset(); }
  void Reset() {
    packet_flush_count = 0;
    packet_send_count = 0;
    frame_flush_count = 0;
    frame_send_count = 0;
  }
};
/** Possible states for the Connection Update status */
typedef enum {
  NONE,      // Not Connected
  AWAITING,  // Waiting for start the Connection Update operation
  STARTED,   // Connection Update has started
  COMPLETED  // Connection Update is completed successfully
} connection_update_status_t;
struct HearingDevice {
  RawAddress address;
  /* This is true only during first connection to profile, until we store the
   * device */
  bool first_connection;
  /* we are making active attempt to connect to this device, 'direct connect'.
   * This is true only during initial phase of first connection. */
  bool connecting_actively;
  /* For two hearing aids, you must update their parameters one after another,
   * not simulteanously, to ensure start of connection events for both devices
   * are far from each other. This status tracks whether this device is waiting
   * for update of parameters, that should happen after "LE Connection Update
   * Complete" event
   */
  connection_update_status_t connection_update_status;
  uint16_t requested_connection_interval;
  /* if true, we are connected, L2CAP socket is open, we can stream audio.
     However, the actual audio stream also depends on whether the
     Audio Service has resumed.
   */
  bool accepting_audio;
  uint16_t conn_id;
  uint16_t gap_handle;
  uint16_t audio_control_point_handle;
  uint16_t volume_handle;
  uint16_t psm;
  uint8_t capabilities;
  uint64_t hi_sync_id;
  uint16_t render_delay;
  uint16_t preparation_delay;
  uint16_t codecs;
  AudioStats audio_stats;
  /* Keep tracks of whether the "Start Cmd" has been send to this device. When
     the "Stop Cmd" is send or when this device disconnects, then this flag is
     cleared. Please note that the "Start Cmd" is not send during device
     connection in the case when the audio is suspended. */
  bool playback_started;
  HearingDevice(const RawAddress& address, uint16_t psm, uint8_t capabilities,
                uint16_t codecs, uint16_t audio_control_point_handle,
                uint16_t volume_handle, uint64_t hiSyncId,
                uint16_t render_delay, uint16_t preparation_delay)
      : address(address),
        first_connection(false),
        connecting_actively(false),
        connection_update_status(NONE),
        accepting_audio(false),
        conn_id(0),
        gap_handle(0),
        audio_control_point_handle(audio_control_point_handle),
        volume_handle(volume_handle),
        psm(psm),
        capabilities(capabilities),
        hi_sync_id(hiSyncId),
        render_delay(render_delay),
        preparation_delay(preparation_delay),
        codecs(codecs),
        playback_started(false) {}
  HearingDevice(const RawAddress& address, bool first_connection)
      : address(address),
        first_connection(first_connection),
        connecting_actively(first_connection),
        connection_update_status(NONE),
        accepting_audio(false),
        conn_id(0),
        gap_handle(0),
        psm(0),
        playback_started(false) {}
  HearingDevice() : HearingDevice(RawAddress::kEmpty, false) {}
  /* return true if this device represents left Hearing Aid. Returned value is
   * valid only after capabilities are discovered */
  bool isLeft() const { return !(capabilities & CAPABILITY_SIDE); }
};
class HearingAid {
 public:
  virtual ~HearingAid() = default;
  static void Initialize(bluetooth::hearing_aid::HearingAidCallbacks* callbacks,
                         base::Closure initCb);
  static void CleanUp();
  static bool IsHearingAidRunning();
  static HearingAid* Get();
  static void DebugDump(int fd);
  static void AddFromStorage(const HearingDevice& dev_info,
                             uint16_t is_white_listed);
  static int GetDeviceCount();
  virtual void Connect(const RawAddress& address) = 0;
  virtual void Disconnect(const RawAddress& address) = 0;
  virtual void SetVolume(int8_t volume) = 0;
};
/* Represents configuration of audio codec, as exchanged between hearing aid and
 * phone.
 * It can also be passed to the audio source to configure its parameters.
 */
struct CodecConfiguration {
  /** sampling rate that the codec expects to receive from audio framework */
  uint32_t sample_rate;
  /** bitrate that codec expects to receive from audio framework in bits per
   * channel */
  uint32_t bit_rate;
  /** Data interval determines how often we send samples to the remote. This
   * should match how often we grab data from audio source, optionally we can
   * grab data every 2 or 3 intervals, but this would increase latency.
   *
   * Value is provided in ms, must be divisable by 1.25 to make sure the
   * connection interval is integer.
   */
  uint16_t data_interval_ms;
};
/** Represents source of audio for hearing aids */
class HearingAidAudioSource {
 public:
  static void Start(const CodecConfiguration& codecConfiguration,
                    HearingAidAudioReceiver* audioReceiver);
  static void Stop();
  static void Initialize();
  static void CleanUp();
  static void DebugDump(int fd);
};
