#pragma once
#include <spdlog/sinks/daily_rotating_file_sink.h>

namespace spdlog
{
	namespace sinks
	{
		template<typename Mutex>
		SPDLOG_INLINE daily_rotating_file_sink<Mutex>::daily_rotating_file_sink(filename_t base_filename, std::size_t max_size, int hour, int minute, bool truncate /*= false*/)
			:base_filename_(std::move(base_filename))
			,max_size_(max_size)
			,current_size_(0)
			,truncate_(truncate)
			,hour_(hour)
			,minute_(minute)
		{
			if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
			{
				throw_spdlog_ex("daily_rotating_file_sink: Invalid rotation time in ctor");
			}

			auto now = log_clock::now();
			auto filename = calc_filename(base_filename_, now_tm(now));
			file_helper_.open(filename, truncate_);
			rotation_timepoint_ = next_rotation_tp_();
		}

		template<typename Mutex>
		SPDLOG_INLINE filename_t daily_rotating_file_sink<Mutex>::calc_filename(const filename_t& filename, const tm& now_tm)
		{
			filename_t basename, ext;
			std::tie(basename, ext) = details::file_helper::split_by_extension(filename);

			return fmt::format(
				SPDLOG_FILENAME_T("{}-{:04d}-{:02d}-{:02d}-{:02d}-{:02d}-{:02d}{}"),
				basename,
				now_tm.tm_year + 1900,
				now_tm.tm_mon + 1,
				now_tm.tm_mday,
				now_tm.tm_hour,
				now_tm.tm_min,
				now_tm.tm_sec,
				ext);
		}

		template<typename Mutex>
		SPDLOG_INLINE log_clock::time_point daily_rotating_file_sink<Mutex>::next_rotation_tp_()
		{
			auto now = log_clock::now();
			tm date = now_tm(now);
			date.tm_hour = hour_;
			date.tm_min = minute_;
			date.tm_sec = 0;

			auto rotation_time = log_clock::from_time_t(std::mktime(&date));
			if (rotation_time > now)
			{
				return rotation_time;
			}

			return { rotation_time + std::chrono::hours(24) };
		}

		template<typename Mutex>
		SPDLOG_INLINE tm daily_rotating_file_sink<Mutex>::now_tm(log_clock::time_point tp)
		{
			time_t tnow = log_clock::to_time_t(tp);
			return spdlog::details::os::localtime(tnow);
		}
	}
}