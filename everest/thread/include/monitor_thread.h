#ifndef MONITOR_THREAD_H_
#define MONITOR_THREAD_H_

NAMESPACE_BEGIN

class MonitorThread final : public ThreadBase
{
public:
	template<typename... Args>
	MonitorThread(Args&&... args) :ThreadBase(std::forward<Args>(args)...)
	{
	}

public:
	// �߳�����
	virtual const char* Name()const override;

	// ��ʼ���߳�
	virtual bool Init() override;

	// �̸߳���
	virtual void Update() override;

	// �߳��������¼�
	virtual void OnStart() override;
};

NAMESPACE_END

#endif
