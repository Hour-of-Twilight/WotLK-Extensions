#include <Defines.h>
#include <Character/HDCharacters/HDCharacters.h>
#include <ClientData/CharComponent.h>
#include <ClientData/M2Model.h>

#include <Logger.h>
#include <Macros.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace HDCharacters
{
	namespace
	{
		constexpr uint32 kMatrixTranslation = 12;

		CLIENT_FUNCTION(C44Matrix__Identity, 0x0057C2A0, __thiscall, void, (void* matrix))
		CLIENT_FUNCTION(C44Matrix__Scale, 0x004C1B90, __thiscall, void, (void* matrix, const float* scale))

		struct HelmFit
		{
			const char* name;
			float x;
			float y;
			float z;
			float scale;
		};

		constexpr HelmFit kHelmFits[] = {
#define HELM_FIT(name, x, y, z, scale) { name, x, y, z, scale },
#include <Character/HDCharacters/HelmFit.inc>
#undef HELM_FIT
		};

		std::unordered_map<std::string, const HelmFit*> g_helmFits;

		struct RaceFit
		{
			int race;
			int sex;
			float x;
			float y;
			float z;
			float scale;
		};

		constexpr RaceFit kRaceFits[] = {
#define HELM_RACE_FIT(race, sex, x, y, z, scale) { race, sex, x, y, z, scale },
#include <Character/HDCharacters/HelmRaceFit.inc>
#undef HELM_RACE_FIT
		};

		int Field(const void* component, uint32 offset)
		{
			return *reinterpret_cast<const int*>(static_cast<const uint8*>(component) + offset);
		}

		const HelmFit* HelmFitFor(const char* stem)
		{
			if (!stem || !*stem)
				return nullptr;
			if (g_helmFits.empty())
			{
				g_helmFits.reserve(sizeof(kHelmFits) / sizeof(kHelmFits[0]));
				for (const HelmFit& fit : kHelmFits)
					g_helmFits.emplace(fit.name, &fit);
			}
			const char* name = stem;
			for (const char* p = stem; *p; ++p)
			{
				if (*p == '\\' || *p == '/')
					name = p + 1;
			}
			std::string key(name);
			size_t dot = key.rfind('.');
			if (dot != std::string::npos)
				key.erase(dot);
			for (char& c : key)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			auto it = g_helmFits.find(key);
			return it == g_helmFits.end() ? nullptr : it->second;
		}

		const RaceFit* RaceFitFor(int race, int sex)
		{
			for (const RaceFit& fit : kRaceFits)
			{
				if (fit.race == race && fit.sex == sex)
					return &fit;
			}
			return nullptr;
		}
	}

	void ApplyHelmFit(void* component)
	{
		if (!component)
			return;
		int race = Field(component, Component::kOffRace);
		int sex = Field(component, Component::kOffSex);
		if (race < 0 || race >= kRaceCount || sex < 0 || sex > 1)
			return;
		void* parent = ClientData::CharComponent::Model(component);
		if (!parent)
			return;
		void* link = ClientData::M2Model::FirstChild(parent);
		if (!link || ClientData::M2Model::AttachmentId(link) != ClientData::M2Model::ATTACHMENT_HELM)
			return;
		const char* stem = ModelStem(link);
		const HelmFit* fit = HelmFitFor(stem);
		const RaceFit* raceFit = fit ? nullptr : RaceFitFor(race, sex);
		float offset[3] = { 0.0f, 0.0f, 0.0f };
		float scale;
		if (fit)
		{
			offset[0] = fit->x;
			offset[1] = fit->y;
			offset[2] = fit->z;
			scale = fit->scale;
		}
		else if (raceFit)
		{
			offset[0] = raceFit->x;
			offset[1] = raceFit->y;
			offset[2] = raceFit->z;
			scale = raceFit->scale;
		}
		else
		{
			scale = BakedScale(race, sex);
			if (scale == 1.0f)
				return;
		}
		float* placement = ClientData::M2Model::Placement(link);
		const float factor[3] = { scale, scale, scale };
		C44Matrix__Identity(placement);
		C44Matrix__Scale(placement, factor);
		placement[kMatrixTranslation + 0] = offset[0];
		placement[kMatrixTranslation + 1] = offset[1];
		placement[kMatrixTranslation + 2] = offset[2];
		LOG_DEBUG << "hd characters: helm " << (stem ? stem : "?") << " scaled by " << scale << " and moved by "
		          << offset[0] << " " << offset[1] << " " << offset[2] << (fit ? " (measured)" : " (race median)");
	}
}
