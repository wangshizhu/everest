#ifndef NON_COPYABLE_H_
#define NON_COPYABLE_H_

namespace everest
{
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
}

#endif
