#ifndef THREAD_BASE_H_
#define THREAD_BASE_H_

NAMESPACE_BEGIN

class ThreadBase : public everest::NonCopyable
{
	struct PrivateFlag {};

public:
	ThreadBase(const PrivateFlag& placehold);

	virtual ~ThreadBase();

public:
	// 线程名称
	virtual const char* Name()const;

	// 线程类型
	virtual ThreadType GetThreadType() const;

	// 初始化线程
	virtual bool Init();

	// 线程更新
	virtual void Update();

	// 线程启动
	virtual void OnStart();

public:
	/*
	* @brief 创建线程的工厂函数，创建线程必须调用此函数，否则编译失败
	*/
	template<class ThreadType,typename... Args>
	static std::shared_ptr<ThreadType> CreateThread(Args&&... args)
	{
		auto ptr = std::make_shared<ThreadType>(std::forward<Args>(args)...,PrivateFlag());

		CONTROL_MONITOR_SINGLETON()->RegisterThread(ptr);

		return ptr;
	}

public:
	void Start();

	void Join();

	void Stop();

	template<class T>
	void Post(T&& cb)
	{
		pending_num_++;
		context_.post([this,task = std::forward<T>(cb)]()mutable
			{
				pending_num_--;
				
				TRY_MACRO
				task();
				CATCH_MACRO
			});
	}

	template<class T>
	void Dispatch(T&& cb)
	{
		pending_num_++;
		context_.dispatch([this,task = std::forward<T>(cb)]()mutable
			{
				pending_num_--;
				
				TRY_MACRO
				task();
				CATCH_MACRO
			});
	}

	void SetThreadIndex(uint32_t index);

	template<class Duration = std::chrono::microseconds>
	void SetUpdateInterval(Duration&& d)
	{
		interval_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(d);
	}

	void Snapshot(std::shared_ptr<ThreadBase> to, SnapshotCb&& cb);

	bool IsSameThread()const;

	bool IsStopped()const;

	asio::io_context* GetIoContext();

	std::size_t PendingNum()const;

	std::string FullName()const;

	ThreadIdType ThisThreadId() const;

private:
	// 增加定时任务，这个任务为了驱动本线程Update
	void AddTimerTaskToActuateUpdate();

private:
	// 线程执行环境
	asio::io_context context_;

	// 线程编号
	uint32_t index_;

	// 线程Update定时器
	asio::steady_timer run_timer_;

	// 本线程时钟
	Clock clock_;

	// update更新间隔时间
	std::chrono::steady_clock::duration interval_;

	// 未处理任务数量
	std::atomic_size_t pending_num_;

	// 线程start 标识
	std::atomic_flag  started_;
	bool started_flag_;

	// 单次update执行最大时间
	uint64_t execute_once_max_time_;

	std::thread thread_;
};

NAMESPACE_END

#endif
