# CLAUDE.md

本文件为 Claude Code（claude.ai/code）在本仓库中工作时提供指引。

## 仓库概览

`everest` 是一个基于 asio 的 C++17 游戏服务器基础设施框架，包括网络框架、buffer、线程封装、线程池等，同时内置了第三方代码，并包含一个独立的 C++20 搜索库。

- 公共头文件根目录是 `everest/`；包含时直接写相对该目录的路径（`"net/session_base.h"`、`"common/include.h"`），所有构建都已把 `everest/` 加入 include 目录。
- `everest/common/include.h` 是总头文件，按依赖顺序包含几乎所有组件。
- `everest/` 下的代码通过 `NAMESPACE_EVEREST_BEGIN` / `NAMESPACE_EVEREST_END`（`everest/common/define.h`）使用 `namespace everest`。
- 例外：`everest/fuzzy_search/` 是 `namespace metaverse` 下独立的 C++20 header-only 库，不依赖 `everest` 其余部分。
- `windows_sln/` 下的 Visual Studio 解决方案是当前维护的、主要的构建方式；CMake 文件较为单薄，属于次要路径。
- 源码和注释以中文为主。搜索库的文档在 `everest/fuzzy_search/README.md` 和 `DESIGN.md`。
- 仓库根目录的 `README.md` 是空文件。

## 构建

### Windows（主要方式）

用 Visual Studio 打开 `windows_sln/erverest.sln`（注意文件名拼写是 `erverest`），或在 Developer Command Prompt 中执行：

```bat
msbuild windows_sln\erverest.sln /p:Configuration=Debug /p:Platform=x64
```

- 工具集 v142，C++17，静态库，配置为 Debug/Release × Win32/x64。
- 输出到 `libs/`（`everest.lib`），中间文件到 `tmp/`。
- 解决方案中包含第三方项目：`fmt`、`spd`（spdlog）、`date`、`asio`、`yaml`、`redis-plus-plus`、`protobuf-3.18.0` 以及 `log/`（基于 g3log 改造的日志库）。
- include 目录：`everest`、`3rd/asio-1.16.1/include`、`3rd/spd/include`、`3rd/fmt/include`、`3rd/date/include`、`3rd/random/include`（Debug|x64 额外包含 `3rd/protobuf-3.18.0/src`）。
- Win32 配置定义了 `SPDLOG_COMPILED_LIB`；Debug|x64 使用 `/MTd` 运行库（`MultiThreadedDebug`）。

### Linux

```bash
cd linux_build
bash build.sh          # rm -rf build; cmake ..; make
```

产物：`libs/libeverest.a`。根 `CMakeLists.txt` 要求 CMake 3.18+、C++17、`-g -pthread`，默认构建 fmt。可通过 `-DBUILD_YAML=ON`、`-DBUILD_SPDLOG=ON`、`-DBUILD_PROTOBUF=ON`、`-DBUILD_REDISPLUS=ON` 启用内置的可选依赖。

CMake 路径已知的不足：
- 根 CMakeLists 没有把 `3rd/protobuf-3.18.0/src` 加入 include 路径，但统一编译单元会编译 `packet_serialize.cpp`，它包含 protobuf 头文件。如果构建报找不到 `google/protobuf/...`，需要补上该 include 目录，或保证系统已安装 protobuf。
- 测试的 CMake 工程链接 `libs/` 下预编译的静态库（如 `libeverest.a`、`libfmt.a`），所以必须先构建核心库，并把所需的第三方静态库放进 `libs/`。

## 测试

仓库没有测试框架，也没有 ctest。`test/` 下的每个目录都是一个独立可执行程序，各自带有 `CMakeLists.txt`、`build.sh` 和 Visual Studio `.sln`/`.vcxproj`。

```bash
cd test/net_test
bash build.sh                       # cmake ..; make
./bin/net_test                      # 每个测试目录对应一个可执行文件
```

运行单个测试就是直接运行对应目录的可执行文件。Windows 上打开该测试自己的 `.sln`（例如 `test/net_test/net_test.sln`）。

