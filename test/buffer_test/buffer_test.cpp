/**
 * @file buffer_test.cpp
 * @brief buffer 组件的 GoogleTest 用例：覆盖读写、扩容、越界失败与 protobuf 序列化。
 */

#include "buffer/buffer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <google/protobuf/any.pb.h>
#include <gtest/gtest.h>

#include "packet/packet_serialize.h"

// 被测类型。
using Buffer = everest::Buffer;

namespace {

/**
 * @brief 构造确定内容的测试数据。
 * @param size 字节数。
 * @param seed 内容种子；相同参数的结果稳定。
 * @return 填充后的数据。
 */
std::vector<std::int8_t> MakeData(std::size_t size, std::int8_t seed) {
    std::vector<std::int8_t> data(size);
    for (std::size_t i = 0; i < size; ++i) {
        data[i] = static_cast<std::int8_t>((seed + static_cast<std::int8_t>(i % 64)) & 0x7f);
    }

    return data;
}

}

TEST(BufferTest, DefaultConstruction) {
    const Buffer buffer;

    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_TRUE(buffer.Empty());
    EXPECT_FALSE(buffer.Full());
    EXPECT_EQ(buffer.CanWriteSize(), Buffer::kDefaultCapacity);
}

TEST(BufferTest, CapacityRoundedToStep) {
    const Buffer buffer(600, 4096);

    EXPECT_EQ(buffer.Capacity(), 1024u);
}

TEST(BufferTest, WriteSomeCommitsData) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(4, 1);

    const std::optional<std::size_t> written = buffer.WriteSome(data.data(), data.size());

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, 4u);
    EXPECT_EQ(buffer.Size(), 4u);
    EXPECT_EQ(buffer.CanWriteSize(), Buffer::kDefaultCapacity - 4);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, WriteExpandsAndKeepsData) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(1000, 3);

    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_GE(buffer.Capacity(), data.size());
    EXPECT_EQ(buffer.Size(), data.size());
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, WriteBeyondMaxFails) {
    Buffer buffer(512, 512);
    const std::vector<std::int8_t> data = MakeData(1024, 0);

    EXPECT_FALSE(buffer.Write(data.data(), data.size()));

    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), 512u);
}

TEST(BufferTest, ReadSomeAdvancesReadPos) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(8, 5);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    std::vector<std::int8_t> out(4, 0);
    const std::optional<std::size_t> read = buffer.ReadSome(out.data(), out.size());

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, 4u);
    EXPECT_EQ(buffer.Size(), 4u);
    EXPECT_EQ(buffer.CanWriteSize(), Buffer::kDefaultCapacity - data.size());
    EXPECT_EQ(std::memcmp(out.data(), data.data(), 4), 0);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data() + 4, 4), 0);
}

TEST(BufferTest, ConsumeReadKeepsDataInPlace) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(8, 12);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    ASSERT_TRUE(buffer.ConsumeRead(4));

    EXPECT_EQ(buffer.Size(), 4u);
    EXPECT_EQ(buffer.CanWriteSize(), Buffer::kDefaultCapacity - data.size());
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data() + 4, 4), 0);

    const std::vector<std::int8_t> tail = MakeData(4, 33);
    ASSERT_TRUE(buffer.Write(tail.data(), tail.size()));

    EXPECT_EQ(buffer.Size(), 8u);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data() + 4, 4), 0);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos() + 4, tail.data(), 4), 0);
}

TEST(BufferTest, WriteAfterConsumeReusesFront) {
    Buffer buffer;
    const std::vector<std::int8_t> first = MakeData(Buffer::kDefaultCapacity, 10);
    ASSERT_TRUE(buffer.Write(first.data(), first.size()));
    ASSERT_TRUE(buffer.Full());

    ASSERT_TRUE(buffer.ConsumeRead(256));

    const std::vector<std::int8_t> second = MakeData(256, 90);
    ASSERT_TRUE(buffer.Write(second.data(), second.size()));

    EXPECT_EQ(buffer.Size(), Buffer::kDefaultCapacity);
    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), first.data() + 256, 256), 0);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos() + 256, second.data(), 256), 0);
}

TEST(BufferTest, FullExcludesReclaimablePrefix) {
    Buffer buffer(512, 512);
    const std::vector<std::int8_t> data = MakeData(512, 31);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));
    ASSERT_TRUE(buffer.Full());

    ASSERT_TRUE(buffer.ConsumeRead(256));

    EXPECT_FALSE(buffer.Full());
    EXPECT_EQ(buffer.CanWriteSize(), 0u);

    const std::vector<std::int8_t> tail = MakeData(256, 71);
    ASSERT_TRUE(buffer.Write(tail.data(), tail.size()));

    EXPECT_TRUE(buffer.Full());
    EXPECT_EQ(buffer.Capacity(), 512u);
}

