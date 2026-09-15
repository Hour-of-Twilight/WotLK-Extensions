#pragma once

#include <windows.h>

namespace ModernM2
{
	inline long long TickNow()
	{
		LARGE_INTEGER v{};
		QueryPerformanceCounter(&v);
		return v.QuadPart;
	}

	inline double TickMs(long long from, long long to)
	{
		LARGE_INTEGER f{};
		QueryPerformanceFrequency(&f);
		return f.QuadPart ? static_cast<double>(to - from) * 1000.0 / static_cast<double>(f.QuadPart) : 0.0;
	}
}
