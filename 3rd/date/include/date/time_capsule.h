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

int32_t g_time_zone = 8;

class TimeCapsule
{
public:

	/*
	* @brief ��ǰʱ����1970-01-01 00:00:00����ǰ������
	*/
	static uint64_t Now() noexcept
	{
		return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	}

	/*
	* @brief ��ǰʱ����1970-01-01 00:00:00����ǰ�ĺ�����
	*/
	static uint64_t NowMillSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration,std::chrono::milliseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief ��ǰʱ����1970-01-01 00:00:00����ǰ��΢����
	*/
	static uint64_t NowMicroSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration, std::chrono::microseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief ��ǰʱ����1970-01-01 00:00:00����ǰ��������
	*/
	static uint64_t NowNanoSeconds() noexcept
	{
		return TimePointToDurationCount<std::chrono::system_clock::duration, std::chrono::nanoseconds>(std::chrono::system_clock::now());
	}

	/*
	* @brief ���㵱ǰʱ����1970-01-01 00:00:00����ָ����������һ���ĳʱĳ��ĳ��
	* ���磺���㱱��ʱ�䣨EST 8��1638691687�루2021-12-05 16:08:07�����ڵ������10:00:00����2021-12-05 10:00:00��
	* Ӧ�÷��ص�ǰʱ����1970-01-01 00:00:00��2021-12-05 10:00:00��������������Ӧ�÷���1638669600��
	* 
	* @param [in] src ָ������������:1638691687�루2021-12-05 16:08:07��
	* @param [in] h ��ʱ ���磺10ʱ ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] m ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] s ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* 
	* @return ��ǰʱ����1970-01-01 00:00:00����ָ����������һ���ĳʱĳ��ĳ���������
	*/
	static uint64_t ThatDayTimePoint(uint64_t src, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return SecondsToDayZeroClock(src) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	/*
	* @brief �ӱ��ܿ�ʼ��ǰ�����ƫ�Ƽ��ܺ���ܼ���ĳʱĳ��ĳ��
	* ���磺(1) �ӱ��ܿ�ʼƫ��1�ܺ����һ��10:00:00,����ǰʱ����1970-01-01 00:00:00������һ��10:00:00������������
	* (2) �ӱ��ܿ�ʼƫ��-1�ܺ����һ��10:00:00,����ǰʱ����1970-01-01 00:00:00������һ��10:00:00������������
	* 
	* @param [in] offset_weeks ƫ�������������������ӣ�����������٣���offset_weeksΪ0ʱ����Ϊ����
	* 
	* @param [in] week_day �ܼ�
	* ע�⣺�����������޷������ͣ�ֻ�Ǽ��������һ����������ת��Ϊ������
	* �����ⲿ����10����Ȼÿ��ֻ����һ���ܶ�...���գ���û����10��������Ϊ��10����һ��������ֵ�����������ֵת��Ϊ����
	* �������������ֵ�����ⲿ��֤������ȷ�ԣ�������10ʱ��ȷ������֪��
	* 
	* @param [in] h ��ʱ ���磺10ʱ ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] m ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] s ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* 
	* @return ����
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
	* @brief �����ĳʱĳ��ĳ�룬��ݺ���������ʹ��ThatDayTimePoint�������
	* 
	* @param [in] h ��ʱ ���磺10ʱ ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] m ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* @param [in] s ���� ���磺0�� ע�⣺�����������޷������ͣ�ֻ�ǽ���ת��Ϊ����
	* 
	* @return ��ǰʱ����1970-01-01 00:00:00�������ĳʱĳ��ĳ��������
	*/
	static uint64_t TodayTimePoint(uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return ThatDayTimePoint(Now(),h,m,s);
	}

	/*
	* @brief ��������ʱ����Ƿ���ͬһ��
	* ע�⣺������ձ���ʱ�䣨EST 8�����㣬ÿ�յġ�08:00:00�����յ�07:59:59����Ϊһ�죬
	* �������ĳ�յġ�00:00:00,07:59:59���롾08:00:00,23:59:59��������ͬһ��
	* �������ǽ��������ڵ�ǰʱ���ġ�00:00:00,23:59:59��ʱ��㣬����Ϊͬһ��
	* 
	* @param [in] src ��ǰʱ������
	* @param [in] dest ��ǰʱ������
	* 
	* @return true ���� false
	*/
	static bool IsSameDay(uint64_t src, uint64_t dest) noexcept
	{
		return DurationCountToLocalTimeDayBegin(src) == DurationCountToLocalTimeDayBegin(dest);
	}

	/*
	* @brief ��������ʱ����Ƿ���ͬһ��
	* ע�⣺������ձ���ʱ�䣨EST 8�����㣬ÿ�յġ�08:00:00�����յ�07:59:59����Ϊһ�죬
	* ���������һ�ġ�00:00:00,07:59:59������һ�ġ�08:00:00,23:59:59��������ͬһ��
	* �������ǽ��������ڵ�ǰʱ���ġ���һ��00:00:00,���յ�23:59:59��ʱ��㣬����Ϊͬһ��
	* 
	* @param [in] src ��ǰʱ������
	* @param [in] dest ��ǰʱ������
	* 
	* @return true ���� false
	*/
	static bool IsSameWeek(uint64_t src, uint64_t dest) noexcept
	{
		auto&& fixed_src = DurationCountToLocalTimeDayBegin(src);
		auto&& fixed_dest = DurationCountToLocalTimeDayBegin(dest);
		auto&& src_days = date::year_month_weekday(DurationCountToTimePoint<date::days>(fixed_src));
		auto&& dest_days = date::year_month_weekday(DurationCountToTimePoint<date::days>(fixed_dest));

		return src_days == dest_days;
	}

	/*
	* @brief ��������������YYYY-MM-DD-HH-MM-SS�ĸ�ʽ���أ����磺2021-11-29-22-00-00
	* @param [in] s ��ǰʱ������
	* @return ��ʽ�����ʱ��
	*/
	static std::string FormatTimePoint(uint64_t s)
	{
		return fmt::format("{}-{}", FormatTimePointToYearMonthDay(s), FormatTimePointToHourMinuteSecond(s));
	}

	/*
	* @brief ��������������YYYY-MM-DD�ĸ�ʽ���أ����磺2021-11-29
	* @param [in] s ��ǰʱ������
	* @return ��ʽ�����ʱ��
	*/
	static std::string FormatTimePointToYearMonthDay(uint64_t s)
	{
		auto&& fixed_second = DurationCountToLocalTimeDayBegin(s);
		auto&& system_days = DurationCountToTimePoint<date::days>(fixed_second);
		auto&& ymd = date::year_month_day(system_days);

		return fmt::format("{}-{:02}-{:02}",int32_t(ymd.year()),uint32_t(ymd.month()),uint32_t(ymd.day()));
	}

	/*
	* @brief ��������������HH-MM-SS�ĸ�ʽ���أ����磺22-00-00
	* @param [in] s ��ǰʱ������
	* @return ��ʽ�����ʱ��
	*/
	static std::string FormatTimePointToHourMinuteSecond(uint64_t s)
	{
		auto&& ymd_seconds = SecondsToDayZeroClock(s);
		auto&& diff = s - ymd_seconds;
		auto&& hms = date::time_of_day<std::chrono::seconds>(std::chrono::seconds(diff));

		return fmt::format("{:02}-{:02}-{:02}",hms.hours().count(),hms.minutes().count(),hms.seconds().count());
	}

	/*
	* @brief ���ݸ���ʱ�䰴�ո����ĸ�ʽת��Ϊ��ǰʱ����1970-01-01 00:00:00������ʱ��������
	* 
	* @param [in] format_time ��Ҫת����ʱ�� ���磺2021-12-05-16-08-07
	* @param [in] format �ο���ʽ ���磺%d-%d-%d-%d-%d-%d
	* 
	* @return �����ʽ���󷵻�nullopt�����÷���Ҫ����nullopt�������
	* �����ʽ��ȷ,�򷵻ص�ǰʱ����1970-01-01 00:00:00������ʱ��������
	*/
	static std::optional<uint64_t> MakeTimePointByYMDHMS(const char* format_time,const char* format = "%d-%d-%d %d:%d:%d") noexcept
	{
		uint32_t y,m,d,h,mi,s = 0;
		auto num = sscanf(format_time, format,&y,&m,&d,&h,&mi,&s);
		if (num != 6)
		{
			return std::nullopt;
		}

		auto&& ymd = date::year_month_day(date::year(y),date::month(m),date::day(d));
		auto&& that_day = TToThatDayZeroClock(ymd);

		return std::make_optional(ThatDayTimePoint(that_day,h,mi,s));
	}

	/*
	* @brief ����������ת��Ϊ����
	* @param [in] w ����
	*/
	static int64_t WeeksToSeconds(int32_t w = 1) noexcept
	{
		return DurationCountToDuration<std::chrono::seconds, date::weeks>(w).count();
	}

	/*
	* @brief ����������ת��Ϊ����
	* @param [in] d ����
	*/
	static int64_t DaysToSeconds(int32_t d = 1) noexcept
	{
		return DurationCountToDuration<std::chrono::seconds, date::days>(d).count();
	}

	/*
	* @brief ������������ת��Ϊ����
	* @param [in] m ������
	*/
	static int64_t MinutesToSeconds(int32_t m = 1) noexcept
	{
		return DurationCountToDuration<std::chrono::seconds, std::chrono::minutes>(m).count();
	}

	/*
	* @brief ������Сʱ��ת��Ϊ����
	* @param [in] h Сʱ��
	*/
	static int64_t HoursToSeconds(int32_t h = 1) noexcept
	{
		return DurationCountToDuration<std::chrono::seconds, std::chrono::hours>(h).count();
	}


//-----------------------------------------------------------------------------
//	duration_count �� duration��ת��
//  duration_count �� time_point��ת��
//-----------------------------------------------------------------------------
private:
	/*
	* @brief ��ָ��duration�ļ���ת��Ϊָ��duration
	* @param [in] s ָ��duration�ļ��������durationΪstd::chrono::seconds����sΪ����
	* @return ָ��duration�����磺std::chrono::hours
	*/
	template<class To, class From = std::chrono::seconds>
	static To DurationCountToDuration(int64_t s) noexcept
	{
		return std::chrono::duration_cast<To>(From(s));
	}

	template <class To, class From = std::chrono::seconds>
	static std::chrono::time_point<std::chrono::system_clock, To> DurationCountToTimePoint(uint64_t s)
	{
		return std::chrono::time_point<std::chrono::system_clock, To>(DurationCountToDuration<To,From>(s));
	}

//-----------------------------------------------------------------------------
//	time_point �� duration_count��ת��
//-----------------------------------------------------------------------------
private:
	/*
	* @brief ��time_pointת��Ϊָ��duration�ļ���
	* ���磺��std::chrono::time_point<std::chrono::system_clock, std::chrono::hours>ת��Ϊ
	* std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
	*
	* @param [in] src ʱ���
	* @return ָ��duration�ļ���
	*/
	template<class T, class ToDuration = std::chrono::seconds>
	static uint64_t TimePointToDurationCount(const std::chrono::time_point<std::chrono::system_clock, T>& src) noexcept
	{
		return std::chrono::time_point_cast<ToDuration>(src).time_since_epoch().count();
	}

private:
	/*
	* @brief ��ǰʱ���ԡ�1970-01-01 00:00:00��������ǰʱ�������ܵ���һ00:00:00��������������
	*/
	static uint64_t ThisWeekMondayZeroClock() noexcept
	{
		auto&& fixed_seconds = DurationCountToLocalTimeDayBegin(Now());
		auto&& today = DurationCountToTimePoint<date::days>(fixed_seconds);
		auto&& this_weekday = date::year_month_weekday(today);
		auto&& week_monday = date::weekday_indexed(date::Monday, this_weekday.index());
		auto&& that_day = date::year_month_weekday(this_weekday.year(), this_weekday.month(), week_monday);

		return TToThatDayZeroClock(that_day);
	}

	/*
	* @brief ��������ת��Ϊ�������ڵ������00:00:00
	* @param [in] s ����
	* @return ��ǰʱ���ԡ�1970-01-01 00:00:00�����������00:00:00����������������
	*/
	static uint64_t SecondsToDayZeroClock(uint64_t s) noexcept
	{
		auto&& fixed_seconds = DurationCountToLocalTimeDayBegin(s);
		auto&& that_day = DurationCountToTimePoint<date::days>(fixed_seconds);

		return TToThatDayZeroClock(that_day);
	}

	/*
	* @brief ������ת��ʱ���std::chrono::time_point<std::chrono::system_clock,date::days>,
	* �����㵱ǰʱ���ԡ�1970-01-01 00:00:00�����������00:00:00����������������
	* 
	* @param [in] t���������ڹ���date::sys_days��T������date::year_month_weekday��Ҳ������date::year_month_day
	* 
	* @return ��ǰʱ���ԡ�1970-01-01 00:00:00�����������00:00:00����������������
	*/
	template<class T,class ToDuration = std::chrono::seconds>
	static uint64_t TToThatDayZeroClock(T&& t)
	{
		return TimePointToDurationCount<date::sys_days::duration,ToDuration>(date::sys_days(t)) - GetTimeZoneDurationCount<ToDuration>();
	}

	/*
	* @brief ��ݺ�������������ת��Ϊtime_point(date::sys_days)
	* @param [in] s ����
	* @return time_point(date::sys_days)
	*/
	static date::sys_days SecondsToDayTimePoint(uint64_t s) noexcept
	{
		return date::sys_days(DurationCountToDuration<date::days>(s));
	}

	/*
	* @brief ��ݺ���,��ʱ��ת��Ϊ����
	*/
	template <class ToDuration = std::chrono::seconds>
	static int64_t GetTimeZoneDurationCount() noexcept
	{
		return DurationCountToDuration<ToDuration,std::chrono::hours>(g_time_zone).count();
	}

	/*
	* @brief ���յ�ǰʱ������������Ϊ��ǰʱ����������������������
	* ���磺������������䣨����ʱ�䣨EST 8����2021-12-05 00:00:00��2021-12-05 07:59:59��������������Ϊ���ڵ������8:00:00����2021-12-05 08:00:00
	* EST 8 ÿ������Ϊ08:00:00, EST 7 ÿ������Ϊ07:00:00,
	* ��������Ϊ�˱�֤��������������䣨����ʱ�䣨EST 8����2021-12-05 00:00:00��2021-12-05 07:59:59������
	* ��������������䣨����ʱ�䣨EST 8����2021-12-05 08:00:00��2021-12-05 23:59:59����Ϊͬһ�죬���������������
	* ���磺����ʱ�䣨EST 8��2021-12-05 07:59:59ת��Ϊstd::chrono::time_point<std::chrono::system_clock, date::days>.countΪX��
	* ������ʱ�䣨EST 8��2021-12-05 08:00:00ת��Ϊstd::chrono::time_point<std::chrono::system_clock, date::days>.countΪX+1��
	* ������ɡ�����ʱ�䣨EST 8��2021-12-05 07:59:59���롾����ʱ�䣨EST 8��2021-12-05 08:00:00�����ڲ�ͬ��
	* 
	* @param [in] duration_count ����ʱ�����
	* @return ����ʱ���������������ʱ�䣬����ǰʱ���ԡ�1970-01-01 00:00:00�����������08:00:00���������ĳ���ʱ��
	*/
	template<class ToDuration = std::chrono::seconds,class FromDuration = std::chrono::seconds>
	static uint64_t DurationCountToLocalTimeDayBegin(uint64_t duration_count) noexcept
	{
		auto&& src_day = DurationCountToTimePoint<date::days, FromDuration>(duration_count);
		auto&& src_day_duration_count = TimePointToDurationCount<src_day::duration,ToDuration>(src_day);

		auto&& demarcation = duration_count + GetTimeZoneDurationCount<FromDuration>();
		auto&& demarcation_day = DurationCountToTimePoint<date::days,FromDuration>(demarcation);
		auto&& demarcation_day_duration_count = TimePointToDurationCount<src_day::duration, ToDuration>(demarcation_day);

		if (src_day_duration_count == demarcation_day_duration_count)
		{
			return src_day_duration_count;
		}

		return demarcation_day_duration_count;
	}
};

NAMESPACE_END

#endif