/**
 * @file safe_integral_calculator.h
 * @brief 整型饱和加、减、乘、除工具。
 */

#ifndef SAFE_INTEGRAL_CALCULATOR_H_
#define SAFE_INTEGRAL_CALCULATOR_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

#include "common/define.h"

NAMESPACE_EVEREST_BEGIN

/**
 * @brief 求整型绝对值，返回同宽度的无符号幅度。
 *
 * @tparam T 非 bool 整型。
 * @param value 输入值。
 * @return 绝对值的无符号表示；对 `min(T)` 返回 `|min(T)|`（如 `Abs<int64_t>(INT64_MIN)` 为 `2^63`）。
 * @note 纯函数，无异常，线程安全，可用于常量表达式；保留为公共接口以兼容外部调用。
 */
template <typename T>
[[nodiscard]] constexpr std::make_unsigned_t<std::remove_cvref_t<T>> Abs(T value) noexcept {
    static_assert(std::is_integral_v<std::remove_cvref_t<T>> && !std::is_same_v<std::remove_cvref_t<T>, bool>, "Abs requires a non-bool integral type");

    using RealType = std::remove_cvref_t<T>;
    using UnsignedType = std::make_unsigned_t<RealType>;

    if constexpr (std::is_signed_v<RealType>) {
        if (value < 0) {
            // -(value + 1) + 1 避免对 min(T) 直接取负导致溢出。
            return static_cast<UnsignedType>(-(value + 1)) + UnsignedType{1};
        }
    }

    return static_cast<UnsignedType>(value);
}

/**
 * @brief 整型饱和加、减、乘、除工具。
 *
 * 结果类型与饱和范围由第一个参数类型决定；混合符号、混合宽度的输入先按
 * 64 位无符号幅度与符号处理，再钳制到 T1 的范围。所有函数均为纯函数，
 * 不使用异常，可用于常量表达式求值，线程安全。除法遇到除数为 0 时返回
 * `std::nullopt`（原地接口返回 false），不做占位取值。
 */
class SafeIntegralCalculator {
public:
    /**
     * @brief 禁止实例化：本类只提供静态函数。
     */
    SafeIntegralCalculator() = delete;

    /**
     * @brief 禁止实例化：本类只提供静态函数。
     */
    ~SafeIntegralCalculator() = delete;

    /**
     * @brief 按第一个参数类型 T1 的范围做饱和加法。
     *
     * 实际结果超出 `[min(T1), max(T1)]` 时钳制到最近的边界。混合符号与混合宽度的输入
     * 先转为无符号幅度与符号再运算，避免有符号溢出与隐式转换带来的错误。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 加数类型，必须是非 bool 整型。
     * @param augend 被加数，同时决定结果类型与饱和范围。
     * @param addend 加数。
     * @return 饱和到 T1 范围的加法结果。
     * @note 纯函数，无异常，线程安全，可用于常量表达式。
     */
    template <typename T1, typename T2>
    [[nodiscard]] static constexpr T1 SafeAdd(T1 augend, T2 addend) noexcept;

    /**
     * @brief 按第一个参数类型 T1 的范围做饱和减法。
     *
     * 实际结果超出 `[min(T1), max(T1)]` 时钳制到最近的边界；无符号 T1 的负结果钳制到 0。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 减数类型，必须是非 bool 整型。
     * @param minuend 被减数，同时决定结果类型与饱和范围。
     * @param subtrahend 减数。
     * @return 饱和到 T1 范围的减法结果。
     * @note 纯函数，无异常，线程安全，可用于常量表达式。
     */
    template <typename T1, typename T2>
    [[nodiscard]] static constexpr T1 SafeSubtract(T1 minuend, T2 subtrahend) noexcept;

    /**
     * @brief 按第一个参数类型 T1 的范围做饱和乘法。
     *
     * 实际结果超出 `[min(T1), max(T1)]` 时钳制到最近的边界；乘零结果恒为 0。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 乘数类型，必须是非 bool 整型。
     * @param multiplicand 被乘数，同时决定结果类型与饱和范围。
     * @param multiplier 乘数。
     * @return 饱和到 T1 范围的乘法结果。
     * @note 纯函数，无异常，线程安全，可用于常量表达式。
     */
    template <typename T1, typename T2>
    [[nodiscard]] static constexpr T1 SafeMultiply(T1 multiplicand, T2 multiplier) noexcept;

