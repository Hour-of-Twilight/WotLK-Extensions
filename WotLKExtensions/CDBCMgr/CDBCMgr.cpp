#include "CDBCMgr.h"
#include "CDBCDefs/SpellClassMaskExtension.h"
#include "CDBCDefs/SpellCustomAttr.h"
#include <Logger.h>

CDBCMgr GlobalCDBCMap;

void CDBCMgr::Load()
{
	SpellClassMaskExtension().LoadDB();
	SpellCustomAttr().LoadDB();
}

void CDBCMgr::addCDBC(std::string cdbcName)
{
	allCDBCs[cdbcName] = CDBC();
	cdbcIndexRanges[cdbcName] = { 0, 0 };
}
