NAMESPACE_EVEREST_BEGIN

ControlMonitor::ControlMonitor() : monitor_thread_(nullptr)
{
}

ControlMonitor::~ControlMonitor()
{
	monitor_thread_->Stop();
	monitor_thread_->Join();
}

void ControlMonitor::Update()
{
	thread_monitor_.Update();
}

void ControlMonitor::RegisterThread(ThreadBaseSharedPtr thread_ptr)
{
	if (thread_ptr->GetThreadType() == ThreadType::kMonitor)
	{
		monitor_thread_ = thread_ptr;
	}

	thread_monitor_.RegisterThread(thread_ptr);
}

void ControlMonitor::OnThreadStart()
{
	thread_monitor_.OnThreadStart();
}

ThreadMonitor& ControlMonitor::GetThreadMonitor()
{
	return thread_monitor_;
}

NAMESPACE_EVEREST_END