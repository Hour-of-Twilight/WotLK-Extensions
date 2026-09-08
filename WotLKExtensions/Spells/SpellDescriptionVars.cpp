#include "SpellDescriptionVars.h"

#include <Packets/AuraValuesCache.h>
#include <Packets/UnitLevelCache.h>
#include <Util.h>

#include <cstring>

// CFormula::CompileElement, the linear scan that turns a ${} name into an opcode
static constexpr uint32_t kNameTableOperand = 0x00576B63; // disp32 of "mov eax, table[esi*4]"
static constexpr uint32_t kNameCountOperand = 0x00576B7C; // imm32 of "cmp esi, 140"

// CFormula::GetVariableValue, the opcode to value switch
static constexpr uint32_t kGetVariableValue = 0x005782D0;
static constexpr size_t kPrologueLength = 6; // push ebp / mov ebp, esp / sub esp, 34h

static constexpr uint32_t kFirstOpcode = 22;
static constexpr size_t kMaxNames = 256 - kFirstOpcode;

// Both live in our data section so the client can read them once the operands are patched. The
// name table has to stay put for the lifetime of the process, which is why it is a fixed array
// rather than the vector that owns the strings.
static const char* sNames[kMaxNames] = {};
static SpellDescriptionVars::ValueFn sHandlers[256] = {};

// Called from the naked stub with a pointer to the client's own argument block. Returns false for
// anything we have not claimed so the client's switch keeps handling it.
static bool __cdecl DispatchFormulaVariable(FormulaContext* ctx)
{
	if (!ctx || ctx->opcode > 255 || !ctx->valueStack)
		return false;

	SpellDescriptionVars::ValueFn fn = sHandlers[ctx->opcode];
	if (!fn)
		return false;

	float value = fn(*ctx);

	// the value stack keeps its index in the slot just past its 32 entries and grows downwards.
	// the client never bounds checks it, a formula deep enough to underflow would write off the
	// end of CFormula::Evaluate's frame
	int32_t* sp = reinterpret_cast<int32_t*>(ctx->valueStack + 32);
	if (*sp > 0)
	{
		*sp -= 1;
		ctx->valueStack[*sp] = value;
	}

	return true;
}

// CFormula::GetVariableValue is __thiscall with eight stack args and a retn 20h. Sitting on the
// entry means the args are still laid out contiguously above the return address, so they map
// straight onto FormulaContext. Anything we decline falls through the replayed prologue back into
// the client at 0x005782D6.
__declspec(naked) void CFormula__GetVariableValue_Hook()
{
	__asm {
        push ecx
        lea  eax, [esp+8]
        push eax
        call DispatchFormulaVariable
        add  esp, 4
        pop  ecx
        test al, al
        jz   passthrough
        ret  0x20

    passthrough:
        push ebp
        mov  ebp, esp
        sub  esp, 0x34
        push 0x005782D6
        ret
	}
}

// jmp rel32 to hook, nops out to len so nothing lands mid instruction
static void WriteJumpPatch(uint32_t at, void* hook, size_t len)
{
	uint8_t patch[16];
	int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook) - (at + 5));
	patch[0] = 0xE9;
	std::memcpy(&patch[1], &rel, sizeof(rel));
	std::memset(&patch[5], 0x90, len - 5);
	Util::OverwriteBytesAtAddress(at, patch, len);
}

static UnitFields* ActivePlayerFields()
{
	CGPlayer* player = ClntObjMgr::GetActivePlayerObj();
	return player ? player->unitBase.unitData : nullptr;
}

// The player descriptor block is a second array hanging off CGPlayer+0x1008, not an extension of
// the unit one. Script_GetDodgeChance reads it the same way.
static PlayerFields* ActivePlayerPlayerFields()
{
	CGPlayer* player = ClntObjMgr::GetActivePlayerObj();
	return player ? player->PlayerData : nullptr;
}

static float VarCurrentHealth(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitCurrHealth) : 0.0f;
}

