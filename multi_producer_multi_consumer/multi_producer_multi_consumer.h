#ifndef MULTI_PRODUCER_MULTI_CONSUMER_H_
#define MULTI_PRODUCER_MULTI_CONSUMER_H_

#include "circular_queue.h"
#include <lock-free-queue/concurrentqueue.h>

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
#ifdef USE_LOCK_FREE_QUEUE  
			: queue_(max_items)
#else
			: q_(max_items)
#endif
		{}

#ifndef __MINGW32__
		// try to enqueue and block if no room left
		void enqueue(T&& item)
		{
#ifdef USE_LOCK_FREE_QUEUE

			queue_.enqueue(std::move(item));
#else
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				pop_cv_.wait(lock, [this] { return !this->q_.full(); });
				q_.push_back(std::move(item));
			}
			push_cv_.notify_one();
#endif // USE_LOCK_FREE_QUEUE
		}

		// enqueue immediately. overrun oldest message in the queue if no room left.
		void enqueue_nowait(T&& item)
		{
#ifdef USE_LOCK_FREE_QUEUE

			queue_.enqueue(std::move(item));
#else
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				q_.push_back(std::move(item));
			}
			push_cv_.notify_one();
#endif // USE_LOCK_FREE_QUEUE
		}

		// try to dequeue item. if no item found. wait upto timeout and try again
		// Return true, if succeeded dequeue item, false otherwise
		bool dequeue_for(T& popped_item, std::chrono::milliseconds wait_duration)
		{
#ifdef USE_LOCK_FREE_QUEUE
			while (true)
			{
				if (queue_.try_dequeue(popped_item))
				{
					break;
				}
				else
				{
					std::this_thread::yield();
				}
			}

			return true;
#else
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
#endif
		}

#else
		// apparently mingw deadlocks if the mutex is released before cv.notify_one(),
		// so release the mutex at the very end each function.

		// try to enqueue and block if no room left
		void enqueue(T&& item)
		{
#ifdef USE_LOCK_FREE_QUEUE

			queue_.enqueue(std::move(item));
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			pop_cv_.wait(lock, [this] { return !this->q_.full(); });
			q_.push_back(std::move(item));
			push_cv_.notify_one();
#endif // USE_LOCK_FREE_QUEUE
		}

		// enqueue immediately. overrun oldest message in the queue if no room left.
		void enqueue_nowait(T&& item)
		{
#ifdef USE_LOCK_FREE_QUEUE

			queue_.enqueue(std::move(item));
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			q_.push_back(std::move(item));
			push_cv_.notify_one();
#endif // USE_LOCK_FREE_QUEUE
		}

		// try to dequeue item. if no item found. wait upto timeout and try again
		// Return true, if succeeded dequeue item, false otherwise
		bool dequeue_for(T& popped_item, std::chrono::milliseconds wait_duration)
		{
#ifdef USE_LOCK_FREE_QUEUE
			while (true)
			{
				if (queue_.try_dequeue(popped_item))
				{
					break;
				}
				else
				{
					std::this_thread::yield();
				}
		}

			return true;
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			if (!push_cv_.wait_for(lock, wait_duration, [this] { return !this->q_.empty(); }))
			{
				return false;
			}
			popped_item = std::move(q_.front());
			q_.pop_front();
			pop_cv_.notify_one();
			return true;
#endif // USE_LOCK_FREE_QUEUE
		}

#endif

		size_t overrun_counter()
		{
#ifdef USE_LOCK_FREE_QUEUE
			return 0;
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			return q_.overrun_counter();
#endif // USE_LOCK_FREE_QUEUE
		}

		size_t size()
		{
#ifdef USE_LOCK_FREE_QUEUE
			return queue_.size_approx();
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			return q_.size();
#endif // USE_LOCK_FREE_QUEUE
		}

	private:
#ifdef USE_LOCK_FREE_QUEUE
		moodycamel::ConcurrentQueue<T> queue_;
#else
		std::mutex queue_mutex_;
		std::condition_variable push_cv_;
		std::condition_variable pop_cv_;
		everest::CircularQueue<T> q_;
#endif // USE_LOCK_FREE_QUEUE
	};
}
#endif
