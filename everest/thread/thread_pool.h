#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

NAMESPACE_EVEREST_BEGIN

template<class ThreadType>
class ThreadPool : private everest::NonCopyable
{
	static constexpr std::size_t kThreadPoolMinSize = 2;
	static constexpr std::size_t kThreadPoolMaxSize = 10;
public:
	ThreadPool(std::size_t pool_size);
	virtual ~ThreadPool();

public:
	virtual const char* Name()const;
	virtual bool Init();

public:
	void StartAll();

	void StopAll();

	void JoinAll();

	template<class T>
	void PostAll(T&& cb);

	template<class T>
	void PostAny(T&& cb);

	template<class T,class K>
	void PostByKey(T&& cb,K&& key);

	template<class T>
	void PostByIndex(T&& cb,std::size_t index);

	std::shared_ptr<ThreadType> GetAnyThread();

private:
	// TODO:增加参数列表，向线程对象传递构造函数参数
	void CreateAllThread();

	template<class K>
	std::size_t CalculateHashValue(K&& key) const;

	std::size_t GetThreadIndexByHashValue(std::size_t hash_value) const;

private:
	std::size_t pool_size_;
	std::vector<std::shared_ptr<ThreadType>> threads_;
};

NAMESPACE_EVEREST_END

#include "thread/src/thread_pool.ipp"

#endif
