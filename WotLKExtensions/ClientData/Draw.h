#pragma once

#include <Defines.h>
#include <Macros.h>
#include <ClientData/GxDevice.h>
#include <ClientData/GxTypes.h>
#include <ClientData/MathTypes.h>
#include <cstdint>

namespace ClientData
{
	CLIENT_FUNCTION(GxSceneClear, 0x6813B0, __cdecl, void, (uint32, CImVector))
	CLIENT_FUNCTION(GxRsSet, 0x408240, __cdecl, void, (EGxRenderState, void*))
	CLIENT_FUNCTION(GxRsSet_int32_t, 0x408BF0, __cdecl, void, (EGxRenderState, int32))
	CLIENT_FUNCTION(GxPrimIndexPtr, 0x681AB0, __cdecl, void, (uint32, uint16*))
	CLIENT_FUNCTION(GxPrimVertexPtr, 0x682400, __cdecl, void,
	    (uint32, C3Vector*, uint32, C3Vector*, uint32, CImVector*, uint32, C2Vector*, uint32, C2Vector*, uint32))
	CLIENT_FUNCTION(GxXformViewProjNativeTranspose, 0x408110, __cdecl, void, (C44Matrix*))
	CLIENT_FUNCTION(GxXformPush, 0x616AD0, __cdecl, void, (EGxXform, C44Matrix*))
}
