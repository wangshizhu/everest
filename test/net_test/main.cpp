#include "common/include.h"

class SelfSession : public everest::SessionBase
{
public:
  template<typename... Args>
  SelfSession(Args&&... args) :SessionBase(std::forward<Args>(args)...)
  {
  }
public:
  everest::SessionType GetSessionType() override
  {
    return everest::SessionType::kExternal;
  }

};

class SelfSessionCreator : public everest::SessionCreator
{
public:
  std::shared_ptr<everest::SessionBase> CreateSession() override
  {
    return everest::SessionBase::CreateSession<SelfSession>();
  }
};

class NetLauncher : public everest::LauncherBase
{
public:
  void OnLoadServiceConfig()override
  {
    everest::ServiceConfig sc;
    sc.service_id_ = 1;
    sc.type_ = everest::ListenConnectType::kListen;
    sc.ip_ = "0.0.0.0";
    sc.port_ = 9527;
    sc.net_thread_allocate_policy_id_ = 1;

    CONFIGURATION_SINGLETON()->AddServiceConfig(sc);

    everest::ThreadAllocateConfiguration tac;
    tac.policy_id_ = 1;

    CONFIGURATION_SINGLETON()->AddThreadAllocateConfig(tac);
  }

  const char* Name()const override
  {
    return "NetLauncher";
  }

  void OnStartLauncher() override
  {
    CreateOneService(1, std::make_shared<SelfSessionCreator>());
  }

};

int main(int argc,char** argv)
{
  {
    NetLauncher nl;
    if (!nl.StartLauncher())
    {
      return 0;
    }

    auto tb = everest::ThreadBase::CreateThread<everest::ThreadBase>();
    tb->SetUpdateInterval(std::chrono::milliseconds(10));

    tb->Start();

    tb->Join();
  }

  system("pause");
  return 0;
}