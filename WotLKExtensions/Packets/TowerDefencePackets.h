#pragma once

#include <Defines.h>

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

	static constexpr uint32 MAX_SCOREBOARD_ROWS = 64;
	static constexpr uint32 MAX_SAVED_GAME_ROWS = 32;
	static constexpr uint32 MAX_TOWER_ROWS = 64;

	struct ScoreboardRow
	{
		std::string name;
		uint32 score = 0;
		uint32 wave = 0;
		uint32 money = 0;
		uint32 lives = 0;
		std::string difficulty;
		std::string playDate;
		bool highlight = false;
	};

	struct SavedGameRow
	{
		uint32 id = 0;
		uint32 mode = 0;
		uint32 wave = 0;
		uint32 lives = 0;
		uint32 difficulty = 0;
	};

	struct TowerRow
	{
		uint32 towerSet = 0;
		uint32 towerId = 0;
		uint32 cost = 0;
		std::string name;
		std::string icon;
	};

	static void Handler_SMSG_TOWERDEFENCE_SCOREBOARD(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_SAVED_GAMES(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_MONEY(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_WAVE(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_LIVES(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_SCORE(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_GAME_END(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_TOWERDEFENCE_TOWER_LIST(void*, uint32, uint32, CDataStore* pkt);

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
	uint32 m_mode = 0;
	bool m_hasData = false;

	std::vector<SavedGameRow> m_saves;
	bool m_hasSaves = false;

	std::vector<TowerRow> m_towers;
	bool m_hasTowers = false;
};

#define sTowerDefencePackets TowerDefencePackets::Instance()
