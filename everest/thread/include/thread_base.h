#ifndef THREAD_BASE_H_
#define THREAD_BASE_H_

#include <asio.hpp>

NAMESPACE_BEGIN

class ThreadBase : public everest::NonCopyable
{
public:
	ThreadBase(uint32_t index);
	virtual ~ThreadBase();

public:
	virtual const char* Name();
	virtual bool Init();

public:
	void Start();

	void Join();

	void Stop();

	void Update();

	template<class T>
	void Post(T&& t);

	template<class T>
	void Dispatch(T&& t);

private:
	// 线程执行环境
	asio::io_context context_;

	// 线程编号
	uint32_t index_;

	// 线程Update定时器
	asio::steady_timer run_timer_;

	// 本线程时钟
	Clock clock_;

	// 是否已经启动
	bool start_;

	std::thread thread_;
};

NAMESPACE_END

#endif
