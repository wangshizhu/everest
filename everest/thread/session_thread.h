#ifndef SESSION_THREAD_H_
#define SESSION_THREAD_H_

NAMESPACE_EVEREST_BEGIN

class SessionThread : public everest::ThreadBase
{
public:
	template<typename... Args>
	SessionThread(Args&&... args) :ThreadBase(std::forward<Args>(args)...)
	{
	}

public:
	// �߳�����
	virtual const char* Name()const override;

	// �߳�����
	virtual ThreadType GetThreadType() const;

	// ��ʼ���߳�
	virtual bool Init() override;

	// �̸߳���
	virtual void Update() override;

	// �߳��������¼�
	virtual void OnStart() override;
};

NAMESPACE_EVEREST_END

#endif // !SESSION_THREAD_H_

