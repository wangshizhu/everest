/*
* 禁止拷贝实现
* 禁止拷贝的自定义类型需要private 继承此类
* 例如：class Test : private NonCopyable
*/

#ifndef NON_COPYABLE_H_
#define NON_COPYABLE_H_

NAMESPACE_EVEREST_BEGIN

class NonCopyable
{
protected:
	NonCopyable()
	{
	}

	NonCopyable(NonCopyable&&) = delete;
	NonCopyable& operator=(NonCopyable&&) = delete;

	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

NAMESPACE_EVEREST_END

#endif
