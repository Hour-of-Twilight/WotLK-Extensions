#pragma once

#include <map>
#include <string>
#include <vector>

namespace HDCharacters::Manifest
{
	constexpr int kOptionSkinColor = 1;
	constexpr int kOptionFace = 2;
	constexpr int kOptionHairStyle = 3;
	constexpr int kOptionHairColor = 4;
	constexpr int kOptionFacial = 5;
	constexpr int kOptionDeathKnight = 6;
	constexpr int kOptionCount = 7;

	constexpr int kTargetSkin = 1;
	constexpr int kTargetSkinExtra = 2;
	constexpr int kTargetHair = 10;
	constexpr int kTargetPelvis = 13;
	constexpr int kTargetTorso = 14;

	constexpr int kRectWhole = -1;
	constexpr int kRectFace = -2;
	constexpr int kRectBody = -3;

	struct Look
	{
		int values[kOptionCount] = {};
	};

	struct Cond
	{
		int option;
		int value;
	};

	struct Entry
	{
		std::vector<Cond> conds;
		bool Matches(const Look& look) const;
	};

	struct TexLine : Entry
	{
		int target = 0;
		std::string path;
	};

	struct SlotLine : Entry
	{
		int type = 0;
		std::string path;
	};

	struct LayerLine : Entry
	{
		int order = 0;
		int target = 0;
		int rect = kRectWhole;
		int blend = 0;
		std::string path;
	};

	struct ShowLine : Entry
	{
		int id = 0;
	};

	struct Range
	{
		int from;
		int to;
	};

	struct Data
	{
		std::string model;
		std::vector<TexLine> textures;
		std::vector<SlotLine> slots;
		std::vector<LayerLine> layers;
		std::vector<Range> hides;
		std::vector<ShowLine> shows;
		std::map<int, int> helms;
	};

	const Data* Get(int race, int sex);
	Look LookOf(const void* component);
	const char* Texture(const Data& data, int target, const Look& look);
	const char* Slot(const Data& data, int type, const Look& look);
	void ResolveLayers(const Data& data, const Look& look, std::vector<const LayerLine*>& out);
	void ResolveShows(const Data& data, const Look& look, std::vector<int>& out, bool keepMarkers = false);
}
