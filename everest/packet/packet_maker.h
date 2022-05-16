#ifndef PACKET_MAKER_H_
#define PACKET_MAKER_H_

NAMESPACE_EVEREST_BEGIN

class PacketMakerABC
{
public:
  using UniquePacketMakerABC = std::unique_ptr<PacketMakerABC>;
  using SharedPacketMakerABC = std::shared_ptr<PacketMakerABC>;

public:
  virtual ~PacketMakerABC() = default;

public:
  virtual PacketABC::UniquePacketABC MakeUniquePacket() const = 0;

  virtual PacketABC::SharedPacketABC MakeSharedPacket() const = 0;

  virtual packet_id_t GetPacketId() const = 0;
};

template<class PacketType>
class PacketMaker final : public PacketMakerABC
{
public:
  using UniquePacketMaker = std::unique_ptr<PacketMaker>;
  static constexpr packet_id_t kPacketId = PacketType::kPacketId;

public:
  PacketABC::UniquePacketABC MakeUniquePacket() const override
  {
    return PacketType::MakeUnique();
  }

  PacketABC::SharedPacketABC MakeSharedPacket() const override
  {
    return PacketType::MakeShared();
  }

  packet_id_t GetPacketId() const override
  {
    return kPacketId;
  }

public:
  static UniquePacketMaker MakeUnique()
  {
    return std::make_unique<PacketMaker>();
  }
};

class PacketMakerManager final : private everest::NonCopyable
{
public:
  void Register(PacketMakerABC::UniquePacketMakerABC maker)
  {
    maker_[maker->GetPacketId()] = std::move(maker);
  }

  PacketABC::UniquePacketABC MakeUniquePacket(packet_id_t packet_id) const
  {
    auto iter = maker_.find(packet_id);
    if (iter == maker_.cend())
    {
      return nullptr;
    }

    return iter->second->MakeUniquePacket();
  }

  PacketABC::SharedPacketABC MakeSharedPacket(packet_id_t packet_id) const
  {
    auto iter = maker_.find(packet_id);
    if (iter == maker_.cend())
    {
      return nullptr;
    }

    return iter->second->MakeSharedPacket();
  }

private:
  std::map<packet_id_t, PacketMakerABC::UniquePacketMakerABC> maker_;
};

EVEREST_INLINE everest::PacketMakerManager* PacketMakerMgrSingleton()
{
  return everest::ThreadSafeSingleton<everest::PacketMakerManager>::GetInstance();
}

template<class PacketType>
class PacketMakerRegister final
{
public:
  PacketMakerRegister()
  {
    PacketMakerMgrSingleton()->Register(PacketMaker<PacketType>::MakeUnique());
  }
};

#define PACKET_MAKER_REGISTER(PACKET_TYPE) \
static PacketMakerRegister<PACKET_TYPE> PACKET_TYPE##_maker_register;

NAMESPACE_EVEREST_END

#endif // !PACKET_MAKER_H_

