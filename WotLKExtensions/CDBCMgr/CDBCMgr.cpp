#include "CDBCMgr.h"
#include "CDBCDefs/HDCharacterManifest.h"
#include "CDBCDefs/HDDisplaySwap.h"
#include "CDBCDefs/HDHelmHide.h"
#include "CDBCDefs/HDNpcDisplay.h"
#include "CDBCDefs/SpellClassMaskExtension.h"
#include "CDBCDefs/SpellCustomAttr.h"
#include <Logger.h>

CDBCMgr GlobalCDBCMap;

void CDBCMgr::Load()
{
	SpellClassMaskExtension().LoadDB();
	SpellCustomAttr().LoadDB();
	HDCharacterManifest().LoadDB();
	HDNpcDisplay().LoadDB();
	HDDisplaySwap().LoadDB();
	HDHelmHide().LoadDB();
}

void CDBCMgr::addCDBC(std::string cdbcName)
{
	allCDBCs[cdbcName] = CDBC();
	cdbcIndexRanges[cdbcName] = { 0, 0 };
}
