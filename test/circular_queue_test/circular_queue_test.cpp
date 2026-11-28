/**
 * @file circular_queue_test.cpp
 * @brief CircularQueue<T> 的 GoogleTest 用例。
 */

#include "multi_producer_multi_consumer/circular_queue.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

// 被测类型位于扁平命名空间 everest。
using everest::CircularQueue;

/**
 * @brief 统计默认构造、移动构造、移动赋值与析构次数的测试类型。
 *
 * 计数器是进程级静态状态，测试开始前必须调用 Reset()。
 */
class TrackedValue final {
public:
    /**
     * @brief 默认构造并累加默认构造计数。
     */
    TrackedValue() {
        ++construct_count_;
    }

    /**
     * @brief 移动构造：复制 value 并累加移动构造计数。
     * @param other 源对象。
     */
    TrackedValue(TrackedValue&& other) noexcept {
        value = other.value;
        ++move_construct_count_;
    }

    /**
     * @brief 移动赋值：复制 value 并累加移动赋值计数。
     * @param other 源对象。
     * @return 自身引用。
     */
    TrackedValue& operator=(TrackedValue&& other) noexcept {
        value = other.value;
        ++move_assign_count_;
        return *this;
    }

    /**
     * @brief 析构并累加析构计数。
     */
    ~TrackedValue() {
        ++destruct_count_;
    }

    /**
     * @brief 重置全部计数器。
     */
    static void Reset() {
        construct_count_ = 0;
        move_construct_count_ = 0;
        destruct_count_ = 0;
        move_assign_count_ = 0;
    }

    /**
     * @brief 获取默认构造次数。
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

    /**
     * @brief 获取析构次数。
     * @return 析构次数。
     */
    static int DestructCount() {
        return destruct_count_;
    }

    /**
     * @brief 获取移动赋值次数。
     * @return 移动赋值次数。
     */
    static int MoveAssignCount() {
        return move_assign_count_;
    }

    // 携带的测试值。
    int value = 0;

private:
    // 默认构造次数。
    static int construct_count_;
    // 移动构造次数。
    static int move_construct_count_;
    // 析构次数。
    static int destruct_count_;
    // 移动赋值次数。
    static int move_assign_count_;
};

int TrackedValue::construct_count_ = 0;
int TrackedValue::move_construct_count_ = 0;
int TrackedValue::destruct_count_ = 0;
int TrackedValue::move_assign_count_ = 0;

/**
 * @brief 对齐要求高于默认值的测试类型。
 */
struct alignas(64) OverAlignedValue {
    // 携带的测试值。
    int value = 0;
};

/**
 * @brief 无默认构造、不可移动赋值的测试类型。
 *
 * 用于验证 CircularQueue 只要求可移动构造与可析构。
 */
class NoDefaultCtorValue final {
public:
    // 携带的测试值。
    int value = 0;

    /**
     * @brief 以给定初始值构造。
     * @param initial_value 初始值。
     */
    explicit NoDefaultCtorValue(int initial_value) : value(initial_value) {}

    /**
     * @brief 移动构造：复制 value。
     * @param other 源对象。
     */
    NoDefaultCtorValue(NoDefaultCtorValue&& other) noexcept {
        value = other.value;
    }

    NoDefaultCtorValue(const NoDefaultCtorValue&) = delete;
    NoDefaultCtorValue& operator=(const NoDefaultCtorValue&) = delete;
    NoDefaultCtorValue& operator=(NoDefaultCtorValue&&) = delete;
};

/**
 * @brief 移动构造可被置位为抛异常的测试类型。
 *
 * 异常开关是进程级状态，测试用例负责置位与复位。
 */
class ThrowingMoveCtorValue final {
public:
    // 携带的测试值。
    int value = 0;

    /**
     * @brief 默认构造为 0。
     */
    ThrowingMoveCtorValue() = default;

