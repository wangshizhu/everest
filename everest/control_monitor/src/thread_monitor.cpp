NAMESPACE_BEGIN

ThreadMonitor::ThreadMonitor():snapshot_timer_(kSnapshotTimerDuration,true)
{
	thread_monitor_data_.clear();
	registed_thread_.reserve(kMaxThreadNum);
}

void ThreadMonitor::Update()
{
	Snapshot();
}

void ThreadMonitor::OnThreadStart()
{
	snapshot_timer_.Start();
}

void ThreadMonitor::RegisterThread(ThreadBaseSharedPtr thread_ptr)
{
	registed_thread_.push_back(thread_ptr);
}

void ThreadMonitor::PrintThreadMonitorData()const
{
	for (auto&& [thread_id,monitor_data] : thread_monitor_data_)
	{
		EVEREST_LOG_INFO(monitor_data.FormatForLog());
	}
}

void ThreadMonitor::Snapshot()
{
	if (!snapshot_timer_.Expired())
	{
		return;
	}

	PrintThreadMonitorData();

	for (auto iter = registed_thread_.begin();iter != registed_thread_.end();)
	{
		auto thread_base = iter->lock();
		if (nullptr == thread_base)
		{
			iter = registed_thread_.erase(iter);
			continue;
		}

		thread_base->Snapshot(thread_base, 
			[this](ThreadMonitorData data) 
			{
				this->thread_monitor_data_[data.thread_id_] = data;
			});

		++iter;
	}
}

NAMESPACE_END