/**
 * @file circular_queue.h
 * @brief 固定容量的环形队列：写满后覆盖最旧元素。
 */

#ifndef CIRCULAR_QUEUE_H_
#define CIRCULAR_QUEUE_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common/define.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

// 槽位存储与环形索引的实现细节；仅供 CircularQueue 使用。
namespace detail {

// 槽位存储与环形索引；定义在 circular_queue.cpp，对外不可见。
class CircularQueueCore;

// 原地构造回调：在未初始化的槽位上用 params 描述的参数构造 T。
using CircularQueueConstructFn = void (*)(void* slot, void* params);

/**
 * @brief 环形队列与具体类型之间的移动构造、移动赋值、析构回调。
 */
struct CircularQueueHooks {
    // 在未初始化的槽位上 placement-new 移动构造 T；src 由调用方保证有效。
    void (*move_construct)(void* slot, void* src);
    // 把 *src 移动赋值到已构造的 *dst；仅供需要的使用方（出队到 T&），CircularQueue 不使用。
    void (*move_assign)(void* dst, void* src);
    // 析构槽位上的 T。
    void (*destroy)(void* slot);
    // T 的字节大小。
    std::size_t slot_size;
    // T 的对齐要求。
    std::size_t slot_align;
};

/**
 * @brief 把模板胶水与 .cpp 中的存储实现连接起来。
 *
 * 类本身不持有状态，只提供静态转发入口；仅供 CircularQueue<T> 使用。
 */
class CircularQueueBridge final {
public:
    CircularQueueBridge() = delete;

    /**
     * @brief 创建队列存储。
     * @param capacity 可用元素数；0 归一为 1。
     * @param hooks 槽位的移动构造、移动赋值、析构回调。
     * @return 队列存储的共享所有权。
     * @throw std::length_error 容量或槽位字节数无法表示时抛出。
     */
    static std::shared_ptr<CircularQueueCore> Create(std::size_t capacity, const CircularQueueHooks& hooks);

    /**
     * @brief 把元素移动到队尾；队列已满时覆盖最旧元素。
     * @param core 队列存储。
     * @param item 待入队元素；成功后处于已移动状态。
     */
    static void PushBack(CircularQueueCore& core, void* item);

    /**
     * @brief 尝试把元素移动到队尾；队列已满时拒绝写入。
     * @param core 队列存储。
     * @param item 待入队元素；仅在入队成功时被移动。
     * @return 入队成功返回 true；队列已满返回 false。
     */
    static bool TryPushBack(CircularQueueCore& core, void* item);

    /**
     * @brief 用构造参数在队尾原地构造元素；队列已满时覆盖最旧元素。
     * @param core 队列存储。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     */
    static void EmplaceBack(CircularQueueCore& core, void* params, CircularQueueConstructFn construct);

    /**
     * @brief 尝试用构造参数在队尾原地构造元素；队列已满时拒绝写入。
     * @param core 队列存储。
     * @param params 构造参数元组；仅在本次调用内有效，回调不得缓存。
     * @param construct 在槽位上构造 T 的回调。
     * @return 构造成功返回 true；队列已满返回 false。
     */
    static bool TryEmplaceBack(CircularQueueCore& core, void* params, CircularQueueConstructFn construct);

    /**
     * @brief 返回队首元素地址。
     * @param core 队列存储；要求队列非空。
     * @return 队首元素地址。
     */
    static void* Front(CircularQueueCore& core) noexcept;

    /**
     * @brief 返回队首元素地址。
     * @param core 队列存储；要求队列非空。
     * @return 队首元素地址。
     */
    static const void* Front(const CircularQueueCore& core) noexcept;

    /**
     * @brief 返回逻辑下标对应元素的地址。
     * @param core 队列存储。
     * @param index 逻辑下标，0 表示队首；要求 index 小于 Size()。
     * @return 元素地址。
     */
    static void* At(CircularQueueCore& core, std::size_t index) noexcept;

    /**
     * @brief 返回逻辑下标对应元素的地址。
     * @param core 队列存储。
     * @param index 逻辑下标，0 表示队首；要求 index 小于 Size()。
     * @return 元素地址。
     */
    static const void* At(const CircularQueueCore& core, std::size_t index) noexcept;

    /**
     * @brief 移除队首元素；队列为空时无操作。
     * @param core 队列存储。
     */
    static void PopFront(CircularQueueCore& core) noexcept;

