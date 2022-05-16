NAMESPACE_EVEREST_BEGIN

std::string GetCurrentThreadIdStr()
{
  std::stringstream ss;
  ss << std::this_thread::get_id();

  return ss.str();
}

uint64_t GetCurrentThreadId()
{
  static thread_local uint64_t current_thread_id = std::stoull(GetCurrentThreadIdStr());
  return current_thread_id;
}

NAMESPACE_EVEREST_END