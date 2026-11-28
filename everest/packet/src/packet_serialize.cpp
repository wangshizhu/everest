#include <limits>

NAMESPACE_EVEREST_BEGIN

void operator << (Buffer& buffer, const ::google::protobuf::Message& msg)
{
  const std::size_t byte_size = static_cast<std::size_t>(msg.ByteSizeLong());
  if (byte_size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
  {
    EVEREST_LOG_ERROR("Packet Serialize size overflow {},type {}", byte_size, msg.GetTypeName());

    return;
  }

  if (!buffer.Reserve(byte_size))
  {
    EVEREST_LOG_ERROR("Packet Serialize no space, type {} size {} capacity {}", msg.GetTypeName(), byte_size, buffer.Capacity());

    return;
  }

  ::google::protobuf::io::ArrayOutputStream buffer_stream(buffer.GetWritePos(), static_cast<int>(byte_size));
  if (!msg.SerializePartialToZeroCopyStream(&buffer_stream))
  {
    EVEREST_LOG_ERROR("Packet Serialize failed, type {}", msg.GetTypeName());

    return;
  }

  if (!buffer.CommitWrite(static_cast<std::size_t>(buffer_stream.ByteCount())))
  {
    EVEREST_LOG_ERROR("Packet Serialize commit failed, type {} bytes {}", msg.GetTypeName(), buffer_stream.ByteCount());

    return;
  }

  EVEREST_LOG_DEBUG("Packet Serialize to buffer {}",msg.ShortDebugString());
}

void operator >>(Buffer& buffer, ::google::protobuf::Message& msg)
{
  const std::size_t byte_size = buffer.Size();
  if (byte_size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
  {
    EVEREST_LOG_ERROR("Packet Unserialize size overflow {},type {}", byte_size, msg.GetTypeName());

    if (!buffer.ConsumeRead(byte_size))
    {
      EVEREST_LOG_ERROR("Packet Unserialize consume read failed, type {} size {}", msg.GetTypeName(), byte_size);
    }

    return;
  }

  if (!msg.ParseFromArray(buffer.GetReadPos(), static_cast<int>(byte_size)))
  {
    EVEREST_LOG_ERROR("Unserialize failed from buffer {},failed msg {}",
                      msg.GetTypeName(),msg.InitializationErrorString());
  }

  if (!buffer.ConsumeRead(byte_size))
  {
    EVEREST_LOG_ERROR("Packet Unserialize consume read failed, type {} size {}", msg.GetTypeName(), byte_size);

    return;
  }

  EVEREST_LOG_DEBUG("Packet Unserialize from buffer {}", msg.ShortDebugString());
}

void operator << (std::string& to, const ::google::protobuf::Message& msg)
{
  to = msg.SerializeAsString();

  EVEREST_LOG_DEBUG("Packet Serialize to string {}", msg.ShortDebugString());
}

void operator >> (std::string& from, ::google::protobuf::Message& msg)
{
  if (!msg.ParseFromString(from))
  {
    EVEREST_LOG_ERROR("Unserialize failed from string {},failed msg {}", 
                      msg.GetTypeName(), msg.InitializationErrorString());
  }

  EVEREST_LOG_DEBUG("Packet Unserialize from string {}", msg.ShortDebugString());
}

NAMESPACE_EVEREST_END