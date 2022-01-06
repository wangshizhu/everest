#ifndef THREAD_MONITOR_DATA_H_
#define THREAD_MONITOR_DATA_H_

NAMESPACE_BEGIN

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
	static constexpr std::string_view thread_state_describe[] = { "wait_start","running","havntstart_stopped","stopped" };
	static constexpr std::size_t kThreadStateBitNum = sizeof(int8_t) * ONE_BYTE_TO_BIT;
public:
	// �߳�id
	ThreadIdType thread_id_;

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
	std::string FormatForLog() const 
	{
		return fmt::format("thread_id:{},thread_full_name:{},pending_num:{},update_interval:{},execute_once_max_time: {},thread_state_flag: {}",
			thread_id_,
			thread_full_name_,
			pending_num_,
			NanosecondConvertSecond(interval_.count()),
			NanosecondConvertSecond(execute_once_max_time_),
			thread_state_describe[thread_state_flag_.to_ulong()]);
	}

private:
	std::string NanosecondConvertSecond(uint64_t nanosecond) const
	{
		auto second = TimeCapsule::DurationCountToDurationCount<std::chrono::seconds, std::chrono::steady_clock::duration>(nanosecond);
		auto mill_second = TimeCapsule::DurationCountToDurationCount<std::chrono::milliseconds, std::chrono::steady_clock::duration>(nanosecond - second * std::nano::den);
		auto micro_second = TimeCapsule::DurationCountToDurationCount<std::chrono::microseconds, std::chrono::steady_clock::duration>(nanosecond - second * std::nano::den - mill_second * std::micro::den);
		auto nano_second = nanosecond - second * std::nano::den - mill_second * std::micro::den - micro_second * std::milli::den;

		return fmt::format("{}s-{}ms-{}us-{}ns",
			second, mill_second, micro_second, nano_second);
	}

	const char* ThreadStateFlagToString()
	{
		auto val = thread_state_flag_.to_ulong();
		if (val == ThreadState::kWaitingStart)
		{
			return "wait_start";
		}
		else if (val == ThreadState::kRunning)
		{
			return "running";
		}
		else if (val == ThreadState::kStopped)
		{
			return "stopped";
		}
		else
		{
			return "unknow_state";
		}
	}
};

using SnapshotCb = std::function<void(ThreadMonitorData)>;

NAMESPACE_END

#endif