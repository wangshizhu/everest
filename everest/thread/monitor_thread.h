#ifndef MONITOR_THREAD_H_
#define MONITOR_THREAD_H_

NAMESPACE_EVEREST_BEGIN

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

	// �߳�����
	virtual ThreadType GetThreadType() const;

	// ��ʼ���߳�
	virtual bool Init() override;

	// �̸߳���
	virtual void Update() override;

	// �߳��������¼�
	virtual void OnStart() override;
};

bool CreateAndStartMonitorThread();

NAMESPACE_EVEREST_END

#endif
