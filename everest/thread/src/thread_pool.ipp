
NAMESPACE_EVEREST_BEGIN

template<class ThreadType>
ThreadPool<ThreadType>::ThreadPool(std::size_t pool_size)
{
	pool_size_ = std::max(pool_size, kThreadPoolMinSize);
	pool_size_ = std::min(pool_size_, kThreadPoolMaxSize);

	threads_.reserve(pool_size_);

	EVEREST_LOG_INFO("ThreadPool name:{} ,pool_size_: {}, param_pool_size : {}", Name(),pool_size_, pool_size);
}

template<class ThreadType>
ThreadPool<ThreadType>::~ThreadPool()
{
}

template<class ThreadType>
const char* ThreadPool<ThreadType>::Name()const
{
	return "ThreadPool";
}

template<class ThreadType>
bool ThreadPool<ThreadType>::Init()
{
	CreateAllThread();

	return true;
}

template<class ThreadType>
void ThreadPool<ThreadType>::StartAll()
{
	for (auto&& one : threads_)
	{
		one->Start();
	}
}

template<class ThreadType>
void ThreadPool<ThreadType>::StopAll()
{
	for (auto&& one : threads_)
	{
		one->Stop();
	}
}

template<class ThreadType>
void ThreadPool<ThreadType>::JoinAll()
{
	for (auto&& one : threads_)
	{
		one->Join();
	}
}

template<class ThreadType>
template<class T>
void ThreadPool<ThreadType>::PostAll(T&& cb)
{
	for (auto&& one : threads_)
	{
		one->Post(cb);
	}
}

template<class ThreadType>
template<class T>
void ThreadPool<ThreadType>::PostAny(T&& cb)
{
	auto&& index = GetThreadIndexByHashValue(effolkronium::random_thread_local::get());

	PostByIndex(std::forward<T>(cb), index);
}

template<class ThreadType>
template<class T,class K>
void ThreadPool<ThreadType>::PostByKey(T&& cb,K&& k)
{
	auto&& index = GetThreadIndexByHashValue(CalculateHashValue(std::forward<K>(k)));

	PostByIndex(std::forward<T>(cb),index);
}

template<class ThreadType>
template<class T>
void ThreadPool<ThreadType>::PostByIndex(T&& cb, std::size_t index)
{
	if (index >= pool_size_)
	{
		EVEREST_LOG_ERROR("ThreadPool::PostByIndex index error,index:{},pool_size:{}",index,pool_size_);
		return;
	}

	threads_[index]->Post(std::forward<T>(cb));
}

template<class ThreadType>
void ThreadPool<ThreadType>::CreateAllThread()
{
	for (std::size_t i = 0; i < pool_size_; ++i)
	{
		auto tmp = ThreadBase::CreateThread<ThreadType>();
		tmp->SetThreadIndex(i);

		threads_.emplace_back(tmp);
	}
}

template<class ThreadType>
template<class K>
std::size_t ThreadPool<ThreadType>::CalculateHashValue(K&& key)const
{
	return std::hash<K>{}(std::forward<K>(key));
}

template<class ThreadType>
std::size_t ThreadPool<ThreadType>::GetThreadIndexByHashValue(std::size_t hash_value)const
{
	return hash_value % pool_size_;
}


NAMESPACE_EVEREST_END
