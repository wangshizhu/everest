#ifndef THREAD_DEFINE_H_
#define THREAD_DEFINE_H_

enum class ThreadType
{
	kInvalid = 0,
	kCommon,				// �����߳�����
	kMonitor,				// ���м�����߳�
	kNetListenerConnector,	// ���ܼ��������ӵ������߳�
	kNetSession,			// ���������ӵ��߳�
};

#endif