TEST(BufferTest, ReserveCompactsWithoutGrowing) {
    Buffer buffer(512, 512);
    const std::vector<std::int8_t> data = MakeData(512, 21);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));
    ASSERT_TRUE(buffer.ConsumeRead(512));

    const std::vector<std::int8_t> one = MakeData(1, 44);
    ASSERT_TRUE(buffer.Write(one.data(), one.size()));

    EXPECT_EQ(buffer.Capacity(), 512u);
    EXPECT_EQ(buffer.Size(), 1u);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), one.data(), 1), 0);
}

TEST(BufferTest, ReserveDoublesCapacity) {
    Buffer buffer(512, 8192);
    const std::vector<std::int8_t> first = MakeData(513, 15);
    ASSERT_TRUE(buffer.Write(first.data(), first.size()));
    EXPECT_EQ(buffer.Capacity(), 1024u);

    const std::vector<std::int8_t> second = MakeData(512, 61);
    ASSERT_TRUE(buffer.Write(second.data(), second.size()));

    EXPECT_EQ(buffer.Size(), 1025u);
    EXPECT_EQ(buffer.Capacity(), 2048u);
}

TEST(BufferTest, ReserveAtMaxFails) {
    Buffer buffer(512, 512);
    const std::vector<std::int8_t> data = MakeData(512, 51);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_FALSE(buffer.Reserve(1));
    EXPECT_EQ(buffer.Capacity(), 512u);
    EXPECT_EQ(buffer.Size(), 512u);
}

TEST(BufferTest, CommitWriteAfterDirectWrite) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(16, 7);

    std::memcpy(buffer.GetWritePos(), data.data(), data.size());

    ASSERT_TRUE(buffer.CommitWrite(data.size()));

    EXPECT_EQ(buffer.Size(), data.size());
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, ReserveGrowsAndKeepsData) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(100, 9);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    ASSERT_TRUE(buffer.Reserve(2000));

    EXPECT_GE(buffer.Capacity(), 2100u);
    EXPECT_EQ(buffer.Size(), data.size());
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, ExpandByStep) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(64, 2);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    const std::size_t capacity = buffer.Capacity();
    ASSERT_TRUE(buffer.Expand());
    EXPECT_EQ(buffer.Capacity(), capacity + Buffer::kExpandStep);

    const std::size_t expanded = buffer.Capacity();
    ASSERT_TRUE(buffer.Expand(1024));
    EXPECT_EQ(buffer.Capacity(), expanded + 1024);
    EXPECT_EQ(buffer.Size(), data.size());
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, FullAndEmpty) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(buffer.Capacity(), 4);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_TRUE(buffer.Full());
    EXPECT_FALSE(buffer.Empty());

    ASSERT_TRUE(buffer.ConsumeRead(data.size()));

    EXPECT_FALSE(buffer.Full());
    EXPECT_TRUE(buffer.Empty());
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST(BufferTest, DestructorReleasesStorage) {
    for (int i = 0; i < 1000; ++i) {
        Buffer buffer(0, 4096);
        const std::vector<std::int8_t> data = MakeData(300, static_cast<std::int8_t>(i % 64));
        ASSERT_TRUE(buffer.Write(data.data(), data.size()));
        ASSERT_TRUE(buffer.Expand());
    }

    SUCCEED();
}

TEST(BufferTest, SerializerRoundTrip) {
    ::google::protobuf::Any source;
    source.set_type_url("type.googleapis.com/test.BufferTest");
    source.set_value(std::string(32, 'x'));

    Buffer buffer;
    buffer << source;
    ASSERT_GT(buffer.Size(), 0u);

    ::google::protobuf::Any target;
    buffer >> target;

    EXPECT_EQ(target.type_url(), source.type_url());
    EXPECT_EQ(target.value(), source.value());
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST(BufferTest, NotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<Buffer>);
    static_assert(!std::is_copy_assignable_v<Buffer>);
    static_assert(!std::is_move_constructible_v<Buffer>);
    static_assert(!std::is_move_assignable_v<Buffer>);

    SUCCEED();
}

TEST(BufferTest, HeaderIsSelfContained) {
    // buffer.h 是本文件的首个 include，能编译即证明头文件自包含。
    EXPECT_EQ(Buffer::kExpandStep, 512u);
    EXPECT_EQ(Buffer::kMaxCapacity, 2u * 1024u * 1024u);
}

TEST(BufferTest, WriteNullWithNonZeroFails) {
    Buffer buffer;
    const std::size_t capacity = buffer.Capacity();

    EXPECT_FALSE(buffer.Write(nullptr, 1));
    EXPECT_FALSE(buffer.WriteSome(nullptr, 1).has_value());
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), capacity);
}

TEST(BufferTest, WriteZeroBytesSucceeds) {
    Buffer buffer;

    EXPECT_TRUE(buffer.Write(nullptr, 0));

    const std::optional<std::size_t> written = buffer.WriteSome(nullptr, 0);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, 0u);
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
}

