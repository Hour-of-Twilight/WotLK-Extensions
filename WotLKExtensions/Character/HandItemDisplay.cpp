#include <Defines.h>
#include <Character/HandItemDisplay.h>

#include <ClientDetours.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/M2Model.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/WowObjects.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace HandItemDisplay
{

	constexpr int kNoAttachment = -1;

	constexpr size_t kVTableGetVirtualItem = 0x12C / sizeof(void*);
	constexpr size_t kVTableGetVirtualItemDisplay = 0x130 / sizeof(void*);

	constexpr uintptr_t kModelRefCountOffset = 0x00;
	constexpr uintptr_t kModelLocalMatrixOffset = 0xB4;
	constexpr size_t kMatrixCells = 16;
	constexpr float kDefaultShieldOffset[3] = { -0.015f, -0.104f, 0.132f };

	constexpr uint8 kFoundWeapon = 1;
	constexpr uint8 kFoundShield = 2;

	constexpr size_t kPathSize = 260;

	const char* const kWeaponFolder = "Item\\ObjectComponents\\Weapon\\";
	const char* const kShieldFolder = "Item\\ObjectComponents\\Shield\\";

	struct VirtualItemInfo
	{
		uint8 itemClass;
		uint8 subClass;
		uint8 pad[2];
		uint8 inventoryType;
		uint8 sheath;
	};

	using GetVirtualItemFn = VirtualItemInfo*(__thiscall*)(void* unit, int slot, int a3);
	using GetVirtualItemDisplayFn = int(__thiscall*)(void* unit, int slot, ItemDisplayInfoRec* out);

	static int SheathAttachment(int sheath, bool leftHand)
	{
		switch (sheath)
		{
		case 1:
			return leftHand ? 27 : 26;
		case 2:
			return leftHand ? 31 : 30;
		case 3:
			return leftHand ? 33 : 32;
		case 4:
			return 28;
		default:
			return kNoAttachment;
		}
	}

	static std::string ModelFileName(const char* modelName)
	{
		std::string name(modelName);
		for (char& c : name)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		size_t dot = name.rfind('.');
		if (dot != std::string::npos)
		{
			std::string extension = name.substr(dot);
			if (extension == ".mdx" || extension == ".mdl")
				name.replace(dot, std::string::npos, ".m2");
		}
		return name;
	}

	static uint8 ProbeFolders(const std::string& fileName)
	{
		static std::unordered_map<std::string, uint8> cache;

		auto it = cache.find(fileName);
		if (it != cache.end())
			return it->second;

		uint8 found = 0;
		if (SFile::FileExistsEx((std::string(kWeaponFolder) + fileName).c_str(), 0))
			found |= kFoundWeapon;
		if (SFile::FileExistsEx((std::string(kShieldFolder) + fileName).c_str(), 0))
			found |= kFoundShield;

		if (found)
			cache.emplace(fileName, found);
		return found;
	}

	ModelFolder ResolveFolder(const ItemDisplayInfoRec* record, bool preferShield)
	{
		ModelFolder preferred = preferShield ? ModelFolder::Shield : ModelFolder::Weapon;
		if (!record || !record->m_modelName[0] || !*record->m_modelName[0])
			return preferred;

		uint8 found = ProbeFolders(ModelFileName(record->m_modelName[0]));
		if (found == kFoundWeapon)
			return ModelFolder::Weapon;
		if (found == kFoundShield)
			return ModelFolder::Shield;
		return preferred;
	}

	static bool AttachmentBindPosition(void* model, unsigned int attachment, float out[3])
	{
		if (!ClientData::M2Model::IsLoaded(model))
			return false;

		uintptr_t header = ClientData::M2Model::SharedData(model);
		if (!header)
			return false;

		uint32 lookupCount = *reinterpret_cast<uint32*>(header + ClientData::M2Model::kDataAttachmentLookupOffset);
		const uint16* lookup = *reinterpret_cast<const uint16**>(header + ClientData::M2Model::kDataAttachmentLookupOffset + 4);
		if (!lookup || attachment >= lookupCount)
			return false;

		uint16 index = lookup[attachment];
		uint32 count = *reinterpret_cast<uint32*>(header + ClientData::M2Model::kDataAttachmentsOffset);
		const uint8* attachments = *reinterpret_cast<const uint8**>(header + ClientData::M2Model::kDataAttachmentsOffset + 4);
		if (!attachments || index == ClientData::M2Model::kNoAttachmentIndex || index >= count)
			return false;

		const float* position = reinterpret_cast<const float*>(attachments + index * ClientData::M2Model::kAttachmentStride + ClientData::M2Model::kAttachmentPositionOffset);
		out[0] = position[0];
		out[1] = position[1];
		out[2] = position[2];
		return true;
	}

	static void MainHandShieldOffset(void* parent, float out[3])
	{
		float shieldPoint[3];
		float handPoint[3];
		if (AttachmentBindPosition(parent, ClientData::M2Model::ATTACHMENT_SHIELD, shieldPoint) && AttachmentBindPosition(parent, ClientData::M2Model::ATTACHMENT_HAND_LEFT, handPoint))
		{
			out[0] = shieldPoint[0] - handPoint[0];
			out[1] = handPoint[1] - shieldPoint[1];
			out[2] = shieldPoint[2] - handPoint[2];
			return;
		}

		out[0] = kDefaultShieldOffset[0];
		out[1] = kDefaultShieldOffset[1];
		out[2] = kDefaultShieldOffset[2];
	}

	static void SetMainHandShieldTransform(void* child, void* parent, bool applied)
	{
		float* matrix = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(child) + kModelLocalMatrixOffset);
		for (size_t i = 0; i < kMatrixCells; ++i)
			matrix[i] = 0.0f;

		float mirror = applied ? -1.0f : 1.0f;
		matrix[0] = 1.0f;
		matrix[5] = mirror;
		matrix[10] = mirror;
		matrix[15] = 1.0f;

		if (!applied)
			return;

		float offset[3];
		MainHandShieldOffset(parent, offset);
		matrix[12] = offset[0];
		matrix[13] = offset[1];
		matrix[14] = offset[2];
	}

	static void* FindChild(void* model, int attachment)
	{
		for (void* child = ClientData::M2Model::FirstChild(model); child; child = ClientData::M2Model::NextSibling(child))
		{
			if (static_cast<int>(ClientData::M2Model::AttachmentId(child)) == attachment)
				return child;
		}
		return nullptr;
	}

	CLIENT_DETOUR(CCharacterComponent__AddHandItem_ResolveFolder, 0x004EACD0, __cdecl, int, (void* model, ItemDisplayInfoRec* record, unsigned int hand, int sheath, char sheathed, char isShield, char isRangedRight, int itemVisual))
	{
		if (!model || !record)
			return kNoAttachment;

		bool leftHand;
		switch (hand)
		{
		case ClientData::EQUIPMENT_SLOT_MAINHAND:
			leftHand = false;
			break;
		case ClientData::EQUIPMENT_SLOT_OFFHAND:
			leftHand = true;
			break;
		case ClientData::EQUIPMENT_SLOT_RANGED:
			leftHand = isRangedRight == 0;
			break;
		default:
			return kNoAttachment;
		}

		ModelFolder folder = ResolveFolder(record, isShield != 0);
		int handAttachment = leftHand ? ClientData::M2Model::ATTACHMENT_HAND_LEFT : ClientData::M2Model::ATTACHMENT_HAND_RIGHT;
		if (hand == ClientData::EQUIPMENT_SLOT_OFFHAND && folder == ModelFolder::Shield)
			handAttachment = ClientData::M2Model::ATTACHMENT_SHIELD;
		int sheathAttachment = SheathAttachment(sheath, leftHand);

		if (hand == ClientData::EQUIPMENT_SLOT_OFFHAND)
		{
			CCharacterComponent::RemoveLinkpt(model, ClientData::M2Model::ATTACHMENT_SHIELD);
			CCharacterComponent::RemoveLinkpt(model, ClientData::M2Model::ATTACHMENT_HAND_LEFT);
		}
		else
			CCharacterComponent::RemoveLinkpt(model, static_cast<unsigned int>(handAttachment));
		CCharacterComponent::RemoveLinkpt(model, static_cast<unsigned int>(sheathAttachment));

		int attachment = sheathed ? sheathAttachment : handAttachment;
		if (ClientData::M2Model::IsLoadedNative(model, 0, 0) && !ClientData::M2Model::HasAttachment(model, static_cast<unsigned int>(attachment)))
			return kNoAttachment;

		const char* folderPath = folder == ModelFolder::Shield ? kShieldFolder : kWeaponFolder;
		const char* modelName = record->m_modelName[0] ? record->m_modelName[0] : "";
		const char* textureName = record->m_modelTexture[0] ? record->m_modelTexture[0] : "";

		static char modelPath[kPathSize];
		static char texturePath[kPathSize];
		std::snprintf(modelPath, sizeof(modelPath), "%s%s", folderPath, modelName);
		std::snprintf(texturePath, sizeof(texturePath), "%s%s.blp", folderPath, textureName);

		CCharacterComponent::AddLink(model, attachment, modelPath, texturePath, itemVisual ? itemVisual : record->m_itemVisual, record);

		if (folder == ModelFolder::Shield && attachment == ClientData::M2Model::ATTACHMENT_HAND_RIGHT)
		{
			if (void* child = FindChild(model, ClientData::M2Model::ATTACHMENT_HAND_RIGHT))
				SetMainHandShieldTransform(child, model, true);
		}
		return attachment;
	}

	CLIENT_DETOUR(CCharacterComponent__RemoveHandItem_BothOffHandAttachments, 0x004EB070, __cdecl, void, (void* model, int hand, int sheath, char isShield))
	{
		if (!model)
			return;

		switch (hand)
		{
		case ClientData::EQUIPMENT_SLOT_MAINHAND:
			CCharacterComponent::RemoveLinkpt(model, ClientData::M2Model::ATTACHMENT_HAND_RIGHT);
			CCharacterComponent::RemoveLinkpt(model, static_cast<unsigned int>(SheathAttachment(sheath, false)));
			break;
		case ClientData::EQUIPMENT_SLOT_OFFHAND:
			CCharacterComponent::RemoveLinkpt(model, ClientData::M2Model::ATTACHMENT_SHIELD);
			CCharacterComponent::RemoveLinkpt(model, ClientData::M2Model::ATTACHMENT_HAND_LEFT);
			CCharacterComponent::RemoveLinkpt(model, static_cast<unsigned int>(SheathAttachment(sheath, true)));
			break;
		case ClientData::EQUIPMENT_SLOT_RANGED:
			CCharacterComponent::RemoveLinkpt(model, isShield ? ClientData::M2Model::ATTACHMENT_SHIELD : ClientData::M2Model::ATTACHMENT_HAND_LEFT);
			CCharacterComponent::RemoveLinkpt(model, static_cast<unsigned int>(SheathAttachment(sheath, true)));
			break;
		default:
			break;
		}
	}

	int SheatheHandItem(void* unit, int slot, int moveToSheath)
	{
		void* model = ClientData::WowUnit(unit).BodyModel();
		if (!model)
			return 0;

		void** vtable = *reinterpret_cast<void***>(unit);
		VirtualItemInfo* info = reinterpret_cast<GetVirtualItemFn>(vtable[kVTableGetVirtualItem])(unit, slot, 0);
		if (!info)
			return 0;

		bool mainHandRules = slot == ClientData::VIRTUAL_ITEM_SLOT_MAINHAND || info->inventoryType == ClientData::INVTYPE_RANGEDRIGHT || info->inventoryType == ClientData::INVTYPE_THROWN;

		ItemDisplayInfoRec record = {};
		const ItemDisplayInfoRec* recordPtr = nullptr;
		if (slot != ClientData::VIRTUAL_ITEM_SLOT_RANGED && reinterpret_cast<GetVirtualItemDisplayFn>(vtable[kVTableGetVirtualItemDisplay])(unit, slot, &record))
			recordPtr = &record;
		bool shieldModel = slot != ClientData::VIRTUAL_ITEM_SLOT_RANGED && ResolveFolder(recordPtr, info->inventoryType == ClientData::INVTYPE_SHIELD) == ModelFolder::Shield;

		int handAttachment;
		switch (slot)
		{
		case ClientData::VIRTUAL_ITEM_SLOT_MAINHAND:
			handAttachment = ClientData::M2Model::ATTACHMENT_HAND_RIGHT;
			break;
		case ClientData::VIRTUAL_ITEM_SLOT_OFFHAND:
			handAttachment = shieldModel ? ClientData::M2Model::ATTACHMENT_SHIELD : ClientData::M2Model::ATTACHMENT_HAND_LEFT;
			break;
		case ClientData::VIRTUAL_ITEM_SLOT_RANGED:
			handAttachment = mainHandRules ? ClientData::M2Model::ATTACHMENT_HAND_RIGHT : ClientData::M2Model::ATTACHMENT_HAND_LEFT;
			break;
		default:
			return 0;
		}

		int sheathAttachment = CCharacterComponent::GetSheatheLink(info->sheath, static_cast<char>(mainHandRules));
		int from = moveToSheath ? handAttachment : sheathAttachment;
		int to = moveToSheath ? sheathAttachment : handAttachment;

		bool hideShieldInBarberShop = false;
		if (CGUnit_C::IsActivePlayer(unit) && *CGBarberShop::m_barberShopEnabled && info->inventoryType == ClientData::INVTYPE_SHIELD)
		{
			hideShieldInBarberShop = true;
			to = kNoAttachment;
		}

		void* child = FindChild(model, from);
		if (!child)
		{
			if (!hideShieldInBarberShop)
				CGUnit_C::AddHandItemForSlot(unit, slot);
		}
		else
		{
			++*reinterpret_cast<uint32*>(reinterpret_cast<uintptr_t>(child) + kModelRefCountOffset);
			ClientData::M2Model::DetachFromParent(child);
			if (to != kNoAttachment)
				ClientData::M2Model::AttachToParent(child, model, static_cast<unsigned int>(to), nullptr, 0);
			if (slot == ClientData::VIRTUAL_ITEM_SLOT_MAINHAND && shieldModel)
				SetMainHandShieldTransform(child, model, to == ClientData::M2Model::ATTACHMENT_HAND_RIGHT);
			ClientData::M2Model::Release(child);

			if (handAttachment != ClientData::M2Model::ATTACHMENT_SHIELD)
			{
				int leftHand = handAttachment != ClientData::M2Model::ATTACHMENT_HAND_RIGHT ? 1 : 0;
				if (to == handAttachment)
					CCharacterComponent::ComponentCloseFingers(model, leftHand);
				else
					CCharacterComponent::ComponentOpenFingers(model, leftHand);
			}

			if (slot == ClientData::VIRTUAL_ITEM_SLOT_RANGED)
			{
				if (!moveToSheath)
					return 1;
				CGUnit_C::AddHandItemForSlot(unit, ClientData::VIRTUAL_ITEM_SLOT_MAINHAND);
				CGUnit_C::AddHandItemForSlot(unit, ClientData::VIRTUAL_ITEM_SLOT_OFFHAND);
			}
		}

		if (moveToSheath && mainHandRules)
			CGUnit_C::ShowHandItemSpellEffects(unit, 1, 0);
		return 1;
	}
}
