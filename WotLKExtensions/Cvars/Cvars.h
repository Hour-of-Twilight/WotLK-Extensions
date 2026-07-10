#pragma once

#include "Cvar.h"

#include <memory>
#include <vector>

namespace ClientData
{
	struct CVar;
}

class Cvars
{
public:
	static Cvars& Instance();

	using ChangeFn = std::function<void(Cvar&)>;

	Cvar& Register(const char* name, const char* help, bool defaultValue, ChangeFn onChange = {});
	Cvar& Register(const char* name, const char* help, int defaultValue, ChangeFn onChange = {});
	Cvar& Register(const char* name, const char* help, const char* defaultValue, ChangeFn onChange = {});

	Cvar* Find(const char* name);

	void ReapplyAll();

	Cvars(const Cvars&) = delete;
	Cvars& operator=(const Cvars&) = delete;

private:
	Cvars() = default;

	Cvar& RegisterInternal(const char* name, const char* help, const char* defaultValue, ChangeFn onChange);
	void CreateHandle(Cvar& cvar);

	static char __cdecl OnCvarChanged(ClientData::CVar* cvar, const char* prev, const char* value, const char* unused);

	std::vector<std::unique_ptr<Cvar>> m_cvars;
};

#define sCvars Cvars::Instance()
