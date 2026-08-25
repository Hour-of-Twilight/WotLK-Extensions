#pragma once

#include <ClientData/AdtTypes.h>
#include <ClientData/Map.h>

// The client structs and bindings the editor rides on all live in ClientData. This pulls them into
// reach and keeps the short Access:: name the editor has always used for them.
namespace MapEditor
{
	using namespace ClientData;

	namespace Access = ClientData::Map;
}
