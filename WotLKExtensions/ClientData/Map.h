#pragma once

#include <Macros.h>
#include <ClientData/AdtTypes.h>
#include <ClientData/MathTypes.h>

#include <cstddef>
#include <cstdint>

// The client's CMap terrain subsystem: the live structs it builds per tile, and the globals and
// calls the map editor drives them through.
namespace ClientData
{
	struct TSExplicitList
	{
		int32_t linkOffset;
		void* prevLink;
		void* next;
	};

	struct CMapChunk;

	// CMapDoodadDefMapChunkLink and CMapObjDefMapChunkLink share this layout. One link is threaded
	// on two lists at once, refLink on the chunk's and ownerLink on the def's.
	struct CMapDefChunkLink
	{
		uint32_t objectIndex; // 0x00
		void* owner;          // 0x04 the CMapDoodadDef or CMapObjDef
		CMapChunk* ref;       // 0x08
		void* refLink[2];     // 0x0C
		void* ownerLink[2];   // 0x14
	};

	struct CMapChunk
	{
		void** vtable;                // 0x000
		uint32_t objectIndex;         // 0x004
		uint16_t type;                // 0x008
		uint16_t refCount;            // 0x00A
		uint32_t unk_0C;              // 0x00C
		CMapChunk* prev;              // 0x010
		CMapChunk* next;              // 0x014
		TSExplicitList linkList;      // 0x018
		C2iVector aIndex;             // 0x024 chunk index within the tile
		C2iVector sOffset;            // 0x02C
		C2iVector cOffset;            // 0x034 global chunk index across the whole map
		C3Vector center;              // 0x03C
		float radius;                 // 0x048
		CAaBox bbox;                  // 0x04C
		C3Vector bottomRight;         // 0x064
		C3Vector topLeft;             // 0x070
		C3Vector topLeftCoords;       // 0x07C vertex heights are relative to this z
		float distToCamera;           // 0x088
		CAaBox bbox2;                 // 0x08C
		void* detailDoodadInst;       // 0x0A4
		void* renderChunk;            // 0x0A8
		int32_t hasRenderChunk;       // 0x0AC set once CMapChunk::CreateRenderChunk has run
		int32_t areaId;               // 0x0B0
		int32_t unk_B4[4];            // 0x0B4
		TSExplicitList doodadDefLink; // 0x0C4
		TSExplicitList mapObjDefLink; // 0x0D0
		TSExplicitList unkLink_DC;    // 0x0DC
		TSExplicitList unkLink_E8;    // 0x0E8
		TSExplicitList unkLink_F4;    // 0x0F4
		TSExplicitList liquidLink;    // 0x100
		uint8_t* chunkInfoBeginPtr;   // 0x10C points at the MCNK IFF header inside filePtr
		SMChunk* header;              // 0x110
		uint8_t* lowQualityTexMap;    // 0x114
		uint8_t* predTexture;         // 0x118
		float* vertices;              // 0x11C MCVT, 145 floats
		uint32_t* vertexShading;      // 0x120 MCCV
		int8_t* normals;              // 0x124 MCNR
		uint8_t* shadowMap;           // 0x128 MCSH
		SMLayer* layers;              // 0x12C MCLY
		uint8_t* alpha;               // 0x130 MCAL, mislabelled additionalShadowmap in IDA
		uint8_t* refs;                // 0x134 MCRF
		void* liquid;                 // 0x138 MCLQ
		void* soundEmitters;          // 0x13C MCSE
		int32_t unk_140[6];           // 0x140
	};

