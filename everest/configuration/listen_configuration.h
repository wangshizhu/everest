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
	// 网络协议，true:使用ipv4 false:使用ipv6
	bool ipv4_ = true;

	// 监听地址
	std::string ip_ = "";

	// 监听端口
	uint16_t port_ = 0;

	// 是否禁用Nagle
	bool no_delay_ = true;

	// 地址重用
	bool reuse_addr_ = true;
	
	// listen参数 backlog
	int32_t backlog_ = 1024;
};

NAMESPACE_EVEREST_END

#endif // !LISTEN_CONFIGURATION_H_

