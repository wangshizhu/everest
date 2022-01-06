#ifndef THREAD_MONITOR_DATA_H_
#define THREAD_MONITOR_DATA_H_

NAMESPACE_BEGIN

enum ThreadStateBitFlag
{
	// 是否启动
	kStartedPos = 0,

	// 是否停止
	kStoppedPos = 1,
};

enum ThreadState
{
	// 未启动
	kWaitingStart = 0,

	// 正在运行
	kRunning = 1,

	// 未启动但是已停止
	kHavntStartStopped = 2,

	// 已停止
	kStopped = 3,
};

class ThreadMonitorData
{
	static constexpr std::string_view thread_state_describe[] = { "wait_start","running","havntstart_stopped","stopped" };
	static constexpr std::size_t kThreadStateBitNum = sizeof(int8_t) * ONE_BYTE_TO_BIT;
public:
	// 线程id
	ThreadIdType thread_id_;

	// 线程全名
	std::string thread_full_name_ = "";

	// 排队任务数量
	std::size_t pending_num_ = 0;

	// 线程更新间隔时间
	std::chrono::steady_clock::duration interval_ = std::chrono::steady_clock::duration(0);

	// 线程时钟当前时间点
	Clock::SteadyTimePoint last_timepoint_;

	// 单次update最大执行时间
	std::size_t execute_once_max_time_ = 0;

	// 线程状态标记位
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