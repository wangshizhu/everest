#ifndef HELPER_H_
#define HELPER_H_

// trait type that stripping const��volatile��reference
template <typename T>
struct TraitTypeStrippingCVR
{
	using RealType = std::remove_cv_t<std::remove_reference_t<T>>;
};

NAMESPACE_EVEREST_BEGIN

/*
* @brief �Ӹ�������������From���󡿵ġ�ָ��λbegin_pos����ȡ��ָ��λ��disjunction_bit_num����
* ����ÿһλ��ֵ�������������������To����
* ���磺from ֵΪ15����Ӧ�����ƣ�0000 1111
* begin_pos:1
* disjunction_bit_num:3
* �������ڵ�λ��Ҫȡ��λ0000 [111]1
* ��[111]����To���͵Ķ���
* 
* @note: From��To���������ͣ�������bool��
*
* @param [in] from From���Ͷ��� 
*
* @return ����������To����
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

// ��ȡ��ǰ�߳�id
uint64_t GetCurrentThreadId();

NAMESPACE_EVEREST_END

#endif // !HELPER_H_

