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
	CONTROL_MONITOR_SINGLETON()->Update();
}

void MonitorThread::OnStart()
{
	CONTROL_MONITOR_SINGLETON()->OnThreadStart();
}

bool CreateAndStartMonitorThread()
{
	auto ptr = MonitorThread::CreateThread<everest::MonitorThread>();
	if (nullptr == ptr)
	{
		return false;
	}

	ptr->Start();

	return true;
}

// TODO: Ë½ÓÐ»¯
bool created_moniter_thread = CreateAndStartMonitorThread();


NAMESPACE_EVEREST_END