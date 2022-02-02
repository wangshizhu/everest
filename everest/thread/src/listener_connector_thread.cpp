NAMESPACE_EVEREST_BEGIN

const char* ListenerConnectorThread::Name()const
{
	return "ListenerConnectorThread";
}

ThreadType ListenerConnectorThread::GetThreadType() const
{
	return ThreadType::kNetListenerConnector;
}

bool ListenerConnectorThread::Init()
{
	return ThreadBase::Init();
}

void ListenerConnectorThread::Update()
{
}

void ListenerConnectorThread::OnStart()
{
}

NAMESPACE_EVEREST_END