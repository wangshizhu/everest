NAMESPACE_EVEREST_BEGIN

ServiceBase::ServiceBase(ServiceIdType service_id,const ServicePrivateFlag& placehold)
	:service_id_(service_id)
{
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