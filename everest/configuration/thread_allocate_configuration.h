#ifndef THREAD_ALLOCATE_CONFIGURATION_H_
#define THREAD_ALLOCATE_CONFIGURATION_H_

NAMESPACE_EVEREST_BEGIN

// 关于更多线程模型，可以参考这篇文章https://zhuanlan.zhihu.com/p/95662364
enum class NetThreadMode
{
	/// <summary>
	/// reactor单线程模式
	/// 网络（监听、连接、已连接）都使用相同线程
	/// </summary>
	kReactorSingleThreadMode = 1,

	/// <summary>
	/// reactor多线程模式
	/// 监听、连接单线程模式，所有监听、连接操作都使用相同线程
	/// 已连接使用多线程模式，所有已连接从线程池中分配线程
	/// </summary>
	kReactorMultiThreadMode = 2,
};

class ThreadAllocateConfiguration
{
	static constexpr uint32_t kReadBuffPoolSize = 1024 * 512;
	static constexpr uint32_t kWriteBuffPoolSize = 1024 * 512;
public:
	// 策略id
	uint32_t policy_id_ = 0;

	// 网络线程模型
	NetThreadMode net_io_mode_ = NetThreadMode::kReactorMultiThreadMode;

	// 处理session的线程池大小
	std::size_t session_thread_pool_size_ = 2;

  // 新连接的接收消息BUFF池大小,默认512K
  uint32_t read_buff_pool_size_ = kReadBuffPoolSize;

  // 新连接的发送消息BUFF池大小,默认512K
  uint32_t write_buff_pool_size_ = kWriteBuffPoolSize;
};

NAMESPACE_EVEREST_END

#endif // !THREAD_ALLOCATE_CONFIGURATION_H_

