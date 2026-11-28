/**
 * @file multi_producer_multi_consumer_lock_free_test.cpp
 * @brief MultiProducerMultiConsumer<T> 无锁模式（定义 USE_LOCK_FREE_QUEUE）的测试入口。
 */

// 只包含共享用例头；USE_LOCK_FREE_QUEUE 由工程/CMake 的编译定义提供。
#include "multi_producer_multi_consumer_test_suite.h"
