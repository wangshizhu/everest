/**
 * @file multi_producer_multi_consumer_test_suite.h
 * @brief MultiProducerMultiConsumer<T> 两种编译模式共享的 GoogleTest 用例。
 *
 * 本头文件被默认模式与无锁模式两个测试入口包含；模式专属用例用
 * USE_LOCK_FREE_QUEUE 区分。单独包含本头文件即在 gtest 之前包含被测组件头，
 * 以验证公共头自包含。
 */

#ifndef MULTI_PRODUCER_MULTI_CONSUMER_TEST_SUITE_H_
#define MULTI_PRODUCER_MULTI_CONSUMER_TEST_SUITE_H_

#include "multi_producer_multi_consumer/multi_producer_multi_consumer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

// 被测类型位于扁平命名空间 everest。
using everest::MultiProducerMultiConsumer;

/**
 * @brief 移动构造与移动赋值各耗时约 10ms 的测试类型。
 *
 * 用于验证默认模式与无锁模式都不依赖 T 移动操作的速度完成出队契约。
 * 无锁模式的入队使用移动构造，出队使用移动赋值，两种模式都需要两个移动操作。
 */
class SlowMoveValue final {
public:
    // 携带的测试值。
    int value = 0;

    /**
     * @brief 默认构造为 0。
     */
    SlowMoveValue() = default;

    /**
     * @brief 移动构造：休眠约 10ms 后复制 value。
     * @param other 源对象。
     */
    SlowMoveValue(SlowMoveValue&& other) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        value = other.value;
    }

    /**
     * @brief 移动赋值：休眠约 10ms 后复制 value。
     * @param other 源对象。
     * @return 自身引用。
     */
    SlowMoveValue& operator=(SlowMoveValue&& other) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        value = other.value;
        return *this;
    }
};

/**
 * @brief 移动构造不抛异常、移动赋值可被置位为抛异常的测试类型。
 *
 * 入队走移动构造，出队走移动赋值；仅验证后者的异常契约。
 * 仅用于默认模式；异常开关是进程级状态，测试用例负责置位与复位。
 */
class ThrowingMoveValue final {
public:
    // 携带的测试值。
    int value = 0;

    /**
     * @brief 默认构造为 0。
     */
    ThrowingMoveValue() = default;

    /**
     * @brief 移动构造：复制 value，不抛异常。
     * @param other 源对象。
     */
    ThrowingMoveValue(ThrowingMoveValue&& other) noexcept {
        value = other.value;
    }

    /**
     * @brief 移动赋值；开关置位时抛出 std::runtime_error。
     * @param other 源对象。
     * @return 自身引用。
     * @throw std::runtime_error 开关置位时抛出。
     */
    ThrowingMoveValue& operator=(ThrowingMoveValue&& other) {
        if (ThrowEnabled()) {
            throw std::runtime_error("throwing move assignment");
        }

        value = other.value;
        return *this;
    }

    /**
     * @brief 获取移动赋值抛异常开关。
     * @return 开关的引用；测试用例负责置位与复位。
     */
    static bool& ThrowEnabled() {
        static bool enabled = false;
        return enabled;
    }
};

/**
 * @brief 统计构造与移动构造次数的测试类型。
 *
 * 两参数构造用于区分「原地构造」与「构造临时对象再入队」；计数器是进程级
 * 静态状态，测试开始前必须调用 Reset()。
 */
class MoveCountedValue final {
public:
    /**
     * @brief 默认构造为 0 并累加构造计数。
     */
    MoveCountedValue() {
        ++construct_count_;
    }

    /**
     * @brief 以两个分量构造，value 为 first * 10 + second，并累加构造计数。
     * @param first 十位分量。
     * @param second 个位分量。
     */
    MoveCountedValue(int first, int second) : value_(first * 10 + second) {
        ++construct_count_;
    }

    /**
     * @brief 移动构造：复制 value 并累加移动构造计数。
     * @param other 源对象。
     */
    MoveCountedValue(MoveCountedValue&& other) noexcept : value_(other.value_) {
        ++move_construct_count_;
    }

    /**
     * @brief 移动赋值：复制 value，不计数。
     * @param other 源对象。
     * @return 自身引用。
     */
    MoveCountedValue& operator=(MoveCountedValue&& other) noexcept {
        value_ = other.value_;
        return *this;
    }

    /**
     * @brief 返回携带的测试值。
     * @return 测试值。
     */
    int Value() const {
        return value_;
    }

