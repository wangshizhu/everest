#ifndef LISTENER_H_
#define LISTENER_H_

NAMESPACE_EVEREST_BEGIN

class Listener final : public everest::NonCopyable
{
	struct PrivateFlag {};
	friend class ServiceBase;

public:
	Listener(ServiceIdType service_id, SessionCreatorSharedPtr session_creator,
					 NetThreadAllocateMgrSharedPtr net_thread_allocate,const PrivateFlag&);

public:
	bool StartListen();

private:
	bool Open(const everest_tcp::endpoint& ep);

	bool Bind(const everest_tcp::endpoint& ep);

	bool SetSocketOption();

	bool Listen();

	void StartAccept();

	// 处理新连接
	void HandleAccept(const asio::error_code& error,everest_tcp::socket new_socket);

	// 构造ip地址和端口
	std::optional<everest_tcp::endpoint> MakeEndpoint();

private:
	asio::io_context& io_context_;

	everest_tcp::acceptor acceptor_;

	SessionCreatorSharedPtr session_creator_;

	NetThreadAllocateMgrSharedPtr net_thread_allocate_;

	ServiceIdType service_id_;
};

using ListenerSharedPtr = std::shared_ptr<everest::Listener>;

NAMESPACE_EVEREST_END

#endif // !LISTENER_H_
