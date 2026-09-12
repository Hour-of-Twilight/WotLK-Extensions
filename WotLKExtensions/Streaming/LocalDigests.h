#pragma once

#include <string>

namespace Streaming::LocalDigests
{
	enum class Kind
	{
		Quick,
		Sha256,
		ContentId
	};

	bool Get(const std::wstring& path, Kind kind, std::string& out);
	void Remember(const std::wstring& path, Kind kind, const std::string& digest);
	void Save();
}
