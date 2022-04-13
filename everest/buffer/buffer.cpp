NAMESPACE_EVEREST_BEGIN

BufferBase::BufferBase()
{
  Reset(nullptr,0,0);
}

BufferBase::BufferBase(int8_t* data, std::size_t size, std::size_t capacity /* = 0 */)
{
  Reset(data,size,std::max(size,capacity));
}

BufferBase::~BufferBase()
{
}

std::size_t BufferBase::Size() const
{
  return write_ - read_;
}

std::size_t BufferBase::Capacity() const
{
  return capacity_;
}

std::size_t BufferBase::RemainSize() const
{
  return capacity_ - (write_ - data_);
}

bool BufferBase::Empty()const
{
  return Size() == 0;
}

void BufferBase::Tidy()
{
  if (data_ == read_)
  {
    return;
  }

  auto old_size = Size();
  std::copy(read_,write_,data_);

  read_ = data_;
  write_ = read_ + old_size;
}

void BufferBase::Reset(int8_t* data, std::size_t size, std::size_t capacity)
{
  data_ = data;
  read_ = data_;
  write_ = read_ + size;
  capacity_ = capacity;
}

std::optional<std::size_t> BufferBase::WriteSome(const int8_t* from, std::size_t write_size)
{
  if (write_size > RemainSize())
  {
    Tidy();
  }

  if (write_size > RemainSize())
  {
    return std::nullopt;
  }

  std::copy(from,from + write_size,write_);

  write_ += write_size;

  return write_size;
}

std::optional<std::size_t> BufferBase::ReadSome(int8_t* to, std::size_t read_size)
{
  if (read_size > Size())
  {
    return std::nullopt;
  }

  std::copy(read_,read_ + read_size,to);

  read_ += read_size;

  return read_size;
}

Buffer::BufferHolder::BufferHolder(std::size_t capacity)
{
  data_ = new int8_t[capacity];
}

Buffer::BufferHolder::~BufferHolder()
{
  delete[] data_;
}

Buffer::Buffer(std::size_t init_capacity /* = kDefaultCapacity */)
{
  // 默认容量必须大于扩展尺寸
  static_assert(kDefaultCapacity >= kExpandStep);

  auto fixed_capacity = FixCapacity(init_capacity);
  data_ = std::make_unique<BufferHolder>(fixed_capacity);
  Reset(data_->data_,0,fixed_capacity);
}

std::optional<std::size_t> Buffer::WriteSome(const int8_t* from, std::size_t write_size)
{
  auto&& writed_size = BufferBase::WriteSome(from, write_size);
  if (writed_size)
  {
    return writed_size;
  }

  Expand(Capacity() + (write_size - RemainSize()));

  return BufferBase::WriteSome(from, write_size);
}

std::size_t Buffer::FixCapacity(std::size_t capacity)
{
  if (capacity < kDefaultCapacity)
  {
    return kDefaultCapacity;
  }

  if (capacity > kMaxCapacity)
  {
    return kMaxCapacity;
  }

  auto fixed_capacity = (capacity / kExpandStep) * kExpandStep;
  if (fixed_capacity < capacity)
  {
    fixed_capacity += kExpandStep;
  }

  return fixed_capacity;
}

void Buffer::Expand(std::size_t new_capacity)
{
  auto fixed_capacity = FixCapacity(new_capacity);
  auto old_size = Size();
  auto&& new_buffer_holder = std::make_unique<BufferHolder>(fixed_capacity);
  
  ReadSome(new_buffer_holder->data_, old_size);

  Reset(new_buffer_holder->data_,old_size,fixed_capacity);

  data_.swap(new_buffer_holder);
}


NAMESPACE_EVEREST_END