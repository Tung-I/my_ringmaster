#ifndef PROTOCOL_HH
#define PROTOCOL_HH

#include <string>
#include <memory>
#include <utility> 

enum class FrameType : uint8_t { 
  UNKNOWN = 0, // unknown
  KEY = 1,     // key frame
  NONKEY = 2,  // non-key frame
};

// (frame_id, frag_id)
using SeqNum = std::pair<uint32_t, uint16_t>;


// Datagram on wire
struct Datagram
{
  Datagram() {}
  Datagram(const uint32_t _frame_id,
           const FrameType _frame_type,
           const uint16_t _frag_id,
           const uint16_t _frag_cnt, 
           const uint16_t _frame_width,
           const uint16_t _frame_height,
           const std::string_view _payload);

  uint32_t frame_id {};    
  FrameType frame_type {}; 
  uint16_t frag_id {};    
  uint16_t frag_cnt {};  

  uint16_t frame_width {};
  uint16_t frame_height {};  
  uint64_t send_ts {};

  std::string payload {};  

  // retransmission-related
  unsigned int num_rtx {0};  
  uint64_t last_send_ts {0};  

  // header size after serialization
  static constexpr size_t HEADER_SIZE = sizeof(uint32_t) +   // static: only one copy of HEADER_SIZE is shared by all instances of Datagram
      sizeof(FrameType) + 2 * sizeof(uint16_t) + sizeof(uint64_t);    // constexpr: HEADER_SIZE is a compile-time constant

  // maximum size for 'payload' (initialized in .cc and modified by set_mtu())
  static size_t max_payload;
  static void set_mtu(const size_t mtu);

  // construct this datagram by parsing binary string on wire
  bool parse_from_string(const std::string & binary);
  // serialize this datagram to binary string on wire
  std::string serialize_to_string() const;
};


// Base class for all messages
struct Msg
{
  enum class Type : uint8_t {
    INVALID = 0, 
    ACK = 1,     
    CONFIG = 2  
  };

  Type type {Type::INVALID};

  Msg() {}
  Msg(const Type _type) : type(_type) {} 
  virtual ~Msg() {} // q: what's this syntax? a: virtual destructor

  // factory method to make a (derived class of) Msg
  static std::shared_ptr<Msg> parse_from_string(const std::string & binary);

  // virtual functions for overriding
  virtual size_t serialized_size() const;
  virtual std::string serialize_to_string() const;
};

struct AckMsg : Msg
{
  AckMsg() : Msg(Type::ACK) {}
  AckMsg(const Datagram & datagram);

  uint32_t frame_id {}; 
  uint16_t frag_id {};  
  uint64_t send_ts {};  

  size_t serialized_size() const override; 
  std::string serialize_to_string() const override;
};

struct ConfigMsg : Msg
{
  ConfigMsg() : Msg(Type::CONFIG) {} 
  ConfigMsg(const uint16_t _width, const uint16_t _height,
            const uint16_t _frame_rate, const uint32_t _target_bitrate);  

  uint16_t width {};         
  uint16_t height {};         
  uint16_t frame_rate {};    
  uint32_t target_bitrate {}; 

  size_t serialized_size() const override;
  std::string serialize_to_string() const override;
};

#endif /* PROTOCOL_HH */
