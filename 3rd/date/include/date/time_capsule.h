#ifndef TIME_CAPSULE_H_
#define TIME_CAPSULE_H_

#include <chrono>
#include <stdint.h>

#include "date/date.h"

#define NAMESPACE_HEADER namespace everest{
#define NAMESPACE_TAIL }

NAMESPACE_HEADER

int32_t g_time_zone = 8;

class TimeCapsule
{
public:
	static uint64_t Now() noexcept
	{
		return SystemTimeToSeconds(std::chrono::system_clock::now());
	}

	static uint64_t TodayZeroClock() noexcept
	{
		auto&& fixed_seconds = GetFixedSeconds(Now());
		auto&& today = SecondsToSystemDays(fixed_seconds);

		return SystemTimeToSeconds(today) - GetTimeZoneSeconds();
	}

	static uint64_t ThisWeekMondayZeroClock() noexcept
	{
		auto&& fixed_seconds = GetFixedSeconds(Now());
		auto&& today = SecondsToSystemDays(fixed_seconds);
		auto&& this_weekday = date::year_month_weekday(today);
		auto&& week_monday = date::weekday_indexed(date::Monday, this_weekday.index());
		auto&& that_day = date::year_month_weekday(this_weekday.year(), this_weekday.month(), week_monday);

		return SystemTimeToSeconds(date::sys_days(that_day)) - GetTimeZoneSeconds();
	}

	static uint64_t PrevWeekDayTimePoint(uint8_t week_day = 1, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		if (week_day == 0)
		{
			week_day = 7;
		}

		auto&& this_monday = ThisWeekMondayZeroClock();
		auto&& prev_monday = this_monday - WeeksToSeconds(1);

		return prev_monday + DaysToSeconds(week_day - 1) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	static uint64_t NextWeekDayTimePoint(uint8_t week_day = 1, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		if (week_day == 0)
		{
			week_day = 7;
		}

		auto&& this_monday = ThisWeekMondayZeroClock();
		auto&& next_monday = this_monday + WeeksToSeconds(1);

		return next_monday + DaysToSeconds(week_day - 1) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	static uint64_t TodayTimePoint(uint8_t h, uint8_t m, uint8_t s) noexcept
	{
		return TodayZeroClock() + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	static bool IsSameDay(uint64_t src, uint64_t dest) noexcept
	{
		return GetFixedSeconds(src) == GetFixedSeconds(dest);
	}

	static bool IsSameWeek(uint64_t src, uint64_t dest) noexcept
	{
		auto&& fixed_src = GetFixedSeconds(src);
		auto&& fixed_dest = GetFixedSeconds(dest);
		auto&& src_days = date::year_month_weekday(SecondsToSystemDays(fixed_src));
		auto&& dest_days = date::year_month_weekday(SecondsToSystemDays(fixed_dest));

		return src_days == dest_days;
	}

	static int64_t WeeksToSeconds(int32_t w = 1) noexcept
	{
		return std::chrono::duration_cast<std::chrono::seconds>(date::weeks(w)).count();
	}

	static int64_t DaysToSeconds(int32_t d = 1) noexcept
	{
		return std::chrono::duration_cast<std::chrono::seconds>(date::days(d)).count();
	}

	static int64_t MinutesToSeconds(int32_t m = 1) noexcept
	{
		return std::chrono::seconds(std::chrono::minutes(m)).count();
	}

	static int64_t HoursToSeconds(int32_t h = 1) noexcept
	{
		return std::chrono::seconds(std::chrono::hours(h)).count();
	}

private:

	template<class T>
	static uint64_t SystemTimeToSeconds(const std::chrono::time_point<std::chrono::system_clock,T>& src) noexcept
	{
		return std::chrono::time_point_cast<std::chrono::seconds>(src).time_since_epoch().count();
	}

	static date::days  SecondsToDays(uint64_t s) noexcept
	{
		return std::chrono::duration_cast<date::days>(std::chrono::seconds(s));
	}

	static date::sys_days SecondsToSystemDays(uint64_t s) noexcept
	{
		return date::sys_days(SecondsToDays(s));
	}

	static int64_t GetTimeZoneSeconds() noexcept
	{
		return std::chrono::seconds(std::chrono::hours(g_time_zone)).count();
	}

	static uint64_t GetFixedSeconds(uint64_t src) noexcept
	{
		auto&& src_day = SecondsToSystemDays(src);
		auto&& src_day_seconds = SystemTimeToSeconds(src_day);

		auto&& demarcation = src + GetTimeZoneSeconds();
		auto&& demarcation_day = SecondsToSystemDays(demarcation);
		auto&& demarcation_day_seconds = SystemTimeToSeconds(demarcation_day);

		if (src_day_seconds == demarcation_day_seconds)
		{
			return src_day_seconds;
		}

		return demarcation_day_seconds;
	}
};

NAMESPACE_TAIL

#endif