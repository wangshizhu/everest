#ifndef SESSION_CONFIGURATION_H_
#define SESSION_CONFIGURATION_H_

NAMESPACE_EVEREST_BEGIN

class SessionConfiguration
{
public:
	// 新连接的接收消息BUFF池大小,默认512K
	uint32_t read_buff_pool_size_ = 1024 * 512;

	// 新连接的发送消息BUFF池大小,默认512K
	uint32_t write_buff_pool_size_ = 1024 * 512;
};

NAMESPACE_EVEREST_END


#endif // !SESSION_CONFIGURATION_H_

