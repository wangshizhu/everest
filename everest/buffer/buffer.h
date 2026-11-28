/**
 * @file buffer.h
 * @brief 可扩容字节缓冲区。
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "common/define.h"
#include "non_copyable/non_copyable.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 持有连续存储、按需扩容的字节缓冲区。
 *
 * 读位置指向有效数据起点；消费只前移读位置，未消费数据保持原位。已读前缀会在
 * Reserve() 或写空间不足时一次性压缩到存储开头，因此：
 * - CanWriteSize() 是「从写位置起的连续可写空间」，不含尚未回收的已读前缀；
 * - 需要保证可写空间时先调用 Reserve(size)，由它负责压缩与扩容。
 *
 * 典型用法：
 * - 直接写入：用 GetWritePos()/CanWriteSize() 取得可写区，写入后调用
 *   CommitWrite()；或用 Write()/WriteSome() 由缓冲区完成复制。
 * - 读取：用 GetReadPos()/Size() 读取，消费后调用 ConsumeRead()。
 * - 保证容量：调用 Reserve(size) 后再按 CanWriteSize() 写入。
 *
 * 所有权：Buffer 独占存储，不可拷贝、不可移动。
 *
 * 线程安全：不做同步，同一对象不得跨线程并发访问。
 */
class Buffer final : private NonCopyable {
public:
    // 默认初始容量。
    static constexpr std::size_t kDefaultCapacity = 512;
    // 扩容步长，也是容量取整粒度。
    static constexpr std::size_t kExpandStep = 512;
    // 默认最大容量（2 MB）。
    static constexpr std::size_t kMaxCapacity = 2 * 1024 * 1024;

    /**
     * @brief 构造缓冲区并分配初始容量。
     * @param init_capacity 初始容量；不足 kDefaultCapacity 时按 kDefaultCapacity 处理，
     *        超过 max_capacity 时按 max_capacity 处理。
     * @param max_capacity 最大容量；按 kExpandStep 向上取整。
     * @throw std::bad_alloc 初始存储分配失败时由标准库抛出。
     */
    explicit Buffer(std::size_t init_capacity = kDefaultCapacity, std::size_t max_capacity = kMaxCapacity);

    /**
     * @brief 释放存储。
     */
    ~Buffer();

    /**
     * @brief 可读字节数。
     * @return 当前可读字节数。
     */
    [[nodiscard]] std::size_t Size() const noexcept;

    /**
     * @brief 当前容量。
     * @return 已分配的字节数，恒为 kExpandStep 的整数倍。
     */
    [[nodiscard]] std::size_t Capacity() const noexcept;

    /**
     * @brief 从 GetWritePos() 起连续可写的字节数。
     * @return 连续可写字节数；已读但未压缩的前缀不计入，需要更多空间时调用 Reserve()。
     */
    [[nodiscard]] std::size_t CanWriteSize() const noexcept;

    /**
     * @brief 是否没有可读数据。
     * @return 没有可读数据返回 true。
     */
    [[nodiscard]] bool Empty() const noexcept;

    /**
     * @brief 可读数据是否已占满容量，且没有可回收的已读前缀。
     * @return 已无可写空间返回 true。
     */
    [[nodiscard]] bool Full() const noexcept;

    /**
     * @brief 可写位置；配合 CommitWrite() 使用。
     * @return 存储内下一个可写字节的地址。
     */
    [[nodiscard]] int8_t* GetWritePos() noexcept;

    /**
     * @brief 可写位置；配合 CommitWrite() 使用。
     * @return 存储内下一个可写字节的地址。
     */
    [[nodiscard]] const int8_t* GetWritePos() const noexcept;

    /**
     * @brief 可读位置；配合 ConsumeRead() 使用。
     * @return 有效数据起点的地址。
     */
    [[nodiscard]] int8_t* GetReadPos() noexcept;

    /**
     * @brief 可读位置；配合 ConsumeRead() 使用。
     * @return 有效数据起点的地址。
     */
    [[nodiscard]] const int8_t* GetReadPos() const noexcept;

    /**
     * @brief 非扩容写入。
     * @param from 源数据；size 非 0 时不能为 nullptr。
     * @param size 写入字节数。
     * @return 成功返回写入字节数；空间不足或参数非法返回 nullopt，状态不变。
     */
    [[nodiscard]] std::optional<std::size_t> WriteSome(const int8_t* from, std::size_t size);

    /**
     * @brief 写入，空间不足时自动扩容；源数据可以与自身存储重叠。
     * @param from 源数据；size 非 0 时不能为 nullptr。
     * @param size 写入字节数。
     * @return 成功返回 true；参数非法或超过最大容量返回 false，状态不变。
     */
    [[nodiscard]] bool Write(const int8_t* from, std::size_t size);

    /**
     * @brief 非扩容读取。
     * @param to 目标缓冲；size 非 0 时不能为 nullptr。
     * @param size 读取字节数。
     * @return 成功返回读取字节数；数据不足或参数非法返回 nullopt，状态不变。
     * @note 消费后只前移读位置，剩余数据保持原位。
     */
    [[nodiscard]] std::optional<std::size_t> ReadSome(int8_t* to, std::size_t size);

    /**
     * @brief 提交调用方直接写入 GetWritePos() 的字节。
     * @param size 本次写入的字节数。
     * @return 未超过 CanWriteSize() 返回 true；否则返回 false，状态不变。
     */
    [[nodiscard]] bool CommitWrite(std::size_t size) noexcept;

    /**
     * @brief 消费从 GetReadPos() 起的已读字节。
     * @param size 本次消费的字节数。
     * @return 未超过 Size() 返回 true；否则返回 false，状态不变。
     * @note 只前移读位置，剩余数据保持原位；已读前缀在下一次 Reserve() 或扩容时压缩。
     */
    [[nodiscard]] bool ConsumeRead(std::size_t size) noexcept;

    /**
     * @brief 保证从 GetWritePos() 起至少有 size 字节连续可写空间。
     * @param size 需要的连续可写字节数。
     * @return 成功返回 true；超过最大容量或容量无法表示返回 false，状态不变。
     * @note 空间不足时先压缩已读前缀，仍不足时按倍增扩容。
     */
    [[nodiscard]] bool Reserve(std::size_t size);

    /**
     * @brief 立即扩容。
     * @param expand_size 追加的容量；0 表示不扩容。
     * @return 成功或无需扩容返回 true；超过最大容量返回 false，状态不变。
     */
    [[nodiscard]] bool Expand(std::size_t expand_size = kExpandStep);

private:
    // 存储、游标与扩容策略；定义位于 buffer.cpp，对外不可见。
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

// 独占所有权的缓冲区。
using UniqueBuffer = std::unique_ptr<Buffer>;

NAMESPACE_EVEREST_END

#endif
