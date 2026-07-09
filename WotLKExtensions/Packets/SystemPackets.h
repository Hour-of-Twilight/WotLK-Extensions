#pragma once

#include "SharedDefines.h"

class SystemPackets
{
public:
	static void RegisterRealmHandlers();

private:
	static void Packet_SMSG_RECEIVE_SECURITY_LEVEL(void* handlerParam, uint32_t opcode, uint32_t a2, CDataStore* a3);
};
