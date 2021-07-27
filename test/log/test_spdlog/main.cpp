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
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
#include "singleton/singleton.h"
#include "cmd_line/command_line_parser.h"
#include "spdlog/everest_spdlog.h"

namespace spd = spdlog;

#define CMD_LINE_SINGLETON everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()

namespace {
	uint64_t g_iterations{ 1000000 };


	std::atomic<size_t> g_counter = { 0 };

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

	void AddCmdLineParam()
	{
		CMD_LINE_SINGLETON->Add<int>("production_thread_num", 'p', "product log thread num", false, 1);

		CMD_LINE_SINGLETON->Add<int>("production_model", 'm',
			"produce log model,0 is that single thread produce consistent num log,1 is that all production thread produce consistent num log", false, 0);

		CMD_LINE_SINGLETON->Add<int>("production_log_num", 'l',
			"product log num,when is 0 model that mean single thread produce log num,when is 1 model that mean all production thread produce log num", false, 100000);

		CMD_LINE_SINGLETON->AddWithoutValueCommand("help", '?', "show usage");
	}

	void MeasurePeakDuringLogWrites(const size_t id, std::vector<uint64_t>& result) {

		while (true) {
			const size_t value_now = ++g_counter;
			if (value_now > g_iterations) {
				return;
			}
			auto start_time = std::chrono::high_resolution_clock::now();
			NAMED_LOG_INFO("test", "Some text to log for thread: {}", id);
			//logger->info("Some text to log for thread: {}", id);
			auto stop_time = std::chrono::high_resolution_clock::now();
			uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count();
			result.push_back(time_us);

		}
	}


	void   PrintStats(const std::string& filename, const std::map<size_t, std::vector<uint64_t>>& threads_result, const uint64_t total_time_in_us) {

		size_t idx = 0;
		std::ostringstream oss;
		for (auto t_result : threads_result) {
			uint64_t worstUs = (*std::max_element(t_result.second.begin(), t_result.second.end()));
			oss << idx++ << " the worst thread latency was:" << worstUs / uint64_t(1000) << " ms  (" << worstUs << " us)] " << std::endl;
		}


		oss << "Total time :" << total_time_in_us / uint64_t(1000) << " ms (" << total_time_in_us
			<< " us)" << std::endl;
		oss << "Average time: " << double(total_time_in_us) / double(g_iterations) << " us" << std::endl;
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

	void WriteLog()
	{
		auto start_time_application_total = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < g_iterations; ++i)
		{
			//SPDLOG_LOGGER_CALL(logger, spdlog::level::info, "Some text to log for thread: {}", i);
			NAMED_LOG_INFO("test", "Some text to log for thread: {}", i);
			LOG_DEBUG("debug message {}",i);
			//logger->info("Some text to log for thread: {}", i);
		}

		auto stop_time_application_total = std::chrono::high_resolution_clock::now();

		uint64_t total_time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time_application_total - start_time_application_total).count();

		std::cout << "thread id:" << std::this_thread::get_id() << " total time: " << total_time_in_ms << "ms" << std::endl;
	}

	void TestForModel0()
	{
		size_t number_of_threads = CMD_LINE_SINGLETON->Get<int>("production_thread_num");
		std::vector<std::thread> all;

		auto start_time_application_total = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < number_of_threads; ++i)
		{
			all.push_back(std::thread(WriteLog));
		}

		for (int i = 0; i < number_of_threads; i++)
		{
			all[i].join();
		}

		auto stop_time_application_total = std::chrono::high_resolution_clock::now();

		uint64_t total_time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time_application_total - start_time_application_total).count();

		std::cout << "model 0,total time: " << total_time_in_ms << "ms" << std::endl;
		// crash
		/*std::vector<int> v;
		v[0] = 5;*/
	}

	void TestForModel1()
	{
		size_t number_of_threads = CMD_LINE_SINGLETON->Get<int>("production_thread_num");

		std::vector<std::thread> threads(number_of_threads);
		std::map<size_t, std::vector<uint64_t>> threads_result;

		for (size_t idx = 0; idx < number_of_threads; ++idx) {
			// reserve to 1 million for all the result
			// it's a test so  let's not care about the wasted space
			threads_result[idx].reserve(g_iterations);
		}

		auto filename_result = fmt::format("test.result_spd_{}.csv",g_iterations);

		std::ostringstream oss;
		oss << "Using " << number_of_threads;
		oss << " to log in total " << g_iterations << " log entries to " << filename_result << std::endl;
		WriteToFile(filename_result, oss.str());


		auto start_time_application_total = std::chrono::high_resolution_clock::now();
		for (uint64_t idx = 0; idx < number_of_threads; ++idx) {
			threads[idx] = std::thread(MeasurePeakDuringLogWrites, idx, std::ref(threads_result[idx]));
		}
		for (size_t idx = 0; idx < number_of_threads; ++idx) {
			threads[idx].join();
		}
		auto stop_time_application_total = std::chrono::high_resolution_clock::now();

		uint64_t total_time_in_us = std::chrono::duration_cast<std::chrono::microseconds>(stop_time_application_total - start_time_application_total).count();

		PrintStats(filename_result, threads_result, total_time_in_us);
		SaveResultToBucketFile(filename_result, threads_result);
	}


} // anonymous


// The purpose of this test is NOT to see how fast
// each thread can possibly write. It is to see what
// the worst latency is for writing a log entry
//
// In the test 1 million log entries will be written
// an atomic counter is used to give each thread what
// it is to write next. The overhead of atomic
// synchronization between the threads are not counted in the worst case latency
int main(int argc, char** argv)
{
	{
		AddCmdLineParam();
		CMD_LINE_SINGLETON->ParseGetOrSetBehavior(argc, argv);

		std::cout << CMD_LINE_SINGLETON->GetProgramName() << std::endl;

		g_iterations = CMD_LINE_SINGLETON->Get<int>("production_log_num");

		CREATE_DEFAULT_LOGGER("test_spd",spdlog::level::debug,true,true,true);

		int model = CMD_LINE_SINGLETON->Get<int>("production_model");
		if (model == 0)
		{
			TestForModel0();
		}
		else
		{
			TestForModel1();
		}
	}

	//system("pause");
	return 0;
}