    /**
     * @brief 移动构造；开关置位时抛出 std::runtime_error。
     * @param other 源对象。
     * @throw std::runtime_error 开关置位时抛出。
     */
    ThrowingMoveCtorValue(ThrowingMoveCtorValue&& other) {
        if (ThrowEnabled()) {
            throw std::runtime_error("throwing move construction");
        }

        value = other.value;
    }

    /**
     * @brief 获取移动构造抛异常开关。
     * @return 开关的引用；测试用例负责置位与复位。
     */
    static bool& ThrowEnabled() {
        static bool enabled = false;
        return enabled;
    }
};

/**
 * @brief 统计默认构造、移动构造、移动赋值与析构次数的测试类型。
 *
 * 两参数构造用于验证 EmplaceBack 在槽位上直接构造、不产生额外移动；
 * 计数器是进程级静态状态，测试开始前必须调用 Reset()。
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
     * @brief 移动赋值：复制 value 并累加移动赋值计数。
     * @param other 源对象。
     * @return 自身引用。
     */
    MoveCountedValue& operator=(MoveCountedValue&& other) noexcept {
        value_ = other.value_;
        ++move_assign_count_;
        return *this;
    }

    /**
     * @brief 析构并累加析构计数。
     */
    ~MoveCountedValue() {
        ++destruct_count_;
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
        move_assign_count_ = 0;
        destruct_count_ = 0;
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

    /**
     * @brief 获取移动赋值次数。
     * @return 移动赋值次数。
     */
    static int MoveAssignCount() {
        return move_assign_count_;
    }

    /**
     * @brief 获取析构次数。
     * @return 析构次数。
     */
    static int DestructCount() {
        return destruct_count_;
    }

private:
    // 携带的测试值。
    int value_ = 0;
    // 构造次数。
    static int construct_count_;
    // 移动构造次数。
    static int move_construct_count_;
    // 移动赋值次数。
    static int move_assign_count_;
    // 析构次数。
    static int destruct_count_;
};

int MoveCountedValue::construct_count_ = 0;
int MoveCountedValue::move_construct_count_ = 0;
int MoveCountedValue::move_assign_count_ = 0;
int MoveCountedValue::destruct_count_ = 0;

/**
 * @brief 只接受 std::unique_ptr<int> 的测试类型，用于验证完美转发。
 */
class Holder final {
public:
    /**
     * @brief 接管给定的值。
     * @param initial_value 待接管的指针；不能为空。
     */
    explicit Holder(std::unique_ptr<int> initial_value) : value_(std::move(initial_value)) {}

    /**
     * @brief 返回持有的值。
     * @return 持有指针的只读引用。
     */
    const std::unique_ptr<int>& Value() const {
        return value_;
    }

private:
    // 持有的测试值。
    std::unique_ptr<int> value_;
};

/**
 * @brief 构造函数可被置位为抛异常的测试类型。
 *
 * 移动构造不抛异常，用于验证 EmplaceBack 构造抛异常时的队列保证。
 * 异常开关是进程级状态，测试用例负责置位与复位。
 */
class ThrowingCtorValue final {
public:
    /**
     * @brief 以给定初始值构造；开关置位时抛出 std::runtime_error。
     * @param initial_value 初始值。
     * @throw std::runtime_error 开关置位时抛出。
     */
    explicit ThrowingCtorValue(int initial_value) : value_(initial_value) {
        if (ThrowEnabled()) {
            throw std::runtime_error("throwing construction");
        }
    }

    /**
     * @brief 移动构造：复制 value，不抛异常。
     * @param other 源对象。
     */
    ThrowingCtorValue(ThrowingCtorValue&& other) noexcept : value_(other.value_) {}

    /**
     * @brief 返回携带的测试值。
     * @return 测试值。
     */
    int Value() const {
        return value_;
    }

    /**
     * @brief 获取构造抛异常开关。
     * @return 开关的引用；测试用例负责置位与复位。
     */
    static bool& ThrowEnabled() {
        static bool enabled = false;
        return enabled;
    }

private:
    // 携带的测试值。
    int value_ = 0;
};