TEST(BufferTest, WriteSomeInsufficientKeepsState) {
    Buffer buffer(512, 512);
    const std::vector<std::int8_t> data = MakeData(513, 0);

    EXPECT_FALSE(buffer.WriteSome(data.data(), data.size()).has_value());
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), 512u);
}

TEST(BufferTest, ReadSomeInsufficientKeepsState) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(2, 1);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    std::vector<std::int8_t> out(3, 0);

    EXPECT_FALSE(buffer.ReadSome(out.data(), out.size()).has_value());
    EXPECT_EQ(buffer.Size(), 2u);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, ReadSomeNullFails) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(4, 2);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_FALSE(buffer.ReadSome(nullptr, 1).has_value());
    EXPECT_EQ(buffer.Size(), 4u);
}

TEST(BufferTest, CommitWriteBeyondCanWriteFails) {
    Buffer buffer;
    const std::int8_t* write_pos = buffer.GetWritePos();

    EXPECT_FALSE(buffer.CommitWrite(buffer.CanWriteSize() + 1));
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.GetWritePos(), write_pos);
}

TEST(BufferTest, ConsumeReadBeyondSizeFails) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(4, 3);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_FALSE(buffer.ConsumeRead(buffer.Size() + 1));
    EXPECT_EQ(buffer.Size(), 4u);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), data.size()), 0);
}

TEST(BufferTest, BoundaryCommitAndConsume) {
    Buffer buffer;

    ASSERT_TRUE(buffer.CommitWrite(buffer.CanWriteSize()));
    EXPECT_EQ(buffer.Size(), Buffer::kDefaultCapacity);

    ASSERT_TRUE(buffer.ConsumeRead(buffer.Size()));
    EXPECT_EQ(buffer.Size(), 0u);
}

TEST(BufferTest, ReserveBeyondMaxFails) {
    Buffer buffer(512, 1024);
    const std::vector<std::int8_t> data = MakeData(100, 4);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    EXPECT_FALSE(buffer.Reserve(1025));
    EXPECT_EQ(buffer.Capacity(), 512u);
    EXPECT_EQ(buffer.Size(), data.size());
}

TEST(BufferTest, ExpandZeroDoesNothing) {
    Buffer buffer;

    EXPECT_TRUE(buffer.Expand(0));
    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
}

TEST(BufferTest, ExpandBeyondMaxFails) {
    Buffer buffer(512, 1024);

    EXPECT_FALSE(buffer.Expand(1024));
    EXPECT_EQ(buffer.Capacity(), 512u);
}

TEST(BufferTest, ExpandOverflowFails) {
    Buffer buffer;

    EXPECT_FALSE(buffer.Expand(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
}

TEST(BufferTest, ReserveOverflowFails) {
    Buffer buffer;

    EXPECT_FALSE(buffer.Reserve(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);
}

TEST(BufferTest, MaxSmallerThanInit) {
    const Buffer buffer(4096, 512);

    EXPECT_EQ(buffer.Capacity(), 512u);
}

TEST(BufferTest, ZeroCapacityNormalized) {
    Buffer buffer(0, 0);

    EXPECT_EQ(buffer.Capacity(), Buffer::kDefaultCapacity);

    const std::vector<std::int8_t> data = MakeData(16, 6);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));
    EXPECT_EQ(buffer.Size(), data.size());
}

TEST(BufferTest, SelfWriteThroughExpansion) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(500, 8);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    const std::size_t size = buffer.Size();
    const std::int8_t* self = buffer.GetReadPos();
    ASSERT_TRUE(buffer.Write(self, size));

    EXPECT_EQ(buffer.Size(), size * 2);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos(), data.data(), size), 0);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos() + size, data.data(), size), 0);
}

TEST(BufferTest, WriteSomeFromOwnReadRegion) {
    Buffer buffer;
    const std::vector<std::int8_t> data = MakeData(8, 11);
    ASSERT_TRUE(buffer.Write(data.data(), data.size()));

    const std::optional<std::size_t> written = buffer.WriteSome(buffer.GetReadPos(), 4);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, 4u);
    EXPECT_EQ(buffer.Size(), 12u);
    EXPECT_EQ(std::memcmp(buffer.GetReadPos() + 8, data.data(), 4), 0);
}

TEST(BufferTest, SerializerOversizeNoPartialCommit) {
    Buffer buffer(512, 1024);
    ::google::protobuf::Any source;
    source.set_type_url("type.googleapis.com/test.BufferTest");
    source.set_value(std::string(2000, 'y'));

    const std::size_t capacity = buffer.Capacity();
    buffer << source;

    EXPECT_EQ(buffer.Size(), 0u);
    EXPECT_EQ(buffer.Capacity(), capacity);
}