    /**
     * @brief 按第一个参数类型 T1 的范围做饱和除法；除数为 0 时返回空。
     *
     * 结果向零截断；除数为 0 属于非法输入，返回 `std::nullopt`，不选择任何占位值。
     * `min(T1) / -1` 这类有符号除法溢出饱和到 `max(T1)`。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 除数类型，必须是非 bool 整型。
     * @param dividend 被除数，同时决定结果类型与饱和范围。
     * @param divisor 除数。
     * @return 成功返回饱和到 T1 范围的结果；除数为 0 返回 `std::nullopt`。
     * @note 纯函数，无异常，线程安全，可用于常量表达式；调用方必须处理除零失败。
     */
    template <typename T1, typename T2>
    [[nodiscard]] static constexpr std::optional<T1> SafeDivide(T1 dividend, T2 divisor) noexcept;

    /**
     * @brief 原地饱和加法。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 加数类型，必须是非 bool 整型。
     * @param augend 被加数，运算结果写回该引用。
     * @param addend 加数。
     * @note 纯函数语义，无异常，线程安全。
     */
    template <typename T1, typename T2>
    static constexpr void SafeAddAssign(T1& augend, T2 addend) noexcept;

    /**
     * @brief 原地饱和减法。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 减数类型，必须是非 bool 整型。
     * @param minuend 被减数，运算结果写回该引用。
     * @param subtrahend 减数。
     * @note 纯函数语义，无异常，线程安全。
     */
    template <typename T1, typename T2>
    static constexpr void SafeSubtractAssign(T1& minuend, T2 subtrahend) noexcept;

    /**
     * @brief 原地饱和乘法。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 乘数类型，必须是非 bool 整型。
     * @param multiplicand 被乘数，运算结果写回该引用。
     * @param multiplier 乘数。
     * @note 纯函数语义，无异常，线程安全。
     */
    template <typename T1, typename T2>
    static constexpr void SafeMultiplyAssign(T1& multiplicand, T2 multiplier) noexcept;

    /**
     * @brief 原地饱和除法。
     *
     * 除数为 0 时不修改 `dividend`，返回 false。
     *
     * @tparam T1 结果类型，必须是非 bool 整型。
     * @tparam T2 除数类型，必须是非 bool 整型。
     * @param dividend 被除数，成功时结果写回该引用。
     * @param divisor 除数。
     * @return 成功返回 true；除数为 0 返回 false 且不修改 `dividend`。
     * @note 纯函数语义，无异常，线程安全；调用方必须检查返回值。
     */
    template <typename T1, typename T2>
    [[nodiscard]] static constexpr bool SafeDivideAssign(T1& dividend, T2 divisor) noexcept;

private:
    /**
     * @brief 带符号的 64 位幅度表示。
     *
     * 内部运算统一用它表示操作数，避免混合符号与有符号溢出；幅度为 0 时符号无意义。
     */
    struct SignMagnitude {
        // 是否为负；0 视为非负。
        bool negative = false;
        // 无符号幅度。
        std::uint64_t magnitude = 0;
    };

    /**
     * @brief 判断有符号值是否为负；无符号值恒为非负。
     * @tparam T 非 bool 整型。
     * @param value 待判断的值。
     * @return 负数返回 true，其余返回 false。
     */
    template <typename T>
    static constexpr bool IsNegative(T value) noexcept;

    /**
     * @brief 把整型值拆成符号与 64 位幅度。
     * @tparam T 非 bool 整型。
     * @param value 输入值。
     * @return 拆分后的符号与幅度。
     */
    template <typename T>
    static constexpr SignMagnitude MakeSignMagnitude(T value) noexcept;

    /**
     * @brief 取整型值的无符号幅度，复用公共 Abs 并拓宽为 64 位。
     * @tparam T 非 bool 整型。
     * @param value 输入值。
     * @return 64 位无符号幅度。
     */
    template <typename T>
    static constexpr std::uint64_t Magnitude(T value) noexcept;

    /**
     * @brief 目标类型 T 的最大值幅度。
     * @tparam T 非 bool 整型。
     * @return `max(T)` 的无符号幅度。
     */
    template <typename T>
    static constexpr std::uint64_t MaxMagnitude() noexcept;

    /**
     * @brief 目标类型 T 的最小值幅度。
     * @tparam T 非 bool 整型。
     * @return `|min(T)|` 的无符号幅度；无符号类型返回 0。
     */
    template <typename T>
    static constexpr std::uint64_t MinMagnitude() noexcept;

    /**
     * @brief 以补码方式把符号与幅度还原为整型值。
     *
     * 只处理能由 T 表示的负值或非负值，调用前必须已用 ClampToT1 钳制到范围内。
     *
     * @tparam T 目标整型。
     * @param negative 是否为负。
     * @param magnitude 无符号幅度。
     * @return 还原后的整型值。
     */
    template <typename T>
    static constexpr T FromSignMagnitude(bool negative, std::uint64_t magnitude) noexcept;

