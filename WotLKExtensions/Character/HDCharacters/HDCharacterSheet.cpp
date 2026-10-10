#include <Defines.h>
#include <Character/HDCharacters/HDCharacterSheet.h>
#include <Character/HDCharacters/BlpImage.h>
#include <Character/HDCharacters/HDCharacters.h>
#include <Character/HDCharacters/HDManifest.h>
#include <Character/HDCharacters/HDSheetPool.h>

#include <ClientDetours.h>
#include <Logger.h>
#include <Macros.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace HDCharacters::Sheet
{
	namespace
	{
		using namespace HDCharacters::Component;

		constexpr uint32 kWidth = 1024;
		constexpr uint32 kHeight = 512;
		constexpr uint32 kBands = 8;
		constexpr uint32 kParallelMipRows = 64;
		constexpr uint32 kMipLevels = 11;
		constexpr size_t kSheetCacheMax = 32;
		constexpr double kReportMillis = 2.0;
		constexpr int kGxFormatArgb8888 = 2;
		constexpr int kGxApiWithExtraFlag = 5;
		constexpr uint32 kGxFlagExtra = 0x4000;
		constexpr int kSectionCount = 10;
		constexpr int kBodySectionCount = 8;
		constexpr int kSectionTorsoUpper = 3;
		constexpr int kSectionLegUpper = 5;
		constexpr int kCommandLock = 0;
		constexpr int kCommandLatch = 1;
		constexpr int kCommandUnlock = 2;
		constexpr int kCommandDestroy = 3;

		struct Rect
		{
			uint32 x, y, w, h;
		};

		constexpr Rect kRects[kSectionCount] = {
			{ 0, 0, 256, 128 },
			{ 0, 128, 256, 128 },
			{ 0, 256, 256, 64 },
			{ 256, 0, 256, 128 },
			{ 256, 128, 256, 64 },
			{ 256, 192, 256, 128 },
			{ 256, 320, 256, 128 },
			{ 256, 448, 256, 64 },
			{ 512, 0, 512, 512 },
			{ 512, 0, 512, 512 },
		};

		struct SlotRange
		{
			int first, end, firstRobe, endRobe;
		};

		constexpr SlotRange kSlots[kBodySectionCount] = {
			{ 0, 2, 0, 2 },
			{ 0, 7, 0, 7 },
			{ 0, 1, 0, 1 },
			{ 0, 5, 0, 5 },
			{ 0, 7, 0, 7 },
			{ 0, 3, 1, 2 },
			{ 0, 6, 1, 4 },
			{ 0, 1, 0, 1 },
		};

		struct SheetData
		{
			std::vector<std::vector<uint32>> mips;
			std::vector<uint32> widths;
			std::vector<uint32> heights;
		};

		struct PasteOp
		{
			std::string path;
			Rect rect;
			bool blend;
		};

		struct Recipe
		{
			std::vector<PasteOp> ops;
			std::string key;
		};

		struct Job
		{
			Recipe recipe;
			std::shared_ptr<const SheetData> result;
			std::atomic<bool> done{ false };
		};

		struct Slot
		{
			std::shared_ptr<const SheetData> ready;
			std::weak_ptr<const SheetData> uploaded;
			std::shared_ptr<Job> job;
			bool fresh = false;
		};

		struct CacheEntry
		{
			std::shared_ptr<const SheetData> sheet;
			std::list<std::string>::iterator order;
		};

		std::unordered_map<const void*, Slot> g_slots;
		std::mutex g_cacheMutex;
		std::unordered_map<std::string, CacheEntry> g_cache;
		std::list<std::string> g_cacheOrder;

		CLIENT_FUNCTION(TextureCreateSized, 0x004B9200, __cdecl, void*, (uint32 width, uint32 height, int format, int dataFormat, uint32 flags, void* userArg, void* userFunc, const char* name, int a9))
		CLIENT_FUNCTION(CGxTexFlagsConstruct, 0x00681BE0, __thiscall, int, (uint32 * self, int filter, char wrapU, char wrapV, char a4, char a5, char a6, unsigned int a7, char a8, char a9, char a10))
		CLIENT_FUNCTION(CGxDevice__DeviceApi, 0x00682D20, __thiscall, int, (void* device))
		CLIENT_FUNCTION(CGxDevice__CapsHasContext, 0x00682E20, __thiscall, int, (void* device, int query))
		CLIENT_FUNCTION(CM2Model__ReplaceTexture, 0x00825260, __thiscall, void, (void* model, int slot, void* texture))
		CLIENT_FUNCTION(TextureGetGxTex, 0x004B6CB0, __cdecl, void*, (void* texture, int a2, void* status))
		CLIENT_FUNCTION(GxTexUpdate, 0x00681F20, __cdecl, int, (void* gxTex, int x0, int y0, int x1, int y1, int immediate))
		CLIENT_FUNCTION(TSLink__Unlink, 0x0079F820, __thiscall, void, (void* link))
		CLIENT_FUNCTION(CCharacterComponent__RenderPrepSections, 0x004F14A0, __thiscall, void, (void* self))
		CLIENT_FUNCTION(CCharacterComponent__VariationsLoaded, 0x004ED640, __thiscall, char, (void* self, int wait))
		CLIENT_FUNCTION(CCharacterComponent__ItemsLoaded, 0x004ED6D0, __thiscall, char, (void* self, int wait))
		CLIENT_FUNCTION(CCharacterComponent__DestroyRenderTextures, 0x004E7650, __thiscall, void, (void* self))
		CLIENT_FUNCTION(CCharacterComponent__UpdateBaseTexture, 0x004EFDF0, __cdecl, void, (int command, uint32 width, int height, int depth, int mipLevel, void* userArg, uint32* stride, void** texels))
		CLIENT_ADDRESS(void*, g_theGxDevicePtr, 0x00C5DF88)
		CLIENT_ADDRESS(int, s_bInRenderPrep, 0x00B6B884)

		uint32 Field(const void* component, uint32 offset)
		{
			return *reinterpret_cast<const uint32*>(static_cast<const uint8*>(component) + offset);
		}

		void SetField(void* component, uint32 offset, uint32 value)
		{
			*reinterpret_cast<uint32*>(static_cast<uint8*>(component) + offset) = value;
		}

		const char* EntryPath(const void* component, uint32 offset)
		{
			const uint8* entry = *reinterpret_cast<const uint8* const*>(static_cast<const uint8*>(component) + offset);
			if (!entry)
				return nullptr;
			const char* path = reinterpret_cast<const char*>(entry + kCacheEntryPath);
			return *path ? path : nullptr;
		}

		bool IsHdTexturePath(const char* path)
		{
			return _strnicmp(path, "characterhd\\", 12) == 0;
		}

		double Millis(std::chrono::steady_clock::time_point start)
		{
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		inline uint32 Blend(uint32 dst, uint32 src)
		{
			uint32 a = src >> 24;
			if (a == 255)
				return src | 0xFF000000u;
			if (a == 0)
				return dst;
			uint32 ia = 255 - a;
			uint32 r = ((src >> 16 & 0xFF) * a + (dst >> 16 & 0xFF) * ia) / 255;
			uint32 g = ((src >> 8 & 0xFF) * a + (dst >> 8 & 0xFF) * ia) / 255;
			uint32 b = ((src & 0xFF) * a + (dst & 0xFF) * ia) / 255;
			return 0xFF000000u | (r << 16) | (g << 8) | b;
		}

		inline uint32 Sample(const BlpImage& img, float fx, float fy)
		{
			fx = std::clamp(fx, 0.0f, (float)(img.width - 1));
			fy = std::clamp(fy, 0.0f, (float)(img.height - 1));
			uint32 x0 = (uint32)fx;
			uint32 y0 = (uint32)fy;
			uint32 x1 = std::min(x0 + 1, img.width - 1);
			uint32 y1 = std::min(y0 + 1, img.height - 1);
			uint32 tx = (uint32)((fx - (float)x0) * 256.0f);
			uint32 ty = (uint32)((fy - (float)y0) * 256.0f);
			uint32 p00 = img.At(x0, y0), p10 = img.At(x1, y0), p01 = img.At(x0, y1), p11 = img.At(x1, y1);
			uint32 out = 0;
			for (int shift = 0; shift < 32; shift += 8)
			{
				uint32 c00 = (p00 >> shift) & 0xFF, c10 = (p10 >> shift) & 0xFF, c01 = (p01 >> shift) & 0xFF, c11 = (p11 >> shift) & 0xFF;
				uint32 top = c00 * (256 - tx) + c10 * tx;
				uint32 bottom = c01 * (256 - tx) + c11 * tx;
				uint32 c = (top * (256 - ty) + bottom * ty) >> 16;
				out |= (c & 0xFF) << shift;
			}
			return out;
		}

		void PasteRows(std::vector<uint32>& sheet, const Rect& rect, const BlpImage& img, bool blend, uint32 y0, uint32 y1)
		{
			const uint32 rowBegin = std::max(y0, rect.y);
			const uint32 rowEnd = std::min({ y1, rect.y + rect.h, kHeight });
			const uint32 cols = std::min(rect.w, kWidth - std::min(rect.x, kWidth));
			if (rowBegin >= rowEnd || !cols)
				return;
			if (img.width == rect.w && img.height == rect.h)
			{
				for (uint32 y = rowBegin; y < rowEnd; ++y)
				{
					uint32* row = &sheet[y * kWidth + rect.x];
					const uint32* src = &img.pixels[(y - rect.y) * img.width];
					if (blend)
					{
						for (uint32 x = 0; x < cols; ++x)
							row[x] = Blend(row[x], src[x]);
					}
					else
					{
						for (uint32 x = 0; x < cols; ++x)
							row[x] = src[x] | 0xFF000000u;
					}
				}
				return;
			}
			const float sx = (float)img.width / (float)rect.w;
			const float sy = (float)img.height / (float)rect.h;
			for (uint32 y = rowBegin; y < rowEnd; ++y)
			{
				uint32* row = &sheet[y * kWidth + rect.x];
				const float fy = ((float)(y - rect.y) + 0.5f) * sy - 0.5f;
				for (uint32 x = 0; x < cols; ++x)
				{
					uint32 s = Sample(img, ((float)x + 0.5f) * sx - 0.5f, fy);
					row[x] = blend ? Blend(row[x], s) : (s | 0xFF000000u);
				}
			}
		}

		void MipRows(const std::vector<uint32>& src, uint32 w, uint32 h, std::vector<uint32>& dst, uint32 nw, uint32 y0, uint32 y1)
		{
			for (uint32 y = y0; y < y1; ++y)
			{
				const uint32 sy0 = std::min(y * 2, h - 1), sy1 = std::min(y * 2 + 1, h - 1);
				for (uint32 x = 0; x < nw; ++x)
				{
					const uint32 sx0 = std::min(x * 2, w - 1), sx1 = std::min(x * 2 + 1, w - 1);
					const uint32 p[4] = { src[sy0 * w + sx0], src[sy0 * w + sx1], src[sy1 * w + sx0], src[sy1 * w + sx1] };
					uint32 out = 0;
					for (int shift = 0; shift < 32; shift += 8)
					{
						uint32 sum = 0;
						for (uint32 v : p)
							sum += (v >> shift) & 0xFF;
						out |= ((sum + 2) / 4) << shift;
					}
					dst[y * nw + x] = out;
				}
			}
		}

		void BuildMips(SheetData& sheet, bool parallel)
		{
			sheet.mips.resize(1);
			sheet.mips.reserve(kMipLevels);
			sheet.widths.assign(1, kWidth);
			sheet.heights.assign(1, kHeight);
			uint32 w = kWidth, h = kHeight;
			while (w > 1 || h > 1)
			{
				const uint32 nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
				const std::vector<uint32>& src = sheet.mips.back();
				std::vector<uint32> dst((size_t)nw * nh);
				if (parallel && nh >= kParallelMipRows)
				{
					Pool::Group group;
					const uint32 rows = nh / kBands;
					for (uint32 b = 0; b < kBands; ++b)
					{
						const uint32 y0 = b * rows;
						const uint32 y1 = b + 1 == kBands ? nh : y0 + rows;
						Pool::Submit([&src, w, h, &dst, nw, y0, y1]
						{
							MipRows(src, w, h, dst, nw, y0, y1);
						}, &group, true);
					}
					Pool::Wait(group);
				}
				else
					MipRows(src, w, h, dst, nw, 0, nh);
				sheet.mips.push_back(std::move(dst));
				sheet.widths.push_back(nw);
				sheet.heights.push_back(nh);
				w = nw;
				h = nh;
			}
		}

		const SheetData& Blank()
		{
			static const std::shared_ptr<const SheetData> blank = []
			{
				auto sheet = std::make_shared<SheetData>();
				sheet->mips.emplace_back((size_t)kWidth * kHeight, 0xFF000000u);
				BuildMips(*sheet, false);
				return sheet;
			}();
			return *blank;
		}

		Rect RectOf(int rect)
		{
			if (rect >= 0 && rect < kBodySectionCount)
				return kRects[rect];
			if (rect == Manifest::kRectFace)
				return kRects[8];
			if (rect == Manifest::kRectBody)
				return Rect{ 0, 0, 512, 512 };
			return Rect{ 0, 0, kWidth, kHeight };
		}

		void AddOp(Recipe& recipe, const char* path, const Rect& rect, bool blend)
		{
			if (!path || !*path)
				return;
			recipe.ops.push_back(PasteOp{ path, rect, blend });
		}

		void AddItemOps(Recipe& recipe, const void* component, int section)
		{
			const uint8* comp = static_cast<const uint8*>(component);
			const uint8* sectionBase = comp + kOffItemSections + section * kItemSectionStride;
			const uint32 mask = *reinterpret_cast<const uint32*>(sectionBase + kItemSectionMask);
			const uint32 flags = Field(component, kOffFlags);
			const bool robe = (flags & kFlagRobe) != 0;
			const SlotRange& range = kSlots[section];
			const int first = robe ? range.firstRobe : range.first;
			const int end = robe ? range.endRobe : range.end;
			if (section == kSectionTorsoUpper && !(mask & 7))
				AddOp(recipe, EntryPath(component, kOffTorsoUnderwear), kRects[section], true);
			if (section == kSectionLegUpper && (robe || !(mask & 3)))
				AddOp(recipe, EntryPath(component, kOffPelvisUnderwear), kRects[section], true);
			for (int slot = first; slot < end; ++slot)
			{
				if (!(mask & (1u << slot)))
					continue;
				const uint8* entry = *reinterpret_cast<const uint8* const*>(sectionBase + slot * 4);
				if (!entry)
					continue;
				AddOp(recipe, reinterpret_cast<const char*>(entry + kCacheEntryPath), kRects[section], true);
			}
		}

		bool ItemEntriesMissing(const void* component)
		{
			const uint8* comp = static_cast<const uint8*>(component);
			for (int section = 0; section < kItemSectionCount; ++section)
			{
				const uint8* sectionBase = comp + kOffItemSections + section * kItemSectionStride;
				for (int slot = 0; slot < kItemSlotCount; ++slot)
				{
					const uint32 displayId = *reinterpret_cast<const uint32*>(sectionBase + kItemSectionDisplayIds + slot * 4);
					const void* entry = *reinterpret_cast<const void* const*>(sectionBase + slot * 4);
					if (displayId && !entry)
						return true;
				}
			}
			return false;
		}

		void EnsureItems(void* component)
		{
			if (!ItemEntriesMissing(component))
				return;
			static int reports = 0;
			if (reports < 20)
			{
				++reports;
				LOG_INFO << "hd characters: item textures were released before the sheet composite (dirty mask " << Field(component, kOffDirty) << ", skin colour " << Field(component, kOffSkinColor) << "), reloading them";
			}
			SetField(component, kOffDirty, 0xFFFFFFFFu);
			CCharacterComponent__ItemsLoaded(component, 1);
		}

		std::string KeyOf(const std::vector<PasteOp>& ops)
		{
			std::string key;
			char buf[48];
			for (const PasteOp& op : ops)
			{
				std::snprintf(buf, sizeof(buf), "|%u,%u,%u,%u,%d:", op.rect.x, op.rect.y, op.rect.w, op.rect.h, op.blend ? 1 : 0);
				key += buf;
				for (char c : op.path)
					key += c == '/' ? '\\' : (char)std::tolower((unsigned char)c);
			}
			return key;
		}

		Recipe BuildRecipe(void* component, bool quiet)
		{
			EnsureItems(component);
			Recipe recipe;
			const Rect whole{ 0, 0, kWidth, kHeight };
			const char* skinPath = EntryPath(component, kOffSkin);
			if (!quiet)
			{
				if (!skinPath)
					LOG_WARN << "hd characters: component has no skin texture, sheet stays black";
				else if (!IsHdTexturePath(skinPath))
					LOG_WARN << "hd characters: skin fell back to the standard texture " << skinPath;
			}
			AddOp(recipe, skinPath, whole, false);
			for (int section = 0; section < kBodySectionCount; ++section)
				AddItemOps(recipe, component, section);
			if (const Manifest::Data* data = Manifest::Get((int)Field(component, kOffRace), (int)Field(component, kOffSex)))
			{
				std::vector<const Manifest::LayerLine*> layers;
				Manifest::ResolveLayers(*data, Manifest::LookOf(component), layers);
				for (const Manifest::LayerLine* layer : layers)
					AddOp(recipe, layer->path.c_str(), RectOf(layer->rect), true);
			}
			recipe.key = KeyOf(recipe.ops);
			return recipe;
		}

		std::shared_ptr<const SheetData> Compose(const Recipe& recipe, bool parallel)
		{
			std::unordered_map<std::string, std::shared_ptr<const BlpImage>> images;
			for (const PasteOp& op : recipe.ops)
				images.emplace(op.path, nullptr);
			if (parallel && images.size() > 1)
			{
				Pool::Group group;
				for (auto& entry : images)
				{
					auto* target = &entry;
					Pool::Submit([target]
					{
						target->second = LoadBlp(target->first.c_str());
					}, &group, true);
				}
				Pool::Wait(group);
			}
			else
			{
				for (auto& entry : images)
					entry.second = LoadBlp(entry.first.c_str());
			}
			auto sheet = std::make_shared<SheetData>();
			sheet->mips.reserve(kMipLevels);
			sheet->mips.emplace_back((size_t)kWidth * kHeight, 0xFF000000u);
			std::vector<uint32>& base = sheet->mips[0];
			auto paste = [&](uint32 y0, uint32 y1)
			{
				for (const PasteOp& op : recipe.ops)
				{
					auto it = images.find(op.path);
					if (it != images.end() && it->second)
						PasteRows(base, op.rect, *it->second, op.blend, y0, y1);
				}
			};
			if (parallel)
			{
				Pool::Group group;
				const uint32 rows = kHeight / kBands;
				for (uint32 b = 0; b < kBands; ++b)
				{
					const uint32 y0 = b * rows;
					const uint32 y1 = b + 1 == kBands ? kHeight : y0 + rows;
					Pool::Submit([&paste, y0, y1]
					{
						paste(y0, y1);
					}, &group, true);
				}
				Pool::Wait(group);
			}
			else
				paste(0, kHeight);
			BuildMips(*sheet, parallel);
			return sheet;
		}

		std::shared_ptr<const SheetData> CacheFind(const std::string& key)
		{
			std::lock_guard<std::mutex> lock(g_cacheMutex);
			auto it = g_cache.find(key);
			if (it == g_cache.end())
				return nullptr;
			g_cacheOrder.splice(g_cacheOrder.begin(), g_cacheOrder, it->second.order);
			return it->second.sheet;
		}

		void CacheInsert(const std::string& key, std::shared_ptr<const SheetData> sheet)
		{
			std::lock_guard<std::mutex> lock(g_cacheMutex);
			auto it = g_cache.find(key);
			if (it != g_cache.end())
			{
				it->second.sheet = std::move(sheet);
				g_cacheOrder.splice(g_cacheOrder.begin(), g_cacheOrder, it->second.order);
				return;
			}
			g_cacheOrder.push_front(key);
			g_cache[key] = CacheEntry{ std::move(sheet), g_cacheOrder.begin() };
			while (g_cache.size() > kSheetCacheMax && !g_cacheOrder.empty())
			{
				g_cache.erase(g_cacheOrder.back());
				g_cacheOrder.pop_back();
			}
		}

		void CompositeNow(void* component)
		{
			Recipe recipe = BuildRecipe(component, false);
			std::shared_ptr<const SheetData> sheet = CacheFind(recipe.key);
			if (!sheet)
			{
				const auto start = std::chrono::steady_clock::now();
				sheet = Compose(recipe, true);
				CacheInsert(recipe.key, sheet);
				const double ms = Millis(start);
				if (ms >= kReportMillis)
					LOG_INFO << "hd characters: sheet composed on the main thread in " << ms << " ms (" << recipe.ops.size() << " pastes, " << Pool::ThreadCount() << " helper threads)";
				else
					LOG_DEBUG << "hd characters: sheet composed on the main thread in " << ms << " ms";
			}
			Slot& slot = g_slots[component];
			slot.job.reset();
			slot.ready = std::move(sheet);
			slot.fresh = true;
		}

		void StartJob(Slot& slot, Recipe&& recipe)
		{
			auto job = std::make_shared<Job>();
			job->recipe = std::move(recipe);
			slot.job = job;
			Pool::Submit([job]
			{
				const auto start = std::chrono::steady_clock::now();
				std::shared_ptr<const SheetData> sheet = Compose(job->recipe, false);
				CacheInsert(job->recipe.key, sheet);
				job->result = std::move(sheet);
				job->done.store(true, std::memory_order_release);
				LOG_DEBUG << "hd characters: sheet composed on a worker thread in " << Millis(start) << " ms";
			}, nullptr, false);
		}

		bool Pin(void* component)
		{
			Slot& slot = g_slots[component];
			if (!slot.ready)
				slot.ready = slot.uploaded.lock();
			return slot.ready != nullptr;
		}

		void Unpin(const void* component)
		{
			auto it = g_slots.find(component);
			if (it == g_slots.end() || !it->second.ready)
				return;
			it->second.uploaded = it->second.ready;
			it->second.ready.reset();
			it->second.fresh = false;
		}

		void Prepare(void* component)
		{
			auto it = g_slots.find(component);
			if (it == g_slots.end() || !it->second.fresh || !it->second.ready)
				CompositeNow(component);
			g_slots[component].fresh = false;
		}

		void Upload(const void* component)
		{
			void* texture = *reinterpret_cast<void* const*>(static_cast<const uint8*>(component) + kOffBaseTexture);
			if (!texture)
				return;
			void* gxTex = TextureGetGxTex(texture, 1, nullptr);
			if (!gxTex)
				return;
			GxTexUpdate(gxTex, 0, 0, (int)kWidth, (int)kHeight, 1);
			Unpin(component);
		}

		void RenderPrepAll(void* component)
		{
			SetField(component, kOffFlags, Field(component, kOffFlags) | kFlagNeedsFullPrep);
			uint8* request = *reinterpret_cast<uint8**>(static_cast<uint8*>(component) + kOffRequest);
			if (request)
			{
				*reinterpret_cast<uint32*>(request) &= ~1u;
				SetField(component, kOffRequest, 0);
			}
			SetField(component, kOffDirty, 0xFFFFFFFFu);
			SetField(component, kOffFlags, Field(component, kOffFlags) & ~kFlagRequestQueued);
			CCharacterComponent__VariationsLoaded(component, 1);
			CCharacterComponent__ItemsLoaded(component, 1);
			CompositeNow(component);
			SetField(component, kOffFlags, Field(component, kOffFlags) & ~kFlagNeedsFullPrep);
			SetField(component, kOffDirty, 0);
			CCharacterComponent__DestroyRenderTextures(component);
			TSLink__Unlink(component);
		}

		void __cdecl SheetTextureCallback(int command, uint32 width, int height, int depth, int mipLevel, void* userArg, uint32* stride, void** texels)
		{
			void* component = userArg;
			switch (command)
			{
			case kCommandLock:
			{
				if (*s_bInRenderPrep)
					break;
				if (!Pin(component))
					RenderPrepAll(component);
				break;
			}
			case kCommandLatch:
			{
				Pin(component);
				auto it = g_slots.find(component);
				const SheetData& sheet = (it != g_slots.end() && it->second.ready) ? *it->second.ready : Blank();
				size_t level = std::min<size_t>((size_t)std::max(mipLevel, 0), sheet.mips.size() - 1);
				if (stride)
					*stride = sheet.widths[level] * 4;
				if (texels)
					*texels = const_cast<uint32*>(sheet.mips[level].data());
				break;
			}
			case kCommandUnlock:
				if (!*s_bInRenderPrep)
					Unpin(component);
				break;
			case kCommandDestroy:
			{
				CCharacterComponent__UpdateBaseTexture(command, width, height, depth, mipLevel, userArg, stride, texels);
				auto it = g_slots.find(component);
				if (it != g_slots.end())
				{
					it->second.ready.reset();
					it->second.uploaded.reset();
					it->second.fresh = false;
				}
				break;
			}
			default:
				break;
			}
		}

		bool HasGxContext()
		{
			return *g_theGxDevicePtr && CGxDevice__CapsHasContext(*g_theGxDevicePtr, -1) != 0;
		}

		void* CreateBaseTexture(void* component)
		{
			uint32 flags = 0;
			CGxTexFlagsConstruct(&flags, 4, 0, 0, 0, 0, 0, 1, 0, 0, 0);
			if (*g_theGxDevicePtr && CGxDevice__DeviceApi(*g_theGxDevicePtr) == kGxApiWithExtraFlag)
				flags |= kGxFlagExtra;
			void* texture = TextureCreateSized(kWidth, kHeight, kGxFormatArgb8888, kGxFormatArgb8888, flags, component, reinterpret_cast<void*>(&SheetTextureCallback), "CharacterBaseSkinHD", 1);
			SetField(component, kOffBaseTexture, reinterpret_cast<uint32>(texture));
			void* model = *reinterpret_cast<void**>(static_cast<uint8*>(component) + kOffModel);
			if (model && texture)
				CM2Model__ReplaceTexture(model, 1, texture);
			return texture;
		}

		bool Request(void* component)
		{
			if (!HasGxContext())
				return false;
			Slot& slot = g_slots[component];
			Recipe recipe = BuildRecipe(component, slot.job != nullptr);
			std::shared_ptr<const SheetData> sheet;
			if (slot.job)
			{
				if (slot.job->recipe.key != recipe.key)
					slot.job.reset();
				else if (!slot.job->done.load(std::memory_order_acquire))
					return true;
				else
				{
					sheet = slot.job->result;
					slot.job.reset();
				}
			}
			if (!sheet)
				sheet = CacheFind(recipe.key);
			if (!sheet)
			{
				StartJob(slot, std::move(recipe));
				return true;
			}
			slot.ready = std::move(sheet);
			slot.fresh = true;
			CCharacterComponent__RenderPrepSections(component);
			TSLink__Unlink(component);
			return true;
		}
	}

	void Destroy(const void* component)
	{
		g_slots.erase(component);
	}

	void ClearCache()
	{
		std::lock_guard<std::mutex> lock(g_cacheMutex);
		g_cache.clear();
		g_cacheOrder.clear();
	}
}

CLIENT_DETOUR_THISCALL_NOARGS(CCharacterComponent__CreateBaseTexture_HD, 0x004EFF10, void*)
{
	if (!HDCharacters::IsHdComponent(self))
		return CCharacterComponent__CreateBaseTexture_HD(self);
	return HDCharacters::Sheet::CreateBaseTexture(self);
}

CLIENT_DETOUR_THISCALL_NOARGS(CCharacterComponent__PrepSections_HD, 0x004EE0D0, void)
{
	if (!HDCharacters::IsHdComponent(self))
	{
		CCharacterComponent__PrepSections_HD(self);
		return;
	}
	HDCharacters::Sheet::Prepare(self);
	HDCharacters::Sheet::Upload(self);
	HDCharacters::ApplySlots(self);
}

CLIENT_DETOUR_THISCALL_NOARGS(CCharacterComponent__CreateComponentRequest_HD, 0x004F1790, int)
{
	if (!HDCharacters::IsHdComponent(self))
		return CCharacterComponent__CreateComponentRequest_HD(self);
	return HDCharacters::Sheet::Request(self) ? 1 : 0;
}
