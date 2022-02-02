NAMESPACE_EVEREST_BEGIN

ThreadMonitor::ThreadMonitor()
	:BaseType()
{
	thread_monitor_data_.clear();
	registed_thread_.reserve(BaseType::kMonitorObjectMaxNum);
}

void ThreadMonitor::Update()
{
	Snapshot();
}

void ThreadMonitor::OnThreadStart()
{
}

void ThreadMonitor::Register(ThreadBaseSharedPtr thread_ptr)
{
	monitor_thread_->Post([self = this,thread_ptr = thread_ptr]()
	{
		self->registed_thread_.push_back(thread_ptr);
	});
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

		thread_base->Snapshot(monitor_thread_,
													[this](ThreadMonitorData data)
		{
			// 记录最后一次收集数据时间点
			data.last_timepoint_ = g_tls_clock->LatestTimePoint();

			this->thread_monitor_data_[data.thread_id_] = data;
		});

		++iter;
	}
}

NAMESPACE_EVEREST_END