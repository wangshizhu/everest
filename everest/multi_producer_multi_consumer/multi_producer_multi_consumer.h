/**
 * @file multi_producer_multi_consumer.h
 * @brief 多生产者多消费者队列：编译期选择有界阻塞实现或无锁实现。
 */

#ifndef MULTI_PRODUCER_MULTI_CONSUMER_H_
#define MULTI_PRODUCER_MULTI_CONSUMER_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef USE_LOCK_FREE_QUEUE
#include <lock-free-queue/blockingconcurrentqueue.h>
#endif

#include "common/define.h"
#include "multi_producer_multi_consumer/circular_queue.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

// 队列的模板胶水与非模板实现细节。
namespace detail {

// 默认模式的同步与等待实现；定义在 multi_producer_multi_consumer.cpp。
class MultiProducerMultiConsumerCore;

/**
 * @brief 把模板胶水与非模板实现连接起来。
 *
 * 类本身不持有状态，只提供静态转发入口；仅供 MultiProducerMultiConsumer<T> 使用。
 */
class MultiProducerMultiConsumerBridge final {
public:
    MultiProducerMultiConsumerBridge() = delete;

    /**
     * @brief 创建默认模式核心。
     * @param capacity 可用元素数；0 归一为 1。
     * @param hooks 槽位的移动构造、移动赋值、析构回调。
     * @return 核心的共享所有权。
     * @throw std::length_error 容量或槽位字节数无法表示时抛出。
     * @throw std::bad_alloc 分配失败时抛出。
     */
    static std::shared_ptr<MultiProducerMultiConsumerCore> Create(std::size_t capacity, const CircularQueueHooks& hooks);

    /**
     * @brief 把元素移动到队尾；队列已满时阻塞等待。
     * @param core 队列核心。
     * @param item 待入队元素；成功后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     */
    static bool Enqueue(MultiProducerMultiConsumerCore& core, void* item);

    /**
     * @brief 立即把元素移动到队尾；队列已满时覆盖最旧元素。
     * @param core 队列核心。
     * @param item 待入队元素；成功后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     */
    static bool EnqueueNoWait(MultiProducerMultiConsumerCore& core, void* item);

    /**
     * @brief 尝试入队；队列已满或已关闭时立即返回。
     * @param core 队列核心。
     * @param item 待入队元素；仅在入队成功时被移动。
     * @return 入队成功返回 true；队列已满或已关闭返回 false，实参保持不变。
     */
    static bool TryEnqueue(MultiProducerMultiConsumerCore& core, void* item);

    /**
     * @brief 用构造参数原地入队；队列已满时阻塞等待。
     * @param core 队列核心。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 入队成功返回 true；队列已关闭返回 false。
     */
    static bool EnqueueEmplace(MultiProducerMultiConsumerCore& core, void* params, CircularQueueConstructFn construct);

    /**
     * @brief 尝试用构造参数原地入队；队列已满或已关闭时立即返回。
     * @param core 队列核心。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 入队成功返回 true；队列已满或已关闭返回 false。
     */
    static bool TryEnqueueEmplace(MultiProducerMultiConsumerCore& core, void* params, CircularQueueConstructFn construct);

    /**
     * @brief 限时取出队首元素。
     * @param core 队列核心。
     * @param popped_item 输出参数；成功时被移动赋值。
     * @param wait_duration 最长等待时间；小于等于 0 时只做一次非阻塞尝试。
     * @return 取到元素返回 true；超时或已关闭且为空返回 false。
     */
    static bool DequeueFor(MultiProducerMultiConsumerCore& core, void* popped_item, std::chrono::milliseconds wait_duration);

    /**
     * @brief 关闭队列并唤醒全部等待线程。
     * @param core 队列核心。
     */
    static void Close(MultiProducerMultiConsumerCore& core);

    /**
     * @brief 返回当前元素数。
     * @param core 队列核心。
     * @return 元素数，范围 [0, Capacity()]。
     */
    static std::size_t Size(const MultiProducerMultiConsumerCore& core);

    /**
     * @brief 返回可用元素数。
     * @param core 队列核心。
     * @return 可用元素数。
     */
    static std::size_t Capacity(const MultiProducerMultiConsumerCore& core) noexcept;

