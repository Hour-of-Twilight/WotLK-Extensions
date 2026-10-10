#include <Defines.h>
#include "LootWindow.h"

#include <SharedDefines.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/WowObjects.h>
#include <Util.h>

static const uint32 kLootMoveCloseJz[] = {
	0x0072E64B, // CGUnit_C__OnMoveStartLocal
	0x0072E6FB, // CGUnit_C__OnStrafeStartLocal
	0x0072E7AB, // move variant (sub_72E730)
	0x0072E8BD, // CGUnit_C__OnTurnStartLocal
	0x0072E97B, // CGUnit_C__OnPitchStartLocal
	0x0072EA2B, // CGUnit_C__OnPitchStopLocal
	0x0072EB4B, // CGUnit_C__OnMovementInitiated
	0x0072EBCA, // move variant (sub_72EB80)
	0x0072AF47, // move variant
	0x0072D22E, // move variant
};

static const uint32 kLootRemovedTargetGateJz = 0x006D5897; // CGPlayer_C__OnLootRemoved
static const uint8 kLootRemovedTargetGateJzBytes[] = { 0x74, 0x2C };

static const float kMaxRange = 45.0f;

LootWindow& LootWindow::Instance()
{
	static LootWindow instance;
	return instance;
}

void LootWindow::Apply()
{
	const uint8* gate = reinterpret_cast<const uint8*>(kLootRemovedTargetGateJz);
	for (size_t i = 0; i < sizeof(kLootRemovedTargetGateJzBytes); ++i)
		if (gate[i] != kLootRemovedTargetGateJzBytes[i])
			return;

	Util::OverwriteBytesAtAddress((void*)kLootRemovedTargetGateJz, 0x90, sizeof(kLootRemovedTargetGateJzBytes));
}

void LootWindow::SetStaysOpenWhileMoving(bool enabled)
{
	const uint8 patched = enabled ? 0xEB : 0x74;
	for (uint32 jz : kLootMoveCloseJz)
		Util::SetByteAtAddress((void*)jz, patched);
}

void LootWindow::OnServerPushedLoot(uint64 guid)
{
	ClientData::WowPlayer player = ClientData::WowPlayer::Active();
	if (!player)
		return;

	player.SetLootTarget(guid, CGGameUI::IsAutoLooting());

	m_pushedGuid = guid;
}

uint64 LootWindow::GetLootTargetGuid() const
{
	return ClientData::WowPlayer::Active().LootTargetGuid();
}

void LootWindow::Tick()
{
	uint64 lootGuid = GetLootTargetGuid();
	if (!lootGuid)
	{
		m_pushedGuid = 0;
		return;
	}

	if (lootGuid == m_pushedGuid)
		return;

	ClientData::WowObject target = ClientData::WowObject::FromGuid(lootGuid);
	if (!target)
		return;

	switch (target.Type())
	{
	case TYPEID_UNIT:
	case TYPEID_PLAYER:
	case TYPEID_GAMEOBJECT:
	case TYPEID_CORPSE:
		break;
	default:
		return;
	}

	ClientData::WowPlayer player = ClientData::WowPlayer::Active();
	if (!player || player.DistanceTo(target) <= kMaxRange)
		return;

	CGGameUI::CloseLoot(1, 1, 0);
}
