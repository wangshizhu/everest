#include "test_strand.h"

TestStrand::TestStrand(std::size_t consumer_thread_num,
                       std::size_t producer_thread_num,
                       std::size_t strand_client_num)
  :test_code_(0)
{
  for (auto i = 0;i<consumer_thread_num;++i)
  {
    consumer_thread_.push_back(std::thread([this]
    {
      LOG_INFO("create consumer thread {}", everest::GetCurrentThreadIdUint64());
      strand_queue_.Run();
    }));
  }

  for (auto i = 0;i<producer_thread_num;++i)
  {
    producer_thread_.push_back(std::thread([this,thread_index = i]
    {
      ProducerThreadRun(thread_index);
    }));
  }

  for (auto i = 0;i<strand_client_num;++i)
  {
    strand_client_.push_back(std::make_unique<StrandClient>(i,strand_queue_));
  }
}

TestStrand::~TestStrand()
{
  strand_queue_.Stop();

  for (auto&& t : producer_thread_)
  {
    t.join();
  }

  for (auto&& t : consumer_thread_)
  {
    t.join();
  }
}

void TestStrand::MainThreadRun()
{
  int32_t test_code = 0;
  while (std::cin >> test_code)
  {
    NotifyProducerThread(test_code);
    if (test_code == kStop)
    {
      return;
    }
  }
}

void TestStrand::ProducerThreadRun(std::size_t thread_index)
{
  LOG_INFO("create producer thread {}",everest::GetCurrentThreadIdUint64());
  thread_local bool onoff = true;
  while (true)
  {
    {
      std::unique_lock<std::mutex> lock(producer_thread_mutex_);
      producer_thread_condi_.wait(lock, [this]
      {
        onoff = !onoff;
        return test_code_ != 0 && onoff;
      });
    }

    if (test_code_ == kProducerThreadProduceDataToSameStrandClient)
    {
      ProducerThreadProduceDataToSameStrandClient(thread_index);
    }
    else if (test_code_ == kConsumerThreadProduceDataToSameStrandClient)
    {
      ConsumerThreadProduceDataToSameStrandClient(thread_index);
    }
    else if (test_code_ == kStop)
    {
      return;
    }
  }
}

void TestStrand::NotifyProducerThread(int32_t test_code)
{
  test_code_ = test_code;
  producer_thread_condi_.notify_all();
}

void TestStrand::ProducerThreadProduceDataToSameStrandClient(std::size_t thread_index)
{
  auto&& strand_client = strand_client_.front();

  for (auto i = 0;i<10;++i)
  {
    DataFootPrint data;
    data.trigger_thread_id_ = everest::GetCurrentThreadIdUint64();
    data.producer_thread_id_ = everest::GetCurrentThreadIdUint64();
    data.data_index_ = i;

    strand_client->strand_.Dispatch([data = data]() mutable
    {
      data.consumer_thread_id_ = everest::GetCurrentThreadIdUint64();
      data.Logged();
    });
  }
}

void TestStrand::ConsumerThreadProduceDataToSameStrandClient(std::size_t thread_index)
{
  auto&& strand_client = strand_client_.front();

  for (auto i = 0;i<10;++i)
  {
    DataFootPrint data;
    data.trigger_thread_id_ = everest::GetCurrentThreadIdUint64();
    data.data_index_ = i;

    strand_queue_.Push([data = data, &strand_client]() mutable
    {
      data.producer_thread_id_ = everest::GetCurrentThreadIdUint64();

      strand_client->strand_.Dispatch([data = data] ()mutable
      {
        data.consumer_thread_id_ = everest::GetCurrentThreadIdUint64();
        data.Logged();
      });
    });
  }
}