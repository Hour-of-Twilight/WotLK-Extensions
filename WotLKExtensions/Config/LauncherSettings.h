#pragma once

#include <atomic>
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
		return m_hdPatch.load();
	}

	double MaxDownloadMBps()
	{
		Load();
		return m_maxDownloadMBps.load();
	}

	long long MaxDownloadBytesPerSecond()
	{
		double mbps = MaxDownloadMBps();
		return mbps > 0.0 ? static_cast<long long>(mbps * 1024 * 1024) : 0;
	}

	int SelectedRealmId()
	{
		Load();
		return m_selectedRealmId.load();
	}

	std::string InstallDir()
	{
		Load();
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_installDir;
	}

	// Full path of the launcher exe as the player last ran it, which may be a renamed copy.
	std::string LauncherPath()
	{
		Load();
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_launcherPath;
	}

	// Written back into the launcher's settings.json.
	void SetHdPatch(bool enabled);
	void SetMaxDownloadMBps(double mbps);

	LauncherSettings(const LauncherSettings&) = delete;
	LauncherSettings& operator=(const LauncherSettings&) = delete;

private:
	LauncherSettings() = default;

	void LoadFromDisk();
	void Persist(const char* key, const std::string& rawValue);

	std::once_flag m_once;
	std::mutex m_mutex;
	std::atomic<bool> m_hdPatch{ false };
	std::atomic<double> m_maxDownloadMBps{ 0.0 };
	std::atomic<int> m_selectedRealmId{ 2 };
	std::string m_installDir;
	std::string m_launcherPath;
};

#define sLauncherSettings LauncherSettings::Instance()
