# C++ 编码规范

依据 [Google C++ Style Guide](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/contents.html) 整理。新代码遵守；改既有文件时保持原风格。本仓库以 C++20 为准。

## 语言与工具链

- 语言标准：C++20。MSVC 使用 v143 工具集并开启 `/std:c++20`；CMake 使用 `CMAKE_CXX_STANDARD 20`。
- 源文件编码：UTF-8 无 BOM；MSVC 编译单元必须加 `/utf-8`；GCC/Clang 默认按 UTF-8 处理。
- 不使用 C++20 modules（与统一编译单元、VS 工程模型冲突）。
- 不使用 `<format>`，格式化统一用仓库内置的 fmt。
- 协程仅在明确设计异步模型时使用；asio 1.16.1 已提供 `co_spawn` / `awaitable`，不得随意混入现有回调模型。
- C++20 没有 `std::expected`；错误返回继续使用错误码、`std::optional` 或项目自定义结果类型。

## 头文件

- 自给自足，直接包含所用符号的头文件。
- 防护符：`<文件>_H_`，文件名为头文件名（不含扩展名），全部大写。
  - 例：`everest/net/session_base.h` → `SESSION_BASE_H_`；`everest/fuzzy_search/fuzzy_search_system.h` → `FUZZY_SEARCH_SYSTEM_H_`。
- 不使用 `#pragma once`。
- 只有被重写的组件采用上述规则；未重写的头文件暂时保持原保护符。
- 不同目录下出现同名头文件导致保护符冲突时，优先重命名文件；确需保留同名时，在保护符前加目录名作为例外。
- 不用前向声明，尤其不要为 `std::` 和其他项目实体做前向声明。
- 只内联约 10 行以内的小函数；含循环、`switch`、递归的不内联。
- 包含顺序（组间空行，组内字母序；路径相对源码根目录，不用 `.`、`..`）：依次为配套头文件、C 系统头文件、C++ 标准库、其他库、本项目头文件。

```cpp
#include "net/session_base.h"

#include <stdint.h>

#include <vector>

#include <asio.hpp>

#include "common/define.h"
```

## 作用域

- 统一使用扁平的 `namespace everest`，通过 `NAMESPACE_EVEREST_BEGIN` / `NAMESPACE_EVEREST_END` 进入和退出，不按目录拆分子命名空间；禁用 `using namespace` 和 inline namespace；命名空间内容不缩进。
- 仅当前 `.cpp` 使用的定义放匿名命名空间或加 `static`；头文件不用内部链接。
- 统一编译单元下所有 `.cpp` 属同一 TU，文件作用域 `static`/匿名命名空间不跨文件隔离；模块内同名 helper 会重定义，用类作用域、按文件命名的 internal 命名空间或唯一名字隔离。
- 非成员函数放 `everest` 命名空间。大量相关的静态函数可用不可实例化的工具类归组（构造函数 `= delete`，只含 static 成员，按职责拆分）；零星函数不要建类。类名本身就是一层作用域，冲突比扁平命名空间更少。
- 局部变量作用域尽量小，声明时初始化。
- 静态/全局变量仅允许可平凡析构；优先 `constexpr`/`constinit`；禁止用动态容器、智能指针做全局。
- 命名空间/类作用域的 `thread_local` 用编译期常量初始化。

## 类

- 构造/析构中不调用虚函数；可能失败的初始化用 `Init()` 或工厂。
- 单参数构造函数、转换运算符加 `explicit`。
- 明确拷贝/移动语义：可拷贝→声明拷贝；仅可移动→继承 `MoveOnly`；都不可→继承 `NonCopyable` 或 `= delete`；声明赋值运算符时同时提供对应构造函数。
- `struct` 只用于无不变式的被动数据；有行为/不变式用 `class`；有含义的多字段不用 `pair`/`tuple`。
- 优先组合；public 继承只表达 is-a；重写加 `override`/`final`，不重复 `virtual`；避免多重实现继承。
- 数据成员一律 `private`（常量除外）。
- 声明顺序 `public`→`protected`→`private`；类内按 类型、静态常量、工厂、构造/赋值、析构、成员函数、数据成员 排列。
- 运算符重载须符合内置语义；禁止重载 `&&`、`||`、逗号、一元 `&` 和自定义字面量。

## 函数

- 优先返回值，少用输出参数。输入：值或 `const&`；非可选输出：引用；可选输入：`std::optional`；可选输出：非 const 指针；仅“可能为空”时返回指针。输入参数放输出参数之前。
- 约 40 行考虑拆分。
- 重载须让调用点一眼可辨，否则用 `AppendString()`/`AppendInt()` 这类名字。
- 缺省参数尽量避免，优先重载；只用于非虚函数且默认值固定。