    /**
     * @brief 重置全部计数器。
     */
    static void Reset() {
        construct_count_ = 0;
        move_construct_count_ = 0;
    }

    /**
     * @brief 获取构造次数。
     * @return 构造次数。
     */
    static int ConstructCount() {
        return construct_count_;
    }

    /**
     * @brief 获取移动构造次数。
     * @return 移动构造次数。
     */
    static int MoveConstructCount() {
        return move_construct_count_;
    }

private:
    // 携带的测试值。
    int value_ = 0;
    // 构造次数；本头文件可被多个测试目标包含，使用内联静态成员避免重复定义。
    inline static int construct_count_ = 0;
    // 移动构造次数。
    inline static int move_construct_count_ = 0;
};

TEST(MultiProducerMultiConsumerTest, ConstructEmpty) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_EQ(queue.Capacity(), 4u);
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(MultiProducerMultiConsumerTest, EnqueueThenDequeueFifo) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_TRUE(queue.Enqueue(1));
    EXPECT_TRUE(queue.Enqueue(2));

    int first = 0;
    int second = 0;

    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(1000)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(1000)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(MultiProducerMultiConsumerTest, EmplaceThenDequeueFifo) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_TRUE(queue.Emplace(1));
    EXPECT_TRUE(queue.Emplace(2));

    int first = 0;
    int second = 0;

    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(1000)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(1000)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(MultiProducerMultiConsumerTest, DequeueUntilEmpty) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_TRUE(queue.Enqueue(1));
    EXPECT_TRUE(queue.Enqueue(2));
    EXPECT_TRUE(queue.Enqueue(3));

    for (int expected = 1; expected <= 3; ++expected) {
        int item = 0;
        EXPECT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(1000)));
        EXPECT_EQ(item, expected);
    }

    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, DequeueForWakesOnEnqueue) {
    MultiProducerMultiConsumer<int> queue(4);

    /**
     * @brief 消费者线程共享的结果；线程 join 之后读取。
     */
    struct ConsumerOutcome {
        // 收到的值。
        int received = 0;
        // 是否成功出队。
        bool dequeued = false;
        // 出队耗时。
        std::chrono::milliseconds elapsed{0};
        // 消费者即将进入等待的握手信号。
        std::promise<void> ready;
    };

    ConsumerOutcome outcome;
    std::future<void> ready_future = outcome.ready.get_future();

    std::thread consumer([&queue, &outcome] {
        outcome.ready.set_value();

        const auto start = std::chrono::steady_clock::now();
        outcome.dequeued = queue.DequeueFor(outcome.received, std::chrono::milliseconds(5000));
        outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    });

    ready_future.wait();
    EXPECT_TRUE(queue.Enqueue(7));
    consumer.join();

    EXPECT_TRUE(outcome.dequeued);
    EXPECT_EQ(outcome.received, 7);
    EXPECT_LT(outcome.elapsed.count(), 4000);
}

