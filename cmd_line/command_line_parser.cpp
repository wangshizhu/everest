#include "command_line_parser.h"

namespace everest
{
	CommandLineParser::CommandLineParser() :parser_()
	{
	}

	void CommandLineParser::ParseGetOrSetBehavior(int argc, char** argv)
	{
		const std::string program_full_name(argv[0]);
		char flag = '/';

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
		flag = '\\';
#endif
		auto pos = program_full_name.find_last_of(flag);
		auto program_short_name = program_full_name.substr(pos + 1);
		auto point_pos = program_short_name.find_first_of('.');
		if (point_pos != std::string::npos)
		{
			program_short_name = program_short_name.substr(0,point_pos);
		}

		parser_.set_program_name(program_short_name);

		if (argc <= 1)
		{
			return;
		}

		if (argc > 2)
		{
			ParseSetBehavior(argc,argv);
			return;
		}

		int arg_index = 1;

		if (strncmp(argv[arg_index], "--", 2) == 0)
		{
			const char* p = strchr(argv[arg_index] + 2, '=');
			if (p)
			{
				ParseSetBehavior(argc, argv);
			}
			else
			{
				parser_.print(std::string(argv[arg_index] + 2));
			}
			return;
		}
		else if (strncmp(argv[arg_index], "-", 1) == 0)
		{
			if (!argv[arg_index][1])
			{
				std::cerr << "error command line param,need a letter,you given:"<< argv[arg_index] << std::endl;
				exit(EXIT_FAILURE);
			}

			if (argv[arg_index][2])
			{
				std::cerr << "error short name,short name must be single letter,you given:"<< argv[arg_index] << std::endl;
				exit(EXIT_FAILURE);
			}

			parser_.print(argv[arg_index][1]);
			return;
		}
	}

	void CommandLineParser::ParseSetBehavior(int argc, char** argv)
	{
		parser_.parse_check(argc, argv);
	}
}