## 所有权

- 谁分配谁持有；他人用副本、指针或引用访问。
- 独占用 `std::unique_ptr`；真正共享才用 `std::shared_ptr`；禁用 `std::auto_ptr`。

## 其他特性

| 特性 | 规则 |
| --- | --- |
| 异常 | 不使用 |
| RTTI | 一般禁用（单测除外）；优先虚函数/访问者模式 |
| 转换 | 只用 `static_cast`/`const_cast`/`reinterpret_cast`；禁 C 风格转换 |
| 流 | 仅用于日志 |
| 自增 | 迭代器/模板类型用前置 `++i` |
| const | 不修改的参数、成员函数、构造后不变的数据成员 |
| constexpr | 用于真正常量和常量初始化 |
| 整型 | 默认 `int`；明确位宽用 `<cstdint>`；避免无符号（位集除外） |
| 宏 | 尽量避免；头文件除防护符外不定义宏 |
| 空值 | 指针 `nullptr`，空字符 `'\0'` |
| sizeof | 优先 `sizeof(var)` |
| auto | 只用于局部变量；不要 `auto x{...}` |
| 列表初始化 | 可用于聚合/容器/返回值/实参；注意 `initializer_list` 优先 |
| 右值引用 | 只用于移动构造/赋值；用 `std::move` 表示转移 |
| lambda | 逃逸作用域时显式捕获；`[&]` 只在不会悬垂时用 |
| 模板元编程 | 避免递归实例化、SFINAE、类型列表等复杂技巧 |
| 变长数组/alloca | 禁止；用 `std::vector` 或 `std::unique_ptr<T[]>` |

## 命名

| 对象 | 规则 | 示例 |
| --- | --- | --- |
| 文件 | 小写下划线；`.h`/`.cc`（本仓库用 `.cpp`） | `session_base.h` |
| 类型 | 大驼峰（类、结构体、枚举、using、类型模板参数） | `SessionBase` |
| 变量/参数 | 小写下划线 | `session_id` |
| 类成员 | 变量规则 + 末尾 `_`（静态成员同） | `session_id_` |
| 结构体成员 | 变量规则，不加 `_` | `num_entries` |
| 常量 | `k` + 大驼峰（const/constexpr/全局/静态） | `kMaxSize` |
| 函数 | 大驼峰 | `HandleAccept()` |
| 存取器 | 与变量一致 | `count()` / `set_count()` |
| 命名空间 | 小写下划线，扁平使用项目名 | `everest` |
| 枚举值 | `k` + 大驼峰（新代码） | `kOK` |
| 宏 | 全大写下划线 | `EVEREST_LOG_INFO` |
| 缩写 | 按单词处理 | `StartRpc`，非 `StartRPC` |

## 注释

- 注释分两类，边界固定：
  - Doxygen `/** ... */` 块注释：文件、类、结构体、联合体、枚举类型、所有函数（成员函数、自由函数、模板函数、构造/析构/运算符，以及 `.cpp` 文件作用域的辅助函数）。
  - 行注释 `//`：仅用于非 Doxygen 对象和函数体内部——成员变量（含静态数据成员）、结构体字段、枚举值、`using`/`typedef` 别名、常量、宏、命名空间、局部变量、实现说明、`// TODO` / `// DEPRECATED`。
- Doxygen 一律使用多行块，不用 `/** @brief ... */` 单行形式；命令统一使用 `@` 风格（`@brief`、`@param`、`@return`），不使用 `\brief` 风格。
- 至少包含 `@brief`；有参数用 `@param`，有返回值用 `@return`，有模板参数用 `@tparam`；可选 `@note`/`@warning`/`@see`/`@deprecated`。详细说明直接写在 `@brief` 下方空行后，不用 `@details`。
- 文件：`@file` + `@brief`；新建或重写的文件必须有，不写 `@author`/`@date`/`@version`。
- 函数：Doxygen 写在声明处；`.cpp` 中的定义不重复 Doxygen，实现说明用 `//`；只在 `.cpp` 中出现的函数（文件作用域、匿名命名空间）在定义处写 Doxygen。
- 注释写在被注释代码的上方，不写在行尾；枚举值同样写在值上方，不用 `///<`。`}  // namespace`、`#endif  // XXX_H_` 等收尾标记也属于行尾注释，禁止使用；确需说明时写在对应代码上方或直接省略。
- 内容要求：类/结构体写用途、用法、线程安全前提；函数写功能、参数所有权/生命周期、空指针、线程安全；成员变量/字段写特殊值（`nullptr`/`-1`）、单位、生命周期；枚举值写含义。
- 解释“为什么”，不翻译代码；`// TODO(姓名/issue): 说明`；`// DEPRECATED(姓名): 迁移说明`。
- 完整句子，标点拼写正确。

