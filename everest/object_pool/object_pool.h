/**
 * @file object_pool.h
 * @brief 通用对象池：以租约形式复用同类型对象。
 */

#ifndef OBJECT_POOL_H_
#define OBJECT_POOL_H_

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "common/define.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

// 存储与并发控制实现细节；仅供 ObjectPool 使用。
namespace detail {

// 池存储与并发控制；定义在 object_pool.cpp，对外不可见。
class ObjectPoolCore;

/**
 * @brief 对象池与具体类型之间的构造、复位、析构回调。
 */
struct ObjectPoolHooks {
    // 构造一个新对象并返回其裸指针。
    void* (*allocate)();
    // 把对象复位为可复用状态。
    void (*clear)(void* object);
    // 销毁对象。
    void (*destroy)(void* object);
};

/**
 * @brief 把模板类型胶水与 .cpp 中的存储实现连接起来。
 *
 * 类本身不持有状态，只提供静态转发入口；仅供 ObjectPool<T> 使用。
 */
class ObjectPoolBridge final {
public:
    ObjectPoolBridge() = delete;

    /**
     * @brief 创建池存储。
     * @param expand_step 每次扩容构造的对象数量。
     * @param max_free 空闲对象上限；0 表示不限制。
     * @param hooks 对象构造、复位、析构回调。
     * @return 池存储的共享所有权。
     */
    static std::shared_ptr<ObjectPoolCore> Create(uint32_t expand_step, uint32_t max_free, const ObjectPoolHooks& hooks);

    /**
     * @brief 取一个已复位的空闲对象。
     * @param core 池存储。
     * @return 非空对象指针。
     */
    static void* Apply(ObjectPoolCore& core);

    /**
     * @brief 归还或销毁对象。
     * @param core 池存储。
     * @param object 待归还的对象。
     */
    static void Release(ObjectPoolCore& core, void* object) noexcept;

    /**
     * @brief 把空闲对象收缩到扩容步长。
     * @param core 池存储。
     */
    static void Shrink(ObjectPoolCore& core);
};

/**
 * @brief 构造一个池对象。
 * @tparam T 被池管理的类型。
 * @return 新构造对象的裸指针。
 */
template <class T>
void* AllocatePooledObject() {
    return new T();
}

/**
 * @brief 复位一个池对象。
 * @tparam T 被池管理的类型。
 * @param object 待复位的对象；不能为空。
 */
template <class T>
void ClearPooledObject(void* object) {
    static_cast<T*>(object)->Clear();
}

/**
 * @brief 销毁一个池对象。
 * @tparam T 被池管理的类型。
 * @param object 待销毁的对象；不能为空。
 */
template <class T>
void DestroyPooledObject(void* object) {
    delete static_cast<T*>(object);
}

/**
 * @brief 生成指定类型的构造、复位、析构回调。
 * @tparam T 被池管理的类型。
 * @return 填充后的回调集合。
 */
template <class T>
ObjectPoolHooks MakeObjectPoolHooks() {
    ObjectPoolHooks hooks{};
    hooks.allocate = &AllocatePooledObject<T>;
    hooks.clear = &ClearPooledObject<T>;
    hooks.destroy = &DestroyPooledObject<T>;
    return hooks;
}

}

/**
 * @brief 复用同类型对象的线程安全对象池。
 *
 * 池对象在首次申请或空闲对象耗尽时按 expand_step 批量构造。每次申请返回一个
 * 租约（SharedObject 或 UniqueObject）；租约释放时对象被复位并归还空闲链。
 * 被池管理的类型 T 必须可默认构造、可析构，并提供 Clear()：Clear() 把对象
 * 复位到可复用状态，且不得释放池外资源。
 *
 * 线程安全：ApplySharedObject()、ApplyUniqueObject()、Shrink() 以及租约释放
 * 可以在任意线程并发调用。对象池本身不得在使用期间析构，但池先于租约销毁是安全的：
 * 租约持有池存储的共享所有权，最后一个租约释放后池存储才真正析构。
 *
 * @tparam T 被池管理的类型。
 */
