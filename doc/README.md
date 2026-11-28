# everest 文档

| 类别 | 位置 | 说明 |
| --- | --- | --- |
| 组件文档 | [components/README.md](components/README.md) | 按组件组织的使用与设计文档 |
| 编码规范 | [standard/](standard/) | 代码风格、评审与重构规范 |
| 优化路线图 | [optimization/](optimization/) | 组件的性能优化计划 |
| 基准 | [../benchmarks/](../benchmarks/) | 基准程序、历史结果与报告 |

## 组件文档

- [calculator](components/calculator/README.md)：饱和整型运算
- [circular_queue](components/circular_queue/README.md)：固定容量环形队列
- [cmd_line](components/cmd_line/README.md)：命令行参数解析
- [fuzzy_search](components/fuzzy_search/README.md)：UTF-8 子串索引，设计见 [DESIGN.md](components/fuzzy_search/DESIGN.md)
- [multi_producer_multi_consumer](components/multi_producer_multi_consumer/README.md)：多生产者多消费者队列（默认/无锁两种模式）

## 约定

- 组件文档放在 `doc/components/<组件>/`，目录名与 `everest/<组件>` 一致，`README.md` 为入口。
- 示例代码跟随对应测试工程（如 `test/fuzzy_search_test/fuzzy_search_example.cpp`），基准放在仓库顶层 `benchmarks/<组件>/`。
- 文档与代码在同一次提交中更新；新增组件文档时在本页和 `components/README.md` 登记。
- `code-plan/` 是本地计划文档，由 `.gitignore` 忽略，不入库。
