/**
 * @file lru.h
 * @brief 基于滑动 TTL 的缓存与全局清扫器。
 */

#ifndef LRU_H_
#define LRU_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/define.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

// 缓存与清扫器的实现辅助。
namespace detail {

/**
 * @brief 判断类型是否为 std::shared_ptr。
 * @tparam T 待判断的类型。
 */
template <class T>
struct LruIsSharedPtr : std::false_type {};

/**
 * @brief std::shared_ptr 特化：判定成立。
 * @tparam T 被 shared_ptr 管理的类型。
 */
template <class T>
struct LruIsSharedPtr<std::shared_ptr<T>> : std::true_type {};

}

/**
 * @brief 带滑动 TTL 的缓存基类：为全局清扫器提供非模板句柄。
 *
 * 线程亲和：所有方法只能在缓存所属线程调用，不得跨线程并发访问。
 */
class LruBase : private NonCopyable {
public:
    /**
     * @brief 销毁缓存基类。
     */
    virtual ~LruBase();

    /**
     * @brief 清理全部已过期条目。
     *
     * 可重复调用；值的析构中不得访问缓存。
     */
    virtual void Update() = 0;

protected:
    // 计时时钟。
    using Clock = std::chrono::steady_clock;
    // 到期时间点类型。
    using TimePoint = Clock::time_point;
    // 时长类型。
    using Duration = Clock::duration;

    /**
     * @brief 构造缓存基类。
     * @param ttl 条目存活时间；小于等于 0 时条目写入即过期。
     */
    explicit LruBase(Duration ttl);

    /**
     * @brief 取条目存活时间。
     * @return 条目存活时间。
     */
    Duration Ttl() const {
        return ttl_;
    }

private:
    // 条目存活时间。
    Duration ttl_;
};

// 缓存基类的共享所有权。
using LruBaseSharedPtr = std::shared_ptr<LruBase>;
// 缓存基类的弱引用。
using LruBaseWeakPtr = std::weak_ptr<LruBase>;

/**
 * @brief 登记缓存并按固定间隔统一清理过期条目。
 *
 * 只持有弱引用，不影响缓存生命周期；缓存被销毁后登记项在下次
 * RegisterLruList() 或实际清扫时清理。清扫只回收不再被访问的过期条目所占内存，
 * 缓存的正确性由 Get() 自身的过期校验保证，不依赖本管理器被调用。
 *
 * 线程亲和：RegisterLruList() 与 Update() 必须在同一线程调用，且不得与
 * 缓存自身的读写并发。
 */
class LruManager final : private NonCopyable {
public:
    // 相邻两次实际清扫的最小间隔。
    static constexpr std::chrono::seconds kSweepInterval{1};

    /**
     * @brief 取进程内唯一实例。
     * @return 单例指针，始终非空。
     */
    static LruManager* GetInstance();

    /**
     * @brief 登记缓存；为空时忽略。重复登记同一缓存会重复登记，不影响正确性。
     * @param cache 缓存。
     */
    void RegisterLruList(const LruBaseSharedPtr& cache);

    /**
     * @brief 清理所有已登记缓存中的过期条目。
     *
     * 距上次实际清扫不足 kSweepInterval 时直接返回。
     */
    void Update();

private:
    /**
     * @brief 构造单例；仅供 GetInstance() 使用。
     */
    LruManager();

    /**
     * @brief 销毁单例。
     */
    ~LruManager();

    // 已登记缓存。
    std::vector<LruBaseWeakPtr> caches_;
    // 下一次允许清扫的时间点。
    std::chrono::steady_clock::time_point next_update_time_point_;
};

/**
 * @brief 以滑动 TTL 淘汰的缓存。
 *
 * 条目在 Get()/Put() 命中时刷新到期时间，因此热点条目会持续续期。
 * 过期条目在 Get() 时立即判定为未命中并移除，不依赖 Update() 的调用时机；
 * Update() 只回收不再被访问的过期条目所占内存。
 *
 * 非线程安全：全部公共方法只能在缓存所属线程调用，不得跨线程并发访问。
 * 值的析构可能在容器操作过程中执行，析构中不得访问本缓存。
 *
 * @tparam K 键类型。
 * @tparam V 值类型，必须是 std::shared_ptr。
 * @tparam Hash 键哈希策略。
 */
template <class K, class V, class Hash = std::hash<K>>
class LruList final : public LruBase {
public:
    /**
     * @brief 创建缓存并登记到 LruManager。
     * @param ttl 条目存活时间；默认 1 小时。
     * @return 缓存的共享所有权。
     */
    static std::shared_ptr<LruList> MakeLruList(Duration ttl = std::chrono::hours(1));

