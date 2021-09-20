#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <map>
#include <future>
#include <string>

#include "singleton/singleton.h"
#include "cmd_line/command_line_parser.h"


#ifdef USE_SPDLOG
#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/everest_spdlog.h"
#include "spdlog/logger_create_info.h"

namespace spd = spdlog;

#else
#include "format/format.h"
#include "log/g3log/g3log.hpp"
#include "log/g3log/logworker.hpp"
#include "log/everest_log.h"

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
#endif

#define CMD_LINE_SINGLETON everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()

namespace {
	uint64_t g_iterations{ 1000000 };

	std::atomic<size_t> g_counter = { 0 };

	std::string GetSaveTestResultFileName()
	{
		bool async_model = CMD_LINE_SINGLETON->Get<bool>("async_model");

#ifdef USE_SPDLOG
		return fmt::format("spdlog_result_{}_{}.csv",
			g_iterations,
			async_model ? "async_model" : "sync_model");
#else
		return fmt::format("g3log_result_{}.csv",g_iterations);
#endif
		
	}

	void WriteToFile(std::string result_filename, std::string content) {

		std::ofstream out;
		std::ios_base::openmode mode = std::ios_base::out | std::ios_base::app;
		;
		out.open(result_filename.c_str(), mode);
		if (!out.is_open()) {
			std::cerr << "Error writing to " << result_filename << std::endl;
		}
		out << content << std::flush;
		std::cout << content;
	}

	void WriteResultHeadToFile()
	{
		auto&& result_file_name = GetSaveTestResultFileName();
		size_t production_thread_num = CMD_LINE_SINGLETON->Get<int>("production_thread_num");

		std::ostringstream oss;
		oss << "Using " << production_thread_num;
		oss << " to log in total " << g_iterations << " log entries to " << result_file_name << std::endl;
		WriteToFile(result_file_name, oss.str());
	}

	void AddCmdLineParam()
	{
		CMD_LINE_SINGLETON->Add<int>("production_thread_num", 'p', "product log thread num", false, 1);

		CMD_LINE_SINGLETON->Add<int>("consumer_thread_num", 'c', "consumer log thread num", false, 1);

		CMD_LINE_SINGLETON->Add<int>("production_model", 'm',
			"produce log model,0 is that single thread produce consistent num log,1 is that all production thread produce consistent num log", false, 0);

		CMD_LINE_SINGLETON->Add<int>("production_log_num", 'l',
			"product log num,when is 0 model that mean single thread produce log num,when is 1 model that mean all production thread produce log num", false, 100000);

		CMD_LINE_SINGLETON->Add<bool>("async_model", 'a',
			"product log message as async model,consumer_thread_num is invalid when async model,this param is invalid when use g3log", false, true);

		CMD_LINE_SINGLETON->AddWithoutValueCommand("help", '?', "show usage");
	}

	void LogMessage(const size_t id)
	{
#ifdef USE_SPDLOG
		NAMED_LOG_INFO("test", "Some text to log for thread: {}", id);
#else
		LOG_INFO << "Some text to log for thread: " << id;
#endif
	}

	void AllThreadWriteConstantNumLog(const size_t id, std::vector<uint64_t>& result)
	{
		while (true) 
		{
			const size_t value_now = ++g_counter;
			if (value_now > g_iterations) 
			{
				return;
			}

			auto start_time = std::chrono::steady_clock::now();

			LogMessage(id);

			auto stop_time = std::chrono::steady_clock::now();
			uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
			result.push_back(time_us);
		}
	}

	void   PrintStats(const std::string& filename, const std::map<size_t, std::vector<uint64_t>>& threads_result, const uint64_t total_time_in_us) {

		size_t idx = 0;
		std::ostringstream oss;
		for (auto t_result : threads_result) 
		{
			uint64_t worstUs = (*std::max_element(t_result.second.begin(), t_result.second.end()));
			oss << idx++ << " the worst thread latency was:" << worstUs / uint64_t(1000) << " ms  (" << worstUs << " us)] " << std::endl;
		}

		oss << "Total time :" << total_time_in_us / uint64_t(1000) << " ms (" << total_time_in_us
			<< " us)" << std::endl;

		int model = CMD_LINE_SINGLETON->Get<int>("production_model");
		size_t number_of_threads = CMD_LINE_SINGLETON->Get<int>("production_thread_num");
		if (model == 0)
		{
			oss << "Average time: " << double(total_time_in_us) / double((g_iterations * number_of_threads)) << " us" << std::endl;
		}
		else
		{
			oss << "Average time: " << double(total_time_in_us) / double(g_iterations) << " us" << std::endl;
		}
		WriteToFile(filename, oss.str());

	}

