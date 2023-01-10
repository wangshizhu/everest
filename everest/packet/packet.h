#ifndef PACKET_H_
#define PACKET_H_

NAMESPACE_EVEREST_BEGIN

/*
* abstract packet base class
*/
class PacketABC
{
public:
  using UniquePacketABC = std::unique_ptr<PacketABC>;
  using SharedPacketABC = std::shared_ptr<PacketABC>;

public:
  virtual ~PacketABC() = default;

public:
  virtual UniquePacketABC MakeUniquePacket() const = 0;

  virtual SharedPacketABC MakeSharedPacket() const = 0;

  virtual PacketIdType GetPacketId() const = 0;

  virtual std::size_t Size() const = 0;

  virtual void Serialize(BufferBase& buffer) const = 0;

  virtual void Unserialize(BufferBase& buffer) const = 0;

};

template<class BodyType, PacketIdType packet_id>
class Packet : public PacketABC
{
public:
  using UniquePacket = std::unique_ptr<Packet>;
  using SharedPacket = std::shared_ptr<Packet>;
  static constexpr PacketIdType kPacketId = packet_id;

public:
  UniquePacketABC MakeUniquePacket() const override
  {
    return MakeUnique();
  }

  SharedPacketABC MakeSharedPacket() const override
  {
    return MakeShared();
  }

  PacketIdType GetPacketId() const override
  {
    return kPacketId;
  }

  std::size_t Size() const override
  {
    return body_.ByteSize();
  }

  void Serialize(BufferBase& buffer) const override
  {
    buffer << body_;
  }

  void Unserialize(BufferBase& buffer) const override
  {
    buffer >> body_;
  }

public:
  static UniquePacket MakeUnique()
  {
    return std::make_unique<Packet>();
  }

  static SharedPacket MakeShared()
  {
    return std::make_shared<Packet>();
  }

public:
  BodyType& GetBody()
  {
    return body_;
  }

private:
  BodyType body_;
};

#define PACKET_TYPE_DEFINE(PACKET_NAME,PACKET_ID,BODY_TYPE) \
using PACKET_NAME = Packet<BODY_TYPE,PACKET_ID>;

NAMESPACE_EVEREST_END

#endif // !PACKET_H_

