NAMESPACE_BEGIN

ControlMonitor::ControlMonitor()
{
}

void ControlMonitor::Update()
{
	thread_monitor_.Update();
}

void ControlMonitor::OnThreadStart()
{
	thread_monitor_.OnThreadStart();
}

ThreadMonitor& ControlMonitor::GetThreadMonitor()
{
	return thread_monitor_;
}

NAMESPACE_END