template <class T>
class ObjectPool final : private NonCopyable {
public:
    // 默认每次扩容构造的对象数量。
    static constexpr uint32_t kDefaultExpandStep = 64;

    // 共享所有权的池对象。
    using SharedObject = std::shared_ptr<T>;

    /**
     * @brief 归还或销毁池对象的删除器。
     *
     * 持有池存储的共享所有权，因此池对象先于租约销毁时仍能安全归还对象。
     */
    class Deleter {
    public:
        /**
         * @brief 构造删除器。
         * @param core 池存储的共享所有权。
         */
        explicit Deleter(std::shared_ptr<detail::ObjectPoolCore> core) noexcept;

        /**
         * @brief 归还或销毁对象。
         * @param object 待归还的对象；为空时不做任何处理。
         * @note 声明为 noexcept；日志失败会按标准库语义终止进程。
         */
        void operator()(T* object) noexcept;

    private:
        // 池存储的共享所有权。
        std::shared_ptr<detail::ObjectPoolCore> core_;
    };

    // 独占所有权的池对象。
    using UniqueObject = std::unique_ptr<T, Deleter>;

    /**
     * @brief 构造对象池。
     * @param expand_step 每次扩容构造的对象数量；0 归一为 kDefaultExpandStep。
     * @param max_free 空闲对象上限；0 表示不限制。取值小于 expand_step 时，
     *                 高频申请/释放会反复构造与析构对象。
     */
    explicit ObjectPool(uint32_t expand_step = kDefaultExpandStep, uint32_t max_free = 0);

    ~ObjectPool() = default;

    /**
     * @brief 申请一个共享所有权对象。
     * @return 非空的池对象租约。
     * @note 每次申请都会分配一个控制块；独占场景优先使用 ApplyUniqueObject()。
     */
    [[nodiscard]] SharedObject ApplySharedObject();

    /**
     * @brief 申请一个独占所有权对象。
     * @return 非空的池对象租约。
     */
    [[nodiscard]] UniqueObject ApplyUniqueObject();

    /**
     * @brief 把空闲对象收缩到 expand_step，若 max_free 非 0 且更小则收缩到 max_free。
     * @note 只销毁空闲对象；在用的租约不受影响，可以重复调用。
     */
    void Shrink();

private:
    static_assert(std::is_default_constructible_v<T>, "pooled type must be default constructible");
    static_assert(std::is_destructible_v<T>, "pooled type must be destructible");
    static_assert(
        requires(T& object) { object.Clear(); }, "pooled type must provide Clear()");

    // 池存储的共享所有权。
    std::shared_ptr<detail::ObjectPoolCore> core_;
};

template <class T>
ObjectPool<T>::Deleter::Deleter(std::shared_ptr<detail::ObjectPoolCore> core) noexcept : core_(std::move(core)) {}

template <class T>
void ObjectPool<T>::Deleter::operator()(T* object) noexcept {
    if (object == nullptr) {
        return;
    }

    detail::ObjectPoolBridge::Release(*core_, object);
}

template <class T>
ObjectPool<T>::ObjectPool(uint32_t expand_step, uint32_t max_free)
    : core_(detail::ObjectPoolBridge::Create(expand_step == 0 ? kDefaultExpandStep : expand_step, max_free, detail::MakeObjectPoolHooks<T>())) {}

template <class T>
typename ObjectPool<T>::SharedObject ObjectPool<T>::ApplySharedObject() {
    T* object = static_cast<T*>(detail::ObjectPoolBridge::Apply(*core_));
    return SharedObject(object, Deleter(core_));
}

template <class T>
typename ObjectPool<T>::UniqueObject ObjectPool<T>::ApplyUniqueObject() {
    T* object = static_cast<T*>(detail::ObjectPoolBridge::Apply(*core_));
    return UniqueObject(object, Deleter(core_));
}

template <class T>
void ObjectPool<T>::Shrink() {
    detail::ObjectPoolBridge::Shrink(*core_);
}

NAMESPACE_EVEREST_END

#endif
