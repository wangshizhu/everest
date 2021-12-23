//#include "common/include.h"

NAMESPACE_BEGIN

ThreadBase::ThreadBase()
	:index_(0), run_timer_(context_), interval_(THREAD_UPDATE_INTERVAL),
	pending_num_(0)
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

	// 线程时钟重置
	clock_.Restart();

	// 将此时钟设置为线程时钟
	SetThreadLocalClock(&clock_);

	// 增加一个定时任务
	AddTimerTaskToActuateUpdate();

	// 创建线程
	std::thread t([context = &context_]() {context->run(); });
	thread_.swap(t);
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
	EVEREST_LOG_INFO("name:{}", FullName().c_str());
	//std::cout << "name:" << FullName().c_str() << std::endl;
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
					FullName().c_str(),int32_t(code.value()),code.message());

				return;
			}

			// 再次调用AddTimerTaskToActuateUpdate，为了不断驱动Update
			this->AddTimerTaskToActuateUpdate();

			// 拨动线程时钟
			this->clock_.TickTock();

			TRY_MACRO

			this->Update();

			CATCH_MACRO
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

asio::io_context& ThreadBase::GetIoContext()
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

NAMESPACE_END