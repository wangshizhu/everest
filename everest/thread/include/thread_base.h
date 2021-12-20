#ifndef THREAD_BASE_H_
#define THREAD_BASE_H_

#include <asio.hpp>

NAMESPACE_BEGIN

class ThreadBase : public everest::NonCopyable
{
public:
	ThreadBase();
	virtual ~ThreadBase();

public:
	virtual const char* Name()const;
	virtual bool Init();
	virtual void Update();

public:
	void Start();

	void Join();

	void Stop();

	template<class T>
	void Post(T&& cb)
	{
		++pending_num_;
		context_.post([this,task = std::forward<T>(cb)]
			{
				--pending_num_;
				
				TRY_MACRO
				task();
				CATCH_MACRO
			});
	}

	template<class T>
	void Dispatch(T&& cb)
	{
		++pending_num_;
		context_.dispatch([this,task = std::forward<T>(cb)]
			{
				--pending_num_;
				
				TRY_MACRO
				task();
				CATCH_MACRO
			});
	}

	void SetThreadIndex(uint32_t index);

	template<class Duration = std::chrono::microseconds>
	void SetUpdateInterval(Duration&& d)
	{
		interval_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(d);
	}

	bool IsSameThread()const;

	bool IsStopped()const;

	asio::io_context& GetIoContext();

	std::size_t PendingNum();

	std::string FullName()const;

private:
	// ���Ӷ�ʱ�����������Ϊ���������߳�Update
	void AddTimerTaskToActuateUpdate();

private:
	// �߳�ִ�л���
	asio::io_context context_;

	// �̱߳��
	uint32_t index_;

	// �߳�Update��ʱ��
	asio::steady_timer run_timer_;

	// ���߳�ʱ��
	Clock clock_;

	// update���¼��ʱ��
	std::chrono::steady_clock::duration interval_;

	// δ������������
	std::size_t pending_num_;

	std::thread thread_;
};

NAMESPACE_END

#endif
