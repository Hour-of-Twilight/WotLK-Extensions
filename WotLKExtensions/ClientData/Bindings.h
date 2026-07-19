#pragma once

#include <Macros.h>

#include <cstdint>

namespace ClientData::Bindings
{
	constexpr uintptr_t kExecKeyAddr = 0x00563150;

	CLIENT_FUNCTION(GetReducedKeyBinding, 0x005622E0, __thiscall, void*, (void* self, int keyMode, void* slot, char* outBuf, int bufSize))
	CLIENT_FUNCTION(GetCommandForBinding, 0x0055E470, __thiscall, const char*, (void* self, void* entry, int keyMode))
}
