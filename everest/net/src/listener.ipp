NAMESPACE_EVEREST_BEGIN

template<class SessionType>
Listener<SessionType>::Listener(asio::io_context& io_context) 
	:io_context_(io_context), acceptor_(io_context, everest_tcp::endpoint(everest_tcp::v4(),9527))
{
}

template<class SessionType>
void Listener<SessionType>::StartAccept()
{
	auto&& session = SessionBase::CreateSession<SessionType>(io_context_,SessionDirection::kNegative);

	acceptor_.async_accept(session->GetSocket(), 
		std::bind(&Listener<SessionType>::HandleAccept, this, session, std::placeholders::_1));
}

template<class SessionType>
void Listener<SessionType>::HandleAccept(std::shared_ptr<SessionType> new_session, const asio::error_code& error)
{
	if (!error)
	{
		EVEREST_LOG_INFO("Listener::HandleAccept");
		//TODO:
	}
	else
	{
		EVEREST_LOG_ERROR("Listener::HandleAccept error,msg:{}", error.message());
	}

	StartAccept();
}


NAMESPACE_EVEREST_END