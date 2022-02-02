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

	// 注册线程
	void RegisterThread(ThreadBaseSharedPtr thread_ptr);

	// 注册服务
	void RegisterService(ServiceBaseSharedPtr service_ptr);

	ThreadMonitor& GetThreadMonitor();

	ServiceMonitor& GetServiceMonitor();

private:
	// 线程监控器
	ThreadMonitor thread_monitor_;

	// 服务监控器
	ServiceMonitor service_monitor_;

	// 执行监控器的线程
	ThreadBaseSharedPtr monitor_thread_;
};

extern ControlMonitor* g_control_monitor;

NAMESPACE_EVEREST_END

#endif
