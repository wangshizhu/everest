# command_line_parser

`everest::CommandLineParser` 是 `everest` 的命令行参数解析器，负责注册选项、解析 `argc/argv`、校验取值并生成 usage 文本。

- 公共头文件：`cmd_line/command_line_parser.h`
- 实现：`cmd_line/src/command_line_parser.cpp`（PImpl，实现不暴露在头文件）
- 测试：`test/cmd_line_test/`

## 基本用法

```cpp
#include <iostream>

#include "cmd_line/command_line_parser.h"

int main(int argc, char** argv) {
    everest::CommandLineParser parser;

    // 必需项
    parser.Add<std::string>({.name = "host", .short_name = 'H', .desc = "server host"});
    // 可选项，带默认值
    parser.Add<int>({.name = "port", .short_name = 'p', .desc = "listen port", .need = false}, 8080);
    // 开关
    parser.AddWithoutValueCommand({.name = "verbose", .short_name = 'v', .desc = "verbose output"});
    // 区间校验
    parser.AddIntRange({.name = "workers", .short_name = 'w', .desc = "worker count", .need = false}, 4, {1, 64});
    // 取值集合校验
    parser.AddStringOneOf({.name = "format", .short_name = 'f', .desc = "output format", .need = false}, "json", {"json", "xml"});

    if (!parser.Parse(argc, argv)) {
        std::cerr << parser.ErrorFull() << parser.Usage();
        return 1;
    }

    const std::string host = parser.Get<std::string>("host");
    const int port = parser.Get<int>("port");
    const bool verbose = parser.IsSet("verbose");
    // ...
}
```

进程级单例场景：

```cpp
#include <iostream>

#include "cmd_line/command_line_parser.h"
#include "singleton/singleton.h"

#define CMD_LINE everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()

int main(int argc, char** argv) {
    CMD_LINE->Add<std::string>({.name = "version", .short_name = 'v', .desc = "show version", .need = false}, "1.0.0");
    CMD_LINE->ParseGetOrSetBehavior(argc, argv);
    std::cout << CMD_LINE->Get<std::string>("version") << std::endl;
}
```

## 线程与所有权

- 非线程安全。先 `Add` 注册、再 `Parse`，须在同一线程完成；跨线程使用时自行加锁。
- `CommandLineParser` 不可拷贝、不可移动；内部状态由 PImpl 持有。
- `OptionDefine` 只在注册调用期内有效：`name`/`desc` 是 `string_view`，解析器会立即拷贝成 `std::string`，不要保存 `OptionDefine` 的副本跨调用使用。

## 注册接口

### OptionDefine

```cpp
struct OptionDefine {
    std::string_view name;       // 长名，不能为空
    char short_name = 0;         // 短名，0 表示无短名
    std::string_view desc;       // usage 描述
    bool need = true;            // 是否必需；开关选项忽略该字段
};
```

### 带值选项

```cpp
template <class T>
void Add(const OptionDefine& define, const T& def = T());
```

- `def` 是未传入时 `Get` 返回的默认值。
- 支持的类型：`std::string`、`bool`、`int`、`int32_t`、`int64_t`、`uint32_t`、`uint64_t`、`size_t`、`float`、`double`。
- 其他类型会在编译期被 `static_assert` 拒绝。

### 开关选项

```cpp
void AddWithoutValueCommand(const OptionDefine& define);
```

开关不消耗参数，`IsSet` 表示是否出现。

### 区间校验

```cpp
template <class T>
struct RangeOption { T low = 0; T high = 0; };

void AddIntRange(const OptionDefine& define, int def, RangeOption<int> range);
void AddInt64Range(const OptionDefine& define, int64_t def, RangeOption<int64_t> range);
void AddUint32Range(const OptionDefine& define, uint32_t def, RangeOption<uint32_t> range);
void AddUint64Range(const OptionDefine& define, uint64_t def, RangeOption<uint64_t> range);
```

取值超出 `[low, high]` 时解析失败，记录 `option value is invalid`，不写入选项值。

### 取值集合校验

```cpp
void AddStringOneOf(const OptionDefine& define, std::string_view def,
                    std::initializer_list<std::string> allowed);
```

`allowed` 只支持花括号列表，内部会立即拷贝。取值不在集合内时记录 `option value is invalid`。

### 注册错误

- 长名为空：向 `stderr` 打印 `option name must not be empty`，忽略本次注册。
- 长名重复：向 `stderr` 打印 `multiple definition: <name>`，忽略本次注册。
- 短名重复：向 `stderr` 打印 `short option 'x' is ambiguous`，忽略本次注册（与长名重复的处理一致）。
- 注册错误不退出进程，也不进入 `Error()` 列表。

### Footer / 程序名

```cpp
void Footer(std::string_view footer);
void SetProgramName(std::string_view name);
const std::string& GetProgramName() const;
```

`Footer` 追加到 usage 首行末尾；`SetProgramName` 覆盖 usage 中显示的程序名。

## 解析接口

```cpp
bool Parse(int argc, const char* const argv[]);
bool Parse(const std::vector<std::string>& args);
bool Parse(const std::string& command_line);
```

| 重载 | 程序名 | 说明 |
|---|---|---|
| `Parse(argc, argv)` | `argv[0]` | 标准进程参数形式 |
| `Parse(vector)` | `args[0]` | 空列表返回 `false` |
| `Parse(string)` | 无 | 切分出的所有 token 都视为参数 |

