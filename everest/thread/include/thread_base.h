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
	virtual void Update();

public:
	void Start();

	void Join();

	void Stop();

	template<class T>
	void Post(T&& t);

	template<class T>
	void Dispatch(T&& t);

	void SetThreadIndex(uint32_t index);

	bool IsSameThread()const;

	bool IsStopped()const;

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

	// �Ƿ��Ѿ�����
	bool start_;

	std::thread thread_;
};

NAMESPACE_END

#endif
