#pragma once

#include <Defines.h>

#include <cstdint>

namespace ClientData
{
	struct PlayerFields
	{
		uint32 padding0x00[876];
		float blockPct;
		float dodgePct;
		float parryPct;
		uint32 expertise;
		uint32 offhandExperise;
		float critPct;
		float rangedCritPct;
		float offhandCritPct;
		float spellCritPct[7];
		float shieldBlock;
		float shieldBlockCritPct;
		uint32 exploredzones[128];
		uint32 restedXP;
		uint32 coinage;
		int32 SPPos[7];
		int32 SPNeg[7];
		float SPBonus[7];
		uint32 healingPower;
		float healingTakenMult;
		float healingDoneMult;
		uint32 padding0x0DEC[36];
		int32 crWeaponSkill;
		int32 crDefenseSkill;
		int32 crDodge;
		int32 crParry;
		int32 crBlock;
		int32 crSpeed;     // crHitMelee
		int32 crLifesteal; // crHitRanged
		int32 crAvoidance; // crHitSpell
		int32 crCrit;      // crCritMelee
		int32 crCritRanged;
		int32 crCritSpell;
		int32 crHitTakenMelee;
		int32 crHitTakenRanged;
		int32 crHitTakenSpell;
		int32 crCritTakenMelee;
		int32 crCritTakenRanged;
		int32 crCritTakenSpell;
		int32 crHaste; // crHasteMelee
		int32 crHasteRanged;
		int32 crHasteSpell;
		int32 crWeaponSkillMainhand;
		int32 crWeaponSkillOffhand;
		int32 crWeaponSkillRanged;
		int32 crMastery; // crExpertise
		int32 crThorns;  // crArmorPenetration
		uint32 padding0x1120[56];
		uint32 glyphslots[6];
		float speed;
		float leech;
		float avoidance;
		int32 weaponBonusAP[3];
		uint32 glyphsEnabled; // can reuse, glyphs are disabled
		int32 petSpellPower;
		// TODO: add rest when needed
	};
}
