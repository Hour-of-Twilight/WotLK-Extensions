#pragma once

#include <Defines.h>

#include <cstdint>

namespace ClientData
{
	struct C2Vector
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct C3Vector
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct CRange
	{
		float l;
		float h;
	};

	struct CiRange
	{
		int32 l;
		int32 h;
	};

	struct C2iVector
	{
		int32 x;
		int32 y;
	};

	struct C4Vector
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;
	};

	struct CImVector
	{
		union
		{
			struct
			{
				uint8 b;
				uint8 g;
				uint8 r;
				uint8 a;
			};

			uint32 value;
		};
	};

	struct CRect
	{
		float minY = 0.0f;
		float minX = 0.0f;
		float maxY = 0.0f;
		float maxX = 0.0f;
	};

	struct CiRect
	{
		int32 minY;
		int32 minX;
		int32 maxY;
		int32 maxX;
	};

	struct CBoundingBox
	{
		CRange x;
		CRange y;
		CRange z;
	};

	struct CAaBox
	{
		C3Vector b;
		C3Vector t;
	};

	struct C44Matrix
	{
		float a0 = 1.0f;
		float a1 = 0.0f;
		float a2 = 0.0f;
		float a3 = 0.0f;
		float b0 = 0.0f;
		float b1 = 1.0f;
		float b2 = 0.0f;
		float b3 = 0.0f;
		float c0 = 0.0f;
		float c1 = 0.0f;
		float c2 = 1.0f;
		float c3 = 0.0f;
		float d0 = 0.0f;
		float d1 = 0.0f;
		float d2 = 0.0f;
		float d3 = 1.0f;
	};
}
