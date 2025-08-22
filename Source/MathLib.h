#pragma once

#include <Platform/PlatformContext.h>

#if ZV_OS_WINDOWS
#include <ThirdParty/SimpleMath/SimpleMath.h>
using namespace DirectX::SimpleMath;
#endif

#include <cmath>

namespace ZV
{
    template<typename T> 
	static constexpr inline T min(const T a, const T b)
	{
		return (a < b) ? a : b;
	}

    template<typename T> 
	static constexpr inline T max(const T a, const T b)
	{
		return (a > b) ? a : b;
	}

    template <typename T>
    inline T cos(T x) { return std::cos(x); }

    template <typename T>
    inline T sin(T x) { return std::sin(x); }

    template <typename T>
    inline T abs(T x) { return std::abs(x); }

    template <typename T>
    inline T sqrt(T x) { return std::sqrt(x); }

    template <typename T>
    inline T exp(T x) { return std::exp(x); }

    template <typename T>
    inline T log(T x) { return std::log(x); }

    template <typename T, typename U>
    inline auto pow(T base, U exp) { return std::pow(base, exp); }
}
