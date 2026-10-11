#include <Defines.h>
#include <Character/HDCharacters/HDCharacters.h>
#include <Character/HDCharacters/BlpImage.h>
#include <Character/HDCharacters/HDCharacterSheet.h>
#include <Character/HDCharacters/HDManifest.h>
#include <Character/HDCharacters/HDNpcDisplay.h>
#include <Character/PlayerRenderLimit.h>

#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/GameClient.h>
#include <ClientData/M2Model.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/Streaming.h>
#include <ClientData/System.h>
#include <ClientData/WowObjects.h>
#include <Lua/XMLExtensions.h>
#include <Streaming/LooseFiles.h>
#include <Logger.h>
#include <Macros.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HDCharacters
{
	namespace
	{
		constexpr const char* kHdRoot = "CharacterHD\\";
		constexpr const char* kStockRoot = "Character\\";
		constexpr const char* kBlankTexture = "CharacterHD\\Blank.blp";
		constexpr const char* kModelSuffix = "_HD.m2";
		constexpr const char* kProbeModel = "CharacterHD\\Human\\Male\\HumanMale_HD.m2";
		constexpr int kTargetBlank = 0;
		constexpr int kTargetByName = -1;
		constexpr int kSlotTypes[] = { 11, 12, 13 };
		constexpr int kTextureTypeSkinExtra = 8;
		constexpr size_t kModelDataRecordSize = 0x74;
		constexpr int kRaceCount = 12;
		constexpr size_t kGluePathSize = 260;

		const char* const kRaceFolders[] = { nullptr, "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll", nullptr, "BloodElf", "Draenei" };
		const char* const kSexFolders[] = { "Male", "Female" };

		CLIENT_FUNCTION(CharacterCreateTexture, 0x004E8D30, __cdecl, void*, (const char* path, void* status))
		CLIENT_FUNCTION(TextureRelease, 0x0047BF30, __cdecl, int, (void* texture))
		CLIENT_FUNCTION(CM2Model__ReplaceTexture, 0x00825260, __thiscall, void, (void* model, int type, void* texture))
		CLIENT_FUNCTION(CM2Model__SetGeometryVisible, 0x0082C7C0, __thiscall, void*, (void* model, unsigned int start, unsigned int end, int visible))
		CLIENT_ADDRESS(int, s_textureStatus, 0x00AC46D0)
		CLIENT_FUNCTION(Player_C_GetDisplayId, 0x006D5C30, __cdecl, int, (int race, unsigned int sex))
		CLIENT_ADDRESS(ClientData::WoWClientDB, s_creatureDisplayInfoDB, 0x00AD34B8)
		CLIENT_ADDRESS(ClientData::WoWClientDB, s_creatureModelDataDB, 0x00AD3500)

		bool g_enabled = false;
		int g_available = -1;
		void* g_componentUnit = nullptr;
		std::unordered_set<const void*> g_hdComponents;
		std::unordered_map<std::string, std::string> g_modelPaths;
		std::unordered_map<std::string, bool> g_fileExists;

		struct TextureContext
		{
			bool active = false;
			int target = kTargetByName;
			Manifest::Look look;
			const Manifest::Data* data = nullptr;
		};

		TextureContext g_context;
		uint8 g_glueRecords[kRaceCount][2][kModelDataRecordSize];
		char g_gluePaths[kRaceCount][2][kGluePathSize];
		int g_glueTransformDepth = 0;
		std::unordered_map<int, float> g_bakedScales;

		int Field(const void* component, uint32 offset)
		{
			return *reinterpret_cast<const int*>(static_cast<const uint8*>(component) + offset);
		}

		const Manifest::Data* ManifestOf(const void* component)
		{
			return Manifest::Get(Field(component, Component::kOffRace), Field(component, Component::kOffSex));
		}

		int TargetOf(int variation, int textureIndex)
		{
			switch (variation)
			{
			case 0:
				return textureIndex == 0 ? Manifest::kTargetSkin : Manifest::kTargetSkinExtra;
			case 3:
				return textureIndex == 0 ? Manifest::kTargetHair : kTargetBlank;
			case 4:
				return textureIndex == 0 ? Manifest::kTargetPelvis : Manifest::kTargetTorso;
			default:
				return kTargetBlank;
			}
		}

		struct ContextScope
		{
			TextureContext previous;

			ContextScope(const void* component, int target)
			    : previous(g_context)
			{
				g_context.active = true;
				g_context.target = target;
				g_context.look = Manifest::LookOf(component);
				g_context.data = ManifestOf(component);
			}

			void Override(int option, int value)
			{
				g_context.look.values[option] = value;
			}

			~ContextScope()
			{
				g_context = previous;
			}
		};

		bool KindFromName(const std::string& lowerName, int& variation, int& textureIndex)
		{
			struct Kind
			{
				const char* key;
				int variation;
				int textureIndex;
			};
			static const Kind kinds[] = {
				{ "nakedpelvis", 4, 0 },
				{ "pelvis", 4, 0 },
				{ "nakedtorso", 4, 1 },
				{ "torso", 4, 1 },
				{ "faciallowerhair", 2, 0 },
				{ "facialupperhair", 2, 1 },
				{ "facelower", 1, 0 },
				{ "faceupper", 1, 1 },
				{ "scalplowerhair", 3, 1 },
				{ "scalpupperhair", 3, 2 },
				{ "hair", 3, 0 },
				{ "extra", 0, 1 },
				{ "skin", 0, 0 },
			};
			for (const Kind& kind : kinds)
			{
				if (lowerName.find(kind.key) != std::string::npos)
				{
					variation = kind.variation;
					textureIndex = kind.textureIndex;
					return true;
				}
			}
			return false;
		}

		std::string LowerKey(const char* path)
		{
			std::string key(path);
			for (char& c : key)
				c = c == '/' ? '\\' : (char)std::tolower((unsigned char)c);
			return key;
		}

		bool StartsWithNoCase(const char* text, const char* prefix)
		{
			return _strnicmp(text, prefix, std::strlen(prefix)) == 0;
		}

		bool IsPlayerUnit(void* unit)
		{
			return unit && static_cast<ClientData::CGObject_C*>(unit)->GetTypeID() == ClientData::TYPEID_PLAYER;
		}

		bool IsMirrorImageUnit(void* unit)
		{
			return ClientData::WowUnit(unit).HasUnitFlag2(ClientData::UNIT_FLAG2_MIRROR_IMAGE);
		}

		uint32 DisplayExtraIdOf(void* unit)
		{
			return ClientData::WowUnit(unit).DisplayInfoExtraId();
		}

		bool IsHdUnit(void* unit)
		{
			if (!unit || !g_enabled)
				return false;
			if (IsPlayerUnit(unit) || IsMirrorImageUnit(unit))
				return true;
			const HDNpcDisplayRow* row = NpcDisplay::Find(DisplayExtraIdOf(unit));
			if (!row || !(row->flags & HD_NPC_DISPLAY_HD_RACE))
				return false;
			return (row->flags & (HD_NPC_DISPLAY_NPC_LOOK | HD_NPC_DISPLAY_BAKE)) != 0;
		}

		int FolderIndex(const char* const* folders, int count, const std::string& name)
		{
			for (int i = 0; i < count; ++i)
			{
				if (folders[i] && _stricmp(folders[i], name.c_str()) == 0)
					return i;
			}
			return -1;
		}

		bool RaceSexOfCharacterPath(const char* path, int& race, int& sex)
		{
			if (!path)
				return false;
			const char* rest = nullptr;
			if (StartsWithNoCase(path, kHdRoot))
				rest = path + std::strlen(kHdRoot);
			else if (StartsWithNoCase(path, kStockRoot))
				rest = path + std::strlen(kStockRoot);
			else
				return false;
			std::string text(rest);
			size_t first = text.find_first_of("\\/");
			if (first == std::string::npos)
				return false;
			size_t second = text.find_first_of("\\/", first + 1);
			if (second == std::string::npos)
				return false;
			race = FolderIndex(kRaceFolders, kRaceCount, text.substr(0, first));
			sex = FolderIndex(kSexFolders, 2, text.substr(first + 1, second - first - 1));
			return race >= 0 && sex >= 0;
		}

		bool RaceSexOfHdModel(const void* model, int& race, int& sex)
		{
			const char* stem = ModelStem(model);
			return stem && StartsWithNoCase(stem, kHdRoot) && RaceSexOfCharacterPath(stem, race, sex);
		}

		void* GlueModelRecord(int race, unsigned int sex, void* record, const char* hdPath)
		{
			if (race < 0 || race >= kRaceCount || sex > 1 || std::strlen(hdPath) >= kGluePathSize)
				return record;
			uint8* copy = g_glueRecords[race][sex];
			char* path = g_gluePaths[race][sex];
			std::memcpy(copy, record, kModelDataRecordSize);
			std::strcpy(path, hdPath);
			reinterpret_cast<ClientData::CreatureModelDataRow*>(copy)->m_modelName = path;
			return copy;
		}

		const char* ResolveModelPath(const char* stockPath)
		{
			if (!stockPath || !StartsWithNoCase(stockPath, kStockRoot))
				return nullptr;
			std::string key = LowerKey(stockPath);
			auto it = g_modelPaths.find(key);
			if (it == g_modelPaths.end())
			{
				std::string hd = std::string(kHdRoot) + (stockPath + std::strlen(kStockRoot));
				size_t dot = hd.find_last_of('.');
				size_t slash = hd.find_last_of('\\');
				if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
					hd.erase(dot);
				hd += kModelSuffix;
				if (!FileExists(hd.c_str()))
					hd.clear();
				it = g_modelPaths.emplace(key, hd).first;
			}
			return it->second.empty() ? nullptr : it->second.c_str();
		}

		const char* RedirectTexture(const char* stockPath)
		{
			if (!g_context.active || !stockPath || !StartsWithNoCase(stockPath, kStockRoot))
				return nullptr;
			int target = g_context.target;
			if (target == kTargetByName)
			{
				const char* fileName = std::strrchr(stockPath, '\\');
				fileName = fileName ? fileName + 1 : stockPath;
				int variation = 0;
				int textureIndex = 0;
				if (!KindFromName(LowerKey(fileName), variation, textureIndex))
					return nullptr;
				target = TargetOf(variation, textureIndex);
			}
			if (target == kTargetBlank)
				return kBlankTexture;
			if (!g_context.data)
				return nullptr;
			const char* path = Manifest::Texture(*g_context.data, target, g_context.look);
			if (!path)
				LOG_DEBUG << "hd characters: no hd texture for target " << target << ", keeping " << stockPath;
			return path;
		}

		int g_refreshed = 0;

		int __cdecl RefreshUnitCallback(uint32 guidLow, uint32 guidHigh, void*)
		{
			uint64 guid = ((uint64)guidHigh << 32) | guidLow;
			ClientData::CGObject_C* unit = ClientData::ObjectManager::GetObject(
			    guid, static_cast<ClientData::ObjectTypeMask>(ClientData::TYPEMASK_UNIT | ClientData::TYPEMASK_PLAYER));
			if (unit)
			{
				unit->UpdateDisplayInfo(1);
				++g_refreshed;
			}
			return 1;
		}

		void RefreshUnits()
		{
			if (!ClientData::GameClient::IsInitialized() || !ClientData::ObjectManager::GetActivePlayerObject())
				return;
			g_refreshed = 0;
			ClntObjMgr::EnumVisibleObjects(RefreshUnitCallback, nullptr);
			FrameXMLExtensions::SignalEvent("UNIT_MODEL_CHANGED", "%s", "player");
			LOG_INFO << "hd characters: rebuilt " << g_refreshed << " unit models, hd=" << (g_enabled ? 1 : 0);
		}

		void InvalidateContent()
		{
			LooseFiles::Invalidate();
			g_fileExists.clear();
			g_modelPaths.clear();
			ClearBlpCache();
			Sheet::ClearCache();
		}

		bool ProbeAvailable()
		{
			bool blank = FileExists(kBlankTexture);
			bool model = FileExists(kProbeModel);
			if (blank && model)
				return true;
			LOG_INFO << "hd characters: the hd art is not readable, " << kBlankTexture << "="
			         << (blank ? 1 : 0) << " " << kProbeModel << "=" << (model ? 1 : 0);
			return false;
		}
	}

	void Apply()
	{
		LOG_INFO << "hd characters: module ready";
	}

	// ResolveModelPath caches its answer either way, including the empty string standing
	// for "no HD version of this model exists". Once art streams in, every one of those
	// answers taken before it arrived is wrong, and SetEnabled below only clears them when
	// the enabled state changes - so a model resolved while its HD file was still
	// downloading would stay on the standard art for the rest of the session.
	void InvalidateArt()
	{
		InvalidateContent();
	}

	void SetEnabled(bool enabled)
	{
		if (enabled && !IsAvailable())
		{
			LOG_INFO << "hd characters: the hd patch is not installed, staying on the standard models";
			enabled = false;
		}
		if (g_enabled == enabled)
			return;
		g_enabled = enabled;
		if (enabled)
		{
			InvalidateContent();
			NpcDisplay::Report();
		}
		RefreshUnits();
	}

	bool IsEnabled()
	{
		return g_enabled;
	}

	bool IsAvailable()
	{
		if (g_available < 0)
			g_available = ProbeAvailable() ? 1 : 0;
		return g_available == 1;
	}

	bool RefreshAvailability()
	{
		InvalidateContent();
		int now = ProbeAvailable() ? 1 : 0;
		if (now != g_available)
			LOG_INFO << "hd characters: the hd art is now " << (now ? "available" : "unavailable");
		g_available = now;
		return g_available == 1;
	}

	bool IsHdComponent(const void* component)
	{
		return component && g_hdComponents.count(component) != 0;
	}

	bool IsHdModel(const void* model)
	{
		const char* stem = ModelStem(model);
		return stem && StartsWithNoCase(stem, kHdRoot);
	}

	float GlueScale(const void* model)
	{
		int race = 0;
		int sex = 0;
		if (!RaceSexOfHdModel(model, race, sex))
			return 1.0f;
		return 1.0f / BakedScale(race, sex);
	}

	bool InGlueTransform()
	{
		return g_glueTransformDepth > 0;
	}

	GlueTransformScope::GlueTransformScope()
	{
		++g_glueTransformDepth;
	}

	GlueTransformScope::~GlueTransformScope()
	{
		--g_glueTransformDepth;
	}

	ComponentUnitScope::ComponentUnitScope(void* unit)
	    : m_previous(g_componentUnit)
	{
		g_componentUnit = unit;
	}

	ComponentUnitScope::~ComponentUnitScope()
	{
		g_componentUnit = m_previous;
	}

	const char* RaceFolder(int raceId)
	{
		if (raceId < 0 || raceId >= (int)(sizeof kRaceFolders / sizeof kRaceFolders[0]))
			return nullptr;
		return kRaceFolders[raceId];
	}

	const char* SexFolder(int sexId)
	{
		return sexId == 0 || sexId == 1 ? kSexFolders[sexId] : nullptr;
	}

	bool FileExists(const char* path)
	{
		std::string key = LowerKey(path);
		auto it = g_fileExists.find(key);
		if (it != g_fileExists.end())
			return it->second;
		void* handle = nullptr;
		bool exists = ClientData::Streaming::OpenFileEx(nullptr, path, 0, &handle) != 0 && handle;
		if (exists)
			ClientData::Streaming::CloseFile(handle);
		g_fileExists.emplace(key, exists);
		return exists;
	}

	void ApplySlots(void* component)
	{
		void* model = ClientData::CharComponent::Model(component);
		const Manifest::Data* data = ManifestOf(component);
		if (!model || !data)
			return;
		Manifest::Look look = Manifest::LookOf(component);
		for (int type : kSlotTypes)
		{
			const char* path = Manifest::Slot(*data, type, look);
			if (!path)
				continue;
			void* texture = CharacterCreateTexture(path, s_textureStatus);
			if (!texture)
			{
				LOG_WARN << "hd characters: slot " << type << " texture " << path << " failed to load";
				continue;
			}
			CM2Model__ReplaceTexture(model, type, texture);
			TextureRelease(texture);
		}
	}

	bool ApplySkinExtra(void* component)
	{
		void* model = ClientData::CharComponent::Model(component);
		const Manifest::Data* data = ManifestOf(component);
		if (!model || !data)
			return false;
		const char* path = Manifest::Texture(*data, Manifest::kTargetSkinExtra, Manifest::LookOf(component));
		if (!path)
			return false;
		void* texture = CharacterCreateTexture(path, s_textureStatus);
		if (!texture)
		{
			LOG_WARN << "hd characters: skin extra texture " << path << " failed to load";
			return false;
		}
		CM2Model__ReplaceTexture(model, kTextureTypeSkinExtra, texture);
		TextureRelease(texture);
		return true;
	}

	void ApplyNpcBake(void* componentData)
	{
		if (!componentData)
			return;
		uint32& flags = *reinterpret_cast<uint32*>(static_cast<uint8*>(componentData) + Component::kOffDataFlags);
		if (!(flags & Component::kDataFlagBaked))
			return;
		char* path = reinterpret_cast<char*>(static_cast<uint8*>(componentData) + Component::kOffDataBakePath);
		const uint32 extraId = DisplayExtraIdOf(g_componentUnit);
		const char* bake = NpcDisplay::BakePath(NpcDisplay::Find(extraId));
		if (bake && std::strlen(bake) < Component::kDataBakePathSize && FileExists(bake))
		{
			std::strcpy(path, bake);
			LOG_DEBUG << "hd characters: display extra " << extraId << " uses the baked hd texture " << bake;
			return;
		}
		flags &= ~Component::kDataFlagBaked;
		path[0] = 0;
		LOG_DEBUG << "hd characters: display extra " << extraId << " has no baked hd texture, the sheet is composited from the manifest and the npc items";
	}

	bool IsCustomizationGroup(int group)
	{
		return group == 0 || group == 1 || group == 2 || group == 3 || group == 16;
	}

	void ApplyGeosets(void* component)
	{
		void* model = ClientData::CharComponent::Model(component);
		const Manifest::Data* data = ManifestOf(component);
		if (!model || !data)
			return;
		for (const Manifest::Range& range : data->hides)
			CM2Model__SetGeometryVisible(model, (unsigned int)range.from, (unsigned int)range.to, 0);
		std::vector<int> shows;
		Manifest::ResolveShows(*data, Manifest::LookOf(component), shows);
		int boots = Field(component, Component::kOffGeosetGroups + 4 * Component::kGroupBoots);
		for (int id : shows)
		{
			int group = id / 100;
			if (group < Component::kFirstUnmanagedGroup && group != Component::kGroupEyeGlow)
				continue;
			if (id == Component::kGeosetFeet && boots != 0)
				id = Component::kGeosetFeetBooted;
			CM2Model__SetGeometryVisible(model, (unsigned int)id, (unsigned int)id, 1);
		}
	}

	void ApplyCustomizationGroups(void* component)
	{
		const Manifest::Data* data = ManifestOf(component);
		if (!data)
			return;
		std::vector<int> shows;
		Manifest::ResolveShows(*data, Manifest::LookOf(component), shows, true);
		int* groups = reinterpret_cast<int*>(static_cast<uint8*>(component) + Component::kOffGeosetGroups);
		for (int id : shows)
		{
			int group = id / 100;
			if (!IsCustomizationGroup(group))
				continue;
			groups[group] = id % 100 == 0 ? 0 : id;
		}
	}

	void SnapshotHelmGroups(const void* component, int* out)
	{
		for (int i = 0; i < Component::kHelmGroupCount; ++i)
			out[i] = Field(component, Component::kOffGeosetGroups + 4 * Component::kHelmGroups[i]);
	}

	void ApplyHelmHides(void* component, const int* before)
	{
		const Manifest::Data* data = ManifestOf(component);
		if (!data)
			return;
		int* groups = reinterpret_cast<int*>(static_cast<uint8*>(component) + Component::kOffGeosetGroups);
		std::vector<int> shows;
		bool resolved = false;
		for (int i = 0; i < Component::kHelmGroupCount; ++i)
		{
			int group = Component::kHelmGroups[i];
			int hidden = group * 100 + 1;
			if (groups[group] != hidden || before[i] == hidden)
				continue;
			int value = hidden;
			auto it = data->helms.find(group);
			if (it != data->helms.end())
				value = it->second;
			if (group == Component::kGroupHair)
			{
				if (!resolved)
				{
					Manifest::ResolveShows(*data, Manifest::LookOf(component), shows, true);
					resolved = true;
				}
				for (int id : shows)
				{
					if (id / 100 == Component::kGroupHair && id % 100 == 0)
						value = 0;
				}
			}
			if (value == groups[group])
				continue;
			LOG_DEBUG << "hd characters: helm hides group " << group << ", geoset " << value << " instead of " << hidden;
			groups[group] = value;
		}
	}

	const char* ModelStem(const void* model)
	{
		if (!model)
			return nullptr;
		return ClientData::M2Model::SharedFilePath(model);
	}

	float BakedScale(int race, int sex)
	{
		int key = race * 2 + sex;
		auto it = g_bakedScales.find(key);
		if (it != g_bakedScales.end())
			return it->second;
		float baked = 1.0f;
		int displayId = Player_C_GetDisplayId(race, (unsigned int)sex);
		auto* display = static_cast<const ClientData::CreatureDisplayInfoRow*>(ClientDB::GetRowById(s_creatureDisplayInfoDB, (uint32)displayId));
		if (display)
		{
			auto* modelData = static_cast<const ClientData::CreatureModelDataRow*>(ClientDB::GetRowById(s_creatureModelDataDB, display->m_modelId));
			if (modelData)
				baked = display->m_creatureModelScale * modelData->m_modelScale;
		}
		if (baked <= 0.0f)
			baked = 1.0f;
		g_bakedScales.emplace(key, baked);
		return baked;
	}
}

