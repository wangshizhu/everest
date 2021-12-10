#ifndef CLOCK_H_
#define CLOCK_H_

NAMESPACE_BEGIN

class Clock
{
public:
	using SteadyDuration = std::chrono::steady_clock::duration;
	using SteadyTimePoint = std::chrono::steady_clock::time_point;

public:
	Clock();

public:
	void TickTock();

	void Restart();

	SteadyTimePoint LatestTimePoint() const;

	template<class Duration>
	uint64_t Elapsed() const
	{
		return std::chrono::floor<Duration>(lastest_time_point_ - start_time_point_).count();
	}

	template<class Duration>
	uint64_t RealTimeElapsed() const 
	{
		return std::chrono::floor<Duration>(std::chrono::steady_clock::now() - start_time_point_).count();
	}

private:
	SteadyTimePoint start_time_point_;
	SteadyTimePoint lastest_time_point_;
};

void SetThreadLocalClock(Clock* clock);

NAMESPACE_END


#endif
