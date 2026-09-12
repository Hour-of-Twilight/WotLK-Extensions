#pragma once

#include <string>

namespace Streaming::MpqFile
{
	// Reads one named file straight out of an MPQ on disk, without touching the client's list of
	// mounted archives, so it is safe from any thread and works on archives the client never
	// opened. Sectors are decoded with the client's own decompressor, so anything the client or
	// StormLib wrote is readable.
	bool ReadFile(const std::wstring& archivePath, const char* name, std::string& out, std::string* error = nullptr,
	    bool* unreadable = nullptr);

	// SHA-256 of the archive's (hotmanifest), or "" when it has none. False only when the archive
	// could not be read at all.
	bool ContentId(const std::wstring& archivePath, std::string& out);
}