CLIENT_DETOUR_THISCALL(CGUnit_C__GetModelFileName_HD, 0x00717B20, bool, (const char** modelFileName))
{
	bool result = CGUnit_C__GetModelFileName_HD(self, modelFileName);
	if (!result || !modelFileName || !*modelFileName || !HDCharacters::IsEnabled())
		return result;
	if (!HDCharacters::IsHdUnit(self))
		return result;
	if (const char* hd = HDCharacters::ResolveModelPath(*modelFileName))
	{
		LOG_DEBUG << "hd characters: " << *modelFileName << " -> " << hd;
		*modelFileName = hd;
	}
	else
		LOG_DEBUG << "hd characters: no hd model for " << *modelFileName;
	return result;
}

CLIENT_DETOUR_THISCALL(CGUnit_C__GetModelScale_HD, 0x0071C110, float, (void* displayInfo, void* modelData))
{
	using namespace HDCharacters;
	float scale = CGUnit_C__GetModelScale_HD(self, displayInfo, modelData);
	if (!IsEnabled() || !displayInfo || !modelData || !IsHdUnit(self))
		return scale;
	const char* modelName = static_cast<const ClientData::CreatureModelDataRow*>(modelData)->m_modelName;
	if (!ResolveModelPath(modelName))
		return scale;
	int race = 0;
	int sex = 0;
	float baked = RaceSexOfCharacterPath(modelName, race, sex)
	                  ? BakedScale(race, sex)
	                  : static_cast<const ClientData::CreatureDisplayInfoRow*>(displayInfo)->m_creatureModelScale * static_cast<const ClientData::CreatureModelDataRow*>(modelData)->m_modelScale;
	if (baked <= 0.0f)
		return scale;
	return scale / baked;
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__Init_HD, 0x004F24D0, char, (void* componentData, int a3))
{
	using namespace HDCharacters;
	g_hdComponents.erase(self);
	bool hd = false;
	if (componentData)
	{
		const void* model = *reinterpret_cast<void**>(static_cast<uint8*>(componentData) + Component::kOffDataModel);
		hd = IsHdModel(model);
		if (hd)
			g_hdComponents.insert(self);
	}
	if (hd)
		ApplyNpcBake(componentData);
	char result = CCharacterComponent__Init_HD(self, componentData, a3);
	if (hd)
		ApplySlots(self);
	return result;
}