    /**
     * @brief 判断队列是否为空。
     * @param core 队列存储。
     * @return 没有元素返回 true。
     */
    static bool Empty(const CircularQueueCore& core) noexcept;

    /**
     * @brief 判断队列是否已满。
     * @param core 队列存储。
     * @return 元素数等于容量返回 true。
     */
    static bool Full(const CircularQueueCore& core) noexcept;

    /**
     * @brief 返回当前元素数。
     * @param core 队列存储。
     * @return 元素数，范围 [0, Capacity()]。
     */
    static std::size_t Size(const CircularQueueCore& core) noexcept;

    /**
     * @brief 返回可用元素数。
     * @param core 队列存储。
     * @return 可用元素数。
     */
    static std::size_t Capacity(const CircularQueueCore& core) noexcept;

    /**
     * @brief 返回覆盖最旧元素的累计次数。
     * @param core 队列存储。
     * @return 累计覆盖次数。
     */
    static std::size_t OverrunCounter(const CircularQueueCore& core) noexcept;
};

/**
 * @brief 在未初始化的槽位上 placement-new 移动构造一个 T。
 * @tparam T 被存储的类型。
 * @param slot 已按 alignof(T) 对齐且未构造的槽位；不能为空。
 * @param src 源对象；不能为空。
 */
template <class T>
void MoveConstructCircularQueueSlot(void* slot, void* src) {
    ::new (slot) T(std::move(*static_cast<T*>(src)));
}

/**
 * @brief 用 Tuple 中的参数在未初始化的槽位上 placement-new 构造 T。
 *
 * params 指向调用方栈上的引用元组，仅在本次调用内有效，回调不得缓存。
 *
 * @tparam T 被存储的类型。
 * @tparam Tuple 参数元组类型，来自 std::forward_as_tuple。
 * @param slot 已对齐且未构造的槽位；不能为空。
 * @param params 指向 Tuple 的指针；不能为空。
 */
template <class T, class Tuple>
void EmplaceCircularQueueSlot(void* slot, void* params) {
    auto& tuple = *static_cast<Tuple*>(params);
    // 按 Tuple&& 调用 apply：元组元素是引用，右值元组下才能保留右值引用元素的
    // 值类别；对左值元组 apply 会把右值引用元素降为左值，移动语义参数将无法转发。
    std::apply([slot](auto&&... values) { ::new (slot) T(std::forward<decltype(values)>(values)...); }, static_cast<Tuple&&>(tuple));
}

/**
 * @brief 把 src 指向的对象移动赋值到 dst 指向的对象。
 * @tparam T 被存储的类型。
 * @param dst 目标对象；不能为空。
 * @param src 源对象；不能为空。
 */
template <class T>
void MoveAssignCircularQueueSlot(void* dst, void* src) {
    *static_cast<T*>(dst) = std::move(*static_cast<T*>(src));
}

/**
 * @brief 析构槽位上的 T。
 * @tparam T 被存储的类型。
 * @param slot 已构造对象的槽位；不能为空。
 */
template <class T>
void DestroyCircularQueueSlot(void* slot) {
    static_cast<T*>(slot)->~T();
}

/**
 * @brief 生成指定类型的槽位移动构造与析构回调。
 *
 * move_assign 留空：CircularQueue 不使用移动赋值，需要出队到 T& 的使用方自行补上。
 *
 * @tparam T 被存储的类型。
 * @return 填充后的回调集合。
 */
template <class T>
CircularQueueHooks MakeCircularQueueHooks() {
    CircularQueueHooks hooks{};
    hooks.move_construct = &MoveConstructCircularQueueSlot<T>;
    hooks.move_assign = nullptr;
    hooks.destroy = &DestroyCircularQueueSlot<T>;
    hooks.slot_size = sizeof(T);
    hooks.slot_align = alignof(T);
    return hooks;
}

}

