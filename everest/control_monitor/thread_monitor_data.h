#ifndef THREAD_MONITOR_DATA_H_
#define THREAD_MONITOR_DATA_H_

NAMESPACE_EVEREST_BEGIN

enum ThreadStateBitFlag
{
	// �Ƿ�����
	kStartedPos = 0,

	// �Ƿ�ֹͣ
	kStoppedPos = 1,
};

enum ThreadState
{
	// δ����
	kWaitingStart = 0,

	// ��������
	kRunning = 1,

	// δ����������ֹͣ
	kHavntStartStopped = 2,

	// ��ֹͣ
	kStopped = 3,
};

class ThreadMonitorData
{
	static constexpr std::string_view thread_state_describe[] = { "wait_start","running","no_start_stopped","stopped" };
	static constexpr std::size_t kThreadStateBitNum = sizeof(int8_t) * kOneByteBit;
public:
	// �߳�id
	ThreadIdType thread_id_ = 0;

	// �߳�ȫ��
	std::string thread_full_name_ = "";

	// �Ŷ���������
	std::size_t pending_num_ = 0;

	// �̸߳��¼��ʱ��
	std::chrono::steady_clock::duration interval_ = std::chrono::steady_clock::duration(0);

	// �߳�ʱ�ӵ�ǰʱ���
	Clock::SteadyTimePoint last_timepoint_;

	// ����update���ִ��ʱ��
	std::size_t execute_once_max_time_ = 0;

	// �߳�״̬���λ
	std::bitset<kThreadStateBitNum> thread_state_flag_ = 0;

public:
	std::string FormatForLog() const;

private:
	std::string NanosecondConvertSecond(uint64_t nanosecond) const;
};

using SnapshotCb = std::function<void(ThreadMonitorData)>;

NAMESPACE_EVEREST_END

#endif