CLIENT_DETOUR(CCharacterComponent__FreeComponent_HD, 0x004F16C0, __cdecl, void, (void* component))
{
	HDCharacters::g_hdComponents.erase(component);
	HDCharacters::Sheet::Destroy(component);
	CCharacterComponent__FreeComponent_HD(component);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__LoadBaseVariation_HD, 0x004EA1F0, void, (unsigned int variation, int textureIndex, int variationIndex, int colorIndex, char section, const char* a7))
{
	using namespace HDCharacters;
	if (!IsHdComponent(self))
	{
		CCharacterComponent__LoadBaseVariation_HD(self, variation, textureIndex, variationIndex, colorIndex, section, a7);
		return;
	}
	ContextScope scope(self, TargetOf((int)variation, textureIndex));
	switch (variation)
	{
	case 0:
	case 4:
		scope.Override(Manifest::kOptionSkinColor, colorIndex);
		break;
	case 1:
		scope.Override(Manifest::kOptionFace, variationIndex);
		scope.Override(Manifest::kOptionSkinColor, colorIndex);
		break;
	case 2:
		scope.Override(Manifest::kOptionFacial, variationIndex);
		scope.Override(Manifest::kOptionHairColor, colorIndex);
		break;
	case 3:
		scope.Override(Manifest::kOptionHairStyle, variationIndex);
		scope.Override(Manifest::kOptionHairColor, colorIndex);
		break;
	default:
		break;
	}
	CCharacterComponent__LoadBaseVariation_HD(self, variation, textureIndex, variationIndex, colorIndex, section, a7);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__SetSkinColor_HD, 0x004EA6B0, void, (int skinColor, char a3, char a4, const char* a5))
{
	using namespace HDCharacters;
	if (!IsHdComponent(self))
	{
		CCharacterComponent__SetSkinColor_HD(self, skinColor, a3, a4, a5);
		return;
	}
	ContextScope scope(self, kTargetByName);
	scope.Override(Manifest::kOptionSkinColor, skinColor);
	CCharacterComponent__SetSkinColor_HD(self, skinColor, a3, a4, a5);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__ReplaceHairTexture_HD, 0x004EA150, void, (int hairStyle, int a3))
{
	using namespace HDCharacters;
	if (!IsHdComponent(self))
	{
		CCharacterComponent__ReplaceHairTexture_HD(self, hairStyle, a3);
		return;
	}
	ContextScope scope(self, Manifest::kTargetHair);
	scope.Override(Manifest::kOptionHairStyle, hairStyle);
	CCharacterComponent__ReplaceHairTexture_HD(self, hairStyle, a3);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__SetHairStyle_HD, 0x004EA3E0, void, (int hairStyle, int a4))
{
	CCharacterComponent__SetHairStyle_HD(self, hairStyle, a4);
	if (HDCharacters::IsHdComponent(self))
		HDCharacters::ApplyCustomizationGroups(self);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__SetBeardStyle_HD, 0x004EA590, void, (int facialStyle, char a3, int a4))
{
	CCharacterComponent__SetBeardStyle_HD(self, facialStyle, a3, a4);
	if (HDCharacters::IsHdComponent(self))
		HDCharacters::ApplyCustomizationGroups(self);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__ReplaceExtraSkinTexture_HD, 0x004EA0B0, char, (int a2))
{
	using namespace HDCharacters;
	if (!IsHdComponent(self))
		return CCharacterComponent__ReplaceExtraSkinTexture_HD(self, a2);
	if (ApplySkinExtra(self))
		return 1;
	ContextScope scope(self, Manifest::kTargetSkinExtra);
	return CCharacterComponent__ReplaceExtraSkinTexture_HD(self, a2);
}