TEST(MultiProducerMultiConsumerTest, ConcurrentProducersConsumers) {
    constexpr int kProducerCount = 4;
    constexpr int kConsumerCount = 4;
    constexpr int kPerProducer = 500;
    constexpr int kTotal = kProducerCount * kPerProducer;

    MultiProducerMultiConsumer<int> queue(2000);

    std::atomic<int> consumed(0);
    std::atomic<bool> timed_out(false);
    std::mutex collected_mutex;
    std::vector<int> collected;
    collected.reserve(kTotal);

    std::vector<std::thread> producers;
    for (int producer = 0; producer < kProducerCount; ++producer) {
        producers.emplace_back([&queue, producer] {
            const int base = producer * kPerProducer;
            for (int index = 0; index < kPerProducer; ++index) {
                EXPECT_TRUE(queue.Enqueue(base + index));
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int consumer = 0; consumer < kConsumerCount; ++consumer) {
        consumers.emplace_back([&queue, &consumed, &timed_out, &collected, &collected_mutex] {
            const auto start = std::chrono::steady_clock::now();
            int item = 0;

            while (consumed.load() < kTotal) {
                if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
                    timed_out.store(true);
                    return;
                }

                if (!queue.DequeueFor(item, std::chrono::milliseconds(10))) {
                    continue;
                }

                {
                    std::lock_guard<std::mutex> guard(collected_mutex);
                    collected.push_back(item);
                }

                consumed.fetch_add(1);
            }
        });
    }

    for (std::thread& producer : producers) {
        producer.join();
    }

    for (std::thread& consumer : consumers) {
        consumer.join();
    }

    EXPECT_FALSE(timed_out.load());
    ASSERT_EQ(collected.size(), static_cast<std::size_t>(kTotal));

    std::sort(collected.begin(), collected.end());

    std::vector<int> expected(static_cast<std::size_t>(kTotal));
    std::iota(expected.begin(), expected.end(), 0);

    EXPECT_EQ(collected, expected);
}

TEST(MultiProducerMultiConsumerTest, MoveOnlyPayload) {
    MultiProducerMultiConsumer<std::unique_ptr<int>> queue(4);

    EXPECT_TRUE(queue.Enqueue(std::make_unique<int>(7)));

    std::unique_ptr<int> popped;
    EXPECT_TRUE(queue.DequeueFor(popped, std::chrono::milliseconds(1000)));
    ASSERT_NE(popped, nullptr);
    EXPECT_EQ(*popped, 7);
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, ConstQueries) {
    MultiProducerMultiConsumer<int> queue(3);
    EXPECT_TRUE(queue.EnqueueNoWait(5));

    const MultiProducerMultiConsumer<int>& const_queue = queue;

    EXPECT_EQ(const_queue.Capacity(), 3u);
    EXPECT_EQ(const_queue.Size(), 1u);
    EXPECT_EQ(const_queue.OverrunCounter(), 0u);
}

TEST(MultiProducerMultiConsumerTest, SlowMoveAssignmentDeliversItem) {
    MultiProducerMultiConsumer<SlowMoveValue> queue(4);

    SlowMoveValue source;
    source.value = 42;
    EXPECT_TRUE(queue.Enqueue(std::move(source)));

    SlowMoveValue popped;
    const auto start = std::chrono::steady_clock::now();
    const bool dequeued = queue.DequeueFor(popped, std::chrono::milliseconds(5000));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_TRUE(dequeued);
    EXPECT_EQ(popped.value, 42);
    EXPECT_GE(elapsed.count(), 5);
    EXPECT_LT(elapsed.count(), 5000);
}

TEST(MultiProducerMultiConsumerTest, NotCopyableNotMovable) {
    static_assert(!std::is_copy_constructible_v<MultiProducerMultiConsumer<int>>);
    static_assert(!std::is_copy_assignable_v<MultiProducerMultiConsumer<int>>);
    static_assert(!std::is_move_constructible_v<MultiProducerMultiConsumer<int>>);
    static_assert(!std::is_move_assignable_v<MultiProducerMultiConsumer<int>>);

    SUCCEED();
}

TEST(MultiProducerMultiConsumerTest, AliasesAndTraits) {
    static_assert(std::is_same_v<MultiProducerMultiConsumer<int>::value_type, int>);
    static_assert(std::is_same_v<MultiProducerMultiConsumer<std::unique_ptr<int>>::value_type, std::unique_ptr<int>>);

    SUCCEED();
}

TEST(MultiProducerMultiConsumerTest, HeaderIsSelfContained) {
    MultiProducerMultiConsumer<int> queue(2);

    EXPECT_TRUE(queue.Enqueue(1));

    int item = 0;
    EXPECT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(1000)));
    EXPECT_EQ(item, 1);
}

#ifndef USE_LOCK_FREE_QUEUE

TEST(MultiProducerMultiConsumerTest, EmplaceConstructsInPlaceWithoutMove) {
    MoveCountedValue::Reset();

    MultiProducerMultiConsumer<MoveCountedValue> queue(4);

    EXPECT_TRUE(queue.Emplace(1, 2));
    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(MoveCountedValue::ConstructCount(), 1);
    EXPECT_EQ(MoveCountedValue::MoveConstructCount(), 0);

    MoveCountedValue popped;
    EXPECT_TRUE(queue.DequeueFor(popped, std::chrono::milliseconds(1000)));
    EXPECT_EQ(popped.Value(), 12);
}

TEST(MultiProducerMultiConsumerTest, EnqueueNowaitDropsOldest) {
    MultiProducerMultiConsumer<int> queue(2);

    EXPECT_TRUE(queue.EnqueueNoWait(1));
    EXPECT_TRUE(queue.EnqueueNoWait(2));
    EXPECT_TRUE(queue.EnqueueNoWait(3));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.OverrunCounter(), 1u);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(1000)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(1000)));
    EXPECT_EQ(first, 2);
    EXPECT_EQ(second, 3);
}

