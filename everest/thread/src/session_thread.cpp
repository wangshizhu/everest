NAMESPACE_EVEREST_BEGIN

const char* SessionThread::Name()const
{
	return "SessionThread";
}

ThreadType SessionThread::GetThreadType() const
{
	return ThreadType::kNetSession;
}

bool SessionThread::Init()
{
	return ThreadBase::Init();
}

void SessionThread::Update()
{
}

void SessionThread::OnStart()
{
}

NAMESPACE_EVEREST_END