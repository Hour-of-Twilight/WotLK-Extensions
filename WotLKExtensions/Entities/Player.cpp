#include "Player.h"
#include <CustomLua.h>
#include <CDBCMgr.h>
#include <CDBCDefs/SpellClassMaskExtension.h>
#include "Logger.h"

Player& Player::Instance()
{
	static Player instance;
	return instance;
}

void Player::ApplyPatches()
{
	if (characterCreationRaceFix)
		CharacterCreationRaceCrashfix();

	if (comboPointFix)
		Util::SetByteAtAddress((void*)0x611707, 0xEB);
}

void Player::CharacterCreationRaceCrashfix()
{
	std::vector<uint32> patchedAddresses = { 0x4E157D, 0x4E16A3, 0x4E15B5, 0x4E20EE, 0x4E222A, 0x4E2127, 0x4E1E94, 0x4E1C3A };

	for (uint8 i = 0; i < patchedAddresses.size(); i++)
		Util::OverwriteUInt32AtAddress(patchedAddresses[i], (uint32)&memoryTable);

	Util::OverwriteUInt32AtAddress(0x4CDA43, (uint32)&raceNameTable);

	// copy existing pointer table from wow.exe and fill the remaining slots with pointer to dummy
	memcpy(&raceNameTable, (const void*)0xB24180, 0x30);

	for (uint8 i = 22; i < 32; i++)
		raceNameTable[i] = (uint32)&dummy;

	// I have a hunch this one is needed too
	Util::SetByteAtAddress((void*)0x4E0F86, 0x40);
}

void Player::ResetVariables()
{
	for (uint8 i = 0; i < MAX_SPELL_SCHOOL; i++)
		m_customSpellPen[i] = 0;
	m_magicFind = 0;
	m_healthLeech = 0.0f;
	m_manaLeech = 0.0f;
	m_critDamageMod = 0.0f;
	m_critHealingMod = 0.0f;
	ClearSpellMods();
}

uint32 Player::SpellModKey(uint8 family, uint8 op, uint8 bit)
{
	return (uint32(family) << 16) | (uint32(op) << 8) | uint32(bit);
}

void Player::SetSpellMod(uint8 family, SpellModOp op, uint8 bit, bool isPct, int32 value)
{
	SpellModEntry& e = m_spellMods[SpellModKey(family, op, bit)];
	if (isPct)
		e.pct = value;
	else
		e.flat = value;
}

void Player::ClearSpellMods()
{
	m_spellMods.clear();
}

void Player::AccumulateBlock(uint8 family, const uint32* classMask, SpellModOp op, int32& flat, int32& pct) const
{
	if (family == 0 || family >= C_MAX_SPELL_MOD_FAMILY || op >= MAX_CUSTOM_SPELLMOD)
		return;
	for (uint32 dword = 0; dword < 3; ++dword)
	{
		uint32 word = classMask[dword];
		while (word) // only the set bits
		{
			uint32 bit = 0;
			for (uint32 w = word; !(w & 1u); w >>= 1)
				++bit;
			word &= word - 1;
			auto it = m_spellMods.find(SpellModKey(family, op, (uint8)(dword * 32 + bit)));
			if (it != m_spellMods.end())
			{
				flat += it->second.flat;
				pct += it->second.pct;
			}
		}
	}
}

// True as soon as any set class-mask bit has an entry for `op` (used for boolean ops).
bool Player::BlockHasMod(uint8 family, const uint32* classMask, SpellModOp op) const
{
	if (family == 0 || family >= C_MAX_SPELL_MOD_FAMILY || op >= MAX_CUSTOM_SPELLMOD)
		return false;
	for (uint32 dword = 0; dword < 3; ++dword)
	{
		uint32 word = classMask[dword];
		while (word)
		{
			uint32 bit = 0;
			for (uint32 w = word; !(w & 1u); w >>= 1)
				++bit;
			word &= word - 1;
			if (m_spellMods.count(SpellModKey(family, op, (uint8)(dword * 32 + bit))))
				return true;
		}
	}
	return false;
}

void Player::GetSpellModifiers(SpellRow* spell, SpellModOp op, int32& outFlat, int32& outPct) const
{
	outFlat = 0;
	outPct = 0;
	AccumulateBlock((uint8)spell->m_spellClassSet, spell->m_spellClassMask, op, outFlat, outPct);

	auto* ext = GlobalCDBCMap.getRow<SpellClassMaskExtensionRow>("SpellClassMaskExtension", spell->m_ID);
	if (ext)
		for (uint32 i = 0; i < 3; ++i)
			AccumulateBlock((uint8)ext->entries[i].spellClassSet, ext->entries[i].spellClassMask, op, outFlat, outPct);
}

bool Player::CanCastWhileMoving(SpellRow* spell) const
{
	if (BlockHasMod((uint8)spell->m_spellClassSet, spell->m_spellClassMask, SPELLMOD_CAST_WHILE_MOVING))
		return true;

	auto* ext = GlobalCDBCMap.getRow<SpellClassMaskExtensionRow>("SpellClassMaskExtension", spell->m_ID);
	if (ext)
		for (uint32 i = 0; i < 3; ++i)
			if (BlockHasMod((uint8)ext->entries[i].spellClassSet, ext->entries[i].spellClassMask, SPELLMOD_CAST_WHILE_MOVING))
				return true;

	return false;
}

void Player::RegisterCustomLuaFunctions()
{
	sLua.RegisterFunction("GetSecurityLevel", &GetSecurityLevelFunction, LuaFunctionState::ALL);
}

int Player::GetSecurityLevelFunction(lua_State* L)
{
	FrameScript::PushNumber(L, sPlayer.GetSecurityLevel());
	return 1;
}
