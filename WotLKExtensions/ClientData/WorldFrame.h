#pragma once

#include <Macros.h>
#include <ClientData/Camera.h>
#include <ClientData/GxDevice.h>
#include <ClientData/MathTypes.h>

#include <cstdint>

namespace ClientData
{
	struct WorldHitTest
	{
		uint64_t guid;
		C3Vector hitpoint;
		float distance;
		C3Vector start;
		C3Vector end;
	};

	struct CSimpleTop
	{
		char pad1[0x78];
		void* mouseFocus;
		char pad2[0x11A8];
		C2Vector mousePosition;
	};

	struct CGWorldFrame
	{
		char pad1[0xA0];
		CSimpleTop* simpleTop;
		char pad2[0x234];
		uint32_t ukn16;
		uint32_t ukn17;
		WorldHitTest m_actionHitTest;
	};

	// The circle the client projects onto the terrain under a targeting cursor. sub_4F8A40 reads
	// all three of these, builds a CAaBox of (pos +/- radius) on x and y by (pos +/- 2.0) on z and
	// hands it to CGxDevice::ProjectTex2D.
	namespace TerrainDecal
	{
		// Mode doubles as the index into the two textures CGWorldFrame loads at startup,
		// Spell-Shadow-Acceptable.blp and Spell-Shadow-Unacceptable.blp. Both draw calls are
		// guarded by `cmp mode, 2 / jge skip`, so anything >= 2 is hidden.
		CLIENT_ADDRESS(int32_t, sMode, 0x00AC79A4)
		CLIENT_ADDRESS(float, sRadius, 0x00B74370)
		CLIENT_ADDRESS(C3Vector, sPosition, 0x00B74380)

		constexpr int32_t kModeAcceptable = 0;
		constexpr int32_t kModeUnacceptable = 1;
		constexpr int32_t kModeHidden = 3;
	}

	class CGWorldFrameFull;
	CLIENT_FUNCTION(CGWorldFrame_GetLineSegment, 0x4F6450, __thiscall, bool,
	    (CGWorldFrameFull * frame, float mouseX, float mouseY, C3Vector* start, C3Vector* end))
	CLIENT_FUNCTION(CGWorldFrame_OnWorldRender, 0x4F8EA0, __thiscall, bool, (CGWorldFrameFull * frame))

	class CGWorldFrameFull
	{
	public:
		static CGWorldFrameFull* Current()
		{
			return *reinterpret_cast<CGWorldFrameFull**>(0x00B7436C);
		}

		bool GetLineSegment(float mouseX, float mouseY, C3Vector* start, C3Vector* end)
		{
			return CGWorldFrame_GetLineSegment(this, mouseX, mouseY, start, end);
		}

		void MouseToWorld(float mouseX, float mouseY, C3Vector* start, C3Vector* end)
		{
			CGxDevice* device = CGxDevice::Get();
			if (!device || !m_camera)
				return;

			C44Matrix* projection = &device->m_projection;
			C44Matrix* view = &device->m_xforms[10].m_mtx[device->m_xforms[10].m_level];
			m_camera->SetupWorldProjection(&unkRect_0320);

			if (!GetLineSegment(mouseX, mouseY, start, end))
			{
				*start = {};
				*end = {};
			}

			device->XformSetProjection(projection);
			device->XformSetView(view);
		}

		char simpleFrame[0x29C];
		uint32_t unk_029C;
		uint32_t unk_02A0;
		uint32_t unk_02A4;
		uint32_t unk_02A8;
		uint32_t unk_02AC;
		uint32_t unk_02B0;
		uint32_t unk_02B4;
		uint32_t unk_02B8;
		uint32_t unk_02BC;
		uint32_t unk_02C0;
		uint32_t unk_02C4;
		uint64_t currentGuid;
		uint32_t unk_02D0;
		uint32_t unk_02D4;
		uint32_t unk_02D8;
		uint32_t unk_02DC;
		uint32_t unk_02E0;
		uint32_t unk_02E4;
		uint32_t unk_02E8;
		uint32_t unk_02EC;
		uint32_t unk_02F0;
		uint32_t unk_02F4;
		uint32_t unk_02F8;
		uint32_t unk_02FC;
		uint32_t unk_0300;
		uint32_t unk_0304;
		uint32_t unk_0308;
		uint32_t unk_030C;
		uint32_t unk_0310;
		uint32_t unk_0314;
		uint32_t unk_0318;
		uint32_t unk_031C;
		CRect unkRect_0320;
		CRect unkRect_0330;
		C44Matrix unkMatrix_0340;
		uint32_t unk_0380;
		uint32_t unk_0384;
		uint32_t unk_0388;
		uint32_t unk_038C;
		char unk_0390[0x780];
		uint32_t unk_0B10;
		uint32_t unk_0B14;
		uint32_t unk_0B18[7083];
		uint32_t unk_79C4[279];
		CGCamera* m_camera;
		uint32_t unk_7E24;
	};
}
