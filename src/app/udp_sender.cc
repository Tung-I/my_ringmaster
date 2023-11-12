#include <getopt.h>
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <utility>
#include <chrono>

#include "conversion.hh"
#include "timerfd.hh"
#include "udp_socket.hh"
#include "poller.hh"
#include "yuv4mpeg.hh"
#include "protocol.hh"
#include "vp9_encoder.hh"
#include "timestamp.hh"

using namespace std;
using namespace chrono;

// global variables in an unnamed namespace
namespace {
  constexpr unsigned int BILLION = 1000 * 1000 * 1000;
}

void print_usage(const string & program_name)
{
  cerr <<
  "Usage: " << program_name << " [options] port y4m\n\n"
  "Options:\n"
  "--mtu <MTU>                MTU for deciding UDP payload size\n"
  "-o, --output <file>        file to output performance results to\n"
  "-v, --verbose              enable more logging for debugging"
  << endl;
}

pair<Address, ConfigMsg> recv_config_msg(UDPSocket & udp_sock)
{
  // wait until a valid ConfigMsg is received
  while (true) {
    const auto & [peer_addr, raw_data] = udp_sock.recvfrom();
    const shared_ptr<Msg> msg = Msg::parse_from_string(raw_data.value());
    if (msg == nullptr or msg->type != Msg::Type::CONFIG) {
      cerr << "Unknown message type received on video port." << endl;
      continue; 
    }
    const auto config_msg = dynamic_pointer_cast<ConfigMsg>(msg);
    if (config_msg) {
      return {peer_addr, *config_msg};
    }
  }
}

pair<Address, SignalMsg> recv_signal_msg(UDPSocket & udp_sock)
{
  while (true) {
    const auto & [peer_addr, raw_data] = udp_sock.recvfrom();
    const shared_ptr<Msg> msg = Msg::parse_from_string(raw_data.value());
    if (msg == nullptr or msg->type != Msg::Type::SIGNAL) {
      cerr << "Unknown message type received on signal port." << endl;
      continue; 
    }
    const auto signal_msg = dynamic_pointer_cast<SignalMsg>(msg);
    if (signal_msg) {
      return {peer_addr, *signal_msg};
    }
  }
}

