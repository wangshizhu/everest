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
};

NAMESPACE_EVEREST_END

#endif // !SERVICE_H_

