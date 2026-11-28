/**
 * @file lru_test.cpp
 * @brief lru 组件的 GoogleTest 用例。
 */

#include "lru/lru.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

// 测试使用的整数缓存类型。
using IntCache = everest::LruList<int, std::shared_ptr<int>>;

/**
 * @brief 构造一个整数缓存值。
 * @param value 负载值。
 * @return 新建的共享指针。
 */
std::shared_ptr<int> MakeInt(int value) {
    return std::make_shared<int>(value);
}

/**
 * @brief 记录存活对象数量的测试值对象。
 */
class TrackedValue final {
public:
    /**
     * @brief 构造对象并增加存活计数。
     */
    TrackedValue() {
        ++live_count_;
    }

    /**
     * @brief 销毁对象并减少存活计数。
     */
    ~TrackedValue() {
        --live_count_;
    }

    /**
     * @brief 取当前存活对象数量。
     * @return 存活对象数量。
     */
    static int LiveCount() {
        return live_count_.load();
    }

    /**
     * @brief 重置存活对象数量。
     */
    static void Reset() {
        live_count_.store(0);
    }

private:
    // 当前存活对象数量。
    inline static std::atomic<int> live_count_{0};
};

// 测试使用的受监控缓存类型。
using TrackedCache = everest::LruList<int, std::shared_ptr<TrackedValue>>;

/**
 * @brief 把单个字符折叠为小写。
 * @param ch 待折叠字符。
 * @return ASCII 大写转小写，其他字符原样返回。
 */
char FoldAscii(char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

/**
 * @brief 大小写不敏感的字符串键。
 *
 * LruList 只把哈希策略参数化，因此大小写不敏感需要键类型自身提供忽略大小写的
 * 相等比较，配合忽略大小写的哈希策略使用。
 */
class CaseInsensitiveKey final {
public:
    /**
     * @brief 构造键。
     * @param text 键文本。
     */
    explicit CaseInsensitiveKey(std::string text) : text_(std::move(text)) {}

    /**
     * @brief 忽略 ASCII 大小写比较两个键。
     * @param other 另一个键。
     * @return 忽略大小写后相同返回 true。
     */
    bool operator==(const CaseInsensitiveKey& other) const {
        if (text_.size() != other.text_.size()) {
            return false;
        }

        for (std::size_t index = 0; index < text_.size(); ++index) {
            if (FoldAscii(text_[index]) != FoldAscii(other.text_[index])) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 计算忽略 ASCII 大小写的哈希值。
     * @return 哈希值。
     */
    std::size_t Hash() const {
        std::size_t result = 0;
        for (char ch : text_) {
            result = (result * 131U) + static_cast<std::size_t>(FoldAscii(ch));
        }

        return result;
    }

private:
    // 键文本。
    std::string text_;
};

/**
 * @brief 大小写不敏感的哈希策略。
 */
class CaseInsensitiveKeyHash final {
public:
    /**
     * @brief 计算键的哈希值。
     * @param key 键。
     * @return 忽略 ASCII 大小写的哈希值。
     */
    std::size_t operator()(const CaseInsensitiveKey& key) const {
        return key.Hash();
    }
};

TEST(LruTest, PutThenGetReturnsValue) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));

    cache->Put(1, MakeInt(42));
    std::shared_ptr<int> value = cache->Get(1);

    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
}

TEST(LruTest, GetMissingReturnsNullptr) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));

    std::shared_ptr<int> value = cache->Get(7);

    EXPECT_EQ(value, nullptr);
    EXPECT_EQ(cache->Size(), std::size_t{0});
}

TEST(LruTest, PutOverwritesValue) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));

    cache->Put(1, MakeInt(10));
    cache->Put(1, MakeInt(20));

    std::shared_ptr<int> value = cache->Get(1);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 20);
    EXPECT_EQ(cache->Size(), std::size_t{1});
}

TEST(LruTest, EraseRemovesEntry) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));
    cache->Put(1, MakeInt(1));

    EXPECT_TRUE(cache->Erase(1));
    EXPECT_FALSE(cache->Erase(1));
    EXPECT_EQ(cache->Get(1), nullptr);
}

TEST(LruTest, SizeReportsEntryCount) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));

    cache->Put(1, MakeInt(1));
    cache->Put(2, MakeInt(2));
    cache->Put(3, MakeInt(3));

    EXPECT_EQ(cache->Size(), std::size_t{3});
}

