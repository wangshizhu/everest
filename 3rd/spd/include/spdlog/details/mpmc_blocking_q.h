// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

// multi producer-multi consumer blocking queue.
// enqueue(..) - will block until room found to put the new message.
// enqueue_nowait(..) - will return immediately with false if no room left in
// the queue.
// dequeue_for(..) - will block until the queue is not empty or timeout have
// passed.

#include <spdlog/details/circular_q.h>
#ifdef USE_LOCK_FREE_QUEUE  
#include <lock-free-queue/concurrentqueue.h>
#endif

#include <condition_variable>
#include <mutex>

namespace spdlog {
namespace details {

template<typename T>
class mpmc_blocking_queue
{
public:
    using item_type = T;
    explicit mpmc_blocking_queue(size_t max_items)
#ifdef USE_LOCK_FREE_QUEUE  
        : queue_(max_items)
#else
        : q_(max_items)
#endif
    {}

#ifndef __MINGW32__
    // try to enqueue and block if no room left
    void enqueue(T &&item)
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
    void enqueue_nowait(T &&item)
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
    bool dequeue_for(T &popped_item, std::chrono::milliseconds wait_duration)
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
    void enqueue(T &&item)
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
    void enqueue_nowait(T &&item)
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
    bool dequeue_for(T &popped_item, std::chrono::milliseconds wait_duration)
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
#endif
    }

private:

#ifdef USE_LOCK_FREE_QUEUE
	moodycamel::ConcurrentQueue<T> queue_;
#else
    std::mutex queue_mutex_;
    std::condition_variable push_cv_;
    std::condition_variable pop_cv_;
    spdlog::details::circular_q<T> q_;
#endif // USE_LOCK_FREE_QUEUE
};
} // namespace details
} // namespace spdlog
