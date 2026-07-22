#include "Cvars.h"

#include <ClientData/SharedDefines.h> // CVar_C::Register, ClientData::CVar
#include <ClientDetours.h>
#include <cstdio>

using ClientData::CVar;

Cvars& Cvars::Instance()
{
	static Cvars instance;
	return instance;
}

Cvar& Cvars::Register(const char* name, const char* help, bool defaultValue, ChangeFn onChange)
{
	return RegisterInternal(name, help, defaultValue ? "1" : "0", std::move(onChange));
}

Cvar& Cvars::Register(const char* name, const char* help, int defaultValue, ChangeFn onChange)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%d", defaultValue);
	return RegisterInternal(name, help, buf, std::move(onChange));
}

Cvar& Cvars::Register(const char* name, const char* help, const char* defaultValue, ChangeFn onChange)
{
	return RegisterInternal(name, help, defaultValue, std::move(onChange));
}

Cvar* Cvars::Find(const char* name)
{
	for (auto& c : m_cvars)
		if (c->m_name == name)
			return c.get();
	return nullptr;
}

Cvar& Cvars::RegisterInternal(const char* name, const char* help, const char* defaultValue, ChangeFn onChange)
{
	if (Cvar* existing = Find(name))
		return *existing;

	auto entry = std::make_unique<Cvar>();
	entry->m_name = name ? name : "";
	entry->m_help = help ? help : "";
	entry->m_default = defaultValue ? defaultValue : "";
	entry->m_value = entry->m_default;
	entry->m_onChange = std::move(onChange);

	Cvar& ref = *entry;
	m_cvars.push_back(std::move(entry));

	CreateHandle(ref);

	return ref;
}

void Cvars::CreateHandle(Cvar& cvar)
{
	cvar.m_handle = CVar_C::Register(
	    const_cast<char*>(cvar.m_name.c_str()),
	    const_cast<char*>(cvar.m_help.c_str()),
	    /*flags*/ 1,
	    const_cast<char*>(cvar.m_default.c_str()),
	    &OnCvarChanged,
	    /*category*/ 5,
	    0, 0, 0);

	if (cvar.m_handle && cvar.m_handle->m_stringValue.string)
		cvar.m_value = cvar.m_handle->m_stringValue.string;

	if (cvar.m_onChange)
		cvar.m_onChange(cvar);
}

void Cvars::ReapplyAll()
{
	for (auto& c : m_cvars)
		if (c->m_onChange)
			c->m_onChange(*c);
}

char __cdecl Cvars::OnCvarChanged(CVar* cvar, const char* /*prev*/, const char* value, const char* /*unused*/)
{
	for (auto& c : Instance().m_cvars)
	{
		if (c->m_handle == cvar)
		{
			c->m_value = value ? value : "";
			if (c->m_onChange)
				c->m_onChange(*c);
			break;
		}
	}
	return 1;
}

CLIENT_DETOUR(CVarInitialize, 0x00768340, __cdecl, void, (char* filename))
{
	CVarInitialize(filename);
	sCvars.ReapplyAll();
}
