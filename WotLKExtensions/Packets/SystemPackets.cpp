#include "SystemPackets.h"
#include "Packet.h"
#include "CustomPacket.h"
#include "Player.h"
#include <Logger.h>
void SystemPackets::RegisterRealmHandlers()
{
	sCustomPacket.RegisterRealmHandler(SMSG_RECEIVE_SECURITY_LEVEL, &Packet_SMSG_RECEIVE_SECURITY_LEVEL);
}

void SystemPackets::Packet_SMSG_RECEIVE_SECURITY_LEVEL(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3)
{
	int8_t security = Packet(a3).GetInt8();
	sPlayer.SetSecurityLevel(security);
	if (security >= 2)
	{
		Util::OverwriteBytesAtAddress((void*)AFK_CHECK_SUB, 0x90, 6);
		uint8_t patch5min[] = { 0xE9, 0xEE, 0xFD, 0xFF, 0xFF, 0x90 };
		Util::OverwriteBytesAtAddress(AFK_5MIN_CHECK, patch5min, 6);
		Util::SetByteAtAddress((void*)AFK_30MIN_CHECK, 0xEB);
	}
}
