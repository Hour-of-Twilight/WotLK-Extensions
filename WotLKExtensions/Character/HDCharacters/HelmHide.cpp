#include <Defines.h>
#include <Character/HDCharacters/HDCharacters.h>

#include <CDBCDefs/HDHelmHide.h>
#include <CDBCMgr.h>
#include <ClientData/ClientDB.h>
#include <ClientData/SharedDefines.h>
#include <Logger.h>
#include <Macros.h>

#include <cstdint>

namespace HDCharacters
{
	namespace
	{
		const char* kHelmHideTable = "HDHelmHide";

		CLIENT_FUNCTION(CM2Model__SetGeosetVisible, 0x0082C7C0, __thiscall, void*,
		    (void* model, unsigned int start, unsigned int end, int visible))

		int Field(const void* component, uint32 offset)
		{
			return *reinterpret_cast<const int*>(static_cast<const uint8*>(component) + offset);
		}

		int HelmVisId(const void* component)
		{
			int displayId = Field(component, Component::kOffItemDisplayIds + 4 * Component::kItemSlotHelm);
			if (displayId <= 0)
				return 0;
			ClientData::DB::ItemDisplayInfoRec record;
			if (!ClientData::DB::ItemDisplayInfoDB::Copy(displayId, record))
				return 0;
			int sex = Field(component, Component::kOffSex);
			if (sex < 0 || sex > 1)
				return 0;
			return static_cast<int>(record.m_helmetGeosetVisID[sex]);
		}

		const HDHelmHideRow* RowFor(int visId, int race)
		{
			if (visId <= 0 || race < 0)
				return nullptr;
			return GlobalCDBCMap.getRow<HDHelmHideRow>(kHelmHideTable,
			    (uint32)visId * 100u + (uint32)race);
		}
	}

	void ApplyRetailHelmHides(void* component, const int* before)
	{
		if (!component)
			return;
		int race = Field(component, Component::kOffRace);
		const HDHelmHideRow* row = RowFor(HelmVisId(component), race);
		if (!row)
			return;

		int* groups = reinterpret_cast<int*>(static_cast<uint8*>(component) + Component::kOffGeosetGroups);
		int added = 0;
		int restored = 0;
		for (int i = 0; i < Component::kHelmGroupCount; ++i)
		{
			int group = Component::kHelmGroups[i];
			int hidden = group * 100 + 1;
			bool wanted = (row->maskGroups & (1u << i)) != 0;
			if (wanted && groups[group] != hidden)
			{
				groups[group] = hidden;
				++added;
			}
			else if (!wanted && groups[group] == hidden && before[i] != hidden)
			{
				groups[group] = before[i];
				++restored;
			}
		}

		void* model = ClientData::CharComponent::Model(component);
		int cleared = 0;
		if (model)
		{
			for (uint32 i = 0; i < HD_HELM_HIDE_EXTRA_SLOTS; ++i)
			{
				uint32 group = row->groups[i];
				if (!group)
					continue;
				CM2Model__SetGeosetVisible(model, group * 100, group * 100 + 99, 0);
				++cleared;
			}
		}

		if (added || restored || cleared)
		{
			LOG_DEBUG << "hd characters: helm vis " << row->visID << " race " << race
			          << " hid " << added << " groups, put back " << restored
			          << ", cleared " << cleared << " customisation groups";
		}
	}
}
