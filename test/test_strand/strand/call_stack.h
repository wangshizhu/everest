#ifndef CALL_STACK_H_
#define CALL_STACK_H_

template <typename Key, typename Value = unsigned char>
class Callstack 
{
public:
	class Iterator;

	class Context 
	{
	public:
		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;

		explicit Context(Key* k)
			: key_(k), next_(Callstack<Key, Value>::top_)
		{
			value_ = reinterpret_cast<unsigned char*>(this);
			Callstack<Key, Value>::top_ = this;
		}

		Context(Key* k, Value& v)
			: key_(k), value_(&v), next_(Callstack<Key, Value>::top_)
		{
			Callstack<Key, Value>::top_ = this;
		}

		~Context() 
		{
			Callstack<Key, Value>::top_ = next_;
		}

		Key* GetKey() 
		{
			return key_;
		}

		Value* GetValue() 
		{
			return value_;
		}

	private:
		friend class Callstack<Key, Value>;
		friend class Callstack<Key, Value>::Iterator;
		Key* key_;
		Value* value_;
		Context* next_;
	};

	class Iterator 
	{
	public:
		Iterator(Context* ctx) : context_(ctx)
		{
		}

		Iterator& operator++() 
		{
			if (context_)
			{
				context_ = context_->m_next;
			}
			return *this;
		}

		bool operator!=(const Iterator& other) 
		{
			return context_ != other.context_;
		}

		Context* operator*() 
		{
			return context_;
		}

	private:
		Context* context_;
	};

	// Determine if the specified owner is on the stack
	// \return
	//  The address of the value if present, nullptr if not present
	static Value* Contains(const Key* k) 
	{
		Context* elem = top_;
		while (elem) 
		{
			if (elem->key_ == k)
			{
				return elem->value_;
			}
			elem = elem->next_;
		}
		return nullptr;
	}

	static Iterator begin() 
	{
		return Iterator(top_);
	}

	static Iterator end() 
	{
		return Iterator(nullptr);
	}

private:
	static thread_local Context* top_;
};

template <typename Key, typename Value>
typename thread_local Callstack<Key, Value>::Context*
Callstack<Key, Value>::top_ = nullptr;

#endif // !CALL_STACK_H_

