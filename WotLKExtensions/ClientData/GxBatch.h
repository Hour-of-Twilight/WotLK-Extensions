#pragma once

#include <Defines.h>
#include <ClientData/GxTypes.h>
#include <cstdint>

namespace ClientData
{
	class CGxBatch
	{
	public:
		EGxPrim m_primType;
		uint32 m_start;
		uint32 m_count;
		uint16 m_minIndex;
		uint16 m_maxIndex;
	};
}
