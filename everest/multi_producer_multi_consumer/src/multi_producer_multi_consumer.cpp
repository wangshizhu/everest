/**
 * @file multi_producer_multi_consumer.cpp
 * @brief MultiProducerMultiConsumer<T> 默认模式的同步与等待实现。
 */

// 本文件在统一编译单元中被 include，公共头由 common/include.h 先行包含。
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>

NAMESPACE_EVEREST_BEGIN

namespace detail {

/**
 * @brief MultiProducerMultiConsumer<T> 默认模式的同步与等待实现。
 *
 * 只操作 void*，具体类型的移动构造、移动赋值与析构通过 CircularQueueHooks 完成。
 * 线程安全：单互斥量保护内部状态，not_full_cv_ / not_empty_cv_ 分别通知生产者与消费者。
 */
class MultiProducerMultiConsumerCore final : private NonCopyable {
public:
    /**
     * @brief 构造核心，并创建底层环形队列。
     * @param capacity 可用元素数；0 归一为 1。
     * @param hooks 槽位的移动构造、移动赋值、析构回调。
     * @throw std::length_error 容量或槽位字节数无法表示时抛出。
     * @throw std::bad_alloc 分配失败时抛出。
     */
    MultiProducerMultiConsumerCore(std::size_t capacity, const CircularQueueHooks& hooks);

    /**
     * @brief 把元素移动到队尾；队列已满时阻塞等待。
     * @param item 待入队元素；成功后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     */
    bool Enqueue(void* item);

    /**
     * @brief 立即把元素移动到队尾；队列已满时覆盖最旧元素。
     * @param item 待入队元素；成功后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     */
    bool EnqueueNoWait(void* item);

    /**
     * @brief 尝试入队；队列已满或已关闭时立即返回。
     * @param item 待入队元素；仅在入队成功时被移动。
     * @return 入队成功返回 true；队列已满或已关闭返回 false，实参保持不变。
     */
    bool TryEnqueue(void* item);

    /**
     * @brief 用构造参数原地入队；队列已满时阻塞等待。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 入队成功返回 true；队列已关闭返回 false。
     */
    bool EnqueueEmplace(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 尝试用构造参数原地入队；队列已满或已关闭时立即返回。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 入队成功返回 true；队列已满或已关闭返回 false。
     */
    bool TryEnqueueEmplace(void* params, CircularQueueConstructFn construct);

    /**
     * @brief 限时取出队首元素。
     * @param popped_item 输出参数；成功时被移动赋值。
     * @param wait_duration 最长等待时间；小于等于 0 时只做一次非阻塞尝试。
     * @return 取到元素返回 true；超时或已关闭且为空返回 false。
     */
    bool DequeueFor(void* popped_item, std::chrono::milliseconds wait_duration);

    /**
     * @brief 关闭队列并唤醒全部等待线程；可重复调用。
     */
    void Close();

    /**
     * @brief 返回当前元素数。
     * @return 元素数，范围 [0, capacity_]。
     */
    std::size_t Size() const;

    /**
     * @brief 返回可用元素数。
     * @return 可用元素数。
     */
    std::size_t Capacity() const noexcept;

    /**
     * @brief 返回覆盖最旧的累计次数。
     * @return 累计覆盖次数。
     */
    std::size_t OverrunCounter() const;

private:
    // 底层环形队列；构造后始终有效。
    std::shared_ptr<CircularQueueCore> queue_;
    // 槽位的移动构造、移动赋值与析构回调。
    CircularQueueHooks hooks_{};
    // 可用元素数；构造后不变。
    std::size_t capacity_ = 0;
    // 关闭标记；为 true 后拒绝新的入队并在排空后结束出队等待。
    bool closed_ = false;
    // 保护 queue_ 的互斥量；查询方法也会加锁，因此声明为 mutable。
    mutable std::mutex mutex_;
    // 队列腾出空位或关闭时通知阻塞的生产者。
    std::condition_variable not_full_cv_;
    // 队列有新元素或关闭时通知阻塞的消费者。
    std::condition_variable not_empty_cv_;
};

MultiProducerMultiConsumerCore::MultiProducerMultiConsumerCore(std::size_t capacity, const CircularQueueHooks& hooks)
    : queue_(CircularQueueBridge::Create(capacity, hooks)), hooks_(hooks), capacity_(CircularQueueBridge::Capacity(*queue_)) {}

bool MultiProducerMultiConsumerCore::Enqueue(void* item) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] { return closed_ || !CircularQueueBridge::Full(*queue_); });

        if (closed_) {
            return false;
        }

        CircularQueueBridge::PushBack(*queue_, item);
    }

    not_empty_cv_.notify_one();
    return true;
}

bool MultiProducerMultiConsumerCore::EnqueueNoWait(void* item) {
    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        CircularQueueBridge::PushBack(*queue_, item);
    }

    not_empty_cv_.notify_one();
    return true;
}

