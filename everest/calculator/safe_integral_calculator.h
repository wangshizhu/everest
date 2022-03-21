#ifndef SAFE_INTEGRAL_CALCULATOR_H_
#define SAFE_INTEGRAL_CALCULATOR_H_

#include <limits>
#include <map>
#include <type_traits>

template <typename T>
auto Abs(T&& t)noexcept
{
	using RealType = typename TraitTypeStrippingCVR<T>::RealType;
	using UnsignedRealType = typename std::make_unsigned<RealType>::type;
	if (t >= 0)
	{
		return UnsignedRealType(t);
	}

	constexpr RealType min_val = (std::numeric_limits<RealType>::min)();

	if (t == min_val)
	{
		return UnsignedRealType(t);
	}

	return UnsignedRealType((~t) + 1);
}

namespace everest
{
	using SignedInt64 = long long;
	using UnsignedInt64 = unsigned long long;

	class SafeIntegralCalculator
	{
	private:
		template<typename T1, typename T2,
			typename = typename std::enable_if_t
			<
			std::is_integral_v<typename TraitTypeStrippingCVR<T1>::RealType>&&
			std::is_integral_v<typename TraitTypeStrippingCVR<T2>::RealType> &&
			!std::is_same<typename TraitTypeStrippingCVR<T1>::RealType, bool>::value &&
			!std::is_same<typename TraitTypeStrippingCVR<T2>::RealType, bool>::value
			>
		>
			static auto SafeSum(T1&& augend, T2&& addend) noexcept
		{
			using T1RealType = typename TraitTypeStrippingCVR<T1>::RealType;
			using UnsignedT1RealType = typename std::make_unsigned<T1RealType>::type;
			using T2RealType = typename TraitTypeStrippingCVR<T2>::RealType;
			using UnsignedT2RealType = typename std::make_unsigned<T2RealType>::type;

			constexpr T1RealType max_val = std::numeric_limits<T1RealType>::max();
			constexpr T1RealType min_val = std::numeric_limits<T1RealType>::min();

			if (addend == 0)
			{
				return augend;
			}

			// add
			if (addend > 0)
			{
				// char a = 125;
				// int b = 1000;
				// char sum = (a + b) = 127
				if (augend >= 0)
				{
					T1RealType tmp = max_val - augend;
					if (addend >= tmp)
					{
						return max_val;
					}
				}
				// char a = -128;
				// int b = 1000;
				// char sum = (a + b) = 127
				else
				{
					UnsignedT1RealType t1_abs = Abs(augend);
					if (addend >= (max_val + t1_abs))
					{
						return max_val;
					}
				}
			}
			// sub
			else
			{
				// char a = -100;
				// char b = -125;
				// char sum = (a + b) = -128
				if (augend < 0)
				{
					// char a = -10;
					// int b = -128;
					// char sum = (a + b) = -128
					UnsignedT1RealType t1_abs = Abs(augend);
					T1RealType expected = t1_abs + min_val;
					if (addend <= expected)
					{
						return min_val;
					}
				}
				else
				{
					// unsigned char a = 255;
					// int b = -1000;
					// char sum = (a + b) = -128
					UnsignedT1RealType unsigned_t1 = augend;
					UnsignedT1RealType t1_min_abs = Abs(min_val);
					UnsignedT1RealType expected = unsigned_t1 + t1_min_abs;

					UnsignedT2RealType t2_abs = Abs(addend);
					if (t2_abs >= expected)
					{
						return min_val;
					}
				}
			}

			return T1RealType(augend + addend);
		}