TEST(MultiProducerMultiConsumerTest, EnqueueBlocksUntilSpace) {
    MultiProducerMultiConsumer<int> queue(1);
    EXPECT_TRUE(queue.Enqueue(1));

    std::promise<void> started;
    std::future<void> started_future = started.get_future();
    std::promise<void> finished;
    std::future<void> finished_future = finished.get_future();

    std::thread producer([&queue, &started, &finished] {
        started.set_value();
        EXPECT_TRUE(queue.Enqueue(2));
        finished.set_value();
    });

    started_future.wait();

    EXPECT_EQ(finished_future.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    int first = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(5000)));
    EXPECT_EQ(first, 1);

    EXPECT_EQ(finished_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(5000)));
    EXPECT_EQ(second, 2);

    producer.join();
}

TEST(MultiProducerMultiConsumerTest, ZeroCapacityNormalized) {
    MultiProducerMultiConsumer<int> queue(0);

    EXPECT_EQ(queue.Capacity(), 1u);

    EXPECT_TRUE(queue.EnqueueNoWait(1));
    EXPECT_TRUE(queue.EnqueueNoWait(2));

    int item = 0;
    EXPECT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(1000)));
    EXPECT_EQ(item, 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(MultiProducerMultiConsumerTest, HugeCapacityThrowsLengthError) {
    EXPECT_THROW({ MultiProducerMultiConsumer<int> queue(std::numeric_limits<std::size_t>::max()); }, std::length_error);
}

TEST(MultiProducerMultiConsumerTest, MultipleBlockedProducersWake) {
    MultiProducerMultiConsumer<int> queue(1);
    EXPECT_TRUE(queue.Enqueue(1));

    std::promise<void> first_ready;
    std::promise<void> second_ready;
    std::promise<void> first_done;
    std::promise<void> second_done;

    std::future<void> first_ready_future = first_ready.get_future();
    std::future<void> second_ready_future = second_ready.get_future();
    std::future<void> first_done_future = first_done.get_future();
    std::future<void> second_done_future = second_done.get_future();

    std::thread first([&queue, &first_ready, &first_done] {
        first_ready.set_value();
        EXPECT_TRUE(queue.Enqueue(2));
        first_done.set_value();
    });
    std::thread second([&queue, &second_ready, &second_done] {
        second_ready.set_value();
        EXPECT_TRUE(queue.Enqueue(3));
        second_done.set_value();
    });

    first_ready_future.wait();
    second_ready_future.wait();

    EXPECT_EQ(first_done_future.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);
    EXPECT_EQ(second_done_future.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    std::set<int> received;
    for (int count = 0; count < 3; ++count) {
        int item = 0;
        ASSERT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(5000)));
        received.insert(item);
    }

    first.join();
    second.join();

    const std::set<int> expected = {1, 2, 3};
    EXPECT_EQ(received, expected);
}

TEST(MultiProducerMultiConsumerTest, MoveAssignThrowKeepsQueueConsistent) {
    MultiProducerMultiConsumer<ThrowingMoveValue> queue(2);

    ThrowingMoveValue source;
    source.value = 9;
    EXPECT_TRUE(queue.Enqueue(std::move(source)));

    EXPECT_EQ(queue.Size(), 1u);

    ThrowingMoveValue popped;
    popped.value = 0;

    ThrowingMoveValue::ThrowEnabled() = true;
    EXPECT_THROW(
        {
            const bool outcome = queue.DequeueFor(popped, std::chrono::milliseconds(1000));
            (void)outcome;
        },
        std::runtime_error);
    ThrowingMoveValue::ThrowEnabled() = false;

    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_TRUE(queue.DequeueFor(popped, std::chrono::milliseconds(1000)));
    EXPECT_EQ(popped.value, 9);
}

#endif

#ifdef USE_LOCK_FREE_QUEUE

TEST(MultiProducerMultiConsumerTest, LockFreeEnqueueBeyondCapacityHint) {
    MultiProducerMultiConsumer<int> queue(1);

    for (int value = 0; value < 100; ++value) {
        EXPECT_TRUE(queue.Enqueue(std::move(value)));
    }

    EXPECT_EQ(queue.Capacity(), 1u);
    EXPECT_GE(queue.Size(), 90u);
}

