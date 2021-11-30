#include <iostream>
#include <chrono>
#include <time.h>
#include "date/time_capsule.h"

int main()
{

	{
		auto now = everest::TimeCapsule::Now();
		std::cout << "now:" << now << std::endl;

		auto today = everest::TimeCapsule::TodayTimePoint();
		std::cout << "today:" << today << std::endl;

		auto that_day = everest::TimeCapsule::ThatDayTimePoint(now,10);
		std::cout << "that_day:" << that_day << std::endl;

		auto&& monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(0,1);
		std::cout << "monday:" << monday << std::endl;

		auto&& prev_monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(-1,1,10);
		std::cout << "prev_monday:" << prev_monday << std::endl;

		auto&& next_monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(1,1, 10);
		std::cout << "next_monday:" << next_monday << std::endl;

		// 02:02:02
		auto&& that_time_point = everest::TimeCapsule::TodayTimePoint(1, 61, 61);
		std::cout << "1_61_61:" << that_time_point << std::endl;

		std::cout << "is_same_day:" << everest::TimeCapsule::IsSameDay(today, now) << std::endl;

		std::cout << "is_same_week:" << everest::TimeCapsule::IsSameWeek(today, now) << std::endl;

		std::cout << "format_time:" << everest::TimeCapsule::FormatTimePoint(now) << std::endl;
	}

	{
		time_t t = time(nullptr);
		struct tm* local = std::localtime(&t);
		local->tm_hour = 0;
		local->tm_min = 0;
		local->tm_sec = 0;
		std::cout <<"local_time:"<< mktime(local) << std::endl;
	}

	system("pause");
	return 0;
}