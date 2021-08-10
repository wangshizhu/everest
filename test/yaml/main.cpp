#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"

int main(int argc,char** argv)
{
	{
		YAML::Node config = YAML::LoadFile("config.yaml");

		if (config["lastLogin"]) {
			std::cout << "Last logged in: " << config["lastLogin"].as<std::string>() << "\n";
		}

		const std::string username = config["username"].as<std::string>();
		const std::string password = config["password"].as<std::string>();
		std::cout << username.c_str() << std::endl;
		std::cout << password.c_str() << std::endl;
		/*login(username, password);
		config["lastLogin"] = getCurrentDateTime();

		std::ofstream fout("config.yaml");
		fout << config;*/
	}

	system("pause");
	return 0;
}