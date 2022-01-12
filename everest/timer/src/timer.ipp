#ifndef TIMER_IPP_
#define TIMER_IPP_

NAMESPACE_EVEREST_BEGIN

extern thread_local Clock* g_tls_clock;

template<class Duration>
Timer<Duration>::Timer()
	:duration_(0),
	is_stop_(true),
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
Timer<Duration>::Timer(const Timer& t):Timer(t.duration_,t.repeat_)
{
}

template<class Duration>
Timer<Duration>& Timer<Duration>::operator=(const Timer& t)
{
	Timer(t.duration_, t.repeat_);
	return *this;
}

template<class Duration>
Timer<Duration>::Timer(Timer&& t) : Timer(t.duration_, t.repeat_)
{
}

template<class Duration>
Timer<Duration>& Timer<Duration>::operator=(Timer&& t)
{
	Timer(t.duration_, t.repeat_);
	return *this;
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
		start_time_point_ = LastTimePoint();
	}

	return expired;
}

template<class Duration>
void Timer<Duration>::Start()
{
	start_time_point_ = LastTimePoint();
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
	if (g_tls_clock != nullptr)
	{
		return g_tls_clock->LatestTimePoint();
	}

	return std::chrono::steady_clock::now();
}

template<class Duration>
Duration Timer<Duration>::ElapseToDuration() const
{
	return std::chrono::floor<Duration>(LastTimePoint() - start_time_point_);
}

NAMESPACE_EVEREST_END

#endif