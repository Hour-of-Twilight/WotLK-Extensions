#pragma once

#include <Helpers/Util.h>

#include <filesystem>

namespace Streaming::Paths
{
	inline std::filesystem::path LogsDir()
	{
		return Util::GetExeDir() / "Logs";
	}

	inline std::filesystem::path WorkDir()
	{
		return Util::GetExeDir() / ".hotlauncher";
	}

	inline std::filesystem::path DownloadsDir()
	{
		return WorkDir() / "downloads";
	}

	inline std::filesystem::path StagedDir()
	{
		return WorkDir() / "staged";
	}

	inline std::filesystem::path BlobsDir()
	{
		return WorkDir() / "blobs";
	}

	inline std::filesystem::path LegacyCacheDir()
	{
		return Util::GetExeDir() / "Cache";
	}
}
