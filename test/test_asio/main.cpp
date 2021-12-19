#include <iostream>
#include <asio.hpp>

void print(const asio::error_code& /*e*/)
{
	std::cout << "Hello, world!" << std::endl;
}

class printer
{
public:
	printer(asio::io_context& io)
		: timer_(io, asio::chrono::seconds(1)),
		count_(0)
	{
		timer_.async_wait([self = this] (const asio::error_code&){self->print(); });
	}
	~printer()
	{
		std::cout << "Final count is " << count_ << std::endl;
	}
	void print()
	{
		if (count_ < 5)
		{
			std::cout << count_ << std::endl;
			++count_;

			timer_.expires_at(timer_.expiry() + asio::chrono::seconds(1));
			timer_.async_wait([self = this] (const asio::error_code&){self->print(); });
		}
	}

private:
	asio::steady_timer timer_;
	int count_;
};

int main()
{
	/*{
		asio::io_context io;
		asio::steady_timer t(io, asio::chrono::seconds(5));
		t.wait();
		std::cout << "Hello, world!" << std::endl;
	}*/
	/*{
		asio::io_context io;

		asio::steady_timer t(io);
		t.expires_after(asio::chrono::seconds(5));
		t.async_wait(&print);
		io.run();
	}*/

	{
		asio::io_context io;
		printer p(io);
		io.run();
	}
	system("pause");
	return 0;
}