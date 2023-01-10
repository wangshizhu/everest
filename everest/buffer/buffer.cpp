NAMESPACE_EVEREST_BEGIN

BufferBase::BufferBase(int8_t* data, std::size_t size, std::size_t capacity /* = 0 */)
{
  Reset(data,size,FixCapacity(std::max(size, capacity)));
}

BufferBase::~BufferBase()
{
}

std::size_t BufferBase::Size() const
{
  return GetWritePos() - GetReadPos();
}

std::size_t BufferBase::Capacity() const
{
  return capacity_;
}

std::size_t BufferBase::RemainSize() const
{
  return Capacity() - Size();
}

std::size_t BufferBase::CanWriteSize() const
{
  return Capacity() - (GetWritePos() - data_);
}

bool BufferBase::Full() const
{
  return RemainSize() == 0;
}

bool BufferBase::Empty()const
{
  return Size() == 0;
}

void BufferBase::Tidy()
{
  if (data_ == GetReadPos())
  {
    return;
  }

  auto old_size = Size();
  std::copy(GetReadPos(),GetWritePos(),data_);

  read_ = data_;
  write_ = GetReadPos() + old_size;
}

int8_t* BufferBase::GetWritePos()
{
  return write_;
}

const int8_t* BufferBase::GetWritePos() const
{
  return write_;
}

void BufferBase::MoveWritePos(std::size_t size)
{
  write_ += size;

  Tidy();
}

int8_t* BufferBase::GetReadPos()
{
  return read_;
}

const int8_t* BufferBase::GetReadPos() const
{
  return read_;
}

void BufferBase::MoveReadPos(std::size_t size)
{
  read_ += size;

  Tidy();
}

void BufferBase::Reset(int8_t* data, std::size_t size, std::size_t capacity)
{
  data_ = data;
  read_ = data_;
  write_ = read_ + size;
  capacity_ = capacity;
}

std::size_t BufferBase::FixCapacity(std::size_t capacity)
{
  if (capacity < kDefaultCapacity)
  {
    return kDefaultCapacity;
  }

  auto fixed_capacity = (capacity / kExpandStep) * kExpandStep;
  if (fixed_capacity < capacity)
  {
    fixed_capacity += kExpandStep;
  }

  return fixed_capacity;
}

std::optional<std::size_t> BufferBase::WriteSome(const int8_t* from, std::size_t write_size)
{
  if (write_size > CanWriteSize())
  {
    Tidy();
  }

  if (write_size > CanWriteSize())
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

  Tidy();

  return read_size;
}

ExpandBuffer::BufferHolder::BufferHolder(std::size_t capacity)
{
  data_ = new int8_t[capacity];
}

ExpandBuffer::BufferHolder::~BufferHolder()
{
  delete[] data_;
}

ExpandBuffer::ExpandBuffer(std::size_t init_capacity /* = kDefaultCapacity */, 
                           std::size_t max_capacity/* = kMaxCapacity*/)
  :BufferBase(nullptr,0)
{
  auto fixed_init_capacity = FixCapacity(init_capacity);
  data_ = std::make_unique<BufferHolder>(fixed_init_capacity);

  Reset(data_->data_, 0, fixed_init_capacity);

  max_capacity_ = std::max(fixed_init_capacity,FixCapacity(max_capacity));
}

bool ExpandBuffer::ExpandWrite(const int8_t* from, std::size_t write_size)
{
  auto&& writed_size = BufferBase::WriteSome(from, write_size);
  if (writed_size)
  {
    return true;
  }

  if (!ExpandTo(Capacity() + (write_size - CanWriteSize())))
  {
    return false;
  }

  return bool(BufferBase::WriteSome(from, write_size));
}

bool ExpandBuffer::Expand(std::size_t expand_size /*= kExpandStep*/)
{
  return ExpandTo(Capacity() + expand_size);
}

bool ExpandBuffer::ExpandTo(std::size_t new_capacity)
{
  auto fixed_capacity = FixCapacity(new_capacity);
  if (fixed_capacity > max_capacity_)
  {
    return false;
  }

  auto old_size = Size();
  auto&& new_buffer_holder = std::make_unique<BufferHolder>(fixed_capacity);
  
  ReadSome(new_buffer_holder->data_, old_size);

  Reset(new_buffer_holder->data_,old_size,fixed_capacity);

  data_.swap(new_buffer_holder);

  capacity_ = fixed_capacity;

  return true;
}


NAMESPACE_EVEREST_END