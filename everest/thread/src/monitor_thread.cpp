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
	ControlMonitorSingleton::GetInstance()->Update();
}

void MonitorThread::OnStart()
{
	ControlMonitorSingleton::GetInstance()->OnThreadStart();
}


NAMESPACE_END