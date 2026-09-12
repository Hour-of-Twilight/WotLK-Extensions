#include "MPQScanner.h"
#include "CustomLua.h"
#include "Player.h"

#include "Streaming/BackgroundDownloader.h"
#include "Streaming/LocalDigests.h"
#include "Streaming/TextConv.h"

#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <windows.h>
namespace fs = std::filesystem;

int MpqScanner::GetMpqList(lua_State* L)
{
	sMpqScanner.ScanAsync([](const std::vector<MpqInfo>& mpqs)
	{
		if (!sPlayer.IsInWorld())
			return;
		char buffer[512];
		for (const auto& mpq : mpqs)
		{
			SStr::Printf(buffer, sizeof(buffer), "%s %s", mpq.filename_lower.c_str(),
			    mpq.updating ? "(updating)" : mpq.digest.c_str());
			CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		}
	});
	return 0;
}

namespace
{
	std::string ToLower(const std::string& s)
	{
		std::string out = s;
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c)
		{
			return std::tolower(c);
		});
		return out;
	}

	fs::path GetDataFolder()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		return fs::path(buffer).parent_path() / "Data";
	}

	// An archive that carries a (hotmanifest) is identified by its contents, because patching
	// it in place changes its bytes but not what it holds. Anything else keeps the sampled digest.
	std::string DigestOf(const std::wstring& path)
	{
		using Streaming::LocalDigests::Kind;
		std::string digest;
		if (!Streaming::LocalDigests::Get(path, Kind::ContentId, digest))
			return "";
		if (digest.empty())
			Streaming::LocalDigests::Get(path, Kind::Quick, digest);
		return digest;
	}
}

void MpqScanner::Start()
{
	sLua.RegisterFunction("GetMpqList", &GetMpqList, LuaFunctionState::FRAME);
	std::thread([this]
	{
		while (sBackgroundDownloader.IsBusy())
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ScanAsync(nullptr);
	}).detach();
}

std::vector<MpqInfo> MpqScanner::GetResults()
{
	std::lock_guard<std::mutex> lock(mutex);
	const unsigned generation = Streaming::UpdateGeneration();
	if (!done.load() || generation != scannedGeneration)
	{
		Rescan();
		scannedGeneration = generation;
		done = true;
	}
	return results;
}

void MpqScanner::ScanAsync(Callback onDone)
{
	std::lock_guard<std::mutex> lock(asyncMutex);
	if (onDone)
		waiting.push_back(std::move(onDone));
	if (asyncRunning)
		return;
	asyncRunning = true;
	asyncReady = false;
	std::thread([this]
	{
		SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
		std::vector<MpqInfo> scanned;
		try
		{
			scanned = GetResults();
		}
		catch (...)
		{
		}
		std::lock_guard<std::mutex> lock(asyncMutex);
		asyncResults = std::move(scanned);
		asyncReady = true;
		asyncRunning = false;
	}).detach();
}

void MpqScanner::Pump()
{
	std::vector<Callback> callbacks;
	std::vector<MpqInfo> scanned;
	{
		std::lock_guard<std::mutex> lock(asyncMutex);
		if (!asyncReady || waiting.empty())
			return;
		callbacks.swap(waiting);
		scanned = asyncResults;
	}
	for (Callback& cb : callbacks)
		cb(scanned);
}

void MpqScanner::Rescan()
{
	results.clear();

	fs::path dataFolder = GetDataFolder();
	std::error_code ec;
	if (!fs::exists(dataFolder, ec))
		return;

	auto scanFolder = [&](const fs::path& folder)
	{
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(folder, ec))
		{
			if (!entry.is_regular_file(ec))
				continue;

			if (Streaming::Text::LowerPath(entry.path().extension().wstring()) != L".mpq")
				continue;

			// A download parked next to the real file waiting for the swap. It isn't installed
			// yet and has no row in client_mpqs, so reporting it would only draw a warning.
			if (Streaming::IsStagedName(entry.path().filename().wstring()))
				continue;

			MpqInfo info;
			info.filename_lower = ToLower(Streaming::Text::NarrowAcp(entry.path().filename().wstring()));
			// Still reported so it counts as present, but skip the read: the copy on disk is the
			// old one and we already know it, so hashing it would only cost I/O.
			info.updating = Streaming::IsUpdatePending(entry.path().wstring());
			if (!info.updating)
				info.digest = DigestOf(entry.path().wstring());
			results.push_back(info);
		}
	};

	scanFolder(dataFolder);
	scanFolder(dataFolder / "enUS");
	scanFolder(dataFolder / "enGB");
	Streaming::LocalDigests::Save();
}
