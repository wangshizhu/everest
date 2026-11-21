---
name: code-impl
description: 按已确认的代码计划执行实现：严格遵循 doc/standard 规范，完成代码、GoogleTest 测试、Windows/Linux 构建与运行、机械和人工合规检查，并输出可核对的报告。
---

# code-impl

## 输入

`$ARGUMENTS` — 计划文档路径，必须是具体的 `.md` 文件。

- 只给文件名时，在 `doc/code-plan/` 下递归查找。
- 传相对/绝对路径时按路径解析。
- 为空、不是 `.md`、或找不到文件时停止，并提示先运行计划 skill。

## 前置条件

- 计划已由用户确认；新会话执行本命令即视为确认。
- 计划是本次实现的唯一契约，不重新做 review 或需求分析。
- 计划中存在未决项、或与代码事实冲突时，**停止并询问用户**，不自行发挥。
- 登记、构建、测试流程对所有计划一致，不区分“重写”或“新建”。

## 必读规范

执行前完整读取以下文件；缺失时停止，不凭记忆：

- `doc/standard/code_standard.md`
- `doc/standard/cpp_style_guide.md`
- `doc/standard/review_standard.md`
- `doc/standard/refactor_standard.md`
- 仓库根 `CLAUDE.md`

## 硬约束

- 不修改 `3rd/` 下的源代码；`3rd/` 构建文件变更需单独说明。
- 不提交 commit，除非用户明确要求。
- 不扩大改动面；只改计划涉及的文件和函数。
- 不伪造构建或测试结果。
- 不修改计划正文；发现偏离只报告。
- 不更新 `doc/standard`，除非计划明确列出且用户确认。
- 新 `.cpp` 必须加入 `everest/common/src/include.cpp`，不能只加到 VS 工程或 CMake。
- 不跳过失败的构建或测试。
- 遇到未决项停止，不自行发挥。

## 步骤

### 1. 读取计划与规范

- 读计划正文、计划元信息、计划引用的文件。
- 完整读四份规范。
- 核对计划中的 `file:line` 锚点是否仍成立；锚点失效时以当前代码为准，并在报告中记录。
- 计划与代码事实冲突、或存在未决项时，停止并询问用户。

### 2. 按计划执行

- 严格按计划和规范实现，不参考被改文件的旧风格。
- 只改计划涉及的文件和函数，不顺手重构、不扩大改动面。
- 计划与规范冲突时以规范为准，并在报告中说明偏离。
- 逐项落实计划中的新增、修改、删除、重命名。
- 新建和重写的文件统一使用 UTF-8 无 BOM、LF、4 空格、`<文件>_H_`、Doxygen 注释。
- 新 `.cpp` 加入 `everest/common/src/include.cpp`；公共头文件按需加入 `everest/common/include.h`。
- 更新 `windows_sln` 工程、`CMakeLists.txt`、`.sln`。
- 处理所有被删除或重命名符号的引用。
- 只有计划明确列出时才更新文档。

### 3. 编写测试

- 框架：GoogleTest v1.14.0，源码在 `3rd/googletest`。
- 目录：`test/<component>_test/`。
- 至少包含：
  - `<component>_test.cpp`
  - `CMakeLists.txt`
  - `build.sh`
  - Windows `.vcxproj` 和 `.sln`
- Windows 链接：
  - include：`3rd/googletest/googletest/include`
  - Debug：`libs/gtestd.lib` + `libs/gtest_maind.lib`，`/MTd`
  - Release：`libs/gtest.lib` + `libs/gtest_main.lib`，`/MT`
- Linux/CMake：
  - `SET(CMAKE_CXX_STANDARD 20)`；MSVC 加 `/utf-8`。
  - 通过 `add_subdirectory(${PROJECT_SOURCE_DIR}/../../3rd/googletest ${PROJECT_BINARY_DIR}/googletest)` 构建 gtest，链接 `gtest_main`。
  - 核心库链接 `${PROJECT_SOURCE_DIR}/../../libs/${CMAKE_BUILD_TYPE}/libeverest.a`（Linux，`CMAKE_BUILD_TYPE` 缺省为 `Debug`）或 `../../libs/$(Configuration)/everest.lib`（Windows；`AdditionalLibraryDirectories` 需同时包含 `../../libs/$(Configuration)` 与 `../../libs`，后者用于 gtest/protobuf 预编译库）。
- 按计划的用例实现，使用 `TEST(ComponentName, Behavior)` 和 AAA 结构。
- 异步/网络测试使用 loopback、临时端口、超时；禁止裸 `sleep`；用例间清理资源。
- 长驻服务器测试必须能自行退出；不能退出的在计划中标记为手动验证，`code-impl` 只构建不运行，并在报告中说明。

