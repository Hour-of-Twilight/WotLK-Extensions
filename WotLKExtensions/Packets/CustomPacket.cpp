#include "CustomPacket.h"
#include "Packet.h"
#include <Editor/EditorRuntime.h>
#include <TalentFramePackets.h>
#include "TestFramePackets.h"
#include "UnitLevelCache.h"
#include "GemSocketPackets.h"
#include "ItemLevelPackets.h"
#include "ItemDbCachePackets.h"
#include "FeaturePackets.h"
#include "DBCRecordPackets.h"
#include "StreamingPackets.h"
#include "LootWindowPackets.h"
#include "Streaming/BackgroundDownloader.h"
#include "SystemPackets.h"
#include "XMLExtensions.h"
#include "Player.h"
#include <string>
#include <iostream>
#include "../Tools/MPQScanner.h"
#include "ClientDetours.h"
#include "Misc.h"
#include <sstream>
#include "Editor/EditorObject.h"

CustomPacket& CustomPacket::Instance()
{
	static CustomPacket instance;
	return instance;
}

void CustomPacket::Apply()
{
	Util::OverwriteUInt32AtAddress(0x6E8EE2, (uint32_t)&InitializePlayerEx - 0x6E8EE6);
	//
	Util::OverwriteUInt32AtAddress(0x6324CA, (uint32_t)&ProcessMessageEx - 0x6324CE);
	Util::OverwriteUInt32AtAddress(0x714AFC, (uint32_t)&ProcessMessageEx - 0x714B00);
	Util::OverwriteUInt32AtAddress(0x716A79, (uint32_t)&ProcessMessageEx - 0x716A7D);
	//
	Util::OverwriteUInt32AtAddress(0x6B0B9E, (uint32_t)&SetMessageHandlerEx - 0x6B0BA2);
	Util::OverwriteUInt32AtAddress(0x4DB67F, (uint32_t)&UpdateCharacterListEx - 0x4DB683);

	RegisterHandler(SMSG_TB_BULLSHIT, &Packet_SMSG_TB_BULLSHIT);
	RegisterHandler(SMSG_CUSTOM_STAT_TRACK, &Packet_SMSG_CUSTOM_STAT_TRACK);
	RegisterHandler(SMSG_SANITY_CHECK, &Packet_SMSG_SANITY_CHECK);
	RegisterHandler(SMSG_FUCKED_CLIENT_DETECTED, &Packet_SMSG_FUCKED_CLIENT_DETECTED);
	RegisterHandler(SMSG_GOB_EDITOR_SELECT, &Packet_SMSG_GOB_EDITOR_SELECT);
	RegisterHandler(SMSG_BACKGROUND_DOWNLOAD, &Packet_SMSG_BACKGROUND_DOWNLOAD);

	sTalentFramePackets.Apply();
	sFeaturePackets.Apply();
	sGemSocketPackets.Apply();
	sItemLevelPackets.Apply();
	sTestFramePackets.Apply();
	sUnitLevelCache.Apply();
	DBCRecordPackets::Apply();
	StreamingPackets::Apply();
	sLootWindowPackets.Apply();
	ItemDbCachePackets::RegisterHandlers();

	// Realm handlers: queued now, applied against the realm connection in SetCustomRealmHandlers.
	RegisterRealmHandler(SMSG_REQUEST_CHAR_ENUM_LEVELS, &Packet_SMSG_REQUEST_CHAR_ENUM_LEVELS);
	SystemPackets::RegisterRealmHandlers();
	ItemDbCachePackets::RegisterRealmHandlers();
}

void CustomPacket::SetCustomRealmHandlers(void* realmConn)
{
	for (const StoredHandler& h : m_realmHandlers)
		SetMessageHandlerEx(nullptr, 0, h.opcode, (void*)h.handler, realmConn);
}

void CustomPacket::SetCustomHandlers()
{
	for (const StoredHandler& h : m_handlers)
		SetMessageHandlerEx(nullptr, 0, h.opcode, (void*)h.handler, nullptr);
}

void CustomPacket::RegisterHandler(uint32_t opcode, PacketHandlerFn handler)
{
	m_handlers.push_back({ opcode, handler });
}

void CustomPacket::RegisterRealmHandler(uint32_t opcode, PacketHandlerFn handler)
{
	m_realmHandlers.push_back({ opcode, handler });
}

void CustomPacket::InitializePlayerEx()
{
	ClientServices::InitializePlayer();
	Instance().SetCustomHandlers();
}

void __fastcall CustomPacket::ProcessMessageEx(void* _this, uint32_t unused, uint32_t a2, CDataStore* a3, uint32_t a4)
{
	int16_t opcode = 0;
	CDataStore_C::GetInt16(a3, &opcode);

	if (opcode < SMSG_UPDATE_CUSTOM_COMBAT_RATING)
	{
		a3->m_read -= 2;
		CNetClient::ProcessMessage(_this, a2, a3, a4);
	}
	else
	{
		++*(uint32_t*)0xC5D638;
		uint32_t num = opcode - SMSG_UPDATE_CUSTOM_COMBAT_RATING;
		CustomNetClient& data = Instance().customData;
		if (opcode < NUM_CUSTOM_MSG_TYPES && data.handler[num])
			// I've got cancer writing this, function typedefs are ugly as sin
			((void (*)(void*, uint32_t, uint32_t, CDataStore*))data.handler[num])(data.handlerParam[num], opcode, a2, a3);
		else
			CDataStore_C::IsRead(a3);
	}
}

