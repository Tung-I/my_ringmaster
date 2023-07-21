#ifndef AUDIO_ENCODER_HH
#define AUDIO_ENCODER_HH

extern "C" {
#include <opus/opus.h>
}

#include <deque>
#include <map>
#include <memory>
#include <optional>

#include "exception.hh"
#include "audio.hh"  // Assuming you have an audio.hh similar to image.hh that handles audio samples
#include "protocol.hh"
#include "file_descriptor.hh"

class AudioEncoder
{
public:
  AudioEncoder(const uint32_t sample_rate, const uint32_t frame_duration,
               const std::string & output_path = "");
  ~AudioEncoder();

  void compress_frame(const RawAudio & raw_audio);

  void add_unacked(const AudioDatagram & datagram);
  void add_unacked(AudioDatagram && datagram);

  void handle_ack(const std::shared_ptr<AckMsg> & ack);

  void output_periodic_stats();

  uint32_t frame_id() const { return frame_id_; }
  std::deque<AudioDatagram> & send_buf() { return send_buf_; }
  std::map<SeqNum, AudioDatagram> & unacked() { return unacked_; }

  void set_verbose(const bool verbose) { verbose_ = verbose; }
  void set_target_bitrate(const unsigned int bitrate_kbps);

  AudioEncoder(const AudioEncoder & other) = delete;
  const AudioEncoder & operator=(const AudioEncoder & other) = delete;
  AudioEncoder(AudioEncoder && other) = delete;
  AudioEncoder & operator=(AudioEncoder && other) = delete;

private:
  uint32_t sample_rate_;
  uint32_t frame_duration_;
  std::optional<FileDescriptor> output_fd_;

  bool verbose_ {false};
  unsigned int target_bitrate_ {0};

  OpusEncoder *encoder_;

  uint32_t frame_id_ {0};

  std::deque<AudioDatagram> send_buf_ {};

  std::map<SeqNum, AudioDatagram> unacked_ {};

  std::optional<unsigned int> min_rtt_us_ {};
  std::optional<double> ewma_rtt_us_ {};
  static constexpr double ALPHA = 0.2;

  unsigned int num_encoded_frames_ {0};
  double total_encode_time_ms_ {0.0};
  double max_encode_time_ms_ {0.0};

  static constexpr unsigned int MAX_NUM_RTX = 3;
  static constexpr uint64_t MAX_UNACKED_US = 1000 * 1000; // 1 second

  void add_rtt_sample(const unsigned int rtt_us);

  void encode_frame(const RawAudio & raw_audio);

  size_t packetize_encoded_frame();

};

#endif /* AUDIO_ENCODER_HH */