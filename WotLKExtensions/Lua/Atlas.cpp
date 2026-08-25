#include "Atlas.h"

#include <ClientData/ClientFunctions.h>

#include <cctype>
#include <string>
#include <unordered_map>

static const int kLuaGlobalsIndex = -10002;
static const int kLuaTypeNumber = 3;
static const int kLuaTypeString = 4;
static const int kLuaTypeTable = 5;
static const int kLuaTypeFunction = 6;

struct AtlasEntry
{
	int width;
	int height;
	double left;
	double right;
	double top;
	double bottom;
	bool tileHorizontally;
	bool tileVertically;
	std::string file;
};

static std::unordered_map<std::string, AtlasEntry> sAtlases;

static std::string NormalizeName(const char* name)
{
	std::string out(name ? name : "");
	for (char& c : out)
		c = (char)tolower((unsigned char)c);

	return out;
}

// Texture paths get typed by hand here, so accept forward slashes and trailing whitespace.
static std::string NormalizePath(const char* path)
{
	std::string out(path ? path : "");
	for (char& c : out)
	{
		if (c == '/')
			c = '\\';
	}

	while (!out.empty() && (out.back() == ' ' || out.back() == '\t'))
		out.pop_back();

	return out;
}

static void Pop(lua_State* L, int n)
{
	FrameScript::SetTop(L, FrameScript::GetTop(L) - n);
}

static double FieldNumber(lua_State* L, int tableIdx, int n)
{
	FrameScript::RawGetI(L, tableIdx, n);
	double value = FrameScript::Type(L, -1) == kLuaTypeNumber ? FrameScript::GetNumber(L, -1) : 0.0;
	Pop(L, 1);

	return value;
}

static bool FieldBool(lua_State* L, int tableIdx, int n)
{
	FrameScript::RawGetI(L, tableIdx, n);
	bool value = FrameScript::ToBoolean(L, -1);
	Pop(L, 1);

	return value;
}

// Leaves the method and its self argument on the stack, ready for the caller to push arguments.
static bool BeginMethod(lua_State* L, int objIdx, const char* method)
{
	FrameScript::GetField(L, objIdx, method);
	if (FrameScript::Type(L, -1) != kLuaTypeFunction)
	{
		Pop(L, 1);
		return false;
	}

	FrameScript::PushValue(L, objIdx);
	return true;
}

static void EndMethod(lua_State* L, int argCount)
{
	if (FrameScript::PCall(L, argCount + 1, 0, 0) != 0)
		Pop(L, 1);
}

void LuaAtlas::Install(lua_State* L)
{
	if (!L)
		return;

	FrameScript::GetField(L, kLuaGlobalsIndex, "C_Texture");
	if (FrameScript::Type(L, -1) != kLuaTypeTable)
	{
		Pop(L, 1);
		FrameScript::CreateTable(L, 0, 4);
		FrameScript::PushValue(L, -1);
		FrameScript::SetField(L, kLuaGlobalsIndex, "C_Texture");
	}

	FrameScript::PushCClosure(L, &LuaAtlas::LoadAtlasData, 0);
	FrameScript::SetField(L, -2, "LoadAtlasData");

	FrameScript::PushCClosure(L, &LuaAtlas::GetAtlasInfo, 0);
	FrameScript::SetField(L, -2, "GetAtlasInfo");

	FrameScript::PushCClosure(L, &LuaAtlas::SetAtlas, 0);
	FrameScript::SetField(L, -2, "SetAtlas");

	Pop(L, 1);

	FrameScript::PushCClosure(L, &LuaAtlas::SetAtlas, 0);
	FrameScript::SetField(L, kLuaGlobalsIndex, "SetAtlas");
}

// C_Texture.LoadAtlasData({ ["path\\to\\sheet.blp"] = { ["atlas-name"] = { w, h, l, r, t, b,
// tileH, tileV, scale } } }) -> number of atlases added.
int LuaAtlas::LoadAtlasData(lua_State* L)
{
	if (FrameScript::Type(L, 1) != kLuaTypeTable)
	{
		FrameScript::PushNumber(L, 0);
		return 1;
	}

	int count = 0;

	FrameScript::PushNil(L);
	while (FrameScript::Next(L, 1))
	{
		int groupIdx = FrameScript::GetTop(L);
		if (FrameScript::Type(L, groupIdx - 1) == kLuaTypeString && FrameScript::Type(L, groupIdx) == kLuaTypeTable)
		{
			std::string file = NormalizePath(FrameScript::ToLString(L, groupIdx - 1, false));

			FrameScript::PushNil(L);
			while (FrameScript::Next(L, groupIdx))
			{
				int dataIdx = FrameScript::GetTop(L);
				if (FrameScript::Type(L, dataIdx - 1) == kLuaTypeString && FrameScript::Type(L, dataIdx) == kLuaTypeTable)
				{
					AtlasEntry entry;
					entry.width = (int)FieldNumber(L, dataIdx, 1);
					entry.height = (int)FieldNumber(L, dataIdx, 2);
					entry.left = FieldNumber(L, dataIdx, 3);
					entry.right = FieldNumber(L, dataIdx, 4);
					entry.top = FieldNumber(L, dataIdx, 5);
					entry.bottom = FieldNumber(L, dataIdx, 6);
					entry.tileHorizontally = FieldBool(L, dataIdx, 7);
					entry.tileVertically = FieldBool(L, dataIdx, 8);
					entry.file = file;

					sAtlases[NormalizeName(FrameScript::ToLString(L, dataIdx - 1, false))] = entry;
					++count;
				}

				Pop(L, 1);
			}
		}

		Pop(L, 1);
	}

	FrameScript::PushNumber(L, count);
	return 1;
}

