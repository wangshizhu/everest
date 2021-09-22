/** ==========================================================================
* 2010 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================
*
* Example of a normal std::queue protected by a mutex for operations,
* making it safe for thread communication, using std::mutex from C++0x with
* the help from the std::thread library from JustSoftwareSolutions
* ref: http://www.stdthread.co.uk/doc/headers/mutex.html
*
* This example  was totally inspired by Anthony Williams lock-based data structures in
* Ref: "C++ Concurrency In Action" http://www.manning.com/williams */

#pragma once

#include <queue>
#include <mutex>
#include <exception>
#include <condition_variable>
#include "../concurrentqueue.h"

template<typename T>
class circular_q
{
	size_t max_items_ = 0;
	typename std::vector<T>::size_type head_ = 0;
	typename std::vector<T>::size_type tail_ = 0;
	size_t overrun_counter_ = 0;
	std::vector<T> v_;

public:
	using value_type = T;

	// empty ctor - create a disabled queue with no elements allocated at all
	circular_q() = default;

	explicit circular_q(size_t max_items)
		: max_items_(max_items + 1) // one item is reserved as marker for full q
		, v_(max_items_)
	{}

	circular_q(const circular_q&) = default;
	circular_q& operator=(const circular_q&) = default;

	// move cannot be default,
	// since we need to reset head_, tail_, etc to zero in the moved object
	circular_q(circular_q&& other) noexcept
	{
		copy_moveable(std::move(other));
	}

	circular_q& operator=(circular_q&& other) noexcept
	{
		copy_moveable(std::move(other));
		return *this;
	}

	// push back, overrun (oldest) item if no room left
	void push_back(T&& item)
	{
		if (max_items_ > 0)
		{
			v_[tail_] = std::move(item);
			tail_ = (tail_ + 1) % max_items_;

			if (tail_ == head_) // overrun last item if full
			{
				head_ = (head_ + 1) % max_items_;
				++overrun_counter_;
			}
		}
	}

	// Return reference to the front item.
	// If there are no elements in the container, the behavior is undefined.
	const T& front() const
	{
		return v_[head_];
	}

	T& front()
	{
		return v_[head_];
	}

	// Return number of elements actually stored
	size_t size() const
	{
		if (tail_ >= head_)
		{
			return tail_ - head_;
		}
		else
		{
			return max_items_ - (head_ - tail_);
		}
	}

	// Return const reference to item by index.
	// If index is out of range 0¡­size()-1, the behavior is undefined.
	const T& at(size_t i) const
	{
		assert(i < size());
		return v_[(head_ + i) % max_items_];
	}

	// Pop item from front.
	// If there are no elements in the container, the behavior is undefined.
	void pop_front()
	{
		head_ = (head_ + 1) % max_items_;
	}

	bool empty() const
	{
		return tail_ == head_;
	}

	bool full() const
	{
		// head is ahead of the tail by 1
		if (max_items_ > 0)
		{
			return ((tail_ + 1) % max_items_) == head_;
		}
		return false;
	}

	size_t overrun_counter() const
	{
		return overrun_counter_;
	}

private:
	// copy from other&& and reset it to disabled state
	void copy_moveable(circular_q&& other) noexcept
	{
		max_items_ = other.max_items_;
		head_ = other.head_;
		tail_ = other.tail_;
		overrun_counter_ = other.overrun_counter_;
		v_ = std::move(other.v_);

		// put &&other in disabled, but valid state
		other.max_items_ = 0;
		other.head_ = other.tail_ = 0;
		other.overrun_counter_ = 0;
	}
};

/** Multiple producer, multiple consumer thread safe queue
* Since 'return by reference' is used this queue won't throw */
template<typename T>
class shared_queue
{
private:
   shared_queue& operator=(const shared_queue&) = delete;
   shared_queue(const shared_queue& other) = delete;

public:
   shared_queue()
#ifdef USE_SAME_QUEUE_WITH_SPDLOG
	   :queue_(8192)
#endif
   {}

   void push(T item) 
   {
#ifdef USE_LOCK_FREE_QUEUE

       queue_.enqueue(std::move(item));
#else
#ifdef USE_SAME_QUEUE_WITH_SPDLOG
	   {
		   std::unique_lock<std::mutex> lock(m_);
		   pop_cv_.wait(lock, [this] { return !this->queue_.full(); });
		   queue_.push_back(std::move(item));
	   }
	   push_cv_.notify_one();
#else
	   {
		   std::lock_guard<std::mutex> lock(m_);
		   queue_.push(std::move(item));
	   }
	   data_cond_.notify_one();
	   
#endif

#endif // USE_LOCK_FREE_QUEUE
   }

#ifndef USE_LOCK_FREE_QUEUE
   /// \return immediately, with true if successful retrieval
   bool try_and_pop(T &popped_item) 
   {
      std::lock_guard<std::mutex> lock(m_);
      if (queue_.empty()) {
         return false;
      }
      popped_item = std::move(queue_.front());
      queue_.pop();
      return true;
   }
#endif // USE_LOCK_FREE_QUEUE

   /// Try to retrieve, if no items, wait till an item is available and try again
   void wait_and_pop(T &popped_item) 
   {
#ifdef USE_LOCK_FREE_QUEUE
	  while (true) {
		  if (queue_.try_dequeue(popped_item)) {
			  break;
		  }
		  else {
			  std::this_thread::yield();
		  }
	  }
#else

#ifdef USE_SAME_QUEUE_WITH_SPDLOG
	   
	  {
		  std::unique_lock<std::mutex> lock(m_);
		  push_cv_.wait(lock, [this] { return !this->queue_.empty(); });
		  popped_item = std::move(queue_.front());
		  queue_.pop_front();
	  }
	  pop_cv_.notify_one();
#else
	   std::unique_lock<std::mutex> lock(m_);
		while (queue_.empty())
		{
		    data_cond_.wait(lock);
		    //  This 'while' loop is equal to
		    //  data_cond_.wait(lock, [](bool result){return !queue_.empty();});
		}
		popped_item = std::move(queue_.front());
		queue_.pop();
#endif
#endif
   }

#ifndef USE_LOCK_FREE_QUEUE
   bool empty() const 
   {
      std::lock_guard<std::mutex> lock(m_);
      return queue_.empty();
   }

   unsigned size() const {
      std::lock_guard<std::mutex> lock(m_);
      return queue_.size();
   }
#endif // USE_LOCK_FREE_QUEUE

private:
#ifdef USE_LOCK_FREE_QUEUE
	moodycamel::ConcurrentQueue<T> queue_;
#else
	mutable std::mutex m_;
#ifdef USE_SAME_QUEUE_WITH_SPDLOG
	circular_q<T> queue_;
	std::condition_variable push_cv_;
	std::condition_variable pop_cv_;
#else
	std::queue<T> queue_;
	std::condition_variable data_cond_;
#endif

#endif // USE_LOCK_FREE_QUEUE
};
