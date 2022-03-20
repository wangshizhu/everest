#ifndef OBJECT_POOL_H_
#define OBJECT_POOL_H_

NAMESPACE_EVEREST_BEGIN

template<class T>
class ObjectPool final : private everest::NonCopyable
{
private:
	class Deleter
	{
		friend class ObjectPool;

	public:
		Deleter(ObjectPool* object_pool) :object_pool_(object_pool)
		{
		}

	public:
		void operator()(T* raw_ptr)
		{
			object_pool_->Release(raw_ptr);
		}

	private:
		ObjectPool* object_pool_;
	};

private:
	static constexpr uint32_t kDefaultExpandStep = 64;

public:
	ObjectPool(uint32_t expand_step = kDefaultExpandStep);

	~ObjectPool();

public:
	std::shared_ptr<T> ApplySharedObject();

	std::unique_ptr<T, Deleter> ApplyUniqueObject();

private:
	T* Apply();

	void Expand();

	void Shrink();

	void Release(T* raw_ptr);

private:
	std::mutex mutex_;

	// 开辟空间步长
	uint32_t expand_step_;

	// 对象池空间大小
	uint32_t space_;

	// 被使用的对象集合
	std::set<T*> used_;

	// 未被使用的对象列表
	std::list<T*> free_;
};

NAMESPACE_EVEREST_END

#include "object_pool/src/object_pool.ipp"

#endif // !OBJECT_POOL_H_