int main(int argc, char * argv[])
{
  // argument parsing
  string output_path;
  bool verbose = false;

  const option cmd_line_opts[] = {
    {"mtu",     required_argument, nullptr, 'M'},
    {"output",  required_argument, nullptr, 'o'},
    {"verbose", no_argument,       nullptr, 'v'},
    { nullptr,  0,                 nullptr,  0 },
  };

  while (true) {
    const int opt = getopt_long(argc, argv, "o:v", cmd_line_opts, nullptr);
    if (opt == -1) {
      break;
    }

    switch (opt) {
      case 'M':
        FrameDatagram::set_mtu(strict_stoi(optarg));
        break;
      case 'o':
        output_path = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if (optind != argc - 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  const auto video_port = narrow_cast<uint16_t>(strict_stoi(argv[optind]));
  const auto signal_port = narrow_cast<uint16_t>(video_port + 1);
  const string y4m_path = argv[optind + 1];
  UDPSocket video_sock;
  video_sock.bind({"0", video_port});
  cerr << "Local address: " << video_sock.local_address().str() << endl;
  UDPSocket signal_sock;
  signal_sock.bind({"0", signal_port});
  cerr << "Local address: " << signal_sock.local_address().str() << endl;

  // Ensure that the receiver is ready to receive the first datagram
  cerr << "Waiting for receiver..." << endl;
  const auto & [peer_addr_video, init_config_msg] = recv_config_msg(video_sock); 
  cerr << "Video stream address: " << peer_addr_video.str() << endl;
  video_sock.connect(peer_addr_video);
  const auto & [peer_addr_signal, init_signal_msg] = recv_signal_msg(signal_sock); 
  cerr << "Signal stream address: " << peer_addr_signal.str() << endl;
  signal_sock.connect(peer_addr_signal);

  // read configuration from the peer
  const auto init_width = init_config_msg.width;
  const auto init_height = init_config_msg.height;
  const auto init_frame_rate = init_config_msg.frame_rate;
  const auto init_target_bitrate = init_config_msg.target_bitrate;

  cerr << "Received config: width=" << to_string(init_width)
       << " height=" << to_string(init_height)
       << " FPS=" << to_string(init_frame_rate)
       << " bitrate=" << to_string(init_target_bitrate) << endl;

  // set UDP socket to non-blocking now
  video_sock.set_blocking(false);
  signal_sock.set_blocking(false);

  // open the video file
  YUV4MPEG video_input(y4m_path, init_width, init_height);

  // allocate a raw image
  RawImage raw_img(init_width, init_height);

  // initialize the encoder
  Encoder encoder(init_width, init_height, init_frame_rate, output_path);
  encoder.set_target_bitrate(init_target_bitrate);
  encoder.set_verbose(verbose);

  // create a periodic timer with the same period as the frame interval
  Poller poller;
  Timerfd fps_timer;
  const timespec frame_interval {0, static_cast<long>(BILLION / init_frame_rate)}; // {sec, nsec}
  fps_timer.set_time(frame_interval, frame_interval); // {initial expiration, interval}

  // // a counter for the number of frames sent
  // unsigned int num_frames_sent = 0;

  // read a raw frame when the periodic timer fires
  poller.register_event(fps_timer, Poller::In,
    [&]()
    {
      // being lenient: read raw frames 'num_exp' times and use the last one
      const auto num_exp = fps_timer.read_expirations(); 
      if (num_exp > 1) {
        cerr << "Warning: skipping " << num_exp - 1 << " raw frames" << endl;
      }

      for (unsigned int i = 0; i < num_exp; i++) {
        // fetch a raw frame into 'raw_img' from the video input
        if (not video_input.read_frame(raw_img)) {
          throw runtime_error("Reached the end of video input");
        }
      }

      //////////////////////////
      // num_frames_sent++;
      // if (num_frames_sent > 30) {
      //   exit(0);
      // }
      /////////////////////////

      // compress 'raw_img' into frame 'frame_id' and packetize it
      encoder.compress_frame(raw_img);

      // interested in socket being writable if there are datagrams to send
      if (not encoder.send_buf().empty()) {
        poller.activate(video_sock, Poller::Out);
      }
    }
  );

  // when the video socket is writable
  poller.register_event(video_sock, Poller::Out,
    [&]()
    {
      deque<FrameDatagram> & send_buf = encoder.send_buf();

      while (not send_buf.empty()) {
        auto & datagram = send_buf.front();

        // timestamp the sending time before sending
        datagram.send_ts = timestamp_us();

        if (video_sock.send(datagram.serialize_to_string())) {
          if (verbose) {
            cerr << "Sent datagram: frame_id=" << datagram.frame_id
                 << " frag_id=" << datagram.frag_id
                 << " frag_cnt=" << datagram.frag_cnt
                 << " rtx=" << datagram.num_rtx << endl;
          }

          // move the sent datagram to unacked if not a retransmission
          if (datagram.num_rtx == 0) {
            encoder.add_unacked(move(datagram));
          }

          send_buf.pop_front();
        } else { // EWOULDBLOCK; try again later
          datagram.send_ts = 0; // since it wasn't sent successfully
          break;
        }
      }

      // not interested in socket being writable if no datagrams to send
      if (send_buf.empty()) {
        poller.deactivate(video_sock, Poller::Out);
      }
    }
  );

  // when the video socket is readable
  poller.register_event(video_sock, Poller::In,
    [&]()
    {
      while (true) {
        const auto & raw_data = video_sock.recv();

        if (not raw_data) { // EWOULDBLOCK; try again when data is available
          break;
        }
        const shared_ptr<Msg> msg = Msg::parse_from_string(*raw_data);

        // ignore invalid or non-ACK messages
        if (msg == nullptr or msg->type != Msg::Type::ACK) {
          return;
        }

        const auto ack = dynamic_pointer_cast<AckMsg>(msg);

        if (verbose) {
          cerr << "Received ACK: frame_id=" << ack->frame_id
               << " frag_id=" << ack->frag_id << endl;
        }

        // RTT estimation, retransmission, etc.
        encoder.handle_ack(ack);

        // send_buf might contain datagrams to be retransmitted now
        if (not encoder.send_buf().empty()) {
          poller.activate(video_sock, Poller::Out);
        }
      }
    }
  );

  // create a periodic timer for outputting stats every second
  Timerfd stats_timer;
  const timespec stats_interval {1, 0};
  stats_timer.set_time(stats_interval, stats_interval);
  poller.register_event(stats_timer, Poller::In,
    [&]()
    {
      if (stats_timer.read_expirations() == 0) {
        return;
      }
      // output stats every second
      encoder.output_periodic_stats();
    }
  );

  // when the signal socket is readable
  poller.register_event(signal_sock, Poller::In, 
    [&]() 
    {
      while (true) {
        const auto & raw_data = signal_sock.recv();
        if (not raw_data) { // EWOULDBLOCK; try again when data is available
        cerr << "Unknown message type received on RTCP port." << endl;
          break;
        }
        const shared_ptr<Msg> sig_msg = Msg::parse_from_string(*raw_data);

        // handle the signal message
       if (sig_msg->type == Msg::Type::SIGNAL) {
          const auto signal = dynamic_pointer_cast<SignalMsg>(sig_msg);
          
          cerr << "Received signal: bitrate=" << signal->target_bitrate
               << endl;
          
          // update the encoder's configuration
    
          encoder.set_target_bitrate(signal->target_bitrate);
        }
        // ignore invalid messages
        return;
      }
    }
  );

  // main loop
  while (true) {
    poller.poll(-1);
  }

  return EXIT_SUCCESS;
}