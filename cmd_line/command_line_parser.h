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
		void ParseGetOrSetBehavior(int argc, char** argv);

		template<class T,typename... Args>
		void Add(Args&&... args)
		{
			try
			{
				parser_.add<T>(std::forward<decltype(args)>(args)...);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
				exit(EXIT_FAILURE);
			}
		}

		template<class T>
		const T Get(const std::string& arg_name) const
		{
			try
			{
				if (parser_.exist(arg_name))
				{
					return parser_.get<T>(arg_name);
				}
				else
				{
					std::cerr << "there is no flag: --" + arg_name << std::endl;
					return T();
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl<< parser_.usage();
				exit(EXIT_FAILURE);
			}
		}

	private:
		void ParseSetBehavior(int argc, char** argv);

	private:
		cmdline::parser parser_;
	};
}

#endif // !COMMAND_LINE_PARSER_H_

