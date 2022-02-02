#ifndef SERVICE_MONITOR_H_
#define SERVICE_MONITOR_H_

NAMESPACE_EVEREST_BEGIN

class ServiceBase;
using ServiceBaseSharedPtr = std::shared_ptr<ServiceBase>;

class ServiceMonitor final : public everest::MonitorBase<ServiceBaseSharedPtr>
{
	using BaseType = everest::MonitorBase<ServiceBaseSharedPtr>;

public:
	ServiceMonitor();

public:
	void Register(ServiceBaseSharedPtr monitor_object) override;

	void Update() override;

	void OnThreadStart() override;

public:
	ServiceBaseSharedPtr GetService(ServiceIdType service_id);

private:
	std::map<ServiceIdType, ServiceBaseSharedPtr> service_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_MONITOR_H_

