/**
 * @file safe_integral_calculator_test.cpp
 * @brief SafeIntegralCalculator 的 GoogleTest 用例。
 */

#include "calculator/safe_integral_calculator.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

using everest::SafeIntegralCalculator;

// 覆盖全部固定宽度有符号与无符号整型，避免 char 符号性带来的差异。
using IntegralTypes = ::testing::Types<std::int8_t, std::int16_t, std::int32_t, std::int64_t, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>;

/**
 * @brief 按类型参数化的通用边界用例。
 */
template <typename T>
class SafeIntegralCalculatorTypeTest : public ::testing::Test {};

TYPED_TEST_SUITE(SafeIntegralCalculatorTypeTest, IntegralTypes);

TYPED_TEST(SafeIntegralCalculatorTypeTest, AddStaysInRange) {
    using T = TypeParam;

    const T result = SafeIntegralCalculator::SafeAdd<T>(T{1}, 1);

    EXPECT_EQ(result, static_cast<T>(2));
    EXPECT_TRUE((std::is_same_v<decltype(SafeIntegralCalculator::SafeAdd<T>(T{1}, 1)), T>));
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, AddSaturatesAtMax) {
    using T = TypeParam;

    const T maximum = (std::numeric_limits<T>::max)();

    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<T>(maximum, 1), maximum);
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, AddSaturatesAtMin) {
    using T = TypeParam;

    if constexpr (std::is_signed_v<T>) {
        const T minimum = (std::numeric_limits<T>::min)();

        EXPECT_EQ(SafeIntegralCalculator::SafeAdd<T>(minimum, -1), minimum);
    }
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, SubtractSaturates) {
    using T = TypeParam;

    if constexpr (std::is_signed_v<T>) {
        const T minimum = (std::numeric_limits<T>::min)();

        EXPECT_EQ(SafeIntegralCalculator::SafeSubtract<T>(minimum, 1), minimum);
    } else {
        EXPECT_EQ(SafeIntegralCalculator::SafeSubtract<T>(T{0}, 1), T{0});
    }
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, MultiplyZero) {
    using T = TypeParam;

    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<T>(T{0}, 12345), T{0});
    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<T>((std::numeric_limits<T>::max)(), 0), T{0});
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, MultiplySaturatesAtMax) {
    using T = TypeParam;

    const T maximum = (std::numeric_limits<T>::max)();

    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<T>(maximum, 2), maximum);
}

TYPED_TEST(SafeIntegralCalculatorTypeTest, DivideByZeroPolicy) {
    using T = TypeParam;

    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<T>(T{1}, 0), std::nullopt);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<T>(T{0}, 0), std::nullopt);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<T>(T{6}, 3), T{2});
}

TEST(SafeIntegralCalculatorTest, Int8Edges) {
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{127}, 1), std::int8_t{127});
    EXPECT_EQ(SafeIntegralCalculator::SafeSubtract<std::int8_t>(std::int8_t{-128}, 1), std::int8_t{-128});
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{127}, -1), std::int8_t{126});
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{-128}, 127), std::int8_t{-1});
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{-1}, 1), std::int8_t{0});
}

TEST(SafeIntegralCalculatorTest, Int64AddOverflow) {
    using I64 = std::int64_t;
    const I64 maximum = (std::numeric_limits<I64>::max)();
    const I64 minimum = (std::numeric_limits<I64>::min)();

    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<I64>(maximum, 1), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeSubtract<I64>(minimum, 1), minimum);
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<I64>(maximum, maximum), maximum);
}

TEST(SafeIntegralCalculatorTest, Int64Multiply) {
    using I64 = std::int64_t;
    const I64 maximum = (std::numeric_limits<I64>::max)();
    const I64 minimum = (std::numeric_limits<I64>::min)();

    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<I64>(minimum, -1), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<I64>(maximum, 2), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<I64>(minimum, 2), minimum);
}

TEST(SafeIntegralCalculatorTest, Int64DivideOverflow) {
    using I64 = std::int64_t;
    const I64 maximum = (std::numeric_limits<I64>::max)();
    const I64 minimum = (std::numeric_limits<I64>::min)();

    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<I64>(minimum, -1), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<I64>(minimum, 1), minimum);
}
TEST(SafeIntegralCalculatorTest, SignedDivisionTruncatesTowardZero) {
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(7, 2), 3);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(-7, 2), -3);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(7, -2), -3);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(-7, -2), 3);
}

TEST(SafeIntegralCalculatorTest, DivideByZeroSigned) {
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(1, 0), std::nullopt);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(-1, 0), std::nullopt);
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<int>(0, 0), std::nullopt);
}

TEST(SafeIntegralCalculatorTest, UnsignedSaturation) {
    using U64 = std::uint64_t;
    const U64 maximum = (std::numeric_limits<U64>::max)();

    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<U64>(maximum, 1), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<U64>(maximum, 2), maximum);
    EXPECT_EQ(SafeIntegralCalculator::SafeSubtract<U64>(U64{0}, 1), U64{0});
}

