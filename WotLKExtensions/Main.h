#pragma once

#include "CustomLua.h"
#include <CustomPacket.h>
#include "Misc.h"
#include "Player.h"
#include "XMLExtensions.h"
#include "Spell.h"
#include "CDBCMgr/CDBCMgr.h"

class Main
{
public:
	static void OnAttach();
	static void Init();
};
