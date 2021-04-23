#include "command_line_parser.h"

namespace everest
{
	CommandLineParser::CommandLineParser() :parser_()
	{
	}

	void CommandLineParser::ParseAndCheck(int argc, char** argv)
	{
		if (argc <= 1)
		{
			return;
		}

		parser_.parse_check(argc, argv);
	}
}