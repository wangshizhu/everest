#ifndef CUSTOM_LEVEL_H_
#define CUSTOM_LEVEL_H_

#include "g3log/loglevels.hpp"

const LEVELS ERR{ INFO.value + 1, {"ERROR"} };

namespace everest
{
	void AddCustomLogLevel()
	{
		g3::only_change_at_initialization::addLogLevel(ERR);
		// ...
	}
}

#endif // !CUSTOM_LEVEL_H_

