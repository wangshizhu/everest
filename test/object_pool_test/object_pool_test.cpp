/**
 * @file object_pool_test.cpp
 * @brief ObjectPool<T> 的 GoogleTest 用例。
 */

#include "object_pool/object_pool.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

/**
 * @brief 记录构造、Clear、析构次数的可池化测试类型。
 *
 * 计数器与回调是进程级静态状态，测试开始前必须调用 Reset()。
 */
class TrackedObject final {
public:
    /**
     * @brief 构造对象并累加构造计数。
     */
    TrackedObject() {
        construct_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief 析构对象，累加析构计数并执行析构回调。
     */
    ~TrackedObject() {
        destruct_count_.fetch_add(1, std::memory_order_relaxed);
        if (destruct_hook_) {
            destruct_hook_();
        }
    }

    /**
     * @brief 复位对象，累加 Clear 计数并执行 Clear 回调。
     */
    void Clear() {
        clear_count_.fetch_add(1, std::memory_order_relaxed);
        value = 0;
        if (clear_hook_) {
            clear_hook_();
        }
    }

    /**
     * @brief 重置计数器与回调。
     */
    static void Reset() {
        construct_count_.store(0, std::memory_order_relaxed);
        destruct_count_.store(0, std::memory_order_relaxed);
        clear_count_.store(0, std::memory_order_relaxed);
        clear_hook_ = nullptr;
        destruct_hook_ = nullptr;
    }

    /**
     * @brief 获取构造次数。
     * @return 构造次数。
     */
    static std::int64_t ConstructCount() {
        return construct_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取析构次数。
     * @return 析构次数。
     */
    static std::int64_t DestructCount() {
        return destruct_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取 Clear 次数。
     * @return Clear 次数。
     */
    static std::int64_t ClearCount() {
        return clear_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 设置 Clear 内执行的回调。
     * @param hook 回调；nullptr 表示不执行。
     */
    static void SetClearHook(void (*hook)()) {
        clear_hook_ = hook;
    }

    /**
     * @brief 设置析构内执行的回调。
     * @param hook 回调；nullptr 表示不执行。
     */
    static void SetDestructHook(void (*hook)()) {
        destruct_hook_ = hook;
    }

    // 携带的测试值；Clear() 会将其清零。
    int value = 0;

private:
    // 构造次数。
    static std::atomic<std::int64_t> construct_count_;
    // 析构次数。
    static std::atomic<std::int64_t> destruct_count_;
    // Clear 次数。
    static std::atomic<std::int64_t> clear_count_;
    // Clear 内执行的回调；nullptr 表示不执行。
    static void (*clear_hook_)();
    // 析构内执行的回调；nullptr 表示不执行。
    static void (*destruct_hook_)();
};

std::atomic<std::int64_t> TrackedObject::construct_count_{0};
std::atomic<std::int64_t> TrackedObject::destruct_count_{0};
std::atomic<std::int64_t> TrackedObject::clear_count_{0};
void (*TrackedObject::clear_hook_)() = nullptr;
void (*TrackedObject::destruct_hook_)() = nullptr;

// 测试专用的对象池类型。
using TrackedPool = everest::ObjectPool<TrackedObject>;

// hook 要调用 Shrink 的测试池；仅在设置 hook 的单线程用例中使用。
static TrackedPool* g_hook_pool = nullptr;

/**
 * @brief 测试 hook：对 g_hook_pool 执行 Shrink。
 */
static void ShrinkFromHook() {
    if (g_hook_pool != nullptr) {
        g_hook_pool->Shrink();
    }
}

/**
 * @brief 统计当前存活对象数量。
 * @return 构造次数减析构次数。
 */
static std::int64_t AliveCount() {
    return TrackedObject::ConstructCount() - TrackedObject::DestructCount();
}

/**
 * @brief 在工作线程中释放共享租约。
 * @param lease 待释放的共享租约。
 */
static void ReleaseLease(std::shared_ptr<TrackedObject> lease) {
    lease.reset();
}

TEST(ObjectPoolTest, ConstructDoesNotAllocate) {
    TrackedObject::Reset();

    TrackedPool pool(4);

    EXPECT_EQ(TrackedObject::ConstructCount(), 0);
    EXPECT_EQ(TrackedObject::DestructCount(), 0);
}

TEST(ObjectPoolTest, FirstApplyExpandsOneStep) {
    TrackedObject::Reset();
    TrackedPool pool(4);

    auto lease = pool.ApplySharedObject();

    EXPECT_NE(lease.get(), nullptr);
    EXPECT_EQ(TrackedObject::ConstructCount(), 4);
}

TEST(ObjectPoolTest, ApplyReturnsClearedObject) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    TrackedObject* first = nullptr;
    {
        auto lease = pool.ApplySharedObject();
        first = lease.get();
        first->value = 7;
    }

    auto again = pool.ApplySharedObject();

    EXPECT_EQ(again.get(), first);
    EXPECT_EQ(again->value, 0);
    EXPECT_EQ(TrackedObject::ClearCount(), 2);
}

TEST(ObjectPoolTest, ReuseDoesNotConstruct) {
    TrackedObject::Reset();
    TrackedPool pool(4);

    for (int index = 0; index < 4; ++index) {
        auto lease = pool.ApplySharedObject();
        EXPECT_NE(lease.get(), nullptr);
    }

    EXPECT_EQ(TrackedObject::ConstructCount(), 4);
}

TEST(ObjectPoolTest, ExpandsWhenExhausted) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    auto first = pool.ApplySharedObject();
    auto second = pool.ApplySharedObject();
    auto third = pool.ApplySharedObject();

    EXPECT_NE(first.get(), nullptr);
    EXPECT_NE(second.get(), nullptr);
    EXPECT_NE(third.get(), nullptr);
    EXPECT_EQ(TrackedObject::ConstructCount(), 4);
}

TEST(ObjectPoolTest, SharedLeaseReturnsOnRelease) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    TrackedObject* raw = nullptr;
    {
        auto lease = pool.ApplySharedObject();
        raw = lease.get();
    }

    auto again = pool.ApplySharedObject();

    EXPECT_EQ(again.get(), raw);
    EXPECT_EQ(TrackedObject::ConstructCount(), 2);
}

TEST(ObjectPoolTest, UniqueLeaseReturnsOnReset) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    TrackedObject* raw = nullptr;
    {
        auto lease = pool.ApplyUniqueObject();
        raw = lease.get();
        lease.reset();
    }

