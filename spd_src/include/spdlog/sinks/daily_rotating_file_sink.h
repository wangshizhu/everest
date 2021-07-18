#pragma once

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/chrono.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/os.h>
#include <spdlog/details/circular_q.h>
#include <spdlog/details/synchronous_factory.h>

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
		log_clock::time_point next_rotation_tp_();

	private:
		static filename_t calc_filename(const filename_t& filename, const tm& now_tm);

	private:
		filename_t base_filename_;
		std::size_t max_size_;
		std::size_t current_size_;
		bool truncate_;
		int hour_;
		int minute_;
		log_clock::time_point rotation_timepoint_;
		details::file_helper file_helper_;
	};

}
}