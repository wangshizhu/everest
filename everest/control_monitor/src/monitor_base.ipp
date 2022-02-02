NAMESPACE_EVEREST_BEGIN

template<class MonitorType>
MonitorBase<MonitorType>::MonitorBase(uint32_t snapshot_timer_second_duration /*= kSnapshotTimerDuration*/)
	:snapshot_timer_(snapshot_timer_second_duration, true), monitor_thread_(nullptr)
{
}

template<class MonitorType>
void MonitorBase<MonitorType>::MonitorThreadStart()
{
	snapshot_timer_.Start();
	OnThreadStart();
}

template<class MonitorType>
void MonitorBase<MonitorType>::SetMonitorThread(ThreadBaseSharedPtr monitor_thread)
{
	monitor_thread_ = monitor_thread;
}

NAMESPACE_EVEREST_END