TEST(SafeIntegralCalculatorTest, UnsignedAddNegative) {
    using U64 = std::uint64_t;

    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<U64>(U64{5}, -3), U64{2});
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<U64>(U64{5}, -10), U64{0});
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<U64>(U64{2}, -2), U64{0});
}

TEST(SafeIntegralCalculatorTest, UnsignedDivideNegative) {
    using U64 = std::uint64_t;
    const U64 dividend = (std::numeric_limits<U64>::max)();

    EXPECT_EQ(SafeIntegralCalculator::SafeDivide<U64>(dividend, -1), U64{0});
    EXPECT_EQ(SafeIntegralCalculator::SafeDivide(dividend, -1), U64{0});
}

TEST(SafeIntegralCalculatorTest, MixedWidthAdd) {
    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{100}, std::int64_t{1000}), std::int8_t{127});
}

TEST(SafeIntegralCalculatorTest, MixedWidthMultiply) {
    EXPECT_EQ(SafeIntegralCalculator::SafeMultiply<std::int8_t>(std::int8_t{-100}, std::int64_t{-2}), std::int8_t{127});
}

TEST(SafeIntegralCalculatorTest, SignedPlusWideUnsigned) {
    const std::uint64_t wide = std::uint64_t{1} << 63;

    EXPECT_EQ(SafeIntegralCalculator::SafeAdd<std::int64_t>(std::int64_t{-1}, wide), (std::numeric_limits<std::int64_t>::max)());
}

TEST(SafeIntegralCalculatorTest, AddAssignSaturates) {
    std::int32_t value = (std::numeric_limits<std::int32_t>::max)();

    SafeIntegralCalculator::SafeAddAssign(value, 1);

    EXPECT_EQ(value, (std::numeric_limits<std::int32_t>::max)());
}

TEST(SafeIntegralCalculatorTest, SubtractAssignUnderflow) {
    std::uint32_t value = 0;

    SafeIntegralCalculator::SafeSubtractAssign(value, 1);

    EXPECT_EQ(value, std::uint32_t{0});
}

TEST(SafeIntegralCalculatorTest, MultiplyAssignSaturates) {
    std::int8_t value = 100;

    SafeIntegralCalculator::SafeMultiplyAssign(value, 2);

    EXPECT_EQ(value, std::int8_t{127});
}

TEST(SafeIntegralCalculatorTest, DivideAssignZeroDivisor) {
    int value = 5;

    const bool ok = SafeIntegralCalculator::SafeDivideAssign(value, 0);

    EXPECT_FALSE(ok);
    EXPECT_EQ(value, 5);
}

TEST(SafeIntegralCalculatorTest, DivideAssignSaturates) {
    std::int32_t value = (std::numeric_limits<std::int32_t>::min)();

    const bool ok = SafeIntegralCalculator::SafeDivideAssign(value, -1);

    EXPECT_TRUE(ok);
    EXPECT_EQ(value, (std::numeric_limits<std::int32_t>::max)());
}

TEST(SafeIntegralCalculatorTest, ConstexprEvaluation) {
    static_assert(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{100}, 100) == std::int8_t{127});
    static_assert(SafeIntegralCalculator::SafeMultiply<std::int8_t>(std::int8_t{-100}, std::int64_t{-2}) == std::int8_t{127});
    static_assert(SafeIntegralCalculator::SafeDivide<std::int64_t>((std::numeric_limits<std::int64_t>::min)(), -1) == (std::numeric_limits<std::int64_t>::max)());
    static_assert(!SafeIntegralCalculator::SafeDivide<std::int32_t>(1, 0).has_value());

    SUCCEED();
}

TEST(SafeIntegralCalculatorTest, ResultTypeIsFirstArgument) {
    static_assert(std::is_same_v<decltype(SafeIntegralCalculator::SafeAdd<std::int8_t>(std::int8_t{1}, std::int64_t{2})), std::int8_t>);

    SUCCEED();
}

TEST(SafeIntegralCalculatorTest, AbsMagnitude) {
    EXPECT_EQ(everest::Abs(std::int8_t{-5}), std::uint8_t{5});
    EXPECT_EQ(everest::Abs((std::numeric_limits<std::int64_t>::min)()), std::uint64_t{1} << 63);
    EXPECT_EQ(everest::Abs(std::uint32_t{7}), std::uint32_t{7});
}

TEST(SafeIntegralCalculatorTest, AbsReturnType) {
    static_assert(std::is_same_v<decltype(everest::Abs(std::int8_t{-1})), std::uint8_t>);

    SUCCEED();
}

TEST(SafeIntegralCalculatorTest, BoolRejected) {
    // SafeAdd(true, 1) 会被函数体内的 static_assert 在编译期拒绝，无法在运行时断言。
    GTEST_SKIP() << "static_assert 在编译期拒绝 bool 实参，无运行时断言";
}

TEST(SafeIntegralCalculatorTest, ClassNotConstructible) {
    static_assert(!std::is_default_constructible_v<SafeIntegralCalculator>);

    SUCCEED();
}
