#ifndef SERVICE_CONFIG_H_
#define SERVICE_CONFIG_H_

NAMESPACE_EVEREST_BEGIN

enum class ListenConnectType
{
	kNone = 0,
	kListen = 1,
	kConnect = 2,
};

class ServiceConfig
{
public:
	std::string FormatForLog() const
	{
		return fmt::format("ServiceConfig,service_id:{},service_type:{},ipv4:{},ip:{},port:{},no_delay:{},reuse_addr:{},backlog:{}",
			service_id_, type_,ipv4_, ip_, port_, no_delay_, reuse_addr_, backlog_);
	}

public:
	uint32_t service_id_ = 0;

	ListenConnectType type_ = ListenConnectType::kNone;

	// ����Э�飬true:ʹ��ipv4 false:ʹ��ipv6
	bool ipv4_ = true;

	// ������ַ
	std::string ip_ = "";

	// �����˿�
	uint16_t port_ = 0;

	// �Ƿ����Nagle
	bool no_delay_ = true;

	// ��ַ����
	bool reuse_addr_ = true;

	// listen���� backlog
	int32_t backlog_ = 1024;

	// �����̷߳������id
	uint32_t net_thread_allocate_policy_id_ = 0;

};

NAMESPACE_EVEREST_END

#endif // !SERVICE_CONFIG_H_

