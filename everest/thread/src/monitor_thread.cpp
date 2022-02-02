NAMESPACE_EVEREST_BEGIN

const char* MonitorThread::Name()const
{
	return "MonitorThread";
}

ThreadType MonitorThread::GetThreadType() const
{
	return ThreadType::kMonitor;
}

bool MonitorThread::Init()
{
	return ThreadBase::Init();
}

void MonitorThread::Update()
{
	g_control_monitor->Update();
}

void MonitorThread::OnStart()
{
	g_control_monitor->OnThreadStart();
}


NAMESPACE_EVEREST_END