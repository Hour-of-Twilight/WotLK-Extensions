#include <Defines.h>
#include <ClientDetours.h>
#include <Macros.h>

#include <Windows.h>
#include <cstring>
#include <vector>

namespace MonitorModes
{
	struct CGxMonitorMode
	{
		int32 width;
		int32 height;
		uint32 bpp;
		uint32 refreshRate;
	};

	struct TSGrowableArray_CGxMonitorMode
	{
		uint32 alloc;
		uint32 count;
		CGxMonitorMode* data;
		uint32 chunk;
	};

	CLIENT_FUNCTION(TSGrowableArray_CGxMonitorMode__SetCount, 0x0068A0F0, __thiscall, void, (TSGrowableArray_CGxMonitorMode*, uint32))

	static bool s_cacheValid = false;
	static std::vector<CGxMonitorMode> s_modes;

	CLIENT_DETOUR(CGxDevice_AdapterMonitorModes, 0x0068A4C0, __cdecl, void, (TSGrowableArray_CGxMonitorMode* modes))
	{
		if (!s_cacheValid)
		{
			CGxDevice_AdapterMonitorModes(modes);
			if (modes->count == 0)
				return;
			s_modes.assign(modes->data, modes->data + modes->count);
			s_cacheValid = true;
			return;
		}
		TSGrowableArray_CGxMonitorMode__SetCount(modes, (uint32)s_modes.size());
		std::memcpy(modes->data, s_modes.data(), s_modes.size() * sizeof(CGxMonitorMode));
	}

	CLIENT_DETOUR(OsWindowProc, 0x0086A210, __cdecl, LRESULT, (HWND window, UINT message, WPARAM wParam, LPARAM lParam))
	{
		if (message == WM_DISPLAYCHANGE)
			s_cacheValid = false;
		return OsWindowProc(window, message, wParam, lParam);
	}
}
