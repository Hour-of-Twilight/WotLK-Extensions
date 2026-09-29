#include "TowerDefencePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

TowerDefencePackets& TowerDefencePackets::Instance()
{
	static TowerDefencePackets instance;
	return instance;
}

void TowerDefencePackets::Reset()
{
	m_rows.clear();
	m_mode = 0;
	m_hasData = false;
	m_saves.clear();
	m_hasSaves = false;
	m_towers.clear();
	m_hasTowers = false;
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_SCOREBOARD(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	TowerDefencePackets& self = Instance();

	{
		Packet r(pkt);
		char buffer[128];

		self.m_rows.clear();
		self.m_mode = r.GetUInt32();

		uint32_t count = r.GetUInt32();
		if (count > MAX_SCOREBOARD_ROWS)
			count = MAX_SCOREBOARD_ROWS;

		self.m_rows.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			ScoreboardRow row;
			r.GetString(buffer, sizeof(buffer));
			row.name = buffer;
			row.score = r.GetUInt32();
			row.wave = r.GetUInt32();
			row.money = r.GetUInt32();
			row.lives = r.GetUInt32();
			r.GetString(buffer, sizeof(buffer));
			row.difficulty = buffer;
			r.GetString(buffer, sizeof(buffer));
			row.playDate = buffer;
			row.highlight = r.GetUInt8() != 0;
			self.m_rows.push_back(std::move(row));
		}

		self.m_hasData = true;
	}

	FrameXMLExtensions::SignalEvent("HOT_TOWERDEFENCE_SCOREBOARD", "");
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_SAVED_GAMES(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	TowerDefencePackets& self = Instance();
	uint32_t openUI = 0;

	{
		Packet r(pkt);

		self.m_saves.clear();
		openUI = r.GetUInt8();

		uint32_t count = r.GetUInt32();
		if (count > MAX_SAVED_GAME_ROWS)
			count = MAX_SAVED_GAME_ROWS;

		self.m_saves.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			SavedGameRow row;
			row.id = r.GetUInt32();
			row.mode = r.GetUInt32();
			row.wave = r.GetUInt32();
			row.lives = r.GetUInt32();
			row.difficulty = r.GetUInt32();
			self.m_saves.push_back(row);
		}

		self.m_hasSaves = true;
	}

	FrameXMLExtensions::SignalEvent("HOT_TOWERDEFENCE_SAVED_GAMES", "%u", openUI);
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_TOWER_LIST(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	TowerDefencePackets& self = Instance();

	{
		Packet r(pkt);
		char buffer[128];

		self.m_towers.clear();

		uint32_t count = r.GetUInt32();
		if (count > MAX_TOWER_ROWS)
			count = MAX_TOWER_ROWS;

		self.m_towers.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			TowerRow row;
			row.towerSet = r.GetUInt32();
			row.towerId = r.GetUInt32();
			row.cost = r.GetUInt32();
			r.GetString(buffer, sizeof(buffer));
			row.name = buffer;
			r.GetString(buffer, sizeof(buffer));
			row.icon = buffer;
			self.m_towers.push_back(std::move(row));
		}

		self.m_hasTowers = true;
	}

	FrameXMLExtensions::SignalEvent("HOT_TOWERDEFENCE_TOWERS", "");
}