CLIENT_DETOUR_THISCALL_NOARGS(CCharacterComponent__GeosRenderPrep_HD, 0x004ED900, void)
{
	CCharacterComponent__GeosRenderPrep_HD(self);
	if (HDCharacters::IsHdComponent(self))
		HDCharacters::ApplyGeosets(self);
}

CLIENT_DETOUR(TextureCacheCreateTexture_HD, 0x004F3930, __cdecl, void*, (char* path))
{
	if (const char* resolved = HDCharacters::RedirectTexture(path))
		return TextureCacheCreateTexture_HD(const_cast<char*>(resolved));
	return TextureCacheCreateTexture_HD(path);
}

CLIENT_DETOUR(CharacterCreateTexture_HD, 0x004E8D30, __cdecl, void*, (char* path, int status))
{
	if (const char* resolved = HDCharacters::RedirectTexture(path))
		return CharacterCreateTexture_HD(const_cast<char*>(resolved), status);
	return CharacterCreateTexture_HD(path, status);
}

CLIENT_DETOUR_THISCALL(CCharacterComponent__AddHelm_HD, 0x004EF0D0, void, (int itemDisplayId))
{
	using namespace HDCharacters;
	if (!IsHdComponent(self))
	{
		CCharacterComponent__AddHelm_HD(self, itemDisplayId);
		return;
	}
	int before[Component::kHelmGroupCount];
	SnapshotHelmGroups(self, before);
	CCharacterComponent__AddHelm_HD(self, itemDisplayId);
	ApplyRetailHelmHides(self, before);
	ApplyHelmHides(self, before);
	ApplyHelmAttach(self);
}

