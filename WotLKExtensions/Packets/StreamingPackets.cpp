#include "StreamingPackets.h"
#include "Packet.h"

#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <SharedDefines.h>
#include <ClientData/Streaming.h>
#include <Helpers/Util.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

static constexpr uint8_t STREAMING_FLAG_START = 0x1;

void StreamingPackets::Handler_SMSG_STREAMING_START(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	int8_t flags = r.GetInt8();

	char filename[128] = {};
	r.GetString(filename, sizeof(filename));
	if (filename[0] == '\0')
		std::strcpy(filename, "WoW.mfil");

	char initArg[260] = {};
	r.GetString(initArg, sizeof(initArg));

	uint32_t mfilSize = r.GetUInt32();

	std::vector<uint8_t> body(mfilSize);
	r.GetBytes(body.data(), mfilSize);

	r.Release();

	fs::path target = Util::GetExeDir() / filename;
	if (mfilSize)
	{
		std::error_code ec;
		fs::create_directories(target.parent_path(), ec);
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			Util::DebugOutput("SMSG_STREAMING_START: failed to write %s", target.string().c_str());
			return;
		}
		out.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(mfilSize));
		out.close();
		Util::DebugOutput("SMSG_STREAMING_START: wrote %u bytes to %s",
		    mfilSize, target.string().c_str());
	}

	if (!(static_cast<uint8_t>(flags) & STREAMING_FLAG_START))
		return;

	if (ClientData::Streaming::IsReady())
	{
		Util::DebugOutput("SMSG_STREAMING_START: streaming already active, not re-initializing");
		return;
	}

	std::string arg = initArg[0] ? std::string(initArg) : target.string();
	bool ok = ClientData::Streaming::Start(arg.c_str());
	Util::DebugOutput("SMSG_STREAMING_START: InitializeStreaming(%s) -> %s",
	    arg.c_str(), ok ? "ok" : "FAILED");
}

void StreamingPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_STREAMING_START, &Handler_SMSG_STREAMING_START);
}
