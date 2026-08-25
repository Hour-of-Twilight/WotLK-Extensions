#pragma once

#include <ClientData/MathTypes.h>

#include <cstddef>
#include <cstdint>

// The raw .adt chunk layouts. Sizes are asserted at the bottom so a bad edit fails the build
// rather than the client.
namespace ClientData
{
	// MHDR. Every offset is relative to the start of MHDR's data and points at the target chunk's
	// 8-byte IFF header, not its data.
	struct SMMapHeader
	{
		uint32_t flags;
		uint32_t mcin;
		uint32_t mtex;
		uint32_t mmdx;
		uint32_t mmid;
		uint32_t mwmo;
		uint32_t mwid;
		uint32_t mddf;
		uint32_t modf;
		uint32_t mfbo;
		uint32_t mh2o;
		uint32_t mtxf;
		uint8_t mampValue;
		uint8_t padding[3];
		uint32_t unused[3];
	};

	// MCIN entry. offset is absolute from the start of the file and points at the MCNK header.
	struct SMChunkInfo
	{
		uint32_t offset;
		uint32_t size;
		uint32_t flags; // bit 0 is set by the client once the chunk has been prepared
		uint32_t asyncId;
	};

	// MCNK header. sizeAlpha and sizeLiquid include the 8-byte IFF header of their sub-chunk.
	struct SMChunk
	{
		uint32_t flags;
		C2iVector index;
		uint32_t nLayers;
		uint32_t nDoodadRefs;
		uint32_t ofsHeight;
		uint32_t ofsNormal;
		uint32_t ofsLayer;
		uint32_t ofsRefs;
		uint32_t ofsAlpha;
		uint32_t sizeAlpha;
		uint32_t ofsShadow;
		uint32_t sizeShadow;
		uint32_t areaid;
		uint32_t nMapObjRefs;
		uint32_t holes;
		uint8_t lowQualityTextureMap[16];
		uint32_t predTex;
		uint32_t nEffectDoodad;
		uint32_t ofsSndEmitters;
		uint32_t nSndEmitters;
		uint32_t ofsLiquid;
		uint32_t sizeLiquid;
		C3Vector position;
		uint32_t ofsMCCV;
		uint32_t unused1;
		uint32_t unused2;
	};

	struct SMLayer
	{
		uint32_t textureId;
		uint32_t flags;
		uint32_t offsetInMCAL;
		uint32_t effectId;
	};

	// MDDF entry. nameId indexes MMID, which holds byte offsets into the MMDX string blob.
	struct SMDoodadDef
	{
		uint32_t nameId;
		uint32_t uniqueId;
		C3Vector position;
		C3Vector rotation;
		uint16_t scale;
		uint16_t flags;
	};

	// MODF entry. nameId indexes MWID into MWMO, and uniqueId must be unique per instance across
	// the whole map or CMapChunk::CreateRefs silently drops it.
	struct SMMapObjDef
	{
		uint32_t nameId;
		uint32_t uniqueId;
		C3Vector position;
		C3Vector rotation;
		CAaBox extents;
		uint16_t flags;
		uint16_t doodadSet;
		uint16_t nameSet;
		uint16_t scale;
	};

	// MH2O per-chunk header, 256 of them at the very start of MH2O, indexed chunkY * 16 + chunkX.
	// Every offset in the chunk is measured from the start of MH2O's data, so from just after its
	// IFF header.
	struct SMLiquidChunk
	{
		uint32_t offsetInstances;
		uint32_t layerCount;
		uint32_t offsetAttributes; // two uint64 masks, fishable then deep. 0 means all ones.
	};

	// One liquid layer over a sub-rectangle of the chunk's 8x8 cells. Liquid__GetInstanceDesc
	// (0x008A3090) indexes these at base + offsetInstances + i * 24.
	struct SMLiquidInstance
	{
		uint16_t liquidType;   // LiquidType.dbc id
		uint16_t vertexFormat; // 0 height+depth, 1 height+uv, 2 depth only
		float minHeight;
		float maxHeight;
		uint8_t xOffset; // 0..7
		uint8_t yOffset; // 0..7
		uint8_t width;   // 1..8
		uint8_t height;  // 1..8
		uint32_t offsetExistsBitmap; // width * height bits, x fast. 0 means every cell exists.
		uint32_t offsetVertexData;
	};

	// One entry of CMapArea::textureData, the tile's MTEX list once loaded. SMLayer::textureId
	// indexes it, and CMap::LoadTerrainTexture fills in `texture` on first use.
	struct SMTerrainTexture
	{
		char* name;
		void* texture;
	};

	static_assert(offsetof(SMChunk, sizeAlpha) == 0x28, "SMChunk::sizeAlpha moved");
	static_assert(offsetof(SMChunk, position) == 0x68, "SMChunk::position moved");
	static_assert(sizeof(SMChunk) == 128, "SMChunk must be 128 bytes");
	static_assert(sizeof(SMChunkInfo) == 16, "SMChunkInfo must be 16 bytes");
	static_assert(sizeof(SMMapHeader) == 64, "SMMapHeader must be 64 bytes");
	static_assert(sizeof(SMDoodadDef) == 36, "SMDoodadDef must be 36 bytes");
	static_assert(sizeof(SMMapObjDef) == 64, "SMMapObjDef must be 64 bytes");
	static_assert(sizeof(SMLayer) == 16, "SMLayer must be 16 bytes");
	static_assert(sizeof(SMLiquidChunk) == 12, "SMLiquidChunk must be 12 bytes");
	static_assert(sizeof(SMLiquidInstance) == 24, "SMLiquidInstance must be 24 bytes");
}
