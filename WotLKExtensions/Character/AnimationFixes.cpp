#include <Defines.h>
#include <Character/AnimationFixes.h>
#include <Character/HandItemDisplay.h>

#include <ClientDetours.h>
#include <ClientData/SharedDefines.h>
#include <Config/LauncherSettings.h>

#include <algorithm>
#include <cstring>
#include <intrin.h>
#include <vector>
#include <ClientData/Animation.h>
#include <ClientData/Aura.h>
#include <ClientData/Spell.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/WowObjects.h>

using namespace ClientData;

namespace
{
	struct AnimationSequenceState
	{
		int animationId;
		float sequenceTime;
		int frame;
		float speed;
	};

	CLIENT_FUNCTION(CGUnit_C__IsSpellKnown_MonkUnarmed, 0x7260E0, __thiscall, bool, (CGUnit*, uint32))
	CLIENT_FUNCTION(CMovement__CalcCurrentSpeed_ChannelLowerBody, 0x987570, __thiscall, float, (void*, int))
	CLIENT_FUNCTION(CGUnit_C__ResolveCurrentAnimation_ChannelLowerBody, 0x724500, __thiscall, int, (void*, int, char*, void*))
	CLIENT_FUNCTION(CGUnit_C__SetBoneSequence_ChannelLowerBody, 0x735820, __thiscall, void, (void*, uintptr_t, int, int, float, int, float, int, int, int))
	CLIENT_FUNCTION(CGUnit_C__UnsetBoneSequence_ChannelLowerBody, 0x735A60, __thiscall, int, (void*, void*, int, int, int, int))
	CLIENT_FUNCTION(CM2Model__GetBoneSequenceOriginalAnimId_ChannelLowerBody, 0x8267E0, __thiscall, int, (void*, uint32))
	CLIENT_FUNCTION(M2Data__HasSequenceById_ExtendedAnimationIds, 0x825E00, __stdcall, bool, (void*, unsigned int))

	WowUnit AsUnit(uintptr_t unit)
	{
		return WowUnit(reinterpret_cast<void*>(unit));
	}

	uint32 GetUnitCurrentChannelId(uintptr_t unit);
	uint32 GetUnitChannelSpell(uintptr_t unit);
	bool IsUnitChanneling(uintptr_t unit);
	int GetModelBoneSequenceOriginalAnimId(void* model, int boneSeqSlot);
	bool IsAirborneAnimation(int animationId);
	bool IsJumpingUpward(uintptr_t unit);
	bool CM2ModelHasDirectSequence(void* model, unsigned int animationId);
	int ResolveExtendedModelAnimationId(uintptr_t unit, int animationId, void* model);
	int GetMountedAnimationForSituation(uintptr_t unit, int animationId, void* model);

	bool SpellHasAuraType(SpellRow const& row, uint32 auraType)
	{
		for (size_t effectIndex = 0; effectIndex < 3; ++effectIndex)
			if (row.m_effectAura[effectIndex] == auraType)
				return true;

		return false;
	}

	bool s_monkUnarmedSpellsScanned = false;
	std::vector<uint32> s_monkUnarmedSpellIds;

	bool EnsureMonkUnarmedSpellScan()
	{
		if (s_monkUnarmedSpellsScanned)
			return true;

		WoWClientDB* spellDB = reinterpret_cast<WoWClientDB*>(0x00AD49D0);
		if (!spellDB || !spellDB->isLoaded)
			return false;

		SpellRow row{};

		for (int spellId = spellDB->minIndex; spellId <= spellDB->maxIndex; ++spellId)
			if (ClientDB::GetLocalizedRow(spellDB, spellId, &row) && SpellHasAuraType(row, Aura::SPELL_AURA_MONK_ANIMS))
				s_monkUnarmedSpellIds.push_back(row.m_ID);

		std::sort(s_monkUnarmedSpellIds.begin(), s_monkUnarmedSpellIds.end());
		s_monkUnarmedSpellsScanned = true;
		return true;
	}

	bool UnitHasMonkUnarmedAura(CGUnit* unit)
	{
		if (!unit || !EnsureMonkUnarmedSpellScan() || s_monkUnarmedSpellIds.empty())
			return false;

		const int auraCount = CGUnit_C::GetAuraCount(unit);

		for (int auraIndex = 0; auraIndex < auraCount; ++auraIndex)
		{
			AuraData* aura = CGUnit_C::GetAura(unit, auraIndex);
			if (aura && std::binary_search(s_monkUnarmedSpellIds.begin(), s_monkUnarmedSpellIds.end(), aura->spellId))
				return true;
		}

		return false;
	}

