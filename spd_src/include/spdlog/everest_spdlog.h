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

namespace everest
{
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

	template<class T>
	struct AsyncLoggerFactory
	{
		AsyncLoggerFactory(const std::string& logger_name, spdlog::level::level_enum level,
			bool is_stdout, bool is_daily, bool is_rotate)
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
			logger->set_level(level);

			spdlog::register_logger(logger);
		}
	};

	template<class T>
	struct CloneLoggerFactory
	{
		CloneLoggerFactory(const std::string& logger_name)
		{
			auto&& default_logger = spdlog::default_logger();
			spdlog::register_logger(default_logger->clone(logger_name));
		}
	};

	template<class T>
	void CreateAsyncLogger(const std::string& logger_name, spdlog::level::level_enum level,
		bool is_stdout, bool is_daily, bool is_rotate)
	{
		static AsyncLoggerFactory<T> logger(logger_name, level, is_stdout, is_daily, is_rotate);
	}

	template<class T>
	void CloneLogger(const std::string& logger_name)
	{
		static everest::CloneLoggerFactory<T> clone_logger(logger_name);
	}
}


#define STRING_LITERAL_TO_SEQUENCE(s) \
    (everest::MakeIndexSequenceImpl([] { \
        struct tmp { static constexpr decltype(auto) get() { return s; } }; \
        return tmp{}; \
    }()))\

#define CREATE_ASYNC_LOGGER(LOGGER_NAME,LEVEL,IS_STDOUT,IS_DAILY,IS_ROTATE) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
everest::CreateAsyncLogger<decltype(result)>(std::string(LOGGER_NAME),LEVEL,IS_STDOUT,IS_DAILY,IS_ROTATE);\
}\

#define CLONE_LOGGER_FROM_DEFAULT_IF_NULL(LOGGER_NAME,LEVEL,...)\
{\
auto&& logger = spdlog::get(LOGGER_NAME);\
if(nullptr == logger)\
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
everest::CloneLogger<decltype(result)>(std::string(LOGGER_NAME));\
logger = spdlog::get(LOGGER_NAME);\
}\
logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, LEVEL, __VA_ARGS__);\
}\

#define VARIABLE_PREFIX result
#define CONCATENATE_DIRECT(s1, s2) s1_##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE CONCATENATE(VARIABLE_PREFIX, __LINE__)

#define CREATE_DEFAULT_LOGGER(LOGGER_NAME,LEVEL,IS_STDOUT,IS_DAILY,IS_ROTATE) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
everest::CreateAsyncLogger<decltype(result)>(std::string(LOGGER_NAME),LEVEL,IS_STDOUT,IS_DAILY,IS_ROTATE);\
auto&& logger = spdlog::get(LOGGER_NAME);\
spdlog::set_default_logger(logger);\
spdlog::set_level(LEVEL);\
}\

#define NAMED_LOG_INFO(name,...) \
CLONE_LOGGER_FROM_DEFAULT_IF_NULL(name,spdlog::level::info,__VA_ARGS__)\

#define NAMED_LOG_DEBUG(name,...) \
CLONE_LOGGER_FROM_DEFAULT_IF_NULL(name,spdlog::level::debug,__VA_ARGS__)\

#define NAMED_LOG_ERROR(name,...) \
CLONE_LOGGER_FROM_DEFAULT_IF_NULL(name,spdlog::level::err,__VA_ARGS__)\

#define NAMED_LOG_WARNING(name,...) \
CLONE_LOGGER_FROM_DEFAULT_IF_NULL(name,spdlog::level::warn,__VA_ARGS__)\

#define NAMED_LOG_CRITICAL(name,...) \
CLONE_LOGGER_FROM_DEFAULT_IF_NULL(name,spdlog::level::critical,__VA_ARGS__)\

#define LOG_INFO(...) \
auto&& logger = spdlog::default_logger();logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)\

#define LOG_DEBUG(...) \
auto&& logger = spdlog::default_logger();logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)\

#define LOG_ERROR(...) \
auto&& logger = spdlog::default_logger();logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)\

#define LOG_WARNING(...) \
auto&& logger = spdlog::default_logger();logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)\

#define LOG_CRITICAL(...) \
auto&& logger = spdlog::default_logger();logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)\


#endif