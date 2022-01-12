#ifndef MONITOR_THREAD_H_
#define MONITOR_THREAD_H_

NAMESPACE_EVEREST_BEGIN

class MonitorThread final : public ThreadBase
{
public:
	template<typename... Args>
	MonitorThread(Args&&... args) :ThreadBase(std::forward<Args>(args)...)
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

bool CreateAndStartMonitorThread();

NAMESPACE_EVEREST_END

#endif
