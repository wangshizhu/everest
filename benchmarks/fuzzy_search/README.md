# fuzzy_search 基准

fuzzy_search 的 500 万条名字基准程序，单进程、单线程，仅支持 Windows（依赖 `windows.h` 与 `psapi.h`）。不纳入构建系统和自动测试，手动编译运行。

## 目录

| 路径 | 内容 |
| --- | --- |
| `fuzzy_search_benchmark.cpp` | 基准程序源码 |
| `data/names_sample_100k.txt` | 运行生成的 10 万条名字样本（历史快照） |
| `results/result_min*.json` | 历史基准结果 |
| `report/*.html` | 由结果 JSON 生成、面向策划的报告（历史快照） |

`data/`、`results/`、`report/` 都是历史快照，不随代码变更更新。

## 构建

先构建对应配置的核心库（`libs/Release/everest.lib` 与 `libs/libprotobuf.lib`），再在 Developer Command Prompt 中执行：

```bat
cl /std:c++20 /O2 /MT /EHsc /utf-8 /Ieverest /I3rd\spd\include ^
   fuzzy_search_benchmark.cpp libs\Release\everest.lib libs\libprotobuf.lib ^
   /Fe:fuzzy_search_benchmark.exe
```

- include 根是 `everest/`，只需要 `everest` 与 `3rd/spd/include` 两个目录。
- 运行库必须与核心库一致：Release 用 `/MT` + `libs/Release/everest.lib` + `libs/libprotobuf.lib`；Debug 用 `/MTd` + `libs/Debug/everest.lib` + `libs/libprotobufd.lib`。
- `psapi.lib` 由源码中的 `#pragma comment(lib, "psapi.lib")` 自动链接。
- 核心库是统一编译单元并包含 `packet_serialize.cpp`，因此链接 `everest.lib` 时必须同时链接 protobuf 静态库。

## 运行

```bat
fuzzy_search_benchmark.exe --min-search=1 --output=results\result_min1.json --names-out=data\names_sample_100k.txt
```

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--min-search=1` / `--min-search=2` | `1` | 最小查询长度；`1` 启用 unigram，`2` 只建 bigram |
| `--output=<path>` | `fuzzy_search_benchmark.json` | 结果 JSON 路径 |
| `--names-out=<path>` | `names_sample_100k.txt` | 名字样本输出路径（10 万条） |

运行会插入 500 万条名字、建索引、执行查询、删除并做两次 `Compact`，耗时分钟级、内存占用可达 GB 量级。结果 JSON 中的绝对路径字段来自生成时的机器，不影响结果解读。

## 报告

`report/*.html` 由结果 JSON 生成，生成脚本 `make_report.py` 未随仓库提供，报告仅作历史参考。
