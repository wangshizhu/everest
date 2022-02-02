#ifndef LISTEN_CONFIGURATION_H_
#define LISTEN_CONFIGURATION_H_

NAMESPACE_EVEREST_BEGIN

class ListenConfiguration
{
public:
	std::string FormatForLog() const
	{
		return fmt::format("ListenConfiguration,ipv4:{},ip:{},port:{},no_delay:{},reuse_addr:{},backlog:{}",
			ipv4_,ip_,port_,no_delay_,reuse_addr_,backlog_);
	}

public:
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
};

NAMESPACE_EVEREST_END

#endif // !LISTEN_CONFIGURATION_H_

