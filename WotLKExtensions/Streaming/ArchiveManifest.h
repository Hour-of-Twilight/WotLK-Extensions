#pragma once

#include <string>
#include <vector>

namespace Streaming
{
	struct ArchiveEntry
	{
		std::string name;
		std::string key;
		long long size = 0;
		std::string sha256;
	};

	struct ArchiveDiff
	{
		std::vector<ArchiveEntry> changed;
		std::vector<std::string> removed;
		long long changedBytes = 0;

		bool Empty() const
		{
			return changed.empty() && removed.empty();
		}
	};

	// The file list an MPQ carries about itself as "(hotmanifest)": a "hotmanifest1" header line,
	// then "<sha256> <size> <name>" per file sorted by the lowercased name, UTF-8 with LF ends.
	// The contentId of an archive is the SHA-256 of those exact bytes. The patch wranglers in
	// HoTEmulator write it and HoTLauncher.Shared/ArchiveManifest.cs is the twin of this file.
	namespace ArchiveManifest
	{
		constexpr const char* kFileName = "(hotmanifest)";
		constexpr const char* kHeader = "hotmanifest1";
		constexpr const char* kIndexExtension = ".hotmanifest";

		std::string NormalizeName(std::string name);
		std::string KeyOf(const std::string& name);
		bool IsInternalName(const std::string& name);
		bool IsContentId(const std::string& s);

		bool Parse(const std::string& bytes, std::vector<ArchiveEntry>& out, std::string* error = nullptr);
		std::string ContentIdOf(const std::string& bytes);

		ArchiveDiff Diff(const std::vector<ArchiveEntry>& local, const std::vector<ArchiveEntry>& target);
	}
}
