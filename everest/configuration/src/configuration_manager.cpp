NAMESPACE_EVEREST_BEGIN

ConfigurationManager::ConfigurationManager()
{
	service_config_.clear();
	net_thread_allocate_config_.clear();
}

const ServiceConfig* ConfigurationManager::GetServiceConfig(ServiceIdType service_id) const
{
	auto iter = service_config_.find(service_id);
	if (iter == service_config_.cend())
	{
		return nullptr;
	}

	return &iter->second;
}

void ConfigurationManager::AddServiceConfig(const ServiceConfig& service_config)
{
	service_config_[service_config.service_id_] = service_config;
}

bool ConfigurationManager::CheckServiceConfig() const 
{
	for (auto&& [_,service_config] : service_config_)
	{
		auto iter = net_thread_allocate_config_.find(service_config.net_thread_allocate_policy_id_);
		if (iter == net_thread_allocate_config_.cend())
		{
			return false;
		}

		if (service_config.type_ == ListenConnectType::kNone)
		{
			return false;
		}
  }

	for (auto&& [policy_id,_] : net_thread_allocate_config_)
	{
		bool exist = false;
		for (auto&& [_, service_config] : service_config_)
		{
			if (service_config.net_thread_allocate_policy_id_ == policy_id)
			{
				exist = true;
				break;
			}
		}

		if (!exist)
		{
			return false;
		}
	}

	return true;
}

const ThreadAllocateConfiguration* ConfigurationManager::GetNetThreadAllocateConfig(uint32_t policy_id) const
{
  auto iter = net_thread_allocate_config_.find(policy_id);
  if (iter == net_thread_allocate_config_.cend())
  {
	  return nullptr;
  }

  return &iter->second;
}

void ConfigurationManager::AddThreadAllocateConfig(const ThreadAllocateConfiguration& config)
{
	net_thread_allocate_config_[config.policy_id_] = config;
}

const std::map<uint32_t, ThreadAllocateConfiguration>& ConfigurationManager::GetAllNetThreadAllocateConfig()const
{
	return net_thread_allocate_config_;
}

NAMESPACE_EVEREST_END