    /**
     * @brief 返回覆盖最旧元素的累计次数。
     * @param core 队列核心。
     * @return 累计覆盖次数。
     */
    static std::size_t OverrunCounter(const MultiProducerMultiConsumerCore& core);
};

/**
 * @brief 生成默认模式的槽位回调：移动构造 + 移动赋值 + 析构。
 *
 * CircularQueue 自身不需要移动赋值；默认模式出队把元素移动赋值到调用方
 * 提供的 T&，因此在这里补上 move_assign。
 *
 * @tparam T 被存储的类型。
 * @return 填充后的回调集合。
 */
template <class T>
CircularQueueHooks MakeMultiProducerMultiConsumerHooks() {
    CircularQueueHooks hooks = MakeCircularQueueHooks<T>();
    hooks.move_assign = &MoveAssignCircularQueueSlot<T>;
    return hooks;
}

}

/**
 * @brief 多生产者多消费者队列；编译期选择阻塞实现或无锁实现。
 *
 * 未定义 USE_LOCK_FREE_QUEUE 时使用固定容量环形队列 + 互斥量 + 两个条件变量：
 * - Capacity() 是有界上限；Enqueue() 在满时阻塞；
 * - EnqueueNoWait() 在满时覆盖最旧元素并累加 OverrunCounter()；
 * - TryEnqueue() 在满时立即返回 false，不覆盖、不阻塞；
 * - Emplace()/TryEmplace() 用构造参数在槽位上原地构造，不产生额外移动，语义分别
 *   与 Enqueue()/TryEnqueue() 相同；构造在锁内进行，T 的构造不得回调本队列；
 * - DequeueFor() 最多等待 wait_duration，超时返回 false。
 *
 * 定义 USE_LOCK_FREE_QUEUE 时使用 moodycamel::BlockingConcurrentQueue<T>：
 * - Capacity() 只是初始分配提示，队列可增长到超过它；Enqueue() 不会因满而阻塞；
 * - EnqueueNoWait() 与 Enqueue() 行为相同；OverrunCounter() 恒为 0；
 * - TryEnqueue() 仅在已关闭或分配失败时返回 false；
 * - Emplace()/TryEmplace() 先构造临时对象再入队，比原地构造多一次移动，语义分别
 *   与 Enqueue()/TryEnqueue() 相同；
 * - DequeueFor() 使用 BlockingConcurrentQueue 的限时出队；Size() 为估算值。
 *
 * 所有公共方法可以安全地被多个线程并发调用；元素按先进先出取出。
 *
 * 所有权：队列持有全部元素；入队把实参移动进队列，实参随后处于已移动状态；
 * 出队把元素移动赋值到调用方提供的对象。
 *
 * 关闭：Close() 之后不再接受入队，Enqueue()/EnqueueNoWait()/TryEnqueue() 均返回
 * false 且实参保持不变；DequeueFor() 继续取出剩余元素，队列为空时立即返回 false，
 * 不再等待。Close() 可重复调用。默认模式下 Close() 会唤醒所有阻塞的
 * Enqueue()/DequeueFor()；无锁模式下无法中断已经进入 wait_dequeue_timed 的等待，
 * 该调用仍会在超时后返回，调用方应使用有界超时。
 *
 * 异常：默认模式构造可能抛 std::length_error（容量无法表示）或 std::bad_alloc；
 * 入队的移动构造抛异常时元素未入队，未满时队列保持不变，覆盖最旧元素时
 * 最旧元素已丢且 Size() 减 1；出队的移动赋值抛异常时元素仍在队列中，
 * 调用方对象内容未定义；队列自身保持可继续使用。
 * 无锁模式 Enqueue()/EnqueueNoWait() 分配失败抛 std::bad_alloc；TryEnqueue() 不抛，
 * 分配失败以 false 返回。moodycamel 不保证 T 的移动赋值抛异常时的强异常安全。
 *
 * 线程安全：默认模式用互斥量保护内部状态；无锁模式由 moodycamel 保证。
 * T 的移动赋值不得回调本队列（默认模式会死锁）。在仍有线程阻塞于
 * Enqueue()/DequeueFor() 时析构本对象是未定义行为，调用方必须先关闭队列并 join 所有线程。
 *
 * Size() 是瞬时快照；无锁模式下为估算值，不能用于同步判断。
 *
 * @tparam T 被存储的类型；必须可默认构造、可移动构造、可移动赋值、可析构。
 */
template <class T>
class MultiProducerMultiConsumer final : private NonCopyable {
public:
    // 被存储的类型。
    using value_type = T;

