#ifndef LISTENER_H_
#define LISTENER_H_

NAMESPACE_EVEREST_BEGIN

template<class SessionType>
class Listener final : public everest::NonCopyable
{
public:
	Listener(asio::io_context& io_context);

public:
	void StartAccept();

private:
	void HandleAccept(std::shared_ptr<SessionType> new_session,const asio::error_code& error);

private:
	asio::io_context& io_context_;
	everest_tcp::acceptor acceptor_;
};

NAMESPACE_EVEREST_END

#include "net/src/listener.ipp"

#endif // !LISTENER_H_
