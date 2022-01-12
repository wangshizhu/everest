
NAMESPACE_EVEREST_BEGIN

void GetSinksByLoggerCreateInfo(std::vector<spdlog::sink_ptr>& sinks, const std::string& logger_name, const LoggerCreateInfo& info)
{
	// �Ƿ����������̨
	if (info.to_stdout)
	{
		sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	}

	if (info.daily && info.rotate)
	{
		auto daily_sink = std::make_shared<spdlog::sinks::daily_rotating_file_sink_mt>(logger_name, info.log_file_size, 0, 0, false);
		sinks.push_back(daily_sink);
	}
	else
	{
		// ÿ��̶�ʱ�������log�ļ�
		if (info.daily)
		{
			auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logger_name, 0, 0, false);
			sinks.push_back(daily_sink);
		}

		// �����ļ���С������log�ļ�
		if (info.rotate)
		{
			auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logger_name, info.log_file_size, 3);
			sinks.push_back(rotating_sink);
		}
	}
}

NAMESPACE_EVEREST_END