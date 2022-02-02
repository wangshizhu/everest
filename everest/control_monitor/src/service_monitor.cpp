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

NAMESPACE_EVEREST_END