#ifndef THREAD_DEFINE_H_
#define THREAD_DEFINE_H_

enum class ThreadType
{
	kInvalid = 0,
	kCommon,				// 基类线程类型
	kMonitor,				// 运行监控器线程
	kNetListenerConnector,	// 接受监听和连接的网络线程
	kNetSession,			// 处理已连接的线程
};

#endif