TEST(LruTest, GetRefreshesTtl) {
    auto cache = IntCache::MakeLruList(std::chrono::seconds(1));
    cache->Put(1, MakeInt(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_NE(cache->Get(1), nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_NE(cache->Get(1), nullptr);
}

TEST(LruTest, UpdateEvictsOnlyExpired) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(400));

    cache->Put(1, MakeInt(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    cache->Put(2, MakeInt(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    cache->Update();

    EXPECT_EQ(cache->Size(), std::size_t{1});
    EXPECT_EQ(cache->Get(1), nullptr);
    EXPECT_NE(cache->Get(2), nullptr);
}

TEST(LruTest, ManagerUpdateSweepsRegisteredCache) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
    cache->Put(1, MakeInt(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    everest::LruManager::GetInstance()->Update();

    EXPECT_EQ(cache->Size(), std::size_t{0});
}

TEST(LruTest, PublicUpdateOverride) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
    cache->Put(1, MakeInt(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    everest::LruBaseSharedPtr base = cache;
    base->Update();

    EXPECT_EQ(cache->Size(), std::size_t{0});
}

TEST(LruTest, CustomHash) {
    using CustomCache = everest::LruList<CaseInsensitiveKey, std::shared_ptr<int>, CaseInsensitiveKeyHash>;
    auto cache = CustomCache::MakeLruList(std::chrono::hours(1));

    cache->Put(CaseInsensitiveKey("Key"), MakeInt(42));
    std::shared_ptr<int> value = cache->Get(CaseInsensitiveKey("key"));

    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(cache->Size(), std::size_t{1});
}

TEST(LruTest, DestructionReleasesValues) {
    TrackedValue::Reset();

    {
        auto cache = TrackedCache::MakeLruList(std::chrono::hours(1));
        cache->Put(1, std::make_shared<TrackedValue>());
        cache->Put(2, std::make_shared<TrackedValue>());
        cache->Put(3, std::make_shared<TrackedValue>());

        EXPECT_EQ(TrackedValue::LiveCount(), 3);
    }

    EXPECT_EQ(TrackedValue::LiveCount(), 0);
}

TEST(LruTest, NotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<IntCache>);
    static_assert(!std::is_move_constructible_v<IntCache>);
    static_assert(!std::is_copy_assignable_v<IntCache>);
    static_assert(!std::is_move_assignable_v<IntCache>);
    static_assert(!std::is_copy_constructible_v<everest::LruManager>);
    SUCCEED();
}

TEST(LruTest, ManagerIsSingleInstance) {
    everest::LruManager* first = everest::LruManager::GetInstance();
    everest::LruManager* second = everest::LruManager::GetInstance();

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
}

TEST(LruTest, HeaderIsSelfContained) {
    // 本文件首行只包含 "lru/lru.h"，能编译即证明头文件自足。
    EXPECT_TRUE(std::is_class_v<IntCache>);
}

TEST(LruTest, GetExpiredWithoutUpdate) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
    cache->Put(1, MakeInt(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(cache->Get(1), nullptr);
}

TEST(LruTest, GetExpiredErasesEntry) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
    cache->Put(1, MakeInt(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(cache->Get(1), nullptr);
    EXPECT_EQ(cache->Size(), std::size_t{0});
}

TEST(LruTest, ExpiredAfterTtl) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
    cache->Put(1, MakeInt(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(cache->Get(1), nullptr);
}

TEST(LruTest, NotExpiredBeforeTtl) {
    auto cache = IntCache::MakeLruList(std::chrono::seconds(10));
    cache->Put(1, MakeInt(1));

    EXPECT_NE(cache->Get(1), nullptr);
}

TEST(LruTest, ZeroTtlExpiresImmediately) {
    auto cache = IntCache::MakeLruList(std::chrono::milliseconds(0));
    cache->Put(1, MakeInt(1));

    EXPECT_EQ(cache->Get(1), nullptr);
}

TEST(LruTest, NegativeTtlExpiresImmediately) {
    auto cache = IntCache::MakeLruList(std::chrono::seconds(-1));
    cache->Put(1, MakeInt(1));

    EXPECT_EQ(cache->Get(1), nullptr);
}

TEST(LruTest, EraseMissingReturnsFalse) {
    auto cache = IntCache::MakeLruList(std::chrono::hours(1));

    EXPECT_FALSE(cache->Erase(1));
}

TEST(LruTest, ManagerRegisterNullIgnored) {
    everest::LruManager::GetInstance()->RegisterLruList(nullptr);

    EXPECT_NO_THROW(everest::LruManager::GetInstance()->Update());
}

TEST(LruTest, ManagerUpdateAfterCacheDestroyed) {
    {
        auto cache = IntCache::MakeLruList(std::chrono::milliseconds(10));
        cache->Put(1, MakeInt(1));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    EXPECT_NO_THROW(everest::LruManager::GetInstance()->Update());

    auto cache = IntCache::MakeLruList(std::chrono::hours(1));
    cache->Put(2, MakeInt(2));
    everest::LruManager::GetInstance()->Update();

    EXPECT_EQ(cache->Size(), std::size_t{1});
}

}
