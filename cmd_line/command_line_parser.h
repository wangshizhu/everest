#ifndef COMMAND_LINE_PARSER_H_
#define COMMAND_LINE_PARSER_H_

#include "cmdline.h"
#include "../singleton/singleton.h"

namespace everest
{
	class CommandLineParser
	{
	public:
		CommandLineParser();

	public:
		void ParseAndCheck(int argc, char** argv);

		template<class T,typename... Args>
		void Add(Args&&... args)
		{
			parser_.add<T>(std::forward<decltype(args)>(args)...);
		}

		template<class T>
		const T& Get(const std::string& arg_name) const
		{
			try
			{
				if (parser_.exist(arg_name))
				{
					return parser_.get<T>(arg_name);
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl<< parser_.usage();
				exit(0);
			}
		}

	private:
		cmdline::parser parser_;
	};
}

#endif // !COMMAND_LINE_PARSER_H_

