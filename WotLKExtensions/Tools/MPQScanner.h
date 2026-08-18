#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

struct lua_State;

struct MpqInfo
{
	std::string filename_lower;
	std::string digest;    // "q1:<hex>" sampled digest, empty when the file could not be read
	bool updating = false; // the downloader is replacing this one, so its digest proves nothing
};

class MpqScanner
{
public:
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

	static int GetMpqList(lua_State* L);

private:
	MpqScanner() = default;

	// Size and mtime of the file the digest was taken from, so a rescan only pays for the archives
	// the downloader actually replaced.
	struct CachedDigest
	{
		long long size;
		long long mtime;
		std::string digest;
	};

	void Rescan();
	std::string DigestOf(const std::wstring& path);

	std::mutex mutex;
	std::vector<MpqInfo> results;
	std::map<std::wstring, CachedDigest> digests;
	std::atomic<bool> done{ false };
	unsigned scannedGeneration = 0;
};

#define sMpqScanner MpqScanner::GetInstance()
