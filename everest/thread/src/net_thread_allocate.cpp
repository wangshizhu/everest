NAMESPACE_EVEREST_BEGIN

ServiceNetThreadManager::ServiceNetThreadManager(uint32_t policy_id)
	:policy_id_(policy_id),
	reactor_single_thread_(nullptr),
	listener_connector_thread_(nullptr),
	session_thread_pool_(nullptr)
{
}

ServiceNetThreadManager::~ServiceNetThreadManager()
{
}

void ServiceNetThreadManager::Stop()
{
	listener_connector_thread_->Stop();
	listener_connector_thread_->Join();

	session_thread_pool_->StopAll();
	session_thread_pool_->JoinAll();
}

bool ServiceNetThreadManager::CreateAndStartNetThreadByConfig()
{
  auto config = CONFIGURATION_SINGLETON()->GetNetThreadAllocateConfig(policy_id_);
  if (nullptr == config)
  {
    EVEREST_LOG_ERROR("ServiceNetThreadManager::CreateAndStartNetThreadByConfig dont find service config;policy_id:{}",
											policy_id_);

    return false;
  }

	if (config->net_io_mode_ == NetThreadMode::kReactorSingleThreadMode)
	{
		reactor_single_thread_ = everest::ThreadBase::CreateThread<everest::ThreadBase>();
		listener_connector_thread_ = reactor_single_thread_;

		reactor_single_thread_->Start();

		return true;
	}

	session_thread_pool_ = std::make_unique<SessionThreadPool>(config->session_thread_pool_size_);
	if (!session_thread_pool_->Init())
	{
		EVEREST_LOG_ERROR("ServiceNetThreadManager::CreateAndStartNetThreadByConfig init session_thread_pool faild;policy_id:{}",
											policy_id_);

		return false;
	}

	session_thread_pool_->StartAll();

	listener_connector_thread_ = everest::ThreadBase::CreateThread<everest::ListenerConnectorThread>();
	listener_connector_thread_->Start();

	return true;
}

ThreadBaseSharedPtr ServiceNetThreadManager::GetListenerConnectorThread()
{
	return listener_connector_thread_;
}

ThreadBaseSharedPtr ServiceNetThreadManager::AllocateSessionThread()
{
	auto config = CONFIGURATION_SINGLETON()->GetNetThreadAllocateConfig(policy_id_);
	if (config->net_io_mode_ == NetThreadMode::kReactorSingleThreadMode)
	{
		return reactor_single_thread_;
	}

	return session_thread_pool_->GetAnyThread();
}

NAMESPACE_EVEREST_END