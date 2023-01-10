NAMESPACE_EVEREST_BEGIN

void operator << (BufferBase& buffer, const ::google::protobuf::Message& msg)
{
  auto&& buffer_stream = ::google::protobuf::io::ArrayOutputStream(buffer.GetWritePos(), buffer.RemainSize());
  msg.SerializePartialToZeroCopyStream(&buffer_stream);

  buffer.MoveWritePos(std::size_t(buffer_stream.ByteCount()));

  EVEREST_LOG_DEBUG("Packet Serialize to buffer {}",msg.ShortDebugString());
}

void operator >>(BufferBase& buffer, ::google::protobuf::Message& msg)
{
  if (!msg.ParseFromArray(buffer.GetReadPos(), buffer.Size()))
  {
    EVEREST_LOG_ERROR("Unserialize failed from buffer {},failed msg {}",
                      msg.GetTypeName(),msg.InitializationErrorString());
  }

  buffer.MoveReadPos(buffer.Size());

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