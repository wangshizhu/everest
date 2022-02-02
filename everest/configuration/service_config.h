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

	// 网络线程分配策略id
	uint32_t net_thread_allocate_policy_id_ = 0;

};

NAMESPACE_EVEREST_END

#endif // !SERVICE_CONFIG_H_

