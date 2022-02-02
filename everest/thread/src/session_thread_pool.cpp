NAMESPACE_EVEREST_BEGIN

SessionThreadPool::SessionThreadPool(std::size_t pool_size)
	: everest::ThreadPool<SessionThread>(pool_size)
{
}

const char* SessionThreadPool::Name()const
{
	return "SessionThreadPool";
}

bool SessionThreadPool::Init()
{
	return everest::ThreadPool<SessionThread>::Init();
}

NAMESPACE_EVEREST_END