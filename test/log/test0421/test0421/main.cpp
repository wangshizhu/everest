#include <iostream>
#include <future>

#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"

const std::string path_to_log_file = "./log/";

void breakHere() {
	std::ostringstream oss;
	oss << "Fatal hook function: " << __FUNCTION__ << ":" << __LINE__ << " was called";
	oss << " through g3::setFatalPreLoggingHook(). setFatalPreLoggingHook should be called AFTER g3::initializeLogging()" << std::endl;
	LOG(G3LOG_DEBUG) << oss.str();
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	__debugbreak();
#endif
}

int main()
{
	{
		auto worker = g3::LogWorker::createLogWorker();
		auto handle = worker->addDefaultLogger("test", path_to_log_file);
		g3::initializeLogging(worker.get());
		g3::setFatalPreLoggingHook(&breakHere);
		std::future<std::string> log_file_name = handle->call(&g3::FileSink::fileName);

		worker->addSink(std::make_unique<g3::FileSink>("sink", path_to_log_file, "sink"), &g3::FileSink::fileWrite);

		LOGF(G3LOG_DEBUG, "Fatal exit example starts now, it's as easy as  %d", 123);
		LOG(INFO) << "Feel free to read the source code also in g3log/example/main_fatal_choice.cpp";
	}
	system("pause");
	return 0;
}