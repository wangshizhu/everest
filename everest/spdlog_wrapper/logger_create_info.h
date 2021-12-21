#ifndef LOGGER_CREATE_INFO_H_
#define LOGGER_CREATE_INFO_H_

#include "spdlog/common.h"

NAMESPACE_BEGIN

struct LoggerCreateInfo
{
	// 是否异步logger,默认true
	bool async_logger = true;

	// 是否控制台输出,默认true
	bool to_stdout = true;

	// 是否每日产生log文件,默认true
	bool daily = true;

	// 是否设置log文件大小,默认true
	bool rotate = true;

	// log文件大小，当rotate为true时才有效,默认10M
	std::size_t log_file_size = 1024 * 1024 * 10;

	// 消费线程数量,默认1个消费线程
	std::size_t consumer_thread_num = 1;

	// 当使用异步log时，异步log队列大小,默认8192
	std::size_t async_queue_size = spdlog::details::default_async_q_size;

	// log级别
	spdlog::level::level_enum level = spdlog::level::debug;
};

struct CreateInfoFactory
{
	static LoggerCreateInfo info_;
	CreateInfoFactory(const LoggerCreateInfo& info)
	{
		info_ = info;
	}

	static const LoggerCreateInfo& GetLoggerCreateInfo()
	{
		return info_;
	}
};

void SetLoggerCreateInfo(const LoggerCreateInfo& info);

NAMESPACE_END

#endif
