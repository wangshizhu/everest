NAMESPACE_EVEREST_BEGIN

SessionBase::SessionBase(asio::io_context& io_context,SessionDirection session_direction,const PrivateFlag& placehold)
	:session_direction_(session_direction), socket_(io_context)
{
}

SessionBase::~SessionBase()
{
}

everest_tcp::socket& SessionBase::GetSocket()
{
	return socket_;
}

//void SessionBase::GetSessionId()const
//{
//	return socket_.
//}

NAMESPACE_EVEREST_END