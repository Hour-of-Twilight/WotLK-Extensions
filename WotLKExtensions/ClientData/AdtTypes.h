#pragma once

#include <Defines.h>

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
		uint32 flags;
		uint32 mcin;
		uint32 mtex;
		uint32 mmdx;
		uint32 mmid;
		uint32 mwmo;
		uint32 mwid;
		uint32 mddf;
		uint32 modf;
		uint32 mfbo;
		uint32 mh2o;
		uint32 mtxf;
		uint8 mampValue;
		uint8 padding[3];
		uint32 unused[3];
	};

	// MCIN entry. offset is absolute from the start of the file and points at the MCNK header.
	struct SMChunkInfo
	{
		uint32 offset;
		uint32 size;
		uint32 flags; // bit 0 is set by the client once the chunk has been prepared
		uint32 asyncId;
	};

	// MCNK header. sizeAlpha and sizeLiquid include the 8-byte IFF header of their sub-chunk.
	struct SMChunk
	{
		uint32 flags;
		C2iVector index;
		uint32 nLayers;
		uint32 nDoodadRefs;
		uint32 ofsHeight;
		uint32 ofsNormal;
		uint32 ofsLayer;
		uint32 ofsRefs;
		uint32 ofsAlpha;
		uint32 sizeAlpha;
		uint32 ofsShadow;
		uint32 sizeShadow;
		uint32 areaid;
		uint32 nMapObjRefs;
		uint32 holes;
		uint8 lowQualityTextureMap[16];
		uint32 predTex;
		uint32 nEffectDoodad;
		uint32 ofsSndEmitters;
		uint32 nSndEmitters;
		uint32 ofsLiquid;
		uint32 sizeLiquid;
		C3Vector position;
		uint32 ofsMCCV;
		uint32 unused1;
		uint32 unused2;
	};

	struct SMLayer
	{
		uint32 textureId;
		uint32 flags;
		uint32 offsetInMCAL;
		uint32 effectId;
	};

	// MDDF entry. nameId indexes MMID, which holds byte offsets into the MMDX string blob.
	struct SMDoodadDef
	{
		uint32 nameId;
		uint32 uniqueId;
		C3Vector position;
		C3Vector rotation;
		uint16 scale;
		uint16 flags;
	};

	// MODF entry. nameId indexes MWID into MWMO, and uniqueId must be unique per instance across
	// the whole map or CMapChunk::CreateRefs silently drops it.
	struct SMMapObjDef
	{
		uint32 nameId;
		uint32 uniqueId;
		C3Vector position;
		C3Vector rotation;
		CAaBox extents;
		uint16 flags;
		uint16 doodadSet;
		uint16 nameSet;
		uint16 scale;
	};

	// MH2O per-chunk header, 256 of them at the very start of MH2O, indexed chunkY * 16 + chunkX.
	// Every offset in the chunk is measured from the start of MH2O's data, so from just after its
	// IFF header.
	struct SMLiquidChunk
	{
		uint32 offsetInstances;
		uint32 layerCount;
		uint32 offsetAttributes; // two uint64 masks, fishable then deep. 0 means all ones.
	};

	// One liquid layer over a sub-rectangle of the chunk's 8x8 cells. Liquid__GetInstanceDesc
	// (0x008A3090) indexes these at base + offsetInstances + i * 24.
	struct SMLiquidInstance
	{
		uint16 liquidType;   // LiquidType.dbc id
		uint16 vertexFormat; // 0 height+depth, 1 height+uv, 2 depth only
		float minHeight;
		float maxHeight;
		uint8 xOffset;             // 0..7
		uint8 yOffset;             // 0..7
		uint8 width;               // 1..8
		uint8 height;              // 1..8
		uint32 offsetExistsBitmap; // width * height bits, x fast. 0 means every cell exists.
		uint32 offsetVertexData;
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
