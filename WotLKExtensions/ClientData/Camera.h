#pragma once

#include <Macros.h>
#include <ClientData/MathTypes.h>
#include <ClientData/SharedDefines.h>

namespace ClientData
{
	class CGCamera;

	// Runs once per frame per camera and is where the client writes m_position and m_facing back,
	// so a hook here is the last word on where the camera ends up.
	CLIENT_FUNCTION(CGCamera_UpdateCallback, 0x00607B00, __cdecl, int, (void* param, CGCamera* camera))

	class CGCamera
	{
	public:
		void SetupWorldProjection(CRect* rect)
		{
			reinterpret_cast<void(__thiscall*)(CGCamera*, CRect*)>(0x5FE880)(this, rect);
		}

		virtual float Fov();
		virtual C3Vector& Forward(C3Vector& vec);
		virtual C3Vector& Right(C3Vector& vec);
		virtual C3Vector& Up(C3Vector& vec);

		char _pad0[0x4];
		C3Vector m_position;
		float m_facing[3 * 3];
		float m_nearZ;
		float m_farZ;
		float m_fov;
		float m_aspect;
	};
}
