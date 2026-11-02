#pragma once

// 这是 fuzzy_search 库中唯一依赖具体日志实现的文件。
// 迁移到新项目时，只需替换本文件为目标项目的日志适配，或定义 FUZZY_SEARCH_DISABLE_GAMESERVER_LOG 关闭日志。
// FuzzySearchSystem 在类内放置 FUZZY_SEARCH_LOG_CHANNEL(FuzzySearch)，让 LOG_* 能通过非限定查找找到 LoggerProxy()。

#ifdef FUZZY_SEARCH_DISABLE_GAMESERVER_LOG

#define FUZZY_SEARCH_LOG_CHANNEL(Name)
#define FUZZY_SEARCH_LOG_DEBUG(...) ((void)0)
#define FUZZY_SEARCH_LOG_INFO(...) ((void)0)
#define FUZZY_SEARCH_LOG_WARN(...) ((void)0)
#define FUZZY_SEARCH_LOG_ERROR(...) ((void)0)

#else

#include "common/logger/logger.h"

#define FUZZY_SEARCH_LOG_CHANNEL(Name) CREATE_LOG_CHANNEL(Name)
#define FUZZY_SEARCH_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define FUZZY_SEARCH_LOG_INFO(...) LOG_INFO(__VA_ARGS__)
#define FUZZY_SEARCH_LOG_WARN(...) LOG_WARN(__VA_ARGS__)
#define FUZZY_SEARCH_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

#endif
