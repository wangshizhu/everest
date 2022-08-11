#ifndef LAUNCHER_BASE_H_
#define LAUNCHER_BASE_H_

NAMESPACE_EVEREST_BEGIN

/// <summary>
/// 进程基类，外部必须通过调用StartLauncher()才能启动进程
/// 基类会按照如下调用顺序向子类传递事件：
/// 1.OnLoadServiceConfig()
/// 2.OnStartLauncherBefore()
/// 3.OnStartLauncher()
/// </summary>

class LauncherBase : private everest::NonCopyable
{
public:
	LauncherBase();
	virtual ~LauncherBase();

public:
	/*
	* @brief:进程开始运行的唯一函数
	*/
	bool StartLauncher();

protected:
	/*
	* @brief: 子类需要加载配置数据到ConfigurationManager中，为下面创建listener或者connector服务准备好配置数据
	*/
	virtual void OnLoadServiceConfig() = 0;

	/*
	* @brief:向子类传递进程启动之前的事件
	*/
	virtual void OnStartLauncherBefore();

	/*
	* @brief:向子类传递进程启动之后的事件
	*/
	virtual void OnStartLauncher();

	/*
	* @brief:进程名称，例如：center、login、gate等等
	*/
	virtual const char* Name()const = 0;

protected:
	/*
	* @brief:子类可以在虚函数OnStartLauncher内部创建一个服务
	* 服务：只是listener或者connector
	* 
	* @param [in] service_id 服务id，内部会根据服务id去ConfigurationManager中查找这个服务id的
	* 配置数据，配置数据决定了这个服务是listener还是connector
	* 
	* @param [in] session_creator session的工厂函数，当accept新连接或者connect成功时要创建session,
	* 具体是什么session则由这个工厂函数来决定
	* 
	* @return bool 创建成功返回true,否则返回false
	*/
	bool CreateOneService(ServiceIdType service_id,SessionCreatorSharedPtr session_creator);

private:
	bool CreateMonitorThreadAndStart();
	bool CreateAllNetThreadAllocate();

	NetThreadAllocateMgrSharedPtr GetNetThreadAllocate(uint32_t policy_id);

private:
	ControlMonitor control_monitor_;

	// 执行监控器的线程
	ThreadBaseSharedPtr monitor_thread_;

	// 线程分配策略
  std::map<uint32_t, NetThreadAllocateMgrSharedPtr> net_thread_allocate_map_;
};

NAMESPACE_EVEREST_END

#endif // !LAUNCHER_BASE_H_

