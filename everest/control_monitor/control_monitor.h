#ifndef CONTROL_MONITOR_H_
#define CONTROL_MONITOR_H_

NAMESPACE_EVEREST_BEGIN

class ControlMonitor : public everest::NonCopyable
{
public:
	ControlMonitor();

	~ControlMonitor();

public:
	void Update();

	void OnThreadStart();

	void RegisterThread(ThreadBaseSharedPtr thread_ptr);

	ThreadMonitor& GetThreadMonitor();

private:
	// 线程监控器
	ThreadMonitor thread_monitor_;

	// 执行监控器的线程
	ThreadBaseSharedPtr monitor_thread_;
};

#define CONTROL_MONITOR_SINGLETON() everest::ThreadSafeSingleton<everest::ControlMonitor>::GetInstance()

NAMESPACE_EVEREST_END

#endif
