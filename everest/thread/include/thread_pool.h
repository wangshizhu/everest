#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

NAMESPACE_BEGIN

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
};

NAMESPACE_END

#endif
