
NAMESPACE_BEGIN

template<class ThreadType>
ThreadPool<ThreadType>::ThreadPool(std::size_t pool_size)
{
	pool_size_ = std::max(pool_size,THREAD_POOL_MIN_SIZE);
	pool_size_ = std::min(pool_size_,THREAD_POOL_MAX_SIZE);

	threads_.reserve(pool_size_);
}

template<class ThreadType>
ThreadPool<ThreadType>::~ThreadPool()
{
}

template<class ThreadType>
bool ThreadPool<ThreadType>::Init()
{
	return true;
}


NAMESPACE_END
