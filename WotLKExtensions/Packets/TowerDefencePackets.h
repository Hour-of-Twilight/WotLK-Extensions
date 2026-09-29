#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct lua_State;
struct CDataStore;

class TowerDefencePackets
{
public:
	static TowerDefencePackets& Instance();

	TowerDefencePackets(const TowerDefencePackets&) = delete;
	TowerDefencePackets& operator=(const TowerDefencePackets&) = delete;

	void Apply();
	void Reset();

private:
	TowerDefencePackets() = default;

	static constexpr uint32_t MAX_SCOREBOARD_ROWS = 64;
	static constexpr uint32_t MAX_SAVED_GAME_ROWS = 32;
	static constexpr uint32_t MAX_TOWER_ROWS = 64;

	struct ScoreboardRow
	{
		std::string name;
		uint32_t score = 0;
		uint32_t wave = 0;
		uint32_t money = 0;
		uint32_t lives = 0;
		std::string difficulty;
		std::string playDate;
		bool highlight = false;
	};

	struct SavedGameRow
	{
		uint32_t id = 0;
		uint32_t mode = 0;
		uint32_t wave = 0;
		uint32_t lives = 0;
		uint32_t difficulty = 0;
	};

	struct TowerRow
	{
		uint32_t towerSet = 0;
		uint32_t towerId = 0;
		uint32_t cost = 0;
		std::string name;
		std::string icon;
	};

	static void Handler_SMSG_TOWERDEFENCE_SCOREBOARD(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_SAVED_GAMES(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_MONEY(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_WAVE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_LIVES(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_SCORE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_GAME_END(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_TOWER_LIST(void*, uint32_t, uint32_t, CDataStore* pkt);

	static void SignalCounter(CDataStore* pkt, const char* eventName);

	static int RequestTowerDefenceScoreboard(lua_State* L);
	static int GetTowerDefenceScoreboard(lua_State* L);
	static int RequestTowerDefenceSavedGames(lua_State* L);
	static int GetTowerDefenceSavedGames(lua_State* L);
	static int TowerDefenceSpawnTower(lua_State* L);
	static int TowerDefenceCancelTower(lua_State* L);
	static int TowerDefenceConfirmTower(lua_State* L);
	static int TowerDefenceStartWave(lua_State* L);
	static int TowerDefenceStartGame(lua_State* L);
	static int TowerDefenceLoadGame(lua_State* L);
	static int TowerDefenceDeleteGame(lua_State* L);
	static int TowerDefenceEndGame(lua_State* L);
	static int RequestTowerDefenceTowers(lua_State* L);
	static int GetTowerDefenceTowers(lua_State* L);

	std::vector<ScoreboardRow> m_rows;
	uint32_t m_mode = 0;
	bool m_hasData = false;

	std::vector<SavedGameRow> m_saves;
	bool m_hasSaves = false;

	std::vector<TowerRow> m_towers;
	bool m_hasTowers = false;
};

#define sTowerDefencePackets TowerDefencePackets::Instance()
