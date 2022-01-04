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
	// �߳�����
	virtual const char* Name()const;

	// �߳�����
	virtual ThreadType GetThreadType() const;

	// ��ʼ���߳�
	virtual bool Init();

	// �̸߳���
	virtual void Update();

	// �߳�����
	virtual void OnStart();

public:
	/*
	* @brief �����̵߳Ĺ��������������̱߳�����ô˺������������ʧ��
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
	// ���Ӷ�ʱ�����������Ϊ���������߳�Update
	void AddTimerTaskToActuateUpdate();

private:
	// �߳�ִ�л���
	asio::io_context context_;

	// �̱߳��
	uint32_t index_;

	// �߳�Update��ʱ��
	asio::steady_timer run_timer_;

	// ���߳�ʱ��
	Clock clock_;

	// update���¼��ʱ��
	std::chrono::steady_clock::duration interval_;

	// δ������������
	std::atomic_size_t pending_num_;

	// �߳�start ��ʶ
	std::atomic_flag  started_;
	bool started_flag_;

	// ����updateִ�����ʱ��
	uint64_t execute_once_max_time_;

	std::thread thread_;
};

NAMESPACE_END

#endif
