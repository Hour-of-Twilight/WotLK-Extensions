#pragma once

#include "Models/Offsets/GxOffsets.h"

#include <cstdint>

namespace ModernM2
{
	template <class Fn>
	inline Fn Native(uintptr_t address)
	{
		return reinterpret_cast<Fn>(address);
	}

	namespace Gx
	{
		namespace off = ModernM2::Offsets::Gx;

		template <class Fn>
		inline Fn Vtbl(void* obj, unsigned idx)
		{
			return reinterpret_cast<Fn>((*reinterpret_cast<void***>(obj))[idx]);
		}

		inline void* RawGraphicsDevice()
		{
			return *reinterpret_cast<void**>(off::kGxDevicePtr);
		}

		inline void* RawDevice()
		{
			void* g = RawGraphicsDevice();
			if (!g)
				return nullptr;
			return static_cast<off::GxDevice*>(g)->d3dDevice;
		}
	}
}