	bool UnitKnowsMonkUnarmedSpell(CGUnit* unit)
	{
		if (!unit || !EnsureMonkUnarmedSpellScan())
			return false;

		for (uint32 spellId : s_monkUnarmedSpellIds)
			if (CGUnit_C__IsSpellKnown_MonkUnarmed(unit, spellId))
				return true;

		return false;
	}

	bool UnitIsMonk(CGUnit* unit)
	{
		WowUnit wowUnit(unit);
		return wowUnit && wowUnit.Class() == CLASS_MONK;
	}

	bool UnitUsesMonkUnarmedAnimations(CGUnit* unit)
	{
		return UnitIsMonk(unit) || UnitHasMonkUnarmedAura(unit) || UnitKnowsMonkUnarmedSpell(unit);
	}

	bool IsMonkUnarmedOffhandAttackBehavior(uint32 behaviorId)
	{
		switch (behaviorId)
		{
		case ANIMATION_ATTACK_OFFHAND:
		case ANIMATION_ATTACK_OFFHAND_PIERCE:
		case ANIMATION_ATTACK_UNARMED_OFFHAND:
			return true;
		default:
			return false;
		}
	}

	bool IsMonkUnarmedMainhandAttackBehavior(uint32 behaviorId)
	{
		switch (behaviorId)
		{
		case ANIMATION_ATTACK_1H:
		case ANIMATION_ATTACK_2H:
		case ANIMATION_ATTACK_2H_LOOSE:
		case ANIMATION_SPECIAL_1H:
		case ANIMATION_SPECIAL_2H:
		case ANIMATION_ATTACK_1H_PIERCE:
		case ANIMATION_ATTACK_2H_LOOSE_PIERCE:
		case ANIMATION_SPECIAL_UNARMED:
			return true;
		default:
			return false;
		}
	}

	int GetMonkUnarmedAttackAnimation(int animationId)
	{
		if (animationId < 0)
			return -1;

		if (IsMonkUnarmedOffhandAttackBehavior(static_cast<uint32>(animationId)))
			return ANIMATION_MONK_ATTACK_OFFHAND;

		if (IsMonkUnarmedMainhandAttackBehavior(static_cast<uint32>(animationId)))
			return ANIMATION_MONK_ATTACK_MAINHAND;

		auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
		if (!row)
			return -1;

		if (IsMonkUnarmedOffhandAttackBehavior(row->behaviorId))
			return ANIMATION_MONK_ATTACK_OFFHAND;

		if (IsMonkUnarmedMainhandAttackBehavior(row->behaviorId))
			return ANIMATION_MONK_ATTACK_MAINHAND;

		return -1;
	}

	bool IsChannelCastAnimation(int animationId)
	{
		if (animationId == ANIMATION_CHANNEL_CAST_DIRECTED || animationId == ANIMATION_CHANNEL_CAST_OMNI)
			return true;

		if (animationId < 0)
			return false;

		auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
		return row && (row->behaviorId == ANIMATION_CHANNEL_CAST_DIRECTED || row->behaviorId == ANIMATION_CHANNEL_CAST_OMNI);
	}

	bool IsMovingLowerBody(uintptr_t unit)
	{
		const uint32 movementFlags = AsUnit(unit).MovementFlags();
		return (movementFlags & (MOVEMENTFLAG_MASK_DIRECTIONAL | MOVEMENTFLAG_MASK_FALLING | MOVEMENTFLAG_SWIMMING)) != 0;
	}

