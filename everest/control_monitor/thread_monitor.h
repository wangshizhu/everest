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
	// �����̵߳��ϴμ������
	std::unordered_map<ThreadIdType, ThreadMonitorData> thread_monitor_data_;

	// �����̵߳�������
	std::vector<ThreadBaseWeakPtr> registed_thread_;
};

NAMESPACE_EVEREST_END

#endif