TEST(CircularQueueTest, ConstructEmpty) {
    CircularQueue<int> queue(4);

    EXPECT_EQ(queue.Capacity(), 4u);
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.Empty());
    EXPECT_FALSE(queue.Full());
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, ConstructionConstructsNoElements) {
    TrackedValue::Reset();

    {
        CircularQueue<TrackedValue> queue(4);

        EXPECT_EQ(queue.Capacity(), 4u);
        EXPECT_EQ(queue.Size(), 0u);

        // 队列只分配对齐存储，不默认构造任何槽位。
        EXPECT_EQ(TrackedValue::ConstructCount(), 0);
        EXPECT_EQ(TrackedValue::MoveConstructCount(), 0);
        EXPECT_EQ(TrackedValue::DestructCount(), 0);
    }
}

TEST(CircularQueueTest, PushBackAndIndex) {
    CircularQueue<int> queue(4);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);

    EXPECT_EQ(queue.Size(), 3u);
    EXPECT_EQ(queue.Front(), 1);
    EXPECT_EQ(queue.At(0), 1);
    EXPECT_EQ(queue.At(1), 2);
    EXPECT_EQ(queue.At(2), 3);
    EXPECT_FALSE(queue.Full());
}

TEST(CircularQueueTest, FillToCapacity) {
    CircularQueue<int> queue(4);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);
    queue.PushBack(4);

    EXPECT_TRUE(queue.Full());
    EXPECT_EQ(queue.Size(), 4u);
    EXPECT_EQ(queue.Capacity(), 4u);
}

TEST(CircularQueueTest, PopFrontFifo) {
    CircularQueue<int> queue(4);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);
    queue.PushBack(4);
    queue.PopFront();
    queue.PopFront();

    EXPECT_EQ(queue.Front(), 3);
    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.At(0), 3);
    EXPECT_EQ(queue.At(1), 4);
}

TEST(CircularQueueTest, MutableAccessors) {
    CircularQueue<int> queue(2);

    queue.PushBack(1);
    queue.PushBack(2);

    queue.Front() = 10;
    queue.At(1) = 20;

    const CircularQueue<int>& const_queue = queue;

    EXPECT_EQ(const_queue.Front(), 10);
    EXPECT_EQ(const_queue.At(0), 10);
    EXPECT_EQ(const_queue.At(1), 20);
}

TEST(CircularQueueTest, OverrunDropsOldest) {
    CircularQueue<int> queue(2);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front(), 2);
    EXPECT_EQ(queue.At(0), 2);
    EXPECT_EQ(queue.At(1), 3);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, WrapAroundKeepsOrder) {
    CircularQueue<int> queue(3);

    for (int index = 0; index < 200; ++index) {
        queue.PushBack(static_cast<int>(index));

        if (queue.Size() == 3) {
            EXPECT_EQ(queue.Front(), index - 2);
            EXPECT_EQ(queue.At(0), index - 2);
            EXPECT_EQ(queue.At(2), index);
            queue.PopFront();
        }
    }

    EXPECT_EQ(queue.OverrunCounter(), 0u);
    EXPECT_EQ(queue.Size(), 2u);
}

TEST(CircularQueueTest, MoveConstructTransfers) {
    CircularQueue<int> source(4);
    source.PushBack(1);
    source.PushBack(2);

    CircularQueue<int> target(std::move(source));

    EXPECT_EQ(target.Capacity(), 4u);
    EXPECT_EQ(target.Size(), 2u);
    EXPECT_EQ(target.At(0), 1);
    EXPECT_EQ(target.At(1), 2);

    EXPECT_EQ(source.Capacity(), 0u);
    EXPECT_TRUE(source.Empty());
    EXPECT_EQ(source.Size(), 0u);

    source.PushBack(3);
    EXPECT_EQ(source.Size(), 0u);
}