void TowerDefencePackets::SignalCounter(CDataStore* pkt, const char* eventName)
{
	uint32_t amount = 0;

	{
		Packet r(pkt);
		amount = r.GetUInt32();
	}

	FrameXMLExtensions::SignalEvent(eventName, "%u", amount);
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_MONEY(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	SignalCounter(pkt, "HOT_TOWERDEFENCE_MONEY");
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_WAVE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	SignalCounter(pkt, "HOT_TOWERDEFENCE_WAVE");
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_LIVES(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	SignalCounter(pkt, "HOT_TOWERDEFENCE_LIVES");
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_SCORE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	SignalCounter(pkt, "HOT_TOWERDEFENCE_SCORE");
}

void TowerDefencePackets::Handler_SMSG_TOWERDEFENCE_GAME_END(void*, uint32_t, uint32_t, CDataStore*)
{
	FrameXMLExtensions::SignalEvent("HOT_TOWERDEFENCE_GAME_END", "");
}

int TowerDefencePackets::RequestTowerDefenceScoreboard(lua_State* L)
{
	uint32_t mode = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	Packet(CMSG_TOWERDEFENCE_SCOREBOARD_REQUEST).PutUInt32(mode).Send();
	return 0;
}

// Returns mode, rows. Each row is an array of display values in column order:
// name, score, wave, money, lives, difficulty, play date, and a trailing true
// on the run the player has just finished.
int TowerDefencePackets::GetTowerDefenceScoreboard(lua_State* L)
{
	TowerDefencePackets& self = Instance();
	if (!self.m_hasData)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_mode);

	FrameScript::CreateTable(L, (int)self.m_rows.size(), 0);
	int tbl = FrameScript::GetTop(L);

	for (int i = 0; i < (int)self.m_rows.size(); ++i)
	{
		ScoreboardRow const& entry = self.m_rows[i];

		FrameScript::CreateTable(L, entry.highlight ? 8 : 7, 0);
		int row = FrameScript::GetTop(L);

		FrameScript::PushString(L, entry.name.c_str());
		FrameScript::RawSetI(L, row, 1);

		FrameScript::PushNumber(L, entry.score);
		FrameScript::RawSetI(L, row, 2);

		FrameScript::PushNumber(L, entry.wave);
		FrameScript::RawSetI(L, row, 3);

		FrameScript::PushNumber(L, entry.money);
		FrameScript::RawSetI(L, row, 4);

		FrameScript::PushNumber(L, entry.lives);
		FrameScript::RawSetI(L, row, 5);

		FrameScript::PushString(L, entry.difficulty.c_str());
		FrameScript::RawSetI(L, row, 6);

		FrameScript::PushString(L, entry.playDate.c_str());
		FrameScript::RawSetI(L, row, 7);

		if (entry.highlight)
		{
			FrameScript::PushBoolean(L, 1);
			FrameScript::RawSetI(L, row, 8);
		}

		FrameScript::RawSetI(L, tbl, i + 1);
	}

	FrameScript::SetTop(L, tbl);
	return 2;
}

int TowerDefencePackets::RequestTowerDefenceSavedGames(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_SAVED_GAMES).Send();
	return 0;
}

// Returns the saved games as an array sorted by id. Each row is
// { id, mode, wave, lives, difficulty }.
int TowerDefencePackets::GetTowerDefenceSavedGames(lua_State* L)
{
	TowerDefencePackets& self = Instance();
	if (!self.m_hasSaves)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::CreateTable(L, (int)self.m_saves.size(), 0);
	int tbl = FrameScript::GetTop(L);

	for (int i = 0; i < (int)self.m_saves.size(); ++i)
	{
		SavedGameRow const& entry = self.m_saves[i];

		FrameScript::CreateTable(L, 5, 0);
		int row = FrameScript::GetTop(L);

		FrameScript::PushNumber(L, entry.id);
		FrameScript::RawSetI(L, row, 1);

		FrameScript::PushNumber(L, entry.mode);
		FrameScript::RawSetI(L, row, 2);

		FrameScript::PushNumber(L, entry.wave);
		FrameScript::RawSetI(L, row, 3);

		FrameScript::PushNumber(L, entry.lives);
		FrameScript::RawSetI(L, row, 4);

		FrameScript::PushNumber(L, entry.difficulty);
		FrameScript::RawSetI(L, row, 5);

		FrameScript::RawSetI(L, tbl, i + 1);
	}

	FrameScript::SetTop(L, tbl);
	return 1;
}

