# C++ 编码规范

依据 [Google C++ Style Guide](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/contents.html) 整理。新代码遵守；改既有文件时保持原风格。本仓库以 C++17 为准。

## 头文件

- 自给自足，直接包含所用符号的头文件。
- 防护符：`EVEREST_NET_SESSION_BASE_H_`（`项目_路径_文件名_H_`）。
- 不用前向声明，尤其不要为 `std::` 和其他项目实体做前向声明。
- 只内联约 10 行以内的小函数；含循环、`switch`、递归的不内联。
- 包含顺序（组间空行，组内字母序；路径相对源码根目录，不用 `.`、`..`）：

```cpp
#include "net/session_base.h"  // 1. 配套头文件

#include <stdint.h>            // 2. C 系统头文件

#include <vector>              // 3. C++ 标准库

#include <asio.hpp>            // 4. 其他库

#include "common/define.h"     // 5. 本项目
```

## 作用域

- 按模块建命名空间（`everest::net`、`everest::packet`，与目录对应），不要都塞进扁平的 `everest`；禁用 `using namespace` 和 inline namespace；结尾写 `}  // namespace everest::net`，内容不缩进。
- 仅当前 `.cpp` 使用的定义放匿名命名空间或加 `static`；头文件不用内部链接。
- 统一编译单元下所有 `.cpp` 属同一 TU，文件作用域 `static`/匿名命名空间不跨文件隔离；模块内同名 helper 会重定义，用类作用域、按文件命名的 internal 命名空间或唯一名字隔离。
- 非成员函数放模块命名空间。大量相关的静态函数可用不可实例化的工具类归组（构造函数 `= delete`，只含 static 成员，按职责拆分）；零星函数不要建类。类名本身就是一层作用域，冲突比扁平命名空间更少。
- 局部变量作用域尽量小，声明时初始化。
- 静态/全局变量仅允许可平凡析构；优先 `constexpr`/`constinit`；禁止用动态容器、智能指针做全局。
- 命名空间/类作用域的 `thread_local` 用编译期常量初始化。

## 类

- 构造/析构中不调用虚函数；可能失败的初始化用 `Init()` 或工厂。
- 单参数构造函数、转换运算符加 `explicit`。
- 明确拷贝/移动语义：可拷贝→声明拷贝；仅可移动→声明移动；都不可→`= delete`；声明赋值运算符时同时提供对应构造函数。
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
| 命名空间 | 小写下划线，顶层用项目名 | `everest::net` |
| 枚举值 | `k` + 大驼峰（新代码） | `kOK` |
| 宏 | 全大写下划线 | `EVEREST_LOG_INFO` |
| 缩写 | 按单词处理 | `StartRpc`，非 `StartRPC` |

## 注释

- 统一用 `//`；文件头写许可证（如要求）或一句话内容说明。
- 类注释：用途、用法、线程安全前提。
- 函数声明：功能、参数所有权/生命周期、空指针、线程安全；定义处讲实现，不照抄声明，显而易见的可省略。
- 成员变量：特殊值（`nullptr`/`-1`）和生命周期。
- 解释“为什么”，不翻译代码；`// TODO(姓名/issue): 说明`；`// DEPRECATED(姓名): 迁移说明`。
- 完整句子，标点拼写正确。

```cpp
// 找不到返回空串；调用方负责释放。
std::string Find(const std::string& key);

// TODO(wangshizhu): 支持大小写不敏感，见 #123。
```

## 格式

- 80 列；非 ASCII 用 UTF-8，不硬编码 UI 文本。
- 空格缩进 2 格，不用 Tab。
- 函数名后紧跟 `(`；`)` 与 `{` 间一个空格；换行参数缩进 4 格；调用和初始化列表同函数调用。
- 条件/循环：关键字后空格；`{` 与语句同行；`else` 跟 `}` 同行；某分支用花括号则全用。
- `switch` 覆盖所有枚举值，不可达的 `default` 用 `assert(false)`；空循环体用 `{}` 或 `continue`。
- 指针/引用：`char* c` 或 `char *c` 在文件内统一；一元 `*`/`&` 后不加空格；不要 `int x, *y;`。
- 布尔用 `&&`/`||`/`!`；`return` 不加多余括号。
- 预处理指令顶格。
- 访问说明符缩进 1 格，顺序 public→protected→private，除第一个外前空一行。
- 构造初始化列表放不下时缩进 4 格，每行一个成员。
- 不留行尾空格；二元运算符两侧空格；模板尖括号内不加空格。
- 函数间最多空 2 行；函数体首尾不空行。
- 可用 `cpplint`，误报加 `// NOLINT`。

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

- 既有代码：修改时保持原风格，不整文件重排。
- Windows：不用匈牙利命名；调 Windows API 可用 `DWORD`/`HANDLE`；警告级别 ≥3 且视为错误；不用 `#pragma once`；避免非标准扩展。
- 本仓库：`everest/`、`log/` 历史文件为 Tab + GBK，改时沿用；新文件按本规范。
