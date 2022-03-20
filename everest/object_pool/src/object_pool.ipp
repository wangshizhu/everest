#ifndef OBJECT_POOL_IPP_
#define OBJECT_POOL_IPP_

NAMESPACE_EVEREST_BEGIN

template<class T>
ObjectPool<T>::ObjectPool(uint32_t expand_step /*= kDefaultExpandStep*/)
	:expand_step_(expand_step)
{
}

template<class T>
ObjectPool<T>::~ObjectPool()
{
	for (auto one : used_)
	{
		delete one;
	}
	used_.clear();

	for (auto one : free_)
	{
		delete one;
	}
	free_.clear();
}

template<class T>
std::shared_ptr<T> ObjectPool<T>::ApplySharedObject()
{
	return std::shared_ptr<T>(Apply(), Deleter(this));
}

template<class T>
std::unique_ptr<T, typename ObjectPool<T>::Deleter> ObjectPool<T>::ApplyUniqueObject()
{
	return std::unique_ptr<T,Deleter>(Apply(), Deleter(this));
}

template<class T>
T* ObjectPool<T>::Apply()
{
	T* raw_ptr = nullptr;

	{
		std::lock_guard<std::mutex> guard(mutex_);
		if (free_.empty())
		{
			Expand();
		}

		raw_ptr = free_.front();
		free_.pop_front();
		used_.insert(raw_ptr);
	}

	raw_ptr->Clear();

	return raw_ptr;
}

template<class T>
void ObjectPool<T>::Expand()
{
	for (auto i = 0; i < expand_step_; ++i)
	{
		free_.push_back(new T());
		++space_;
	}
}

template<class T>
void ObjectPool<T>::Shrink()
{
	std::lock_guard<std::mutex> guard(mutex_);

	// 未使用列表里保证至少有默认数量kDefaultExpandStep的对象
	if (free_.size() <= kDefaultExpandStep)
	{
		return;
	}

	for (auto i = 0; i < kDefaultExpandStep; ++i)
	{
		auto raw_ptr = free_.front();
		free_.pop_front();
		delete raw_ptr;
	}
}

template<class T>
void ObjectPool<T>::Release(T* raw_ptr)
{
	std::lock_guard<std::mutex> guard(mutex_);
	used_.erase(raw_ptr);
	free_.push_back(raw_ptr);
}

NAMESPACE_EVEREST_END

#endif // !OBJECT_POOL_IPP_