#pragma once

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
}
