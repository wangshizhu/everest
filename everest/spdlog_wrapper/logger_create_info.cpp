NAMESPACE_BEGIN

LoggerCreateInfo CreateInfoFactory::info_ = LoggerCreateInfo();

void SetLoggerCreateInfo(const LoggerCreateInfo& info)
{
	static CreateInfoFactory factory(info);
}

NAMESPACE_END