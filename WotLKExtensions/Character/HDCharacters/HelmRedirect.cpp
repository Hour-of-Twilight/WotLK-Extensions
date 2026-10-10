#include <Defines.h>
#include <Character/HDCharacters/HDCharacters.h>
#include <ClientData/CharComponent.h>
#include <ClientData/M2Model.h>

#include <ClientDetours.h>
#include <Logger.h>
#include <Macros.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace HDCharacters
{
	namespace
	{
		constexpr size_t kPlacementSize = 16 * sizeof(float);
		constexpr uint32 kMatrixTranslation = 12;
		constexpr size_t kPathSize = 260;
		constexpr const char* kStockHeadFolder = "Item\\ObjectComponents\\Head\\";
		constexpr const char* kHdHeadFolder = "CharacterHD\\Item\\ObjectComponents\\Head\\";
		constexpr const char* kHdMarker = "_HD";

		CLIENT_FUNCTION(C44Matrix__Identity, 0x0057C2A0, __thiscall, void, (void* matrix))

		struct HelmAttach
		{
			int race;
			int sex;
			float x;
			float y;
			float z;
		};

		constexpr HelmAttach kHelmAttach[] = {
#define HELM_ATTACH(race, sex, x, y, z) { race, sex, x, y, z },
#include <Character/HDCharacters/HelmAttach.inc>
#undef HELM_ATTACH
		};

		const HelmAttach* AttachFor(int race, int sex)
		{
			for (const HelmAttach& row : kHelmAttach)
			{
				if (row.race == race && row.sex == sex)
					return &row;
			}
			return nullptr;
		}

		bool HdHelmPath(const char* modelPath, char* model)
		{
			if (!modelPath)
				return false;
			size_t folder = std::strlen(kStockHeadFolder);
			if (_strnicmp(modelPath, kStockHeadFolder, folder) != 0)
				return false;
			const char* name = modelPath + folder;
			const char* dot = std::strrchr(name, '.');
			int stem = dot ? static_cast<int>(dot - name) : static_cast<int>(std::strlen(name));
			char probe[kPathSize];
			std::snprintf(probe, sizeof(probe), "%s%.*s%s.m2", kHdHeadFolder, stem, name, kHdMarker);
			if (!FileExists(probe))
				return false;
			std::snprintf(model, kPathSize, "%s%.*s%s%s", kHdHeadFolder, stem, name, kHdMarker,
			    dot ? dot : ".mdx");
			return true;
		}

		bool IsRedirectedHelm(const void* model)
		{
			if (!model)
				return false;
			if (ClientData::M2Model::AttachmentId(model) != ClientData::M2Model::ATTACHMENT_HELM)
				return false;
			const char* stem = ModelStem(model);
			return stem && _strnicmp(stem, kHdHeadFolder, std::strlen(kHdHeadFolder)) == 0;
		}

		void CopyHelmPlacement(void* copy, const void* source)
		{
			if (!copy || !IsRedirectedHelm(source))
				return;
			std::memcpy(ClientData::M2Model::Placement(copy), ClientData::M2Model::Placement(source), kPlacementSize);
		}
	}

	void ApplyHelmAttach(void* component)
	{
		if (!component)
			return;
		int race = *reinterpret_cast<const int*>(static_cast<const uint8*>(component) + Component::kOffRace);
		int sex = *reinterpret_cast<const int*>(static_cast<const uint8*>(component) + Component::kOffSex);
		void* parent = ClientData::CharComponent::Model(component);
		if (!parent)
			return;
		void* link = ClientData::M2Model::FirstChild(parent);
		if (!IsRedirectedHelm(link))
			return;
		const HelmAttach* row = AttachFor(race, sex);
		if (!row)
			return;
		float* placement = ClientData::M2Model::Placement(link);
		C44Matrix__Identity(placement);
		placement[kMatrixTranslation + 0] = row->x;
		placement[kMatrixTranslation + 1] = row->y;
		placement[kMatrixTranslation + 2] = row->z;
		LOG_DEBUG << "hd characters: retail helm " << ModelStem(link)
		          << " moved to the retail attachment by " << row->x << " " << row->y << " " << row->z;
	}
}

CLIENT_DETOUR(CCharacterComponent__AddLink_HD, 0x004EAA70, __cdecl, void,
    (void* model, int attachment, const char* modelPath, const char* texturePath, int itemVisual, void* record))
{
	using namespace HDCharacters;
	char redirected[kPathSize];
	if (attachment == ClientData::M2Model::ATTACHMENT_HELM && IsHdModel(model) && HdHelmPath(modelPath, redirected))
	{
		LOG_DEBUG << "hd characters: helm " << modelPath << " redirected to " << redirected;
		CCharacterComponent__AddLink_HD(model, attachment, redirected, texturePath, itemVisual, record);
		return;
	}
	CCharacterComponent__AddLink_HD(model, attachment, modelPath, texturePath, itemVisual, record);
}

CLIENT_DETOUR_THISCALL(CM2Scene__DuplicateModel_HD, 0x0081F970, void*, (void* source, int flags))
{
	using namespace HDCharacters;
	void* copy = CM2Scene__DuplicateModel_HD(self, source, flags);
	CopyHelmPlacement(copy, source);
	return copy;
}
