# safe_integral_calculator

整型饱和加、减、乘、除工具，header-only，无异常，全部接口为 `constexpr noexcept`。

结果类型和饱和范围由**第一个参数的类型** `T1` 决定；第二个参数 `T2` 可以是任意非 `bool` 整型，宽度和符号都可以不同。除法用 `std::optional<T1>` 表达除零失败。

```cpp
#include "calculator/safe_integral_calculator.h"

using everest::SafeIntegralCalculator;

int32_t value = SafeIntegralCalculator::SafeAdd<std::int32_t>(1, 2);  // 3
```

## 规则

- 结果类型 = `T1`，饱和范围 = `[min(T1), max(T1)]`。
- 参数按值接收，`T1/T2` 自动去掉 `const`/引用。
- 混合符号、混合宽度先转成「64 位无符号幅度 + 符号」再运算，不依赖隐式转换。
- `SafeDivide` 返回 `std::optional<T1>`；除数为 0 时返回 `std::nullopt`，不选占位值。
- `SafeDivideAssign` 返回 `bool`；除数为 0 时返回 `false` 且不修改原值。
- `bool` 实参由 `static_assert` 在编译期拒绝；`SafeIntegralCalculator` 不可实例化。
- 纯函数，线程安全，可用于 `static_assert` 常量求值。

## 饱和规则

只有数学结果存在但不可表示时才饱和；除数为 0 属于非法输入，不走饱和。

| 实际结果 | 返回 |
| --- | --- |
| 落在 `[min(T1), max(T1)]` | 实际结果 |
| 大于 `max(T1)` | `max(T1)` |
| 小于 `min(T1)` | `min(T1)` |
| 无符号 `T1` 的负结果 | `0`（即 `min(T1)`） |

除法除数为 0：`SafeDivide` 返回 `std::nullopt`，`SafeDivideAssign` 返回 `false` 且保持原值。

## 与原生运算对比

同样的写法，原生 C++ 常常给出非预期结果：窄类型先整型提升、再按 `2^N` 取模截断，无符号静默回绕，有符号溢出和除零则是未定义行为。本工具统一给确定结果。

| 原生写法 | 原生结果（非预期） | 本工具结果 |
| --- | --- | --- |
| `uint8_t r = uint8_t{0} - 1;` | `255` | `SafeSubtract<uint8_t>(0, 1)` → `0` |
| `uint8_t r = uint8_t{255} + 1;` | `0` | `SafeAdd<uint8_t>(255, 1)` → `255` |
| `int8_t r = int8_t{127} + 1;` | `-128` | `SafeAdd<int8_t>(int8_t{127}, 1)` → `127` |
| `uint8_t r = uint8_t{200} * 2;` | `144` | `SafeMultiply<uint8_t>(200, 2)` → `255` |
| `int8_t r = int8_t{-100} * int64_t{-2};` | `-56` | `SafeMultiply<int8_t>(int8_t{-100}, int64_t{-2})` → `127` |
| `uint32_t r = uint32_t{10} - int32_t{100};` | `4294967206` | `SafeSubtract<uint32_t>(10, 100)` → `0` |
| `uint64_t r = UINT64_MAX * 2;` | `18446744073709551614` | `SafeMultiply<uint64_t>(UINT64_MAX, 2)` → `UINT64_MAX` |
| `int32_t{INT32_MAX} + 1` | 未定义行为（有符号溢出） | `SafeAdd<int32_t>(INT32_MAX, 1)` → `INT32_MAX` |
| `int32_t{INT32_MIN} / -1` | 未定义行为（有符号溢出） | `SafeDivide<int32_t>(INT32_MIN, -1)` → `INT32_MAX` |
| `int32_t{5} / 0` | 未定义行为 | `SafeDivide<int32_t>(5, 0)` → `std::nullopt` |

## SafeAdd

| 表达式 | 结果 |
| --- | --- |
| `SafeAdd<int32_t>(1, 2)` | `3` |
| `SafeAdd<int8_t>(int8_t{127}, 1)` | `127` |
| `SafeAdd<int8_t>(int8_t{-128}, -1)` | `-128` |
| `SafeAdd<uint64_t>(uint64_t{5}, -3)` | `2` |
| `SafeAdd<uint64_t>(uint64_t{5}, -10)` | `0` |
| `SafeAdd<uint64_t>(UINT64_MAX, 1)` | `UINT64_MAX` |
| `SafeAdd<int8_t>(int8_t{100}, int64_t{1000})` | `127`（混合宽度） |
| `SafeAdd<int64_t>(int64_t{-1}, uint64_t{1} << 63)` | `INT64_MAX`（混合符号+宽度） |

## SafeSubtract

