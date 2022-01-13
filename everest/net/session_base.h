#ifndef SESSION_BASE_H_
#define SESSION_BASE_H_

NAMESPACE_EVEREST_BEGIN

class SessionBase : public everest::NonCopyable
{
	struct PrivateFlag {};
public:
	SessionBase(asio::io_context& io_context,SessionDirection session_direction,const PrivateFlag& placehold);
	virtual ~SessionBase();

public:
	/*
	* @brief �������ӵĹ����������������ӱ�����ô˺������������ʧ��
	*/
	template<class SessionType, typename... Args>
	static std::shared_ptr<SessionType> CreateSession(Args&&... args)
	{
		auto ptr = std::make_shared<SessionType>(std::forward<Args>(args)..., PrivateFlag());

		return ptr;
	}

public:
	everest_tcp::socket& GetSocket();

	void GetSessionId()const;

public:
	virtual SessionType GetSessionType() = 0;

private:
	SessionDirection session_direction_;
	everest_tcp::endpoint local_;
	everest_tcp::endpoint remote_;
	everest_tcp::socket socket_;
};

NAMESPACE_EVEREST_END

#endif