	struct CMapArea
	{
		void** vtable;                    // 0x000
		uint32_t objectIndex;             // 0x004
		uint16_t type;                    // 0x008
		uint16_t refCount;                // 0x00A
		uint32_t unk_0C;                  // 0x00C
		CMapArea* prev;                   // 0x010
		CMapArea* next;                   // 0x014
		TSExplicitList linkList;          // 0x018
		C3Vector bottomRight;             // 0x024
		C3Vector topLeft;                 // 0x030
		C3Vector topLeft2;                // 0x03C
		C2iVector index;                  // 0x048 the x and y in <Map>_<x>_<y>.adt
		C2iVector tileChunkIndex;         // 0x050 index * 16
		uint32_t textureAlloc;            // 0x058
		uint32_t textureCount;            // 0x05C
		void* textureData;                // 0x060
		uint32_t textureChunk;            // 0x064
		SMMapHeader* header;              // 0x068
		int32_t unk_6C;                   // 0x06C
		void* asyncObject;                // 0x070 non-null while the tile is still streaming
		TSExplicitList chunkLinkList;     // 0x074
		void* filePtr;                    // 0x080 the whole raw .adt file image
		int32_t fileSize;                 // 0x084
		SMChunkInfo* chunkInfo;           // 0x088 MCIN, 256 entries
		int32_t unk_8C;                   // 0x08C
		SMDoodadDef* doodadDef;           // 0x090 MDDF
		SMMapObjDef* mapObjDef;           // 0x094 MODF
		int32_t doodadDefCount;           // 0x098
		int32_t mapObjDefCount;           // 0x09C
		char* m2FileNames;                // 0x0A0 MMDX string blob
		char* wmoFileNames;               // 0x0A4 MWMO string blob
		uint32_t* modelFilenamesOffsets;  // 0x0A8 MMID
		uint32_t* wmoFilenamesOffsets;    // 0x0AC MWID
		int16_t* flyingBbox;              // 0x0B0 MFBO
		int32_t* textureFlags;            // 0x0B4 MTXF
		uint8_t* unk_B8;                  // 0x0B8
		CMapChunk* mapChunks[256];        // 0x0BC indexed cy * 16 + cx
	};

	// The live instance the client builds from one SMDoodadDef, keyed by uniqueId in a global hash
	// so a doodad listed in two neighbouring tiles still gets a single instance. `mat` is the whole
	// transform and the only thing the renderer reads.
	struct CMapDoodadDef
	{
		void** vtable;              // 0x000
		uint32_t objectIndex;       // 0x004
		uint16_t type;              // 0x008 bit 0x40 marks a doodad
		uint16_t refCount;          // 0x00A
		uint32_t flags;             // 0x00C 1, or 0x801 when the MDDF entry had bit 0 set
		CMapDoodadDef* prev;        // 0x010
		CMapDoodadDef* next;        // 0x014
		TSExplicitList linkList;    // 0x018 the chunk links this doodad is threaded onto
		uint8_t unk_24[0x10];       // 0x024
		void* model;                // 0x034 CM2Model, null until the m2 has streamed in
		C3Vector sphereCenter;      // 0x038
		float sphereRadius;         // 0x044
		CAaBox bboxStaticEntity;    // 0x048
		C3Vector linkPos;           // 0x060 what CMap::LinkStaticEntitySingle2 reads
		C3Vector position;          // 0x06C world space
		float scale;                // 0x078
		uint8_t unk_7C[0x14];       // 0x07C
		uint32_t uniqueId;          // 0x090 straight off the MDDF entry
		uint8_t unk_94[0x10];       // 0x094
		uint32_t group;             // 0x0A4 0 for a plain MDDF doodad
		void* purgeLink[2];         // 0x0A8
		uint8_t unk_B0[0x10];       // 0x0B0
		CAaBox bboxDoodadDef;       // 0x0C0 world space, only valid once the model has loaded
		C44Matrix mat;              // 0x0D8
		C44Matrix identity;         // 0x118
		uint8_t unk_158[0x18];      // 0x158
	};

