#ifndef CONTROL_MONITOR_H_
#define CONTROL_MONITOR_H_

NAMESPACE_EVEREST_BEGIN

class ControlMonitor final : public everest::NonCopyable
{
public:
	ControlMonitor();

	~ControlMonitor();

public:
	void Update();

	void OnThreadStart();

	// ע���߳�
	void RegisterThread(ThreadBaseSharedPtr thread_ptr);

	// ע�����
	void RegisterService(ServiceBaseSharedPtr service_ptr);

	ThreadMonitor& GetThreadMonitor();

	ServiceMonitor& GetServiceMonitor();

private:
	// �̼߳����
	ThreadMonitor thread_monitor_;

	// ��������
	ServiceMonitor service_monitor_;

	// ִ�м�������߳�
	ThreadBaseSharedPtr monitor_thread_;
};

extern ControlMonitor* g_control_monitor;

NAMESPACE_EVEREST_END

#endif