    /**
     * @brief 把符号与幅度钳制到 T1 的表示范围。
     * @tparam T1 目标整型。
     * @param negative 结果是否为负。
     * @param magnitude 无符号幅度。
     * @return 饱和到 `[min(T1), max(T1)]` 的值。
     */
    template <typename T1>
    static constexpr T1 ClampToT1(bool negative, std::uint64_t magnitude) noexcept;

    /**
     * @brief 无符号幅度加法，溢出时饱和到 `uint64_t` 最大值。
     * @param lhs 左操作数幅度。
     * @param rhs 右操作数幅度。
     * @return 饱和后的和。
     */
    static constexpr std::uint64_t SaturatingAddMagnitude(std::uint64_t lhs, std::uint64_t rhs) noexcept;

    /**
     * @brief 无符号幅度乘法，溢出时饱和到 `uint64_t` 最大值。
     * @param lhs 左操作数幅度。
     * @param rhs 右操作数幅度。
     * @return 饱和后的积。
     */
    static constexpr std::uint64_t SaturatingMultiplyMagnitude(std::uint64_t lhs, std::uint64_t rhs) noexcept;

    /**
     * @brief 按符号合并两个带符号幅度，并钳制到 T1 范围。
     * @tparam T1 目标整型。
     * @param lhs 左操作数的符号与幅度。
     * @param rhs 右操作数的符号与幅度。
     * @return 饱和到 T1 范围的合并结果。
     */
    template <typename T1>
    static constexpr T1 Combine(const SignMagnitude& lhs, const SignMagnitude& rhs) noexcept;
};

template <typename T1, typename T2>
constexpr T1 SafeIntegralCalculator::SafeAdd(T1 augend, T2 addend) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeAdd requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeAdd requires a non-bool integral type for T2");

    return Combine<T1>(MakeSignMagnitude(augend), MakeSignMagnitude(addend));
}

template <typename T1, typename T2>
constexpr T1 SafeIntegralCalculator::SafeSubtract(T1 minuend, T2 subtrahend) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeSubtract requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeSubtract requires a non-bool integral type for T2");

    const SignMagnitude subtrahend_view = MakeSignMagnitude(subtrahend);
    // 减去一个数等价于加上它的相反数；幅度为 0 时符号无意义。
    const SignMagnitude negated_subtrahend{subtrahend_view.magnitude != 0 && !subtrahend_view.negative, subtrahend_view.magnitude};

    return Combine<T1>(MakeSignMagnitude(minuend), negated_subtrahend);
}

template <typename T1, typename T2>
constexpr T1 SafeIntegralCalculator::SafeMultiply(T1 multiplicand, T2 multiplier) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeMultiply requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeMultiply requires a non-bool integral type for T2");

    const SignMagnitude multiplicand_view = MakeSignMagnitude(multiplicand);
    const SignMagnitude multiplier_view = MakeSignMagnitude(multiplier);
    const std::uint64_t product = SaturatingMultiplyMagnitude(multiplicand_view.magnitude, multiplier_view.magnitude);

    return ClampToT1<T1>(multiplicand_view.negative != multiplier_view.negative, product);
}

template <typename T1, typename T2>
constexpr std::optional<T1> SafeIntegralCalculator::SafeDivide(T1 dividend, T2 divisor) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeDivide requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeDivide requires a non-bool integral type for T2");

    const SignMagnitude dividend_view = MakeSignMagnitude(dividend);
    const SignMagnitude divisor_view = MakeSignMagnitude(divisor);
    if (divisor_view.magnitude == 0) {
        return std::nullopt;
    }

    const std::uint64_t quotient = dividend_view.magnitude / divisor_view.magnitude;
    return ClampToT1<T1>(dividend_view.negative != divisor_view.negative, quotient);
}

template <typename T1, typename T2>
constexpr void SafeIntegralCalculator::SafeAddAssign(T1& augend, T2 addend) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeAddAssign requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeAddAssign requires a non-bool integral type for T2");

    augend = SafeAdd(augend, addend);
}

template <typename T1, typename T2>
constexpr void SafeIntegralCalculator::SafeSubtractAssign(T1& minuend, T2 subtrahend) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeSubtractAssign requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeSubtractAssign requires a non-bool integral type for T2");

    minuend = SafeSubtract(minuend, subtrahend);
}

