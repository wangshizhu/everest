#ifndef SESSION_CONFIGURATION_H_
#define SESSION_CONFIGURATION_H_

NAMESPACE_EVEREST_BEGIN

class SessionConfiguration
{
public:
	// �����ӵĽ�����ϢBUFF�ش�С,Ĭ��512K
	uint32_t read_buff_pool_size_ = 1024 * 512;

	// �����ӵķ�����ϢBUFF�ش�С,Ĭ��512K
	uint32_t write_buff_pool_size_ = 1024 * 512;
};

NAMESPACE_EVEREST_END


#endif // !SESSION_CONFIGURATION_H_

