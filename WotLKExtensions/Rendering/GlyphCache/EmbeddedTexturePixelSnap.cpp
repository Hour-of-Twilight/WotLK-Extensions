#include <ClientDetours.h>
#include <Macros.h>

#include <cmath>
#include <cstdint>

namespace
{
	constexpr uintptr_t EMBEDDED_TEXTURE_HEIGHT_OFFSET = 0x18;
	constexpr uintptr_t EMBEDDED_TEXTURE_WIDTH_OFFSET = 0x1C;
	constexpr uintptr_t EMBEDDED_TEXTURE_X_OFFSET_OFFSET = 0x20;
	constexpr uintptr_t EMBEDDED_TEXTURE_Y_OFFSET_OFFSET = 0x24;
	constexpr float MIN_EMBEDDED_TEXTURE_PIXELS = 1.0f;

	float& InfoField(void* info, uintptr_t offset)
	{
		return *reinterpret_cast<float*>(static_cast<uint8_t*>(info) + offset);
	}

	float RoundToPixel(float value)
	{
		return std::floor(value + 0.5f);
	}

	float RoundToPixel(float value, float minimum)
	{
		float rounded = RoundToPixel(value);
		return rounded < minimum ? minimum : rounded;
	}

	CLIENT_DETOUR(ParseEmbeddedTexture, 0x006C0E80, __cdecl, bool, (char const* text, void* info, float heightPixels, float layoutScale, float fontHeight))
	{
		bool parsed = ParseEmbeddedTexture(text, info, heightPixels, layoutScale, fontHeight);
		if (!parsed || heightPixels == fontHeight)
			return parsed;

		InfoField(info, EMBEDDED_TEXTURE_HEIGHT_OFFSET) = RoundToPixel(InfoField(info, EMBEDDED_TEXTURE_HEIGHT_OFFSET), MIN_EMBEDDED_TEXTURE_PIXELS);
		InfoField(info, EMBEDDED_TEXTURE_WIDTH_OFFSET) = RoundToPixel(InfoField(info, EMBEDDED_TEXTURE_WIDTH_OFFSET), MIN_EMBEDDED_TEXTURE_PIXELS);
		InfoField(info, EMBEDDED_TEXTURE_X_OFFSET_OFFSET) = RoundToPixel(InfoField(info, EMBEDDED_TEXTURE_X_OFFSET_OFFSET));
		InfoField(info, EMBEDDED_TEXTURE_Y_OFFSET_OFFSET) = RoundToPixel(InfoField(info, EMBEDDED_TEXTURE_Y_OFFSET_OFFSET));
		return parsed;
	}
}
