#include "Models/Common/ModelLog.h"

#include <Logger.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace ModernM2
{
	void LogWrite(LogLevel level, const char* file, size_t line, const char* fmt, ...)
	{
#if LOG_LEVEL > 0
		if (static_cast<int>(level) > LOG_LEVEL)
			return;

		char stack[512];
		std::string heap;
		const char* text = stack;

		va_list args;
		va_start(args, fmt);
		const int needed = std::vsnprintf(stack, sizeof stack, fmt, args);
		va_end(args);

		if (needed < 0)
		{
			text = fmt ? fmt : "";
		}
		else if (static_cast<size_t>(needed) >= sizeof stack)
		{
			heap.resize(static_cast<size_t>(needed));
			va_start(args, fmt);
			std::vsnprintf(heap.data(), heap.size() + 1, fmt, args);
			va_end(args);
			text = heap.c_str();
		}

		const char* type = "INFO";
		switch (level)
		{
			case LogLevel::Error: type = "ERROR"; break;
			case LogLevel::Warn: type = "WARN"; break;
			case LogLevel::Info: type = "INFO"; break;
			case LogLevel::Debug: type = "DEBUG"; break;
		}
		LogLine(type, file, line) << text;
#else
		(void)level;
		(void)file;
		(void)line;
		(void)fmt;
#endif
	}
}
