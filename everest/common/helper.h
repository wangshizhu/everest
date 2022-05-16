#ifndef HELPER_H_
#define HELPER_H_

// trait type that stripping const、volatile、reference
template <typename T>
struct TraitTypeStrippingCVR
{
	using RealType = std::remove_cv_t<std::remove_reference_t<T>>;
};

NAMESPACE_EVEREST_BEGIN

/*
* @brief 从给定【整型类型From对象】的【指定位begin_pos】提取【指定位数disjunction_bit_num】后，
* 根据每一位的值构造出给定【整型类型To对象】
* 例如：from 值为15，对应二进制：0000 1111
* begin_pos:1
* disjunction_bit_num:3
* 中括号内的位即要取的位0000 [111]1
* 用[111]构造To类型的对象
* 
* @note: From和To必须是整型，不能是bool型
*
* @param [in] from From类型对象 
*
* @return 给定【类型To对象】
*/
template<class To, uint16_t begin_pos, uint16_t disjunction_bit_num, class From,
	typename = typename std::enable_if_t
	<
	std::is_integral_v<typename TraitTypeStrippingCVR<From>::RealType>&&
	std::is_integral_v<typename TraitTypeStrippingCVR<To>::RealType> &&
	!std::is_same_v<typename TraitTypeStrippingCVR<From>::RealType, bool> &&
	!std::is_same_v<typename TraitTypeStrippingCVR<To>::RealType, bool>
	>>
To DisjuctionBit(From from)
{
	static_assert(disjunction_bit_num > 0);
	static_assert(disjunction_bit_num <= sizeof(To) * kOneByteBit, "disjunct bit greater dest type range");
	static_assert((disjunction_bit_num + begin_pos) <= sizeof(From) * kOneByteBit, "disjunct bit from begin position greater src type range");

	std::bitset<disjunction_bit_num> tmp;
	tmp.set();

	return To(from >> begin_pos) & tmp.to_ullong();
}

// 获取当前线程id
uint64_t GetCurrentThreadId();

NAMESPACE_EVEREST_END

#endif // !HELPER_H_

