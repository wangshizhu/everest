#ifndef BUFFER_H_
#define BUFFER_H_

NAMESPACE_EVEREST_BEGIN

class BufferBase
{
protected:
  static constexpr std::size_t kDefaultCapacity = 512;
  static constexpr std::size_t kExpandStep = 512;
  
public:
  BufferBase(int8_t* data,std::size_t size,std::size_t capacity = 0);

  virtual ~BufferBase();

public:
  std::size_t Size() const;

  std::size_t Capacity() const;

  std::size_t RemainSize() const;

  std::size_t CanWriteSize() const;

  bool Full() const;

  bool Empty() const;

  void Tidy();

  int8_t* GetWritePos();
  const int8_t* GetWritePos() const;
  void MoveWritePos(std::size_t size);

  int8_t* GetReadPos();
  const int8_t* GetReadPos() const;
  void MoveReadPos(std::size_t size);

  std::optional<std::size_t> WriteSome(const int8_t* from,std::size_t write_size);
  std::optional<std::size_t> ReadSome(int8_t* to,std::size_t read_size);

protected:
  void Reset(int8_t* data, std::size_t size, std::size_t capacity);

protected:
  static std::size_t FixCapacity(std::size_t capacity);

private:
  int8_t* data_;
  int8_t* write_;
  int8_t* read_;

protected:
  std::size_t capacity_;
};

class ExpandBuffer final : public BufferBase
{
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
  ExpandBuffer(std::size_t init_capacity = kDefaultCapacity,std::size_t max_capacity = kMaxCapacity);

public:
  /*
  * @brief:扩张式的写
  *
  * @param [in] from 外部数据的读取位置
  *
  * @param [in] write_size 从外部数据的读取位置读取多少
  *
  * @return bool true: 写成功 false: 写失败，只有当write_size超过了kMaxCapacity，才会写失败
  */
  bool ExpandWrite(const int8_t* from, std::size_t write_size);

  bool Expand(std::size_t expand_size = kExpandStep);

private:
  bool ExpandTo(std::size_t new_capacity);

private:
  std::unique_ptr<BufferHolder> data_;
  std::size_t max_capacity_;
};

using SharedExpandBuffer = std::shared_ptr<ExpandBuffer>;
using UniqueExpandBuffer = std::unique_ptr<ExpandBuffer>;

NAMESPACE_EVEREST_END

#endif // !BUFFER_H_

