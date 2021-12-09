#ifndef TIMER_IPP_
#define TIMER_IPP_

NAMESPACE_BEGIN

thread_local Timer<std::chrono::milliseconds> g_tls_ms_timer;

template<class Duration>
Timer<Duration>::Timer()
	:duration_(0),
	is_stop_(false),
	repeat_(false)
{
}

template<class Duration>
Timer<Duration>::Timer(uint32_t duration, bool repeat/* = false*/)
	:duration_(duration),
	is_stop_(true),
	repeat_(repeat)
{
}

template<class Duration>
void Timer<Duration>::Update()
{
	if (IsStop())
	{
		return;
	}

	last_tick_time_point_ = std::chrono::steady_clock::now();
}

template<class Duration>
bool Timer<Duration>::IsStop() const
{
	return is_stop_;
}

template<class Duration>
bool Timer<Duration>::Expired()
{
	if (IsStop())
	{
		return false;
	}

	auto&& diff = ElapseToDuration();

	bool expired = diff >= duration_;
	if (expired && repeat_)
	{
		start_time_point_ = last_tick_time_point_;
	}

	return expired;
}

template<class Duration>
void Timer<Duration>::Start()
{
	auto&& now = std::chrono::steady_clock::now();
	last_tick_time_point_ = now;
	start_time_point_ = now;
	is_stop_ = false;
}

template<class Duration>
void Timer<Duration>::Stop()
{
	is_stop_ = true;
}

template<class Duration>
uint32_t Timer<Duration>::Remain() const
{
	if (IsStop())
	{
		return 0;
	}

	auto&& elapse = ElapseToDuration();
	if (elapse > duration_)
	{
		return 0;
	}

	auto&& diff = duration_ - elapse;

	return diff.count();
}

template<class Duration>
uint32_t Timer<Duration>::Elapse() const
{
	return ElapseToDuration().count();
}

template<class Duration>
typename Timer<Duration>::SteadyTimePoint Timer<Duration>::LastTimePoint()const
{
	return last_tick_time_point_;
}

template<class Duration>
Duration Timer<Duration>::ElapseToDuration() const
{
	// TODO:last_tick_time_point_应该取线程局部变量Timer的last_tick_time_point_
	return std::chrono::floor<Duration>(g_tls_ms_timer.LastTimePoint() - start_time_point_);
}

NAMESPACE_END

#endif