CLIENT_DETOUR(Player_C_GetModelName_HD, 0x006DC810, __cdecl, void*, (int race, unsigned int sex))
{
	using namespace HDCharacters;
	void* record = Player_C_GetModelName_HD(race, sex);
	if (!record || !IsEnabled())
		return record;
	const char* modelName = static_cast<const ClientData::CreatureModelDataRow*>(record)->m_modelName;
	const char* hd = ResolveModelPath(modelName);
	if (!hd)
		return record;
	void* copy = GlueModelRecord(race, sex, record, hd);
	if (copy != record)
		LOG_DEBUG << "hd characters: glue " << modelName << " -> " << hd;
	return copy;
}

CLIENT_DETOUR(CharacterSelectionDisplay__SetCharacterFacing_HD, 0x004E2E70, __cdecl, int, (float facing))
{
	HDCharacters::GlueTransformScope scope;
	return CharacterSelectionDisplay__SetCharacterFacing_HD(facing);
}

CLIENT_DETOUR(CCharCreateInfo__SetCharacterFacing_HD, 0x004E0160, __cdecl, int, (float facing))
{
	HDCharacters::GlueTransformScope scope;
	return CCharCreateInfo__SetCharacterFacing_HD(facing);
}

CLIENT_DETOUR_THISCALL(CM2Model__SetWorldTransform_HD, 0x008251D0, void, (void* position, float rotation, float scale))
{
	if (HDCharacters::InGlueTransform() && HDCharacters::IsHdModel(self))
		scale *= HDCharacters::GlueScale(self);
	CM2Model__SetWorldTransform_HD(self, position, rotation, scale);
}

