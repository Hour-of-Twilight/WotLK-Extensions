#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <mutex>
#include <chrono>

namespace Streaming
{
	bool HttpGetString(const std::wstring& url, std::string& out);

	class DownloadThrottle
	{
	public:
		explicit DownloadThrottle(std::function<long long()> bytesPerSecond);

		void Account(long long bytes);

	private:
		std::function<long long()> m_bytesPerSecond;
		std::mutex m_mutex;
		long long m_limit = 0;
		long long m_bytes = 0;
		std::chrono::steady_clock::time_point m_start;
	};

	struct DownloadOptions
	{
		long long resumeFrom = 0;
		DownloadThrottle* throttle = nullptr;
		std::function<void(long long)> onBytes;
		std::function<bool()> cancel;
		std::string* sha256Out = nullptr;
	};
	bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt);
}