	// The live instance built from one SMMapObjDef. The def hash at 0x00D25434 writes the MODF
	// uniqueId to 0x24, which is the only stable way to match a live WMO back to its file entry
	// (nameId indexes the MWID of whichever tile built the instance).
	struct CMapObjDef
	{
		void** vtable;                        // 0x000
		uint32_t objectIndex;                 // 0x004
		uint16_t type;                        // 0x008 bit 0x8 marks a map object
		uint16_t refCount;                    // 0x00A
		uint32_t flags;                       // 0x00C
		CMapObjDef* prev;                     // 0x010
		CMapObjDef* next;                     // 0x014
		TSExplicitList linkList;              // 0x018
		uint32_t uniqueId;                    // 0x024 the MODF uniqueId, written by the def hash
		void* hashLink[2];                    // 0x028
		void* allDefsLink[2];                 // 0x030
		uint32_t unk_38;                      // 0x038
		C3Vector position;                    // 0x03C world space
		CAaBox bbox;                          // 0x048 world space
		C3Vector sphereCenter;                // 0x060
		float sphereRadius;                   // 0x06C
		C44Matrix mat;                        // 0x070
		C44Matrix invMat;                     // 0x0B0
		uint32_t nameId;                      // 0x0F0 indexes MWID
		void* owner;                          // 0x0F4 CMapObj
		uint32_t unk_F8;                      // 0x0F8
		uint32_t unkFlags;                    // 0x0FC
		uint32_t doodadSet;                   // 0x100
		uint32_t nameSet;                     // 0x104
		uint8_t unk_108[0x0C];                // 0x108
		TSExplicitList mapObjDefGroupLinkList; // 0x114
		uint8_t unk_120[0x24];                // 0x120
		uint32_t argbColor;                   // 0x144
		uint8_t unk_148[0x10];                // 0x148
	};

#define CLIENTDATA_MAP_OFFSET_CHECK(type, member, offset) \
	static_assert(offsetof(type, member) == offset, #type "::" #member " moved")

	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, cOffset, 0x034);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, bbox, 0x04C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, topLeftCoords, 0x07C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, renderChunk, 0x0A8);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, areaId, 0x0B0);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, chunkInfoBeginPtr, 0x10C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, header, 0x110);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, vertices, 0x11C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, normals, 0x124);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, layers, 0x12C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, alpha, 0x130);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapChunk, refs, 0x134);
	static_assert(sizeof(CMapChunk) == 344, "CMapChunk must be 344 bytes");

	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, index, 0x048);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, header, 0x068);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, asyncObject, 0x070);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, filePtr, 0x080);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, chunkInfo, 0x088);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, doodadDef, 0x090);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, m2FileNames, 0x0A0);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapArea, mapChunks, 0x0BC);
	static_assert(sizeof(CMapArea) == 1212, "CMapArea must be 1212 bytes");

	CLIENTDATA_MAP_OFFSET_CHECK(CMapDefChunkLink, owner, 0x04);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDefChunkLink, ref, 0x08);
	static_assert(sizeof(CMapDefChunkLink) == 28, "CMapDefChunkLink must be 28 bytes");

	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, linkList, 0x018);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, model, 0x034);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, linkPos, 0x060);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, position, 0x06C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, scale, 0x078);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, uniqueId, 0x090);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, group, 0x0A4);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, bboxDoodadDef, 0x0C0);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, mat, 0x0D8);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapDoodadDef, identity, 0x118);
	static_assert(sizeof(CMapDoodadDef) == 368, "CMapDoodadDef must be 368 bytes");

	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, linkList, 0x018);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, uniqueId, 0x024);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, position, 0x03C);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, bbox, 0x048);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, mat, 0x070);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, invMat, 0x0B0);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, nameId, 0x0F0);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, doodadSet, 0x100);
	CLIENTDATA_MAP_OFFSET_CHECK(CMapObjDef, argbColor, 0x144);
	static_assert(sizeof(CMapObjDef) == 344, "CMapObjDef must be 344 bytes");

