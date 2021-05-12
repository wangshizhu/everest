#include <iostream>
#include <future>

#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"
#include "everest_log.h"

const std::string path_to_log_file = "./log/";

void breakHere() {
	std::ostringstream oss;
	oss << "Fatal hook function: " << __FUNCTION__ << ":" << __LINE__ << " was called";
	oss << " through g3::setFatalPreLoggingHook(). setFatalPreLoggingHook should be called AFTER g3::initializeLogging()" << std::endl;
	LOG(G3LOG_DEBUG) << oss.str();
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	//__debugbreak();
#endif
}

int main(int argc, char** argv)
{
	{
		auto worker = g3::LogWorker::createLogWorker();
		g3::initializeLogging(worker.get());
		g3::setFatalPreLoggingHook(&breakHere);
		everest::AddCustomLogLevel();
		//std::future<std::string> log_file_name = handle->call(std::bind(&g3::FileSink::fileName, handle->sink()->RealSink()));

		CREATE_LOGGER(worker.get(), PROGRAM_NAME, path_to_log_file);
		CREATE_LOGGER(worker.get(),"world", path_to_log_file);
		CREATE_LOGGER(worker.get(), "gate", path_to_log_file);
		
		NAMED_LOG_INFO("world") <<"test named log";
		LOG_INFO << "main log msg";
		NAMED_LOG_ERROR("gate") << "second test named log";

		//CHECK(i == 0)<<"error value";
	}

	system("pause");
	return 0;
}