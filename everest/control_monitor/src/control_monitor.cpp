NAMESPACE_EVEREST_BEGIN

ControlMonitor* g_control_monitor = nullptr;

ControlMonitor::ControlMonitor() : monitor_thread_(nullptr)
{
}

ControlMonitor::~ControlMonitor()
{
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
		thread_monitor_.SetMonitorThread(thread_ptr);
		service_monitor_.SetMonitorThread(thread_ptr);
	}

	thread_monitor_.Register(thread_ptr);
}

void ControlMonitor::RegisterService(ServiceBaseSharedPtr service_ptr)
{
	service_monitor_.Register(service_ptr);
}

void ControlMonitor::OnThreadStart()
{
	thread_monitor_.MonitorThreadStart();
	service_monitor_.MonitorThreadStart();
}

ThreadMonitor& ControlMonitor::GetThreadMonitor()
{
	return thread_monitor_;
}

ServiceMonitor& ControlMonitor::GetServiceMonitor()
{
	return service_monitor_;
}

NAMESPACE_EVEREST_END