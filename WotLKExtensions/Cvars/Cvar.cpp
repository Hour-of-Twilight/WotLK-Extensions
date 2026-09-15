#include "Cvar.h"

#include <ClientData/SharedDefines.h>   // ClientData::CVar
#include <ClientData/ClientFunctions.h> // CVar_C::SetCvar
#include <cstdio>
#include <cstdlib>

const std::string& Cvar::Effective() const
{
	return m_value.empty() ? m_default : m_value;
}

bool Cvar::AsBool() const
{
	return AsInt() != 0;
}

int Cvar::AsInt() const
{
	return std::atoi(Effective().c_str());
}

float Cvar::AsFloat() const
{
	return static_cast<float>(std::atof(Effective().c_str()));
}

const char* Cvar::AsString() const
{
	return Effective().c_str();
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
