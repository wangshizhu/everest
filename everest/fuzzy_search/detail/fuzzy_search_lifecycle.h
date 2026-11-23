/**
 * @file fuzzy_search_lifecycle.h
 * @brief 进程级单实例守卫。
 */

#ifndef FUZZY_SEARCH_LIFECYCLE_H_
#define FUZZY_SEARCH_LIFECYCLE_H_

#include <atomic>

#include "common/define.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 进程级单实例守卫。
 *
 * 守卫被所有模板实例化共享：同一进程内任意两个存活实例（即使 id 类型或 BmpOnly 不同）
 * 不能同时存在。定义 FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES 可关闭守卫。
 */
class FuzzySearchCreationGuard final {
public:
    /**
     * @brief 禁止实例化。
     */
    FuzzySearchCreationGuard() = delete;

    /**
     * @brief 尝试占用唯一的实例名额。
     *
     * @return 占用成功返回 true；已有存活实例时返回 false。
     *
     * @note 多线程同时创建时保证只有一个成功。
     */
    [[nodiscard]] static bool TryAcquire() noexcept {
#ifdef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
        return true;
#else
        bool expected = false;
        return Flag().compare_exchange_strong(expected, true);
#endif
    }

    /**
     * @brief 释放实例名额，供后续实例创建。
     */
    static void Release() noexcept {
#ifndef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
        Flag().store(false, std::memory_order_release);
#endif
    }

private:
#ifndef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
    /**
     * @brief 返回进程级标志位。
     *
     * @return 有意泄漏的 atomic 引用，避免静态析构顺序导致访问已销毁对象。
     */
    static std::atomic<bool>& Flag() noexcept {
        static std::atomic<bool>* flag = new std::atomic<bool>(false);
        return *flag;
    }
#endif
};

NAMESPACE_EVEREST_END

#endif
