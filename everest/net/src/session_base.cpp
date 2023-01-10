NAMESPACE_EVEREST_BEGIN

SessionBase::SessionBase() 
	:session_direction_(SessionDirection::kNone), socket_(nullptr), session_id_(0), 
	closed_(false), lastest_read_time_(0)
{
}

SessionBase::SessionBase(const PrivateFlag& placehold)
	:session_direction_(SessionDirection::kNone), socket_(nullptr), session_id_(0),
	closed_(false), lastest_read_time_(0)
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

void SessionBase::SetSessionId(const SessionId& session_id)
{
	session_id_ = session_id;
}

void SessionBase::Read()
{
	asio::post(socket_->get_executor(), [self = shared_from_this()]()
	{
		self->DoRead();
	});
}

void SessionBase::DoRead()
{
	if (read_data_.Full())
	{
		EVEREST_LOG_ERROR("SessionBase::DoRead read buffer catch max");

		DoClose();

		return;
	}

	socket_->async_read_some(asio::buffer(read_data_.GetWritePos(),read_data_.CanWriteSize()),
													 [this,self = shared_from_this()](const asio::error_code& error, std::size_t bytes_transferred)
	{
		if (Closed())
		{
			EVEREST_LOG_ERROR("SessionBase::DoRead try to read from a closed socket");

			return;
		}

		if (error)
		{
			EVEREST_LOG_ERROR("SessionBase::DoRead read error code {},error msg {}",error.value(), error.message());

			// 关闭
			DoClose();

			return;
		}

		// 读取
		if (!OnRead())
		{
			return DoClose();
		}

		read_data_.MoveWritePos(bytes_transferred);

		lastest_read_time_ = TimeCapsule::Now();

		EVEREST_LOG_DEBUG("SessionBase::DoRead can write size {}",read_data_.CanWriteSize());

		// 继续从网络中读取
		DoRead();
	});
}

void SessionBase::Write(UniqueExpandBuffer packet_buffer)
{
  asio::post(socket_->get_executor(), [this,packet_buffer = std::move(packet_buffer),self = shared_from_this()]()
  {
		if (!send_data_.ExpandWrite(packet_buffer->GetReadPos(),packet_buffer->Size()))
		{
			EVEREST_LOG_ERROR("SessionBase::Write send buffer is max session_id:{}",GetSessionId());

			DoClose();

			return;
		}

		DoWrite();
  });
}

void SessionBase::DoWrite()
{
	if (!socket_->is_open())
	{
		return;
	}

	if (Closed())
	{
		return;
	}

	if (send_data_.Empty())
	{
		return;
	}

	socket_->async_send(asio::buffer(send_data_.GetReadPos(), send_data_.Size()),
											[this,self = shared_from_this()](const asio::error_code& error, std::size_t bytes_transferred)
	{
		if (Closed())
		{
			EVEREST_LOG_ERROR("SessionBase::DoWrite socket is closed");

			return;
		}

		if (error)
		{
			EVEREST_LOG_ERROR("SessionBase::DoWrite write error code {},error msg {}", error.value(), error.message());

			DoClose();
			return;
		}

		send_data_.MoveReadPos(bytes_transferred);

		EVEREST_LOG_DEBUG("SessionBase::DoWrite wait send size {}", send_data_.Size());

		DoWrite();
	});
}

void SessionBase::Close()
{
	asio::post(socket_->get_executor(), [self = shared_from_this()]()
	{
		self->DoClose();
	});
}

void SessionBase::DoClose()
{
	EVEREST_LOG_INFO("SessionBase::DoClose closed:{}", closed_);

	if (Closed())
	{
		return;
	}

	if (!socket_->is_open())
	{
		return;
	}

	socket_->close();

	closed_ = true;

	// 通知service移除这个session
	g_control_monitor->GetServiceMonitor().UnregisterSession(GetServiceId(), GetSessionId());

	OnClose();
}

bool SessionBase::Closed() const
{
	return closed_;
}

SessionIdType SessionBase::GetSessionId()const
{
	return session_id_.UnionSessionId();
}

ServiceIdType SessionBase::GetServiceId() const
{
	return session_id_.GetServiceId();
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

	EVEREST_LOG_INFO("SessionBase::SetLocalEndpoint local_address:{},local_port:{}", 
									 local_.address().to_string(), local_.port());
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

	EVEREST_LOG_INFO("SessionBase::SetRemoteEndpoint remote_address:{},remote_port:{}",
									 remote_.address().to_string(), remote_.port());
}

NAMESPACE_EVEREST_END