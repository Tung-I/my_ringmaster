#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <chrono>

#include "conversion.hh"
#include "udp_socket.hh"
#include "sdl.hh"
#include "protocol.hh"
#include "sp_decoder.hh"

using namespace std;
using namespace chrono;

void print_usage(const string & program_name)
{
  cerr <<
  "Usage: " << program_name << " [options] host port width height\n\n"
  "Options:\n"
  "--fps <FPS>          frame rate to request from sender (default: 30)\n"
  "--cbr <bitrate>      request CBR from sender\n"
  "--lazy <level>       0: decode and display frames (default)\n"
  "                     1: decode but not display frames\n"
  "                     2: neither decode nor display frames\n"
  "-o, --output <file>  file to output performance results to\n"
  "-v, --verbose        enable more logging for debugging"
  "--streamtime         total streaming time in seconds\n"
  << endl;
}

int main(int argc, char * argv[])
{
  // argument parsing
  uint16_t frame_rate = 30;
  unsigned int target_bitrate = 0; // kbps
  int lazy_level = 0;
  string output_path;
  bool verbose = false;
  uint16_t total_stream_time = 0;

  const option cmd_line_opts[] = {
    {"fps",     required_argument, nullptr, 'F'},
    {"cbr",     required_argument, nullptr, 'C'},
    {"lazy",    required_argument, nullptr, 'L'},
    {"output",  required_argument, nullptr, 'o'},
    {"verbose", no_argument,       nullptr, 'v'},
    {"streamtime", required_argument, nullptr, 'T'},
    { nullptr,  0,                 nullptr,  0 },
  };

  while (true) {
    const int opt = getopt_long(argc, argv, "o:v", cmd_line_opts, nullptr);
    if (opt == -1) {
      break;
    }

    switch (opt) {
      case 'F':
        frame_rate = narrow_cast<uint16_t>(strict_stoi(optarg));
        break;
      case 'C':
        target_bitrate = strict_stoi(optarg);
        break;
      case 'L':
        lazy_level = strict_stoi(optarg);
        break;
      case 'o':
        output_path = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      case 'T':
        total_stream_time = narrow_cast<uint16_t>(strict_stoi(optarg));
        break;
      default:
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if (optind != argc - 4) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  const string host = argv[optind];
  const auto port = narrow_cast<uint16_t>(strict_stoi(argv[optind + 1]));
  const auto width = narrow_cast<uint16_t>(strict_stoi(argv[optind + 2]));
  const auto height = narrow_cast<uint16_t>(strict_stoi(argv[optind + 3]));

  // create a RTP socket and connect to the sender
  Address peer_addr{host, port};
  UDPSocket rtp_sock;
  rtp_sock.connect(peer_addr);
  cerr << "RTP connected:" << peer_addr.str() << ":" << rtp_sock.local_address().str() << endl;

  // create a RTCP socket and connect to the sender
  const auto rtcp_port = narrow_cast<uint16_t>(port + 1);
  UDPSocket rtcp_sock;
  Address rtcp_addr{host, rtcp_port};
  rtcp_sock.connect(rtcp_addr);
  cerr << "RTCP connected" << rtcp_addr.str() << ":" << rtcp_sock.local_address().str() << endl;

  // send an INIT_CONFIG message to the sender
  const ConfigMsg init_config_msg(width, height, frame_rate, target_bitrate); 
  rtp_sock.send(init_config_msg.serialize_to_string());
  cerr <<  "init_config_msg sent" << endl;
  const REMBMsg init_remb_msg(target_bitrate); 
  rtcp_sock.send(init_remb_msg.serialize_to_string());
  cerr <<  "init_remb_msg sent" << endl;
  

  // initialize decoders
  Decoder decoder(width, height, lazy_level, output_path);
  decoder.set_verbose(verbose);


  // timer
  unsigned int event_count = 0;
  unsigned int event_idx = 0;
  vector<unsigned int> bitrates = {8000, 5000, 2500, 1000};
  const auto start_time = steady_clock::now();
  auto last_time = steady_clock::now();

  // main loop
  while (true) {
    // parse a datagram received from sender
    Datagram datagram;
    if (not datagram.parse_from_string(rtp_sock.recv().value())) {
      throw runtime_error("failed to parse a datagram");
    }

    // send an ACK back to sender
    AckMsg ack(datagram);
    rtp_sock.send(ack.serialize_to_string());

    if (verbose) {
      cerr << "Acked datagram: frame_id=" << datagram.frame_id
           << " frag_id=" << datagram.frag_id << endl;
    }

    // process the received datagram in the decoder
    decoder.add_datagram(move(datagram));

    // check if the expected frame(s) is complete
    while (decoder.next_frame_complete()) {
      decoder.consume_next_frame();
    }

    // send a new REMB message every 5s
    if (steady_clock::now() - last_time > seconds(5)) {
      event_idx = event_count % bitrates.size();
      event_count++; 
      last_time = steady_clock::now();
      REMBMsg REMB_msg(bitrates[event_idx]);
      rtcp_sock.send(REMB_msg.serialize_to_string());
    }

    // Streaming time up
    if (steady_clock::now() - start_time > seconds(total_stream_time)) {
      cerr << "Time's up!" << endl;
      break;
    }
  }

  return EXIT_SUCCESS;
}