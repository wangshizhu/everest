#ifndef ADDRESS_H_
#define ADDRESS_H_

NAMESPACE_EVEREST_BEGIN

class Address : public everest::NonCopyable
{
public:
	Address(const std::string& ip, uint16_t port);
	Address(const std::string& ip, const std::string& port);

public:
	const std::string& Ip()const;
	void Ip(uint32_t ip);

	uint16_t Port()const;
	void Port(uint16_t port);

	uint16_t NetPort()const;
	std::string StrPort()const;

	static bool String2Ip(const char* ip, uint32_t& address);
	static std::string Ip2String(uint32_t ip);

private:
	std::string ip_;
	uint16_t port_;
};

NAMESPACE_EVEREST_END

#endif // !ADDRESS_H_

