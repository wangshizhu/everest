NAMESPACE_EVEREST_BEGIN

std::string ThreadMonitorData::FormatForLog() const
{
	return fmt::format("thread_id:{},thread_full_name:{},pending_num:{},update_interval:{},"
		"execute_once_max_time: {},thread_state_flag: {},last_snapshot_diff_now:{}",
		thread_id_,
		thread_full_name_,
		pending_num_,
		NanosecondConvertSecond(interval_.count()),
		NanosecondConvertSecond(execute_once_max_time_),
		thread_state_describe[thread_state_flag_.to_ulong()],
		NanosecondConvertSecond((g_tls_clock->LatestTimePoint() - last_timepoint_).count()));
}

std::string ThreadMonitorData::NanosecondConvertSecond(uint64_t nanosecond) const
{
	auto second = TimeCapsule::DurationCountToDurationCount<std::chrono::seconds,
		std::chrono::steady_clock::duration>(nanosecond);
	auto mill_second = TimeCapsule::DurationCountToDurationCount<std::chrono::milliseconds,
		std::chrono::steady_clock::duration>(nanosecond - second * std::nano::den);
	auto micro_second = TimeCapsule::DurationCountToDurationCount<std::chrono::microseconds,
		std::chrono::steady_clock::duration>(nanosecond - second * std::nano::den - mill_second * std::micro::den);
	auto nano_second = nanosecond - second * std::nano::den - mill_second * std::micro::den - micro_second * std::milli::den;

	return fmt::format("{}s-{}ms-{}us-{}ns",
		second, mill_second, micro_second, nano_second);
}

NAMESPACE_EVEREST_END