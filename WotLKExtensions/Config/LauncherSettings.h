#pragma once

#include <mutex>
#include <string>

class LauncherSettings
{
public:
	static LauncherSettings& Instance();

	void Load();

	bool HdPatch()
	{
		Load();
		return m_hdPatch;
	}

	long long MaxDownloadBytesPerSecond()
	{
		Load();
		return m_maxDownloadBytesPerSecond;
	}

	int SelectedRealmId()
	{
		Load();
		return m_selectedRealmId;
	}

	const std::string& InstallDir()
	{
		Load();
		return m_installDir;
	}

	LauncherSettings(const LauncherSettings&) = delete;
	LauncherSettings& operator=(const LauncherSettings&) = delete;

private:
	LauncherSettings() = default;

	void LoadFromDisk();

	std::once_flag m_once;
	bool m_hdPatch = false;
	long long m_maxDownloadBytesPerSecond = 0; // 0 = unlimited
	int m_selectedRealmId = 2;
	std::string m_installDir;
};

#define sLauncherSettings LauncherSettings::Instance()
