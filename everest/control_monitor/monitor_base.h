#ifndef MONITOR_BASE_H_
#define MONITOR_BASE_H_

NAMESPACE_EVEREST_BEGIN

class ThreadBase;
using ThreadBaseSharedPtr = std::shared_ptr<ThreadBase>;
using ThreadBaseWeakPtr = std::weak_ptr<ThreadBase>;

template<class MonitorType>
class MonitorBase : private everest::NonCopyable
{
protected:
	// 每次快照时间间隔
	static constexpr uint32_t kSnapshotTimerDuration = 1;
	// 监控最大数量
	static constexpr std::size_t kMonitorObjectMaxNum = 100;

public:
	MonitorBase(uint32_t snapshot_timer_second_duration = kSnapshotTimerDuration);

public:
	void MonitorThreadStart();
	void SetMonitorThread(ThreadBaseSharedPtr monitor_thread);

public:
	virtual void Register(MonitorType monitor_object) = 0;

	virtual void Update() = 0;

protected:
	virtual void OnThreadStart() = 0;

protected:
	// 收集所有线程的监控数据定时器
	everest::Timer<std::chrono::seconds> snapshot_timer_;

	// 执行监控器的线程
	ThreadBaseSharedPtr monitor_thread_;
};

NAMESPACE_EVEREST_END

#include "control_monitor/src/monitor_base.ipp"

#endif // !MONITOR_BASE_H_

