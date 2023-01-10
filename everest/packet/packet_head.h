#ifndef PACKET_HEAD_H_
#define PACKET_HEAD_H_

NAMESPACE_EVEREST_BEGIN

#pragma pack(push,1)

// 定义了基本的消息包头格式
// 如果有需要可以继承此消息包头进行扩展，例如
// struct MyPacketHead : public PacketHeadBase
// {
//    uint_t version_ = 0;
//    bool is_compress_ = false;
// }
//
struct PacketHeadBase
{
  PacketIdType packet_id_ = 0;
  PacketLengthType packet_len_ = 0;
};

constexpr uint16_t kPacketHeadBaseByteLen = sizeof(PacketHeadBase);

#pragma pack(pop)

NAMESPACE_EVEREST_END

#endif // !PACKET_HEAD_H_

