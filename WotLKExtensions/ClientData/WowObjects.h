#pragma once

#include <Defines.h>

#include <ClientData/ClientFunctions.h>
#include <ClientData/Enums.h>
#include <ClientData/GameEnums.h>
#include <ClientData/GameObject.h>
#include <ClientData/MathTypes.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/Spell.h>
#include <ClientData/Units.h>
#include <ClientData/Players.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ClientData
{
	static_assert(ITEM_FIELD_STACK_COUNT == 14, "item descriptor layout mismatch");
	static_assert(CONTAINER_FIELD_NUM_SLOTS == 64, "container descriptor layout mismatch");
	static_assert(PLAYER_BLOCK_PERCENTAGE - UNIT_END == offsetof(PlayerFields, blockPct) / 4, "PlayerFields layout mismatch");

	enum UnitReaction : int
	{
		REACTION_HATED = 0,
		REACTION_HOSTILE = 1,
		REACTION_UNFRIENDLY = 2,
		REACTION_NEUTRAL = 3,
		REACTION_FRIENDLY = 4,
		REACTION_HONORED = 5,
		REACTION_REVERED = 6,
		REACTION_EXALTED = 7,
	};

	class WowUnit;
	class WowPlayer;
	class WowGameObject;
	class WowItem;
	class WowContainer;

	class WowObject
	{
	public:
		WowObject() = default;
		explicit WowObject(void* object) : m_object(static_cast<CGObject_C*>(object)) {}

		static WowObject FromGuid(uint64 guid, ObjectTypeMask typeMask = TYPEMASK_OBJECT)
		{
			return WowObject(guid ? ObjectManager::GetObject(guid, typeMask) : nullptr);
		}

		explicit operator bool() const
		{
			return m_object && m_object->GetDescriptors();
		}
		bool operator==(const WowObject& other) const
		{
			return m_object == other.m_object;
		}
		bool operator!=(const WowObject& other) const
		{
			return m_object != other.m_object;
		}

		CGObject_C* Get() const
		{
			return m_object;
		}
		void* Raw() const
		{
			return m_object;
		}
		uint32* Descriptors() const
		{
			return m_object ? m_object->GetDescriptors() : nullptr;
		}

		template <typename T>
		T Field(uint32 index) const
		{
			uint32* data = Descriptors();
			return data ? *reinterpret_cast<const T*>(&data[index]) : T{};
		}

		template <typename T>
		void SetField(uint32 index, T value) const
		{
			if (uint32* data = Descriptors())
				*reinterpret_cast<T*>(&data[index]) = value;
		}

		uint8 FieldByte(uint32 index, uint32 byte) const
		{
			return static_cast<uint8>((Field<uint32>(index) >> (byte * 8)) & 0xFF);
		}

		bool HasFieldFlag(uint32 index, uint32 flag) const
		{
			return (Field<uint32>(index) & flag) != 0;
		}

		uint64 Guid() const
		{
			return Field<uint64>(OBJECT_FIELD_GUID);
		}
		uint32 Entry() const
		{
			return Field<uint32>(OBJECT_FIELD_ENTRY);
		}
		float ScaleField() const
		{
			return Field<float>(OBJECT_FIELD_SCALE_X);
		}
		uint32 TypeMask() const
		{
			return Field<uint32>(OBJECT_FIELD_TYPE);
		}
		TypeID Type() const
		{
			return m_object ? m_object->GetTypeID() : TYPEID_OBJECT;
		}

		bool IsType(ObjectTypeMask mask) const
		{
			return (TypeMask() & mask) != 0;
		}
		bool IsItem() const
		{
			return IsType(TYPEMASK_ITEM);
		}
		bool IsContainer() const
		{
			return IsType(TYPEMASK_CONTAINER);
		}
		bool IsUnit() const
		{
			return IsType(TYPEMASK_UNIT);
		}
		bool IsPlayer() const
		{
			return IsType(TYPEMASK_PLAYER);
		}
		bool IsGameObject() const
		{
			return IsType(TYPEMASK_GAMEOBJECT);
		}
		bool IsDynamicObject() const
		{
			return IsType(TYPEMASK_DYNAMICOBJECT);
		}
		bool IsCorpse() const
		{
			return IsType(TYPEMASK_CORPSE);
		}
		bool IsActivePlayer() const
		{
			return *this && Guid() == ClntObjMgr::GetActivePlayer();
		}

		C3Vector Position() const
		{
			C3Vector pos{};
			if (m_object)
				m_object->GetPosition(pos);
			return pos;
		}

		C3Vector RawPosition() const
		{
			C3Vector pos{};
			if (m_object)
				m_object->GetRawPosition(pos);
			return pos;
		}

		float Facing() const
		{
			return m_object ? m_object->GetFacing() : 0.0f;
		}
		float Scale() const
		{
			return m_object ? m_object->GetScale() : 0.0f;
		}
		uint64 TransportGuid() const
		{
			return m_object ? m_object->GetTransportGUID() : 0;
		}
		void* Model() const
		{
			return m_object ? m_object->GetObjectModel() : nullptr;
		}
		void* BodyModel() const
		{
			return m_object ? Member<void*>(kBodyModel) : nullptr;
		}
		float RenderScale() const
		{
			return m_object ? Member<float>(kRenderScale) : 0.0f;
		}
		float ObjectHeight() const
		{
			return m_object ? Member<float>(kObjectHeight) : 0.0f;
		}

		const char* Name() const
		{
			const char* name = m_object ? m_object->GetObjectName() : nullptr;
			return name ? name : "";
		}

		float DistanceSqTo(const C3Vector& pos) const
		{
			C3Vector self = Position();
			float dx = self.x - pos.x;
			float dy = self.y - pos.y;
			float dz = self.z - pos.z;
			return dx * dx + dy * dy + dz * dz;
		}

		float DistanceTo(const C3Vector& pos) const
		{
			return std::sqrt(DistanceSqTo(pos));
		}
		float DistanceTo(const WowObject& other) const
		{
			return other ? DistanceTo(other.Position()) : 0.0f;
		}

		WowUnit ToUnit() const;
		WowPlayer ToPlayer() const;
		WowGameObject ToGameObject() const;
		WowItem ToItem() const;
		WowContainer ToContainer() const;

	protected:
		template <typename T>
		T& Member(uint32 offset) const
		{
			return *reinterpret_cast<T*>(reinterpret_cast<uint8*>(m_object) + offset);
		}

		static constexpr uint32 kRenderScale = 0x98;
		static constexpr uint32 kObjectHeight = 0xAC;
		static constexpr uint32 kBodyModel = 0xB4;

		CGObject_C* m_object = nullptr;
	};

	class WowUnit : public WowObject
	{
	public:
		WowUnit() = default;
		explicit WowUnit(void* unit) : WowObject(unit) {}

		static WowUnit FromGuid(uint64 guid)
		{
			return WowUnit(guid ? ObjectManager::GetObject(guid, TYPEMASK_UNIT) : nullptr);
		}

		static WowUnit FromToken(const char* token)
		{
			return WowUnit(token ? ClntObjMgr::GetUnitFromName(const_cast<char*>(token)) : nullptr);
		}

		CGUnit* Unit() const
		{
			return reinterpret_cast<CGUnit*>(m_object);
		}
		UnitFields* Fields() const
		{
			return m_object ? Unit()->unitData : nullptr;
		}
		MovementInfo* Movement() const
		{
			return m_object ? Unit()->movementInfo : nullptr;
		}

		uint32 Health() const
		{
			return Field<uint32>(UNIT_FIELD_HEALTH);
		}
		uint32 MaxHealth() const
		{
			return Field<uint32>(UNIT_FIELD_MAXHEALTH);
		}
		float HealthPct() const
		{
			uint32 max = MaxHealth();
			return max ? 100.0f * Health() / max : 0.0f;
		}

		Powers PowerType() const
		{
			return static_cast<Powers>(FieldByte(UNIT_FIELD_BYTES_0, 3));
		}
		uint32 Power(Powers power) const
		{
			return Field<uint32>(UNIT_FIELD_POWER1 + power);
		}
		uint32 MaxPower(Powers power) const
		{
			return Field<uint32>(UNIT_FIELD_MAXPOWER1 + power);
		}
		uint32 Power() const
		{
			return Power(PowerType());
		}
		uint32 MaxPower() const
		{
			return MaxPower(PowerType());
		}
		int PredictedPower(Powers power) const
		{
			return *this ? CGUnit_C::GetPredictedPower(Unit(), power) : 0;
		}
		uint32 BaseHealth() const
		{
			return Field<uint32>(UNIT_FIELD_BASE_HEALTH);
		}
		uint32 BaseMana() const
		{
			return Field<uint32>(UNIT_FIELD_BASE_MANA);
		}

		bool IsDead() const
		{
			return *this && Health() == 0;
		}
		bool IsAlive() const
		{
			return *this && Health() > 0;
		}

		uint32 Level() const
		{
			return Field<uint32>(UNIT_FIELD_LEVEL);
		}
		uint32 FactionTemplate() const
		{
			return Field<uint32>(UNIT_FIELD_FACTIONTEMPLATE);
		}
		uint8 Race() const
		{
			return FieldByte(UNIT_FIELD_BYTES_0, 0);
		}
		uint8 Class() const
		{
			return FieldByte(UNIT_FIELD_BYTES_0, 1);
		}
		uint8 Gender() const
		{
			return FieldByte(UNIT_FIELD_BYTES_0, 2);
		}

		uint8 StandState() const
		{
			return FieldByte(UNIT_FIELD_BYTES_1, 0);
		}
		uint8 VisFlags() const
		{
			return FieldByte(UNIT_FIELD_BYTES_1, 2);
		}
		uint8 AnimTier() const
		{
			return FieldByte(UNIT_FIELD_BYTES_1, 3);
		}
		uint8 SheathState() const
		{
			return FieldByte(UNIT_FIELD_BYTES_2, 0);
		}
		uint8 PvpFlags() const
		{
			return FieldByte(UNIT_FIELD_BYTES_2, 1);
		}
		uint8 PetFlags() const
		{
			return FieldByte(UNIT_FIELD_BYTES_2, 2);
		}
		uint8 ShapeshiftFormField() const
		{
			return FieldByte(UNIT_FIELD_BYTES_2, 3);
		}

		uint32 ShapeshiftForm() const
		{
			return *this ? CGUnit_C::GetShapeshiftFormId(Unit()) : 0;
		}
		bool IsShapeshifted() const
		{
			return *this && CGUnit_C__IsShapeShifted(Unit()) != 0;
		}

		uint32 UnitFlags() const
		{
			return Field<uint32>(UNIT_FIELD_FLAGS);
		}
		uint32 UnitFlags2() const
		{
			return Field<uint32>(UNIT_FIELD_FLAGS_2);
		}
		uint32 DynamicFlags() const
		{
			return Field<uint32>(UNIT_DYNAMIC_FLAGS);
		}
		uint32 NpcFlags() const
		{
			return Field<uint32>(UNIT_NPC_FLAGS);
		}
		uint32 AuraState() const
		{
			return Field<uint32>(UNIT_FIELD_AURASTATE);
		}
		bool HasUnitFlag(uint32 flag) const
		{
			return HasFieldFlag(UNIT_FIELD_FLAGS, flag);
		}
		bool HasUnitFlag2(uint32 flag) const
		{
			return HasFieldFlag(UNIT_FIELD_FLAGS_2, flag);
		}
		bool HasDynamicFlag(uint32 flag) const
		{
			return HasFieldFlag(UNIT_DYNAMIC_FLAGS, flag);
		}
		bool HasNpcFlag(uint32 flag) const
		{
			return HasFieldFlag(UNIT_NPC_FLAGS, flag);
		}

		uint32 DisplayId() const
		{
			return Field<uint32>(UNIT_FIELD_DISPLAYID);
		}
		uint32 NativeDisplayId() const
		{
			return Field<uint32>(UNIT_FIELD_NATIVEDISPLAYID);
		}
		uint32 MountDisplayId() const
		{
			return Field<uint32>(UNIT_FIELD_MOUNTDISPLAYID);
		}
		bool IsMounted() const
		{
			return MountDisplayId() != 0;
		}
		uint32 VirtualItem(uint32 slot) const
		{
			return slot < 3 ? Field<uint32>(UNIT_VIRTUAL_ITEM_SLOT_ID + slot) : 0;
		}

		float BoundingRadius() const
		{
			return Field<float>(UNIT_FIELD_BOUNDINGRADIUS);
		}
		float CombatReach() const
		{
			return Field<float>(UNIT_FIELD_COMBATREACH);
		}
		float HoverHeight() const
		{
			return Field<float>(UNIT_FIELD_HOVERHEIGHT);
		}
		float ModCastSpeed() const
		{
			return Field<float>(UNIT_MOD_CAST_SPEED);
		}

		uint64 TargetGuid() const
		{
			return Field<uint64>(UNIT_FIELD_TARGET);
		}
		uint64 CharmGuid() const
		{
			return Field<uint64>(UNIT_FIELD_CHARM);
		}
		uint64 SummonGuid() const
		{
			return Field<uint64>(UNIT_FIELD_SUMMON);
		}
		uint64 CritterGuid() const
		{
			return Field<uint64>(UNIT_FIELD_CRITTER);
		}
		uint64 CharmedByGuid() const
		{
			return Field<uint64>(UNIT_FIELD_CHARMEDBY);
		}
		uint64 SummonedByGuid() const
		{
			return Field<uint64>(UNIT_FIELD_SUMMONEDBY);
		}
		uint64 CreatedByGuid() const
		{
			return Field<uint64>(UNIT_FIELD_CREATEDBY);
		}
		uint64 ChannelObjectGuid() const
		{
			return Field<uint64>(UNIT_FIELD_CHANNEL_OBJECT);
		}
		uint64 OwnerGuid() const
		{
			uint64 owner = SummonedByGuid();
			return owner ? owner : CharmedByGuid();
		}
		WowUnit Target() const
		{
			return FromGuid(TargetGuid());
		}
		WowUnit Owner() const
		{
			return FromGuid(OwnerGuid());
		}
		WowUnit Pet() const
		{
			return FromGuid(SummonGuid());
		}

		uint32 ChannelSpellField() const
		{
			return Field<uint32>(UNIT_CHANNEL_SPELL);
		}
		uint32 CreatedBySpell() const
		{
			return Field<uint32>(UNIT_CREATED_BY_SPELL);
		}
		uint32 EmoteState() const
		{
			return Field<uint32>(UNIT_NPC_EMOTESTATE);
		}
		uint32 PetNumber() const
		{
			return Field<uint32>(UNIT_FIELD_PETNUMBER);
		}

		int32 Stat(uint32 stat) const
		{
			return stat < 5 ? Field<int32>(UNIT_FIELD_STAT0 + stat) : 0;
		}
		int32 Resistance(uint32 school) const
		{
			return school < 7 ? Field<int32>(UNIT_FIELD_RESISTANCES + school) : 0;
		}
		uint32 Armor() const
		{
			return Field<uint32>(UNIT_FIELD_RESISTANCES);
		}
		uint32 AttackPower() const
		{
			return Field<uint32>(UNIT_FIELD_ATTACK_POWER);
		}
		uint32 RangedAttackPower() const
		{
			return Field<uint32>(UNIT_FIELD_RANGED_ATTACK_POWER);
		}
		float MinDamage() const
		{
			return Field<float>(UNIT_FIELD_MINDAMAGE);
		}
		float MaxDamage() const
		{
			return Field<float>(UNIT_FIELD_MAXDAMAGE);
		}
		float MinOffhandDamage() const
		{
			return Field<float>(UNIT_FIELD_MINOFFHANDDAMAGE);
		}
		float MaxOffhandDamage() const
		{
			return Field<float>(UNIT_FIELD_MAXOFFHANDDAMAGE);
		}
		float MinRangedDamage() const
		{
			return Field<float>(UNIT_FIELD_MINRANGEDDAMAGE);
		}
		float MaxRangedDamage() const
		{
			return Field<float>(UNIT_FIELD_MAXRANGEDDAMAGE);
		}
		uint32 BaseAttackTime(uint32 attack) const
		{
			return attack < 3 ? Field<uint32>(UNIT_FIELD_BASEATTACKTIME + attack) : 0;
		}

		uint32 MovementFlags() const
		{
			MovementInfo* movement = Movement();
			return movement ? movement->movementFlags : 0;
		}
		bool HasMovementFlag(uint32 flag) const
		{
			return (MovementFlags() & flag) != 0;
		}
		bool IsMoving() const
		{
			return HasMovementFlag(MOVEMENTFLAG_FORWARD | MOVEMENTFLAG_BACKWARD | MOVEMENTFLAG_STRAFE_LEFT | MOVEMENTFLAG_STRAFE_RIGHT | MOVEMENTFLAG_FALLING);
		}
		bool IsSwimming() const
		{
			return HasMovementFlag(MOVEMENTFLAG_SWIMMING);
		}
		bool IsFalling() const
		{
			return HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR);
		}

		uint32 CastingSpellId() const
		{
			return m_object ? Unit()->currentCastId : 0;
		}
		uint32 ChannelingSpellId() const
		{
			return m_object ? Unit()->currentChannelId : 0;
		}
		uint32 CastEndTime() const
		{
			return m_object ? Member<uint32>(kCastEndTime) : 0;
		}
		uint32 ChannelEndTime() const
		{
			return m_object ? Member<uint32>(kChannelEndTime) : 0;
		}
		bool IsCasting() const
		{
			return CastingSpellId() && static_cast<int32>(static_cast<uint32>(OsGetAsyncTimeMs()) - CastEndTime()) < 0;
		}
		bool IsChanneling() const
		{
			return ChannelingSpellId() && static_cast<int32>(static_cast<uint32>(OsGetAsyncTimeMs()) - ChannelEndTime()) < 0;
		}

		int AuraCount() const
		{
			return *this ? CGUnit_C::GetAuraCount(Unit()) : 0;
		}
		AuraData* Aura(uint32 index) const
		{
			return *this ? CGUnit_C::GetAura(Unit(), index) : nullptr;
		}
		uint8 AuraFlags(uint32 index) const
		{
			return *this ? CGUnit_C::GetAuraFlags(Unit(), index) : 0;
		}
		bool HasAura(uint32 spellId) const
		{
			return *this && CGUnit_C::HasAuraBySpellId(Unit(), spellId);
		}
		bool HasAuraMatchingSpellClass(uint32 auraType, SpellRow* spell) const
		{
			return *this && CGUnit_C::HasAuraMatchingSpellClass(Unit(), auraType, spell);
		}

		AuraData* FindAura(uint32 spellId, uint64 casterGuid = 0) const
		{
			int count = AuraCount();
			for (int i = 0; i < count; ++i)
			{
				AuraData* aura = Aura(i);
				if (aura && aura->spellId == spellId && (!casterGuid || aura->creator == casterGuid))
					return aura;
			}
			return nullptr;
		}

		template <typename Fn>
		void ForEachAura(Fn&& fn) const
		{
			int count = AuraCount();
			for (int i = 0; i < count; ++i)
				if (AuraData* aura = Aura(i); aura && aura->spellId)
					fn(*aura, i);
		}

		void* CreatureCache() const
		{
			return m_object ? Member<void*>(kCreatureCache) : nullptr;
		}
		void* DisplayInfoExtra() const
		{
			return m_object ? Member<void*>(kDisplayInfoExtra) : nullptr;
		}
		uint32 DisplayInfoExtraId() const
		{
			const uint32* extra = static_cast<const uint32*>(DisplayInfoExtra());
			return extra ? *extra : 0;
		}
		void* MountModel() const
		{
			return m_object ? Member<void*>(kMountModel) : nullptr;
		}
		int32& CachedMountDisplayId() const
		{
			return Member<int32>(kCachedMountDisplayId);
		}
		void* CharComponent() const
		{
			return m_object ? Member<void*>(kCharComponent) : nullptr;
		}
		void*& CharComponentSlot() const
		{
			return Member<void*>(kCharComponent);
		}
		void* NamePlate() const
		{
			return m_object ? Member<void*>(kNamePlate) : nullptr;
		}
		uint32& AnimFlags() const
		{
			return Member<uint32>(kAnimFlags);
		}
		uint32 ModelAnimTier() const
		{
			return m_object ? Member<uint32>(kModelAnimTier) : 0;
		}
		int32 SpecialBoneSequenceSlot() const
		{
			return m_object ? Member<int32>(kSpecialBoneSequenceSlot) : -1;
		}
		uint32& PreviousSheatheState() const
		{
			return Member<uint32>(kPreviousSheatheState);
		}
		uint32& CurrentSheatheState() const
		{
			return Member<uint32>(kCurrentSheatheState);
		}
		float WalkSpeed() const
		{
			MovementInfo* movement = Movement();
			return movement ? movement->walkSpeed : 0.0f;
		}
		float JumpVelocity() const
		{
			MovementInfo* movement = Movement();
			return movement ? movement->jumpVelocity : 0.0f;
		}

		bool IsSpellKnown(uint32 spellId) const
		{
			return *this && CGUnit_C::IsSpellKnown(Unit(), spellId);
		}
		bool IsBoss() const
		{
			return *this && CGUnit_C::IsBossMob(m_object);
		}
		bool IsTrivial() const
		{
			return *this && CGPlayer_C::UnitIsTrivial(m_object);
		}
		int Reaction(const WowUnit& other) const
		{
			return *this && other ? CGUnit_C::UnitReaction(m_object, other.m_object) : REACTION_NEUTRAL;
		}
		bool IsInMeleeRange(const WowUnit& target) const
		{
			return *this && target && CGUnit_C::IsInMeleeRange(Unit(), target.Unit()) != 0;
		}

	private:
		static constexpr uint32 kCreatureCache = 0x964;
		static constexpr uint32 kDisplayInfoExtra = 0x96C;
		static constexpr uint32 kMountModel = 0x98C;
		static constexpr uint32 kCachedMountDisplayId = 0x9C0;
		static constexpr uint32 kAnimFlags = 0xA38;
		static constexpr uint32 kCastEndTime = 0xA7C;
		static constexpr uint32 kChannelEndTime = 0xA88;
		static constexpr uint32 kCharComponent = 0xB4C;
		static constexpr uint32 kPreviousSheatheState = 0xB58;
		static constexpr uint32 kCurrentSheatheState = 0xB5C;
		static constexpr uint32 kModelAnimTier = 0xB80;
		static constexpr uint32 kSpecialBoneSequenceSlot = 0xB84;
		static constexpr uint32 kNamePlate = 0xC38;
	};

	class WowPlayer : public WowUnit
	{
	public:
		WowPlayer() = default;
		explicit WowPlayer(void* player) : WowUnit(player) {}

		static WowPlayer Active()
		{
			return WowPlayer(ObjectManager::GetActivePlayerObject());
		}

		static WowPlayer FromGuid(uint64 guid)
		{
			return WowPlayer(guid ? ObjectManager::GetObject(guid, TYPEMASK_PLAYER) : nullptr);
		}

		CGPlayer* Player() const
		{
			return reinterpret_cast<CGPlayer*>(m_object);
		}
		PlayerFields* PlayerData() const
		{
			return m_object ? Player()->PlayerData : nullptr;
		}

		uint32 PlayerFlags() const
		{
			return Field<uint32>(PLAYER_FLAGS);
		}
		bool HasPlayerFlag(uint32 flag) const
		{
			return HasFieldFlag(PLAYER_FLAGS, flag);
		}
		bool IsGhost() const
		{
			return HasPlayerFlag(PLAYER_FLAGS_GHOST);
		}
		bool IsDeadOrGhost() const
		{
			return *this && CGPlayer_C::IsDeadOrGhost(Player());
		}

		uint32 GuildId() const
		{
			return Field<uint32>(PLAYER_GUILDID);
		}
		uint32 GuildRank() const
		{
			return Field<uint32>(PLAYER_GUILDRANK);
		}
		bool IsInGuild() const
		{
			return GuildId() != 0;
		}

		uint8 Skin() const
		{
			return FieldByte(PLAYER_BYTES, 0);
		}
		uint8 Face() const
		{
			return FieldByte(PLAYER_BYTES, 1);
		}
		uint8 HairStyle() const
		{
			return FieldByte(PLAYER_BYTES, 2);
		}
		uint8 HairColor() const
		{
			return FieldByte(PLAYER_BYTES, 3);
		}
		uint8 FacialHair() const
		{
			return FieldByte(PLAYER_BYTES_2, 0);
		}
		uint8 RestState() const
		{
			return FieldByte(PLAYER_BYTES_2, 3);
		}
		uint8 PlayerGender() const
		{
			return FieldByte(PLAYER_BYTES_3, 0);
		}
		uint8 Inebriation() const
		{
			return FieldByte(PLAYER_BYTES_3, 1);
		}

		uint32 XP() const
		{
			return Field<uint32>(PLAYER_XP);
		}
		uint32 NextLevelXP() const
		{
			return Field<uint32>(PLAYER_NEXT_LEVEL_XP);
		}
		uint32 RestedXP() const
		{
			return Field<uint32>(PLAYER_REST_STATE_EXPERIENCE);
		}
		uint32 MaxLevel() const
		{
			return Field<uint32>(PLAYER_FIELD_MAX_LEVEL);
		}
		uint32 Coinage() const
		{
			return Field<uint32>(PLAYER_FIELD_COINAGE);
		}
		uint32 HonorCurrency() const
		{
			return Field<uint32>(PLAYER_FIELD_HONOR_CURRENCY);
		}
		uint32 ArenaCurrency() const
		{
			return Field<uint32>(PLAYER_FIELD_ARENA_CURRENCY);
		}
		uint32 FreeTalentPoints() const
		{
			return Field<uint32>(PLAYER_CHARACTER_POINTS1);
		}
		uint32 ChosenTitle() const
		{
			return Field<uint32>(PLAYER_CHOSEN_TITLE);
		}
		uint32 AmmoId() const
		{
			return Field<uint32>(PLAYER_AMMO_ID);
		}
		uint64 FarsightGuid() const
		{
			return Field<uint64>(PLAYER_FARSIGHT);
		}
		uint64 DuelArbiterGuid() const
		{
			return Field<uint64>(PLAYER_DUEL_ARBITER);
		}

		uint32 VisibleItemEntry(uint32 slot) const
		{
			return slot < 19 ? Field<uint32>(PLAYER_VISIBLE_ITEM_1_ENTRYID + slot * 2) : 0;
		}
		uint32 VisibleItemEnchant(uint32 slot) const
		{
			return slot < 19 ? Field<uint32>(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + slot * 2) : 0;
		}
		uint32* VisibleItemEntrySlot(uint32 slot) const
		{
			uint32* data = Descriptors();
			return data && slot < 19 ? &data[PLAYER_VISIBLE_ITEM_1_ENTRYID + slot * 2] : nullptr;
		}
		uint64 InventorySlotGuid(uint32 slot) const
		{
			return slot < 23 ? Field<uint64>(PLAYER_FIELD_INV_SLOT_HEAD + slot * 2) : 0;
		}
		WowItem InventoryItem(uint32 slot) const;

		float BlockPct() const
		{
			return Field<float>(PLAYER_BLOCK_PERCENTAGE);
		}
		float DodgePct() const
		{
			return Field<float>(PLAYER_DODGE_PERCENTAGE);
		}
		float ParryPct() const
		{
			return Field<float>(PLAYER_PARRY_PERCENTAGE);
		}
		float CritPct() const
		{
			return Field<float>(PLAYER_CRIT_PERCENTAGE);
		}
		float RangedCritPct() const
		{
			return Field<float>(PLAYER_RANGED_CRIT_PERCENTAGE);
		}
		float OffhandCritPct() const
		{
			return Field<float>(PLAYER_OFFHAND_CRIT_PERCENTAGE);
		}
		float SpellCritPct(uint32 school) const
		{
			return school < 7 ? Field<float>(PLAYER_SPELL_CRIT_PERCENTAGE1 + school) : 0.0f;
		}
		uint32 Expertise() const
		{
			return Field<uint32>(PLAYER_EXPERTISE);
		}
		uint32 OffhandExpertise() const
		{
			return Field<uint32>(PLAYER_OFFHAND_EXPERTISE);
		}
		int32 SpellDamageBonus(uint32 school) const
		{
			return school < 7 ? Field<int32>(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + school) : 0;
		}
		uint32 HealingPower() const
		{
			return Field<uint32>(PLAYER_FIELD_MOD_HEALING_DONE_POS);
		}
		int32 CombatRating(uint32 rating) const
		{
			return rating < 25 ? Field<int32>(PLAYER_FIELD_COMBAT_RATING_1 + rating) : 0;
		}

		uint64 LootTargetGuid() const
		{
			return m_object ? Member<uint64>(kLootTargetGuid) : 0;
		}
		bool AutoLootPending() const
		{
			return m_object && Member<uint8>(kAutoLootPending) != 0;
		}
		void SetLootTarget(uint64 guid, bool autoLoot) const
		{
			if (!m_object)
				return;

			Member<uint64>(kLootTargetGuid) = guid;
			Member<uint8>(kAutoLootPending) = autoLoot ? 1 : 0;
		}
		void* Backpack() const
		{
			return m_object ? reinterpret_cast<uint8*>(m_object) + kBackpack : nullptr;
		}

		bool HasSpell(uint32 spellId) const
		{
			return IsActivePlayer() && CGPlayer_C::HasSpell(spellId);
		}
		void RefreshVisibleItems() const
		{
			if (*this)
				CGPlayer_C::RefreshVisibleItems(Player());
		}

	private:
		static constexpr uint32 kLootTargetGuid = 0x18E0;
		static constexpr uint32 kAutoLootPending = 0x18E8;
		static constexpr uint32 kBackpack = 0x18F0;
	};

	class WowGameObject : public WowObject
	{
	public:
		WowGameObject() = default;
		explicit WowGameObject(void* gameObject) : WowObject(gameObject) {}

		static WowGameObject FromGuid(uint64 guid)
		{
			return WowGameObject(guid ? ObjectManager::GetObject(guid, TYPEMASK_GAMEOBJECT) : nullptr);
		}

		CGGameObject_C* GameObject() const
		{
			return reinterpret_cast<CGGameObject_C*>(m_object);
		}
		CPassenger* Passenger() const
		{
			return m_object ? &GameObject()->m_passenger : nullptr;
		}

		uint64 CreatedByGuid() const
		{
			return Field<uint64>(OBJECT_FIELD_CREATED_BY);
		}
		uint32 DisplayId() const
		{
			return Field<uint32>(GAMEOBJECT_DISPLAYID);
		}
		uint32 Flags() const
		{
			return Field<uint32>(GAMEOBJECT_FLAGS);
		}
		bool HasFlag(uint32 flag) const
		{
			return HasFieldFlag(GAMEOBJECT_FLAGS, flag);
		}
		uint32 Faction() const
		{
			return Field<uint32>(GAMEOBJECT_FACTION);
		}
		uint32 Level() const
		{
			return Field<uint32>(GAMEOBJECT_LEVEL);
		}
		uint32 Dynamic() const
		{
			return Field<uint32>(GAMEOBJECT_DYNAMIC);
		}
		uint8 State() const
		{
			return FieldByte(GAMEOBJECT_BYTES_1, 0);
		}
		GameobjectTypes GoType() const
		{
			return static_cast<GameobjectTypes>(FieldByte(GAMEOBJECT_BYTES_1, 1));
		}
		uint8 ArtKit() const
		{
			return FieldByte(GAMEOBJECT_BYTES_1, 2);
		}
		uint8 AnimProgress() const
		{
			return FieldByte(GAMEOBJECT_BYTES_1, 3);
		}
		bool IsTransport() const
		{
			return m_object && m_object->IsTransport();
		}
	};

	class WowItem : public WowObject
	{
	public:
		WowItem() = default;
		explicit WowItem(void* item) : WowObject(item) {}

		static WowItem FromGuid(uint64 guid)
		{
			return WowItem(guid ? ObjectManager::GetObject(guid, TYPEMASK_ITEM) : nullptr);
		}

		static WowItem FromBagSlot(int luaBag, int luaSlot);

		uint64 OwnerGuid() const
		{
			return Field<uint64>(ITEM_FIELD_OWNER);
		}
		uint64 ContainedGuid() const
		{
			return Field<uint64>(ITEM_FIELD_CONTAINED);
		}
		uint64 CreatorGuid() const
		{
			return Field<uint64>(ITEM_FIELD_CREATOR);
		}
		uint64 GiftCreatorGuid() const
		{
			return Field<uint64>(ITEM_FIELD_GIFTCREATOR);
		}
		uint32 StackCount() const
		{
			return Field<uint32>(ITEM_FIELD_STACK_COUNT);
		}
		uint32 Duration() const
		{
			return Field<uint32>(ITEM_FIELD_DURATION);
		}
		int32 SpellCharges(uint32 index) const
		{
			return index < 5 ? Field<int32>(ITEM_FIELD_SPELL_CHARGES + index) : 0;
		}
		uint32 Flags() const
		{
			return Field<uint32>(ITEM_FIELD_FLAGS);
		}
		bool HasFlag(uint32 flag) const
		{
			return HasFieldFlag(ITEM_FIELD_FLAGS, flag);
		}
		uint32 EnchantmentId(uint32 slot) const
		{
			return slot < 12 ? Field<uint32>(ITEM_FIELD_ENCHANTMENT_1_1 + slot * 3) : 0;
		}
		uint32 EnchantmentDuration(uint32 slot) const
		{
			return slot < 12 ? Field<uint32>(ITEM_FIELD_ENCHANTMENT_1_1 + slot * 3 + 1) : 0;
		}
		uint32 EnchantmentCharges(uint32 slot) const
		{
			return slot < 12 ? Field<uint32>(ITEM_FIELD_ENCHANTMENT_1_1 + slot * 3 + 2) : 0;
		}
		uint32 PropertySeed() const
		{
			return Field<uint32>(ITEM_FIELD_PROPERTY_SEED);
		}
		int32 RandomPropertiesId() const
		{
			return Field<int32>(ITEM_FIELD_RANDOM_PROPERTIES_ID);
		}
		uint32 Durability() const
		{
			return Field<uint32>(ITEM_FIELD_DURABILITY);
		}
		uint32 MaxDurability() const
		{
			return Field<uint32>(ITEM_FIELD_MAXDURABILITY);
		}

		int ClassId() const
		{
			return *this ? CGItem_C::GetClassID(m_object) : -1;
		}
		bool IsLocked() const
		{
			return m_object && (Member<uint32>(kLockFlags) & 1) != 0;
		}
		void Lock() const
		{
			if (*this)
				CGItem_C::Lock(m_object);
		}
		void Unlock() const
		{
			if (*this)
				CGItem_C::Unlock(m_object);
		}

	private:
		static constexpr uint32 kLockFlags = 0x394;
	};

	class WowContainer : public WowItem
	{
	public:
		WowContainer() = default;
		explicit WowContainer(void* container) : WowItem(container) {}

		static WowContainer FromGuid(uint64 guid)
		{
			return WowContainer(guid ? ObjectManager::GetObject(guid, TYPEMASK_CONTAINER) : nullptr);
		}

		uint32 NumSlots() const
		{
			return Field<uint32>(CONTAINER_FIELD_NUM_SLOTS);
		}
		uint64 SlotGuid(uint32 slot) const
		{
			return slot < NumSlots() ? Field<uint64>(CONTAINER_FIELD_SLOT_1 + slot * 2) : 0;
		}

		WowItem ItemAt(uint32 slot) const
		{
			if (!*this || slot >= NumSlots())
				return WowItem();

			void* bag = Bag();
			return WowItem(bag ? CGBag_C::GetItemPointer(bag, static_cast<int>(slot)) : nullptr);
		}

		void* Bag() const
		{
			if (!m_object)
				return nullptr;

			using GetBagFn = void*(__thiscall*)(void* self);
			void** vtable = *reinterpret_cast<void***>(m_object);
			return reinterpret_cast<GetBagFn>(vtable[kBagVTableSlot])(m_object);
		}

	private:
		static constexpr uint32 kBagVTableSlot = 10;
	};

	inline WowUnit WowObject::ToUnit() const
	{
		return WowUnit(IsUnit() ? m_object : nullptr);
	}
	inline WowPlayer WowObject::ToPlayer() const
	{
		return WowPlayer(IsPlayer() ? m_object : nullptr);
	}
	inline WowGameObject WowObject::ToGameObject() const
	{
		return WowGameObject(IsGameObject() ? m_object : nullptr);
	}
	inline WowItem WowObject::ToItem() const
	{
		return WowItem(IsItem() ? m_object : nullptr);
	}
	inline WowContainer WowObject::ToContainer() const
	{
		return WowContainer(IsContainer() ? m_object : nullptr);
	}
	inline WowItem WowPlayer::InventoryItem(uint32 slot) const
	{
		return WowItem::FromGuid(InventorySlotGuid(slot));
	}
}

#include <ClientData/Containers.h>