TEST(CircularQueueTest, MoveAssignTransfers) {
    CircularQueue<int> source(2);
    source.PushBack(5);
    source.PushBack(6);

    CircularQueue<int> target(8);
    target.PushBack(1);
    target = std::move(source);

    EXPECT_EQ(target.Capacity(), 2u);
    EXPECT_EQ(target.Size(), 2u);
    EXPECT_EQ(target.At(0), 5);
    EXPECT_EQ(target.At(1), 6);

    EXPECT_EQ(source.Capacity(), 0u);
    EXPECT_TRUE(source.Empty());
}

TEST(CircularQueueTest, OverAlignedPayload) {
    CircularQueue<OverAlignedValue> queue(4);

    for (int index = 0; index < 4; ++index) {
        OverAlignedValue value;
        value.value = index;
        queue.PushBack(std::move(value));
    }

    for (std::size_t index = 0; index < queue.Size(); ++index) {
        // 对齐检查需要把地址转成整数；这里只用于验证，不参与业务逻辑。
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(&queue.At(index));
        EXPECT_EQ(address % alignof(OverAlignedValue), 0u);
        EXPECT_EQ(queue.At(index).value, static_cast<int>(index));
    }
}

TEST(CircularQueueTest, MoveOnlyPayload) {
    CircularQueue<std::unique_ptr<int>> queue(2);

    queue.PushBack(std::make_unique<int>(7));

    EXPECT_EQ(queue.Size(), 1u);
    ASSERT_NE(queue.Front(), nullptr);
    EXPECT_EQ(*queue.Front(), 7);

    queue.PopFront();

    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.Empty());
}

TEST(CircularQueueTest, NoDefaultCtorPayload) {
    CircularQueue<NoDefaultCtorValue> queue(3);

    queue.PushBack(NoDefaultCtorValue(1));
    queue.PushBack(NoDefaultCtorValue(2));
    queue.PushBack(NoDefaultCtorValue(3));

    ASSERT_TRUE(queue.Full());
    EXPECT_EQ(queue.At(0).value, 1);
    EXPECT_EQ(queue.At(1).value, 2);
    EXPECT_EQ(queue.At(2).value, 3);

    queue.PopFront();

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front().value, 2);
    EXPECT_EQ(queue.At(1).value, 3);
}

TEST(CircularQueueTest, TrackedTypeLifecycle) {
    TrackedValue::Reset();

    {
        CircularQueue<TrackedValue> queue(2);

        TrackedValue first;
        first.value = 1;
        TrackedValue second;
        second.value = 2;

        const int constructed_before_push = TrackedValue::ConstructCount();
        const int move_constructed_before_push = TrackedValue::MoveConstructCount();
        const int destructed_before_push = TrackedValue::DestructCount();

        queue.PushBack(std::move(first));
        queue.PushBack(std::move(second));

        // 入队以移动构造写入槽位：不触发默认构造，也不移动赋值。
        EXPECT_EQ(TrackedValue::ConstructCount(), constructed_before_push);
        EXPECT_EQ(TrackedValue::MoveConstructCount(), move_constructed_before_push + 2);
        EXPECT_EQ(TrackedValue::DestructCount(), destructed_before_push);
        EXPECT_EQ(TrackedValue::MoveAssignCount(), 0);

        queue.PopFront();

        // 出队析构队首元素。
        EXPECT_EQ(TrackedValue::DestructCount(), destructed_before_push + 1);
        EXPECT_EQ(queue.Front().value, 2);
    }

    // 队列析构后，经过队列的元素全部被析构：默认构造 + 移动构造 == 析构。
    EXPECT_EQ(TrackedValue::ConstructCount() + TrackedValue::MoveConstructCount(), TrackedValue::DestructCount());
}

