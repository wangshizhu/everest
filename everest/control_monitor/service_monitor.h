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
	* @brief: ע��service
	* @param: [in] monitor_object service
	* @note: �̰߳�ȫ
	*/
	void Register(ServiceBaseSharedPtr monitor_object) override;

	/*
	* @brief: monitor�̵߳��ȵ�update
	*/
	void Update() override;

	/*
	* @brief: monitor�߳������¼�����
	*/
	void OnThreadStart() override;

public:
	/*
	* @brief: ��ȡ����
	* @param: [in] service_id service id
	* @note: ���̰߳�ȫ
	*/
	ServiceBaseSharedPtr GetService(ServiceIdType service_id);
	
	/*
	* @brief: ע��session
  * @param: [in] service_id service id
	* @param: [in] session std::shared_ptr<SessionBase>
  * @note: �̰߳�ȫ
	*/
	void RegisterSession(ServiceIdType service_id, SessionSharedPtr session);

	/*
	* @brief: �Ƴ�session
	* @param: [in] service_id service id
	* @param: [in] session_id session_id
	* @note: �̰߳�ȫ
	*/
	void UnregisterSession(ServiceIdType service_id, SessionIdType session_id);

private:
	std::map<ServiceIdType, ServiceBaseSharedPtr> service_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_MONITOR_H_