    auto again = pool.ApplyUniqueObject();

    EXPECT_EQ(again.get(), raw);
    EXPECT_EQ(TrackedObject::ConstructCount(), 2);
}

TEST(ObjectPoolTest, PoolDestructionReleasesAll) {
    TrackedObject::Reset();
    std::int64_t constructed = 0;

    {
        TrackedPool pool(2);
        {
            auto first = pool.ApplySharedObject();
            auto second = pool.ApplySharedObject();
            auto third = pool.ApplySharedObject();
        }
        constructed = TrackedObject::ConstructCount();
    }

    EXPECT_EQ(constructed, TrackedObject::DestructCount());
}

TEST(ObjectPoolTest, UniqueObjectTypeIsNameable) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    TrackedPool::UniqueObject lease = pool.ApplyUniqueObject();
    EXPECT_NE(lease.get(), nullptr);

    lease.reset();
    EXPECT_EQ(TrackedObject::DestructCount(), 0);
}

TEST(ObjectPoolTest, NotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<TrackedPool>);
    static_assert(!std::is_copy_assignable_v<TrackedPool>);
    static_assert(!std::is_move_constructible_v<TrackedPool>);
    static_assert(!std::is_move_assignable_v<TrackedPool>);

    SUCCEED();
}

TEST(ObjectPoolTest, ConstructorIsExplicit) {
    static_assert(!std::is_convertible_v<std::uint32_t, TrackedPool>);

    SUCCEED();
}

TEST(ObjectPoolTest, DeleterIsNoexcept) {
    static_assert(noexcept(std::declval<TrackedPool::Deleter&>()(std::declval<TrackedObject*>())));

    SUCCEED();
}

TEST(ObjectPoolTest, HeaderIsSelfContained) {
    TrackedObject::Reset();
    TrackedPool pool(1);

    auto lease = pool.ApplySharedObject();

    EXPECT_NE(lease.get(), nullptr);
}

