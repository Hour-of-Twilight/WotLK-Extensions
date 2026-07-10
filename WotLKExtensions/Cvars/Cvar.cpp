#include "Cvar.h"

#include <ClientData/SharedDefines.h>  // ClientData::CVar
#include <ClientData/ClientFunctions.h> // CVar_C::SetCvar
#include <cstdio>
#include <cstdlib>

bool Cvar::AsBool() const
{
	return AsInt() != 0;
}

int Cvar::AsInt() const
{
	return std::atoi(m_value.c_str());
}

float Cvar::AsFloat() const
{
	return static_cast<float>(std::atof(m_value.c_str()));
}

const char* Cvar::AsString() const
{
	return m_value.c_str();
}

void Cvar::Set(const char* value)
{
	if (m_handle)
		CVar_C::SetCvar(m_handle, value, /*setValue*/ true, /*setReset*/ false, /*setDefault*/ false, /*updateConfig*/ true);
	else
		m_default = value ? value : "";
}

void Cvar::Set(int value)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%d", value);
	Set(buf);
}
