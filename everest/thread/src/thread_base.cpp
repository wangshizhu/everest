
NAMESPACE_EVEREST_BEGIN

ThreadBase::ThreadBase(const PrivateFlag& placehold)
	:index_(0), run_timer_(context_), interval_(kThreadUpdateInterval),
	pending_num_(0), started_flag_(false), execute_once_max_time_(0)
{
	started_.clear();
}

ThreadBase::~ThreadBase()
{
}

const char* ThreadBase::Name()const
{
	return "ThreadBase";
}

ThreadType ThreadBase::GetThreadType() const
{
	return ThreadType::kCommon;
}

bool ThreadBase::Init()
{
	return true;
}

void ThreadBase::Start()
{
	if (started_.test_and_set())
	{
		EVEREST_LOG_ERROR("ThreadBase::Start thread started yet,thread_name:{}",FullName().c_str());

		return;
	}

	started_flag_ = true;

	// 线程时钟重置
	clock_.Restart();

	// 将此时钟设置为线程时钟
	Post([this] { SetThreadLocalClock(&(this->clock_)); });
	
	// 增加一个定时任务
	AddTimerTaskToActuateUpdate();

	// 创建线程
	std::thread t([context = &context_]() {context->run(); });
	thread_.swap(t);

	// 向子类传递启动事件
	OnStart();
}

void ThreadBase::Join()
{
	thread_.join();
}

void ThreadBase::Stop()
{
	context_.stop();
}

void ThreadBase::Update()
{
}

void ThreadBase::OnStart()
{
}

void ThreadBase::AddTimerTaskToActuateUpdate()
{
	run_timer_.expires_after(interval_);
	run_timer_.async_wait(
		[this](const asio::error_code& code) 
		{ 
			if (code)
			{
				EVEREST_LOG_ERROR("ThreadBase::AddTimerTaskToActuateUpdate error,thread_name:{},error_code:{},error_message:{}",
													FullName(),int32_t(code.value()),code.message());

				return;
			}

			// 再次调用AddTimerTaskToActuateUpdate，为了不断驱动Update
			this->AddTimerTaskToActuateUpdate();

			// 拨动线程时钟
			this->clock_.TickTock();

			TRY_MACRO

			// 记录更新起始时间
			auto&& begin_time = std::chrono::steady_clock::now();

			// 业务逻辑更新
			this->Update();

			// 记录更新结束时间
			auto&& end_time = std::chrono::steady_clock::now();

			// 更新一次所花费的时间
			execute_once_max_time_ = std::max(execute_once_max_time_, uint64_t((end_time - begin_time).count()));

			CATCH_MACRO
		});
}

void ThreadBase::Snapshot(std::shared_ptr<ThreadBase> to, SnapshotCb&& cb)
{
  Post([to = to, cb = std::move(cb), this]() mutable 
  {
	  ThreadMonitorData data;
	  data.thread_id_ = this->ThisThreadId();
	  data.thread_full_name_ = this->FullName();
	  data.pending_num_ = this->PendingNum();
	  data.interval_ = this->interval_;
	  data.execute_once_max_time_ = this->execute_once_max_time_;
	  data.thread_state_flag_.set(ThreadStateBitFlag::kStartedPos,this->started_flag_);
	  data.thread_state_flag_.set(ThreadStateBitFlag::kStoppedPos,this->IsStopped());

	  to->Post([cb = std::move(cb), data = std::move(data)]() mutable
	  {
		  cb(std::move(data));
	  });
  });
}

void ThreadBase::SetThreadIndex(uint32_t index)
{
	index_ = index;
}

bool ThreadBase::IsSameThread()const
{
	return thread_.get_id() == std::this_thread::get_id();
}

bool ThreadBase::IsStopped()const
{
	return context_.stopped();
}

asio::io_context* ThreadBase::GetIoContext()
{
	return &context_;
}

asio::io_context& ThreadBase::GetIoContextRef()
{
	return context_;
}

std::size_t ThreadBase::PendingNum()const
{
	return pending_num_;
}

std::string ThreadBase::FullName()const
{
	return fmt::format("{}-{}", Name(), index_);
}

ThreadIdType ThreadBase::ThisThreadId() const
{
	std::stringstream tmp;
	tmp << thread_.get_id();

	return atoll(tmp.str().c_str());
}

NAMESPACE_EVEREST_END