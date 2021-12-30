#ifndef CONTROL_MONITOR_H_
#define CONTROL_MONITOR_H_

NAMESPACE_BEGIN

class ControlMonitor : public everest::NonCopyable
{
public:
	ControlMonitor();

public:
	void Update();

	void OnThreadStart();

	ThreadMonitor& GetThreadMonitor();

private:
	// Ïß³Ì¼à¿ØÆ÷
	ThreadMonitor thread_monitor_;
};

#define CONTROL_MONITOR_SINGLETON everest::ThreadSafeSingleton<everest::ControlMonitor>::GetInstance()

NAMESPACE_END

#endif
