#ifndef SERVICE_H_
#define SERVICE_H_

NAMESPACE_EVEREST_BEGIN

class ServiceBase final : public everest::NonCopyable
{
	struct ServicePrivateFlag {};
	friend class LauncherBase;

public:
	ServiceBase(ServiceIdType service_id, const ServicePrivateFlag& placehold);
	~ServiceBase();

public:
	void Stop();

	ServiceIdType GetServiceId()const;

private:
	/*
	* @brief �����������Ĺ�������������������������ô˺������������ʧ��
	*/
	std::shared_ptr<Listener> CreateListener(SessionCreatorSharedPtr session_creator, 
																					 NetThreadAllocateMgrSharedPtr net_thread_allocate);

	/*
	* @brief �����������Ĺ�������,����������������ô˺������������ʧ��
	*/

private:
	ServiceIdType service_id_;
	ListenerSharedPtr listened_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_H_

