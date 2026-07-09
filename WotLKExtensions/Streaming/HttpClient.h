#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace Streaming
{
	bool HttpGetString(const std::wstring& url, std::string& out);

	struct DownloadOptions
	{
		long long resumeFrom = 0;
		long long bytesPerSecond = 0;
		std::function<void(long long)> onBytes;
		std::function<bool()> cancel;
	};
	bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt);
}