TEST(CircularQueueTest, CapacityOne) {
    CircularQueue<int> queue(1);

    queue.PushBack(1);
    queue.PushBack(2);

    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.Front(), 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, NonPowerOfTwoCapacityUsesExactCapacity) {
    CircularQueue<int> queue(5);

    EXPECT_EQ(queue.Capacity(), 5u);

    for (int value = 1; value <= 5; ++value) {
        queue.PushBack(static_cast<int>(value));
    }

    EXPECT_TRUE(queue.Full());
    EXPECT_EQ(queue.Size(), 5u);

    queue.PushBack(6);

    EXPECT_EQ(queue.Size(), 5u);
    EXPECT_EQ(queue.Front(), 2);
    EXPECT_EQ(queue.At(4), 6);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, WrapAroundNonPowerOfTwoKeepsOrder) {
    CircularQueue<int> queue(5);

    for (int index = 0; index < 100; ++index) {
        queue.PushBack(static_cast<int>(index));

        if (queue.Size() == 5) {
            EXPECT_EQ(queue.At(0), index - 4);
            EXPECT_EQ(queue.At(4), index);
            queue.PopFront();
            EXPECT_EQ(queue.At(0), index - 3);
        }
    }

    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, MoveOperationsNoexcept) {
    static_assert(std::is_nothrow_move_constructible_v<CircularQueue<int>>);
    static_assert(std::is_nothrow_move_assignable_v<CircularQueue<int>>);

    SUCCEED();
}

TEST(CircularQueueTest, HeaderIsSelfContained) {
    CircularQueue<int> queue(2);

    queue.PushBack(1);

    EXPECT_EQ(queue.Front(), 1);
}

TEST(CircularQueueTest, ZeroCapacityNormalized) {
    CircularQueue<int> queue(0);

    EXPECT_EQ(queue.Capacity(), 1u);

    queue.PushBack(1);
    EXPECT_TRUE(queue.Full());

    queue.PushBack(2);

    EXPECT_EQ(queue.Front(), 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, HugeCapacityRejected) {
    EXPECT_THROW({ CircularQueue<int> queue(std::numeric_limits<std::size_t>::max()); }, std::length_error);
}

TEST(CircularQueueTest, PopFrontOnEmptyKeepsState) {
    CircularQueue<int> queue(2);

    queue.PopFront();
    queue.PopFront();

    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, PopFrontPastSize) {
    CircularQueue<int> queue(2);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PopFront();
    queue.PopFront();
    queue.PopFront();

    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(CircularQueueTest, OverrunCounterAccumulates) {
    CircularQueue<int> queue(1);

    queue.PushBack(1);
    queue.PushBack(2);
    queue.PushBack(3);
    queue.PushBack(4);
    queue.PushBack(5);

    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.Front(), 5);
    EXPECT_EQ(queue.OverrunCounter(), 4u);
}

TEST(CircularQueueTest, TryPushAcceptsWhenNotFull) {
    CircularQueue<int> queue(2);

    EXPECT_TRUE(queue.TryPushBack(1));
    EXPECT_TRUE(queue.TryPushBack(2));
    EXPECT_TRUE(queue.Full());

    EXPECT_EQ(queue.Front(), 1);
    EXPECT_EQ(queue.At(1), 2);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, TryPushBackRejectsWhenFull) {
    CircularQueue<int> queue(2);

    queue.PushBack(1);
    queue.PushBack(2);

    EXPECT_FALSE(queue.TryPushBack(3));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front(), 1);
    EXPECT_EQ(queue.At(1), 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, TryPushLeavesItemUntouched) {
    CircularQueue<std::unique_ptr<int>> queue(1);

    ASSERT_TRUE(queue.TryPushBack(std::make_unique<int>(1)));

    auto rejected = std::make_unique<int>(2);
    EXPECT_FALSE(queue.TryPushBack(std::move(rejected)));

    ASSERT_NE(rejected, nullptr);
    EXPECT_EQ(*rejected, 2);
    EXPECT_EQ(*queue.Front(), 1);
}

TEST(CircularQueueTest, TryPushOnMovedFromQueue) {
    CircularQueue<int> source(2);
    CircularQueue<int> target(std::move(source));

    EXPECT_FALSE(source.TryPushBack(1));
    EXPECT_EQ(source.Size(), 0u);
}

TEST(CircularQueueTest, PushBackMoveCtorThrowsLeavesQueueUnchanged) {
    CircularQueue<ThrowingMoveCtorValue> queue(4);

    ThrowingMoveCtorValue first;
    first.value = 1;
    queue.PushBack(std::move(first));
    EXPECT_EQ(queue.Size(), 1u);

    ThrowingMoveCtorValue::ThrowEnabled() = true;
    ThrowingMoveCtorValue second;
    second.value = 2;

    EXPECT_THROW(queue.PushBack(std::move(second)), std::runtime_error);
    ThrowingMoveCtorValue::ThrowEnabled() = false;

    // 非满路径强保证：异常向外传播，队列状态不变。
    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.Front().value, 1);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, OverwriteMoveCtorThrowsKeepsQueueUsable) {
    CircularQueue<ThrowingMoveCtorValue> queue(2);

    ThrowingMoveCtorValue first;
    first.value = 1;
    ThrowingMoveCtorValue second;
    second.value = 2;
    queue.PushBack(std::move(first));
    queue.PushBack(std::move(second));
    ASSERT_TRUE(queue.Full());

    ThrowingMoveCtorValue::ThrowEnabled() = true;
    ThrowingMoveCtorValue third;
    third.value = 3;

    EXPECT_THROW(queue.PushBack(std::move(third)), std::runtime_error);
    ThrowingMoveCtorValue::ThrowEnabled() = false;

    // 覆盖路径基本保证：最旧已丢、新元素未入队、size 减 1、overrun 加 1。
    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.Front().value, 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);

    ThrowingMoveCtorValue fourth;
    fourth.value = 4;
    queue.PushBack(std::move(fourth));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front().value, 2);
    EXPECT_EQ(queue.At(1).value, 4);

    queue.PopFront();

    EXPECT_EQ(queue.Front().value, 4);
}

