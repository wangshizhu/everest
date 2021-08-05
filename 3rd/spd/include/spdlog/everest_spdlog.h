#ifndef EVEREST_SPDLOG_H_
#define EVEREST_SPDLOG_H_

#include <utility>

#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/logger.h"
#include "spdlog/logger_create_info.h"
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
		AsyncLoggerFactory(const std::string& logger_name,const LoggerCreateInfo& info)
		{
			std::vector<spdlog::sink_ptr> sinks;

			// 是否输出到控制台
			if (info.to_stdout)
			{
				sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
			}

			if (info.daily && info.rotate)
			{
				auto daily_sink = std::make_shared<spdlog::sinks::daily_rotating_file_sink_mt>(logger_name, info.log_file_size, 0, 0, false);
				sinks.push_back(daily_sink);
			}
			else
			{
				// 每天固定时间产生新log文件
				if (info.daily)
				{
					auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logger_name, 0, 0, false);
					sinks.push_back(daily_sink);
				}

				// 按照文件大小产生新log文件
				if (info.rotate)
				{
					auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logger_name, info.log_file_size, 3);
					sinks.push_back(rotating_sink);
				}
			}

			auto& registry_inst = spdlog::details::registry::instance();

			auto& mutex = registry_inst.tp_mutex();
			std::lock_guard<std::recursive_mutex> tp_lock(mutex);

			auto tp = registry_inst.get_tp();
			if (nullptr == tp)
			{
				tp = std::make_shared<spdlog::details::thread_pool>(info.async_queue_size, info.consumer_thread_num);
				registry_inst.set_tp(tp);
			}

			auto logger = std::make_shared<spdlog::async_logger>(logger_name, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			logger->set_level(info.level);

			spdlog::register_logger(logger);
		}
	};

	template<class T>
	void CreateAsyncLogger(const std::string& logger_name,const LoggerCreateInfo& info)
	{
		static AsyncLoggerFactory<T> logger(logger_name, info);
	}
}


#define STRING_LITERAL_TO_SEQUENCE(s) \
    (everest::MakeIndexSequenceImpl([] { \
        struct tmp { static constexpr decltype(auto) get() { return s; } }; \
        return tmp{}; \
    }()))\

#define CREATE_ASYNC_LOGGER(LOGGER_NAME,LOGGER_CREATE_INFO) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
everest::CreateAsyncLogger<decltype(result)>(std::string(LOGGER_NAME),LOGGER_CREATE_INFO);\
}\

#define CREATE_LOGGER_IF_NULL(LOGGER_NAME,LEVEL,...)\
{\
auto&& logger = spdlog::get(LOGGER_NAME);\
if(nullptr == logger)\
{\
auto&& info = spdlog::details::registry::instance().get_logger_create_info();\
CREATE_ASYNC_LOGGER(LOGGER_NAME,info);\
logger = spdlog::get(LOGGER_NAME);\
}\
logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, LEVEL, __VA_ARGS__);\
}\

#define VARIABLE_PREFIX result
#define CONCATENATE_DIRECT(s1, s2) s1_##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE CONCATENATE(VARIABLE_PREFIX, __LINE__)

#define CREATE_DEFAULT_LOGGER(LOGGER_NAME,LOGGER_CREATE_INFO) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
everest::CreateAsyncLogger<decltype(result)>(std::string(LOGGER_NAME),LOGGER_CREATE_INFO);\
auto&& logger = spdlog::get(LOGGER_NAME);\
spdlog::set_default_logger(logger);\
spdlog::set_level(LOGGER_CREATE_INFO.level);\
spdlog::details::registry::instance().set_logger_create_info(LOGGER_CREATE_INFO);\
}\

#define NAMED_LOG_INFO(name,...) \
CREATE_LOGGER_IF_NULL(name,spdlog::level::info,__VA_ARGS__)\

#define NAMED_LOG_DEBUG(name,...) \
CREATE_LOGGER_IF_NULL(name,spdlog::level::debug,__VA_ARGS__)\

#define NAMED_LOG_ERROR(name,...) \
CREATE_LOGGER_IF_NULL(name,spdlog::level::err,__VA_ARGS__)\

#define NAMED_LOG_WARNING(name,...) \
CREATE_LOGGER_IF_NULL(name,spdlog::level::warn,__VA_ARGS__)\

#define NAMED_LOG_CRITICAL(name,...) \
CREATE_LOGGER_IF_NULL(name,spdlog::level::critical,__VA_ARGS__)\

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