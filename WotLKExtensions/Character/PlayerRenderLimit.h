#pragma once

#include <Defines.h>

#include <cstdint>
#include <vector>

#include <ankerl/unordered_dense.h>

namespace ClientData
{
	class CGObject_C;
}

class PlayerRenderLimit
{
public:
	static PlayerRenderLimit& Instance();

	PlayerRenderLimit(const PlayerRenderLimit&) = delete;
	PlayerRenderLimit& operator=(const PlayerRenderLimit&) = delete;

	void SetLimit(int maxPlayers);
	int Limit() const
	{
		return m_limit;
	}

	void Tick();
	void Reset();

	bool IsHidden(uint64 guid) const
	{
		return !m_hidden.empty() && m_hidden.contains(guid);
	}
	bool IsHiddenUnit(const void* unit) const;
	bool ShouldSuppressComponent(const void* unit) const;
	void SuppressFrame(void* unit) const;
	size_t HiddenCount() const
	{
		return m_hidden.size();
	}
	size_t ReleasedCount() const
	{
		return m_released;
	}

private:
	PlayerRenderLimit() = default;

	enum class Relation : uint8
	{
		Target,
		Party,
		Raid,
		Friend,
		Guild,
		Stranger,
	};

	struct Candidate
	{
		uint64 guid;
		Relation relation;
		float distance;
	};

	struct HiddenState
	{
		uint32 hiddenSinceMs = 0;
		bool released = false;
	};

	void Evaluate(uint32 now);
	void CollectRelations();
	void ReleaseStale(uint32 now);
	static bool ReleaseUnit(void* unit);

	static int __cdecl CollectCallback(uint32 guidLow, uint32 guidHigh, void* param);

	int m_limit = 0;
	uint32 m_nextEvaluateMs = 0;
	size_t m_released = 0;
	ankerl::unordered_dense::map<uint64, Relation> m_relations;
	ankerl::unordered_dense::map<uint64, HiddenState> m_hidden;
	ankerl::unordered_dense::map<uint64, HiddenState> m_nextHidden;
	std::vector<Candidate> m_candidates;
	struct
	{
		ClientData::CGObject_C* self = nullptr;
		uint64 selfGuid = 0;
		uint64 targetGuid = 0;
		uint32 guildId = 0;
	} m_scan;
};

#define sPlayerRenderLimit PlayerRenderLimit::Instance()