- `test/test_protobuf` 通过 `test/test_protobuf/gen_proto_message.bat` 重新生成 `.pb.cc/.pb.h`：它把 `all_proto.txt` 交给 `tool/protoc.exe`（`--cpp_out=.\message\`）。
- `test/net_test` 是串联 launcher、service、session 和 session creator 的最佳参考。

### fuzzy_search 测试/基准

`everest/fuzzy_search/` 没有接入任何 CMake 或 Visual Studio 工程，其 README 中提到的 `cmake --preset x64-debug` 在本仓库并不存在。它是 header-only 的，要求 **C++20**（使用 `std::span`），必须单独编译。

它的测试/示例包含 `"system/fuzzy_search/fuzzy_search_system.h"`（从其他项目拷贝而来），而本仓库的头文件位置是 `everest/fuzzy_search/fuzzy_search_system.h`。此外，未关闭日志的路径会包含 `common/logger/logger.h`，本仓库没有这个文件，因此测试必须定义 `FUZZY_SEARCH_DISABLE_GAMESERVER_LOG`。

可行做法：把 include 改为 `fuzzy_search/fuzzy_search_system.h`（或创建一个指向 `everest` 的 `system` 链接），然后在仓库根目录执行：

```bash
g++ -std=c++20 -DFUZZY_SEARCH_DISABLE_GAMESERVER_LOG -Ieverest \
    everest/fuzzy_search/tests/fuzzy_search_system_test.cpp -o fuzzy_tests
./fuzzy_tests
```

基准程序（`benchmarks/fuzzy_search_benchmark.cpp`）仅支持 Windows（依赖 `windows.h`、`psapi.h`）。

## 架构

### 统一编译单元（Unity Build）——最重要的约定

真正参与编译的只有一个翻译单元：`everest/common/src/include.cpp`。它先包含总头文件，再按依赖顺序 `#include` 其他所有 `.cpp`（`helper.cpp`、`buffer.cpp`、`command_line_parser.cpp`、clock/timer、thread、日志、monitor、configuration、net、packet、service、launcher）。

- VS 工程中，其他所有 `ClCompile` 条目在所有配置下都是 `ExcludedFromBuild`。
- CMake 中，`AUX_SOURCE_DIRECTORY` 只收集到 `everest/common/src/*.cpp`。
- 因此各组件 `.cpp` 故意不包含自己的头文件，依赖 `common/include.h` 先被包含。
- **新增 `.cpp` 文件时，必须在 `everest/common/src/include.cpp` 的正确依赖位置加入 `#include "module/src/file.cpp"`。** 只加进 VS 工程或 CMake 不会真正编译它（或会造成重复符号）。新头文件如果要进入公共总头文件，通常也要加到 `common/include.h`。

### 启动与服务生命周期

应用继承 `everest::LauncherBase`（`everest/launcher/launcher_base.h`）并调用 `StartLauncher()`，执行顺序为：

1. `OnLoadServiceConfig()`——子类通过 `CONFIGURATION_SINGLETON()->AddServiceConfig(...)` / `AddThreadAllocateConfig(...)` 把 `ServiceConfig`、`ThreadAllocateConfiguration` 注册到 `ConfigurationManager` 单例。
2. `CheckServiceConfig()` 校验。
3. `OnStartLauncherBefore()`。
4. 创建并启动 monitor 线程。
5. 按每个网络线程分配策略创建 `ServiceNetThreadManager`。
6. `OnStartLauncher()`——子类在此调用 `CreateOneService(service_id, session_creator)`。

`CreateOneService` 创建 `ServiceBase`；当 `ListenConnectType::kListen` 时再创建绑定到该策略 listener/connector 线程的 `Listener`。`SessionCreator::CreateSession()` 是接受连接时的 session 工厂。`Listener::HandleAccept` 分配 `SessionId`、挂接 socket，并把 session 注册到 `ServiceMonitor`；随后 `ServiceBase::RegisterSession` 调用 `Session::Read()` 开始异步 I/O。`kConnect`（主动连接）目前仍是 TODO。

`test/net_test/main.cpp` 是一个完整的最小示例。

### 线程 / io_context 模型

- `everest::ThreadBase` 封装一个 `asio::io_context`，并启动一个执行 `context.run()` 的 `std::thread`。`Start()` 还会启动一个重复触发的 `steady_timer`，按间隔调用虚函数 `Update()`（默认 100ms；`define.h` 中的 `kThreadUpdateInterval` 单位是微秒）。`Post`/`Dispatch` 把任务投递到该线程。
- `ThreadPool<T>` 创建 N 个线程，并按 key/index 分发任务（`PostAll`、`PostAny`、`PostByKey`、`PostByIndex`）。
- `ServiceNetThreadManager` 实现 `ThreadAllocateConfiguration` 的两种模式：
  - `kReactorSingleThreadMode`：一个 `ThreadBase` 同时负责 accept 和所有 session。
  - `kReactorMultiThreadMode`：`ListenerConnectorThread` 持有 acceptor，接受的连接交给 `SessionThreadPool`；之后 session I/O 在分配到的 session 线程 `io_context` 上运行。
- `ControlMonitor`（全局 `g_control_monitor`）跟踪线程和 service。`ThreadBase::CreateThread<T>(...)`、`SessionBase::CreateSession<T>(...)` 等工厂会自动注册对象，并在构造参数末尾追加私有的 `PrivateFlag`，因此这些类采用“私有构造 + 工厂”模式。

