#include <iostream>
#include <chrono>
#include <time.h>
#include <thread>
#include "common/include.h"


class TestSession : public everest::SessionBase
{
public:
	template<typename... Args>
	TestSession(Args&&... args) :SessionBase(std::forward<Args>(args)...)
	{
	}
public:
	everest::SessionType GetSessionType() override
	{
		return everest::SessionType::kExternal;
	}

};

int main()
{
	{
		auto now = everest::TimeCapsule::Now();
		std::cout << "now:" << now << std::endl;

		auto now_millseconds = everest::TimeCapsule::NowMillSeconds();
		std::cout << "now_millseconds:" << now_millseconds << std::endl;

		auto today = everest::TimeCapsule::TodayTimePoint();
		std::cout << "today:" << today << std::endl;

		auto utc0 = everest::TimeCapsule::LocalTimeZoneDurationCountToDestTimeZoneDurationCount(-28800,0);
		std::cout << "today00:00:00 to utc0:" << utc0 << std::endl;

		std::cout << "-28800 is:" << everest::TimeCapsule::FormatTimePoint(-28800) << std::endl;

		auto USA_EST_5 = everest::TimeCapsule::DestTimeZoneDurationCountBaseOnLocalTimeZone(now, -5);
		std::cout << "today00:00:00 to USA_EST_5:" << USA_EST_5<<" ,format:"<< everest::TimeCapsule::FormatTimePoint(USA_EST_5) << std::endl;

		auto that_day = everest::TimeCapsule::ThatDayTimePoint(now,10);
		std::cout << "that_day:" << that_day << std::endl;

		//std::cout << fmt::format("the date is {:%Y-%m-%d-%H-%M-%S}\n", fmt::localtime(that_day)) << std::endl;

		auto&& monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(0,1);
		std::cout << "monday:" << monday << std::endl;

		auto&& prev_monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(-1,1,10);
		std::cout << "prev_monday:" << prev_monday << std::endl;

		auto&& next_monday = everest::TimeCapsule::WeekDayFromThisWeekOffset(1,1, 10);
		std::cout << "next_monday:" << next_monday << std::endl;

		// 02:02:02
		auto&& that_time_point = everest::TimeCapsule::TodayTimePoint(1, 61, 62);
		std::cout << "1_61_62:" << that_time_point << std::endl;

		std::cout << "is_same_day:" << everest::TimeCapsule::IsSameDay(today, now) << std::endl;

		std::cout << "is_same_week:" << everest::TimeCapsule::IsSameWeek(today, now) << std::endl;

		std::cout << "format_time:" << everest::TimeCapsule::FormatTimePoint(now) << std::endl;

		auto&& result = everest::TimeCapsule::MakeTimePointByYMDHMS("2021-12-2-0-00-1", "%d-%d-%d-%d-%d-%d");
		if (result)
		{
			std::cout << "MakeTimePoint(2021-12-2 0:00:1):" << result.value() << std::endl;
		}
		
	}

	{
		time_t t = time(nullptr);
		struct tm* local = std::localtime(&t);
		local->tm_hour = 0;
		local->tm_min = 0;
		local->tm_sec = 0;
		std::cout <<"local_time:"<< mktime(local) << std::endl;
	}

	{
		everest::Clock clock;
		everest::SetThreadLocalClock(&clock);

		everest::Timer<std::chrono::seconds> t(1, true);
		t.Start();

		std::this_thread::sleep_for(std::chrono::seconds(1));

		clock.TickTock();

		if (t.Expired())
		{
			std::cout << "expired" << std::endl;
		}
	}

	system("pause");
	return 0;
}