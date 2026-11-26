/*
* 拷贝/移动语义基类
*
* NonCopyable：既不可拷贝，也不可移动。自定义类型private 继承此类。
* 例如：class Test : private NonCopyable
*
* MoveOnly：不可拷贝，仅可移动。派生类通常无需声明任何特殊成员，
* 隐式移动构造/赋值会自动可用；若派生类声明了析构函数或其他特殊成员，
* 隐式移动不再生成，需显式补上 "= default" 的移动构造/赋值。
* 例如：class Test : private MoveOnly
*/

#ifndef NON_COPYABLE_H_
#define NON_COPYABLE_H_

NAMESPACE_EVEREST_BEGIN

class NonCopyable
{
protected:
	NonCopyable() = default;

	NonCopyable(NonCopyable&&) = delete;
	NonCopyable& operator=(NonCopyable&&) = delete;

	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

class MoveOnly
{
protected:
	MoveOnly() = default;

	MoveOnly(const MoveOnly&) = delete;
	MoveOnly& operator=(const MoveOnly&) = delete;

	MoveOnly(MoveOnly&&) = default;
	MoveOnly& operator=(MoveOnly&&) = default;
};

NAMESPACE_EVEREST_END

#endif