### 4. 双平台构建与测试

Windows：

```powershell
msbuild windows_sln\erverest.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild test\<component>_test\<component>_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

Release 使用同样的命令，把 `Debug` 换成 `Release`。

- 必须先构建对应配置的 `erverest.vcxproj`，再构建同配置测试。
- 原因：测试链接核心库产物；自建库已按配置输出到 `libs/$(Configuration)/`，Debug/Release 可共存，但仍需先产出核心库。
- 构建完成后运行可自动退出的 GoogleTest 用例，记录通过/失败。

Linux（在 **WSL 中的 Linux 发行版**内执行；Windows 自带的 `bash` 是 Git Bash，没有 g++/make）：

```powershell
wsl -l -v                      # 确认发行版名；默认发行版未必具备工具链
wsl -d <发行版名> -e bash -lc 'g++ --version && make --version && cmake --version'
wsl -d <发行版名> -e bash -lc 'cd /mnt/f/my_src/everest/linux_build && bash build.sh'
wsl -d <发行版名> -e bash -lc 'cd /mnt/f/my_src/everest/test/<component>_test && bash build.sh'
wsl -d <发行版名> -e bash -lc 'cd /mnt/f/my_src/everest/test/<component>_test && ./bin/<component>_test'
```

- `/mnt/f/...` 是仓库所在磁盘的 WSL 挂载路径，盘符不同时相应替换。
- 所有 `.sh` 必须是 LF；CRLF 的 `build.sh` 会让 WSL 报 `cd: $'../\r': No such file or directory`。新建 `build.sh` 后先确认行尾。
- 按计划元信息的“目标平台”决定是否执行 Linux 构建；字段缺失时默认 Windows + Linux。
- **每次执行 Linux 构建前必须先跑上面的环境探测。** 只有探测失败（WSL 未安装、发行版不存在、工具链缺失）时才可报告“未验证环境”，并附探测输出；不得用“未验证”代替实际构建。
- 构建或测试失败先修代码，修完重跑；不要用“环境问题”掩盖代码问题。

### 5. 机械校验

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .claude/commands/code-impl.ps1 `
  -Files <改动文件...> -UseClangFormat
```

- 脚本内置阈值：行宽 180、参数 3、嵌套 3、函数 100 行；`code-impl` 不传阈值，由脚本维护。
- 脚本覆盖：UTF-8/BOM/CRLF/Tab、行宽、行尾空格、`#pragma once`、保护符、文件名、`using namespace`、`NULL`、`if(`、`()const`、行尾注释、`<format>`、modules、`std::auto_ptr`、参数/嵌套/函数行数、`clang-format`。
- 有不符合项先修，修完重跑；脚本输出原样保留在报告中。
- 脚本覆盖不了的条目，按规范原文逐条人工核对，不得跳过。

### 6. 人工逐条核对

- 重新打开四份规范，按条目对照本次改动。
- 只报不符合项，附 `file:line`。
- 无法从代码判断的条目单列“需用户确认”。

### 7. 文档更新

- 只有计划明确列出时才更新文档。
- 允许更新：`CLAUDE.md`、`README`、组件文档。
- `doc/standard` 默认不更新；只有计划明确列出且用户确认时才改。
- 所有文档更新项必须在报告中列出。
- 不修改计划正文。

### 8. 结尾报告

只输出：

```markdown
## 改动文件
## 不符合项
## 偏离计划的项
## 需用户确认项
## 构建与测试结果
## 未验证平台
## 文档更新项
```

要求：

- 构建和测试结果必须真实；失败就写失败和原因。
- 偏离计划的项必须说明原因。
- 未验证的平台明确写“未验证”，并附上对应的环境探测输出。
- 不写与实现无关的结构化报告。

---

## 验收清单（Definition of Done）

- [ ] 计划中的文件新增/修改/删除/重命名已全部落实
- [ ] 新 `.cpp` 已加入 `include.cpp`，公共头文件已按需加入 `include.h`
- [ ] VS 工程、CMake、`.sln` 已更新
- [ ] GoogleTest 测试程序、CMake、`build.sh`、Windows `.vcxproj`/`.sln` 齐全
- [ ] Windows Debug/Release x64 构建通过
- [ ] 可自动退出的测试已运行并记录结果
- [ ] Linux 构建/测试通过；未通过时已附 WSL 环境探测失败的输出
- [ ] 机械校验无不符合项
- [ ] 人工规范核对已完成
- [ ] 交付报告完整
- [ ] 未修改 `3rd/` 源代码
- [ ] 未修改计划正文
- [ ] 未提交 commit（除非用户要求）
