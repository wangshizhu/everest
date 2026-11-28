/**
 * @file lru.cpp
 * @brief LruBase 与 LruManager 的非模板实现。
 */

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

NAMESPACE_EVEREST_BEGIN

LruBase::LruBase(Duration ttl) : ttl_(ttl) {}

LruBase::~LruBase() = default;

LruManager::LruManager() : next_update_time_point_(std::chrono::steady_clock::now() + kSweepInterval) {}

LruManager::~LruManager() = default;

LruManager* LruManager::GetInstance() {
    static LruManager instance;
    return &instance;
}

void LruManager::RegisterLruList(const LruBaseSharedPtr& cache) {
    if (cache == nullptr) {
        return;
    }

    // 顺带清理已销毁缓存的登记，避免登记表随缓存创建/销毁持续增长。
    auto iter = caches_.begin();
    while (iter != caches_.end()) {
        if (iter->expired()) {
            iter = caches_.erase(iter);
            continue;
        }

        ++iter;
    }

    caches_.push_back(cache);
}

void LruManager::Update() {
    auto now = std::chrono::steady_clock::now();
    if (now <= next_update_time_point_) {
        return;
    }

    next_update_time_point_ = now + kSweepInterval;

    std::vector<LruBaseSharedPtr> snapshot;
    auto iter = caches_.begin();
    while (iter != caches_.end()) {
        auto cache = iter->lock();
        if (cache == nullptr) {
            iter = caches_.erase(iter);
            continue;
        }

        snapshot.push_back(std::move(cache));
        ++iter;
    }

    // 遍历完成后统一清扫：值的析构里登记缓存不会破坏上面的遍历。
    for (const auto& cache : snapshot) {
        cache->Update();
    }
}

NAMESPACE_EVEREST_END