	float AnySequenceTime()
	{
		float value;
		uint32 bits = 0xFFFFFFFF;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	int ResolveChannelLowerBodyAnimation(uintptr_t unit, char animationFlags, int resolveFlags, bool allowAirborne)
	{
		const uint32 movementFlags = AsUnit(unit).MovementFlags();
		if ((movementFlags & MOVEMENTFLAG_SWIMMING) != 0)
		{
			if ((movementFlags & MOVEMENTFLAG_BACKWARD) != 0)
				return ANIMATION_SWIM_BACKWARDS;

			return (movementFlags & MOVEMENTFLAG_MASK_DIRECTIONAL) != 0 ? ANIMATION_SWIM : ANIMATION_SWIM_IDLE;
		}

		if (allowAirborne && (movementFlags & MOVEMENTFLAG_MASK_FALLING) != 0)
		{
			void* model = AsUnit(unit).BodyModel();
			const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
			if (rootAnimation == ANIMATION_JUMP_START)
			{
				if (IsJumpingUpward(unit))
					return rootAnimation;
			}
			else if (IsAirborneAnimation(rootAnimation))
			{
				return rootAnimation;
			}

			if (IsJumpingUpward(unit))
				return ANIMATION_JUMP_START;

			char resolvedFlags = animationFlags;
			const int resolvedAnimation = CGUnit_C__ResolveCurrentAnimation_ChannelLowerBody(
			    reinterpret_cast<void*>(unit),
			    resolveFlags | 4,
			    &resolvedFlags,
			    nullptr);

			if (resolvedAnimation != ANIMATION_CURRENT_OR_NONE && !IsChannelCastAnimation(resolvedAnimation))
				return resolvedAnimation;

			return ANIMATION_FALL;
		}

		if ((movementFlags & MOVEMENTFLAG_MASK_DIRECTIONAL) != 0)
		{
			if ((movementFlags & MOVEMENTFLAG_BACKWARD) != 0)
				return ANIMATION_RUN_BACKWARDS;

			const float speed = CMovement__CalcCurrentSpeed_ChannelLowerBody(AsUnit(unit).Movement(), 0);
			if (speed >= 11.0f)
				return ANIMATION_SPRINT;

			const float walkSpeed = AsUnit(unit).WalkSpeed();
			return speed > walkSpeed * 2.0f ? ANIMATION_RUN : ANIMATION_WALK;
		}

		return -1;
	}

	void SetUnitRootSequence(uintptr_t unit, void* model, int animationId)
	{
		CGUnit_C__SetBoneSequence_ChannelLowerBody(
		    reinterpret_cast<void*>(unit),
		    reinterpret_cast<uintptr_t>(model),
		    -1,
		    animationId,
		    AnySequenceTime(),
		    0,
		    1.0f,
		    0,
		    1,
		    0);
	}

	void SetUnitUpperSequence(uintptr_t unit, void* model, int animationId)
	{
		const int upperSlot = AsUnit(unit).SpecialBoneSequenceSlot();
		if (upperSlot == -1)
			return;

		CGUnit_C__SetBoneSequence_ChannelLowerBody(
		    reinterpret_cast<void*>(unit),
		    reinterpret_cast<uintptr_t>(model),
		    upperSlot,
		    animationId,
		    AnySequenceTime(),
		    0,
		    1.0f,
		    0,
		    1,
		    0);
	}

	int GetModelBoneSequenceOriginalAnimId(void* model, int boneSeqSlot)
	{
		if (!model)
			return 0;

		return CM2Model__GetBoneSequenceOriginalAnimId_ChannelLowerBody(model, static_cast<uint32>(boneSeqSlot));
	}

	bool IsAirborneAnimation(int animationId)
	{
		if (animationId == ANIMATION_JUMP_START || animationId == ANIMATION_FALL)
			return true;

		if (animationId < 0)
			return false;

		auto* row = reinterpret_cast<AnimationDataRow*>(ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
		return row && row->behaviorId >= 37 && (row->behaviorId <= 40 || row->behaviorId == 467);
	}

	bool IsJumpingUpward(uintptr_t unit)
	{
		const uint32 movementFlags = AsUnit(unit).MovementFlags();
		if ((movementFlags & MOVEMENTFLAG_MASK_FALLING) == 0)
			return false;

		return AsUnit(unit).JumpVelocity() > 0.0f;
	}

	int GetUnitUpperChannelAnimation(uintptr_t unit, void* model)
	{
		const int upperSlot = AsUnit(unit).SpecialBoneSequenceSlot();
		if (upperSlot == -1)
			return 0;

		const int upperAnimation = GetModelBoneSequenceOriginalAnimId(model, upperSlot);
		return IsChannelCastAnimation(upperAnimation) ? upperAnimation : 0;
	}

	int GetUnitRootChannelAnimation(void* model)
	{
		const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
		return IsChannelCastAnimation(rootAnimation) ? rootAnimation : 0;
	}

	void MoveRootChannelToUpperBody(uintptr_t unit, void* model)
	{
		if (GetUnitUpperChannelAnimation(unit, model) != 0)
			return;

		const int rootChannelAnimation = GetUnitRootChannelAnimation(model);
		if (rootChannelAnimation != 0)
			SetUnitUpperSequence(unit, model, rootChannelAnimation);
	}

	void RestoreRootChannelFromUpperBody(uintptr_t unit, void* model)
	{
		const int upperSlot = AsUnit(unit).SpecialBoneSequenceSlot();
		if (upperSlot == -1)
			return;

		const int upperChannelAnimation = GetUnitUpperChannelAnimation(unit, model);
		if (upperChannelAnimation == 0)
			return;

		if (GetUnitRootChannelAnimation(model) == 0)
			SetUnitRootSequence(unit, model, upperChannelAnimation);

		CGUnit_C__UnsetBoneSequence_ChannelLowerBody(reinterpret_cast<void*>(unit), model, upperSlot, 1, 0, 1);
	}

	void StabilizeChannelLowerBodyAnimation(uintptr_t unit)
	{
		if (!IsUnitChanneling(unit) || !IsMovingLowerBody(unit))
			return;

		const uint32 movementFlags = AsUnit(unit).MovementFlags();
		void* model = AsUnit(unit).BodyModel();
		if (!model)
			return;

		const int rootAnimation = GetModelBoneSequenceOriginalAnimId(model, -1);
		if ((movementFlags & MOVEMENTFLAG_MASK_FALLING) != 0)
		{
			if (!IsAirborneAnimation(rootAnimation))
			{
				MoveRootChannelToUpperBody(unit, model);
				SetUnitRootSequence(unit, model, ANIMATION_JUMP_START);
			}
			return;
		}

		if (!IsAirborneAnimation(rootAnimation))
			return;

		MoveRootChannelToUpperBody(unit, model);

		const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, false);
		if (rootMovementAnimation != -1)
			SetUnitRootSequence(unit, model, rootMovementAnimation);
	}

	bool ShouldRouteMovingChannelToUpperSlot(uintptr_t unit, int animationId)
	{
		return unit && IsMovingLowerBody(unit) && IsUnitChanneling(unit) && IsChannelCastAnimation(animationId) && AsUnit(unit).SpecialBoneSequenceSlot() != -1;
	}

	uint32 GetUnitCurrentChannelId(uintptr_t unit)
	{
		if (!unit)
			return 0;

		return AsUnit(unit).ChannelingSpellId();
	}

	uint32 GetUnitChannelSpell(uintptr_t unit)
	{
		if (!unit)
			return 0;

		return AsUnit(unit).ChannelSpellField();
	}

	bool IsUnitChanneling(uintptr_t unit)
	{
		return GetUnitCurrentChannelId(unit) != 0 || GetUnitChannelSpell(unit) != 0;
	}

	bool CM2ModelHasDirectSequence(void* model, unsigned int animationId)
	{
		if (!model)
			return false;

		const uintptr_t modelAddress = reinterpret_cast<uintptr_t>(model);
		const uintptr_t modelHeader = *reinterpret_cast<uintptr_t*>(modelAddress + 44);
		if (!modelHeader)
			return false;

		void* modelData = *reinterpret_cast<void**>(modelHeader + 336);
		if (!modelData)
			return false;

		return M2Data__HasSequenceById_ExtendedAnimationIds(modelData, animationId);
	}

	int ResolveExtendedModelAnimationId(uintptr_t unit, int animationId, void* model)
	{
		if (animationId <= ANIMATION_CURRENT_OR_NONE)
			return -1;

		if (!model && unit)
			model = AsUnit(unit).BodyModel();

		if (CM2ModelHasDirectSequence(model, static_cast<unsigned int>(animationId)))
			return animationId;

		int currentAnimationId = animationId;
		for (int fallbackDepth = 0; fallbackDepth < 16; ++fallbackDepth)
		{
			auto* row = reinterpret_cast<AnimationDataRow*>(
			    ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), currentAnimationId));
			if (!row || static_cast<int>(row->fallback) == currentAnimationId)
				break;

			currentAnimationId = static_cast<int>(row->fallback);
			if (currentAnimationId >= ANIMATION_CURRENT_OR_NONE && CM2ModelHasDirectSequence(model, static_cast<unsigned int>(currentAnimationId)))
				return currentAnimationId;

			if (currentAnimationId < ANIMATION_CURRENT_OR_NONE)
				return currentAnimationId;
		}

