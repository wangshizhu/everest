#include <iostream>

#include "singleton/singleton.h"
#include "cmd_line/command_line_parser.h"

#define CMD_LINE_SINGLETON everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()

int main(int argc, char** argv)
{
	{
		CMD_LINE_SINGLETON->Add<std::string>("version",'v',"version",false,"1.0.0");

		CMD_LINE_SINGLETON->ParseGetOrSetBehavior(argc, argv);

		std::cout << CMD_LINE_SINGLETON->Get<std::string>("version") << std::endl;
		/*std::cout << CMD_LINE_SINGLETON->Get<std::string>("io_model") << std::endl;*/
	}

	system("pause");
	return 0;
}