| 表达式 | 结果 |
| --- | --- |
| `SafeSubtract<uint32_t>(uint32_t{10}, int32_t{100})` | `uint32_t{0}` |
| `SafeSubtract<int32_t>(10, 100)` | `-90` |
| `SafeSubtract<int8_t>(int8_t{-128}, 1)` | `-128` |
| `SafeSubtract<int32_t>(INT32_MIN, -1)` | `INT32_MIN + 1`（`-2147483647`） |
| `SafeSubtract<uint64_t>(uint64_t{0}, 1)` | `0` |
| `SafeSubtract<uint64_t>(uint64_t{0}, -1)` | `1` |
| `SafeSubtract<int64_t>(INT64_MAX, INT64_MIN)` | `INT64_MAX` |

## SafeMultiply

| 表达式 | 结果 |
| --- | --- |
| `SafeMultiply<int32_t>(0, 12345)` | `0` |
| `SafeMultiply<int8_t>(int8_t{-100}, int64_t{-2})` | `127` |
| `SafeMultiply<int8_t>(int8_t{-128}, -1)` | `127` |
| `SafeMultiply<int32_t>(INT32_MIN, 2)` | `INT32_MIN` |
| `SafeMultiply<int64_t>(INT64_MIN, -1)` | `INT64_MAX` |
| `SafeMultiply<uint64_t>(UINT64_MAX, 2)` | `UINT64_MAX` |

## SafeDivide

返回 `std::optional<T1>`。除法先按幅度相除，再补符号，结果**向零截断**；除数为 0 返回 `std::nullopt`。

| 表达式 | 结果 |
| --- | --- |
| `SafeDivide<int32_t>(7, 2)` | `3` |
| `SafeDivide<int32_t>(-7, 2)` | `-3` |
| `SafeDivide<int32_t>(7, -2)` | `-3` |
| `SafeDivide<int32_t>(-7, -2)` | `3` |
| `SafeDivide<int32_t>(5, 0)` | `std::nullopt` |
| `SafeDivide<int32_t>(-5, 0)` | `std::nullopt` |
| `SafeDivide<int32_t>(0, 0)` | `std::nullopt` |
| `SafeDivide<int32_t>(INT32_MIN, -1)` | `INT32_MAX` |
| `SafeDivide<int64_t>(INT64_MIN, 1)` | `INT64_MIN` |
| `SafeDivide<uint32_t>(uint32_t{10}, -2)` | `0`（无符号负结果） |
| `SafeDivide<uint64_t>(UINT64_MAX, -1)` | `0`（无符号负结果） |

```cpp
const std::optional<int32_t> quotient = SafeIntegralCalculator::SafeDivide<int32_t>(total, count);
if (!quotient.has_value()) {
    // count == 0，非法输入
    return;
}

const int32_t average = *quotient;
```

## 原地接口

直接写回传入的左值。`SafeDivideAssign` 返回 `bool`，其余三个没有返回值。

| 表达式 | 运算后 |
| --- | --- |
| `int32_t v = INT32_MAX; SafeAddAssign(v, 1);` | `v == INT32_MAX` |
| `uint32_t v = 0; SafeSubtractAssign(v, 1);` | `v == 0` |
| `int8_t v = 100; SafeMultiplyAssign(v, 2);` | `v == 127` |
| `int32_t v = INT32_MIN; SafeDivideAssign(v, -1);` | 返回 `true`，`v == INT32_MAX` |
| `int32_t v = 5; SafeDivideAssign(v, 0);` | 返回 `false`，`v == 5`（不修改） |

## Abs

返回**同宽度的无符号幅度**。对 `min(T)` 也能正确返回 `|min(T)|`。

| 表达式 | 结果 |
| --- | --- |
| `Abs(int8_t{-5})` | `uint8_t{5}` |
| `Abs(uint32_t{7})` | `uint32_t{7}` |
| `Abs(int64_t{INT64_MIN})` | `uint64_t{1} << 63` |

## 常用写法

```cpp
// 结果类型取窄类型：显式写 T1，避免结果被 T2 撑大。
const int8_t r = everest::SafeIntegralCalculator::SafeAdd<int8_t>(100, 1000);  // 127

// 常量求值。
static_assert(everest::SafeIntegralCalculator::SafeMultiply<int8_t>(int8_t{-100}, -2) == 127);

// 除零必须处理：失败返回 nullopt，不会伪造结果。
const std::optional<int32_t> quotient = everest::SafeIntegralCalculator::SafeDivide<int32_t>(total, count);
if (!quotient.has_value()) {
    // count == 0，非法输入
    return;
}
```

## 注意

- 结果类型不是 `T2`：`SafeAdd<uint64_t>(5, -3)` 的结果是 `uint64_t`，不是 `int`。
- 无符号运算不会回绕：`uint32_t{10} - int32_t{100}` 得到 `0`，不是 `4294967206`。
- `min(T1) / -1`、`min(T1) * -1` 都会饱和到 `max(T1)`，不会触发未定义行为。
- 除法除数为 0 返回 `std::nullopt` / `false`，不抛异常，也不会返回 `INT_MAX` 之类的占位值。
