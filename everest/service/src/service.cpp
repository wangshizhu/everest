NAMESPACE_EVEREST_BEGIN

ServiceBase::ServiceBase(ServiceIdType service_id,const ServicePrivateFlag& placehold)
	:service_id_(service_id), listened_(nullptr), online_(0)
{
  session_.clear();
}

ServiceBase::~ServiceBase()
{
}

void ServiceBase::Stop()
{
}

ServiceIdType ServiceBase::GetServiceId()const
{
	return service_id_;
}

void ServiceBase::RegisterSession(SessionIdType session_id,SessionSharedPtr session)
{
  auto iter = session_.find(session_id);
  if (iter != session_.cend())
  {
    EVEREST_LOG_ERROR("ServiceBase::RegisterSession find registered session,session_id:{}", session_id);
  }

  session_[session_id] = session;
  ++online_;
  
  // 开始接收网络消息
  session->Read();
}

void ServiceBase::UnregisterSession(SessionIdType session_id)
{
  auto iter = session_.find(session_id);
  if (iter == session_.cend())
  {
    EVEREST_LOG_ERROR("ServiceBase::UnregisterSession dont find registered session,session_id:{}", session_id);
    return;
  }

  session_.erase(session_id);
  --online_;
}

std::shared_ptr<Listener> ServiceBase::CreateListener(SessionCreatorSharedPtr session_creator, 
																											NetThreadAllocateMgrSharedPtr net_thread_allocate)
{
  auto&& listener = std::make_shared<Listener>(service_id_, session_creator, net_thread_allocate, Listener::PrivateFlag());
  if (!listener->StartListen())
  {
	  return nullptr;
  }

  listened_ = listener;

  return listener;
}

NAMESPACE_EVEREST_END