/**
 * @brief 固定容量的环形队列，写满后覆盖最旧元素。
 *
 * 元素按先进先出取出。容量为 Capacity() 时可容纳 Capacity() 个元素。写满时有两种
 * 写法：PushBack() 覆盖 Front() 指向的最旧元素；TryPushBack() 拒绝写入并返回
 * false，实参保持不变。两者都会累加 OverrunCounter()。EmplaceBack() 与
 * TryEmplaceBack() 用构造参数直接在槽位上构造元素，语义分别与 PushBack() 和
 * TryPushBack() 相同，且不产生额外移动。
 *
 * 所有权：队列持有全部槽位及其元素；槽位在首次写入时才构造元素，出队时析构。
 * PushBack() 把实参移动进队列，实参随后处于已移动状态。Front()/At() 返回的
 * 引用在下一次 PushBack() 覆盖该元素、PopFront() 析构该元素或队列销毁后失效。
 *
 * 异常保证：入队未满时为强保证，构造元素抛异常时队列不变；队列已满时
 * PushBack()/EmplaceBack() 先析构并丢弃最旧元素再构造新元素，构造抛异常时
 * 最旧元素已丢、新元素未入队、Size() 减 1、OverrunCounter() 已加 1，队列仍可
 * 继续使用（基本保证）。
 *
 * 前置条件：Front()/At() 要求队列非空；At() 还要求 index < Size()。
 * Debug 下用 assert 检查，Release 下不保证。PopFront() 允许在空队列上调用，
 * 此时不做任何改动。
 *
 * 线程安全：不做同步，调用方负责外部同步。
 *
 * 移动后的对象是空队列：Capacity() == 0、Empty() == true，可安全查询与
 * PushBack()/EmplaceBack()（无操作），不可 Front()/At()/PopFront() 访问元素。
 *
 * @tparam T 被存储的类型；必须可移动构造、可析构。
 */
template <class T>
class CircularQueue final : private MoveOnly {
public:
    using value_type = T;

    /**
     * @brief 构造环形队列。
     * @param capacity 可用元素数；0 归一为 1。
     * @throw std::length_error 容量或槽位字节数无法表示时抛出。
     */
    explicit CircularQueue(std::size_t capacity);

    CircularQueue(CircularQueue&& other) noexcept = default;
    CircularQueue& operator=(CircularQueue&& other) noexcept;

    /**
     * @brief 把元素移动到队尾；队列已满时覆盖最旧元素。
     * @param item 待入队元素；调用后处于已移动状态。
     */
    void PushBack(T&& item);

    /**
     * @brief 尝试把元素移动到队尾；队列已满时拒绝写入。
     * @param item 待入队元素；仅在入队成功时被移动，被拒绝时保持不变。
     * @return 入队成功返回 true；队列已满或队列已移动返回 false。
     */
    [[nodiscard]] bool TryPushBack(T&& item);

    /**
     * @brief 用构造参数在队尾原地构造元素；队列已满时覆盖最旧元素。
     * @tparam Args 构造参数类型。
     * @param args 传给 T 构造函数的参数。
     * @note 元素直接在槽位上构造，无额外移动。
     */
    template <class... Args>
    void EmplaceBack(Args&&... args);

    /**
     * @brief 尝试用构造参数在队尾原地构造元素；队列已满时拒绝写入。
     * @tparam Args 构造参数类型。
     * @param args 传给 T 构造函数的参数。
     * @return 构造成功返回 true；队列已满或队列已移动返回 false。
     * @note 元素直接在槽位上构造，无额外移动。
     */
    template <class... Args>
    [[nodiscard]] bool TryEmplaceBack(Args&&... args);

    /**
     * @brief 返回队首元素的引用。
     * @return 队首元素；要求队列非空。
     */
    [[nodiscard]] T& Front() noexcept;

    /**
     * @brief 返回队首元素的引用。
     * @return 队首元素；要求队列非空。
     */
    [[nodiscard]] const T& Front() const noexcept;

    /**
     * @brief 返回逻辑下标对应元素的引用。
     * @param index 逻辑下标，0 表示队首。
     * @return 元素引用；要求 index < Size()。
     */
    [[nodiscard]] T& At(std::size_t index) noexcept;

    /**
     * @brief 返回逻辑下标对应元素的引用。
     * @param index 逻辑下标，0 表示队首。
     * @return 元素引用；要求 index < Size()。
     */
    [[nodiscard]] const T& At(std::size_t index) const noexcept;

    /**
     * @brief 移除队首元素；队列为空时无操作。
     */
    void PopFront() noexcept;

    /**
     * @brief 判断队列是否为空。
     * @return 没有元素返回 true。
     */
    [[nodiscard]] bool Empty() const noexcept;

    /**
     * @brief 判断队列是否已满。
     * @return 元素数等于容量返回 true。
     */
    [[nodiscard]] bool Full() const noexcept;

    /**
     * @brief 返回当前元素数。
     * @return 元素数，范围 [0, Capacity()]。
     */
    [[nodiscard]] std::size_t Size() const noexcept;