TEST(MultiProducerMultiConsumerTest, LockFreeOverrunCounterAlwaysZero) {
    MultiProducerMultiConsumer<int> queue(2);

    for (int value = 0; value < 50; ++value) {
        EXPECT_TRUE(queue.EnqueueNoWait(std::move(value)));
    }

    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(MultiProducerMultiConsumerTest, LockFreeCapacityIsHint) {
    MultiProducerMultiConsumer<int> queue(1);

    for (int value = 0; value < 100; ++value) {
        EXPECT_TRUE(queue.Enqueue(std::move(value)));
    }

    EXPECT_EQ(queue.Capacity(), 1u);
}

TEST(MultiProducerMultiConsumerTest, LockFreeZeroCapacityHintNormalized) {
    MultiProducerMultiConsumer<int> queue(0);

    EXPECT_EQ(queue.Capacity(), 1u);

    EXPECT_TRUE(queue.Enqueue(1));

    int item = 0;
    EXPECT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(1000)));
    EXPECT_EQ(item, 1);
}

#endif

TEST(MultiProducerMultiConsumerTest, DequeueForTimesOut) {
    MultiProducerMultiConsumer<int> queue(4);
    int item = 0;

    const auto start = std::chrono::steady_clock::now();
    const bool dequeued = queue.DequeueFor(item, std::chrono::milliseconds(50));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_FALSE(dequeued);
    EXPECT_GE(elapsed.count(), 30);
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, DequeueForZeroTimeout) {
    MultiProducerMultiConsumer<int> queue(4);
    int item = 0;

    EXPECT_FALSE(queue.DequeueFor(item, std::chrono::milliseconds(0)));
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, DequeueForNegativeTimeout) {
    MultiProducerMultiConsumer<int> queue(4);
    int item = 0;

    const auto start = std::chrono::steady_clock::now();
    const bool dequeued = queue.DequeueFor(item, std::chrono::milliseconds(-1));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_FALSE(dequeued);
    EXPECT_LT(elapsed.count(), 1000);
}

TEST(MultiProducerMultiConsumerTest, TryEnqueueSucceedsWhenNotFull) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_TRUE(queue.TryEnqueue(1));
    EXPECT_TRUE(queue.TryEnqueue(2));
    EXPECT_EQ(queue.Size(), 2u);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(0)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(0)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(MultiProducerMultiConsumerTest, CloseRejectsEnqueue) {
    MultiProducerMultiConsumer<int> queue(4);
    queue.Close();

    EXPECT_FALSE(queue.Enqueue(1));
    EXPECT_FALSE(queue.EnqueueNoWait(2));
    EXPECT_FALSE(queue.TryEnqueue(3));
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, EmplaceReturnsFalseAfterClose) {
    MultiProducerMultiConsumer<int> queue(4);
    queue.Close();

    EXPECT_FALSE(queue.Emplace(1));
    EXPECT_FALSE(queue.TryEmplace(2));
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(MultiProducerMultiConsumerTest, CloseDrainsRemaining) {
    MultiProducerMultiConsumer<int> queue(4);

    EXPECT_TRUE(queue.Enqueue(1));
    EXPECT_TRUE(queue.Enqueue(2));
    queue.Close();

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(0)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(0)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);

    EXPECT_FALSE(queue.DequeueFor(first, std::chrono::milliseconds(5000)));
}

TEST(MultiProducerMultiConsumerTest, CloseMakesDequeueNonBlocking) {
    MultiProducerMultiConsumer<int> queue(4);
    queue.Close();

    int item = 0;
    const auto start = std::chrono::steady_clock::now();
    const bool dequeued = queue.DequeueFor(item, std::chrono::milliseconds(5000));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_FALSE(dequeued);
    EXPECT_LT(elapsed.count(), 1000);
}

TEST(MultiProducerMultiConsumerTest, CloseIsIdempotent) {
    MultiProducerMultiConsumer<int> queue(2);

    queue.Close();
    queue.Close();

    EXPECT_FALSE(queue.Enqueue(1));
    EXPECT_EQ(queue.Size(), 0u);
}

#ifndef USE_LOCK_FREE_QUEUE

