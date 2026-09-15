#pragma once

#include <cstddef>

// The ported sources log printf-style. Routing that through a plain varargs call, rather than a
// LogLine temporary at the call site, keeps every logging site usable inside a __try block: an
// unwindable local in the same function as a __try is a hard MSVC error (C2712), and most of the
// hot paths here are SEH-guarded.
namespace ModernM2
{
	enum class LogLevel
	{
		Error = 1,
		Warn = 2,
		Info = 3,
		Debug = 4,
	};

	void LogWrite(LogLevel level, const char* file, size_t line, const char* fmt, ...);
}

// clang-format off
#define WLOG_TRACE(...) ::ModernM2::LogWrite(::ModernM2::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define WLOG_DEBUG(...) ::ModernM2::LogWrite(::ModernM2::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define WLOG_INFO(...)  ::ModernM2::LogWrite(::ModernM2::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define WLOG_WARN(...)  ::ModernM2::LogWrite(::ModernM2::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define WLOG_ERROR(...) ::ModernM2::LogWrite(::ModernM2::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
// clang-format on