- `Parse(argc, argv)`/`Parse(vector)` 在程序名为空时从第一个元素推导：按最后一个 `/` 或 `\` 截断目录，再截断第一个 `.` 之后的后缀。
- `Parse(string)` 按空格切分，双引号内保留空格，反斜杠转义下一个字符；引号外连续空白与首尾空白不产生空 token，引号内可得到空串（`--name ""`）；未闭合引号或末尾反斜杠返回 `false` 并记录错误；切不出任何 token 时返回 `false`。
- 每次 `Parse` 都会复位位置参数、错误列表以及所有选项的 `set`/`actual`（`actual` 回到注册默认值 `def`）；`def`、程序名与 footer 不复位。

### 解析规则

- `--name=value`：直接赋值。
- `--name value`：带值选项消费下一个参数；无值选项只置位。
- `-x value`：短选项；末位带值选项消费下一个参数。
- `-abc`：捆绑短选项，逐字符置位；末位若是带值选项则消费下一个参数。
- 单独的 `-` 忽略；单独的 `--` 视为未注册选项（`undefined option: --`）。
- 非选项参数进入 `Rest()`。
- 带值选项接受单个前导 `+`（如 `--count=+5`）；`+` 单独出现、`++5`、`+abc` 仍判非法。
- 短名歧义、未知选项、缺值、值非法、必需项缺失都会记录到 `Error()/ErrorFull()`，`Parse` 返回 `false`。
- `Parse` 不打印、不退出。

## 查询与输出

```cpp
bool Exist(const std::string& name) const;   // 是否已注册
bool IsSet(const std::string& name) const;   // 是否在解析中被显式设置
const std::vector<std::string>& Rest() const;

template <class T> T Get(const std::string& name) const;
template <class T> bool TryGet(const std::string& name, T& out) const;

std::string Usage() const;
std::string Error() const;
std::string ErrorFull() const;
```

- `Exist` 只判断注册；`IsSet` 判断是否显式传入。
- `Get<T>` 失败时记录并打印错误，返回 `T()`，不退出；失败文案区分未注册、开关无值与类型不符。
- `TryGet<T>` 不打印、不退出；失败时 `out` 保持不变。
- `Error()` 返回第一条错误的副本，无错误时为空串；`ErrorFull()` 每条错误一行。
- `Get` 在解析前调用时返回注册时的默认值。

## 退出语义

以下接口保留 CLI 的「打印后退出」行为，其余接口不退出：

```cpp
void ParseCheck(int argc, char** argv);
void ParseCheck(const std::vector<std::string>& args);
void ParseCheck(const std::string& command_line);

void Print(const std::string& name) const;
void Print(char short_name) const;

void ParseGetOrSetBehavior(int argc, char** argv);
```

- `ParseCheck`：若未注册 `help` 会先注册 `help/"?`；解析失败时向 `stderr` 输出 `Error()` + `Usage()`，然后 `exit(EXIT_FAILURE)`；解析成功且 `help` 被设置时向 `stdout` 打印 `Usage()` 并 `exit(EXIT_SUCCESS)`。
- `Print(name)`：找到选项时打印值（开关打印描述）与 usage 后 `exit(EXIT_SUCCESS)`；未找到时打印错误与 usage 后 `exit(EXIT_FAILURE)`。
- `ParseGetOrSetBehavior`：`argc <= 1` 直接返回；`argc > 2` 走解析；`argc == 2` 时 `--name` 打印、`--name=value` 解析、`-x` 打印、普通位置参数解析。

## 错误信息

| 错误串 | 场景 |
|---|---|
| `argument number must be longer than 0` | `argc < 1`、空参数列表或空命令行 |
| `argument must not be empty` | 参数数组中出现空指针 |
| `argument number is too large` | `Parse(vector)` 元素数量超出 `int` 范围 |
| `undefined option: --name` | 长选项未注册 |
| `undefined short option: -x` | 短选项未注册 |
| `option needs value: --name` | 带值选项缺少参数 |
| `option value is invalid: --name=value` | 数值解析失败、越界或不在集合内 |
| `option does not take value: --name=value` | 对开关使用 `--name=value` 形式 |
| `need option: --name` | 必需项未设置 |
| `short option 'x' is ambiguous` | 注册期多个选项使用同一短名 |
| `quote is not closed` | `Parse(string)` 引号未闭合 |
| `unexpected occurrence of '\' at end of string` | `Parse(string)` 末尾反斜杠 |
| `flag 'name' has no value` | 对开关调用 `Get`/`TryGet` |
| `type mismatch flag 'name'` | `Get`/`TryGet` 类型与注册类型不符 |
| `there is no flag: --name` | `Get` 读取未注册选项，或 `Print` 未找到选项 |

## 实现约束

- 头文件只含声明与极薄的模板分派；选项存储、解析、校验、usage 全部在 `src/command_line_parser.cpp` 的 `CommandLineParser::Impl`。
- 不使用异常与 RTTI；数值解析使用 `std::from_chars`（解析前剥离一个前导 `+`），类型擦除使用 `std::variant` + 私有 `ValueKind`。
- 与 `everest` 其他组件一样参与统一编译单元；新增 `.cpp` 时必须加入 `everest/common/src/include.cpp`。
- 使用方链接 `everest` 静态库时需要同时链接 protobuf（统一编译单元包含 `packet_serialize.cpp`）。

## 构建与测试

Windows：

```powershell
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\cmd_line_test\cmd_line_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
test\cmd_line_test\x64\Debug\cmd_line_test.exe
```

Linux（WSL Ubuntu-24.04）：

```powershell
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/cmd_line_test && bash build.sh'
wsl -d Ubuntu-24.04 -e bash -lc 'cd /mnt/f/my_src/everest/test/cmd_line_test && ./bin/cmd_line_test'
```