    /**
     * @brief 取值；命中后刷新到期时间与 LRU 位置。
     * @param key 键。
     * @return 命中返回值的副本；未命中或已过期返回空指针。
     */
    [[nodiscard]] V Get(const K& key);

    /**
     * @brief 写入或覆盖条目，并刷新到期时间与 LRU 位置。
     * @param key 键。
     * @param value 值；调用后被移动进缓存。
     * @throw std::bad_alloc 索引分配失败时抛出；此时缓存内容不变。
     */
    void Put(const K& key, V&& value);

    /**
     * @brief 删除指定条目。
     * @param key 键。
     * @return 删除成功返回 true；不存在返回 false。
     */
    bool Erase(const K& key);

    /**
     * @brief 返回当前条目数。
     * @return 条目数；含尚未被清理的过期条目。
     */
    std::size_t Size() const;

    /**
     * @brief 清理全部已过期条目。
     */
    void Update() override;

private:
    // 条目。
    struct Entry {
        // 键。
        K key;
        // 值。
        V value;
        // 到期时间点。
        TimePoint expiry;
    };

    // 私有构造标记；仅供 MakeLruList 使用。
    class PrivateFlag final {};

    // 条目链表；表头最新，表尾最旧。
    using EntryList = std::list<Entry>;
    // 键到链表迭代器的索引。
    using EntryIndex = std::unordered_map<K, typename EntryList::iterator, Hash>;

    static_assert(detail::LruIsSharedPtr<V>::value, "V must be a std::shared_ptr");

    /**
     * @brief 构造缓存；仅供 MakeLruList 使用。
     * @param ttl 条目存活时间。
     */
    LruList(Duration ttl, const PrivateFlag&);

    // 条目链表。
    EntryList entries_;
    // 键索引。
    EntryIndex index_;
};

template <class K, class V, class Hash>
LruList<K, V, Hash>::LruList(Duration ttl, const PrivateFlag&) : LruBase(ttl) {}

template <class K, class V, class Hash>
std::shared_ptr<LruList<K, V, Hash>> LruList<K, V, Hash>::MakeLruList(Duration ttl) {
    // make_shared 无权调用私有构造，这里在工厂内分配并立即交给 shared_ptr 管理。
    std::shared_ptr<LruList> cache(new LruList(ttl, PrivateFlag()));
    LruManager::GetInstance()->RegisterLruList(cache);
    return cache;
}

template <class K, class V, class Hash>
V LruList<K, V, Hash>::Get(const K& key) {
    auto iter_index = index_.find(key);
    if (iter_index == index_.end()) {
        return V();
    }

    auto iter_entry = iter_index->second;
    TimePoint now = Clock::now();

    if (iter_entry->expiry <= now) {
        index_.erase(iter_index);
        entries_.erase(iter_entry);
        return V();
    }

    iter_entry->expiry = now + Ttl();
    entries_.splice(entries_.begin(), entries_, iter_entry);
    return iter_entry->value;
}

template <class K, class V, class Hash>
void LruList<K, V, Hash>::Put(const K& key, V&& value) {
    TimePoint expiry = Clock::now() + Ttl();

    auto iter_index = index_.find(key);
    if (iter_index != index_.end()) {
        auto iter_entry = iter_index->second;
        iter_entry->value = std::move(value);
        iter_entry->expiry = expiry;
        entries_.splice(entries_.begin(), entries_, iter_entry);
        return;
    }

    entries_.emplace_front(Entry{key, std::move(value), expiry});
    try {
        index_.emplace(key, entries_.begin());
    } catch (...) {
        // 回滚已插入的节点，保持缓存内容不变。
        entries_.pop_front();
        throw;
    }
}

template <class K, class V, class Hash>
bool LruList<K, V, Hash>::Erase(const K& key) {
    auto iter_index = index_.find(key);
    if (iter_index == index_.end()) {
        return false;
    }

    auto iter_entry = iter_index->second;
    index_.erase(iter_index);
    entries_.erase(iter_entry);
    return true;
}

template <class K, class V, class Hash>
std::size_t LruList<K, V, Hash>::Size() const {
    return entries_.size();
}

template <class K, class V, class Hash>
void LruList<K, V, Hash>::Update() {
    TimePoint now = Clock::now();

    // 不变量：所有条目共用同一 ttl，Get()/Put() 命中会把条目移到表头并刷新到期时间，
    // 因此到期时间沿 entries_ 从表头到表尾单调递减，表尾一定是最早到期的条目。
    while (!entries_.empty()) {
        auto iter_entry = std::prev(entries_.end());
        if (iter_entry->expiry > now) {
            break;
        }

        index_.erase(iter_entry->key);
        entries_.erase(iter_entry);
    }
}

NAMESPACE_EVEREST_END

#endif