TEST(CircularQueueTest, SelfMoveAssignPreservesContent) {
    CircularQueue<int> queue(3);

    queue.PushBack(1);
    queue.PushBack(2);

    CircularQueue<int>& alias = queue;
    queue = std::move(alias);

    EXPECT_EQ(queue.Capacity(), 3u);
    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.At(0), 1);
    EXPECT_EQ(queue.At(1), 2);
}

TEST(CircularQueueTest, MovedFromQueueSafeQueries) {
    CircularQueue<int> source(2);
    source.PushBack(1);

    CircularQueue<int> target(std::move(source));

    EXPECT_EQ(source.Capacity(), 0u);
    EXPECT_TRUE(source.Empty());
    EXPECT_FALSE(source.Full());
    EXPECT_EQ(source.Size(), 0u);
    EXPECT_EQ(source.OverrunCounter(), 0u);

    source.PushBack(2);

    EXPECT_EQ(source.Size(), 0u);
}

TEST(CircularQueueTest, NotCopyable) {
    static_assert(!std::is_copy_constructible_v<CircularQueue<int>>);
    static_assert(!std::is_copy_assignable_v<CircularQueue<int>>);

    SUCCEED();
}

TEST(CircularQueueTest, NotDefaultConstructible) {
    static_assert(!std::is_default_constructible_v<CircularQueue<int>>);

    SUCCEED();
}

TEST(CircularQueueTest, MoveAssignNotRequired) {
    static_assert(!std::is_default_constructible_v<NoDefaultCtorValue>);
    static_assert(std::is_move_constructible_v<NoDefaultCtorValue>);
    static_assert(!std::is_move_assignable_v<NoDefaultCtorValue>);

    CircularQueue<NoDefaultCtorValue> queue(2);

    queue.PushBack(NoDefaultCtorValue(1));
    queue.PushBack(NoDefaultCtorValue(2));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front().value, 1);
    EXPECT_EQ(queue.At(1).value, 2);

    queue.PopFront();

    EXPECT_EQ(queue.Front().value, 2);
}

TEST(CircularQueueTest, EmplaceBackConstructsInPlaceWithoutMove) {
    MoveCountedValue::Reset();

    {
        CircularQueue<MoveCountedValue> queue(4);

        queue.EmplaceBack(1, 2);

        EXPECT_EQ(queue.Size(), 1u);
        EXPECT_EQ(queue.At(0).Value(), 12);
        EXPECT_EQ(MoveCountedValue::ConstructCount(), 1);
        EXPECT_EQ(MoveCountedValue::MoveConstructCount(), 0);
        EXPECT_EQ(MoveCountedValue::MoveAssignCount(), 0);
    }
}

