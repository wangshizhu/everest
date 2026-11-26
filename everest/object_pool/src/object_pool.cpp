/**
 * @file object_pool.cpp
 * @brief ObjectPool<T> 的存储与并发控制实现。
 */

// 本文件在统一编译单元中被 include，公共头由 common/include.h 先行包含。
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

NAMESPACE_EVEREST_BEGIN

namespace detail {

/**
 * @brief ObjectPool<T> 的存储与并发控制实现。
 *
 * 只操作 void*，具体类型的构造、复位、析构通过 ObjectPoolHooks 完成。
 * 线程安全：Apply()、Release()、Shrink() 可以在任意线程并发调用；
 * 析构函数只有在没有其他持有者时才会执行。
 */
class ObjectPoolCore final : private NonCopyable {
public:
    /**
     * @brief 构造池存储。
     * @param expand_step 每次扩容构造的对象数量；调用方保证非 0。
     * @param max_free 空闲对象上限；0 表示不限制。
     * @param hooks 对象构造、复位、析构回调。
     */
    ObjectPoolCore(uint32_t expand_step, uint32_t max_free, const ObjectPoolHooks& hooks);

    /**
     * @brief 销毁池存储，并销毁仍留在空闲链与在用集合中的对象。
     */
    ~ObjectPoolCore();

    /**
     * @brief 取一个已复位的空闲对象。
     * @return 非空对象指针；空闲链为空时先扩容。
     */
    void* Apply();

    /**
     * @brief 归还或销毁对象。
     * @param object 待归还的对象；不属于本池时记录错误并丢弃。
     * @note 声明为 noexcept；不抛异常。
     */
    void Release(void* object) noexcept;

    /**
     * @brief 把空闲对象收缩到 expand_step，若 max_free 非 0 且更小则收缩到 max_free。
     */
    void Shrink();

private:
    /**
     * @brief 批量构造的异常安全守卫：未 Commit 时销毁已构造的对象。
     */
    class BatchGuard final : private NonCopyable {
    public:
        /**
         * @brief 绑定待保护的批次。
         * @param batch 已构造对象的批次。
         * @param hooks 对象析构回调。
         */
        BatchGuard(std::vector<void*>& batch, const ObjectPoolHooks& hooks) noexcept : batch_(batch), hooks_(hooks) {}

        ~BatchGuard() {
            if (committed_) {
                return;
            }

            for (void* object : batch_) {
                hooks_.destroy(object);
            }
        }

        /**
         * @brief 标记批次所有权已转移，析构时不再销毁对象。
         */
        void Commit() noexcept {
            committed_ = true;
        }

    private:
        // 受保护的批次。
        std::vector<void*>& batch_;
        // 对象析构回调。
        const ObjectPoolHooks& hooks_;
        // 批次所有权是否已转移。
        bool committed_ = false;
    };

    /**
     * @brief 从空闲链取出一个对象并移入在用集合。
     * @return 取到的对象；空闲链为空时返回 nullptr。
     */
    void* TakeFree();

    /**
     * @brief 锁外批量构造对象，锁内并入空闲链并取出一个对象。
     * @return 非空对象指针。
     */
    void* ExpandAndTake();

    // 保护空闲链与在用集合的互斥锁。
    std::mutex mutex_;
    // 每次扩容构造的对象数量。
    uint32_t expand_step_;
    // 空闲对象上限；0 表示不限制。
    uint32_t max_free_;
    // 对象构造、复位、析构回调。
    ObjectPoolHooks hooks_;
    // 空闲对象；只能通过 mutex_ 访问。
    std::vector<void*> free_;
    // 在用对象；只能通过 mutex_ 访问。
    std::unordered_set<void*> used_;
};

ObjectPoolCore::ObjectPoolCore(uint32_t expand_step, uint32_t max_free, const ObjectPoolHooks& hooks) : expand_step_(expand_step), max_free_(max_free), hooks_(hooks) {}

ObjectPoolCore::~ObjectPoolCore() {
    for (void* object : free_) {
        hooks_.destroy(object);
    }
    free_.clear();

    for (void* object : used_) {
        hooks_.destroy(object);
    }
    used_.clear();
}

void* ObjectPoolCore::Apply() {
    void* object = TakeFree();
    if (object == nullptr) {
        object = ExpandAndTake();
    }

    hooks_.clear(object);
    return object;
}

void ObjectPoolCore::Release(void* object) noexcept {
    if (object == nullptr) {
        return;
    }

    void* object_to_destroy = nullptr;

    {
        std::lock_guard<std::mutex> guard(mutex_);

        const auto used_iter = used_.find(object);
        if (used_iter == used_.end()) {
            EVEREST_LOG_ERROR("ObjectPoolCore.Release ignored unowned object");
            return;
        }

        used_.erase(used_iter);

        if ((max_free_ != 0) && (free_.size() >= max_free_)) {
            object_to_destroy = object;
        } else {
            // 扩容时已把 free_ 容量预留到存活对象总数，这里不会触发重新分配。
            free_.push_back(object);
        }
    }

    if (object_to_destroy != nullptr) {
        hooks_.destroy(object_to_destroy);
    }
}

void ObjectPoolCore::Shrink() {
    uint32_t target = expand_step_;
    if ((max_free_ != 0) && (max_free_ < target)) {
        target = max_free_;
    }

    std::vector<void*> removed;

    {
        std::lock_guard<std::mutex> guard(mutex_);

        const std::size_t target_size = target;
        if (free_.size() <= target_size) {
            return;
        }

        removed.reserve(free_.size() - target_size);
        while (free_.size() > target_size) {
            removed.push_back(free_.back());
            free_.pop_back();
        }
    }

    for (void* object : removed) {
        hooks_.destroy(object);
    }
}

void* ObjectPoolCore::TakeFree() {
    std::lock_guard<std::mutex> guard(mutex_);

    if (free_.empty()) {
        return nullptr;
    }

    void* object = free_.back();
    used_.insert(object);
    free_.pop_back();
    return object;
}

void* ObjectPoolCore::ExpandAndTake() {
    std::vector<void*> batch;
    batch.reserve(expand_step_);

    BatchGuard guard(batch, hooks_);
    for (uint32_t index = 0; index < expand_step_; ++index) {
        batch.push_back(hooks_.allocate());
    }

    std::lock_guard<std::mutex> lock(mutex_);
    free_.reserve(free_.size() + batch.size() + used_.size());
    free_.insert(free_.end(), batch.begin(), batch.end());
    guard.Commit();

    void* object = free_.back();
    used_.insert(object);
    free_.pop_back();
    return object;
}

std::shared_ptr<ObjectPoolCore> ObjectPoolBridge::Create(uint32_t expand_step, uint32_t max_free, const ObjectPoolHooks& hooks) {
    return std::make_shared<ObjectPoolCore>(expand_step, max_free, hooks);
}

void* ObjectPoolBridge::Apply(ObjectPoolCore& core) {
    return core.Apply();
}

void ObjectPoolBridge::Release(ObjectPoolCore& core, void* object) noexcept {
    core.Release(object);
}

void ObjectPoolBridge::Shrink(ObjectPoolCore& core) {
    core.Shrink();
}

}

NAMESPACE_EVEREST_END
