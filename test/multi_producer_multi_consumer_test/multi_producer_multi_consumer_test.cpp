/**
 * @file multi_producer_multi_consumer_test.cpp
 * @brief MultiProducerMultiConsumer<T> 默认模式（未定义 USE_LOCK_FREE_QUEUE）的测试入口。
 */

// 只包含共享用例头，确保被测组件头在 gtest 之前被包含，验证公共头自包含。
#include "multi_producer_multi_consumer_test_suite.h"
