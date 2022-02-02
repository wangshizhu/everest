#ifndef THREAD_ALLOCATE_CONFIGURATION_H_
#define THREAD_ALLOCATE_CONFIGURATION_H_

NAMESPACE_EVEREST_BEGIN

// ���ڸ����߳�ģ�ͣ����Բο���ƪ����https://zhuanlan.zhihu.com/p/95662364
enum class NetThreadMode
{
	/// <summary>
	/// reactor���߳�ģʽ
	/// ���磨���������ӡ������ӣ���ʹ����ͬ�߳�
	/// </summary>
	kReactorSingleThreadMode = 1,

	/// <summary>
	/// reactor���߳�ģʽ
	/// ���������ӵ��߳�ģʽ�����м��������Ӳ�����ʹ����ͬ�߳�
	/// ������ʹ�ö��߳�ģʽ�����������Ӵ��̳߳��з����߳�
	/// </summary>
	kReactorMultiThreadMode = 2,
};

class ThreadAllocateConfiguration
{
public:
	// ����id
	uint32_t policy_id_ = 0;

	// �����߳�ģ��
	NetThreadMode net_io_mode_ = NetThreadMode::kReactorMultiThreadMode;

	// ����session���̳߳ش�С
	std::size_t session_thread_pool_size_ = 2;
};

NAMESPACE_EVEREST_END

#endif // !THREAD_ALLOCATE_CONFIGURATION_H_

