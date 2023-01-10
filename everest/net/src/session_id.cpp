NAMESPACE_EVEREST_BEGIN

SessionId::SessionId(ServiceIdType service_id, uint32_t sequence_id)
  : service_id_(service_id),sequence_id_(sequence_id)
{
}

SessionId::SessionId(SessionIdType session_id)
  : SessionId(session_id >> kSeviceIdOffsetBit, session_id)
{
}

ServiceIdType SessionId::GetServiceId() const
{
  return service_id_;
}

SessionIdType SessionId::UnionSessionId() const
{
  std::bitset<sizeof(SessionIdType) * kOneByteBit> tmp;
  tmp |= service_id_;
  tmp <<= kSeviceIdOffsetBit;
  tmp |= sequence_id_;

  return tmp.to_ullong();
}

NAMESPACE_EVEREST_END