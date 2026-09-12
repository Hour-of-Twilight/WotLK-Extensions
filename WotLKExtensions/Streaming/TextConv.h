#pragma once

#include <windows.h>

#include <string>

namespace Streaming::Text
{
	inline std::wstring Widen(UINT codePage, const std::string& s)
	{
		if (s.empty())
			return L"";
		int n = MultiByteToWideChar(codePage, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring out(n, L'\0');
		MultiByteToWideChar(codePage, 0, s.c_str(), (int)s.size(), out.data(), n);
		return out;
	}

	inline std::string Narrow(UINT codePage, const std::wstring& s)
	{
		if (s.empty())
			return "";
		int n = WideCharToMultiByte(codePage, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
		std::string out(n, '\0');
		WideCharToMultiByte(codePage, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr);
		return out;
	}

	inline std::wstring WidenAcp(const std::string& s)
	{
		return Widen(CP_ACP, s);
	}

	inline std::string NarrowAcp(const std::wstring& s)
	{
		return Narrow(CP_ACP, s);
	}

	inline std::wstring WidenUtf8(const std::string& s)
	{
		return Widen(CP_UTF8, s);
	}

	inline std::string NarrowUtf8(const std::wstring& s)
	{
		return Narrow(CP_UTF8, s);
	}

	inline std::wstring LowerPath(std::wstring p)
	{
		for (wchar_t& c : p)
		{
			if (c == L'/')
				c = L'\\';
			else if (c >= L'A' && c <= L'Z')
				c = (wchar_t)(c + 32);
		}
		return p;
	}

	inline std::string LowerAscii(std::string s)
	{
		for (char& c : s)
			if (c >= 'A' && c <= 'Z')
				c = (char)(c + 32);
		return s;
	}
}
