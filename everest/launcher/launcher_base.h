#ifndef LAUNCHER_BASE_H_
#define LAUNCHER_BASE_H_

NAMESPACE_EVEREST_BEGIN

/// <summary>
/// ���̻��࣬�ⲿ����ͨ������StartLauncher()������������
/// ����ᰴ�����µ���˳�������ഫ���¼���
/// 1.OnLoadServiceConfig()
/// 2.OnStartLauncherBefore()
/// 3.OnStartLauncher()
/// </summary>

class LauncherBase : private everest::NonCopyable
{
public:
	LauncherBase();
	virtual ~LauncherBase();

public:
	/*
	* @brief:���̿�ʼ���е�Ψһ����
	*/
	bool StartLauncher();

protected:
	/*
	* @brief: ������Ҫ�����������ݵ�ConfigurationManager�У�Ϊ���洴��listener����connector����׼������������
	*/
	virtual void OnLoadServiceConfig() = 0;

	/*
	* @brief:�����ഫ�ݽ�������֮ǰ���¼�
	*/
	virtual void OnStartLauncherBefore();

	/*
	* @brief:�����ഫ�ݽ�������֮����¼�
	*/
	virtual void OnStartLauncher();

	/*
	* @brief:�������ƣ����磺center��login��gate�ȵ�
	*/
	virtual const char* Name()const = 0;

protected:
	/*
	* @brief:����������麯��OnStartLauncher�ڲ�����һ������
	* ����ֻ��listener����connector
	* 
	* @param [in] service_id ����id���ڲ�����ݷ���idȥConfigurationManager�в����������id��
	* �������ݣ��������ݾ��������������listener����connector
	* 
	* @param [in] session_creator session�Ĺ�����������accept�����ӻ���connect�ɹ�ʱҪ����session,
	* ������ʲôsession���������������������
	* 
	* @return bool �����ɹ�����true,���򷵻�false
	*/
	bool CreateOneService(ServiceIdType service_id,SessionCreatorSharedPtr session_creator);

private:
	bool CreateMonitorThreadAndStart();
	bool CreateAllNetThreadAllocate();

	NetThreadAllocateMgrSharedPtr GetNetThreadAllocate(uint32_t policy_id);

private:
	ControlMonitor control_monitor_;

	// ִ�м�������߳�
	ThreadBaseSharedPtr monitor_thread_;

	// �̷߳������
  std::map<uint32_t, NetThreadAllocateMgrSharedPtr> net_thread_allocate_map_;
};

NAMESPACE_EVEREST_END

#endif // !LAUNCHER_BASE_H_

