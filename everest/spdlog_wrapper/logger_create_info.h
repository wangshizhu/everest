#ifndef LOGGER_CREATE_INFO_H_
#define LOGGER_CREATE_INFO_H_

#include "spdlog/common.h"

NAMESPACE_BEGIN

struct LoggerCreateInfo
{
	// �Ƿ��첽logger,Ĭ��true
	bool async_logger = true;

	// �Ƿ����̨���,Ĭ��true
	bool to_stdout = true;

	// �Ƿ�ÿ�ղ���log�ļ�,Ĭ��true
	bool daily = true;

	// �Ƿ�����log�ļ���С,Ĭ��true
	bool rotate = true;

	// log�ļ���С����rotateΪtrueʱ����Ч,Ĭ��10M
	std::size_t log_file_size = 1024 * 1024 * 10;

	// �����߳�����,Ĭ��1�������߳�
	std::size_t consumer_thread_num = 1;

	// ��ʹ���첽logʱ���첽log���д�С,Ĭ��8192
	std::size_t async_queue_size = spdlog::details::default_async_q_size;

	// log����
	spdlog::level::level_enum level = spdlog::level::debug;
};

struct CreateInfoFactory
{
	static LoggerCreateInfo info_;
	CreateInfoFactory(const LoggerCreateInfo& info)
	{
		info_ = info;
	}

	static const LoggerCreateInfo& GetLoggerCreateInfo()
	{
		return info_;
	}
};

void SetLoggerCreateInfo(const LoggerCreateInfo& info);

NAMESPACE_END

#endif
