NAMESPACE_EVEREST_BEGIN

LoggerCreateInfo CreateInfoFactory::info_ = LoggerCreateInfo();

void SetLoggerCreateInfo(const LoggerCreateInfo& info)
{
	static CreateInfoFactory factory(info);
}

NAMESPACE_EVEREST_END