#undef CLIENTDATA_MAP_OFFSET_CHECK

	namespace Map
	{
		// CMapArea*[64 * 64], indexed index.y * 64 + index.x. Proven by CMap::PrepareArea, which
		// does `eax = (arg_4 << 6) + arg_0` before storing.
		CLIENT_ADDRESS(CMapArea*, sAreaTable, 0x00CE48D0)
		CLIENT_ADDRESS(char, sMapName, 0x00CE06D0)  // e.g. "Azeroth"
		CLIENT_ADDRESS(char, sMapPath, 0x00CE07D0)  // e.g. "World\Maps\Azeroth"
		CLIENT_ADDRESS(char, sWdtFilename, 0x00CE05D0)
		CLIENT_ADDRESS(int32_t, sMapActive, 0x00CF08F0)
		CLIENT_ADDRESS(int32_t, sMapIsDungeon, 0x00CF08F4)

		// The WDT's MPHD, read straight off disk by the WDT loader at 0x007BF92D, flags first. Bit
		// 2 is big alpha, which is what CMapRenderChunk::CreateLayerTexture tests to decide whether
		// MCAL holds 8 bit or 4 bit masks.
		CLIENT_ADDRESS(uint32_t, sMapFlags, 0x00CF08D0)

		constexpr uint32_t kMapFlagBigAlpha = 0x4;

		inline bool UsesBigAlpha()
		{
			return (*sMapFlags & kMapFlagBigAlpha) != 0;
		}

		CLIENT_FUNCTION(CMap_PurgeArea, 0x007C3700, __cdecl, void, (CMapArea * area))
		CLIENT_FUNCTION(CMap_PrepareArea, 0x007D9A70, __cdecl, CMapArea*, (int32_t x, int32_t y))
		CLIENT_FUNCTION(CMap_LoadArea, 0x007D9A20, __cdecl, void, (CMapArea * area))

		// Rebuilds bbox/center/radius from the current MCVT. Must be called after any height edit
		// or culling and the collision broadphase go stale.
		CLIENT_FUNCTION(CMapChunk_CreateBounds, 0x007C5220, __thiscall, void, (CMapChunk * chunk))
		// Drops the chunk's GPU buffer so RenderPrepBufs rebuilds it from MCVT/MCNR next frame.
		CLIENT_FUNCTION(CMapRenderChunk_FreeBuf, 0x007B9830, __thiscall, void, (void* renderChunk))

		// Releases every layer texture and zeroes the layer count, which is exactly what
		// CMapRenderChunk::RenderPrep tests before rebuilding them from the current MCLY and MCAL.
		// Call after any texture edit.
		CLIENT_FUNCTION(CMapRenderChunk_FreeLayers, 0x007B7350, __thiscall, void, (void* renderChunk))

		// At load time the client merges two adjacent chunks into one render chunk when their layer
		// sets match exactly. CMapRenderChunk::CreateLayers then walks both owners, at
		// renderChunk+0x10 and +0x14, into a fixed four slot array, deduplicating the second
		// against the first but never checking it has room. Identical sets always fit, so nothing
		// bounds checks. The moment an edit makes them diverge the fifth layer lands on the gx
		// buffer pointer at renderChunk+0x8C. The second owner is null on an unpaired chunk.
		constexpr uint32_t kRenderChunkOwnerA = 0x10;
		constexpr uint32_t kRenderChunkOwnerB = 0x14;

		// Unlinks and destructs a render chunk, clearing renderChunk on both of its owners.
		CLIENT_FUNCTION(CMap_FreeRenderChunk, 0x007C0610, __cdecl, void, (void* renderChunk))
		// The client's own unpaired path, one fresh render chunk for one chunk.
		CLIENT_FUNCTION(CMapChunk_CreateRenderChunk, 0x007C3B40, __thiscall, void,
		    (CMapChunk * chunk))

		// Breaks a shared render chunk apart so an edit to one chunk's layer set cannot overflow
		// the four slots the pair has between them. No-op when the chunk already owns its own.
		inline void SplitRenderChunk(CMapChunk* chunk)
		{
			if (!chunk || !chunk->renderChunk)
				return;

			uint8_t* rc = static_cast<uint8_t*>(chunk->renderChunk);
			CMapChunk* a = *reinterpret_cast<CMapChunk**>(rc + kRenderChunkOwnerA);
			CMapChunk* b = *reinterpret_cast<CMapChunk**>(rc + kRenderChunkOwnerB);
			if (!a || !b || a == b)
				return;

			// This clears renderChunk and the flag beside it on both owners.
			CMap_FreeRenderChunk(rc);

			CMapChunk_CreateRenderChunk(a);
			a->hasRenderChunk = 1;
			CMapChunk_CreateRenderChunk(b);
			b->hasRenderChunk = 1;
		}

		// Resizes the tile's texture list, a growable array whose four fields are
		// CMapArea::textureAlloc through textureChunk, so `this` is `&area->textureAlloc`. It goes
		// through SMemReAlloc, which matters because CMapArea's destructor SMemFrees textureData.
		CLIENT_FUNCTION(CMapArea_SetTextureCapacity, 0x007C30B0, __thiscall, void,
		    (void* array, uint32_t capacity))

		// (start, end, distanceInOut, flags, resultOut). distance starts at 1.0 and comes back as
		// the fraction along the ray.
		CLIENT_FUNCTION(CMap_VectorIntersectTerrain, 0x007A39F0, __cdecl, char,
		    (C3Vector * start, C3Vector * end, float* distance, uint32_t flags, uint32_t* result))

		// Every test in CMap::VectorIntersectSubChunks is opt-in, so a flags value of 0 always
		// misses. The client itself passes 0x100 for terrain and 0x20100 for terrain plus liquid.
		constexpr uint32_t kIntersectTerrain = 0x100;      // gates CMapChunk::Intersect at 0x007A3750
		constexpr uint32_t kIntersectLiquid = 0x30000;     // gates the CChunkLiquid tests
		constexpr uint32_t kIntersectDoodads = 0x0000000F; // CMap::VectorIntersectDoodadDefs
		constexpr uint32_t kIntersectEntities = 0x40F00000;

		constexpr int32_t kTilesPerSide = 64;
		constexpr int32_t kChunksPerTileSide = 16;
		constexpr int32_t kChunksPerTile = kChunksPerTileSide * kChunksPerTileSide;
		constexpr int32_t kVertsPerChunk = 145;

		// Rebuilds the world transform of one doodad from a matrix, rederiving position and uniform
		// scale out of it, then re-runs bounds and relinks it onto whichever chunks it now overlaps.
		CLIENT_FUNCTION(CMapDoodadDef_UpdateMatrix, 0x007B5870, __cdecl, void,
		    (CMapDoodadDef * def, C44Matrix* mat))

		// (def, worldPos, rotZ, rotY, rotX) in radians, matching the order CMap::CreateMapObjDef
		// applies them. It rebuilds the matrix, its inverse and the bounds, but leaves the chunk
		// links as the loader made them.
		CLIENT_FUNCTION(CMapObjDef_UpdatePos, 0x007B66E0, __cdecl, void,
		    (CMapObjDef * def, C3Vector* position, float rotZ, float rotY, float rotX))

		CLIENT_FUNCTION(C44Matrix_RotateAroundX, 0x004C3300, __thiscall, void, (C44Matrix * mat, float radians))
		CLIENT_FUNCTION(C44Matrix_RotateAroundY, 0x004C3340, __thiscall, void, (C44Matrix * mat, float radians))
		CLIENT_FUNCTION(C44Matrix_RotateAroundZ, 0x004C3380, __thiscall, void, (C44Matrix * mat, float radians))
		CLIENT_FUNCTION(C44Matrix_Scale, 0x004C1BF0, __thiscall, void, (C44Matrix * mat, float scale))

		// The global hash of live doodads keyed by MDDF uniqueId, which is what makes the uniqueId
		// a usable handle from our side.
		CLIENT_ADDRESS(uint8_t, sDoodadDefTable, 0x00D2545C)

		// (table, uniqueId, group), where `group` is 0 for a plain MDDF doodad and non-zero only
		// for the ones a WMO owns. Returns null when nothing matches, including when no map is
		// loaded.
		CLIENT_FUNCTION(CMapDoodadDefTable_Find, 0x007BDD80, __thiscall, CMapDoodadDef*,
		    (void* table, uint32_t uniqueId, uint32_t* group))

		// The same idea for WMOs. CMap::CreateMapObjDef adds every instance and the destructor takes
		// it back out, so a hit is by definition a live instance.
		CLIENT_ADDRESS(uint8_t, sMapObjDefTable, 0x00D25434)

		// (table, uniqueId, tag). The tag is a hash policy marker the lookup never reads, the client
		// passes a pointer to a spare byte.
		CLIENT_FUNCTION(CMapObjDefTable_Find, 0x007BDDF0, __thiscall, CMapObjDef*,
		    (void* table, uint32_t uniqueId, void* tag))

		// SFile2 keeps a cached index of every loose file under the data tree, guarded by this flag.
		// FindFile rebuilds the whole index whenever it reads 0, so zeroing it makes a file written
		// after startup discoverable instead of the client serving the MPQ copy forever.
		CLIENT_ADDRESS(uint8_t, sLooseFileIndexBuilt, 0x00B324A4)

		inline bool IsActive()
		{
			return *sMapActive != 0;
		}

		inline bool IsValidTile(int32_t tileX, int32_t tileY)
		{
			return tileX >= 0 && tileX < kTilesPerSide && tileY >= 0 && tileY < kTilesPerSide;
		}

		inline CMapArea* GetArea(int32_t tileX, int32_t tileY)
		{
			if (!IsValidTile(tileX, tileY))
				return nullptr;

			return sAreaTable[tileY * kTilesPerSide + tileX];
		}

		// A tile is only safe to read or edit once its async load has finished and Create() has
		// resolved the header. Every caller that touches filePtr must gate on this.
		inline bool IsAreaReady(CMapArea* area)
		{
			return area && !area->asyncObject && area->filePtr && area->header;
		}

		inline CMapChunk* GetChunk(CMapArea* area, int32_t chunkX, int32_t chunkY)
		{
			if (!area || chunkX < 0 || chunkX >= kChunksPerTileSide || chunkY < 0 || chunkY >= kChunksPerTileSide)
				return nullptr;

			return area->mapChunks[chunkY * kChunksPerTileSide + chunkX];
		}

		inline SMChunkInfo* GetChunkInfo(CMapArea* area, int32_t chunkX, int32_t chunkY)
		{
			if (!area || !area->chunkInfo)
				return nullptr;
			if (chunkX < 0 || chunkX >= kChunksPerTileSide || chunkY < 0 || chunkY >= kChunksPerTileSide)
				return nullptr;

			return &area->chunkInfo[chunkY * kChunksPerTileSide + chunkX];
		}

		// Adds a texture to the tile's list and hands back its index, the number SMLayer::textureId
		// holds. `name` has to outlive the loaded tile since the entry only borrows it, and leaving
		// the texture unloaded is deliberate because CMapRenderChunk::CreateLayer pulls the file in
		// itself when the entry's second field is null.
		inline bool AppendTerrainTexture(CMapArea* area, char const* name, uint32_t& textureId)
		{
			if (!area || !name)
				return false;

			uint32_t need = area->textureCount + 1;
			if (need > area->textureAlloc)
			{
				// Same rounding LoadTextures uses, up to a whole multiple of the growth step.
				uint32_t step = area->textureChunk ? area->textureChunk : 16;
				CMapArea_SetTextureCapacity(&area->textureAlloc, ((need + step - 1) / step) * step);
			}

			if (!area->textureData || area->textureAlloc < need)
				return false;

			SMTerrainTexture* entries = static_cast<SMTerrainTexture*>(area->textureData);
			entries[area->textureCount].name = const_cast<char*>(name);
			entries[area->textureCount].texture = nullptr;

			textureId = area->textureCount;
			area->textureCount = need;
			return true;
		}

		inline const char* DoodadFileName(CMapArea* area, uint32_t nameId)
		{
			if (!area || !area->m2FileNames || !area->modelFilenamesOffsets)
				return nullptr;

			return area->m2FileNames + area->modelFilenamesOffsets[nameId];
		}

		// MTEX resolved into the loaded texture list, which is where a layer's textureId ends up
		// pointing once CMap::LoadTerrainTexture has been through it.
		inline const char* TerrainTextureName(CMapArea* area, uint32_t textureId)
		{
			if (!area || !area->textureData || textureId >= area->textureCount)
				return nullptr;

			return static_cast<SMTerrainTexture*>(area->textureData)[textureId].name;
		}

		inline const char* MapObjFileName(CMapArea* area, uint32_t nameId)
		{
			if (!area || !area->wmoFileNames || !area->wmoFilenamesOffsets)
				return nullptr;

			return area->wmoFileNames + area->wmoFilenamesOffsets[nameId];
		}

		// MCRF is a flat u32[nDoodadRefs + nMapObjRefs]: doodad indices into this tile's MDDF
		// first, then WMO indices into its MODF.
		inline const uint32_t* DoodadRefs(CMapChunk* chunk, uint32_t& count)
		{
			count = chunk && chunk->header ? chunk->header->nDoodadRefs : 0;
			return chunk ? reinterpret_cast<const uint32_t*>(chunk->refs) : nullptr;
		}

		inline const uint32_t* MapObjRefs(CMapChunk* chunk, uint32_t& count)
		{
			if (!chunk || !chunk->header || !chunk->refs)
			{
				count = 0;
				return nullptr;
			}

			count = chunk->header->nMapObjRefs;
			return reinterpret_cast<const uint32_t*>(chunk->refs) + chunk->header->nDoodadRefs;
		}

		// Walks a TSExplicitList the way CMap::LinkStaticEntity does: the head is `next`, each node's
		// own next sits at `node + linkOffset + 4`, and the tail marks itself with the low bit set
		// rather than by being null. `fn` may unlink the node it is handed, so next is read first.
		template <typename Fn>
		void ForEachLink(TSExplicitList const& list, Fn&& fn)
		{
			auto* node = static_cast<uint8_t*>(list.next);
			while (node && (reinterpret_cast<uintptr_t>(node) & 1) == 0)
			{
				auto* next = *reinterpret_cast<uint8_t**>(node + list.linkOffset + 4);
				fn(node);
				node = next;
			}
		}

		inline CMapDoodadDef* FindDoodadDef(uint32_t uniqueId)
		{
			uint32_t group = 0;
			return CMapDoodadDefTable_Find(sDoodadDefTable, uniqueId, &group);
		}

		inline CMapObjDef* FindMapObjDef(uint32_t uniqueId)
		{
			return CMapObjDefTable_Find(sMapObjDefTable, uniqueId, nullptr);
		}

		inline void InvalidateLooseFileIndex()
		{
			*sLooseFileIndexBuilt = 0;
		}

		// Purging is all it takes to reload a tile from disk. CMap::PreUpdateAreas notices the null
		// slot and re-prepares plus re-loads it next frame, and CMapArea::Load re-opens through
		// SFile, so file contents are never cached, only the name index is.
		inline void ReloadTile(int32_t tileX, int32_t tileY)
		{
			InvalidateLooseFileIndex();

			if (CMapArea* area = GetArea(tileX, tileY))
				CMap_PurgeArea(area);
		}

		inline bool RaycastTerrain(C3Vector const& start, C3Vector const& end, C3Vector& hit,
		    float* fraction = nullptr)
		{
			C3Vector rayStart = start;
			C3Vector rayEnd = end;
			float distance = 1.0f;
			uint32_t result = 0;

			char didHit = CMap_VectorIntersectTerrain(&rayStart, &rayEnd, &distance, kIntersectTerrain, &result);
			if (fraction)
				*fraction = distance;
			if (!didHit)
				return false;

			hit.x = start.x + (end.x - start.x) * distance;
			hit.y = start.y + (end.y - start.y) * distance;
			hit.z = start.z + (end.z - start.z) * distance;
			return true;
		}
	}
}
