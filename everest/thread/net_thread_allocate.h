#ifndef THREAD_ALLOCATE_H_
#define THREAD_ALLOCATE_H_

NAMESPACE_EVEREST_BEGIN

class ServiceNetThreadManager final : private everest::NonCopyable
{
public:
	ServiceNetThreadManager(uint32_t policy_id);
	~ServiceNetThreadManager();

public:
	// ֹͣ
	void Stop();

	// �������ô�������߳�
	bool CreateAndStartNetThreadByConfig();

	// ��ȡ���ڼ��������ӵ��߳�
	ThreadBaseSharedPtr GetListenerConnectorThread();

	// ��ȡ����session���߳�
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

