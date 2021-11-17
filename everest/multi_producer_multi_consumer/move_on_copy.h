#ifndef MOVE_ON_COPY_H_
#define MOVE_ON_COPY_H_

namespace everest
{
	template<typename T>
	struct MoveOnCopy 
	{
		mutable T move_only_;

		explicit MoveOnCopy(T&& m) : move_only_(std::move(m)) {}
		MoveOnCopy(MoveOnCopy const& t) : move_only_(std::move(t.move_only_)) {}
		MoveOnCopy(MoveOnCopy&& t) : move_only_(std::move(t.move_only_)) {}

		MoveOnCopy& operator=(MoveOnCopy const& other) 
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
}

#endif
