#pragma once

#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

// The eight arguments CFormula::GetVariableValue is called with, in stack order. A variable that
// only cares about the player can ignore all of them, they are here for the ones that do not.
struct FormulaContext
{
	uint32_t opcode;
	float* valueStack;
	SpellRow* spell;     // the spell being described, already retargeted by a $<spellid> prefix
	int32_t spellLevel;  // caster level the description is being rendered at
	int32_t pointsScale; // multiplier the parser folds into $m/$M/$s/$o
	int32_t parseUnit;   // resolve against the inspect target rather than the player
	int32_t parsePet;    // resolve against the active pet rather than the player
	int32_t onlyOneOnAura;
};

// Adds and replaces ${} variables in spell descriptions.
//
// The client keeps its 140 variable names in a flat table that CFormula::CompileElement scans with
// a case sensitive SStrCmp, and the matching index plus 22 becomes the bytecode opcode. We copy
// that table into our own, point the scan at the copy, and hook the opcode dispatcher so any name
// we claim resolves through us. Opcodes are stored as bytes, so 255 is the ceiling and there is
// room for 94 new names on top of the client's.
class SpellDescriptionVars
{
public:
	static SpellDescriptionVars& Instance();

	using ValueFn = float (*)(const FormulaContext&);

	// Defines ${name}. Case sensitive. Naming an existing variable takes it over, same as Replace.
	// False means the opcode space is full or the client table could not be read.
	bool Add(const char* name, ValueFn fn);

	// Takes over one of the client's own variables. False if there is no such name.
	bool Replace(const char* name, ValueFn fn);

	void Apply();

	SpellDescriptionVars(const SpellDescriptionVars&) = delete;
	SpellDescriptionVars& operator=(const SpellDescriptionVars&) = delete;

private:
	SpellDescriptionVars() = default;

	bool CaptureClientTable();
	int FindIndex(const char* name) const;
	void RegisterBuiltins();

	std::vector<std::string> m_names; // backing storage for the names we appended
	uint32_t m_count = 0;
	bool m_captured = false;
	bool m_applied = false;
};

#define sSpellDescriptionVars SpellDescriptionVars::Instance()
