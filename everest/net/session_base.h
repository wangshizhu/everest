#ifndef SESSION_BASE_H_
#define SESSION_BASE_H_

NAMESPACE_EVEREST_BEGIN

class SessionBase : public std::enable_shared_from_this<SessionBase>, private everest::NonCopyable
{
	struct PrivateFlag {};
public:
	SessionBase();
	SessionBase(const PrivateFlag& placehold);
	virtual ~SessionBase();

public:
	/*
	* @brief �������ӵĹ����������������ӱ�����ô˺������������ʧ��
	*/
	template<class SessionType, typename... Args>
	static std::shared_ptr<SessionType> CreateSession(Args&&... args)
	{
		auto ptr = std::make_shared<SessionType>(std::forward<Args>(args)..., PrivateFlag());
		// TODO: ��������ı���

		return ptr;
	}

public:
	void SetSocket(everest_tcp::socket&& new_socket);
	everest_tcp::socket& GetSocket();

	void SetSessionDirection(SessionDirection session_direction);

	void SetSessionId(const SessionId& session_id);
	SessionIdType GetSessionId()const;
	ServiceIdType GetServiceId() const;

	void Read();
	void Write(UniqueExpandBuffer packet_buffer);

	void Close();
	bool Closed() const;

public:
	virtual SessionType GetSessionType() = 0;

protected:
	virtual bool OnRead() = 0;
	virtual void OnClose() = 0;

private:
	void SetLocalEndpoint();
	void SetRemoteEndpoint();

	void DoRead();
	void DoWrite();
	void DoClose();

private:
	SessionDirection session_direction_;
	everest_tcp::endpoint local_;
	everest_tcp::endpoint remote_;
	std::unique_ptr<everest_tcp::socket> socket_;
	SessionId session_id_;
	ExpandBuffer read_data_;
	ExpandBuffer send_data_;
	bool closed_;

	// �����һ�δ������ж�ȡʱ�䣨��λ���룩
	TimeStampType lastest_read_time_;
};

NAMESPACE_EVEREST_END

#endif
