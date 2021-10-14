#include <iostream>
#include "message/test.pb.h"

int main(int argc,char** argv)
{
	{
		tutorial::AddressBook ab;
		auto one = ab.add_people();
		one->set_name("test");

		for (auto i =0;i<ab.people_size();++i)
		{
			std::cout << ab.people(i).name() << std::endl;
		}
	}

	system("pause");
	return 0;
}