### 网络与数据包

- asio 1.16.1 位于 `3rd/asio-1.16.1/include`；`everest_tcp` 是 `asio::ip::tcp` 的别名。
- `SessionBase` 继承 `enable_shared_from_this`，读写使用 `ExpandBuffer`（默认 512 字节，上限 2MB）。子类实现 `OnRead()`、`OnClose()` 和 `GetSessionType()`。
- `Packet<BodyType, packet_id>` 提供带类型的包（`everest/packet/packet.h`），通过 `PACKET_TYPE_DEFINE(NAME, ID, BODY)` 定义别名。
- `PACKET_HANDLE(PACKET_TYPE, SESSION_TYPE, packet_var, session_var)` 特化 `HandlePacket` 并把处理器注册到 `PacketHandlerManager` 单例；`PacketHandlerRegister`/`PACKET_HANDLER_REGISTER` 只负责注册。
- protobuf 消息通过 `BufferBase` 上的 `operator<<` / `operator>>` 序列化（`everest/packet/packet_serialize.h`）。
- `SessionId` 由 service id 和每个 listener 的递增序号组合而成。

### 日志

`EVEREST_LOG_INFO/DEBUG/ERROR`（`everest/common/define.h`）展开为 `everest/spdlog_wrapper/everest_spdlog.h` 中的 `NAMED_LOG_*`，首次使用时会通过 `CREATE_LOGGER_IF_NULL` 懒创建具名 spdlog logger。logger 的配置项在 `LoggerCreateInfo`。另外，`log/` 是基于 g3log 改造的日志库，作为独立 VS 工程构建；`everest/` 的核心头文件使用的是 spdlog 封装，而不是 `log/`。

### fuzzy_search（独立模块）

一个自包含的内存 UTF-8 子串索引，位于 `namespace metaverse`，通过 `FuzzySearchOptions { maxStringLength, minSearchLength, caseInsensitive }` 配置，只能通过 `FuzzySearchSystem::Create` 创建；单线程、无锁，并有进程级单实例守卫。

存储模型：
- `documents_`（全部文档，`alive` 为墓碑标记）、`idToDoc_`（外部 id → 内部 `DocId`）、`nameArena_`（仅折叠 ASCII 大小写的 UTF-8 名字）。
- `baseIndex_`：不可变的紧凑 gram 表 + delta/varint 编码的 posting，由 `Compact()` 重建。
- `deltaIndex_`：可变的每 gram `std::vector<DocId>`；删除只打墓碑。
- 不变量：base 中的 DocId 一定小于 delta 中的 DocId，因此 base posting 与 delta posting 直接拼接即为按文档号升序的完整列表。
- 1–2 个码点的查询只扫描一个 posting 列表；更长查询对 Q−1 个 bigram 列表求交，然后用 arena 中的原名做连续性校验（排除 bigram 同时存在但不相邻的假阳性）。

## 约定与注意事项

- **编码：** 现有 `everest/**` 和 `log/**` 文件几乎都是 GBK/ANSI（不是 UTF-8），因此 Read 显示中文注释为乱码，Edit/Write 会拒绝写入。`everest/fuzzy_search/**` 是 UTF-8，可正常编辑。对于 GBK 文件，要么用 PowerShell 的 `-Encoding Default` 编辑，要么先明确提出 UTF-8（带 BOM）转换再修改，不要悄悄把文件改写成 UTF-8。
- **新增源文件：** 见统一编译单元一节——要更新 `everest/common/src/include.cpp`，而不只是工程文件。
- **`g_control_monitor` 生命周期：** 它在 `LauncherBase` 构造函数中初始化。在 `LauncherBase` 存在之前调用 `ThreadBase::CreateThread`（或任何会向 monitor 注册的工厂）会解引用空指针。另外，`LauncherBase` 的析构函数假定 monitor 线程已经创建。
- **C++ 标准：** 核心是 C++17。不要把 `fuzzy_search` 的头文件纳入 C++17 的统一编译单元，它需要 C++20。
- **私有构造 + 工厂模式：** thread、session、service、listener 都通过静态 `CreateThread`/`CreateSession` 辅助函数构造，这些函数会在参数末尾追加私有 `PrivateFlag`，所以构造函数最后一个参数是该 flag。
- **缩进风格不统一：** 核心文件使用 Tab，`fuzzy_search` 使用 4 个空格。编辑时与所在文件保持一致。
- **第三方代码** 全部内置在 `3rd/`；优先修改 `everest/` 中的封装代码，而不是直接改内置库。
