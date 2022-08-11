NAMESPACE_EVEREST_BEGIN

Listener::Listener(ServiceIdType service_id,SessionCreatorSharedPtr session_creator,
                   NetThreadAllocateMgrSharedPtr net_thread_allocate,const PrivateFlag&)
  : io_context_(net_thread_allocate->GetListenerConnectorThread()->GetIoContextRef()),
  acceptor_(io_context_),session_creator_(session_creator),
  net_thread_allocate_(net_thread_allocate),service_id_(service_id),auto_incre_session_id_(0)
{
}

bool Listener::StartListen() 
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

  io_context_.post([this] { this->StartAccept(); });

  return true;
}

bool Listener::Open(const everest_tcp::endpoint& ep) 
{
  asio::error_code ec;

  acceptor_.open(ep.protocol(), ec);
  if (ec) 
  {
    EVEREST_LOG_ERROR("Listener<SessionType>::Open,accepter open error:{},data:{}",
                      ec.message(),CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_)->FormatForLog());
    return false;
  }

  return true;
}

bool Listener::Bind(const everest_tcp::endpoint& ep) 
{
  asio::error_code ec;

  // 绑定
  acceptor_.bind(ep, ec);
  if (ec) 
  {
    EVEREST_LOG_ERROR("Listener<SessionType>::Bind,bind error:{},data:{}",ec.message(),
                      CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_)->FormatForLog());
    return false;
  }

  return true;
}

bool Listener::SetSocketOption() 
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
    EVEREST_LOG_ERROR( "Listener<SessionType>::SetSocketOption,set_option error:{},data:{}",
                      ec.message(), service_config->FormatForLog());
    return false;
  }

  return true;
}

bool Listener::Listen() 
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

std::optional<everest_tcp::endpoint> Listener::MakeEndpoint() 
{
  auto service_config = CONFIGURATION_SINGLETON()->GetServiceConfig(service_id_);

  everest_tcp::endpoint ep;
  // 设置端口
  ep.port(service_config->port_);

  asio::error_code ec;

  // 设置协议
  if (service_config->ipv4_) 
  {
    ep.address(asio::ip::make_address_v4(service_config->ip_, ec));
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

SessionIdType Listener::GenerateSessionId()
{
  if (auto_incre_session_id_ == std::numeric_limits<uint32_t>::max())
  {
    auto_incre_session_id_ = 0;
  }

  ++auto_incre_session_id_;

  std::bitset<sizeof(SessionIdType) * kOneByteBit> tmp;
  tmp |= service_id_;
  tmp <<= 32;
  tmp |= auto_incre_session_id_;

  return tmp.to_ullong();
}

void Listener::StartAccept() 
{
  acceptor_.async_accept(net_thread_allocate_->AllocateSessionThread()->GetIoContextRef(),
                         std::bind(&Listener::HandleAccept, this, std::placeholders::_1, std::placeholders::_2));
}

void Listener::HandleAccept(const asio::error_code& error,everest_tcp::socket new_socket) 
{
  if (error) 
  {
    EVEREST_LOG_ERROR("Listener::HandleAccept error,msg:{}", error.message());
  } 
  else 
  {
    EVEREST_LOG_INFO("Listener::HandleAccept");

    auto&& session = session_creator_->CreateSession();
    session->SetSessionId(GenerateSessionId());
    session->SetSocket(std::move(new_socket));
    session->SetSessionDirection(SessionDirection::kNegative);

    g_control_monitor->GetServiceMonitor().RegisterSession(service_id_, session);
  }

  StartAccept();
}

NAMESPACE_EVEREST_END