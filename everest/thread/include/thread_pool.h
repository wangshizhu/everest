#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

NAMESPACE_BEGIN

template<class ThreadType,
	typename std::enable_if_t<std::is_base_of_v<everest::ThreadBase,ThreadType>,int> = 0>
class ThreadPool : public everest::NonCopyable
{
public:
	ThreadPool(std::size_t pool_size);
	virtual ~ThreadPool();

public:
	virtual bool Init();

public:
	void StartAll();

	void StopAll();

	template<class T>
	void PostAll(T&& cb);

private:
	std::size_t pool_size_;
	std::vector<std::unique_ptr<ThreadType>> threads_;
};

NAMESPACE_END

#include "../src/thread_pool.ipp"

#endif
