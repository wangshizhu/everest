NAMESPACE_EVEREST_BEGIN

LauncherBase::LauncherBase()
{
	g_control_monitor = &control_monitor_;
}

LauncherBase::~LauncherBase()
{
	// 停止监控线程
	monitor_thread_->Stop();
	monitor_thread_->Join();

	// 停止所有网络线程
	for (auto&& [_,net_thread_allocate_ptr] : net_thread_allocate_map_)
	{
		net_thread_allocate_ptr->Stop();
	}
}

bool LauncherBase::StartLauncher()
{
	OnLoadServiceConfig();
	// 检查配置是否正确
	if (!CONFIGURATION_SINGLETON()->CheckServiceConfig())
	{
		return false;
	}

	OnStartLauncherBefore();

	// 创建监控线程
	if (!CreateMonitorThreadAndStart())
	{
		return false;
	}

	// 创建所有网络线程分配策略
	if (!CreateAllNetThreadAllocate())
	{
		return false;
	}

	OnStartLauncher();

	return true;
}

bool LauncherBase::CreateOneService(ServiceIdType service_id,SessionCreatorSharedPtr session_creator) 
{ 
	auto&& service_config = CONFIGURATION_SINGLETON()->GetServiceConfig(service_id);
	if (nullptr == service_config)
	{
		return false;
	}

	auto&& ptr = std::make_shared<ServiceBase>(service_id, ServiceBase::ServicePrivateFlag());
	if (service_config->type_ == ListenConnectType::kListen)
	{
		auto&& listener = ptr->CreateListener(session_creator, 
																					net_thread_allocate_map_[service_config->net_thread_allocate_policy_id_]);
		if (nullptr == listener)
		{
			return false;
		}
	} 
	else if (service_config->type_ == ListenConnectType::kConnect)
	{
		// TODO: 创建connector
	} 
	else 
	{
		return false;
	}

	control_monitor_.RegisterService(ptr);

	return true; 
}

void LauncherBase::OnStartLauncherBefore()
{
}

void LauncherBase::OnStartLauncher()
{
}

const char* LauncherBase::Name()const
{
	return "LauncherBase";
}

bool LauncherBase::CreateMonitorThreadAndStart()
{
	monitor_thread_ = MonitorThread::CreateThread<everest::MonitorThread>();
	if (nullptr == monitor_thread_)
	{
		return false;
	}

	monitor_thread_->Start();

	return true;
}

bool LauncherBase::CreateAllNetThreadAllocate()
{
	auto&& config = CONFIGURATION_SINGLETON()->GetAllNetThreadAllocateConfig();
	for (auto&& [policy_id,_] : config)
	{
    auto&& ptr = std::make_shared<ServiceNetThreadManager>(policy_id);
		if (!ptr->CreateAndStartNetThreadByConfig())
		{
			return false;
		}

    net_thread_allocate_map_[policy_id] = ptr;
	}

	return true;
}

NetThreadAllocateMgrSharedPtr LauncherBase::GetNetThreadAllocate(uint32_t policy_id)
{
  auto iter = net_thread_allocate_map_.find(policy_id);
  if (iter == net_thread_allocate_map_.cend())
  {
		return nullptr;
  }

	return iter->second;
}


NAMESPACE_EVEREST_END