TEST(CircularQueueTest, EmplaceBackOverwritesOldest) {
    CircularQueue<MoveCountedValue> queue(2);

    queue.EmplaceBack(1, 0);
    queue.EmplaceBack(2, 0);
    queue.EmplaceBack(3, 0);

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.At(0).Value(), 20);
    EXPECT_EQ(queue.At(1).Value(), 30);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, TryEmplaceBackSucceedsWhenNotFull) {
    MoveCountedValue::Reset();

    CircularQueue<MoveCountedValue> queue(2);

    EXPECT_TRUE(queue.TryEmplaceBack(4, 5));

    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.At(0).Value(), 45);
    EXPECT_EQ(MoveCountedValue::MoveConstructCount(), 0);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, EmplaceBackForwardsMoveOnlyArg) {
    CircularQueue<Holder> queue(2);

    queue.EmplaceBack(std::make_unique<int>(7));

    ASSERT_EQ(queue.Size(), 1u);
    ASSERT_NE(queue.Front().Value(), nullptr);
    EXPECT_EQ(*queue.Front().Value(), 7);
}

TEST(CircularQueueTest, EmplaceBackOnMovedQueueNoop) {
    CircularQueue<MoveCountedValue> source(2);
    CircularQueue<MoveCountedValue> target(std::move(source));

    source.EmplaceBack(1, 2);

    EXPECT_EQ(source.Size(), 0u);
}

TEST(CircularQueueTest, TryEmplaceBackRejectsWhenFull) {
    CircularQueue<MoveCountedValue> queue(2);

    ASSERT_TRUE(queue.TryEmplaceBack(1, 0));
    ASSERT_TRUE(queue.TryEmplaceBack(2, 0));
    EXPECT_FALSE(queue.TryEmplaceBack(3, 0));

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.At(0).Value(), 10);
    EXPECT_EQ(queue.At(1).Value(), 20);
    EXPECT_EQ(queue.OverrunCounter(), 1u);
}

TEST(CircularQueueTest, EmplaceBackCtorThrowsLeavesQueueUnchanged) {
    CircularQueue<ThrowingCtorValue> queue(4);

    queue.EmplaceBack(1);
    ASSERT_EQ(queue.Size(), 1u);

    ThrowingCtorValue::ThrowEnabled() = true;
    EXPECT_THROW(queue.EmplaceBack(2), std::runtime_error);
    ThrowingCtorValue::ThrowEnabled() = false;

    // 非满路径强保证：异常向外传播，队列状态不变。
    EXPECT_EQ(queue.Size(), 1u);
    EXPECT_EQ(queue.Front().Value(), 1);
    EXPECT_EQ(queue.OverrunCounter(), 0u);
}

TEST(CircularQueueTest, EmplaceBackCtorThrowsOnOverwriteKeepsQueueUsable) {
    CircularQueue<ThrowingCtorValue> queue(2);

    queue.EmplaceBack(1);
    queue.EmplaceBack(2);
    ASSERT_TRUE(queue.Full());

    ThrowingCtorValue::ThrowEnabled() = true;
    EXPECT_THROW(queue.EmplaceBack(3), std::runtime_error);
    ThrowingCtorValue::ThrowEnabled() = false;

    // 覆盖路径基本保证：最旧已丢、新元素未入队、size 减 1、overrun 加 1。
    EXPECT_EQ(queue.Size(), queue.Capacity() - 1);
    EXPECT_EQ(queue.Front().Value(), 2);
    EXPECT_EQ(queue.OverrunCounter(), 1u);

    queue.EmplaceBack(4);

    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.At(0).Value(), 2);
    EXPECT_EQ(queue.At(1).Value(), 4);

    queue.PopFront();

    EXPECT_EQ(queue.Front().Value(), 4);
}
