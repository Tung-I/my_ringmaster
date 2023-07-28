#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#include "opus_wrapper.hh"

constexpr opus_int32 SAMPLE_RATE = 48000;  // adjust to your sample rate
constexpr int NUM_CHANNELS = 1;  // adjust to your channels
constexpr int APPLICATION = OPUS_APPLICATION_AUDIO;
constexpr int FRAME_SIZE = 960;  // adjust to your frame size

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " [input.raw]\n";
    return -1;
  }

  // Create encoder and decoder
  opus::Encoder encoder(SAMPLE_RATE, NUM_CHANNELS, APPLICATION, 0);
  opus::Decoder decoder(SAMPLE_RATE, NUM_CHANNELS);

  // Open input file
  std::ifstream inputFile(argv[1], std::ios::binary);
  std::vector<opus_int16> inputAudio((std::istreambuf_iterator<char>(inputFile)), 
                                     std::istreambuf_iterator<char>()); 
  

  // Encode
  auto startEncode = std::chrono::high_resolution_clock::now();
  auto encodedAudio = encoder.Encode(inputAudio, FRAME_SIZE); 
  auto stopEncode = std::chrono::high_resolution_clock::now();

  // Decode
  auto startDecode = std::chrono::high_resolution_clock::now();
  auto decodedAudio = decoder.Decode(encodedAudio, FRAME_SIZE, false);
  auto stopDecode = std::chrono::high_resolution_clock::now();

  // Report
  auto encodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(stopEncode - startEncode);
  auto decodeDuration = std::chrono::duration_cast<std::chrono::microseconds>(stopDecode - startDecode);

  std::cout << "Encoding time: " << encodeDuration.count() << " us\n";
  std::cout << "Decoding time: " << decodeDuration.count() << " us\n";

  return 0;
}
