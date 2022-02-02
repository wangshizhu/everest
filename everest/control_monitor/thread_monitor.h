#ifndef THREAD_MONITOR_H_
#define THREAD_MONITOR_H_

NAMESPACE_EVEREST_BEGIN

class ThreadMonitor final : public everest::MonitorBase<ThreadBaseSharedPtr>
{
	using BaseType = everest::MonitorBase<ThreadBaseSharedPtr>;

public:
	ThreadMonitor();

public:
	void Register(ThreadBaseSharedPtr monitor_object) override;

	void Update() override;

	void OnThreadStart() override;

private:
	void PrintThreadMonitorData()const;

	void Snapshot();

private:
	// 所有线程的上次监控数据
	std::unordered_map<ThreadIdType, ThreadMonitorData> thread_monitor_data_;

	// 所有线程的弱引用
	std::vector<ThreadBaseWeakPtr> registed_thread_;
};

NAMESPACE_EVEREST_END

#endif