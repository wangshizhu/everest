#ifndef THREAD_MONITOR_DATA_H_
#define THREAD_MONITOR_DATA_H_

NAMESPACE_EVEREST_BEGIN

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
	static constexpr std::string_view thread_state_describe[] = { "wait_start","running","no_start_stopped","stopped" };
	static constexpr std::size_t kThreadStateBitNum = sizeof(int8_t) * kOneByteBit;
public:
	// 线程id
	ThreadIdType thread_id_ = 0;

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
	std::string FormatForLog() const;

private:
	std::string NanosecondConvertSecond(uint64_t nanosecond) const;
};

using SnapshotCb = std::function<void(ThreadMonitorData)>;

NAMESPACE_EVEREST_END

#endif