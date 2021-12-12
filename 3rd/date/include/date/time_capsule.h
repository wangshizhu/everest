#ifndef TIME_CAPSULE_H_
#define TIME_CAPSULE_H_

#include <chrono>
#include <optional>
#include <utility>
#include <stdint.h>

#include "date/date.h"
#include "fmt/format.h"
#include "fmt/chrono.h"

#define NAMESPACE_BEGIN namespace everest{
#define NAMESPACE_END }

NAMESPACE_BEGIN

// 本地时区
int32_t g_local_time_zone = 8;

class TimeCapsule
{
public:

	/*
	* @brief 当前时区自1970-01-01 00:00:00至当前的秒数
	*/
	static uint64_t Now() noexcept
	{
		return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	}

	/*
	* @brief 当前时区自1970-01-01 00:00:00至当前的毫秒数
	*/
	static uint64_t NowMillSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration,std::chrono::milliseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief 当前时区自1970-01-01 00:00:00至当前的微秒数
	*/
	static uint64_t NowMicroSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration, std::chrono::microseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief 当前时区自1970-01-01 00:00:00至当前的纳秒数
	*/
	static uint64_t NowNanoSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration, std::chrono::nanoseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief 从本地时间持续时间计数转到目标时区持续时间计数
	* 例如：(1) 将北京时间（UTC时间+EST 8）1639238400秒(2021-12-12 00:00:00)转换为
	* 【UTC时间+0】1639238400+8*3600 = 1639267200(2021-12-12 00:00:00)
	* 
	* (2) 将北京时间（UTC时间+EST 8）-28800秒(1970-01-01 00:00:00)转换为
	* 【UTC时间+0】-28800+8*3600 = 0(1970-01-01 00:00:00)
	* 
	* (3) 将北京时间（UTC时间+EST 8）-28800秒(1970-01-01 00:00:00)转换为
	* 美国东部时间【UTC时间-5】-28800+8*3600-(-5*3600) = 18000(1970-01-01 00:00:00)
	* 
	* @param [in] local_duration_count 本地时间持续时间计数
	* @param [in] dest_time_zone 目标时区
	* 
	* @return 目标时区持续时间计数
	*/
	template<class Duration = std::chrono::seconds>
	static int64_t LocalTimeZoneDurationCountToDestTimeZoneDurationCount(int64_t local_duration_count,int32_t dest_time_zone)
	{
		// 先转换到【UTC时间+0】对应的持续时间计数，再转化到目标时区持续时间计数
		auto utc0 = local_duration_count + GetTimeZoneDurationCount<Duration>();

		return utc0 - GetTimeZoneDurationCount<Duration>(dest_time_zone);
	}