int TowerDefencePackets::TowerDefenceSpawnTower(lua_State* L)
{
	uint8_t towerId = static_cast<uint8_t>(FrameScript::GetNumber(L, 1));
	Packet(CMSG_TOWERDEFENCE_SPAWN_TOWER).PutUInt8(towerId).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceCancelTower(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_CANCEL_TOWER).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceConfirmTower(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_CONFIRM_TOWER).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceStartWave(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_START_WAVE).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceStartGame(lua_State* L)
{
	uint8_t difficulty = static_cast<uint8_t>(FrameScript::GetNumber(L, 1));
	uint8_t mode = static_cast<uint8_t>(FrameScript::GetNumber(L, 2));
	Packet(CMSG_TOWERDEFENCE_START_GAME).PutUInt8(difficulty).PutUInt8(mode).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceLoadGame(lua_State* L)
{
	uint32_t saveId = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	Packet(CMSG_TOWERDEFENCE_LOAD_GAME).PutUInt32(saveId).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceDeleteGame(lua_State* L)
{
	uint32_t saveId = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	Packet(CMSG_TOWERDEFENCE_DELETE_GAME).PutUInt32(saveId).Send();
	return 0;
}

int TowerDefencePackets::TowerDefenceEndGame(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_END_GAME).Send();
	return 0;
}

int TowerDefencePackets::RequestTowerDefenceTowers(lua_State*)
{
	Packet(CMSG_TOWERDEFENCE_TOWER_LIST).Send();
	return 0;
}

// Returns the towers of one set as an array sorted by tower id. Each row is
// { towerId, name, cost, icon }.
int TowerDefencePackets::GetTowerDefenceTowers(lua_State* L)
{
	TowerDefencePackets& self = Instance();
	if (!self.m_hasTowers)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint32_t towerSet = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));

	FrameScript::CreateTable(L, (int)self.m_towers.size(), 0);
	int tbl = FrameScript::GetTop(L);

	int written = 0;
	for (TowerRow const& entry : self.m_towers)
	{
		if (entry.towerSet != towerSet)
			continue;

		FrameScript::CreateTable(L, 4, 0);
		int row = FrameScript::GetTop(L);

		FrameScript::PushNumber(L, entry.towerId);
		FrameScript::RawSetI(L, row, 1);

		FrameScript::PushString(L, entry.name.c_str());
		FrameScript::RawSetI(L, row, 2);

		FrameScript::PushNumber(L, entry.cost);
		FrameScript::RawSetI(L, row, 3);

		FrameScript::PushString(L, entry.icon.c_str());
		FrameScript::RawSetI(L, row, 4);

		FrameScript::RawSetI(L, tbl, ++written);
	}

	FrameScript::SetTop(L, tbl);
	return 1;
}

void TowerDefencePackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_SCOREBOARD, &Handler_SMSG_TOWERDEFENCE_SCOREBOARD);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_SAVED_GAMES, &Handler_SMSG_TOWERDEFENCE_SAVED_GAMES);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_MONEY, &Handler_SMSG_TOWERDEFENCE_MONEY);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_WAVE, &Handler_SMSG_TOWERDEFENCE_WAVE);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_LIVES, &Handler_SMSG_TOWERDEFENCE_LIVES);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_SCORE, &Handler_SMSG_TOWERDEFENCE_SCORE);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_GAME_END, &Handler_SMSG_TOWERDEFENCE_GAME_END);
	sCustomPacket.RegisterHandler(SMSG_TOWERDEFENCE_TOWER_LIST, &Handler_SMSG_TOWERDEFENCE_TOWER_LIST);

	sLua.RegisterFunction("RequestTowerDefenceScoreboard", &RequestTowerDefenceScoreboard, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetTowerDefenceScoreboard", &GetTowerDefenceScoreboard, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RequestTowerDefenceSavedGames", &RequestTowerDefenceSavedGames, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetTowerDefenceSavedGames", &GetTowerDefenceSavedGames, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceSpawnTower", &TowerDefenceSpawnTower, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceCancelTower", &TowerDefenceCancelTower, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceConfirmTower", &TowerDefenceConfirmTower, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceStartWave", &TowerDefenceStartWave, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceStartGame", &TowerDefenceStartGame, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceLoadGame", &TowerDefenceLoadGame, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceDeleteGame", &TowerDefenceDeleteGame, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TowerDefenceEndGame", &TowerDefenceEndGame, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RequestTowerDefenceTowers", &RequestTowerDefenceTowers, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetTowerDefenceTowers", &GetTowerDefenceTowers, LuaFunctionState::FRAME);
}
