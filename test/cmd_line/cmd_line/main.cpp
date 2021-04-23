#include <iostream>

#include "singleton/singleton.h"
#include "cmd_line/command_line_parser.h"

int main(int argc, char** argv)
{
	{
		everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()->Add<std::string>("version",'v',"version",false,"1.0.0");
		std::cout << "next line" << std::endl;
		everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()->ParseAndCheck(argc, argv);

		std::cout << everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()->Get<std::string>("version") << std::endl;
		/*std::cout << everest::ThreadSafeSingleton<everest::CommandLineParser>::GetInstance()->Get<std::string>("io_model") << std::endl;*/

		
	}

	system("pause");
	return 0;
}