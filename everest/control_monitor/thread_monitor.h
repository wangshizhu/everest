#ifndef THREAD_MONITOR_H_
#define THREAD_MONITOR_H_

NAMESPACE_BEGIN

class ThreadBase;
using ThreadBaseSharedPtr = std::shared_ptr<ThreadBase>;
using ThreadBaseWeakPtr = std::weak_ptr<ThreadBase>;

class ThreadMonitor : public everest::NonCopyable
{
	static constexpr uint32_t kSnapshotTimerDuration = 1;
	static constexpr std::size_t kMaxThreadNum = 100;

public:
	ThreadMonitor();

public:
	void Update();

	void OnThreadStart();

	void RegisterThread(ThreadBaseSharedPtr thread_ptr);

private:
	void PrintThreadMonitorData()const;

	void Snapshot();

private:
	// 所有线程的上次监控数据
	std::unordered_map<ThreadIdType, ThreadMonitorData> thread_monitor_data_;

	// 所有线程的弱引用
	std::vector<ThreadBaseWeakPtr> registed_thread_;

	// 收集所有线程的监控数据定时器
	everest::Timer<std::chrono::seconds> snapshot_timer_;

	// 执行监控器的线程
	ThreadBaseSharedPtr monitor_thread_;
};

NAMESPACE_END

#endif