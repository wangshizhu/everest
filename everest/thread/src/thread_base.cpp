#include "common/include.h"

NAMESPACE_BEGIN

ThreadBase::ThreadBase(uint32_t index)
	:index_(index), run_timer_(context_), start_(false)
{
}

ThreadBase::~ThreadBase()
{
}

const char* ThreadBase::Name()
{
	return "ThreadBase";
}

bool ThreadBase::Init()
{
	return true;
}

void ThreadBase::Start()
{
	// �߳�ʱ������
	clock_.Restart();

	// ����ʱ������Ϊ�߳�ʱ��
	SetThreadLocalClock(&clock_);

	// ����һ����ʱ����
	AddTimerTaskToActuateUpdate();

	// �����߳�
	std::thread t([context = &context_]() {context->run(); });
	thread_.swap(t);

	start_ = true;
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

template<class T>
void ThreadBase::Post(T&& t)
{
	context_.post(std::forward<T>(t));
}

template<class T>
void ThreadBase::Dispatch(T&& t)
{
	context_.dispatch(std::forward<T>(t));
}

void ThreadBase::AddTimerTaskToActuateUpdate()
{
	run_timer_.expires_after(std::chrono::milliseconds(1000));
	run_timer_.async_wait(
		[this](const asio::error_code& code) 
		{ 
			if (code)
			{
				// TODO: ������־
				return;
			}

			// �ٴε���AddTimerTaskToActuateUpdate��Ϊ�˲�������Update
			this->AddTimerTaskToActuateUpdate();

			// �����߳�ʱ��
			this->clock_.TickTock();

			this->Update();
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

NAMESPACE_END