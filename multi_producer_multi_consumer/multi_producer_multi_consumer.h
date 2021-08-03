#ifndef MULTI_PRODUCER_MULTI_CONSUMER_H_
#define MULTI_PRODUCER_MULTI_CONSUMER_H_

#include "circular_queue.h"

#include <condition_variable>
#include <mutex>

namespace everest
{
	template<typename T>
	class MultiProducerMultiConsumer
	{
	public:
		using item_type = T;
		explicit MultiProducerMultiConsumer(size_t max_items)
			: q_(max_items)
		{}

#ifndef __MINGW32__
		// try to enqueue and block if no room left
		void enqueue(T&& item)
		{
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				pop_cv_.wait(lock, [this] { return !this->q_.full(); });
				q_.push_back(std::move(item));
			}
			push_cv_.notify_one();
		}

		// enqueue immediately. overrun oldest message in the queue if no room left.
		void enqueue_nowait(T&& item)
		{
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				q_.push_back(std::move(item));
			}
			push_cv_.notify_one();
		}

		// try to dequeue item. if no item found. wait upto timeout and try again
		// Return true, if succeeded dequeue item, false otherwise
		bool dequeue_for(T& popped_item, std::chrono::milliseconds wait_duration)
		{
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				if (!push_cv_.wait_for(lock, wait_duration, [this] { return !this->q_.empty(); }))
				{
					return false;
				}
				popped_item = std::move(q_.front());
				q_.pop_front();
			}
			pop_cv_.notify_one();
			return true;
		}

#else
		// apparently mingw deadlocks if the mutex is released before cv.notify_one(),
		// so release the mutex at the very end each function.

		// try to enqueue and block if no room left
		void enqueue(T&& item)
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			pop_cv_.wait(lock, [this] { return !this->q_.full(); });
			q_.push_back(std::move(item));
			push_cv_.notify_one();
		}

		// enqueue immediately. overrun oldest message in the queue if no room left.
		void enqueue_nowait(T&& item)
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			q_.push_back(std::move(item));
			push_cv_.notify_one();
		}

		// try to dequeue item. if no item found. wait upto timeout and try again
		// Return true, if succeeded dequeue item, false otherwise
		bool dequeue_for(T& popped_item, std::chrono::milliseconds wait_duration)
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			if (!push_cv_.wait_for(lock, wait_duration, [this] { return !this->q_.empty(); }))
			{
				return false;
			}
			popped_item = std::move(q_.front());
			q_.pop_front();
			pop_cv_.notify_one();
			return true;
		}

#endif

		size_t overrun_counter()
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			return q_.overrun_counter();
		}

		size_t size()
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			return q_.size();
		}

	private:
		std::mutex queue_mutex_;
		std::condition_variable push_cv_;
		std::condition_variable pop_cv_;
		everest::CircularQueue<T> q_;
	};
}
#endif
