#pragma once

#include <Defines.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace HDCharacters
{
	struct BlpImage
	{
		uint32 width = 0;
		uint32 height = 0;
		bool hasAlpha = false;
		std::vector<uint32> pixels;

		uint32 At(uint32 x, uint32 y) const
		{
			return pixels[y * width + x];
		}
	};

	bool DecodeBlp(const uint8* data, size_t size, BlpImage& out);
	std::shared_ptr<const BlpImage> LoadBlp(const char* path);
	void ClearBlpCache();
}
