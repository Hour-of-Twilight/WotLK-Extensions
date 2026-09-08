#pragma once

#include <BuildVersion.h>
#include <Defines.h>

#include <array>
#include <string_view>

// Git commit this DLL was built from. Sent to the authserver during login, which
// rejects the session if it does not match the build the server requires.
namespace DllVersion
{
	inline constexpr std::string_view Commit{ HOTDLL_COMMIT_HASH };
	inline constexpr bool Dirty = HOTDLL_COMMIT_DIRTY != 0;

	static_assert(Commit.size() == 40, "BuildVersion.h did not produce a 40 character commit hash");

	namespace Detail
	{
		constexpr uint8 HexNibble(char c)
		{
			if (c >= '0' && c <= '9')
				return static_cast<uint8>(c - '0');
			if (c >= 'a' && c <= 'f')
				return static_cast<uint8>(c - 'a' + 10);
			if (c >= 'A' && c <= 'F')
				return static_cast<uint8>(c - 'A' + 10);
			return 0;
		}

		constexpr std::array<uint8, 20> DecodeCommit()
		{
			std::array<uint8, 20> out{};
			for (size_t i = 0; i < out.size(); ++i)
				out[i] = static_cast<uint8>((HexNibble(Commit[i * 2]) << 4) | HexNibble(Commit[i * 2 + 1]));
			return out;
		}
	}

	// The commit as the 20 raw bytes the login proof carries.
	inline constexpr std::array<uint8, 20> CommitBytes = Detail::DecodeCommit();
}
