#ifndef PACKET_HANDLER_H_
#define PACKET_HANDLER_H_

NAMESPACE_EVEREST_BEGIN

class PacketHandlerABC
{
public:
  virtual bool HandlePacket(PacketABC::SharedPacketABC pakcet, SessionSharedPtr session) const = 0;

  virtual packet_id_t GetPacketId() const = 0;
};

using UniquePacketHandlerABC = std::unique_ptr<PacketHandlerABC>;

template<class PacketType,class SessionType>
bool HandlePacket(typename PacketType::SharedPacket packet,std::shared_ptr<SessionType> session);

template<class PacketType,class SessionType>
class PacketHandler final : public PacketHandlerABC
{
public:
  bool HandlePacket(PacketABC::SharedPacketABC packet,SessionSharedPtr session) const override
  {
    auto concrete_packet = std::dynamic_pointer_cast<PacketType>(packet);
    auto concrete_session = std::dynamic_pointer_cast<SessionType>(session);
    return HandlePacket(concrete_packet,concrete_session);
  }

  packet_id_t GetPacketId() const override
  {
    return PacketType::kPacketId;
  }

public:
  static std::unique_ptr<PacketHandler> MakeUniquePacketHandler()
  {
    return std::make_unique<PacketHandler>();
  }
};

class PacketHandlerManager final : private everest::NonCopyable
{
public:
  void Register(UniquePacketHandlerABC handler)
  {
    handler_[handler->GetPacketId()] = std::move(handler);
  }

  bool HandlePacket(PacketABC::SharedPacketABC packet, SessionSharedPtr session) const
  {
    auto iter = handler_.find(packet->GetPacketId());
    if (iter == handler_.cend())
    {
      return false;
    }

    return iter->second->HandlePacket(packet,session);
  }

private:
  std::map<packet_id_t, UniquePacketHandlerABC> handler_;
};

#define PACKET_HANDLER_MGR_SINGLETON() everest::ThreadSafeSingleton<everest::PacketHandlerManager>::GetInstance()

template<class PacketType,class SessionType>
class PacketHandlerRegister
{
public:
  PacketHandlerRegister()
  {
    PACKET_HANDLER_MGR_SINGLETON()->Register(PacketHandler<PacketType,SessionType>::MakeUniquePacketHandler());
  }
};

#define PACKET_HANDLER_REGISTER(PACKET_TYPE,SESSION_TYPE) \
static PacketHandlerRegister<PACKET_TYPE,SESSION_TYPE> PACKET_TYPE##_handler_register;

#define PACKET_HANDLE(PACKET_TYPE,SESSION_TYPE,PACKET_VAR_NAME,SESSION_VAR_NAME) \
PACKET_HANDLER_REGISTER(PACKET_TYPE,SESSION_TYPE) \
PACKET_MAKER_REGISTER(PACKET_TYPE) \
template<> \
bool HandlePacket(PACKET_TYPE PACKET_VAR_NAME, SESSION_TYPE SESSION_VAR_NAME)

NAMESPACE_EVEREST_END

#endif // !PACKET_HANDLER_H_

