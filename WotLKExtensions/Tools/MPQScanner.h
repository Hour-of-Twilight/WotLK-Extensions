#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

struct lua_State;

struct MpqInfo
{
	std::string filename_lower;
	std::string digest;    // contentId of its (hotmanifest), else "q1:<hex>" sampled digest, empty when unreadable
	bool updating = false; // the downloader is replacing this one, so its digest proves nothing
};

class MpqScanner
{
public:
	using Callback = std::function<void(const std::vector<MpqInfo>&)>;

	static MpqScanner& GetInstance()
	{
		static MpqScanner instance;
		return instance;
	}

	MpqScanner(const MpqScanner&) = delete;
	MpqScanner& operator=(const MpqScanner&) = delete;

	void Start();

	bool IsDone() const
	{
		return done.load();
	}

	// Scans on first use and after the downloader changes anything, so the caller never has to
	// think about ordering against the background pass.
	std::vector<MpqInfo> GetResults();

	void ScanAsync(Callback onDone);
	void Pump();

	static int GetMpqList(lua_State* L);

private:
	MpqScanner() = default;

	void Rescan();

	std::mutex mutex;
	std::vector<MpqInfo> results;
	std::atomic<bool> done{ false };
	unsigned scannedGeneration = 0;

	std::mutex asyncMutex;
	std::vector<Callback> waiting;
	std::vector<MpqInfo> asyncResults;
	bool asyncReady = false;
	bool asyncRunning = false;
};

#define sMpqScanner MpqScanner::GetInstance()