int LuaAtlas::GetAtlasInfo(lua_State* L)
{
	if (FrameScript::Type(L, 1) != kLuaTypeString)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	auto it = sAtlases.find(NormalizeName(FrameScript::ToLString(L, 1, false)));
	if (it == sAtlases.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const AtlasEntry& entry = it->second;

	FrameScript::CreateTable(L, 0, 13);

	FrameScript::PushNumber(L, entry.width);
	FrameScript::SetField(L, -2, "width");
	FrameScript::PushNumber(L, entry.height);
	FrameScript::SetField(L, -2, "height");

	FrameScript::PushNumber(L, entry.left);
	FrameScript::SetField(L, -2, "left");
	FrameScript::PushNumber(L, entry.right);
	FrameScript::SetField(L, -2, "right");
	FrameScript::PushNumber(L, entry.top);
	FrameScript::SetField(L, -2, "top");
	FrameScript::PushNumber(L, entry.bottom);
	FrameScript::SetField(L, -2, "bottom");

	// Retail spellings, so ported UI code keeps working unedited.
	FrameScript::PushNumber(L, entry.left);
	FrameScript::SetField(L, -2, "leftTexCoord");
	FrameScript::PushNumber(L, entry.right);
	FrameScript::SetField(L, -2, "rightTexCoord");
	FrameScript::PushNumber(L, entry.top);
	FrameScript::SetField(L, -2, "topTexCoord");
	FrameScript::PushNumber(L, entry.bottom);
	FrameScript::SetField(L, -2, "bottomTexCoord");

	FrameScript::PushBoolean(L, entry.tileHorizontally ? 1 : 0);
	FrameScript::SetField(L, -2, "tilesHorizontally");
	FrameScript::PushBoolean(L, entry.tileVertically ? 1 : 0);
	FrameScript::SetField(L, -2, "tilesVertically");

	FrameScript::PushString(L, entry.file.c_str());
	FrameScript::SetField(L, -2, "file");

	return 1;
}

// SetAtlas(texture, atlasName, useAtlasSize), also reachable as C_Texture.SetAtlas.
int LuaAtlas::SetAtlas(lua_State* L)
{
	if (FrameScript::Type(L, 1) <= 0 || FrameScript::Type(L, 2) != kLuaTypeString)
	{
		FrameScript::PushBoolean(L, 0);
		return 1;
	}

	auto it = sAtlases.find(NormalizeName(FrameScript::ToLString(L, 2, false)));
	if (it == sAtlases.end())
	{
		FrameScript::PushBoolean(L, 0);
		return 1;
	}

	const AtlasEntry& entry = it->second;
	bool useAtlasSize = FrameScript::ToBoolean(L, 3);

	if (BeginMethod(L, 1, "SetTexture"))
	{
		FrameScript::PushString(L, entry.file.c_str());
		EndMethod(L, 1);
	}

	if (BeginMethod(L, 1, "SetTexCoord"))
	{
		FrameScript::PushNumber(L, entry.left);
		FrameScript::PushNumber(L, entry.right);
		FrameScript::PushNumber(L, entry.top);
		FrameScript::PushNumber(L, entry.bottom);
		EndMethod(L, 4);
	}

	if (BeginMethod(L, 1, "SetHorizTile"))
	{
		FrameScript::PushBoolean(L, entry.tileHorizontally ? 1 : 0);
		EndMethod(L, 1);
	}

	if (BeginMethod(L, 1, "SetVertTile"))
	{
		FrameScript::PushBoolean(L, entry.tileVertically ? 1 : 0);
		EndMethod(L, 1);
	}

	if (useAtlasSize)
	{
		if (BeginMethod(L, 1, "SetWidth"))
		{
			FrameScript::PushNumber(L, entry.width);
			EndMethod(L, 1);
		}

		if (BeginMethod(L, 1, "SetHeight"))
		{
			FrameScript::PushNumber(L, entry.height);
			EndMethod(L, 1);
		}
	}

	FrameScript::PushBoolean(L, 1);
	return 1;
}
