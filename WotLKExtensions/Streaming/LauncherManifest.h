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
		bool hd = false;
	};

	struct Manifest
	{
		std::string baseUrl;
		std::vector<ManifestFile> files;
	};

	struct LauncherSettings
	{
		bool hdPatch = false;
		long long maxBytesPerSecond = 0; // 0 = unlimited
	};

	// <installDir>/launcher.json -> manifest URL + patch base URL (defaults if missing).
	void LoadLauncherUrls(const std::wstring& installDir, std::wstring& manifestUrl, std::string& patchBaseUrl);

	// <installDir>/.hotlauncher/settings.json
	LauncherSettings LoadLauncherSettings(const std::wstring& installDir);

	bool ParseManifest(const std::string& json, Manifest& out);
}
