#ifndef SESSION_THREAD_POOL_H_
#define SESSION_THREAD_POOL_H_

NAMESPACE_EVEREST_BEGIN

class SessionThreadPool final : public everest::ThreadPool<SessionThread>
{
public:
	SessionThreadPool(std::size_t pool_size);

public:
	const char* Name()const override;

	bool Init() override;

};

NAMESPACE_EVEREST_END

#endif // !SESSION_THREAD_POOL_H_
