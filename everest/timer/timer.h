#ifndef TIMER_H_
#define TIMER_H_

NAMESPACE_BEGIN

template<class Duration = std::chrono::milliseconds>
class Timer
{
public:
	using SteadyDuration = std::chrono::steady_clock::duration;
	using SteadyTimePoint = std::chrono::steady_clock::time_point;

public:
	Timer();
	explicit Timer(uint32_t duration,bool repeat = false);

	Timer(const Timer& t);
	Timer& operator = (const Timer& t);

	Timer(Timer&& t);
	Timer& operator = (Timer&& t);

public:
	bool IsStop()const;

	bool Expired();

	void Start();

	void Stop();

	uint32_t Remain() const;

	uint32_t Elapse() const;

private:
	SteadyTimePoint LastTimePoint() const;

	Duration ElapseToDuration() const;

private:
	// 计时器时长
	Duration duration_;

	// 是否停止的
	bool is_stop_;

	// 是否重复
	bool repeat_;

	// 开始计时时间点
	SteadyTimePoint start_time_point_;
};

NAMESPACE_END

#include "timer/src/timer.ipp"

#endif // !TIMER_H_
