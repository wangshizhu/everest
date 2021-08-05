#pragma once

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/chrono.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>

#include <chrono>
#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {

	template<typename Mutex>
	class daily_rotating_file_sink final : public base_sink<Mutex>
	{
	public:
		daily_rotating_file_sink(filename_t base_filename, std::size_t max_size, int hour, int minute, bool truncate = false);

	protected:
		void sink_it_(const details::log_msg& msg) override;
		void flush_() override;

	private:
		tm now_tm(log_clock::time_point tp);
		log_clock::time_point next_rotation_tp();
		filename_t calc_filename(const filename_t& filename, const tm& now_tm);

	private:
		filename_t base_filename_;
		std::size_t max_size_;
		std::size_t current_size_;
		bool truncate_;
		int hour_;
		int minute_;
		int increment_sequence_;
		log_clock::time_point rotation_timepoint_;
		details::file_helper file_helper_;
	};

	using daily_rotating_file_sink_mt = daily_rotating_file_sink<std::mutex>;
	using daily_rotating_file_sink_st = daily_rotating_file_sink<details::null_mutex>;
}

template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> daily_rotating_logger_mt(
	const std::string& logger_name, const filename_t& filename, size_t max_file_size, int hour, int minute, bool rotate_on_open = false)
{
	return Factory::template create<sinks::daily_rotating_file_sink_mt>(logger_name, filename, max_file_size, hour,minute, rotate_on_open);
}

template<typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> daily_rotating_logger_st(
	const std::string& logger_name, const filename_t& filename, size_t max_file_size, int hour, int minute, bool rotate_on_open = false)
{
	return Factory::template create<sinks::daily_rotating_file_sink_st>(logger_name, filename, max_file_size, hour, minute, rotate_on_open);
}

}

#ifdef SPDLOG_HEADER_ONLY
#include "daily_rotating_file_sink-inl.h"
#endif