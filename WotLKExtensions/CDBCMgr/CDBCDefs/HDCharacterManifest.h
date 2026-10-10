#pragma once

#include <Defines.h>
#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

enum HDCharacterManifestKind : uint32
{
	HD_MANIFEST_MODEL = 1,
	HD_MANIFEST_TEXTURE = 2,
	HD_MANIFEST_SLOT = 3,
	HD_MANIFEST_LAYER = 4,
	HD_MANIFEST_HIDE = 5,
	HD_MANIFEST_SHOW = 6,
	HD_MANIFEST_HELM = 7,
};

enum HDCharacterManifestRect : uint32
{
	HD_MANIFEST_RECT_WHOLE = 0,
	HD_MANIFEST_RECT_FACE = 1,
	HD_MANIFEST_RECT_BODY = 2,
	HD_MANIFEST_RECT_SECTION = 10,
};

struct HDCharacterManifestCond
{
	uint32 option;
	uint32 value;
};

struct HDCharacterManifestRow
{
	uint32 id;
	uint32 race;
	uint32 sex;
	uint32 kind;
	uint32 target;
	uint32 value;
	uint32 layerOrder;
	uint32 rect;
	uint32 blend;
	uint32 path;
	HDCharacterManifestCond conds[3];
};

namespace HDCharacters::Manifest
{
	void Build(const HDCharacterManifestRow* rows, uint32 count, const char* strings, uint32 stringsSize);
}

class HDCharacterManifest : public CDBC
{
public:
	const char* fileName = "HDCharacterManifest";

	HDCharacterManifest() : CDBC()
	{
		this->numColumns = sizeof(HDCharacterManifestRow) / 4;
		this->rowSize = sizeof(HDCharacterManifestRow);
	}

	HDCharacterManifest* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		HDCharacters::Manifest::Build(static_cast<const HDCharacterManifestRow*>(this->rows), this->numRows,
		    static_cast<const char*>(this->stringTable), this->stringTableSize);
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}
};
#pragma optimize("", on)
