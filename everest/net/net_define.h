#ifndef NET_DEFINE_H_
#define NET_DEFINE_H_

NAMESPACE_EVEREST_BEGIN

enum class SessionType
{
	kNone = 0,
	// �ⲿ����
	kExternal = 1,

	// �ڲ�����
	kInternal = 2,
};

enum class SessionDirection
{
	kNone = 0,

	// �������ӣ�ͨ��connect����������
	kPositive = 1,

	// �������ӣ�ͨ��accept����������
	kNegative = 2,
};

NAMESPACE_EVEREST_END

#endif // !NET_DEFINE_H_
