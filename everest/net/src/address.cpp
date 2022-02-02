NAMESPACE_EVEREST_BEGIN

Address::Address(const std::string& ip, uint16_t port):ip_(ip), port_(port)
{
}

Address::Address(const std::string& ip, const std::string& port)
	:Address(ip,atoi(port.c_str()))
{
}

const std::string& Address::Ip() const
{
	return ip_;
}

void Address::Ip(uint32_t ip)
{
	ip_ = Address::Ip2String(ip);
}

uint16_t Address::Port() const
{
	return port_;
}

void Address::Port(uint16_t port)
{
	port_ = port;
}

uint16_t Address::NetPort()const
{
	return htons(port_);
}

std::string Address::StrPort()const
{
	return std::to_string(port_);
}

bool Address::String2Ip(const char* ip, uint32_t& address)
{
	uint32_t	trial;

#if GENERAL_PLATFORM != PLATFORM_WIN32
	if (inet_aton(ip, (struct in_addr*)&trial) != 0)
#else
	if ((trial = inet_addr(ip)) != INADDR_NONE)
#endif
	{
		address = trial;
		return true;
	}

	struct hostent* hosts = gethostbyname(ip);
	if (hosts != NULL)
	{
		address = *(uint32_t*)(hosts->h_addr_list[0]);
		return true;
	}

	return false;
}

std::string Address::Ip2String(uint32_t address)
{
	address = ntohl(address);

	int p1, p2, p3, p4;
	p1 = address >> 24;
	p2 = (address & 0xffffff) >> 16;
	p3 = (address & 0xffff) >> 8;
	p4 = (address & 0xff);

	return fmt::format("{}.{}.{}.{}",p1,p2,p3,p4);
}

NAMESPACE_EVEREST_END