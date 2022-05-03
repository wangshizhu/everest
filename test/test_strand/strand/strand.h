#ifndef STRAND_H_
#define STRAND_H_

#include "call_stack.h"
#include "monitor.h"
#include <assert.h>
#include <queue>
#include <functional>

//
// A strand serializes handler execution.
// It guarantees the following:
// - No handlers executes concurrently
// - Handlers are only executed from the specified Processor
// - Handler execution order is not guaranteed
//
// Specified Processor must implement the following interface:
//
//	template <typename F> void Processor::push(F w);
//		Add a new work item to the processor. F is a callable convertible
// to std::function<void()>
//
//	bool Processor::CanDispatch();
//		Should return true if we are in the Processor's dispatching function in
// the current thread.
//
template <typename Processor>
class Strand
{
public:
  Strand(Processor& processor) : processor_(processor)
  {
  }

  Strand(const Strand&) = delete;
  Strand& operator=(const Strand&) = delete;

  // Executes the handler immediately if all the strand guarantees are met,
  // or posts the handler for later execution if the guarantees are not met
  // from inside this call
  template <typename F>
  void Dispatch(F handler)
  {
    // If we are not currently in the processor dispatching function (in
    // this thread), then we cannot possibly execute the handler here, so
    // enqueue it and bail out
    if (!processor_.CanDispatch())
    {
      Post(std::move(handler));
      return;
    }

    // NOTE: At this point we know we are in a worker thread (because of the
    // check above)

    // If we are running the strand in this thread, then we can execute the
    // handler immediately without any other checks, since by design no
    // other threads can be running the strand
    if (RunningInThisThread())
    {
      handler();
      return;
    }

    // At this point we know we are in a worker thread, but not running the
    // strand in this thread.
    // The strand can still be running in another worker thread, so we need
    // to atomically enqueue the handler for the other thread to execute OR
    // mark the strand as running in this thread
    auto trigger = monitor_([&](Data& data)
    {
      if (data.running_)
      {
        data.queue_.push(std::move(handler));
        return false;
      }
      else
      {
        data.running_ = true;
        return true;
      }
    });

    if (trigger)
    {
      // Add a marker to the callstack, so the handler knows the strand is
      // running in the current thread
      typename Callstack<Strand>::Context ctx(this);
      handler();

      // Run any remaining handlers.
      // At this point we own the strand (It's marked as running in
      // this thread), and we don't release it until the queue is empty.
      // This means any other threads adding handlers to the strand will
      // enqueue them, and they will be executed here.
      Run();
    }
  }

  // Post an handler for execution and returns immediately.
  // The handler is never executed as part of this call.
  template <typename F>
  void Post(F handler)
  {
    // We atomically enqueue the handler AND check if we need to start the
    // running process.
    bool trigger = monitor_([&](Data& data)
    {

      data.queue_.push(std::move(handler));

      if (data.running_)
      {
        return false;
      }
      else
      {
        data.running_ = true;

        return true;
      }
    });

    // The strand was not running, so trigger a run
    if (trigger)
    {
      processor_.Push([this]
      {
        Run();
      });
    }
  }

  // Checks if we are currently running the strand in this thread
  bool RunningInThisThread()
  {
    return Callstack<Strand>::Contains(this) != nullptr;
  }

private:
  // Processes any enqueued handlers.
  // This assumes the strand is marked as running.
  // When there are no more handlers, it marks the strand as not running.
  void Run()
  {
    typename Callstack<Strand>::Context ctx(this);
    while (true)
    {
      std::function<void()> handler;
      monitor_([&](Data& data)
      {

        assert(data.running_);

        if (data.queue_.size())
        {
          handler = std::move(data.queue_.front());
          data.queue_.pop();
        }
        else
        {
          data.running_ = false;
        }
      });

      if (nullptr == handler)
      {
        return;
      }

      handler();
    }
  }

  struct Data
  {
    bool running_ = false;
    std::queue<std::function<void()>> queue_;
  };

  Monitor<Data> monitor_;
  Processor& processor_;
};

#endif // !STRAND_H_


