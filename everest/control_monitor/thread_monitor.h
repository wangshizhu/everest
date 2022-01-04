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
	// �����̵߳��ϴμ������
	std::unordered_map<ThreadIdType, ThreadMonitorData> thread_monitor_data_;

	// �����̵߳�������
	std::vector<ThreadBaseWeakPtr> registed_thread_;

	// �ռ������̵߳ļ�����ݶ�ʱ��
	everest::Timer<std::chrono::seconds> snapshot_timer_;

	// ִ�м�������߳�
	ThreadBaseSharedPtr monitor_thread_;
};

NAMESPACE_END

#endif