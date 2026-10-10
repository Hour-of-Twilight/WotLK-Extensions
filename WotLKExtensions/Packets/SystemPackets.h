#pragma once

#include <Defines.h>

#include "SharedDefines.h"

class SystemPackets
{
public:
	static void RegisterRealmHandlers();

private:
	static void Packet_SMSG_RECEIVE_SECURITY_LEVEL(void* handlerParam, uint32 opcode, uint32 a2, CDataStore* a3);
};