template <typename T1, typename T2>
constexpr void SafeIntegralCalculator::SafeMultiplyAssign(T1& multiplicand, T2 multiplier) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeMultiplyAssign requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeMultiplyAssign requires a non-bool integral type for T2");

    multiplicand = SafeMultiply(multiplicand, multiplier);
}

template <typename T1, typename T2>
constexpr bool SafeIntegralCalculator::SafeDivideAssign(T1& dividend, T2 divisor) noexcept {
    static_assert(std::is_integral_v<T1> && !std::is_same_v<T1, bool>, "SafeDivideAssign requires a non-bool integral type for T1");
    static_assert(std::is_integral_v<T2> && !std::is_same_v<T2, bool>, "SafeDivideAssign requires a non-bool integral type for T2");

    const std::optional<T1> result = SafeDivide(dividend, divisor);
    if (!result.has_value()) {
        return false;
    }

    dividend = *result;
    return true;
}

template <typename T>
constexpr bool SafeIntegralCalculator::IsNegative(T value) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return value < 0;
    } else {
        return false;
    }
}

template <typename T>
constexpr std::uint64_t SafeIntegralCalculator::Magnitude(T value) noexcept {
    return static_cast<std::uint64_t>(Abs(value));
}

template <typename T>
constexpr SafeIntegralCalculator::SignMagnitude SafeIntegralCalculator::MakeSignMagnitude(T value) noexcept {
    return SignMagnitude{IsNegative(value), Magnitude(value)};
}

template <typename T>
constexpr std::uint64_t SafeIntegralCalculator::MaxMagnitude() noexcept {
    return static_cast<std::uint64_t>((std::numeric_limits<T>::max)());
}

template <typename T>
constexpr std::uint64_t SafeIntegralCalculator::MinMagnitude() noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return 0;
    } else {
        return MaxMagnitude<T>() + 1;
    }
}

template <typename T>
constexpr T SafeIntegralCalculator::FromSignMagnitude(bool negative, std::uint64_t magnitude) noexcept {
    if (!negative) {
        return static_cast<T>(magnitude);
    }

    // 幅度等于 |min(T)| 时直接返回 min(T)，避免对 min(T) 取负溢出。
    if (magnitude == MinMagnitude<T>()) {
        return (std::numeric_limits<T>::min)();
    }

    return static_cast<T>(-static_cast<T>(magnitude));
}

template <typename T1>
constexpr T1 SafeIntegralCalculator::ClampToT1(bool negative, std::uint64_t magnitude) noexcept {
    constexpr std::uint64_t kMaxMagnitude = MaxMagnitude<T1>();

    if (!negative) {
        if (magnitude > kMaxMagnitude) {
            return (std::numeric_limits<T1>::max)();
        }
        return static_cast<T1>(magnitude);
    }

    if constexpr (std::is_unsigned_v<T1>) {
        // 无符号目标类型无法表示负结果，钳制到下界 0。
        return (std::numeric_limits<T1>::min)();
    } else {
        constexpr std::uint64_t kMinMagnitude = MinMagnitude<T1>();
        if (magnitude > kMinMagnitude) {
            return (std::numeric_limits<T1>::min)();
        }
        return FromSignMagnitude<T1>(true, magnitude);
    }
}

constexpr std::uint64_t SafeIntegralCalculator::SaturatingAddMagnitude(std::uint64_t lhs, std::uint64_t rhs) noexcept {
    if (lhs > (std::numeric_limits<std::uint64_t>::max)() - rhs) {
        return (std::numeric_limits<std::uint64_t>::max)();
    }
    return lhs + rhs;
}

constexpr std::uint64_t SafeIntegralCalculator::SaturatingMultiplyMagnitude(std::uint64_t lhs, std::uint64_t rhs) noexcept {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    if (lhs > (std::numeric_limits<std::uint64_t>::max)() / rhs) {
        return (std::numeric_limits<std::uint64_t>::max)();
    }
    return lhs * rhs;
}

template <typename T1>
constexpr T1 SafeIntegralCalculator::Combine(const SignMagnitude& lhs, const SignMagnitude& rhs) noexcept {
    if (lhs.negative == rhs.negative) {
        return ClampToT1<T1>(lhs.negative, SaturatingAddMagnitude(lhs.magnitude, rhs.magnitude));
    }

    if (lhs.magnitude >= rhs.magnitude) {
        return ClampToT1<T1>(lhs.negative, lhs.magnitude - rhs.magnitude);
    }

    return ClampToT1<T1>(rhs.negative, rhs.magnitude - lhs.magnitude);
}

NAMESPACE_EVEREST_END

#endif
