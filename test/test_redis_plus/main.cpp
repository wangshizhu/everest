#include <iostream>
#include <redis++.h>
//#include <sw/redis++/connection.h>

int main(int argc,char** argv)
{
	{
		sw::redis::ConnectionOptions connection_options;
		connection_options.host = "127.0.0.1";  // Required.
		//connection_options.port = 6379; // Optional. The default port is 6379.
		//connection_options.password = "";   // Optional. No password by default.
		connection_options.db = 1;  // Optional. Use the 0th database by default.

		// Optional. Timeout before we successfully send request to or receive response from redis.
		// By default, the timeout is 0ms, i.e. never timeout and block until we send or receive successfuly.
		// NOTE: if any command is timed out, we throw a TimeoutError exception.
		connection_options.socket_timeout = std::chrono::milliseconds(200);

		// Connect to Redis server with a single connection.
		sw::redis::Redis redis1(connection_options);

		redis1.set("key", "val");
		auto val = redis1.get("key");    // val is of type OptionalString. See 'API Reference' section for details.
		if (val) {
			// Dereference val to get the returned value of std::string type.
			std::cout << *val << std::endl;
		}   // else key doesn't exist.

		sw::redis::ConnectionPoolOptions pool_options;
		pool_options.size = 3;  // Pool size, i.e. max number of connections.

		// Optional. Max time to wait for a connection. 0ms by default, which means wait forever.
		// Say, the pool size is 3, while 4 threds try to fetch the connection, one of them will be blocked.
		pool_options.wait_timeout = std::chrono::milliseconds(100);

		// Optional. Max lifetime of a connection. 0ms by default, which means never expire the connection.
		// If the connection has been created for a long time, i.e. more than `connection_lifetime`,
		// it will be expired and reconnected.
		pool_options.connection_lifetime = std::chrono::minutes(10);

		// Connect to Redis server with a connection pool.
		sw::redis::Redis redis2(connection_options, pool_options);
	}

	system("pause");
	return 0;
}