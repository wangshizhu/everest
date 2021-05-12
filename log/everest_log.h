#ifndef EVEREST_LOG_H_
#define EVEREST_LOG_H_

#include <utility>

#include "g3log/sinkhandle.hpp"
#include "g3log/filesink.hpp"
#include "g3log/g3log.hpp"
#include "custom_level.h"

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
struct LoggerFactory
{
	LoggerFactory(g3::LogWorker* w,const std::string& name, const std::string& log_directory)
	{
		logger_ = w->addSink(std::make_unique<g3::FileSink>(name, log_directory, name), &(g3::FileSink::fileWrite));
	}

	inline static std::unique_ptr<g3::FileSinkHandle> logger_ = nullptr;
};

template<class T>
void CreateLogger(g3::LogWorker* worker,const std::string& logger_name,const std::string& log_directory)
{
	static LoggerFactory<T> logger(worker, logger_name, log_directory);
}

#define CREATE_LOGGER(WORKER,LOGGER_NAME,LOG_DIRECTORY) \
{\
auto result = STRING_LITERAL_TO_SEQUENCE(LOGGER_NAME); \
CreateLogger<decltype(result)>(WORKER,std::string(LOGGER_NAME),LOG_DIRECTORY);\
}\

#define VARIABLE_PREFIX result
#define CONCATENATE_DIRECT(s1, s2) s1_##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
# define ANONYMOUS_VARIABLE CONCATENATE(VARIABLE_PREFIX, __LINE__)

#define NAMED_LOG_INFO(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(INFO)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), INFO,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOGIF_INFO(name,boolean_expression) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(INFO) || false == (boolean_expression)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), INFO,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOG_DEBUG(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(G3LOG_DEBUG)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), G3LOG_DEBUG,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOGIF_DEBUG(name,boolean_expression) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(G3LOG_DEBUG) || false == (boolean_expression)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), G3LOG_DEBUG,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOG_ERROR(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(ERR)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), ERR,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOGIF_ERROR(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(ERR) || false == (boolean_expression)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), ERR,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOG_WARNING(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(WARNING)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), WARNING,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOGIF_WARNING(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(WARNING) || false == (boolean_expression)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), WARNING,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOG_FATAL(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(FATAL)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), FATAL,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\

#define NAMED_LOGIF_FATAL(name) \
auto ANONYMOUS_VARIABLE= STRING_LITERAL_TO_SEQUENCE(name); if (!g3::logLevel(FATAL) || false == (boolean_expression)) {} else LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), FATAL,LoggerFactory<decltype(ANONYMOUS_VARIABLE)>::logger_.get()).stream()\



#define LOG_INFO \
NAMED_LOG_INFO(PROGRAM_NAME)

#define LOG_DEBUG \
NAMED_LOG_DEBUG(PROGRAM_NAME)

#define LOG_ERROR \
NAMED_LOG_ERROR(PROGRAM_NAME)

#define LOG_WARNING \
NAMED_LOG_WARNING(PROGRAM_NAME)

#define LOG_FATAL \
NAMED_LOG_FATAL(PROGRAM_NAME)

#endif // !EVEREST_LOG_H_

