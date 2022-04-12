/*
* 对目标类型T对象以移动构造的方式拷贝
* 例如：复制拷贝、赋值拷贝转换为以右值拷贝的方式
*/

#ifndef MOVE_ON_COPY_H_
#define MOVE_ON_COPY_H_

NAMESPACE_EVEREST_BEGIN

template<typename T>
class MoveOnCopy
{
public:
	mutable T move_only_;

public:
	explicit MoveOnCopy(T&& concrete) : move_only_(std::move(concrete)) {}

	MoveOnCopy(const MoveOnCopy& other) : move_only_(std::move(other.move_only_)) {}
	MoveOnCopy(MoveOnCopy&& other) : move_only_(std::move(other.move_only_)) {}

	MoveOnCopy& operator=(const MoveOnCopy& other)
	{
		move_only_ = std::move(other.move_only_);
		return *this;
	}

	MoveOnCopy& operator=(MoveOnCopy&& other)
	{
		move_only_ = std::move(other.move_only_);
		return *this;
	}

	void operator()()
	{
		move_only_();
	}

	T& Get()
	{
		return move_only_;
	}

	T Release()
	{
		return std::move(move_only_);
	}
};

NAMESPACE_EVEREST_END

#endif