TEST(ObjectPoolTest, ZeroExpandStepClamped) {
    TrackedObject::Reset();
    TrackedPool pool(0);

    auto lease = pool.ApplySharedObject();

    EXPECT_NE(lease.get(), nullptr);
    EXPECT_EQ(TrackedObject::ConstructCount(), static_cast<std::int64_t>(TrackedPool::kDefaultExpandStep));
}

TEST(ObjectPoolTest, ApplyNeverReturnsNull) {
    TrackedObject::Reset();
    TrackedPool pool(1);

    for (int index = 0; index < 64; ++index) {
        auto lease = pool.ApplySharedObject();
        EXPECT_NE(lease.get(), nullptr);
    }
}

TEST(ObjectPoolTest, MaxFreeZeroMeansUnlimited) {
    TrackedObject::Reset();
    TrackedPool pool(4, 0);

    std::vector<TrackedPool::SharedObject> leases;
    for (int index = 0; index < 32; ++index) {
        leases.push_back(pool.ApplySharedObject());
    }

    leases.clear();

    EXPECT_EQ(TrackedObject::DestructCount(), 0);
    EXPECT_EQ(AliveCount(), 32);
}

TEST(ObjectPoolTest, MaxFreeCapsOnRelease) {
    TrackedObject::Reset();
    TrackedPool pool(4, 2);

    std::vector<TrackedPool::SharedObject> leases;
    for (int index = 0; index < 4; ++index) {
        leases.push_back(pool.ApplySharedObject());
    }
    EXPECT_EQ(TrackedObject::ConstructCount(), 4);

    leases.clear();

    EXPECT_EQ(TrackedObject::DestructCount(), 2);
    EXPECT_EQ(AliveCount(), 2);
}

TEST(ObjectPoolTest, ShrinkKeepsExpandStep) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    std::vector<TrackedPool::SharedObject> leases;
    for (int index = 0; index < 5; ++index) {
        leases.push_back(pool.ApplySharedObject());
    }
    EXPECT_EQ(TrackedObject::ConstructCount(), 6);

    leases.clear();
    pool.Shrink();

    EXPECT_EQ(TrackedObject::DestructCount(), 4);
    EXPECT_EQ(AliveCount(), 2);
}

TEST(ObjectPoolTest, ShrinkTargetCappedByMaxFree) {
    TrackedObject::Reset();
    TrackedPool pool(4, 1);

    std::vector<TrackedPool::SharedObject> leases;
    for (int index = 0; index < 4; ++index) {
        leases.push_back(pool.ApplySharedObject());
    }
    leases.clear();

    pool.Shrink();

    EXPECT_EQ(AliveCount(), 1);
}

TEST(ObjectPoolTest, ShrinkOnEmptyPoolNoop) {
    TrackedObject::Reset();
    TrackedPool pool(4);

    pool.Shrink();

    EXPECT_EQ(TrackedObject::ConstructCount(), 0);
    EXPECT_EQ(TrackedObject::DestructCount(), 0);
}

TEST(ObjectPoolTest, ShrinkWithOutstandingLeases) {
    TrackedObject::Reset();
    TrackedPool pool(2);

    std::vector<TrackedPool::SharedObject> leases;
    for (int index = 0; index < 6; ++index) {
        leases.push_back(pool.ApplySharedObject());
    }
    ASSERT_EQ(leases.size(), 6);
    EXPECT_EQ(TrackedObject::ConstructCount(), 6);

    for (int index = 0; index < 4; ++index) {
        leases[index].reset();
    }

    pool.Shrink();

    EXPECT_EQ(TrackedObject::DestructCount(), 2);
    EXPECT_EQ(AliveCount(), 4);
    ASSERT_NE(leases[4].get(), nullptr);
    ASSERT_NE(leases[5].get(), nullptr);
    leases[4]->value = 3;
    EXPECT_EQ(leases[4]->value, 3);
}

