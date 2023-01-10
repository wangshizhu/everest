#ifndef SERVICE_MONITOR_H_
#define SERVICE_MONITOR_H_

NAMESPACE_EVEREST_BEGIN

class ServiceBase;
using ServiceBaseSharedPtr = std::shared_ptr<ServiceBase>;

class SessionBase;
using SessionSharedPtr = std::shared_ptr<SessionBase>;

class ServiceMonitor final : public everest::MonitorBase<ServiceBaseSharedPtr>
{
	using BaseType = everest::MonitorBase<ServiceBaseSharedPtr>;

public:
	ServiceMonitor();

public:
	/*
	* @brief: 注册service
	* @param: [in] monitor_object service
	* @note: 线程安全
	*/
	void Register(ServiceBaseSharedPtr monitor_object) override;

	/*
	* @brief: monitor线程调度的update
	*/
	void Update() override;

	/*
	* @brief: monitor线程启动事件函数
	*/
	void OnThreadStart() override;

public:
	/*
	* @brief: 获取服务
	* @param: [in] service_id service id
	* @note: 非线程安全
	*/
	ServiceBaseSharedPtr GetService(ServiceIdType service_id);
	
	/*
	* @brief: 注册session
  * @param: [in] service_id service id
	* @param: [in] session std::shared_ptr<SessionBase>
  * @note: 线程安全
	*/
	void RegisterSession(ServiceIdType service_id, SessionSharedPtr session);

	/*
	* @brief: 移除session
	* @param: [in] service_id service id
	* @param: [in] session_id session_id
	* @note: 线程安全
	*/
	void UnregisterSession(ServiceIdType service_id, SessionIdType session_id);

private:
	std::map<ServiceIdType, ServiceBaseSharedPtr> service_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_MONITOR_H_

