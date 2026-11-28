# 组件文档

每个组件一个目录，目录名与 `everest/<组件>` 一致，`README.md` 为入口。

| 组件 | 文档 | 内容 |
| --- | --- | --- |
| calculator | [README.md](calculator/README.md) | 饱和整型运算 |
| circular_queue | [README.md](circular_queue/README.md) | 固定容量环形队列 |
| cmd_line | [README.md](cmd_line/README.md) | 命令行参数解析 |
| fuzzy_search | [README.md](fuzzy_search/README.md)、[DESIGN.md](fuzzy_search/DESIGN.md) | UTF-8 子串索引 |
| multi_producer_multi_consumer | [README.md](multi_producer_multi_consumer/README.md) | 多生产者多消费者队列（默认/无锁两种模式） |

## 约定

- `README.md`：用法、API、构建与测试入口。
- `DESIGN.md`：可选，记录实现设计、数据布局与不变量。
- 示例与基准不放在本目录：示例跟随测试工程（如 `test/fuzzy_search_test/fuzzy_search_example.cpp`），基准放在仓库顶层 `benchmarks/<组件>/`。
- 文档与代码在同一次提交中更新；新增组件文档时在上表登记。
