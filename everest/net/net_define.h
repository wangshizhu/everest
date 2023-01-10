#ifndef NET_DEFINE_H_
#define NET_DEFINE_H_

NAMESPACE_EVEREST_BEGIN

enum class SessionType
{
	kNone = 0,
	// 外部连接
	kExternal = 1,

	// 内部连接
	kInternal = 2,
};

enum class SessionDirection
{
	kNone = 0,

	// 主动连接，通过connect创建的连接
	kPositive = 1,

	// 被动连接，通过accept创建的连接
	kNegative = 2,
};

using SessionIdType = uint64_t;

using everest_tcp = asio::ip::tcp;

using PacketIdType = uint32_t;
using PacketLengthType = uint32_t;

constexpr uint16_t kPacketIdByteLen = sizeof(PacketIdType);
constexpr uint16_t kPacketLengthByteLen = sizeof(PacketLengthType);

NAMESPACE_EVEREST_END

#endif // !NET_DEFINE_H_

