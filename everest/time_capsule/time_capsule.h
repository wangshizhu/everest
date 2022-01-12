#ifndef TIME_CAPSULE_H_
#define TIME_CAPSULE_H_

NAMESPACE_EVEREST_BEGIN

extern int32_t g_local_time_zone;

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
	* @brief �ӡ�����ʱ�����ʱ���������������ʱ�䡿ת������Ŀ��ʱ����ͬ����ʱ�����ʱ�������
	* ���磺(1) ������ʱ�䣨UTCʱ��+EST 8��1639238400��(2021-12-12 00:00:00)ת��Ϊ
	* ��UTCʱ��+0��1639238400+8*3600 = 1639267200(2021-12-12 00:00:00)
	* 
	* (2) ������ʱ�䣨UTCʱ��+EST 8��- 28800��(1970-01-01 00:00:00)ת��Ϊ
	* ��UTCʱ��+0��-28800+8*3600 = 0(1970-01-01 00:00:00)
	* 
	* (3) ������ʱ�䣨UTCʱ��+EST 8��-28800��(1970-01-01 00:00:00)ת��Ϊ
	* ��������ʱ�䡾UTCʱ��-5��-28800+8*3600-(-5*3600) = 18000(1970-01-01 00:00:00)
	* 
	* @param [in] local_duration_count ����ʱ�����ʱ�����
	* @param [in] dest_time_zone Ŀ��ʱ��
	* 
	* @return Ŀ��ʱ������ʱ�����
	*/
	template<class Duration = std::chrono::seconds>
	static int64_t LocalTimeZoneDurationCountToDestTimeZoneDurationCount(int64_t local_duration_count,int32_t dest_time_zone)
	{
		// ��ת������UTCʱ��+0����Ӧ�ĳ���ʱ���������ת����Ŀ��ʱ������ʱ�����
		auto utc0 = local_duration_count + GetTimeZoneDurationCount<Duration>();

		return utc0 - GetTimeZoneDurationCount<Duration>(dest_time_zone);
	}

	/*
	* @brief �ӡ�����ʱ�����ʱ�������ת������Ŀ��ʱ��ʱ�����ʱ���������
	* Ŀ��ʱ��ʱ�����ʱ������ǻ��ڱ���ʱ��������Ľ�������ؽ��������Ӧ����Ŀ��ʱ����ʱ����㣬
	* ������������(2)���صĽ����1639191600�������������뵽����ʱ������������ʱ�䡾UTCʱ��-5���ĳ�����㲢������á�2021-12-11 11:00:00�������Ľ��
	* 
	* ���磺
	* (1) ������ʱ�䣨UTCʱ��+EST 8��1639238400��(2021-12-12 00:00:00)��һʱ�̶�Ӧ��
	* ��UTCʱ��+0����1639238400 - 8*3600 = 1639209600(2021-12-11 16:00:00)
	*
	* (2) ������ʱ�䣨UTCʱ��+EST 8��1639238400��(2021-12-12 00:00:00)��һʱ�̶�Ӧ��
	* ��������ʱ�䡾UTCʱ��-5����1639238400 - 8*3600 - (5*3600) = 1639191600(2021-12-11 11:00:00)
	*
	* @param [in] local_duration_count ����ʱ�����ʱ�����
	* @param [in] dest_time_zone Ŀ��ʱ��
	*
	* @return Ŀ��ʱ������ʱ�����
	*/
	template<class Duration = std::chrono::seconds>
	static int64_t DestTimeZoneDurationCountBaseOnLocalTimeZone(int64_t local_duration_count, int32_t dest_time_zone)
	{
		// ��ת������UTCʱ��+0����Ӧ�ĳ���ʱ���������ת����Ŀ��ʱ������ʱ�����
		auto utc0 = local_duration_count - GetTimeZoneDurationCount<Duration>();

		if (dest_time_zone < 0)
		{
			return utc0 - GetTimeZoneDurationCount<Duration>(std::abs(dest_time_zone));
		}

		return utc0 + GetTimeZoneDurationCount<Duration>(dest_time_zone);
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
	static int64_t ThatDayTimePoint(int64_t src, uint8_t h = 0, uint8_t m = 0, uint8_t s = 0) noexcept
	{
		return SecondsToDayZeroClock(src) + HoursToSeconds(h) + MinutesToSeconds(m) + s;
	}

	/*
	* @brief �ӱ��ܿ�ʼ��ǰ�����ƫ�Ƽ��ܺ���ܼ���ĳʱĳ��ĳ��
	* ���磺(1) �ӱ��ܿ�ʼƫ��1�ܺ����һ��10:00:00,����ǰʱ����1970-01-01 00:00:00������һ��10:00:00������������
	* (2) �ӱ��ܿ�ʼƫ��-1�ܺ����һ��10:00:00,����ǰʱ����1970-01-01 00:00:00������һ��10:00:00������������
	* 
	* @param [in] offset_weeks ƫ�������������������ӣ������������٣���offset_weeksΪ0ʱ����Ϊ����
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
	static bool IsSameDay(int64_t src, int64_t dest) noexcept
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
	static bool IsSameWeek(int64_t src, int64_t dest) noexcept
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
	static std::string FormatTimePoint(int64_t s)
	{
		return fmt::format("{}-{}", FormatTimePointToYearMonthDay(s), FormatTimePointToHourMinuteSecond(s));
	}

	/*
	* @brief ��������������YYYY-MM-DD�ĸ�ʽ���أ����磺2021-11-29
	* @param [in] s ��ǰʱ������
	* @return ��ʽ�����ʱ��
	*/
	static std::string FormatTimePointToYearMonthDay(int64_t s)
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
	static std::string FormatTimePointToHourMinuteSecond(int64_t s)
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
	* @brief ����������ת��Ϊ����
	* @param [in] w ����
	*/
	static int64_t WeeksToSeconds(int32_t w = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, date::weeks>(w);
	}

	/*
	* @brief ����������ת��Ϊ����
	* @param [in] d ����
	*/
	static int64_t DaysToSeconds(int32_t d = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, date::days>(d);
	}

	/*
	* @brief ������������ת��Ϊ����
	* @param [in] m ������
	*/
	static int64_t MinutesToSeconds(int32_t m = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, std::chrono::minutes>(m);
	}

	/*
	* @brief ������Сʱ��ת��Ϊ����
	* @param [in] h Сʱ��
	*/
	static int64_t HoursToSeconds(int32_t h = 1) noexcept
	{
		return DurationCountToDurationCount<std::chrono::seconds, std::chrono::hours>(h);
	}

	/*
	* @brief ��ָ��duration�ļ���ת��Ϊָ��duration�ļ���
	* ���磺��Сʱ��ת��Ϊ����
	*
	* @param [in] s ָ��duration�ļ��������durationΪstd::chrono::seconds����sΪ����
	*
	* @return ָ��duration�ļ���
	*/
	template<class To, class From = std::chrono::seconds,
		typename std::enable_if_t<!std::is_same_v<To, From>, int> = 0
	>
	static int64_t DurationCountToDurationCount(int64_t s) noexcept
	{
		return DurationCountToDuration<To, From>(s).count();
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

	/*
	* @brief ��ָ��duration�ļ���ת��Ϊָ��time_point
	* @param [in] s ָ��duration�ļ��������durationΪstd::chrono::seconds����sΪ����
	* @return ָ��time_point�����磺std::chrono::time_point<std::chrono::system_clock, date::days>
	*/
	template <class To, class From = std::chrono::seconds>
	static std::chrono::time_point<std::chrono::system_clock, To> DurationCountToTimePoint(int64_t s) noexcept
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
	* @brief ��ݺ���,��ǰʱ���ԡ�1970-01-01 00:00:00��������ǰʱ�������ܵ���һ00:00:00��������������
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
	* @brief ��������ת��Ϊ�������ڵ������00:00:00
	* @param [in] s ����
	* @return ��ǰʱ���ԡ�1970-01-01 00:00:00�����������00:00:00����������������
	*/
	static int64_t SecondsToDayZeroClock(int64_t s) noexcept
	{
		auto&& fixed_seconds = DurationCountToLocalTimeDayBegin(s);
		auto&& that_day = DurationCountToTimePoint<date::days>(fixed_seconds);

		return ToThatDayZeroClock(that_day);
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
	static uint64_t ToThatDayZeroClock(T&& t) noexcept
	{
		return TimePointToDurationCount<date::sys_days::duration,ToDuration>(date::sys_days(t)) - GetTimeZoneDurationCount<ToDuration>();
	}

	/*
	* @brief ��ݺ�������������ת��Ϊtime_point(date::sys_days)
	* @param [in] s ����
	* @return time_point(date::sys_days)
	*/
	static date::sys_days SecondsToDayTimePoint(int64_t s) noexcept
	{
		return date::sys_days(DurationCountToDuration<date::days>(s));
	}

	/*
	* @brief ��ݺ���,��ʱ��ת��Ϊ����
	*/
	template <class ToDuration = std::chrono::seconds>
	static int64_t GetTimeZoneDurationCount(int32_t time_zone = g_local_time_zone) noexcept
	{
		return DurationCountToDurationCount<ToDuration,std::chrono::hours>(time_zone);
	}

	/*
	* @brief ���յ�ǰʱ������������Ϊ��ǰʱ�����������������������
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

NAMESPACE_EVEREST_END

#endif