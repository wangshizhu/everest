#ifndef NON_COPYABLE_H_
#define NON_COPYABLE_H_

NAMESPACE_EVEREST_BEGIN

class NonCopyable
{
public:
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
