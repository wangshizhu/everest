NAMESPACE_EVEREST_BEGIN

template<class SessionType>
Listener<SessionType>::Listener(ServiceIdType service_id,
								asio::io_context& io_context, 
								const PrivateFlag&)
	: io_context_(io_context), 
	acceptor_(io_context),
	service_id_(service_id)
{
}

template<class SessionType>
bool Listener<SessionType>::StartListen()
{
	auto&& ep = MakeEndpoint();
	if (!ep)
	{
		return false;
	}
	if (!Open(ep.value()))
	{
		return false;
	}

	// 绑定
	if (!Bind(ep.value()))
	{
		return false;
	}

	// 设置socket option
	if (!SetSocketOption())
	{
		return false;
	}

	// 监听
	if (!Listen())
	{
		return false;
	}

	io_context_.post([this] {this->StartAccept(); });

	return true;
}

template<class SessionType>
bool Listener<SessionType>::Open(const everest_tcp::endpoint& ep)
{
	asio::error_code ec;

	acceptor_.open(ep.protocol(), ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("Listener<SessionType>::Open,accepter open error:{},data:{}",
						  ec.message(), CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_).FormatForLog());
		return false;
	}

	return true;
}

template<class SessionType>
bool Listener<SessionType>::Bind(const everest_tcp::endpoint& ep)
{
	asio::error_code ec;

	// 绑定
	acceptor_.bind(ep, ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("Listener<SessionType>::Bind,bind error:{},data:{}",
						  ec.message(), CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_).FormatForLog());
		return false;
	}

	return true;
}

template<class SessionType>
bool Listener<SessionType>::SetSocketOption()
{
	auto service_config = CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_);
	if (!service_config->reuse_addr_)
	{
		return true;
	}

	asio::error_code ec;

	// 设置socket option
	acceptor_.set_option(everest_tcp::acceptor::reuse_address(true), ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("Listener<SessionType>::SetSocketOption,set_option error:{},data:{}",
						  ec.message(), service_config->FormatForLog());
		return false;
	}

	return true;
}

template<class SessionType>
bool Listener<SessionType>::Listen()
{
	asio::error_code ec;

	auto service_config = CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_);
	acceptor_.listen(service_config->backlog_, ec);
	if (ec)
	{
		EVEREST_LOG_ERROR("Listener<SessionType>::Listen, error:{},data:{}",
						  ec.message(), service_config->FormatForLog());
		return false;
	}

	return true;
}

template<class SessionType>
std::optional<everest_tcp::endpoint> Listener<SessionType>::MakeEndpoint()
{
	everest_tcp::endpoint ep;

	auto service_config = CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_);
	// 设置端口
	ep.port(service_config->port_);

	asio::error_code ec;

	// 设置协议
	if (service_config->ipv4_)
	{
		ep.address(asio::ip::make_address_v4(service_config->ip_,ec));
	}
	else
	{
		ep.address(asio::ip::make_address_v6(service_config->ip_, ec));
	}

	if (ec)
	{
		EVEREST_LOG_ERROR("Listener<SessionType>::MakeEndpoint error:{},data:{}",
						  ec.message(), service_config->FormatForLog());

		return std::nullopt;
	}

	return std::make_optional<everest_tcp::endpoint>(ep);
}

template<class SessionType>
void Listener<SessionType>::StartAccept()
{
	acceptor_.async_accept(NET_THREAD_ALLOCATE_SINGLETON()->AllocateSessionThread()->GetIoContextRef(),
						   std::bind(&Listener<SessionType>::HandleAccept, this,
									 std::placeholders::_1,std::placeholders::_2));
}

template<class SessionType>
void Listener<SessionType>::HandleAccept(const asio::error_code& error, everest_tcp::socket new_socket)
{
	if (error)
	{
		EVEREST_LOG_ERROR("Listener::HandleAccept error,msg:{}", error.message());
	}
	else
	{
		EVEREST_LOG_INFO("Listener::HandleAccept");

		auto&& session = SessionBase::CreateSession<SessionType>(SessionDirection::kNegative);
		session->SetSocket(std::move(new_socket));
	}

	StartAccept();
}


NAMESPACE_EVEREST_END