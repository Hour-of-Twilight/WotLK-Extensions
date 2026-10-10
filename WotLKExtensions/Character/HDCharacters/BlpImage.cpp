#include <Defines.h>
#include <Character/HDCharacters/BlpImage.h>

#include <ClientData/Streaming.h>
#include <Logger.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace HDCharacters
{
	namespace
	{
		constexpr size_t kHeaderSize = 4 + 4 + 4 + 4 + 4 + 64 + 64 + 1024;
		constexpr size_t kCacheBudgetBytes = 96u * 1024u * 1024u;

		enum Encoding : uint8
		{
			ENCODING_PALETTE = 1,
			ENCODING_DXT = 2,
			ENCODING_RAW = 3,
		};

		struct Header
		{
			uint8 encoding;
			uint8 alphaDepth;
			uint8 alphaEncoding;
			uint8 hasMips;
			uint32 width;
			uint32 height;
			uint32 mipOffset;
			uint32 mipSize;
			const uint32* palette;
		};

		uint32 Pack(uint32 r, uint32 g, uint32 b, uint32 a)
		{
			return (a << 24) | (r << 16) | (g << 8) | b;
		}

		void Expand565(uint16 c, uint32& r, uint32& g, uint32& b)
		{
			r = (c >> 11) & 0x1F;
			g = (c >> 5) & 0x3F;
			b = c & 0x1F;
			r = (r << 3) | (r >> 2);
			g = (g << 2) | (g >> 4);
			b = (b << 3) | (b >> 2);
		}

		void DecodeColorBlock(const uint8* block, bool dxt1, uint32 colors[4], uint32 alphas[4])
		{
			uint16 c0 = block[0] | (block[1] << 8);
			uint16 c1 = block[2] | (block[3] << 8);
			uint32 r0, g0, b0, r1, g1, b1;
			Expand565(c0, r0, g0, b0);
			Expand565(c1, r1, g1, b1);
			colors[0] = Pack(r0, g0, b0, 255);
			colors[1] = Pack(r1, g1, b1, 255);
			alphas[0] = alphas[1] = 255;
			if (!dxt1 || c0 > c1)
			{
				colors[2] = Pack((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
				colors[3] = Pack((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
				alphas[2] = alphas[3] = 255;
			}
			else
			{
				colors[2] = Pack((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
				colors[3] = 0;
				alphas[2] = 255;
				alphas[3] = 0;
			}
		}

		bool DecodeDxt(const Header& h, const uint8* data, BlpImage& out)
		{
			const bool dxt1 = h.alphaEncoding == 0;
			const bool dxt3 = h.alphaEncoding == 1;
			const size_t blockSize = dxt1 ? 8 : 16;
			const uint32 bw = (h.width + 3) / 4;
			const uint32 bh = (h.height + 3) / 4;
			if ((size_t)bw * bh * blockSize > h.mipSize)
				return false;
			const uint8* block = data;
			for (uint32 by = 0; by < bh; ++by)
			{
				for (uint32 bx = 0; bx < bw; ++bx, block += blockSize)
				{
					uint32 colors[4];
					uint32 alphas[4];
					const uint8* colorBlock = dxt1 ? block : block + 8;
					DecodeColorBlock(colorBlock, dxt1, colors, alphas);
					uint32 alphaTable[8] = {};
					if (!dxt1 && !dxt3)
					{
						uint32 a0 = block[0];
						uint32 a1 = block[1];
						alphaTable[0] = a0;
						alphaTable[1] = a1;
						if (a0 > a1)
						{
							for (uint32 i = 1; i < 7; ++i)
								alphaTable[i + 1] = ((7 - i) * a0 + i * a1) / 7;
						}
						else
						{
							for (uint32 i = 1; i < 5; ++i)
								alphaTable[i + 1] = ((5 - i) * a0 + i * a1) / 5;
							alphaTable[6] = 0;
							alphaTable[7] = 255;
						}
					}
					uint32 indices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | ((uint32)colorBlock[7] << 24);
					uint64 alphaBits = 0;
					if (!dxt1 && !dxt3)
					{
						for (int i = 0; i < 6; ++i)
							alphaBits |= (uint64)block[2 + i] << (8 * i);
					}
					for (uint32 py = 0; py < 4; ++py)
					{
						uint32 y = by * 4 + py;
						if (y >= h.height)
							break;
						for (uint32 px = 0; px < 4; ++px)
						{
							uint32 x = bx * 4 + px;
							if (x >= h.width)
								break;
							uint32 texel = py * 4 + px;
							uint32 ci = (indices >> (texel * 2)) & 3;
							uint32 rgb = colors[ci] & 0x00FFFFFFu;
							uint32 a;
							if (dxt1)
								a = h.alphaDepth ? alphas[ci] : 255;
							else if (dxt3)
							{
								uint32 nibble = (block[texel / 2] >> ((texel & 1) * 4)) & 0xF;
								a = nibble * 17;
							}
							else
								a = alphaTable[(alphaBits >> (texel * 3)) & 7];
							out.pixels[y * h.width + x] = rgb | (a << 24);
						}
					}
				}
			}
			return true;
		}

		bool DecodePalette(const Header& h, const uint8* data, BlpImage& out)
		{
			const size_t count = (size_t)h.width * h.height;
			size_t alphaBytes = 0;
			if (h.alphaDepth == 1)
				alphaBytes = (count + 7) / 8;
			else if (h.alphaDepth == 4)
				alphaBytes = (count + 1) / 2;
			else if (h.alphaDepth == 8)
				alphaBytes = count;
			if (count + alphaBytes > h.mipSize)
				return false;
			const uint8* alpha = data + count;
			for (size_t i = 0; i < count; ++i)
			{
				uint32 rgb = h.palette[data[i]] & 0x00FFFFFFu;
				uint32 a = 255;
				if (h.alphaDepth == 1)
					a = (alpha[i / 8] >> (i & 7)) & 1 ? 255 : 0;
				else if (h.alphaDepth == 4)
					a = ((alpha[i / 2] >> ((i & 1) * 4)) & 0xF) * 17;
				else if (h.alphaDepth == 8)
					a = alpha[i];
				out.pixels[i] = rgb | (a << 24);
			}
			return true;
		}

		bool DecodeRaw(const Header& h, const uint8* data, BlpImage& out)
		{
			const size_t count = (size_t)h.width * h.height;
			if (count * 4 > h.mipSize)
				return false;
			for (size_t i = 0; i < count; ++i)
			{
				uint32 v;
				std::memcpy(&v, data + i * 4, 4);
				if (h.alphaDepth == 0)
					v |= 0xFF000000u;
				out.pixels[i] = v;
			}
			return true;
		}

		struct CacheEntry
		{
			std::shared_ptr<const BlpImage> image;
			std::list<std::string>::iterator order;
		};

		std::mutex g_cacheMutex;
		std::unordered_map<std::string, CacheEntry> g_cache;
		std::list<std::string> g_order;
		size_t g_cacheBytes = 0;

		std::string CacheKey(const char* path)
		{
			std::string key(path);
			for (char& c : key)
			{
				if (c == '/')
					c = '\\';
				else
					c = (char)std::tolower((unsigned char)c);
			}
			return key;
		}

		void Evict()
		{
			while (g_cacheBytes > kCacheBudgetBytes && !g_order.empty())
			{
				const std::string& oldest = g_order.back();
				auto it = g_cache.find(oldest);
				if (it != g_cache.end())
				{
					g_cacheBytes -= it->second.image->pixels.size() * sizeof(uint32);
					g_cache.erase(it);
				}
				g_order.pop_back();
			}
		}
	}

	bool DecodeBlp(const uint8* data, size_t size, BlpImage& out)
	{
		if (!data || size < kHeaderSize || std::memcmp(data, "BLP2", 4) != 0)
			return false;
		Header h{};
		h.encoding = data[8];
		h.alphaDepth = data[9];
		h.alphaEncoding = data[10];
		h.hasMips = data[11];
		std::memcpy(&h.width, data + 12, 4);
		std::memcpy(&h.height, data + 16, 4);
		std::memcpy(&h.mipOffset, data + 20, 4);
		std::memcpy(&h.mipSize, data + 20 + 64, 4);
		h.palette = reinterpret_cast<const uint32*>(data + 20 + 128);
		if (h.width == 0 || h.height == 0 || h.width > 8192 || h.height > 8192)
			return false;
		if ((size_t)h.mipOffset + h.mipSize > size || h.mipSize == 0)
			return false;
		out.width = h.width;
		out.height = h.height;
		out.hasAlpha = h.alphaDepth != 0;
		out.pixels.assign((size_t)h.width * h.height, 0);
		const uint8* mip = data + h.mipOffset;
		switch (h.encoding)
		{
		case ENCODING_PALETTE:
			return DecodePalette(h, mip, out);
		case ENCODING_DXT:
			return DecodeDxt(h, mip, out);
		case ENCODING_RAW:
			return DecodeRaw(h, mip, out);
		default:
			return false;
		}
	}

	std::shared_ptr<const BlpImage> LoadBlp(const char* path)
	{
		if (!path || !*path)
			return nullptr;
		std::string key = CacheKey(path);
		{
			std::lock_guard<std::mutex> lock(g_cacheMutex);
			auto it = g_cache.find(key);
			if (it != g_cache.end())
			{
				g_order.splice(g_order.begin(), g_order, it->second.order);
				return it->second.image;
			}
		}
		std::vector<uint8> bytes;
		if (!ClientData::Streaming::ReadWholeFile(nullptr, path, bytes))
		{
			LOG_WARN << "hd characters: cannot read texture " << path;
			return nullptr;
		}
		auto image = std::make_shared<BlpImage>();
		if (!DecodeBlp(bytes.data(), bytes.size(), *image))
		{
			LOG_WARN << "hd characters: cannot decode texture " << path;
			return nullptr;
		}
		std::lock_guard<std::mutex> lock(g_cacheMutex);
		auto it = g_cache.find(key);
		if (it != g_cache.end())
		{
			g_order.splice(g_order.begin(), g_order, it->second.order);
			return it->second.image;
		}
		g_order.push_front(key);
		g_cache[key] = CacheEntry{ image, g_order.begin() };
		g_cacheBytes += image->pixels.size() * sizeof(uint32);
		Evict();
		return image;
	}

	void ClearBlpCache()
	{
		std::lock_guard<std::mutex> lock(g_cacheMutex);
		g_cache.clear();
		g_order.clear();
		g_cacheBytes = 0;
	}
}
