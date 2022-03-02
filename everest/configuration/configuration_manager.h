#ifndef CONFIGURATION_MANAGER_H_
#define CONFIGURATION_MANAGER_H_

NAMESPACE_EVEREST_BEGIN

class ConfigurationManager : private everest::NonCopyable
{
public:
	ConfigurationManager();

public:

	/*
	* @brief:根据服务id获取服务的配置
	* @return:服务配置数据
	*/
	const ServiceConfig* GetServiceConfig(ServiceIdType service_id) const;

	/*
	* @brief:添加服务配置
	* @param: [in] service_config 配置数据
	*/
	void AddServiceConfig(const ServiceConfig& service_config);

	/*
	* @brief: 检查服务配置数据是否正确
	* @return: bool true:配置正确 false:配置错误
	*/
	bool CheckServiceConfig()const;

	/*
	* @brief:获取共享的网络线程分配策略配置，即多个服务使用相同的网络线程分配策略
	* @param: [in] policy_id: 网络线程分配策略id
	* @return:网络线程分配策略配置数据指针，如果为nullptr，说明每个服务都使用自己的网络线程分配策略
	*/
	const ThreadAllocateConfiguration* GetNetThreadAllocateConfig(uint32_t policy_id) const;

	/*
	* @brief:添加线程分配策略配置
	* @param: [in] config 配置数据
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

