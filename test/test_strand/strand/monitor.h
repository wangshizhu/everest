#ifndef MONITOR_H_
#define MONITOR_H_

#include <mutex>

template <class T>
class Monitor 
{
private:
	mutable T t_;
	mutable std::mutex mutex_;

public:
	using Type = T;
	Monitor() 
	{
	}

	Monitor(T t) : t_(std::move(t))
	{
	}

	template <typename F>
	auto operator()(F f) const
	{
		std::lock_guard<std::mutex> hold{ mutex_ };

		return f(t_);
	}
};

#endif // !MONITOR_H_

