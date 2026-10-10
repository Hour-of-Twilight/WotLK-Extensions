#include <Defines.h>
#include <Character/HDCharacters/HDNpcDisplay.h>

#include <CDBCMgr.h>
#include <Logger.h>

namespace HDCharacters::NpcDisplay
{
	namespace
	{
		const char* kDisplayTable = "HDNpcDisplay";
		const char* kSwapTable = "HDDisplaySwap";

		size_t RowCount(const char* table)
		{
			auto it = GlobalCDBCMap.allCDBCs.find(table);
			return it == GlobalCDBCMap.allCDBCs.end() ? 0 : it->second.size();
		}
	}

	const HDNpcDisplayRow* Find(uint32 displayExtraId)
	{
		if (!displayExtraId)
			return nullptr;
		return GlobalCDBCMap.getRow<HDNpcDisplayRow>(kDisplayTable, displayExtraId);
	}

	const char* BakePath(const HDNpcDisplayRow* row)
	{
		if (!row || !(row->flags & HD_NPC_DISPLAY_BAKE) || !g_hdNpcDisplayStrings)
			return nullptr;
		if (!row->bakeTexture || row->bakeTexture >= g_hdNpcDisplayStringsSize)
			return nullptr;
		const char* path = g_hdNpcDisplayStrings + row->bakeTexture;
		return *path ? path : nullptr;
	}

	uint32 Swap(uint32 displayId, bool isPlayer)
	{
		if (!displayId)
			return 0;
		const HDDisplaySwapRow* row = GlobalCDBCMap.getRow<HDDisplaySwapRow>(kSwapTable, displayId);
		if (!row || !row->hdDisplayID)
			return 0;
		if ((row->flags & HD_DISPLAY_SWAP_PLAYERS_ONLY) && !isPlayer)
			return 0;
		if ((row->flags & HD_DISPLAY_SWAP_NPCS_ONLY) && isPlayer)
			return 0;
		return row->hdDisplayID;
	}

	bool HasSwaps()
	{
		return RowCount(kSwapTable) != 0;
	}

	void Report()
	{
		const size_t displays = RowCount(kDisplayTable);
		if (!displays)
		{
			LOG_INFO << "hd characters: HDNpcDisplay.dbc has no rows, npc models stay stock";
			return;
		}
		size_t baked = 0;
		auto it = GlobalCDBCMap.allCDBCs.find(kDisplayTable);
		for (const auto& entry : it->second)
		{
			const HDNpcDisplayRow* row = std::any_cast<HDNpcDisplayRow>(&entry.second);
			if (row && (row->flags & HD_NPC_DISPLAY_BAKE))
				++baked;
		}
		LOG_INFO << "hd characters: " << displays << " display extras are hd capable, " << baked
		         << " have a baked hd texture, " << RowCount(kSwapTable) << " display ids are replaced";
	}
}
