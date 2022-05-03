#ifndef TEST_STRAND_H_
#define TEST_STRAND_H_
#include <string>
#include <iostream>
#include "common/include.h"
#include "../strand/strand.h"
#include "../strand/work_queue.h"

class DataFootPrint
{
public:
  uint64_t trigger_thread_id_ = 0;
  uint64_t producer_thread_id_ = 0;
  std::size_t data_index_ = 0;
  uint64_t consumer_thread_id_ = 0;

public:
  void Logged()
  {
    LOG_INFO("trigger_thread_id:{},producer_thread_id:{},data_index:{},consumer_thread_id:{}",
             trigger_thread_id_,producer_thread_id_,data_index_,consumer_thread_id_);
  }
};

struct StrandClient
{
  explicit StrandClient(int n, WorkQueue& work_queue) : strand_(work_queue)
  {
    name_ = "StrandClient_" + std::to_string(n);
  }

  void DoSomething(int val)
  {
    printf("%s : doing %d\n", name_.c_str(), val);
  }

  std::string name_;
  Strand<WorkQueue> strand_;
};

class TestStrand
{
  enum
  {
    kProducerThreadProduceDataToSameStrandClient = 1,
    kConsumerThreadProduceDataToSameStrandClient = 2,
    kStop = 4,
  };

public:
  TestStrand(std::size_t consumer_thread_num,
             std::size_t producer_thread_num,
             std::size_t strand_client_num);

  ~TestStrand();

public:
  void MainThreadRun();

  void ProducerThreadRun(std::size_t thread_index);

private:
  void NotifyProducerThread(int32_t test_code);

  void ProducerThreadProduceDataToSameStrandClient(std::size_t thread_index);

  void ConsumerThreadProduceDataToSameStrandClient(std::size_t thread_index);

private:
  WorkQueue strand_queue_;

  std::vector<std::unique_ptr<StrandClient>> strand_client_;
  std::vector<std::thread> consumer_thread_;

  std::mutex producer_thread_mutex_;
  std::condition_variable producer_thread_condi_;
  int32_t test_code_;
  std::vector<std::thread> producer_thread_;
};


#endif // !TEST_STRAND_H_

