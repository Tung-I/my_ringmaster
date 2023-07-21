#include <stdexcept>
#include "protocol.hh"
#include "serialization.hh"

using namespace std;


#include "protocol.hh"

// Initialize the static member
size_t BaseDatagram::max_payload = 0;

// BaseDatagram methods
BaseDatagram::BaseDatagram(const uint32_t _frame_id,
                           const FrameType _frame_type,
                           const uint16_t _frag_id,
                           const uint16_t _frag_cnt,
                           const std::string_view _payload)
    : frame_id(_frame_id),
      frame_type(_frame_type),
      frag_id(_frag_id),
      frag_cnt(_frag_cnt),
      payload(_payload.begin(), _payload.end()) // copy string_view to payload
{
}

// Inherited virtual methods would be implemented here...

void BaseDatagram::set_mtu(const size_t mtu) {
  max_payload = mtu - HEADER_SIZE;
}


// VideoDatagram methods
VideoDatagram::VideoDatagram(const uint32_t _frame_id,
                             const FrameType _frame_type,
                             const uint16_t _frag_id,
                             const uint16_t _frag_cnt, 
                             const uint16_t _frame_width,
                             const uint16_t _frame_height,
                             const std::string_view _payload)
    : BaseDatagram(_frame_id, _frame_type, _frag_id, _frag_cnt, _payload),
      frame_width(_frame_width),
      frame_height(_frame_height)
{
}

// Implementation of VideoDatagram's own methods...
// parse_from_string() and serialize_to_string()


// AudioDatagram methods
AudioDatagram::AudioDatagram(const uint32_t _frame_id,
                             const FrameType _frame_type,
                             const uint16_t _frag_id,
                             const uint16_t _frag_cnt,
                             const std::string_view _payload)
    : BaseDatagram(_frame_id, _frame_type, _frag_id, _frag_cnt, _payload)
{
}

// Implementation of AudioDatagram's own methods...
// parse_from_string() and serialize_to_string()


// Message
size_t Msg::serialized_size() const
{
  return sizeof(type);
}

string Msg::serialize_to_string() const
{
  return put_number(static_cast<uint8_t>(type));
}

shared_ptr<Msg> Msg::parse_from_string(const string & binary)
{
  if (binary.size() < sizeof(type)) {
    return nullptr;
  }

  WireParser parser(binary);
  auto type = static_cast<Type>(parser.read_uint8());

  if (type == Type::ACK) {
    auto ret = make_shared<AckMsg>();
    ret->frame_id = parser.read_uint32();
    ret->frag_id = parser.read_uint16();
    ret->send_ts = parser.read_uint64();
    return ret;
  }
  else if (type == Type::CONFIG) {
    auto ret = make_shared<ConfigMsg>();
    ret->width = parser.read_uint16();
    ret->height = parser.read_uint16();
    ret->frame_rate = parser.read_uint16();
    ret->target_bitrate = parser.read_uint32();
    return ret;
  }
  else if (type == Type::REMB) {
    auto ret = make_shared<REMBMsg>();
    ret->target_bitrate = parser.read_uint32();
    return ret;
  }
  else {
    return nullptr;
  }
}

AckMsg::AckMsg(const Datagram & datagram)
  : Msg(Type::ACK), frame_id(datagram.frame_id), frag_id(datagram.frag_id),
    send_ts(datagram.send_ts)
{}

size_t AckMsg::serialized_size() const
{
  return Msg::serialized_size() + sizeof(uint16_t) + sizeof(uint32_t)
         + sizeof(uint64_t);
}

string AckMsg::serialize_to_string() const
{
  string binary;
  binary.reserve(serialized_size());

  binary += Msg::serialize_to_string();
  binary += put_number(frame_id);
  binary += put_number(frag_id);
  binary += put_number(send_ts);

  return binary;
}

ConfigMsg::ConfigMsg(const uint16_t _width, const uint16_t _height,
                     const uint16_t _frame_rate, const uint32_t _target_bitrate)
  : Msg(Type::CONFIG), width(_width), height(_height),
    frame_rate(_frame_rate), target_bitrate(_target_bitrate)
{}

size_t ConfigMsg::serialized_size() const
{
  return Msg::serialized_size() + 3 * sizeof(uint16_t) + sizeof(uint32_t); 
}

string ConfigMsg::serialize_to_string() const
{
  string binary;
  binary.reserve(serialized_size());

  binary += Msg::serialize_to_string();
  binary += put_number(width);
  binary += put_number(height);
  binary += put_number(frame_rate);
  binary += put_number(target_bitrate);

  return binary;
}


REMBMsg::REMBMsg(const uint32_t _target_bitrate)
  : Msg(Type::REMB), target_bitrate(_target_bitrate)
{}

size_t REMBMsg::serialized_size() const
{
  return Msg::serialized_size() + sizeof(uint32_t); 
}

string REMBMsg::serialize_to_string() const
{
  string binary;
  binary.reserve(serialized_size());

  binary += Msg::serialize_to_string();
  binary += put_number(target_bitrate);

  return binary;
}