CLIENT_DETOUR_THISCALL_NOARGS(CGUnit_C__InitializeComponent_HD, 0x00730100, int)
{
	if (sPlayerRenderLimit.ShouldSuppressComponent(self))
		return 0;
	HDCharacters::ComponentUnitScope unit(self);
	return CGUnit_C__InitializeComponent_HD(self);
}

CLIENT_DETOUR_THISCALL_NOARGS(CGUnit_C__RefreshDataPointers_HD, 0x0072D940, int)
{
	using namespace HDCharacters;
	if (!IsEnabled() || !NpcDisplay::HasSwaps())
		return CGUnit_C__RefreshDataPointers_HD(self);
	ClientData::UnitFields* fields = ClientData::WowUnit(self).Fields();
	if (!fields)
		return CGUnit_C__RefreshDataPointers_HD(self);
	uint32& displayId = fields->displayId;
	uint32& nativeId = fields->nativeDisplayId;
	const uint32 swap = NpcDisplay::Swap(displayId, IsPlayerUnit(self));
	if (!swap || swap == displayId)
		return CGUnit_C__RefreshDataPointers_HD(self);
	const uint32 previousDisplay = displayId;
	const uint32 previousNative = nativeId;
	displayId = swap;
	if (previousNative == previousDisplay)
		nativeId = swap;
	int result = CGUnit_C__RefreshDataPointers_HD(self);
	displayId = previousDisplay;
	nativeId = previousNative;
	LOG_DEBUG << "hd characters: display " << previousDisplay << " rendered as " << swap;
	return result;
}
