#ifndef THREAD_ALLOCATE_H_
#define THREAD_ALLOCATE_H_

NAMESPACE_EVEREST_BEGIN

class ServiceNetThreadManager final : private everest::NonCopyable
{
public:
	ServiceNetThreadManager(uint32_t policy_id);
	~ServiceNetThreadManager();

public:
	// 停止
	void Stop();

	// 根据配置创建相关线程
	bool CreateAndStartNetThreadByConfig();

	// 获取用于监听和连接的线程
	ThreadBaseSharedPtr GetListenerConnectorThread();

	// 获取处理session的线程
	ThreadBaseSharedPtr AllocateSessionThread();

private:
	uint32_t policy_id_;
	ThreadBaseSharedPtr reactor_single_thread_;
	ThreadBaseSharedPtr listener_connector_thread_;
	std::unique_ptr<SessionThreadPool> session_thread_pool_;

};

using NetThreadAllocateMgrSharedPtr = std::shared_ptr<ServiceNetThreadManager>;

NAMESPACE_EVEREST_END

#endif // !THREAD_ALLOCATE_H_

