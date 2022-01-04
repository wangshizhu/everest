#ifndef THREAD_DEFINE_H_
#define THREAD_DEFINE_H_

enum class ThreadType
{
	kInvalid = 0,
	kCommon,			// 基类线程类型
	kMonitor,			// 运行监控器线程
};

#endif