#ifndef BUFFER_H_
#define BUFFER_H_

NAMESPACE_EVEREST_BEGIN

class BufferBase
{
public:
  BufferBase();

  BufferBase(int8_t* data,std::size_t size,std::size_t capacity = 0);

  virtual ~BufferBase();

public:
  std::size_t Size() const;

  std::size_t Capacity() const;

  std::size_t RemainSize() const;

  bool Empty() const;

  void Tidy();

  void Reset(int8_t* data, std::size_t size, std::size_t capacity);

  virtual std::optional<std::size_t> WriteSome(const int8_t* from,std::size_t write_size);
  std::optional<std::size_t> ReadSome(int8_t* to,std::size_t read_size);

private:
  int8_t* data_;
  int8_t* write_;
  int8_t* read_;
  std::size_t capacity_;
};

class Buffer final : public BufferBase
{
  static constexpr std::size_t kDefaultCapacity = 512;
  static constexpr std::size_t kExpandStep = 512;
  static constexpr std::size_t kMaxCapacity = 1024 * 1024 * 2;

private:
  class BufferHolder
  {
  public:
    BufferHolder(std::size_t capacity);
    ~BufferHolder();

  public:
    int8_t* data_;
  };

public:
  Buffer(std::size_t init_capacity = kDefaultCapacity);

public:
  std::optional<std::size_t> WriteSome(const int8_t* from, std::size_t write_size) override;

private:
  std::size_t FixCapacity(std::size_t capacity);

  void Expand(std::size_t new_capacity);

private:
  std::unique_ptr<BufferHolder> data_;
};

NAMESPACE_EVEREST_END

#endif // !BUFFER_H_

