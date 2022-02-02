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
	static constexpr uint32_t kReadBuffPoolSize = 1024 * 512;
	static constexpr uint32_t kWriteBuffPoolSize = 1024 * 512;
public:
	// ����id
	uint32_t policy_id_ = 0;

	// �����߳�ģ��
	NetThreadMode net_io_mode_ = NetThreadMode::kReactorMultiThreadMode;

	// ����session���̳߳ش�С
	std::size_t session_thread_pool_size_ = 2;

  // �����ӵĽ�����ϢBUFF�ش�С,Ĭ��512K
  uint32_t read_buff_pool_size_ = kReadBuffPoolSize;

  // �����ӵķ�����ϢBUFF�ش�С,Ĭ��512K
  uint32_t write_buff_pool_size_ = kWriteBuffPoolSize;
};

NAMESPACE_EVEREST_END

#endif // !THREAD_ALLOCATE_CONFIGURATION_H_

