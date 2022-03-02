NAMESPACE_EVEREST_BEGIN

SessionBase::SessionBase() 
	:session_direction_(SessionDirection::kNone), socket_(nullptr), session_id_(0)
{
}

SessionBase::SessionBase(const PrivateFlag& placehold)
	:session_direction_(SessionDirection::kNone), socket_(nullptr), session_id_(0)
{
}

SessionBase::~SessionBase()
{
}

void SessionBase::SetSocket(everest_tcp::socket&& new_socket)
{
	if (socket_ != nullptr)
	{
		return;
	}

	socket_ = std::make_unique<everest_tcp::socket>(std::move(new_socket));

	SetLocalEndpoint();
	SetRemoteEndpoint();
}

everest_tcp::socket& SessionBase::GetSocket()
{
	return *socket_;
}

void SessionBase::SetSessionDirection(SessionDirection session_direction)
{
	session_direction_ = session_direction;
}

void SessionBase::SetSessionId(SessionIdType session_id)
{
	session_id_ = session_id;
}

SessionIdType SessionBase::GetSessionId()const
{
	return session_id_;
}

void SessionBase::SetLocalEndpoint()
{
	asio::error_code ec;
	auto&& local = socket_->local_endpoint(ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("SessionBase::SetSocket SetLocalEndpoint, error:{}", ec.message());

		return;
	}
	
	local_ = local;

	EVEREST_LOG_INFO("SessionBase::SetLocalEndpoint local_address:{},local_port:{}", local_.address().to_string(), local_.port());
}

void SessionBase::SetRemoteEndpoint()
{
	asio::error_code ec;
	auto&& remote = socket_->remote_endpoint(ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("SessionBase::SetSocket SetRemoteEndpoint, error:{}", ec.message());

		return;
	}

	remote_ = remote;

	EVEREST_LOG_INFO("SessionBase::SetRemoteEndpoint remote_address:{},remote_port:{}", remote_.address().to_string(), remote_.port());
}

NAMESPACE_EVEREST_END