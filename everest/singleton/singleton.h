#ifndef SINGLETON_H_
#define SINGLETON_H_

NAMESPACE_BEGIN

template <class T>
class ThreadUnsafeSingleton : public everest::NonCopyable
{
public:
	ThreadUnsafeSingleton()
	{
		assert(!m_pT);
		m_pT = static_cast<T*>(this);
	}
	virtual ~ThreadUnsafeSingleton()
	{
		assert(m_pT);
		m_pT = nullptr;
	}

public:
	static T* GetInstancePtr()
	{
		return m_pT;
	}

private:
	static T* m_pT;
};

template <class T> T* ThreadUnsafeSingleton<T>::m_pT = nullptr;

template <class T>
class ThreadSafeSingleton : public everest::NonCopyable
{
public:
	static T* GetInstance()
	{
		static T instance;
		return &instance;
	}

	~ThreadSafeSingleton() = default;

private:
	ThreadSafeSingleton() {}
};

NAMESPACE_END

#endif // !SINGLETON_H_

