#pragma once

#include <cstdint>

struct CDataStore;

class StreamingPackets
{
public:
	static void Apply();

private:
	static void Handler_SMSG_STREAMING_START(void*, uint32_t, uint32_t, CDataStore* pkt);
};
