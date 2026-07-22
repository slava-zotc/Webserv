#include <exception>
#include <iostream>

#include "Core.hpp"

int main()
{
	try
	{
		Core core(8080);
		core.core_loop();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
