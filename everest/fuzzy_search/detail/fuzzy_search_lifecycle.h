#pragma once

#include <atomic>

namespace metaverse::fuzzy_search_detail {

// 进程级创建守卫：默认只允许一个存活的 FuzzySearchSystem 实例。
// 移植到允许同进程多个实例的项目时，定义 FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES 关闭守卫。
class CreationGuard {
public:
    static bool TryAcquire() {
#ifdef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
        return true;
#else
        bool expected = false;
        return Flag().compare_exchange_strong(expected, true);
#endif
    }

    static void Release() {
#ifndef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
        Flag().store(false, std::memory_order_release);
#endif
    }

private:
#ifndef FUZZY_SEARCH_ALLOW_MULTIPLE_INSTANCES
    // 有意泄漏一个 atomic，避免静态析构顺序导致访问已销毁对象。
    static std::atomic<bool>& Flag() {
        static std::atomic<bool>* flag = new std::atomic<bool>(false);
        return *flag;
    }
#endif
};

}  // namespace metaverse::fuzzy_search_detail