    /**
     * @brief 返回可用元素数。
     * @return 可用元素数；移动后的对象返回 0。
     */
    [[nodiscard]] std::size_t Capacity() const noexcept;

    /**
     * @brief 返回因队列已满而丢失的元素累计次数。
     *
     * PushBack() 覆盖最旧元素时累加被挤掉的旧元素；TryPushBack() 拒绝写入时
     * 累加被拒绝的新元素。
     *
     * @return 累计丢失次数。
     */
    [[nodiscard]] std::size_t OverrunCounter() const noexcept;

private:
    static_assert(std::is_move_constructible_v<T>, "queued type must be move constructible");
    static_assert(std::is_destructible_v<T>, "queued type must be destructible");

    // 槽位存储的共享所有权；移动后为空。
    std::shared_ptr<detail::CircularQueueCore> core_;
};

template <class T>
CircularQueue<T>::CircularQueue(std::size_t capacity) : core_(detail::CircularQueueBridge::Create(capacity, detail::MakeCircularQueueHooks<T>())) {}

template <class T>
CircularQueue<T>& CircularQueue<T>::operator=(CircularQueue&& other) noexcept {
    if (this != &other) {
        core_ = std::move(other.core_);
    }
    return *this;
}

template <class T>
void CircularQueue<T>::PushBack(T&& item) {
    if (core_ == nullptr) {
        return;
    }

    detail::CircularQueueBridge::PushBack(*core_, &item);
}

template <class T>
bool CircularQueue<T>::TryPushBack(T&& item) {
    if (core_ == nullptr) {
        return false;
    }

    return detail::CircularQueueBridge::TryPushBack(*core_, &item);
}

template <class T>
template <class... Args>
void CircularQueue<T>::EmplaceBack(Args&&... args) {
    if (core_ == nullptr) {
        return;
    }

    auto params = std::forward_as_tuple(std::forward<Args>(args)...);
    detail::CircularQueueBridge::EmplaceBack(*core_, &params, &detail::EmplaceCircularQueueSlot<T, decltype(params)>);
}

template <class T>
template <class... Args>
bool CircularQueue<T>::TryEmplaceBack(Args&&... args) {
    if (core_ == nullptr) {
        return false;
    }

    auto params = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::CircularQueueBridge::TryEmplaceBack(*core_, &params, &detail::EmplaceCircularQueueSlot<T, decltype(params)>);
}

template <class T>
T& CircularQueue<T>::Front() noexcept {
    assert(core_ != nullptr);
    return *static_cast<T*>(detail::CircularQueueBridge::Front(*core_));
}

template <class T>
const T& CircularQueue<T>::Front() const noexcept {
    assert(core_ != nullptr);
    return *static_cast<const T*>(detail::CircularQueueBridge::Front(*core_));
}

template <class T>
T& CircularQueue<T>::At(std::size_t index) noexcept {
    assert(core_ != nullptr);
    return *static_cast<T*>(detail::CircularQueueBridge::At(*core_, index));
}

template <class T>
const T& CircularQueue<T>::At(std::size_t index) const noexcept {
    assert(core_ != nullptr);
    return *static_cast<const T*>(detail::CircularQueueBridge::At(*core_, index));
}

template <class T>
void CircularQueue<T>::PopFront() noexcept {
    assert(core_ != nullptr);
    detail::CircularQueueBridge::PopFront(*core_);
}

template <class T>
bool CircularQueue<T>::Empty() const noexcept {
    return (core_ == nullptr) || detail::CircularQueueBridge::Empty(*core_);
}

template <class T>
bool CircularQueue<T>::Full() const noexcept {
    return (core_ != nullptr) && detail::CircularQueueBridge::Full(*core_);
}

template <class T>
std::size_t CircularQueue<T>::Size() const noexcept {
    return (core_ == nullptr) ? 0 : detail::CircularQueueBridge::Size(*core_);
}

template <class T>
std::size_t CircularQueue<T>::Capacity() const noexcept {
    return (core_ == nullptr) ? 0 : detail::CircularQueueBridge::Capacity(*core_);
}

template <class T>
std::size_t CircularQueue<T>::OverrunCounter() const noexcept {
    return (core_ == nullptr) ? 0 : detail::CircularQueueBridge::OverrunCounter(*core_);
}

NAMESPACE_EVEREST_END

#endif