```cpp
/**
 * @file session_base.h
 * @brief TCP 会话基类。
 */

/**
 * @brief 会话类型。
 */
enum class SessionType {
    // 未知类型。
    kUnknown,
    // 客户端连接。
    kClient,
};

/**
 * @brief 持有 socket 并驱动异步读写的会话基类。
 *
 * 线程安全：成员函数只能在会话所属的 io_context 线程调用。
 */
class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    // 全局唯一的会话 id。
    using SessionId = uint64_t;

    /**
     * @brief 构造会话。
     * @param service_id 所属 service 的 id。
     */
    explicit SessionBase(ServiceId service_id);

    /**
     * @brief 开始异步读取。
     */
    void Read();

private:
    // 会话 id；0 表示尚未分配。
    SessionId session_id_ = 0;
    // 关闭标记；为 true 后不再发起新的读写。
    bool closed_ = false;
};
```

只在 `.cpp` 中出现的文件作用域辅助函数，在定义处写 Doxygen：

```cpp
/**
 * @brief 校验会话 id 是否有效。
 * @param session_id 待校验的 id。
 * @return 有效返回 true。
 */
static bool IsValidSessionId(SessionBase::SessionId session_id) {
    return session_id != 0;
}

// TODO(wangshizhu): 支持大小写不敏感，见 #123。
```

## 格式

- 缩进使用 4 个空格，禁止 Tab。
- 访问说明符 `public:` / `protected:` / `private:` 顶格，不缩进；顺序 public→protected→private，除第一个外前空一行。
- 单行不超过 180 字符；超过时换行，续行缩进 4 格。
- 源文件使用 UTF-8，不带 BOM；换行符使用 LF。`.bat` / `.cmd` 使用 CRLF，且内容必须为纯 ASCII。
- MSVC 编译单元必须启用 `/utf-8`；GCC / Clang 默认按 UTF-8 处理。
- 非 ASCII 文本可以出现在注释、日志和测试数据中，不硬编码 UI 文本。
- 函数名后紧跟 `(`；`)` 与 `{` 间一个空格；换行参数缩进 4 格；调用和初始化列表同函数调用。
- 条件/循环：关键字后空格；`{` 与语句同行；`else` 跟 `}` 同行；某分支用花括号则全用。
- `switch` 覆盖所有枚举值，不可达的 `default` 用 `assert(false)`；空循环体用 `{}` 或 `continue`。
- 指针/引用统一写成 `char* c`（星号靠类型）；一元 `*`/`&` 后不加空格；不要 `int x, *y;`。
- 布尔用 `&&`/`||`/`!`；`return` 不加多余括号。
- 预处理指令顶格。
- 构造初始化列表放不下时缩进 4 格，每行一个成员。
- 不留行尾空格；二元运算符两侧空格；模板尖括号内不加空格。
- 函数间最多空 2 行；函数体首尾不空行。
- 可用 `clang-format` / `cpplint`；误报加 `// NOLINT`。

```cpp
class SessionBase : public std::enable_shared_from_this<SessionBase> {
public:
    explicit SessionBase(ServiceId service_id);

    bool Closed() const { return closed_; }

private:
    ServiceId service_id_;
    bool closed_ = false;
};

void SessionBase::DoRead() {
    if (closed_) {
        return;
    }

    socket_->async_read_some(buffer, [this](const ErrorCode& ec, size_t bytes) {
        if (ec) {
            Close();
        }
    });
}
```

## 特例

- 未参与重写的既有文件：修改时保持原风格，不整文件重排。
- 被 `rewrite_system_component` 计划并由 `code-impl` 执行的文件：按本规范整体改写，包括 UTF-8（无 BOM）、LF、4 空格、头文件保护符和 Doxygen 注释。
- `everest/` 源码已统一为 UTF-8（无 BOM）；Tab / CRLF 仍在组件重写时转换，未重写文件暂时保持原样。
- Windows：不用匈牙利命名；调 Windows API 可用 `DWORD`/`HANDLE`；警告级别 ≥3 且视为错误；避免非标准扩展。
