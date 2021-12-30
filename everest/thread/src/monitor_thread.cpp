NAMESPACE_BEGIN

const char* MonitorThread::Name()const
{
	return "MonitorThread";
}

bool MonitorThread::Init()
{
	return ThreadBase::Init();
}

void MonitorThread::Update()
{
	CONTROL_MONITOR_SINGLETON->Update();
}

void MonitorThread::OnStart()
{
	CONTROL_MONITOR_SINGLETON->OnThreadStart();
}


NAMESPACE_END