	/*
	* @brief 计算当前时区自1970-01-01 00:00:00经过指定秒数的那一天的某时某分某秒
	* 例如：计算北京时间（EST 8）1638691687秒（2021-12-05 16:08:07）所在的那天的10:00:00，即2021-12-05 10:00:00，
	* 应该返回当前时区自1970-01-01 00:00:00到2021-12-05 10:00:00所经过的秒数，应该返回1638669600秒
	* 
	* @param [in] src 指定秒数，例如:1638691687秒（2021-12-05 16:08:07）
	* @param [in] h 几时 例如：10时 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] m 几分 例如：0分 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] s 几秒 例如：0秒 注意：参数类型是无符号整型，只是将其转换为秒数
	* 
	* @return 当前时区自1970-01-01 00:00:00经过指定秒数的那一天的某时某分某秒的总秒数
	*/
	static int64_t ThatDayTimePoint(int64_t src, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return SecondsToDayZeroClock(src) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	/*
	* @brief 从本周开始向前或向后偏移几周后的周几的某时某分某秒
	* 例如：(1) 从本周开始偏移1周后的周一的10:00:00,即当前时区自1970-01-01 00:00:00到下周一的10:00:00所经过的秒数
	* (2) 从本周开始偏移-1周后的周一的10:00:00,即当前时区自1970-01-01 00:00:00到上周一的10:00:00所经过的秒数
	* 
	* @param [in] offset_weeks 偏移周数，正数代表增加，负数代表减少，当offset_weeks为0时，即为本周
	* 
	* @param [in] week_day 周几
	* 注意：参数类型是无符号整型，只是计算距离周一的天数，并转换为秒数，
	* 例如外部传入10，虽然每周只有周一、周二...周日，并没有周10，最后计算为周10到周一的天数差值，并将这个差值转换为秒数
	* 所以这个参数的值必须外部保证它的正确性，当传入10时请确保是已知的
	* 
	* @param [in] h 几时 例如：10时 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] m 几分 例如：0分 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] s 几秒 例如：0秒 注意：参数类型是无符号整型，只是将其转换为秒数
	* 
	* @return 秒数
	*/
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

	/*
	* @brief 今天的某时某分某秒，便捷函数，可以使用ThatDayTimePoint函数替代
	* 
	* @param [in] h 几时 例如：10时 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] m 几分 例如：0分 注意：参数类型是无符号整型，只是将其转换为秒数
	* @param [in] s 几秒 例如：0秒 注意：参数类型是无符号整型，只是将其转换为秒数
	* 
	* @return 当前时区自1970-01-01 00:00:00到今天的某时某分某秒总秒数
	*/
	static uint64_t TodayTimePoint(uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return ThatDayTimePoint(Now(),h,m,s);
	}

	/*
	* @brief 两个秒数时间点是否在同一天
	* 注意：如果按照北京时间（EST 8）计算，每日的【08:00:00，次日的07:59:59】即为一天，
	* 所以造成某日的【00:00:00,07:59:59】与【08:00:00,23:59:59】并不在同一天
	* 本函数是将两个处于当前时区的【00:00:00,23:59:59】时间点，计算为同一天
	* 
	* @param [in] src 当前时区秒数
	* @param [in] dest 当前时区秒数
	* 
	* @return true 或者 false
	*/
	static bool IsSameDay(int64_t src, int64_t dest) noexcept
	{
		return DurationCountToLocalTimeDayBegin(src) == DurationCountToLocalTimeDayBegin(dest);
	}

	/*
	* @brief 两个秒数时间点是否在同一周
	* 注意：如果按照北京时间（EST 8）计算，每日的【08:00:00，次日的07:59:59】即为一天，
	* 所以造成周一的【00:00:00,07:59:59】与周一的【08:00:00,23:59:59】并不在同一周
	* 本函数是将两个处于当前时区的【周一的00:00:00,周日的23:59:59】时间点，计算为同一周
	* 
	* @param [in] src 当前时区秒数
	* @param [in] dest 当前时区秒数
	* 
	* @return true 或者 false
	*/
	static bool IsSameWeek(int64_t src, int64_t dest) noexcept
	{
		auto&& fixed_src = DurationCountToLocalTimeDayBegin(src);
		auto&& fixed_dest = DurationCountToLocalTimeDayBegin(dest);
		auto&& src_days = date::year_month_weekday(DurationCountToTimePoint<date::days>(fixed_src));
		auto&& dest_days = date::year_month_weekday(DurationCountToTimePoint<date::days>(fixed_dest));

		return src_days == dest_days;
	}

	/*
	* @brief 将给定秒数按照YYYY-MM-DD-HH-MM-SS的格式返回，例如：2021-11-29-22-00-00
	* @param [in] s 当前时区秒数
	* @return 格式化后的时间
	*/
	static std::string FormatTimePoint(int64_t s)
	{
		return fmt::format("{}-{}", FormatTimePointToYearMonthDay(s), FormatTimePointToHourMinuteSecond(s));
	}

	/*
	* @brief 将给定秒数按照YYYY-MM-DD的格式返回，例如：2021-11-29
	* @param [in] s 当前时区秒数
	* @return 格式化后的时间
	*/
	static std::string FormatTimePointToYearMonthDay(int64_t s)
	{
		auto&& fixed_second = DurationCountToLocalTimeDayBegin(s);
		auto&& system_days = DurationCountToTimePoint<date::days>(fixed_second);
		auto&& ymd = date::year_month_day(system_days);

		return fmt::format("{}-{:02}-{:02}",int32_t(ymd.year()),uint32_t(ymd.month()),uint32_t(ymd.day()));
	}

	/*
	* @brief 将给定秒数按照HH-MM-SS的格式返回，例如：22-00-00
	* @param [in] s 当前时区秒数
	* @return 格式化后的时间
	*/
	static std::string FormatTimePointToHourMinuteSecond(int64_t s)
	{
		auto&& ymd_seconds = SecondsToDayZeroClock(s);
		auto&& diff = s - ymd_seconds;
		auto&& hms = date::time_of_day<std::chrono::seconds>(std::chrono::seconds(diff));

		return fmt::format("{:02}-{:02}-{:02}",hms.hours().count(),hms.minutes().count(),hms.seconds().count());
	}

	/*
	* @brief 根据给定时间按照给定的格式转换为当前时区自1970-01-01 00:00:00到给定时间总秒数
	* 
	* @param [in] format_time 需要转换的时间 例如：2021-12-05-16-08-07
	* @param [in] format 参考格式 例如：%d-%d-%d-%d-%d-%d
	* 
	* @return 如果格式错误返回nullopt，调用方需要处理nullopt的情况，
	* 如果格式正确,则返回当前时区自1970-01-01 00:00:00到给定时间总秒数
	*/
	static std::optional<int64_t> MakeTimePointByYMDHMS(const char* format_time,const char* format = "%d-%d-%d %d:%d:%d") noexcept
	{
		uint32_t y,m,d,h,mi,s = 0;
		auto num = sscanf(format_time, format,&y,&m,&d,&h,&mi,&s);
		if (num != 6)
		{
			return std::nullopt;
		}

		auto&& ymd = date::year_month_day(date::year(y),date::month(m),date::day(d));
		auto&& that_day = ToThatDayZeroClock(ymd);

		return std::make_optional(ThatDayTimePoint(that_day,h,mi,s));
	}

	/*
	* @brief 将给定周数转换为秒数
	* @param [in] w 周数
	*/
	static int64_t WeeksToSeconds(int32_t w = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, date::weeks>(w);
	}

	/*
	* @brief 将给定天数转换为秒数
	* @param [in] d 天数
	*/
	static int64_t DaysToSeconds(int32_t d = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, date::days>(d);
	}

	/*
	* @brief 将给定分钟数转换为秒数
	* @param [in] m 分钟数
	*/
	static int64_t MinutesToSeconds(int32_t m = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, std::chrono::minutes>(m);
	}

	/*
	* @brief 将给定小时数转换为秒数
	* @param [in] h 小时数
	*/
	static int64_t HoursToSeconds(int32_t h = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, std::chrono::hours>(h);
	}


//-----------------------------------------------------------------------------
//	duration_count 到 duration的转换
//  duration_count 到 time_point的转换
//-----------------------------------------------------------------------------
private:
	/*
	* @brief 从指定duration的计数转换为指定duration
	* @param [in] s 指定duration的计数，如果duration为std::chrono::seconds，则s为秒数
	* @return 指定duration，例如：std::chrono::hours
	*/
	template<class To, class From = std::chrono::seconds>
	static To DurationCountToDuration(int64_t s) noexcept
	{
		return std::chrono::duration_cast<To>(From(s));
	}

	/*
	* @brief 从指定duration的计数转换为指定duration的计数
	* 例如：从小时数转换为秒数
	* 
	* @param [in] s 指定duration的计数，如果duration为std::chrono::seconds，则s为秒数
	* 
	* @return 指定duration的计数
	*/
	template<class To, class From = std::chrono::seconds,
	typename std::enable_if_t<!std::is_same_v<To,From>,int> = 0
	>
	static int64_t DurationCountToDurationCount(int64_t s) noexcept
	{
		return DurationCountToDuration<To,From>(s).count();
	}

	/*
	* @brief 从指定duration的计数转换为指定time_point
	* @param [in] s 指定duration的计数，如果duration为std::chrono::seconds，则s为秒数
	* @return 指定time_point，例如：std::chrono::time_point<std::chrono::system_clock, date::days>
	*/
	template <class To, class From = std::chrono::seconds>
	static std::chrono::time_point<std::chrono::system_clock, To> DurationCountToTimePoint(int64_t s) noexcept
	{
		return std::chrono::time_point<std::chrono::system_clock, To>(DurationCountToDuration<To,From>(s));
	}

//-----------------------------------------------------------------------------
//	time_point 到 duration_count的转换
//-----------------------------------------------------------------------------
private:
	/*
	* @brief 从time_point转换为指定duration的计数
	* 例如：从std::chrono::time_point<std::chrono::system_clock, std::chrono::hours>转换为
	* std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
	*
	* @param [in] src 时间点
	* @return 指定duration的计数
	*/
	template<class T, class ToDuration = std::chrono::seconds>
	static uint64_t TimePointToDurationCount(const std::chrono::time_point<std::chrono::system_clock, T>& src) noexcept
	{
		return std::chrono::time_point_cast<ToDuration>(src).time_since_epoch().count();
	}

private:
	/*
	* @brief 便捷函数,当前时区自【1970-01-01 00:00:00】到【当前时间所在周的周一00:00:00】所经过的秒数
	*/
	static uint64_t ThisWeekMondayZeroClock() noexcept
	{
		auto&& fixed_seconds = DurationCountToLocalTimeDayBegin(Now());
		auto&& today = DurationCountToTimePoint<date::days>(fixed_seconds);
		auto&& this_weekday = date::year_month_weekday(today);
		auto&& week_monday = date::weekday_indexed(date::Monday, this_weekday.index());
		auto&& that_day = date::year_month_weekday(this_weekday.year(), this_weekday.month(), week_monday);

		return ToThatDayZeroClock(that_day);
	}

	/*
	* @brief 根据秒数转换为秒数所在的那天的00:00:00
	* @param [in] s 秒数
	* @return 当前时区自【1970-01-01 00:00:00】到【那天的00:00:00】所经过的总秒数
	*/
	static int64_t SecondsToDayZeroClock(int64_t s) noexcept
	{
		auto&& fixed_seconds = DurationCountToLocalTimeDayBegin(s);
		auto&& that_day = DurationCountToTimePoint<date::days>(fixed_seconds);

		return ToThatDayZeroClock(that_day);
	}

	/*
	* @brief 将类型转换时间点std::chrono::time_point<std::chrono::system_clock,date::days>,
	* 并计算当前时区自【1970-01-01 00:00:00】到【那天的00:00:00】所经过的总秒数
	* 
	* @param [in] t必须能用于构造date::sys_days，T可以是date::year_month_weekday，也可以是date::year_month_day
	* 
	* @return 当前时区自【1970-01-01 00:00:00】到【那天的00:00:00】所经过的总秒数
	*/
	template<class T,class ToDuration = std::chrono::seconds>
	static uint64_t ToThatDayZeroClock(T&& t) noexcept
	{
		return TimePointToDurationCount<date::sys_days::duration,ToDuration>(date::sys_days(t)) - GetTimeZoneDurationCount<ToDuration>();
	}

	/*
	* @brief 便捷函数，根据秒数转换为time_point(date::sys_days)
	* @param [in] s 秒数
	* @return time_point(date::sys_days)
	*/
	static date::sys_days SecondsToDayTimePoint(int64_t s) noexcept
	{
		return date::sys_days(DurationCountToDuration<date::days>(s));
	}

	/*
	* @brief 便捷函数,将时区转换为秒数
	*/
	template <class ToDuration = std::chrono::seconds>
	static int64_t GetTimeZoneDurationCount(int32_t time_zone = g_local_time_zone) noexcept
	{
		return DurationCountToDurationCount<ToDuration,std::chrono::hours>(time_zone);
	}

	/*
	* @brief 按照当前时区的秒数修正为当前时区秒数所代表的天数的起点
	* 例如：将落在这个区间（北京时间（EST 8）【2021-12-05 00:00:00，2021-12-05 07:59:59】）的秒数修正为所在的那天的8:00:00，即2021-12-05 08:00:00
	* EST 8 每天的起点为08:00:00, EST 7 每天的起点为07:00:00,
	* 这样做是为了保证秒数落在这个区间（北京时间（EST 8）【2021-12-05 00:00:00，2021-12-05 07:59:59】）和
	* 秒数落在这个区间（北京时间（EST 8）【2021-12-05 08:00:00，2021-12-05 23:59:59】）为同一天，否则会出现如下情况
	* 例如：北京时间（EST 8）2021-12-05 07:59:59转换为std::chrono::time_point<std::chrono::system_clock, date::days>.count为X天
	* 而北京时间（EST 8）2021-12-05 08:00:00转换为std::chrono::time_point<std::chrono::system_clock, date::days>.count为X+1天
	* 进而造成【北京时间（EST 8）2021-12-05 07:59:59】与【北京时间（EST 8）2021-12-05 08:00:00】属于不同天
	* 
	* @param [in] duration_count 持续时间计数
	* @return 持续时间计数所在天的起点时间，即当前时区自【1970-01-01 00:00:00】到【那天的08:00:00】所经过的持续时间
	*/
	template<class ToDuration = std::chrono::seconds,class FromDuration = std::chrono::seconds>
	static int64_t DurationCountToLocalTimeDayBegin(int64_t duration_count) noexcept
	{
		auto&& src_day = DurationCountToTimePoint<date::days, FromDuration>(duration_count);
		auto&& src_day_duration_count = TimePointToDurationCount<date::sys_days::duration,ToDuration>(src_day);

		auto&& later8Clock = duration_count + GetTimeZoneDurationCount<FromDuration>();
		auto&& later8Clock_day = DurationCountToTimePoint<date::days,FromDuration>(later8Clock);
		auto&& later8Clock_day_duration_count = TimePointToDurationCount<date::sys_days::duration, ToDuration>(later8Clock_day);

		if (src_day_duration_count == later8Clock_day_duration_count)
		{
			return src_day_duration_count;
		}

		return later8Clock_day_duration_count;
	}
};

NAMESPACE_END

#endif