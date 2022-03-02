#ifndef SERVICE_H_
#define SERVICE_H_

NAMESPACE_EVEREST_BEGIN

class ServiceBase final : private everest::NonCopyable
{
	struct ServicePrivateFlag {};
	friend class LauncherBase;

public:
	ServiceBase(ServiceIdType service_id, const ServicePrivateFlag& placehold);
	~ServiceBase();

public:
	void Stop();

	ServiceIdType GetServiceId()const;

	void RegisterSession(SessionIdType session_id, SessionSharedPtr session);

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
	std::unordered_map<SessionIdType, SessionSharedPtr> session_;
	std::size_t online_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_H_