void __fastcall CustomPacket::SetMessageHandlerEx(void* _this, uint32_t unused, uint32_t opcode, void* handler, void* param)
{
	if (opcode < SMSG_UPDATE_CUSTOM_COMBAT_RATING)
		CNetClient::SetMessageHandler(_this, opcode, handler, param);
	else
	{
		uint32_t num = opcode - SMSG_UPDATE_CUSTOM_COMBAT_RATING;
		CustomNetClient& data = Instance().customData;
		data.handler[num] = handler;
		data.handlerParam[num] = param;
	}
}

int __cdecl CustomPacket::UpdateCharacterListEx()
{
	int result = CCharacterSelection__UpdateCharacterList();
	Packet(CMSG_REQUEST_CHAR_ENUM_LEVELS).Send();
	return result;
}

// You ain't seeing this sussy shit.
void CustomPacket::Packet_SMSG_TB_BULLSHIT(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	Packet r(a3);
	int32 type = r.GetInt32();
	r.GetInt32(); // toggle (unused)
	switch (type)
	{
	case 1:
	{
		Misc::ToggleWireframeMode();
		break;
	}
	case 2:
	{
		Misc::ToggleDisplayNormals();
		break;
	}
	case 3:
	{
		Misc::ToggleGroundEffects();
		break;
	}
	case 4:
	{
		Misc::ToggleLiquids();
		break;
	}
	case 5:
	{
		Misc::ToggleTerrain();
		break;
	}
	case 6:
	{
		Misc::ToggleTerrainCulling();
		break;
	}
	case 7:
	{
		Misc::ToggleWMO();
		break;
	}
	}
}

void CustomPacket::Packet_SMSG_CUSTOM_STAT_TRACK(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{

	Packet r(a3);
	uint32 magicFind = r.GetUInt32();
	float healthLeech = r.GetFloat();
	float manaLeech = r.GetFloat();
	float critDamageMod = r.GetFloat();
	float critHealingMod = r.GetFloat();
	sPlayer.SetMagicFind(magicFind);
	sPlayer.SetHealthLeech(healthLeech);
	sPlayer.SetManaLeech(manaLeech);
	sPlayer.SetCritDamageMod(critDamageMod);
	sPlayer.SetCritHealingMod(critHealingMod);
	Util::DebugOutput("%u %f %f %f %f", sPlayer.GetMagicFind(), sPlayer.GetHealthLeech(), sPlayer.GetManaLeech(), sPlayer.GetCritDamageMod(), sPlayer.GetCritHealingMod());
	for (uint8 i = 0; i < MAX_SPELL_SCHOOL; ++i)
		sPlayer.SetCustomSpellPen(i, r.GetInt32());
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_STAT_UPDATE"), "");
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("COMBAT_RATING_UPDATE"), "");
}

void CustomPacket::Packet_SMSG_SANITY_CHECK(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	sCustomPacket.SendSanityCheck(true);
}

void CustomPacket::Packet_SMSG_BACKGROUND_DOWNLOAD(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	sBackgroundDownloader.Trigger();
}

void CustomPacket::SendSanityCheck(bool sendMpqs)
{
	Packet pkt(CMSG_SANITY_CHECK);
	pkt.PutInt32(DLL_VER);
	if (sendMpqs)
	{
		std::vector<MpqInfo> mpqs = sMpqScanner.GetResults();
		pkt.PutUInt32(static_cast<uint32>(mpqs.size()));
		for (const auto& mpq : mpqs)
		{
			pkt.PutString(mpq.filename_lower.c_str());
			pkt.PutUInt32(mpq.hash);
		}
	}
	else
		pkt.PutInt32(0);

	pkt.Send();
}

void CustomPacket::Packet_SMSG_FUCKED_CLIENT_DETECTED(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	Packet r(a3);
	int32 numberOfFiles = r.GetInt32();
	if (!numberOfFiles)
	{
		r.Release();
		return;
	}

	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_BAD_MPQ_NUM"), "%d", numberOfFiles);

	for (int32 i = 0; i < numberOfFiles; ++i)
	{
		char filename[256];
		r.GetString(filename, sizeof(filename));
		uint32 crc = r.GetUInt32();

		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_BAD_MPQ_FILE_INFO"), "%s%u", filename, crc);
	}
}

void CustomPacket::Packet_SMSG_REQUEST_CHAR_ENUM_LEVELS(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	Packet r(a3);
	uint32 numOfChars = r.GetUInt32();

	for (uint32 i = 0; i < numOfChars; ++i)
	{
		char name[64] = {};
		r.GetString(name, sizeof(name));
		uint32 level = r.GetUInt32();
		uint32 talentLevel = r.GetUInt32();
		uint32 subClass = r.GetUInt32();
		int8_t gameMode = r.GetInt8();

		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_CHAR_SELECT_UPDATE"), "%s%u%u%u%u", name, level, talentLevel, subClass, (uint32)gameMode);
	}
}

void CustomPacket::SendGobEditorSelectNearest()
{
	Packet(CMSG_GOB_EDITOR_SELECT_NEAREST).Send();
}

void CustomPacket::Packet_SMSG_GOB_EDITOR_SELECT(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	uint64_t guid = Packet(a3).GetUInt64();
	EditorRuntime::SelectGameObject(guid);
	CGGameObject_C* object = EditorRuntime::SelectedGameObject();
	if (object)
		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_GOB_SELECTED"), "%s%u%f%f%f", EditorObject::GuidToString(guid), 0 /*todo get gameobject entry too lazy atm.*/,
		    object->m_passenger.position.x, object->m_passenger.position.y, object->m_passenger.position.z);
}