TEST(ObjectPoolTest, LeaseOutlivesPool) {
    TrackedObject::Reset();
    std::shared_ptr<TrackedObject> lease;

    {
        TrackedPool pool(2);
        lease = pool.ApplySharedObject();
    }

    ASSERT_NE(lease.get(), nullptr);
    lease->value = 9;
    EXPECT_EQ(lease->value, 9);

    lease.reset();

    EXPECT_EQ(TrackedObject::ConstructCount(), 2);
    EXPECT_EQ(TrackedObject::DestructCount(), 2);
}

TEST(ObjectPoolTest, UniqueLeaseOutlivesPool) {
    TrackedObject::Reset();
    std::optional<TrackedPool::UniqueObject> lease;

    {
        TrackedPool pool(2);
        lease.emplace(pool.ApplyUniqueObject());
    }

    ASSERT_TRUE(lease.has_value());

    lease.reset();

    EXPECT_EQ(TrackedObject::ConstructCount(), 2);
    EXPECT_EQ(TrackedObject::DestructCount(), 2);
}

TEST(ObjectPoolTest, LeaseReleasedOnOtherThread) {
    TrackedObject::Reset();
    TrackedPool pool(4);

    TrackedObject* raw = nullptr;
    {
        auto lease = pool.ApplySharedObject();
        raw = lease.get();
        std::thread worker(ReleaseLease, std::move(lease));
        worker.join();
    }

    auto again = pool.ApplySharedObject();

    EXPECT_EQ(again.get(), raw);
    EXPECT_EQ(TrackedObject::ConstructCount(), 4);
}

TEST(ObjectPoolTest, ConcurrentApplyRelease) {
    TrackedObject::Reset();
    std::atomic<bool> all_non_null{true};

    {
        TrackedPool pool(8);
        std::vector<std::thread> workers;
        for (int index = 0; index < 4; ++index) {
            workers.emplace_back([&pool, &all_non_null]() {
                for (int round = 0; round < 200; ++round) {
                    auto lease = pool.ApplySharedObject();
                    if (lease.get() == nullptr) {
                        all_non_null.store(false, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    EXPECT_TRUE(all_non_null.load(std::memory_order_relaxed));
    EXPECT_EQ(TrackedObject::ConstructCount(), TrackedObject::DestructCount());
}

TEST(ObjectPoolTest, ConcurrentWithMaxFree) {
    TrackedObject::Reset();
    std::atomic<bool> all_non_null{true};
    std::int64_t alive_after_join = 0;

    {
        TrackedPool pool(8, 4);
        std::vector<std::thread> workers;
        for (int index = 0; index < 4; ++index) {
            workers.emplace_back([&pool, &all_non_null]() {
                for (int round = 0; round < 200; ++round) {
                    auto lease = pool.ApplySharedObject();
                    if (lease.get() == nullptr) {
                        all_non_null.store(false, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
        alive_after_join = AliveCount();
    }

    EXPECT_TRUE(all_non_null.load(std::memory_order_relaxed));
    EXPECT_LE(alive_after_join, 4);
    EXPECT_EQ(TrackedObject::ConstructCount(), TrackedObject::DestructCount());
}

TEST(ObjectPoolTest, ClearInvokesShrinkWithoutDeadlock) {
    TrackedObject::Reset();
    TrackedPool pool(4);
    g_hook_pool = &pool;
    TrackedObject::SetClearHook(ShrinkFromHook);

    {
        auto lease = pool.ApplySharedObject();
        EXPECT_NE(lease.get(), nullptr);
    }

    TrackedObject::SetClearHook(nullptr);
    g_hook_pool = nullptr;

    EXPECT_EQ(TrackedObject::ClearCount(), 1);
}

TEST(ObjectPoolTest, DestructorInvokesShrinkOnRelease) {
    TrackedObject::Reset();
    TrackedPool pool(2, 2);
    g_hook_pool = &pool;
    TrackedObject::SetDestructHook(ShrinkFromHook);

    {
        std::vector<TrackedPool::SharedObject> leases;
        for (int index = 0; index < 3; ++index) {
            leases.push_back(pool.ApplySharedObject());
        }
        leases.clear();
    }

    TrackedObject::SetDestructHook(nullptr);
    g_hook_pool = nullptr;

    EXPECT_EQ(TrackedObject::DestructCount(), 2);
}
