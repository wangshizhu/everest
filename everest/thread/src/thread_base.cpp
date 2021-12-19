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
	clock_.Restart();
	SetThreadLocalClock(&clock_);

	std::thread t([context = &context_]() {context->run(); });
	thread_.swap(t);

	start_ = true;

	run_timer_.expires_after(std::chrono::milliseconds(1000));
	run_timer_.async_wait([self = this](const asio::error_code&) { self->Update(); });
}

void ThreadBase::Join()
{
	thread_.join();
}

void ThreadBase::Stop()
{
	Post([&] {run_timer_.cancel(); });
}

void ThreadBase::Update()
{
	run_timer_.expires_after(std::chrono::milliseconds(1000));
	run_timer_.async_wait([self = this](const asio::error_code&) { self->Update(); });

	clock_.TickTock();
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

NAMESPACE_END