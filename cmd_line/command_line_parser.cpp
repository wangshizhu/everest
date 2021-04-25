#include "command_line_parser.h"

namespace everest
{
	CommandLineParser::CommandLineParser() :parser_()
	{
	}

	void CommandLineParser::ParseGetOrSetBehavior(int argc, char** argv)
	{
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