    /**
     * @brief 构造队列。
     * @param capacity 可用元素数；默认模式是有界上限，无锁模式是初始容量提示；0 归一为 1。
     * @throw std::length_error 默认模式下容量或槽位字节数无法表示时抛出。
     * @throw std::bad_alloc 分配失败时抛出。
     */
    explicit MultiProducerMultiConsumer(std::size_t capacity);

    /**
     * @brief 把元素移动到队尾。
     * @param item 待入队元素；调用后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     * @note 默认模式满时阻塞；无锁模式立即返回。
     * @throw std::bad_alloc 无锁模式分配失败时抛出。
     */
    [[nodiscard]] bool Enqueue(T&& item);

    /**
     * @brief 立即把元素移动到队尾。
     * @param item 待入队元素；调用后处于已移动状态。
     * @return 入队成功返回 true；队列已关闭返回 false，实参保持不变。
     * @note 默认模式满时覆盖最旧元素并累加 OverrunCounter()；无锁模式与 Enqueue() 相同。
     * @throw std::bad_alloc 无锁模式分配失败时抛出。
     */
    [[nodiscard]] bool EnqueueNoWait(T&& item);

    /**
     * @brief 尝试入队；不阻塞、不覆盖。
     * @param item 待入队元素；仅在入队成功时被移动。
     * @return 入队成功返回 true；默认模式满时或队列已关闭时返回 false。
     * @note 默认模式拒绝时不影响 OverrunCounter()；无锁模式仅在已关闭或分配失败时返回 false。
     */
    [[nodiscard]] bool TryEnqueue(T&& item);

    /**
     * @brief 用构造参数入队；默认模式原地构造，无锁模式先构造临时对象再入队。
     * @tparam Args 构造参数类型。
     * @param args 传给 T 构造函数的参数。
     * @return 入队成功返回 true；队列已关闭返回 false。
     * @note 无锁模式受底层限制，比原地构造多一次移动。
     * @throw std::bad_alloc 无锁模式分配失败时抛出。
     */
    template <class... Args>
    [[nodiscard]] bool Emplace(Args&&... args);

    /**
     * @brief 尝试用构造参数入队；不阻塞、不覆盖。
     * @tparam Args 构造参数类型。
     * @param args 传给 T 构造函数的参数。
     * @return 入队成功返回 true；默认模式满时或队列已关闭时返回 false。
     * @note 无锁模式比原地构造多一次移动；拒绝时不影响 OverrunCounter()。
     */
    template <class... Args>
    [[nodiscard]] bool TryEmplace(Args&&... args);

    /**
     * @brief 限时取出队首元素。
     * @param popped_item 输出参数；成功时被移动赋值。
     * @param wait_duration 最长等待时间；小于等于 0 时只做一次非阻塞尝试。
     * @return 取到元素返回 true；超时或已关闭且为空返回 false。
     */
    [[nodiscard]] bool DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration);

    /**
     * @brief 关闭队列并唤醒全部等待线程。
     *
     * 关闭后拒绝新的入队，DequeueFor() 取出剩余元素后立即返回 false。可重复调用。
     */
    void Close();

    /**
     * @brief 返回当前元素数。
     * @return 元素数；默认模式范围 [0, Capacity()]，无锁模式为估算值。
     */
    [[nodiscard]] std::size_t Size() const;

    /**
     * @brief 返回容量。
     * @return 默认模式为可用元素数；无锁模式为构造时的初始容量提示。
     */
    [[nodiscard]] std::size_t Capacity() const noexcept;

    /**
     * @brief 返回覆盖最旧元素的累计次数。
     * @return 默认模式为覆盖最旧的累计次数；无锁模式恒为 0。
     */
    [[nodiscard]] std::size_t OverrunCounter() const;

private:
    static_assert(std::is_default_constructible_v<T>, "queued type must be default constructible");
    static_assert(std::is_move_constructible_v<T>, "queued type must be move constructible");
    static_assert(std::is_move_assignable_v<T>, "queued type must be move assignable");
    static_assert(std::is_destructible_v<T>, "queued type must be destructible");

#ifdef USE_LOCK_FREE_QUEUE
    // 无锁队列；容量只是初始分配提示。
    moodycamel::BlockingConcurrentQueue<T> queue_;
    // 构造时传入的容量提示；0 归一为 1。
    std::size_t capacity_hint_ = 0;
    // 关闭标记；为 true 后拒绝新的入队。
    std::atomic<bool> closed_{false};
#else
    // 默认模式核心的共享所有权；对象不可移动，构造后始终有效。
    std::shared_ptr<detail::MultiProducerMultiConsumerCore> core_;
#endif
};

