#ifndef TIME_CAPSULE_H_
#define TIME_CAPSULE_H_

#include <chrono>
#include <optional>
#include <stdint.h>

#include "date/date.h"
#include "fmt/format.h"
#include "fmt/chrono.h"

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

	static uint64_t ThatDayTimePoint(uint64_t src, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		auto&& fixed_seconds = GetFixedSeconds(src);
		auto&& that_day = SecondsToSystemDays(fixed_seconds);

		return SystemTimeToSeconds(that_day) - GetTimeZoneSeconds() + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	static uint64_t WeekDayFromThisWeekOffset(int8_t offset_weeks = 0, uint8_t week_day = 1, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		if (week_day == 0)
		{
			week_day = 7;
		}

		auto&& this_monday = ThisWeekMondayZeroClock();
		auto&& that_monday = this_monday + WeeksToSeconds(offset_weeks);

		return that_monday + DaysToSeconds(week_day - 1) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	static uint64_t TodayTimePoint(uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return ThatDayTimePoint(Now(),h,m,s);
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

	// format: 2021-11-29-22-00-00
	static std::string FormatTimePoint(uint64_t s)
	{
		return fmt::format("{}-{}", FormatTimePointToYearMonthDay(s), FormatTimePointToHourMinuteSecond(s));
	}

	// format: 2021-11-29
	static std::string FormatTimePointToYearMonthDay(uint64_t s)
	{
		auto&& fixed_second = GetFixedSeconds(s);
		auto&& system_days = SecondsToSystemDays(fixed_second);
		auto&& ymd = date::year_month_day(system_days);

		return fmt::format("{}-{}-{}",int32_t(ymd.year()),uint32_t(ymd.month()),uint32_t(ymd.day()));
	}

	// format: 22-00-00
	static std::string FormatTimePointToHourMinuteSecond(uint64_t s)
	{
		auto&& fixed_second = GetFixedSeconds(s);
		auto&& system_days = SecondsToSystemDays(fixed_second);
		auto&& ymd_seconds = SystemTimeToSeconds(system_days) - GetTimeZoneSeconds();
		auto&& diff = s - ymd_seconds;
		auto&& hms = date::time_of_day<std::chrono::seconds>(std::chrono::seconds(diff));

		return fmt::format("{}-{}-{}",hms.hours().count(),hms.minutes().count(),hms.seconds().count());
	}

	// 按照year-month-day hour-minutes-seconds构造时间
	// return:格式错误返回nullopt，格式正确返回秒数
	static std::optional<uint64_t> MakeTimePointByYMDHMS(const char* format_time,const char* format = "%d-%d-%d %d:%d:%d")
	{
		uint32_t y,m,d,h,mi,s = 0;
		auto num = sscanf(format_time, format,&y,&m,&d,&h,&mi,&s);
		if (num != 6)
		{
			return std::nullopt;
		}

		auto&& ymd = date::year_month_day(date::year(y),date::month(m),date::day(d));
		auto&& that_day = SystemTimeToSeconds(date::sys_days(ymd)) - GetTimeZoneSeconds();

		return std::make_optional(ThatDayTimePoint(that_day,h,mi,s));
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

	static uint64_t ThisWeekMondayZeroClock() noexcept
	{
		auto&& fixed_seconds = GetFixedSeconds(Now());
		auto&& today = SecondsToSystemDays(fixed_seconds);
		auto&& this_weekday = date::year_month_weekday(today);
		auto&& week_monday = date::weekday_indexed(date::Monday, this_weekday.index());
		auto&& that_day = date::year_month_weekday(this_weekday.year(), this_weekday.month(), week_monday);

		return SystemTimeToSeconds(date::sys_days(that_day)) - GetTimeZoneSeconds();
	}

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