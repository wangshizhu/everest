#include "../common/include.h"

NAMESPACE_BEGIN

thread_local Clock* g_tls_clock = nullptr;

Clock::Clock()
{
	start_time_point_ = std::chrono::steady_clock::now();
}

void Clock::TickTock()
{
	lastest_time_point_ = std::chrono::steady_clock::now();
}

void Clock::Restart()
{
	start_time_point_ = std::chrono::steady_clock::now();
}

Clock::SteadyTimePoint Clock::LatestTimePoint() const
{
	return lastest_time_point_;
}

void SetThreadLocalClock(Clock* clock)
{
	g_tls_clock = clock;
}

NAMESPACE_END