#ifdef USE_LOCK_FREE_QUEUE

template <class T>
MultiProducerMultiConsumer<T>::MultiProducerMultiConsumer(std::size_t capacity) : queue_(capacity == 0 ? 1 : capacity), capacity_hint_(capacity == 0 ? 1 : capacity) {}

template <class T>
bool MultiProducerMultiConsumer<T>::Enqueue(T&& item) {
    if (closed_.load()) {
        return false;
    }

    if (!queue_.enqueue(std::move(item))) {
        throw std::bad_alloc();
    }

    return true;
}

template <class T>
bool MultiProducerMultiConsumer<T>::EnqueueNoWait(T&& item) {
    if (closed_.load()) {
        return false;
    }

    if (!queue_.enqueue(std::move(item))) {
        throw std::bad_alloc();
    }

    return true;
}

template <class T>
bool MultiProducerMultiConsumer<T>::TryEnqueue(T&& item) {
    if (closed_.load()) {
        return false;
    }

    // 无锁队列可增长：这里用会分配但不会阻塞的 enqueue，仅在分配失败时返回 false。
    return queue_.enqueue(std::move(item));
}

template <class T>
template <class... Args>
bool MultiProducerMultiConsumer<T>::Emplace(Args&&... args) {
    T item(std::forward<Args>(args)...);
    return Enqueue(std::move(item));
}

template <class T>
template <class... Args>
bool MultiProducerMultiConsumer<T>::TryEmplace(Args&&... args) {
    T item(std::forward<Args>(args)...);
    return TryEnqueue(std::move(item));
}

template <class T>
bool MultiProducerMultiConsumer<T>::DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration) {
    if (closed_.load()) {
        return queue_.try_dequeue(popped_item);
    }

    if (wait_duration <= std::chrono::milliseconds::zero()) {
        return queue_.try_dequeue(popped_item);
    }

    return queue_.wait_dequeue_timed(popped_item, wait_duration);
}

template <class T>
void MultiProducerMultiConsumer<T>::Close() {
    closed_.store(true);
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::Size() const {
    return queue_.size_approx();
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::Capacity() const noexcept {
    return capacity_hint_;
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::OverrunCounter() const {
    return 0;
}

#else

template <class T>
MultiProducerMultiConsumer<T>::MultiProducerMultiConsumer(std::size_t capacity)
    : core_(detail::MultiProducerMultiConsumerBridge::Create(capacity, detail::MakeMultiProducerMultiConsumerHooks<T>())) {}

template <class T>
bool MultiProducerMultiConsumer<T>::Enqueue(T&& item) {
    return detail::MultiProducerMultiConsumerBridge::Enqueue(*core_, &item);
}

template <class T>
bool MultiProducerMultiConsumer<T>::EnqueueNoWait(T&& item) {
    return detail::MultiProducerMultiConsumerBridge::EnqueueNoWait(*core_, &item);
}

template <class T>
bool MultiProducerMultiConsumer<T>::TryEnqueue(T&& item) {
    return detail::MultiProducerMultiConsumerBridge::TryEnqueue(*core_, &item);
}

template <class T>
template <class... Args>
bool MultiProducerMultiConsumer<T>::Emplace(Args&&... args) {
    auto params = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::MultiProducerMultiConsumerBridge::EnqueueEmplace(*core_, &params, &detail::EmplaceCircularQueueSlot<T, decltype(params)>);
}

template <class T>
template <class... Args>
bool MultiProducerMultiConsumer<T>::TryEmplace(Args&&... args) {
    auto params = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::MultiProducerMultiConsumerBridge::TryEnqueueEmplace(*core_, &params, &detail::EmplaceCircularQueueSlot<T, decltype(params)>);
}

template <class T>
bool MultiProducerMultiConsumer<T>::DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration) {
    return detail::MultiProducerMultiConsumerBridge::DequeueFor(*core_, &popped_item, wait_duration);
}

template <class T>
void MultiProducerMultiConsumer<T>::Close() {
    detail::MultiProducerMultiConsumerBridge::Close(*core_);
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::Size() const {
    return detail::MultiProducerMultiConsumerBridge::Size(*core_);
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::Capacity() const noexcept {
    return detail::MultiProducerMultiConsumerBridge::Capacity(*core_);
}

template <class T>
std::size_t MultiProducerMultiConsumer<T>::OverrunCounter() const {
    return detail::MultiProducerMultiConsumerBridge::OverrunCounter(*core_);
}

#endif

NAMESPACE_EVEREST_END

#endif