		return -1;
	}

	enum class MountAnimationMode
	{
		None,
		Swim,
		Flight
	};

	enum class MountAnimationMotion
	{
		None,
		Idle,
		Forward,
		Walk,
		Sprint,
		Left,
		Right,
		Backward,
		Count
	};

	struct MountAnimationMotionEntry
	{
		int behaviorId;
		MountAnimationMotion motion;
	};

	constexpr MountAnimationMotionEntry MOUNT_ANIMATION_MOTIONS[] = {
		{ ANIMATION_STAND, MountAnimationMotion::Idle },
		{ ANIMATION_SWIM_IDLE, MountAnimationMotion::Idle },
		{ ANIMATION_HOVER, MountAnimationMotion::Idle },
		{ ANIMATION_RUN, MountAnimationMotion::Forward },
		{ ANIMATION_SWIM, MountAnimationMotion::Forward },
		{ ANIMATION_SWIM_RUN, MountAnimationMotion::Forward },
		{ ANIMATION_FLY, MountAnimationMotion::Forward },
		{ ANIMATION_WALK, MountAnimationMotion::Walk },
		{ ANIMATION_SWIM_WALK, MountAnimationMotion::Walk },
		{ ANIMATION_SPRINT, MountAnimationMotion::Sprint },
		{ ANIMATION_SWIM_SPRINT, MountAnimationMotion::Sprint },
		{ ANIMATION_SHUFFLE_LEFT, MountAnimationMotion::Left },
		{ ANIMATION_SWIM_LEFT, MountAnimationMotion::Left },
		{ ANIMATION_RUN_LEFT, MountAnimationMotion::Left },
		{ ANIMATION_SHUFFLE_RIGHT, MountAnimationMotion::Right },
		{ ANIMATION_SWIM_RIGHT, MountAnimationMotion::Right },
		{ ANIMATION_RUN_RIGHT, MountAnimationMotion::Right },
		{ ANIMATION_RUN_BACKWARDS, MountAnimationMotion::Backward },
		{ ANIMATION_SWIM_BACKWARDS, MountAnimationMotion::Backward },
		{ ANIMATION_SWIM_WALK_BACKWARDS, MountAnimationMotion::Backward },
	};

	constexpr int MOUNT_FLIGHT_IDLE_IDS[] = { ANIMATION_MOUNT_FLIGHT_IDLE, ANIMATION_FLY_STAND, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
	constexpr int MOUNT_FLIGHT_FORWARD_IDS[] = { ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_FLY_FLY, ANIMATION_MOUNT_FLIGHT_WALK };
	constexpr int MOUNT_FLIGHT_WALK_IDS[] = { ANIMATION_MOUNT_FLIGHT_WALK, ANIMATION_FLY_WALK, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN };
	constexpr int MOUNT_FLIGHT_SPRINT_IDS[] = { ANIMATION_MOUNT_FLIGHT_SPRINT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_FLY_FLY };
	constexpr int MOUNT_FLIGHT_LEFT_IDS[] = { ANIMATION_MOUNT_FLIGHT_LEFT, ANIMATION_FLY_RUN_LEFT, ANIMATION_FLY_SHUFFLE_LEFT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
	constexpr int MOUNT_FLIGHT_RIGHT_IDS[] = { ANIMATION_MOUNT_FLIGHT_RIGHT, ANIMATION_FLY_RUN_RIGHT, ANIMATION_FLY_SHUFFLE_RIGHT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
	constexpr int MOUNT_FLIGHT_BACKWARD_IDS[] = { ANIMATION_MOUNT_FLIGHT_BACKWARDS, ANIMATION_MOUNT_FLIGHT_WALK_BACKWARDS, ANIMATION_FLY_WALK_BACKWARDS, ANIMATION_FLY_WALK, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };

	constexpr int MOUNT_SWIM_IDLE_IDS[] = { ANIMATION_MOUNT_SWIM_IDLE };
	constexpr int MOUNT_SWIM_FORWARD_IDS[] = { ANIMATION_MOUNT_SWIM_RUN, ANIMATION_MOUNT_SWIM_WALK };
	constexpr int MOUNT_SWIM_WALK_IDS[] = { ANIMATION_MOUNT_SWIM_WALK, ANIMATION_MOUNT_SWIM_RUN };
	constexpr int MOUNT_SWIM_SPRINT_IDS[] = { ANIMATION_MOUNT_SWIM_SPRINT, ANIMATION_MOUNT_SWIM_RUN };
	constexpr int MOUNT_SWIM_LEFT_IDS[] = { ANIMATION_MOUNT_SWIM_LEFT, ANIMATION_MOUNT_SWIM_RUN };
	constexpr int MOUNT_SWIM_RIGHT_IDS[] = { ANIMATION_MOUNT_SWIM_RIGHT, ANIMATION_MOUNT_SWIM_RUN };
	constexpr int MOUNT_SWIM_BACKWARD_IDS[] = { ANIMATION_MOUNT_SWIM_BACKWARDS, ANIMATION_MOUNT_SWIM_WALK_BACKWARDS, ANIMATION_MOUNT_SWIM_RUN };

	template <typename T, size_t Count>
	constexpr size_t ArrayCount(T const (&)[Count])
	{
		return Count;
	}

	struct MountAnimationCandidates
	{
		const int* flight;
		size_t flightCount;
		const int* swim;
		size_t swimCount;
	};

	constexpr MountAnimationCandidates MOUNT_ANIMATION_CANDIDATES[] = {
		{ nullptr, 0, nullptr, 0 },
		{ MOUNT_FLIGHT_IDLE_IDS, ArrayCount(MOUNT_FLIGHT_IDLE_IDS), MOUNT_SWIM_IDLE_IDS, ArrayCount(MOUNT_SWIM_IDLE_IDS) },
		{ MOUNT_FLIGHT_FORWARD_IDS, ArrayCount(MOUNT_FLIGHT_FORWARD_IDS), MOUNT_SWIM_FORWARD_IDS, ArrayCount(MOUNT_SWIM_FORWARD_IDS) },
		{ MOUNT_FLIGHT_WALK_IDS, ArrayCount(MOUNT_FLIGHT_WALK_IDS), MOUNT_SWIM_WALK_IDS, ArrayCount(MOUNT_SWIM_WALK_IDS) },
		{ MOUNT_FLIGHT_SPRINT_IDS, ArrayCount(MOUNT_FLIGHT_SPRINT_IDS), MOUNT_SWIM_SPRINT_IDS, ArrayCount(MOUNT_SWIM_SPRINT_IDS) },
		{ MOUNT_FLIGHT_LEFT_IDS, ArrayCount(MOUNT_FLIGHT_LEFT_IDS), MOUNT_SWIM_LEFT_IDS, ArrayCount(MOUNT_SWIM_LEFT_IDS) },
		{ MOUNT_FLIGHT_RIGHT_IDS, ArrayCount(MOUNT_FLIGHT_RIGHT_IDS), MOUNT_SWIM_RIGHT_IDS, ArrayCount(MOUNT_SWIM_RIGHT_IDS) },
		{ MOUNT_FLIGHT_BACKWARD_IDS, ArrayCount(MOUNT_FLIGHT_BACKWARD_IDS), MOUNT_SWIM_BACKWARD_IDS, ArrayCount(MOUNT_SWIM_BACKWARD_IDS) },
	};

	static_assert(ArrayCount(MOUNT_ANIMATION_CANDIDATES) == static_cast<size_t>(MountAnimationMotion::Count), "mount motion candidate table out of sync");

	int GetAnimationBehaviorId(int animationId)
	{
		if (animationId < 0)
			return -1;

		auto* row = reinterpret_cast<AnimationDataRow*>(
		    ClientDB::GetRow(reinterpret_cast<void*>(ANIMATION_DATA_DB), animationId));
		return row ? static_cast<int>(row->behaviorId) : animationId;
	}

	bool IsUnitMounted(uintptr_t unit)
	{
		return unit && AsUnit(unit).MountModel() != nullptr;
	}

	MountAnimationMode GetMountAnimationMode(uintptr_t unit)
	{
		if (!IsUnitMounted(unit))
			return MountAnimationMode::None;

		const uint32 movementFlags = AsUnit(unit).MovementFlags();
		const uint32 animTier = AsUnit(unit).ModelAnimTier();

		if ((movementFlags & MOVEMENTFLAG_FLYING) != 0 || animTier == ANIM_TIER_FLY)
			return MountAnimationMode::Flight;

		if ((movementFlags & MOVEMENTFLAG_SWIMMING) != 0 || animTier == ANIM_TIER_SWIM)
			return MountAnimationMode::Swim;

		return MountAnimationMode::None;
	}

	MountAnimationMotion GetMountAnimationMotion(int behaviorId)
	{
		if (behaviorId < 0)
			return MountAnimationMotion::None;

		for (MountAnimationMotionEntry const& entry : MOUNT_ANIMATION_MOTIONS)
			if (entry.behaviorId == behaviorId)
				return entry.motion;

		return MountAnimationMotion::None;
	}

	int GetMountedAnimationForSituation(uintptr_t unit, int animationId, void* model)
	{
		if (animationId < 0 || !model)
			return -1;

		const MountAnimationMode mode = GetMountAnimationMode(unit);
		if (mode == MountAnimationMode::None)
			return -1;

		const MountAnimationMotion motion = GetMountAnimationMotion(GetAnimationBehaviorId(animationId));
		if (motion == MountAnimationMotion::None)
			return -1;

		MountAnimationCandidates const& candidates = MOUNT_ANIMATION_CANDIDATES[static_cast<size_t>(motion)];
		const bool flight = mode == MountAnimationMode::Flight;
		const int* candidateIds = flight ? candidates.flight : candidates.swim;
		const size_t candidateCount = flight ? candidates.flightCount : candidates.swimCount;

		for (size_t index = 0; index < candidateCount; ++index)
			if (CM2ModelHasDirectSequence(model, static_cast<unsigned int>(candidateIds[index])))
				return candidateIds[index];

		return -1;
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__AnimationData_MonkUnarmed, 0x7385C0, void, (int animationId, char flags))
	{
		const int monkUnarmedAnimationId = GetMonkUnarmedAttackAnimation(animationId);
		if (monkUnarmedAnimationId != -1 && UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(self)))
			animationId = monkUnarmedAnimationId;

		CGUnit_C__AnimationData_MonkUnarmed(self, animationId, flags);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__ApplyAnimationSequence_ChannelLowerBody, 0x737EF0, void, (AnimationSequenceState * sequence, int previousAnimationId, int useSpecialBoneSlot, int currentAnimationId, int allowTransition, int primary))
	{
		const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
		if (sequence && !useSpecialBoneSlot && ShouldRouteMovingChannelToUpperSlot(unit, sequence->animationId))
			useSpecialBoneSlot = 1;

		CGUnit_C__ApplyAnimationSequence_ChannelLowerBody(self, sequence, previousAnimationId, useSpecialBoneSlot, currentAnimationId, allowTransition, primary);
	}

	CLIENT_DETOUR_THISCALL(CM2Model__HasPlayableSequenceFallback_ExtendedAnimationIds, 0x826050, bool, (unsigned int animationId))
	{
		if (animationId < ANIMATION_CURRENT_OR_NONE)
			return CM2Model__HasPlayableSequenceFallback_ExtendedAnimationIds(self, animationId);

		return CM2ModelHasDirectSequence(self, animationId);
	}

	CLIENT_DETOUR_THISCALL(CM2Model__FindPlayableSequenceFallbackId_ExtendedAnimationIds, 0x825F40, int, (unsigned int animationId))
	{
		if (animationId < ANIMATION_CURRENT_OR_NONE)
			return CM2Model__FindPlayableSequenceFallbackId_ExtendedAnimationIds(self, animationId);

		return CM2ModelHasDirectSequence(self, animationId)
		           ? static_cast<int>(animationId)
		           : -1;
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__HandleModelSequenceCallback_MonkUnarmed, 0x73BBD0, void, (void* model, int boneSeqSlot, int animationId, int a5, int a6))
	{
		const int monkUnarmedAnimationId = GetMonkUnarmedAttackAnimation(animationId);
		if (monkUnarmedAnimationId != -1 && UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(self)))
			animationId = monkUnarmedAnimationId;

		CGUnit_C__HandleModelSequenceCallback_MonkUnarmed(self, model, boneSeqSlot, animationId, a5, a6);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__ResolveModelAnimationId_MonkUnarmed, 0x7176F0, int, (int animationId, void* model))
	{
		const int monkUnarmedAnimationId = GetMonkUnarmedAttackAnimation(animationId);
		if (monkUnarmedAnimationId != -1 && UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(self)))
			animationId = monkUnarmedAnimationId;

		const uintptr_t unit = reinterpret_cast<uintptr_t>(self);

		void* animationModel = model;
		if (!animationModel)
			animationModel = AsUnit(unit).BodyModel();

		const int mountedAnimationId = GetMountedAnimationForSituation(unit, animationId, animationModel);
		if (mountedAnimationId != -1)
			return mountedAnimationId;

		if (animationId > ANIMATION_CURRENT_OR_NONE)
		{
			const int extendedAnimationId = ResolveExtendedModelAnimationId(unit, animationId, model);
			if (extendedAnimationId != -1)
				return extendedAnimationId;
		}

		return CGUnit_C__ResolveModelAnimationId_MonkUnarmed(self, animationId, model);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed, 0x737BD0, void, (void* model, int boneSeqSlot, unsigned int sequence))
	{
		if (UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(self)))
		{
			const int monkUnarmedAnimationId = GetMonkUnarmedAttackAnimation(sequence);
			if (monkUnarmedAnimationId != -1)
				sequence = monkUnarmedAnimationId;
		}

		CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed(self, model, boneSeqSlot, sequence);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__MoveHandItemAttachment_HandItemDisplay, 0x7310A0, int, (int slot, int moveToSheath))
	{
		return HandItemDisplay::SheatheHandItem(self, slot, moveToSheath);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__SetHandItemBoneSequence_MonkUnarmed, 0x735820, void, (uintptr_t model, int boneSeqSlot, int sequence, float sequenceTime, int a6, float speed, int a8, int a9, int a10))
	{
		const uintptr_t unit = reinterpret_cast<uintptr_t>(self);

		if (boneSeqSlot == -1 && ShouldRouteMovingChannelToUpperSlot(unit, sequence))
		{
			const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, true);
			if (rootMovementAnimation != -1)
				CGUnit_C__SetHandItemBoneSequence_MonkUnarmed(self, model, -1, rootMovementAnimation, AnySequenceTime(), a6, 1.0f, a8, a9, a10);

			boneSeqSlot = AsUnit(unit).SpecialBoneSequenceSlot();
		}

		if (UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(self)))
		{
			const int monkUnarmedAnimationId = GetMonkUnarmedAttackAnimation(sequence);
			if (monkUnarmedAnimationId != -1)
				sequence = monkUnarmedAnimationId;
		}

		CGUnit_C__SetHandItemBoneSequence_MonkUnarmed(self, model, boneSeqSlot, sequence, sequenceTime, a6, speed, a8, a9, a10);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody, 0x73AC30, void, (char flags, int animationId))
	{
		const uintptr_t unit = reinterpret_cast<uintptr_t>(self);

		const bool wasChanneling = IsUnitChanneling(unit);
		void* model = wasChanneling ? AsUnit(unit).BodyModel() : nullptr;
		const int rootChannelBeforeUpdate = model ? GetUnitRootChannelAnimation(model) : 0;

		CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody(self, flags, animationId);

		if (!IsUnitChanneling(unit))
			return;

		model = AsUnit(unit).BodyModel();
		if (!model)
			return;

		if (!IsMovingLowerBody(unit))
		{
			RestoreRootChannelFromUpperBody(unit, model);
			return;
		}

		MoveRootChannelToUpperBody(unit, model);
		if (rootChannelBeforeUpdate != 0 && GetUnitUpperChannelAnimation(unit, model) == 0)
			SetUnitUpperSequence(unit, model, rootChannelBeforeUpdate);

		const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, flags, animationId, true);
		if (rootMovementAnimation != -1)
			SetUnitRootSequence(unit, model, rootMovementAnimation);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__PlayFallLandAnimation_ChannelLowerBody, 0x73D2B0, void, (int movementFlags, int forceLandAnimation))
	{
		CGUnit_C__PlayFallLandAnimation_ChannelLowerBody(self, movementFlags, forceLandAnimation);

		const uintptr_t unit = reinterpret_cast<uintptr_t>(self);
		if (!IsUnitChanneling(unit))
			return;

		void* model = AsUnit(unit).BodyModel();
		if (!model)
			return;

		MoveRootChannelToUpperBody(unit, model);

		const int rootMovementAnimation = ResolveChannelLowerBodyAnimation(unit, 0, 4, false);
		if (rootMovementAnimation != -1)
			SetUnitRootSequence(unit, model, rootMovementAnimation);
	}

	CLIENT_DETOUR_THISCALL(CGUnit_C__PreAnimate_ChannelLowerBody, 0x73DAB0, int, (int elapsed))
	{
		const int result = CGUnit_C__PreAnimate_ChannelLowerBody(self, elapsed);
		StabilizeChannelLowerBodyAnimation(reinterpret_cast<uintptr_t>(self));
		return result;
	}

}