TEST(MultiProducerMultiConsumerTest, TryEnqueueRejectsWhenFull) {
    MultiProducerMultiConsumer<int> queue(2);

    EXPECT_TRUE(queue.TryEnqueue(1));
    EXPECT_TRUE(queue.TryEnqueue(2));
    EXPECT_FALSE(queue.TryEnqueue(3));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.OverrunCounter(), 0u);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(0)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(0)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(MultiProducerMultiConsumerTest, TryEmplaceRejectsWhenFullWithoutOverrun) {
    MultiProducerMultiConsumer<int> queue(2);

    EXPECT_TRUE(queue.TryEmplace(1));
    EXPECT_TRUE(queue.TryEmplace(2));
    EXPECT_FALSE(queue.TryEmplace(3));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.OverrunCounter(), 0u);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(0)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(0)));
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(MultiProducerMultiConsumerTest, OverrunCounterCountsOnlyOverwrites) {
    MultiProducerMultiConsumer<int> queue(2);

    EXPECT_TRUE(queue.TryEnqueue(1));
    EXPECT_TRUE(queue.TryEnqueue(2));
    EXPECT_FALSE(queue.TryEnqueue(3));
    EXPECT_EQ(queue.OverrunCounter(), 0u);

    EXPECT_TRUE(queue.EnqueueNoWait(4));
    EXPECT_EQ(queue.OverrunCounter(), 1u);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(queue.DequeueFor(first, std::chrono::milliseconds(0)));
    EXPECT_TRUE(queue.DequeueFor(second, std::chrono::milliseconds(0)));
    EXPECT_EQ(first, 2);
    EXPECT_EQ(second, 4);
}

TEST(MultiProducerMultiConsumerTest, CloseWakesBlockedEnqueue) {
    MultiProducerMultiConsumer<int> queue(1);
    EXPECT_TRUE(queue.Enqueue(1));

    std::promise<void> started;
    std::future<void> started_future = started.get_future();
    std::atomic<bool> produced(true);

    std::thread producer([&queue, &started, &produced] {
        started.set_value();
        produced.store(queue.Enqueue(2));
    });

    started_future.wait();
    queue.Close();
    producer.join();

    EXPECT_FALSE(produced.load());

    int item = 0;
    EXPECT_TRUE(queue.DequeueFor(item, std::chrono::milliseconds(0)));
    EXPECT_EQ(item, 1);
    EXPECT_FALSE(queue.DequeueFor(item, std::chrono::milliseconds(0)));
}

TEST(MultiProducerMultiConsumerTest, CloseWakesBlockedDequeue) {
    MultiProducerMultiConsumer<int> queue(4);

    /**
     * @brief 消费者线程共享的结果；线程 join 之后读取。
     */
    struct ConsumerOutcome {
        // 是否成功出队。
        std::atomic<bool> dequeued{true};
        // 出队耗时。
        std::chrono::milliseconds elapsed{0};
        // 消费者即将进入等待的握手信号。
        std::promise<void> ready;
    };

    ConsumerOutcome outcome;
    std::future<void> ready_future = outcome.ready.get_future();

    std::thread consumer([&queue, &outcome] {
        outcome.ready.set_value();

        int item = 0;
        const auto start = std::chrono::steady_clock::now();
        outcome.dequeued.store(queue.DequeueFor(item, std::chrono::milliseconds(5000)));
        outcome.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    });

    ready_future.wait();
    queue.Close();
    consumer.join();

    EXPECT_FALSE(outcome.dequeued.load());
    EXPECT_LT(outcome.elapsed.count(), 1000);
}

#endif

#ifdef USE_LOCK_FREE_QUEUE

TEST(MultiProducerMultiConsumerTest, LockFreeTryEnqueueBeyondHint) {
    MultiProducerMultiConsumer<int> queue(1);

    for (int value = 0; value < 100; ++value) {
        EXPECT_TRUE(queue.TryEnqueue(std::move(value)));
    }

    EXPECT_GE(queue.Size(), 90u);
}

TEST(MultiProducerMultiConsumerTest, LockFreeEmplaceUsesMove) {
    MoveCountedValue::Reset();

    MultiProducerMultiConsumer<MoveCountedValue> queue(4);

    EXPECT_TRUE(queue.Emplace(1, 2));
    EXPECT_GE(MoveCountedValue::MoveConstructCount(), 1);
}

TEST(MultiProducerMultiConsumerTest, LockFreeTryEmplaceBeyondHint) {
    MultiProducerMultiConsumer<int> queue(1);

    for (int value = 0; value < 100; ++value) {
        EXPECT_TRUE(queue.TryEmplace(value + 1));
    }

    EXPECT_GE(queue.Size(), 90u);
}

#endif

#endif
