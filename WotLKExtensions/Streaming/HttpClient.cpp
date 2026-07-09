#include "HttpClient.h"

#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <vector>
#include <chrono>
#include <thread>

namespace Streaming
{
	namespace
	{
		struct Conn
		{
			HINTERNET session = nullptr;
			HINTERNET connect = nullptr;
			HINTERNET request = nullptr;
			~Conn()
			{
				if (request)
					WinHttpCloseHandle(request);
				if (connect)
					WinHttpCloseHandle(connect);
				if (session)
					WinHttpCloseHandle(session);
			}
		};

		bool OpenGet(Conn& c, const std::wstring& url, long long resumeFrom)
		{
			URL_COMPONENTS uc{};
			uc.dwStructSize = sizeof(uc);
			wchar_t host[256] = {}, path[8192] = {}, extra[4096] = {};
			uc.lpszHostName = host;
			uc.dwHostNameLength = _countof(host);
			uc.lpszUrlPath = path;
			uc.dwUrlPathLength = _countof(path);
			uc.lpszExtraInfo = extra;
			uc.dwExtraInfoLength = _countof(extra);
			if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
				return false;

			const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
			std::wstring fullPath = std::wstring(path) + extra;

			c.session = WinHttpOpen(L"HoTStreaming/1.0",
			    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!c.session)
				return false;

			WinHttpSetTimeouts(c.session, 15000, 15000, 30000, 30000);

			c.connect = WinHttpConnect(c.session, host, uc.nPort, 0);
			if (!c.connect)
				return false;

			c.request = WinHttpOpenRequest(c.connect, L"GET", fullPath.c_str(),
			    nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			    secure ? WINHTTP_FLAG_SECURE : 0);
			if (!c.request)
				return false;

			if (resumeFrom > 0)
			{
				wchar_t range[64];
				swprintf(range, _countof(range), L"Range: bytes=%lld-", resumeFrom);
				WinHttpAddRequestHeaders(c.request, range, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
			}
			return true;
		}

		bool SendAndStatus(Conn& c, DWORD& status)
		{
			if (!WinHttpSendRequest(c.request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			        WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
				return false;
			if (!WinHttpReceiveResponse(c.request, nullptr))
				return false;
			DWORD size = sizeof(status);
			if (!WinHttpQueryHeaders(c.request,
			        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
				return false;
			return true;
		}
	}

	bool HttpGetString(const std::wstring& url, std::string& out)
	{
		out.clear();
		Conn c;
		if (!OpenGet(c, url, 0))
			return false;
		DWORD status = 0;
		if (!SendAndStatus(c, status) || status < 200 || status >= 300)
			return false;

		for (;;)
		{
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(c.request, &avail))
				return false; // connection error mid-stream
			if (avail == 0)
				break; // clean end of response
			std::vector<char> buf(avail);
			DWORD read = 0;
			if (!WinHttpReadData(c.request, buf.data(), avail, &read))
				return false;
			if (read == 0)
				break;
			out.append(buf.data(), read);
		}
		return true;
	}

	bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt)
	{
		Conn c;
		if (!OpenGet(c, url, opt.resumeFrom))
			return false;
		DWORD status = 0;
		if (!SendAndStatus(c, status))
			return false;

		const bool resuming = (opt.resumeFrom > 0 && status == 206);
		if (status != 200 && status != 206)
			return false;

		std::ofstream f(destPath, std::ios::binary | (resuming ? std::ios::app : std::ios::trunc));
		if (!f)
			return false;

		long long written = resuming ? opt.resumeFrom : 0;
		auto start = std::chrono::steady_clock::now();
		long long throttled = 0;
		std::vector<char> buf(1 << 20);

		for (;;)
		{
			if (opt.cancel && opt.cancel())
				return false;
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(c.request, &avail))
				return false; // connection dropped mid-download, don't pass off a truncated file
			if (avail == 0)
				break; // clean end of response
			DWORD want = avail < buf.size() ? avail : (DWORD)buf.size();
			DWORD read = 0;
			if (!WinHttpReadData(c.request, buf.data(), want, &read))
				return false;
			if (read == 0)
				break;
			f.write(buf.data(), read);
			if (!f)
				return false;
			written += read;
			if (opt.onBytes)
				opt.onBytes(written);

			if (opt.bytesPerSecond > 0)
			{
				throttled += read;
				double aheadSec = (double)throttled / opt.bytesPerSecond - std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
				if (aheadSec > 0.005)
				{
					double s = aheadSec < 0.5 ? aheadSec : 0.5;
					std::this_thread::sleep_for(std::chrono::duration<double>(s));
				}
			}
		}
		return true;
	}
}
