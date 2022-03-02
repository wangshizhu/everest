NAMESPACE_EVEREST_BEGIN

ServiceMonitor::ServiceMonitor():BaseType()
{
}

void ServiceMonitor::Register(ServiceBaseSharedPtr monitor_object)
{
	monitor_thread_->Post([self = this, monitor_object = monitor_object]()
	{
		auto service_id = monitor_object->GetServiceId();
		auto iter = self->service_.find(service_id);
    if (iter != self->service_.cend())
    {
      EVEREST_LOG_ERROR("ServiceMonitor::Register service have existed,service_id:{}", service_id);
      return;
    }

    self->service_[service_id] = monitor_object;
	});
}

void ServiceMonitor::Update()
{
}

void ServiceMonitor::OnThreadStart()
{
}

ServiceBaseSharedPtr ServiceMonitor::GetService(ServiceIdType service_id)
{
	auto iter = service_.find(service_id);
	if (iter == service_.cend())
	{
		return nullptr;
	}

	return iter->second;
}

void ServiceMonitor::RegisterSession(ServiceIdType service_id, SessionSharedPtr session)
{
	monitor_thread_->Post([self = this, service_id = service_id,
												session_id = session->GetSessionId(), session = session]()
	{
		self->GetService(service_id)->RegisterSession(session_id,session);
	});
}

NAMESPACE_EVEREST_END