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
		std::function<long long()> bytesPerSecond; // re-read every chunk, 0 or unset = unlimited
		std::function<void(long long)> onBytes;
		std::function<bool()> cancel;
		std::string* sha256Out = nullptr;
	};
	bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt);
}
