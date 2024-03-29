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
	void UnregisterSession(SessionIdType session_id);

private:
	/*
	* @brief 创建监听器的工厂函数，创建监听器必须调用此函数，否则编译失败
	*/
	std::shared_ptr<Listener> CreateListener(SessionCreatorSharedPtr session_creator, 
																					 NetThreadAllocateMgrSharedPtr net_thread_allocate);

	/*
	* @brief 创建连接器的工厂函数,创建连接器必须调用此函数，否则编译失败
	*/

private:
	ServiceIdType service_id_;
	ListenerSharedPtr listened_;
	std::unordered_map<SessionIdType, SessionSharedPtr> session_;
	std::size_t online_;
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_H_

