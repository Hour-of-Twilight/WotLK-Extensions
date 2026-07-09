#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct lua_State;

struct MpqInfo
{
	std::string filename_lower;
	uint32_t hash;
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
		return done;
	}

	std::vector<MpqInfo> GetResults() const
	{
		return results;
	}

	static int GetMpqList(lua_State* L);

private:
	MpqScanner() = default;

	std::vector<MpqInfo> results;
	bool done = false;
};

#define sMpqScanner MpqScanner::GetInstance()
