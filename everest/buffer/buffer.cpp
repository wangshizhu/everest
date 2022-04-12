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


NAMESPACE_EVEREST_END