#include "MPQScanner.h"
#include "CustomLua.h"

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <windows.h>
namespace fs = std::filesystem;

int MpqScanner::GetMpqList(lua_State* L)
{
	const std::vector<MpqInfo> mpqs = sMpqScanner.GetResults();
	char buffer[512];
	for (const auto& mpq : mpqs)
	{
		SStr::Printf(buffer, sizeof(buffer), "%s %u", mpq.filename_lower.c_str(), mpq.hash);
		CGChat::AddChatMessage(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
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

	uint32_t HashMpq(const fs::path& file)
	{
		std::ifstream in(file, std::ios::binary | std::ios::ate);
		if (!in)
			return 0;

		const uint64_t size = static_cast<uint64_t>(in.tellg());

		constexpr std::streamsize SAMPLE = 64;
		char head[SAMPLE] = {};
		char tail[SAMPLE] = {};

		in.seekg(0);
		in.read(head, SAMPLE);

		if (size > static_cast<uint64_t>(SAMPLE))
		{
			in.seekg(-SAMPLE, std::ios::end);
			in.read(tail, SAMPLE);
		}

		constexpr uint64_t FNV_PRIME = 0x00000100000001B3ULL;
		constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;

		uint64_t h = FNV_OFFSET;
		for (int i = 0; i < 8; ++i)
			h = (h ^ static_cast<uint8_t>(size >> (i * 8))) * FNV_PRIME;
		for (std::streamsize i = 0; i < SAMPLE; ++i)
			h = (h ^ static_cast<uint8_t>(head[i])) * FNV_PRIME;
		for (std::streamsize i = 0; i < SAMPLE; ++i)
			h = (h ^ static_cast<uint8_t>(tail[i])) * FNV_PRIME;

		return static_cast<uint32_t>(h ^ (h >> 32));
	}

	fs::path GetDataFolder()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		return fs::path(buffer).parent_path() / "Data";
	}
}

void MpqScanner::Start()
{
	if (done)
		return;

	sLua.RegisterFunction("GetMpqList", &GetMpqList, LuaFunctionState::FRAME);

	fs::path dataFolder = GetDataFolder();
	if (!fs::exists(dataFolder))
	{
		done = true;
		return;
	}

	auto scanFolder = [&](const fs::path& folder)
	{
		if (!fs::exists(folder))
			return;

		for (const auto& entry : fs::directory_iterator(folder))
		{
			if (!entry.is_regular_file())
				continue;

			std::string ext = ToLower(entry.path().extension().string());
			if (ext != ".mpq")
				continue;

			MpqInfo info;
			info.filename_lower = ToLower(entry.path().filename().string());
			info.hash = HashMpq(entry.path());
			results.push_back(info);
		}
	};

	scanFolder(dataFolder);
	scanFolder(dataFolder / "enUS");
	scanFolder(dataFolder / "enGB");

	done = true;
}
