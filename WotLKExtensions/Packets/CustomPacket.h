#pragma once

#include "SharedDefines.h"
#include <vector>

using PacketHandlerFn = void (*)(void*, uint32_t, uint32_t, CDataStore*);

class CustomPacket
{
public:
	static CustomPacket& Instance();

	void Apply();

	// Like Lua functions, handlers are queued at startup and applied at the correct lifecycle point:
	//   RegisterHandler       queues into the world-handler holder, flushed by SetCustomHandlers()
	//                         (runs on every player init).
	//   RegisterRealmHandler  queues into the realm-handler holder, flushed by SetCustomRealmHandlers()
	//                         (runs when the realm connection is set up, before any player).
	void RegisterHandler(uint32_t opcode, PacketHandlerFn handler);
	void RegisterRealmHandler(uint32_t opcode, PacketHandlerFn handler);
	void SendSanityCheck(bool sendMpqs = false);
	void SendGobEditorSelectNearest();
	void SetCustomRealmHandlers(void* realmConn);

	CustomPacket(const CustomPacket&) = delete;
	CustomPacket& operator=(const CustomPacket&) = delete;

private:
	CustomPacket() : customData{ 0 } {}

	CustomNetClient customData;

	struct StoredHandler
	{
		uint32_t opcode;
		PacketHandlerFn handler;
	};
	std::vector<StoredHandler> m_handlers;      // world handlers, flushed by SetCustomHandlers
	std::vector<StoredHandler> m_realmHandlers; // realm handlers, flushed by SetCustomRealmHandlers

	// Applies every queued world handler. Runs on each player init.
	void SetCustomHandlers();

	static void InitializePlayerEx();
	static void __fastcall ProcessMessageEx(void* _this, uint32_t unused, uint32_t a2, CDataStore* a3, uint32_t a4);
	static void __fastcall SetMessageHandlerEx(void* _this, uint32_t unused, uint32_t opcode, void* function, void* param);
	static int __cdecl UpdateCharacterListEx();

	static void Packet_SMSG_TB_BULLSHIT(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_CUSTOM_STAT_TRACK(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_SANITY_CHECK(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_FUCKED_CLIENT_DETECTED(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_REQUEST_CHAR_ENUM_LEVELS(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_GOB_EDITOR_SELECT(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
	static void Packet_SMSG_BACKGROUND_DOWNLOAD(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
};

#define sCustomPacket CustomPacket::Instance()