static float VarMaxHealth(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitMaxHealth) : 0.0f;
}

static float VarCurrentMana(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitCurrPowers[POWER_MANA]) : 0.0f;
}

static float VarMaxMana(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitMaxPowers[POWER_MANA]) : 0.0f;
}

static float VarCurrentFocus(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitCurrPowers[POWER_FOCUS]) : 0.0f;
}

static float VarMaxFocus(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? static_cast<float>(fields->unitMaxPowers[POWER_FOCUS]) : 0.0f;
}

// Already a percentage, so ${$dodge}.2 gives you "5.23" not a fraction.
static float VarDodge(const FormulaContext&)
{
	PlayerFields* fields = ActivePlayerPlayerFields();
	return fields ? fields->dodgePct : 0.0f;
}

static float VarParry(const FormulaContext&)
{
	PlayerFields* fields = ActivePlayerPlayerFields();
	return fields ? fields->parryPct : 0.0f;
}

// The client runs its ranged damage through floor for $rw and ceil for $RW and then clamps both up
// to 1, so a character with no ranged weapon reads as "1 damage" and any arithmetic done on top of
// the variable is working from an already rounded number. Hand back the raw field instead. The
// default ${} format is "%.0f", so a plain ${$rw} still renders as a whole number, and ${$rw}.1
// now actually gets a decimal.
static float VarRangedMinDamage(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? fields->minRangedDamage : 0.0f;
}

static float VarRangedMaxDamage(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	return fields ? fields->maxRangedDamage : 0.0f;
}

// baseAttackTime is main hand, off hand, ranged. Milliseconds on the wire, seconds in a formula.
static float VarRangedSpeed(const FormulaContext&)
{
	UnitFields* fields = ActivePlayerFields();
	float seconds = fields ? static_cast<float>(fields->baseAttackTime[2]) / 1000.0f : 0.0f;
	return seconds < 1.0f ? 1.0f : seconds;
}

// Item level is server pushed, so a cold cache reads 0. Firing the request on a miss means the
// next time the tooltip renders the number is there, which matches how the unit frames use it.
static float VarItemLevel(const FormulaContext&)
{
	uint64_t guid = ClntObjMgr::GetActivePlayer();
	if (!guid)
		return 0.0f;

	if (!sUnitLevelCache.HasUnitItemLevelOrDungeonLevel(guid))
	{
		UnitLevelCache::SendRequest(guid);
		return 0.0f;
	}

	return static_cast<float>(sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid));
}

// What $s<n> would have rendered, for when there is no aura in scope or no server data yet.
static float BaseEffectPoints(const FormulaContext& ctx, uint32_t effectIndex)
{
	if (!ctx.spell || effectIndex < 1 || effectIndex > ClientData::Aura::MaxSpellEffects)
		return 0.0f;

	int min = 0;
	int max = 0;
	Spell_C::GetMinMaxPoints(ctx.spell, static_cast<int>(effectIndex) - 1, &min, &max, 0, 0, 0, 0);
	return static_cast<float>(max);
}

static float AuraAmount(const FormulaContext& ctx, uint32_t effectIndex)
{
	int32_t amount = 0;
	if (ctx.spell && sAuraValuesCache.GetActiveAmount(ctx.spell->m_ID, effectIndex, amount))
		return static_cast<float>(amount);

	return BaseEffectPoints(ctx, effectIndex);
}

static float VarAuraEffect1(const FormulaContext& ctx)
{
	return AuraAmount(ctx, 1);
}

static float VarAuraEffect2(const FormulaContext& ctx)
{
	return AuraAmount(ctx, 2);
}

static float VarAuraEffect3(const FormulaContext& ctx)
{
	return AuraAmount(ctx, 3);
}

static float VarAuraAbsorb(const FormulaContext& ctx)
{
	int32_t amount = 0;
	if (ctx.spell && sAuraValuesCache.GetActiveAbsorb(ctx.spell->m_ID, ctx.spell, amount))
		return static_cast<float>(amount);

	return BaseEffectPoints(ctx, 1);
}