bool MultiProducerMultiConsumerCore::TryEnqueue(void* item) {
    bool pushed = false;

    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 队列已满时直接拒绝，不经过 TryPushBack，避免把调用方的正常拒绝计入 OverrunCounter。
        if (!CircularQueueBridge::Full(*queue_)) {
            CircularQueueBridge::PushBack(*queue_, item);
            pushed = true;
        }
    }

    if (pushed) {
        not_empty_cv_.notify_one();
    }

    return pushed;
}

bool MultiProducerMultiConsumerCore::EnqueueEmplace(void* params, CircularQueueConstructFn construct) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] { return closed_ || !CircularQueueBridge::Full(*queue_); });

        if (closed_) {
            return false;
        }

        CircularQueueBridge::EmplaceBack(*queue_, params, construct);
    }

    not_empty_cv_.notify_one();
    return true;
}

bool MultiProducerMultiConsumerCore::TryEnqueueEmplace(void* params, CircularQueueConstructFn construct) {
    bool emplaced = false;

    {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // 队列已满时直接拒绝，不经过 TryEmplaceBack，避免把调用方的正常拒绝计入 OverrunCounter。
        if (!CircularQueueBridge::Full(*queue_)) {
            CircularQueueBridge::EmplaceBack(*queue_, params, construct);
            emplaced = true;
        }
    }

    if (emplaced) {
        not_empty_cv_.notify_one();
    }

    return emplaced;
}

bool MultiProducerMultiConsumerCore::DequeueFor(void* popped_item, std::chrono::milliseconds wait_duration) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait_for(lock, wait_duration, [this] { return closed_ || !CircularQueueBridge::Empty(*queue_); });

        if (CircularQueueBridge::Empty(*queue_)) {
            return false;
        }

        // 槽位可能被并发覆盖，移动赋值必须与出队在同一临界区内完成。
        hooks_.move_assign(popped_item, CircularQueueBridge::Front(*queue_));
        CircularQueueBridge::PopFront(*queue_);
    }

    not_full_cv_.notify_one();
    return true;
}

void MultiProducerMultiConsumerCore::Close() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        closed_ = true;
    }

    not_full_cv_.notify_all();
    not_empty_cv_.notify_all();
}

std::size_t MultiProducerMultiConsumerCore::Size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return CircularQueueBridge::Size(*queue_);
}

std::size_t MultiProducerMultiConsumerCore::Capacity() const noexcept {
    return capacity_;
}

std::size_t MultiProducerMultiConsumerCore::OverrunCounter() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return CircularQueueBridge::OverrunCounter(*queue_);
}

std::shared_ptr<MultiProducerMultiConsumerCore> MultiProducerMultiConsumerBridge::Create(std::size_t capacity, const CircularQueueHooks& hooks) {
    return std::make_shared<MultiProducerMultiConsumerCore>(capacity, hooks);
}

bool MultiProducerMultiConsumerBridge::Enqueue(MultiProducerMultiConsumerCore& core, void* item) {
    return core.Enqueue(item);
}

bool MultiProducerMultiConsumerBridge::EnqueueNoWait(MultiProducerMultiConsumerCore& core, void* item) {
    return core.EnqueueNoWait(item);
}

bool MultiProducerMultiConsumerBridge::TryEnqueue(MultiProducerMultiConsumerCore& core, void* item) {
    return core.TryEnqueue(item);
}

bool MultiProducerMultiConsumerBridge::EnqueueEmplace(MultiProducerMultiConsumerCore& core, void* params, CircularQueueConstructFn construct) {
    return core.EnqueueEmplace(params, construct);
}

bool MultiProducerMultiConsumerBridge::TryEnqueueEmplace(MultiProducerMultiConsumerCore& core, void* params, CircularQueueConstructFn construct) {
    return core.TryEnqueueEmplace(params, construct);
}

bool MultiProducerMultiConsumerBridge::DequeueFor(MultiProducerMultiConsumerCore& core, void* popped_item, std::chrono::milliseconds wait_duration) {
    return core.DequeueFor(popped_item, wait_duration);
}

void MultiProducerMultiConsumerBridge::Close(MultiProducerMultiConsumerCore& core) {
    core.Close();
}

std::size_t MultiProducerMultiConsumerBridge::Size(const MultiProducerMultiConsumerCore& core) {
    return core.Size();
}

std::size_t MultiProducerMultiConsumerBridge::Capacity(const MultiProducerMultiConsumerCore& core) noexcept {
    return core.Capacity();
}

std::size_t MultiProducerMultiConsumerBridge::OverrunCounter(const MultiProducerMultiConsumerCore& core) {
    return core.OverrunCounter();
}

}

NAMESPACE_EVEREST_END
