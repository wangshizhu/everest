#ifndef SESSION_BASE_H_
#define SESSION_BASE_H_

NAMESPACE_EVEREST_BEGIN

class SessionBase : public everest::NonCopyable
{
	struct PrivateFlag {};
public:
	SessionBase(SessionDirection session_direction,const PrivateFlag& placehold);
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
	virtual SessionType GetSessionType() = 0;

private:
	SessionDirection session_direction_;
	asio::ip::tcp::endpoint local_;
	asio::ip::tcp::endpoint remote_;
};

NAMESPACE_EVEREST_END

#endif
