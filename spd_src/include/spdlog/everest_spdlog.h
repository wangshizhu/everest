#ifndef EVEREST_SPDLOG_H_
#define EVEREST_SPDLOG_H_

#include <utility>

#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/daily_rotating_file_sink.h"

#define PROGRAM_NAME "test"

template <typename T, std::size_t ...N>
constexpr decltype(auto) MakeSequenceByType(T&&, std::index_sequence<N...>)
{
	using t = std::integer_sequence<char, T::get()[N]...>;
	return std::integer_sequence<char, T::get()[N]...>();
}

template <typename T>
constexpr decltype(auto) MakeIndexSequenceImpl(T&& t)
{
	return MakeSequenceByType(std::move(t), std::make_index_sequence<sizeof(T::get()) - 1>());
}

#define STRING_LITERAL_TO_SEQUENCE(s) \
    (MakeIndexSequenceImpl([] { \
        struct tmp { static constexpr decltype(auto) get() { return s; } }; \
        return tmp{}; \
    }()))\

template<class T>
struct AsyncLoggerFactory
{
	AsyncLoggerFactory(const std::string& logger_name, bool is_stdout,bool is_daily,bool is_rotate)
	{
		std::vector<spdlog::sink_ptr> sinks;

		// 是否输出到控制台
		if (is_stdout)
		{
			sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		}

		if (is_daily && is_rotate)
		{
			auto daily_sink = std::make_shared<spdlog::sinks::daily_rotating_file_sink_mt>(logger_name, 1024 * 1024 * 10, 0, 0, false);
			sinks.push_back(daily_sink);
		}
		else
		{
			// 每天固定时间产生新log文件
			if (is_daily)
			{
				auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logger_name, 0, 0, false);
				sinks.push_back(daily_sink);
			}

			// 按照文件大小产生新log文件
			if (is_rotate)
			{
				auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logger_name, 1024 * 1024 * 10, 3);
				sinks.push_back(rotating_sink);
			}
		}

		spdlog::init_thread_pool(spdlog::details::default_async_q_size, 1);
		auto& registry_inst = spdlog::details::registry::instance();

		auto& mutex = registry_inst.tp_mutex();
		std::lock_guard<std::recursive_mutex> tp_lock(mutex);

		auto tp = registry_inst.get_tp();
		if (tp == nullptr)
		{
			tp = std::make_shared<spdlog::details::thread_pool>(spdlog::details::default_async_q_size, 1);
			registry_inst.set_tp(tp);
		}

		auto logger = std::make_shared<spdlog::async_logger>(logger_name, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

		spdlog::register_logger(logger);
	}
};

template<class T>
void CreateAsyncLogger(const std::string& logger_name, bool is_stdout, bool is_daily, bool is_rotate)
{
	static AsyncLoggerFactory<T> logger(logger_name, is_stdout, is_daily,is_rotate);
}

#define CREATE_ASYNCLOGGER(LOGGER_NAME,IS_STDOUT,IS_DAILY,IS_ROTATE) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
CreateAsyncLogger<decltype(result)>(std::string(LOGGER_NAME),IS_STDOUT,IS_DAILY,IS_ROTATE);\
}\

#define VARIABLE_PREFIX result
#define CONCATENATE_DIRECT(s1, s2) s1_##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
# define ANONYMOUS_VARIABLE CONCATENATE(VARIABLE_PREFIX, __LINE__)

#define NAMED_LOG_INFO(name,...) \
auto&& logger = spdlog::get(name);if(logger != nullptr) logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)\

#define NAMED_LOG_DEBUG(name,...) \
auto&& logger = spdlog::get(name);if(logger != nullptr) logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)\

#define NAMED_LOG_ERROR(name,...) \
auto&& logger = spdlog::get(name);if(logger != nullptr) logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)\

#define NAMED_LOG_WARNING(name,...) \
auto&& logger = spdlog::get(name);if(logger != nullptr) logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)\

#define NAMED_LOG_CRITICAL(name,...) \
auto&& logger = spdlog::get(name);if(logger != nullptr) logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)\

//
//#define LOG_INFO \
//NAMED_LOG_INFO(PROGRAM_NAME)
//
//#define LOG_DEBUG \
//NAMED_LOG_DEBUG(PROGRAM_NAME)
//
//#define LOG_ERROR \
//NAMED_LOG_ERROR(PROGRAM_NAME)
//
//#define LOG_WARNING \
//NAMED_LOG_WARNING(PROGRAM_NAME)
//
//#define LOG_FATAL \
//NAMED_LOG_FATAL(PROGRAM_NAME)


#endif