		template<typename T1, typename T2,
			typename = typename std::enable_if_t
			<
			std::is_integral_v<typename TraitTypeStrippingCVR<T1>::RealType>&&
			std::is_integral_v<typename TraitTypeStrippingCVR<T2>::RealType> &&
			!std::is_same<typename TraitTypeStrippingCVR<T1>::RealType, bool>::value &&
			!std::is_same<typename TraitTypeStrippingCVR<T2>::RealType, bool>::value
			>
		>
			static auto SafeMultiply(T1&& multiplicand, T2&& multiplier) noexcept
		{
			using T1RealType = typename TraitTypeStrippingCVR<T1>::RealType;
			using UnsignedT1RealType = typename std::make_unsigned<T1RealType>::type;
			using T2RealType = typename TraitTypeStrippingCVR<T2>::RealType;
			using UnsignedT2RealType = typename std::make_unsigned<T2RealType>::type;

			constexpr T1RealType max_val = std::numeric_limits<T1RealType>::max();
			constexpr T1RealType min_val = std::numeric_limits<T1RealType>::min();

			if (multiplicand == 0 || multiplier == 0)
			{
				return T1RealType(0);
			}

			if (multiplier > 0)
			{
				// char a = 125;
				// int b = 1000;
				// char sum = (a * b) = 127
				if (multiplicand >= 0)
				{
					auto multi = max_val / multiplicand;
					if (multiplier > multi)
					{
						return max_val;
					}
				}
				// char a = -2;
				// int b = 1000;
				// char sum = (a * b) = -128
				else
				{
					auto multi = min_val / multiplicand;
					if (multiplier > multi)
					{
						return min_val;
					}
				}
			}
			else
			{
				if (multiplicand < 0)
				{
					// char a = -128;
					// int b = -128;
					// char sum = (a * b) = 127
					UnsignedT1RealType t1_abs = Abs(multiplicand);
					UnsignedT2RealType t2_abs = Abs(multiplier);
					auto expected_multi = max_val / t1_abs;
					if (t2_abs > expected_multi)
					{
						return max_val;
					}
				}
				else
				{
					// unsigned char a = 200;
					// char a = 64;
					// int b = -1000;
					// char sum = (a * b) = -128
					T1RealType expected_multi = min_val / multiplicand;
					if (multiplier < expected_multi)
					{
						return min_val;
					}
				}
			}

			return T1RealType(multiplicand * multiplier);
		}

		template<typename T1, typename T2,
			typename = typename std::enable_if_t
			<
			std::is_integral_v<typename TraitTypeStrippingCVR<T1>::RealType>&&
			std::is_integral_v<typename TraitTypeStrippingCVR<T2>::RealType> &&
			!std::is_same<typename TraitTypeStrippingCVR<T1>::RealType, bool>::value &&
			!std::is_same<typename TraitTypeStrippingCVR<T2>::RealType, bool>::value
			>
		>
			static auto SafeDivide(T1&& dividend, T2&& divisor) throw()
		{
			using T1RealType = typename TraitTypeStrippingCVR<T1>::RealType;
			using UnsignedT1RealType = typename std::make_unsigned<T1RealType>::type;
			using T2RealType = typename TraitTypeStrippingCVR<T2>::RealType;
			using UnsignedT2RealType = typename std::make_unsigned<T2RealType>::type;

			constexpr T1RealType max_val = std::numeric_limits<T1RealType>::max();
			constexpr T1RealType min_val = std::numeric_limits<T1RealType>::min();

			if (dividend == 0)
			{
				return T1RealType(0);
			}

			if (divisor == 0)
			{
				throw std::invalid_argument("bad divisor,it is 0");
			}

			// note: when dividend is unsigned and divisor is less 0,then the result is 0
			if (std::is_unsigned_v<T1> && divisor < 0)
			{
				// unsigned char a = 12;
				// char b = -1;
				// a /= b;
				return T1RealType(0);
			}

			if (dividend < 0 && divisor < 0)
			{
				// char a = -128;
				// char b = -1;
				// a /= b;
				UnsignedT1RealType t1_abs = Abs(dividend);
				UnsignedT2RealType t2_abs = Abs(divisor);
				UnsignedT1RealType real_reasult = t1_abs / t2_abs;
				if (real_reasult > max_val)
				{
					return max_val;
				}
			}

			return T1RealType(dividend / divisor);

		}

	public:
		template<typename T1, typename T2>
		static void SafeAutoIncrease(T1&& augend, T2&& addend)
		{
			augend = SafeSum(std::forward<T1>(augend), std::forward<T2>(addend));
		}

		template<typename T1, typename T2>
		static void SafeAutoMultiply(T1&& multiplicand, T2&& multiplier)
		{
			multiplicand = SafeMultiply(std::forward<T1>(multiplicand), std::forward<T2>(multiplier));
		}

		template<typename T1, typename T2>
		static void SafeAutoDivide(T1&& dividend, T2&& divisor)
		{
			dividend = SafeDivide(std::forward<T1>(dividend), std::forward<T2>(divisor));
		}
	};
}

#endif // !SAFE_INTEGRAL_CALCULATOR_H_