	void SaveResultToBucketFile(std::string result_filename, const std::map<size_t, std::vector<uint64_t>>& threads_result) {
		// now split the result in buckets of 1ms each so that it's obvious how the peaks go
		std::vector<uint64_t> all_measurements;
		all_measurements.reserve(g_iterations);
		for (auto& t_result : threads_result) {
			all_measurements.insert(all_measurements.end(), t_result.second.begin(), t_result.second.end());
		}

		std::map<uint64_t, uint64_t> bucketsWithEmpty;
		std::map<uint64_t, uint64_t> buckets;
		// force empty buckets to appear
		auto maxValueIterator = std::max_element(all_measurements.begin(), all_measurements.end());
		const auto kMaxValue = *maxValueIterator;

		for (uint64_t idx = 0; idx <= kMaxValue; ++idx) {
			bucketsWithEmpty[idx] = 0;
		}

		for (auto value : all_measurements) {
			++bucketsWithEmpty[value];
			++buckets[value];
		}

		std::cout << "\n\n Microsecond bucket measurement" << std::endl;
		for (const auto ms_bucket : buckets) {
			std::cout << ms_bucket.first << "\t, " << ms_bucket.second << std::endl;
		}
		std::cout << "\n\n";


		std::ostringstream oss;
		// Save to xcel and google doc parsable format. with empty buckets
		oss << "\n\n Microsecond bucket measurement with zero buckets till max" << std::endl;
		for (const auto ms_bucket : bucketsWithEmpty) {
			oss << ms_bucket.first << "\t, " << ms_bucket.second << std::endl;
		}
		WriteToFile(result_filename, oss.str());
		std::cout << "Worst Case Latency, max value: " << kMaxValue << std::endl;
		std::cout << "microsecond bucket result is in file: " << result_filename << std::endl;
	}

	void EachThreadWriteConstantNumLog(const size_t id, std::vector<uint64_t>& result)
	{
		auto start_time_application_total = std::chrono::steady_clock::now();

		for (int i = 0; i < g_iterations; ++i)
		{
			auto start_time = std::chrono::steady_clock::now();

			LogMessage(i);

			auto stop_time = std::chrono::steady_clock::now();

			uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
			result.push_back(time_us);
		}

		auto stop_time_application_total = std::chrono::steady_clock::now();

		uint64_t total_time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time_application_total - start_time_application_total).count();

		std::ostringstream oss;
		oss << "\n thread:" << id << " cost total ms:" << total_time_in_ms << std::endl;

		auto&& result_file_name = GetSaveTestResultFileName();
		WriteToFile(result_file_name, oss.str());
	}

	void Test()
	{
		size_t number_of_threads = CMD_LINE_SINGLETON->Get<int>("production_thread_num");

		std::vector<std::thread> threads(number_of_threads);
		std::map<size_t, std::vector<uint64_t>> threads_result;

		for (size_t idx = 0; idx < number_of_threads; ++idx)
		{
			threads_result[idx].reserve(g_iterations);
		}

		WriteResultHeadToFile();

		int model = CMD_LINE_SINGLETON->Get<int>("production_model");

		auto start_time_application_total = std::chrono::steady_clock::now();
		for (uint64_t idx = 0; idx < number_of_threads; ++idx)
		{
			if (model == 0)
			{
				threads[idx] = std::thread(EachThreadWriteConstantNumLog, idx, std::ref(threads_result[idx]));
			}
			else
			{
				threads[idx] = std::thread(AllThreadWriteConstantNumLog, idx, std::ref(threads_result[idx]));
			}
		}

		for (size_t idx = 0; idx < number_of_threads; ++idx) {
			threads[idx].join();
		}

		auto stop_time_application_total = std::chrono::steady_clock::now();

		uint64_t total_time_in_us = std::chrono::duration_cast<std::chrono::microseconds>(stop_time_application_total - start_time_application_total).count();

		auto&& filename_result = GetSaveTestResultFileName();
		PrintStats(filename_result, threads_result, total_time_in_us);
		SaveResultToBucketFile(filename_result, threads_result);
	}


} // anonymous

int main(int argc,char** argv)
{
	{
		AddCmdLineParam();
		CMD_LINE_SINGLETON->ParseGetOrSetBehavior(argc, argv);

		g_iterations = CMD_LINE_SINGLETON->Get<int>("production_log_num");

#ifdef USE_SPDLOG
		everest::LoggerCreateInfo info;
		info.level = spdlog::level::info;
		info.rotate = false;
		info.to_stdout = false;
		info.async_logger = CMD_LINE_SINGLETON->Get<bool>("async_model");

		CREATE_DEFAULT_LOGGER("test_spd", info);
#else
		auto worker = g3::LogWorker::createLogWorker();
		g3::initializeLogging(worker.get());
		g3::setFatalPreLoggingHook(&breakHere);
		everest::AddCustomLogLevel();

		CREATE_LOGGER(worker.get(), PROGRAM_NAME, path_to_log_file);
#endif
		Test();
		
	}

	//system("pause");
	return 0;
}