#ifndef LISTENER_CONNECTOR_THREAD_H_
#define LISTENER_CONNECTOR_THREAD_H_

NAMESPACE_EVEREST_BEGIN

class ListenerConnectorThread : public everest::ThreadBase
{
public:
	template<typename... Args>
	ListenerConnectorThread(Args&&... args) :ThreadBase(std::forward<Args>(args)...)
	{
	}

public:
	// 线程名称
	virtual const char* Name()const override;

	// 线程类型
	virtual ThreadType GetThreadType() const;

	// 初始化线程
	virtual bool Init() override;

	// 线程更新
	virtual void Update() override;

	// 线程启动的事件
	virtual void OnStart() override;
};

NAMESPACE_EVEREST_END

#endif // !LISTENER_CONNECTOR_THREAD_H_

