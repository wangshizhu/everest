#ifndef WORK_QUEUE_H_
#define WORK_QUEUE_H_

#include <mutex>
#include <condition_variable>
#include <queue>
#include "call_stack.h"

// Really simple Multiple producer / Multiple consumer work queue
class WorkQueue 
{
public:
	// Add a new work item
	template <typename F>
	void Push(F w) 
	{
		std::lock_guard<std::mutex> lock(mutex_);

		queue_.push(std::move(w));
		condition_variable_.notify_all();
	}

	// Continuously waits for and executes any work items, until "stop" is
	// called
	void Run() 
	{
		Callstack<WorkQueue>::Context ctx(this);
		while (true) 
		{
			std::function<void()> w;

			{
				std::unique_lock<std::mutex> lock(mutex_);

				condition_variable_.wait(lock, [this] { return !queue_.empty(); });

				w = std::move(queue_.front());

				queue_.pop();
			}

			if (w) 
			{
				w();
			}
			else 
			{
				// An empty work item means we are shutting down, so enqueue
				// another empty work item. This will in turn shut down another
				// thread that is executing "run"
				Push(nullptr);
				return;
			}
		};
	}

	// Causes any calls to "run" to exit.
	void Stop() 
	{
		Push(nullptr);
	}

	// Tells if "run" is executing in the current thread
	bool CanDispatch() 
	{
		return Callstack<WorkQueue>::Contains(this) != nullptr;
	}

private:
	std::condition_variable condition_variable_;
	std::mutex mutex_;
	std::queue<std::function<void()>> queue_;
};

#endif // !WORK_QUEUE_H_

