#ifndef CONFIGURATION_MANAGER_H_
#define CONFIGURATION_MANAGER_H_

NAMESPACE_EVEREST_BEGIN

class ConfigurationManager : private everest::NonCopyable
{
public:
	ConfigurationManager();

public:

	/*
	* @brief:���ݷ���id��ȡ���������
	* @return:������������
	*/
	const ServiceConfig* GetServiceConfig(ServiceIdType service_id) const;

	/*
	* @brief:��ӷ�������
	* @param: [in] service_config ��������
	*/
	void AddServiceConfig(const ServiceConfig& service_config);

	/*
	* @brief: ���������������Ƿ���ȷ
	* @return: bool true:������ȷ false:���ô���
	*/
	bool CheckServiceConfig()const;

	/*
	* @brief:��ȡ����������̷߳���������ã����������ʹ����ͬ�������̷߳������
	* @param: [in] policy_id: �����̷߳������id
	* @return:�����̷߳��������������ָ�룬���Ϊnullptr��˵��ÿ������ʹ���Լ��������̷߳������
	*/
	const ThreadAllocateConfiguration* GetNetThreadAllocateConfig(uint32_t policy_id) const;

	/*
	* @brief:����̷߳����������
	* @param: [in] config ��������
	*/
	void AddThreadAllocateConfig(const ThreadAllocateConfiguration& config);

	const std::map<uint32_t, ThreadAllocateConfiguration>& GetAllNetThreadAllocateConfig()const;

private:
	std::map<ServiceIdType, ServiceConfig>  service_config_;

	std::map<uint32_t, ThreadAllocateConfiguration> net_thread_allocate_config_;
};

#define CONFIGURATION_SINGLETON() everest::ThreadSafeSingleton<everest::ConfigurationManager>::GetInstance()

NAMESPACE_EVEREST_END

#endif // !CONFIGURATION_MANAGER_H_