namespace
{
	void AnimationLayeringFixes()
	{
		(void)CGUnit_C__ApplyAnimationSequence_ChannelLowerBody__Result;
		(void)CGUnit_C__UpdateCurrentAnimation_ChannelLowerBody__Result;
		(void)CGUnit_C__PlayFallLandAnimation_ChannelLowerBody__Result;
		(void)CGUnit_C__PreAnimate_ChannelLowerBody__Result;
	}

	void ExtendedAnimationIdFixes()
	{
		(void)CM2Model__HasPlayableSequenceFallback_ExtendedAnimationIds__Result;
		(void)CM2Model__FindPlayableSequenceFallbackId_ExtendedAnimationIds__Result;
	}

	void MonkUnarmedAnimationFixes()
	{
		(void)CGUnit_C__AnimationData_MonkUnarmed__Result;
		(void)CGUnit_C__HandleModelSequenceCallback_MonkUnarmed__Result;
		(void)CGUnit_C__ApplyModelAnimationSequence_MonkUnarmed__Result;
		(void)CGUnit_C__MoveHandItemAttachment_HandItemDisplay__Result;
		(void)CGUnit_C__ResolveModelAnimationId_MonkUnarmed__Result;
		(void)CGUnit_C__SetHandItemBoneSequence_MonkUnarmed__Result;
	}
}

bool AnimationFixes::ShouldUseUnarmedAnimations(void* unit)
{
	return UnitUsesMonkUnarmedAnimations(reinterpret_cast<CGUnit*>(unit));
}

void AnimationFixes::InvalidateSpellScan()
{
	s_monkUnarmedSpellsScanned = false;
	s_monkUnarmedSpellIds.clear();
}

void AnimationFixes::Apply()
{
	AnimationLayeringFixes();
	ExtendedAnimationIdFixes();
	MonkUnarmedAnimationFixes();
}
