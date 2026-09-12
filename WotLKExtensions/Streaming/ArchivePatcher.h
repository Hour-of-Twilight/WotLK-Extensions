#pragma once

#include <string>
#include <vector>

namespace Streaming
{
	struct ArchiveWrite
	{
		std::string name;
		std::wstring blobPath;
		long long size = 0;
		std::string sha256;
	};

	struct ArchivePatchJob
	{
		std::wstring archivePath;
		std::string relPath;
		std::vector<ArchiveWrite> writes;
		std::string targetManifest;
		std::string contentId;
	};

	struct ArchivePatchResult
	{
		bool ok = false;
		std::vector<std::string> writtenNames;
		std::string error;
	};

	// Writes a downloaded batch of files into an archive through the client's own mopaq layer.
	// Main thread only: it swaps the mounted archive's handle for a writable one, writes, then
	// closes to commit the tables and reopens read-only.
	namespace ArchivePatcher
	{
		ArchivePatchResult Apply(const ArchivePatchJob& job);
	}
}
