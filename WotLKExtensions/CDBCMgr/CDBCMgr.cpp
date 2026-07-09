#include "CDBCMgr.h"
#include "CDBCDefs/SpellClassMaskExtension.h"
#include <Logger.h>

CDBCMgr GlobalCDBCMap;

void CDBCMgr::Load()
{
	SpellClassMaskExtension().LoadDB();
}

void CDBCMgr::addCDBC(std::string cdbcName)
{
	allCDBCs[cdbcName] = CDBC();
	cdbcIndexRanges[cdbcName] = { 0, 0 };
}