static float VarAuraStacks(const FormulaContext& ctx)
{
	int32_t stacks = 0;
	if (ctx.spell && sAuraValuesCache.GetActiveStacks(ctx.spell->m_ID, stacks))
		return static_cast<float>(stacks);

	return 1.0f;
}

SpellDescriptionVars& SpellDescriptionVars::Instance()
{
	static SpellDescriptionVars instance;
	return instance;
}

// Reads whatever name table the scan is currently pointed at, copies it, and repoints the scan at
// the copy. Taking the base from the operand rather than hardcoding 0x00ACE8F8 keeps us composable
// with anything else that has already moved it.
bool SpellDescriptionVars::CaptureClientTable()
{
	if (m_captured)
		return true;

	const char** names = *reinterpret_cast<const char***>(kNameTableOperand);
	uint32_t count = *reinterpret_cast<uint32_t*>(kNameCountOperand);
	if (!names || count == 0 || count > kMaxNames)
		return false;

	std::memcpy(sNames, names, count * sizeof(const char*));
	m_count = count;
	m_names.reserve(kMaxNames);
	m_captured = true;

	Util::OverwriteValue<uint32_t>(kNameTableOperand, reinterpret_cast<uint32_t>(sNames));
	return true;
}

int SpellDescriptionVars::FindIndex(const char* name) const
{
	for (uint32_t i = 0; i < m_count; ++i)
	{
		if (sNames[i] && std::strcmp(sNames[i], name) == 0)
			return static_cast<int>(i);
	}

	return -1;
}

bool SpellDescriptionVars::Add(const char* name, ValueFn fn)
{
	if (!name || !*name || !fn || !CaptureClientTable())
		return false;

	int index = FindIndex(name);
	if (index < 0)
	{
		if (m_count >= kMaxNames)
			return false;

		index = static_cast<int>(m_count);
		m_names.push_back(name);
		sNames[index] = m_names.back().c_str();
		m_count += 1;
		Util::OverwriteValue<uint32_t>(kNameCountOperand, m_count);
	}

	sHandlers[kFirstOpcode + index] = fn;
	return true;
}

bool SpellDescriptionVars::Replace(const char* name, ValueFn fn)
{
	if (!name || !fn || !CaptureClientTable())
		return false;

	int index = FindIndex(name);
	if (index < 0)
		return false;

	sHandlers[kFirstOpcode + index] = fn;
	return true;
}

void SpellDescriptionVars::RegisterBuiltins()
{
	Add("hp", VarCurrentHealth);
	Add("HP", VarMaxHealth);
	Add("mp", VarCurrentMana);
	Add("MP", VarMaxMana);
	Add("fp", VarCurrentFocus);
	Add("FP", VarMaxFocus);
	Add("ilvl", VarItemLevel);
	Add("ILVL", VarItemLevel);
	Add("dodge", VarDodge);
	Add("DODGE", VarDodge);
	Add("parry", VarParry);
	Add("PARRY", VarParry);

	Add("aura1", VarAuraEffect1);
	Add("aura2", VarAuraEffect2);
	Add("aura3", VarAuraEffect3);
	Add("AURA1", VarAuraEffect1);
	Add("AURA2", VarAuraEffect2);
	Add("AURA3", VarAuraEffect3);
	Add("absorb", VarAuraAbsorb);
	Add("ABSORB", VarAuraAbsorb);
	Add("stacks", VarAuraStacks);
	Add("STACKS", VarAuraStacks);

	Replace("rw", VarRangedMinDamage);
	Replace("RW", VarRangedMaxDamage);
	Replace("rws", VarRangedSpeed);
	Replace("RWS", VarRangedSpeed);
}

void SpellDescriptionVars::Apply()
{
	if (m_applied || !CaptureClientTable())
		return;

	RegisterBuiltins();
	WriteJumpPatch(kGetVariableValue, &CFormula__GetVariableValue_Hook, kPrologueLength);
	m_applied = true;
}
