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
#include "encoder.hh"
#include "timestamp.hh"

using namespace std;
using namespace chrono;

// Define global variables that are only visible to the current source file
namespace {
  constexpr unsigned int BILLION = 1000 * 1000 * 1000;
}

// Print usage of the program
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

// Receive a 'ConfigMsg' from a receiver
pair<Address, ConfigMsg> recv_config_msg(UDPSocket & udp_sock)
{
  // wait until a valid ConfigMsg is received
  while (true) {
    const auto & [peer_addr, raw_data] = udp_sock.recvfrom();

    const shared_ptr<Msg> msg = Msg::parse_from_string(raw_data.value());
    if (msg == nullptr or msg->type != Msg::Type::CONFIG) {
      continue; // ignore invalid or non-config messages
    }

    const auto config_msg = dynamic_pointer_cast<ConfigMsg>(msg);
    if (config_msg) {
      return {peer_addr, *config_msg};
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
        Datagram::set_mtu(strict_stoi(optarg));
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

  const auto port = narrow_cast<uint16_t>(strict_stoi(argv[optind]));
  // open another port for RTCP
  const auto rtcp_port = narrow_cast<uint16_t>(port + 1);

  // video file path 
  const string y4m_path = argv[optind + 1];

  // open the UDP socket
  UDPSocket udp_sock_rtp;
  udp_sock_rtp.bind({"0", port});
  cerr << "Local address: " << udp_sock_rtp.local_address().str() << endl;
  UDPSocket udp_sock_rtcp;
  udp_sock_rtcp.bind({"0", rtcp_port});
  cerr << "Local address: " << udp_sock_rtcp.local_address().str() << endl;

  // wait for a receiver to send 'ConfigMsg' and "connect" to it
  cerr << "Waiting for receiver..." << endl;
  const auto & [peer_addr, config_msg] = recv_config_msg(udp_sock_rtp); 
  cerr << "Peer address: " << peer_addr.str() << endl;
  udp_sock_rtp.connect(peer_addr);
  udp_sock_rtcp.connect(peer_addr);

  // read configuration from the peer
  const auto curr_width = config_msg.width;
  const auto curr_height = config_msg.height;
  const auto curr_frame_rate = config_msg.frame_rate;
  const auto curr_target_bitrate = config_msg.target_bitrate;

  cerr << "Received config: width=" << to_string(curr_width)
       << " height=" << to_string(curr_height)
       << " FPS=" << to_string(curr_frame_rate)
       << " bitrate=" << to_string(curr_target_bitrate) << endl;

  // set UDP socket to non-blocking now
  udp_sock_rtp.set_blocking(false);
  udp_sock_rtcp.set_blocking(false);

  // open the video files
  const string y4m_path_1080p = y4m_path.substr(0, y4m_path.size() - 4) + "_1080p.y4m";
  YUV4MPEG video_input_1080p(y4m_path_1080p, 1080, 1080);  
  const string y4m_path_720p = y4m_path.substr(0, y4m_path.size() - 4) + "_720p.y4m";
  YUV4MPEG video_input_720p(y4m_path_720p, 720, 720); 
  const string y4m_path_480p = y4m_path.substr(0, y4m_path.size() - 4) + "_480p.y4m";
  YUV4MPEG video_input_480p(y4m_path_480p, 480, 480); 
  const string y4m_path_360p = y4m_path.substr(0, y4m_path.size() - 4) + "_360p.y4m";
  YUV4MPEG video_input_360p(y4m_path_360p, 360, 360); 
  map<int, YUV4MPEG*> video_input_map = {
    {1080, &video_input_1080p},
    {720, &video_input_720p},
    {480, &video_input_480p},
    {360, &video_input_360p}
  };

  // allocate a raw image for storing the current frame
  RawImage raw_img_1080p(1080, 1080);
  RawImage raw_img_720p(720, 720);
  RawImage raw_img_480p(480, 480);
  RawImage raw_img_360p(360, 360);
  map<int, RawImage*> raw_img_map = {
    {1080, &raw_img_1080p},
    {720, &raw_img_720p},
    {480, &raw_img_480p},
    {360, &raw_img_360p}
  };

  // initialize the encoders
  Encoder encoder_1080p(1080, 1080, curr_frame_rate, output_path);
  encoder_1080p.set_target_bitrate(8000);
  encoder_1080p.set_verbose(verbose);
  Encoder encoder_720p(720, 720, curr_frame_rate, output_path);
  encoder_720p.set_target_bitrate(5000);
  encoder_720p.set_verbose(verbose);
  Encoder encoder_480p(480, 480, curr_frame_rate, output_path);
  encoder_480p.set_target_bitrate(2500);
  encoder_480p.set_verbose(verbose);
  Encoder encoder_360p(360, 360, curr_frame_rate, output_path);
  encoder_360p.set_target_bitrate(1000);
  encoder_360p.set_verbose(verbose);
  // map from resolution to encoder (use pointer to avoid copying)
  map<int, Encoder*> encoder_map = {
    {1080, &encoder_1080p},
    {720, &encoder_720p},
    {480, &encoder_480p},
    {360, &encoder_360p}
  };

  Poller poller;

  // create a periodic timer with the same period as the frame interval
  Timerfd fps_timer;
  const timespec frame_interval {0, static_cast<long>(BILLION / curr_frame_rate)};
  fps_timer.set_time(frame_interval, frame_interval);

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
        if (not video_input_1080p.read_frame(raw_img_1080p)) {
          throw runtime_error("Reached the end of video input");
        }
        if (not video_input_720p.read_frame(raw_img_720p)) {
          throw runtime_error("Reached the end of video input");
        }
        if (not video_input_480p.read_frame(raw_img_480p)) {
          throw runtime_error("Reached the end of video input");
        }
        if (not video_input_360p.read_frame(raw_img_360p)) {
          throw runtime_error("Reached the end of video input");
        }
      }

      // check the current config resolution and get the corresponding raw image and encoder (by reference)
      RawImage & raw_img = *raw_img_map[curr_width];
      Encoder & encoder = *encoder_map[curr_width];

      // compress 'raw_img' into frame 'frame_id' and packetize it
      encoder.compress_frame(raw_img);

      // interested in socket being writable if there are datagrams to send
      if (not encoder.send_buf().empty()) {
        poller.activate(udp_sock_rtp, Poller::Out);
      }
    }
  );

  // when UDP socket is writable
  poller.register_event(udp_sock_rtp, Poller::Out,
    [&]()
    {
      // iterate through the encoder_map and send the datagrams
      for (auto & kv : encoder_map) {
        deque<Datagram> & send_buf = kv.second->send_buf(); 

        while (not send_buf.empty()) {
          auto & datagram = send_buf.front();

          // timestamp the sending time before sending
          datagram.send_ts = timestamp_us();

          if (udp_sock_rtp.send(datagram.serialize_to_string())) {
            if (verbose) {
              cerr << "Sent datagram: frame_id=" << datagram.frame_id
                  << " frag_id=" << datagram.frag_id
                  << " frag_cnt=" << datagram.frag_cnt
                  << " rtx=" << datagram.num_rtx << endl;
            }

            // move the sent datagram to unacked if not a retransmission
            if (datagram.num_rtx == 0) {
              kv.second->add_unacked(move(datagram));
            }

            send_buf.pop_front();
          } else { // EWOULDBLOCK; try again later; datagram is not removed from the send_buf
            datagram.send_ts = 0; 
            break;
          }
        }

        // not interested in socket being writable if no datagrams to send
        if (send_buf.empty()) {
          poller.deactivate(udp_sock_rtp, Poller::Out);
        }
      }  
      // end of the event
    }
  );

  // when UDP socket is readable
  poller.register_event(udp_sock_rtp, Poller::In, 
    [&]() 
    {
      while (true) {
        const auto & raw_data = udp_sock_rtp.recv();

        if (not raw_data) { // EWOULDBLOCK; try again when data is available
          break;
        }
        const shared_ptr<Msg> msg = Msg::parse_from_string(*raw_data);

        // // handle the CONFIG message
        // if (msg->type == Msg::Type::CONFIG) {
        //   const auto config = dynamic_pointer_cast<ConfigMsg>(msg);

          
        //   cerr << "Received CONFIG: width=" << config->width
        //         << ", height=" << config->height
        //         << ", fps=" << config->frame_rate
        //         << ", br=" << config->target_bitrate << endl;
          

        //   // update the encoder's configuration
        //   encoder.set_target_bitrate(config->target_bitrate);
        //   encoder.set_resolution(config->width, config->height);
        //   return;
        // }

        // handle the ACK message
        if (msg->type == Msg::Type::ACK) {
          const auto ack = dynamic_pointer_cast<AckMsg>(msg); 

          if (verbose) {
            cerr << "Received ACK: frame_id=" << ack->frame_id
                << " frag_id=" << ack->frag_id << endl;
          }

          // RTT estimation, retransmission, etc.
          for (auto & kv : encoder_map) { 
            kv.second->handle_ack(ack);
            // send_buf might contain datagrams to be retransmitted now
            if (not kv.second->send_buf().empty()) {
              poller.activate(udp_sock_rtp, Poller::Out);
            }
          }

          return;
        }

        // ignore invalid messages
        return;
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
      for (auto & kv : encoder_map) {
        kv.second->output_periodic_stats();
      }
    }
  );

  // main loop
  while (true) {
    poller.poll(-1); 
  }

  return EXIT_SUCCESS;
}
