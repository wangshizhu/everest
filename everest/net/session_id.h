#ifndef SESSION_ID_H_
#define SESSION_ID_H_

NAMESPACE_EVEREST_BEGIN

class SessionId final
{
  static constexpr uint16_t kSeviceIdOffsetBit = 32;

public:
  SessionId(ServiceIdType service_id, uint32_t sequence_id);

  SessionId(SessionIdType session_id);

public:
  ServiceIdType GetServiceId() const;

  SessionIdType UnionSessionId() const;

private:
  ServiceIdType service_id_ = 0;
  uint32_t sequence_id_ = 0;
};

NAMESPACE_EVEREST_END

#endif // !SESSION_ID_H_

