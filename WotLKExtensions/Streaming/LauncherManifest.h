#pragma once

#include <string>
#include <vector>

namespace Streaming
{
	struct ManifestFile
	{
		std::string path; // forward-slash, client-relative (e.g. "Data/patch-4.MPQ")
		long long size = 0;
		std::string sha256; // lowercase hex
		std::string quick;  // "q1:<hex>" sampled digest, MPQs only, empty means check sha256
		bool hd = false;
	};

	// An MPQ that carries a (hotmanifest), so it can be brought up to date file by file.
	struct ManifestArchive
	{
		std::string path;      // same form as ManifestFile::path
		std::string contentId; // sha256 hex of its (hotmanifest)
	};

	struct Manifest
	{
		std::string baseUrl;
		std::vector<ManifestFile> files;
		std::vector<ManifestArchive> archives;
	};

	struct LauncherConfig
	{
		std::wstring manifestUrl;
		std::string patchBaseUrl;
		std::string launcherExe;
	};

	// <installDir>/launcher.json, or baked-in defaults if it is missing.
	void LoadLauncherConfig(const std::wstring& installDir, LauncherConfig& out);

	bool ParseManifest(const std::string& json, Manifest& out);
}
