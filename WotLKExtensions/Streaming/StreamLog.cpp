#include "StreamLog.h"

#include <Helpers/Util.h>
#include <Logger.h>

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace Streaming
{
	namespace
	{
		constexpr std::uintmax_t kTruncateAt = 1u << 20;

		std::mutex g_mutex;
		std::ofstream g_file;
		bool g_opened = false;

		std::string Stamp()
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay, st.wHour,
			    st.wMinute, st.wSecond, st.wMilliseconds);
			return buf;
		}

		void EnsureOpen()
		{
			if (g_opened)
				return;
			g_opened = true;
			std::error_code ec;
			std::filesystem::path path = Util::GetExeDir() / "Cache" / "hotstream.log";
			std::filesystem::create_directories(path.parent_path(), ec);
			std::uintmax_t size = std::filesystem::file_size(path, ec);
			std::ios::openmode mode = std::ios::out | ((!ec && size > kTruncateAt) ? std::ios::trunc : std::ios::app);
			g_file.open(path, mode);
			if (g_file)
				g_file << Stamp() << " === client start, pid " << GetCurrentProcessId() << "\n";
		}
	}

	void StreamLog(const char* fmt, ...)
	{
		char buf[1024];
		va_list a;
		va_start(a, fmt);
		std::vsnprintf(buf, sizeof(buf), fmt, a);
		va_end(a);

		sLog.Write("DEBUG", "stream", buf);

		std::lock_guard<std::mutex> lock(g_mutex);
		EnsureOpen();
		if (g_file)
		{
			g_file << Stamp() << ' ' << buf << '\n';
			g_file.flush();
		}
	}
}
