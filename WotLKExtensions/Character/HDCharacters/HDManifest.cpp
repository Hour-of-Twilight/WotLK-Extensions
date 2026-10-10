#include <Defines.h>
#include <Character/HDCharacters/HDManifest.h>
#include <Character/HDCharacters/HDCharacters.h>

#include <CDBCDefs/HDCharacterManifest.h>
#include <Logger.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace HDCharacters::Manifest
{
	namespace
	{
		std::map<int, std::unique_ptr<Data>> g_manifests;

		int RectOf(uint32 rect)
		{
			if (rect == HD_MANIFEST_RECT_FACE)
				return kRectFace;
			if (rect == HD_MANIFEST_RECT_BODY)
				return kRectBody;
			if (rect >= HD_MANIFEST_RECT_SECTION)
				return (int)(rect - HD_MANIFEST_RECT_SECTION);
			return kRectWhole;
		}

		void CondsOf(const HDCharacterManifestRow& row, Entry& entry)
		{
			for (const HDCharacterManifestCond& cond : row.conds)
			{
				if (cond.option > 0 && cond.option < kOptionCount)
					entry.conds.push_back({ (int)cond.option, (int)cond.value });
			}
		}
	}

	void Build(const HDCharacterManifestRow* rows, uint32 count, const char* strings, uint32 stringsSize)
	{
		g_manifests.clear();
		if (!rows || !count)
		{
			LOG_INFO << "hd characters: DBFilesClient\\HDCharacterManifest.dbc has no rows, hd models cannot be dressed";
			return;
		}
		auto path = [strings, stringsSize](uint32 offset)
		{
			if (!strings || !offset || offset >= stringsSize)
				return std::string();
			return std::string(strings + offset);
		};
		for (uint32 i = 0; i < count; ++i)
		{
			const HDCharacterManifestRow& row = rows[i];
			std::unique_ptr<Data>& slot = g_manifests[(int)row.race * 2 + (int)row.sex];
			if (!slot)
				slot = std::make_unique<Data>();
			Data& data = *slot;
			switch (row.kind)
			{
			case HD_MANIFEST_MODEL:
				data.model = path(row.path);
				break;
			case HD_MANIFEST_TEXTURE:
			{
				TexLine entry;
				entry.target = (int)row.target;
				entry.path = path(row.path);
				CondsOf(row, entry);
				data.textures.push_back(std::move(entry));
				break;
			}
			case HD_MANIFEST_SLOT:
			{
				SlotLine entry;
				entry.type = (int)row.target;
				entry.path = path(row.path);
				CondsOf(row, entry);
				data.slots.push_back(std::move(entry));
				break;
			}
			case HD_MANIFEST_LAYER:
			{
				LayerLine entry;
				entry.order = (int)row.layerOrder;
				entry.target = (int)row.target;
				entry.rect = RectOf(row.rect);
				entry.blend = (int)row.blend;
				entry.path = path(row.path);
				CondsOf(row, entry);
				data.layers.push_back(std::move(entry));
				break;
			}
			case HD_MANIFEST_HIDE:
				data.hides.push_back({ (int)row.target, (int)row.value });
				break;
			case HD_MANIFEST_SHOW:
			{
				ShowLine entry;
				entry.id = (int)row.target;
				CondsOf(row, entry);
				data.shows.push_back(std::move(entry));
				break;
			}
			case HD_MANIFEST_HELM:
				data.helms[(int)row.target] = (int)row.value;
				break;
			default:
				break;
			}
		}
		for (const auto& entry : g_manifests)
		{
			const Data& data = *entry.second;
			LOG_DEBUG << "hd characters: manifest race " << (entry.first / 2) << " sex " << (entry.first % 2)
			          << " tex=" << data.textures.size() << " slots=" << data.slots.size()
			          << " layers=" << data.layers.size() << " hides=" << data.hides.size()
			          << " shows=" << data.shows.size() << " helms=" << data.helms.size();
		}
		LOG_INFO << "hd characters: DBFilesClient\\HDCharacterManifest.dbc loaded, " << count
		         << " lines dress " << g_manifests.size() << " race and sex pairs";
	}

	namespace
	{
	}

	bool Entry::Matches(const Look& look) const
	{
		for (const Cond& cond : conds)
		{
			if (look.values[cond.option] != cond.value)
				return false;
		}
		return true;
	}

	const Data* Get(int race, int sex)
	{
		auto it = g_manifests.find(race * 2 + sex);
		return it == g_manifests.end() ? nullptr : it->second.get();
	}

	Look LookOf(const void* component)
	{
		const uint8* bytes = static_cast<const uint8*>(component);
		auto field = [bytes](uint32 offset)
		{
			return *reinterpret_cast<const int*>(bytes + offset);
		};
		Look look;
		look.values[kOptionSkinColor] = field(Component::kOffSkinColor);
		look.values[kOptionFace] = field(Component::kOffFace);
		look.values[kOptionHairStyle] = field(Component::kOffHairStyle);
		look.values[kOptionHairColor] = field(Component::kOffHairColor);
		look.values[kOptionFacial] = field(Component::kOffFacialStyle);
		look.values[kOptionDeathKnight] = field(Component::kOffClass) == ClientData::CLASS_DEATH_KNIGHT ? 1 : 0;
		return look;
	}

	const char* Texture(const Data& data, int target, const Look& look)
	{
		for (const TexLine& line : data.textures)
		{
			if (line.target == target && line.Matches(look))
				return line.path.c_str();
		}
		return nullptr;
	}

	const char* Slot(const Data& data, int type, const Look& look)
	{
		for (const SlotLine& line : data.slots)
		{
			if (line.type == type && line.Matches(look))
				return line.path.c_str();
		}
		return nullptr;
	}

	void ResolveLayers(const Data& data, const Look& look, std::vector<const LayerLine*>& out)
	{
		out.clear();
		for (const LayerLine& line : data.layers)
		{
			if (!line.Matches(look))
				continue;
			bool taken = false;
			for (const LayerLine* chosen : out)
			{
				if (chosen->target == line.target)
				{
					taken = true;
					break;
				}
			}
			if (!taken)
				out.push_back(&line);
		}
		std::stable_sort(out.begin(), out.end(), [](const LayerLine* a, const LayerLine* b)
		{
			return a->order < b->order;
		});
	}

	void ResolveShows(const Data& data, const Look& look, std::vector<int>& out, bool keepMarkers)
	{
		out.clear();
		std::vector<int> matchedGroups;
		for (const ShowLine& line : data.shows)
		{
			if (line.conds.empty() || !line.Matches(look))
				continue;
			matchedGroups.push_back(line.id / 100);
			if (line.id % 100 != 0 || keepMarkers)
				out.push_back(line.id);
		}
		for (const ShowLine& line : data.shows)
		{
			if (!line.conds.empty() || (line.id % 100 == 0 && !keepMarkers))
				continue;
			if (std::find(matchedGroups.begin(), matchedGroups.end(), line.id / 100) == matchedGroups.end())
				out.push_back(line.id);
		}
	}
}
