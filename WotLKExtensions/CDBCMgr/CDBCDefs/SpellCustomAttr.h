#pragma optimize("", off)
#include "../CDBC.h"
#include "../CDBCMgr.h"

enum SpellCustomAttributes : uint32_t
{
	SPELL_ATTR0_CU_ENCHANT_PROC                  = 0x00000001,
	SPELL_ATTR0_CU_CONE_BACK                     = 0x00000002,
	SPELL_ATTR0_CU_CONE_LINE                     = 0x00000004,
	SPELL_ATTR0_CU_SHARE_DAMAGE                  = 0x00000008,
	SPELL_ATTR0_CU_NO_INITIAL_THREAT             = 0x00000010,
	SPELL_ATTR0_CU_AURA_CC                       = 0x00000020,
	SPELL_ATTR0_CU_DONT_BREAK_STEALTH            = 0x00000040,
	SPELL_ATTR0_CU_CAN_CRIT                      = 0x00000080,
	SPELL_ATTR0_CU_DIRECT_DAMAGE                 = 0x00000100,
	SPELL_ATTR0_CU_CHARGE                        = 0x00000200,
	SPELL_ATTR0_CU_PICKPOCKET                    = 0x00000400,
	SPELL_ATTR0_CU_ROLLING_PERIODIC              = 0x00000800,
	SPELL_ATTR0_CU_NEGATIVE_EFF0                 = 0x00001000,
	SPELL_ATTR0_CU_NEGATIVE_EFF1                 = 0x00002000,
	SPELL_ATTR0_CU_NEGATIVE_EFF2                 = 0x00004000,
	SPELL_ATTR0_CU_IGNORE_ARMOR                  = 0x00008000,
	SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER      = 0x00010000,
	SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET      = 0x00020000,
	SPELL_ATTR0_CU_ALLOW_INFLIGHT_TARGET         = 0x00040000,
	SPELL_ATTR0_CU_NEEDS_AMMO_DATA               = 0x00080000,
	SPELL_ATTR0_CU_BINARY_SPELL                  = 0x00100000,
	SPELL_ATTR0_CU_SCHOOLMASK_NORMAL_WITH_MAGIC  = 0x00200000,
	SPELL_ATTR0_CU_DEPRECATED_LIQUID_AURA        = 0x00400000,
	SPELL_ATTR0_CU_IS_TALENT                     = 0x00800000,
	SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED          = 0x01000000,
	SPELL_ATTR0_CU_CAN_TARGET_ANY_PRIVATE_OBJECT = 0x02000000,
	SPELL_ATTR0_CU_REQ_DUAL_WIELD                = 0x04000000,
};

enum SpellCustomAttributes2 : uint32_t
{
	SPELL_ATTR1_CU_NONE                          = 0x00000000,
};

struct SpellCustomAttrRow
{
	uint32_t spellID;
	uint32_t attributes;
	uint32_t attributes2;
};

class SpellCustomAttr : public CDBC
{
public:
	const char* fileName = "SpellCustomAttr";

	SpellCustomAttr() : CDBC()
	{
		this->numColumns = sizeof(SpellCustomAttrRow) / 4;
		this->rowSize = sizeof(SpellCustomAttrRow);
	}

	SpellCustomAttr* LoadDB()
	{
		GlobalCDBCMap.addCDBC(this->fileName);
		GlobalCDBCMap.registerRowType<SpellCustomAttrRow>(this->fileName);
		CDBC::LoadDB(this->fileName, true);
		SpellCustomAttr::setupTable();
		GlobalCDBCMap.setIndexRange(this->fileName, this->minIndex, this->maxIndex);
		return this;
	}

	void setupTable()
	{
		SpellCustomAttrRow* row = static_cast<SpellCustomAttrRow*>(this->rows);
		if (!row)
			return;

		for (uint32_t i = 0; i < this->numRows; i++)
		{
			GlobalCDBCMap.addRow(this->fileName, row->spellID, *row);
			++row;
		}
	}
};
#pragma optimize("", on)
