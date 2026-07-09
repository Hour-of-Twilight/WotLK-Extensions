#pragma once
#include <discord.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>

class DiscordRPC
{
public:
	// Get the singleton instance
	static DiscordRPC& Get()
	{
		static DiscordRPC instance;
		return instance;
	}

	// Initialize with your Discord Client ID
	void Init();

	// Shutdown and clean up
	void Shutdown();

	void ClearActivity();

	// Update Rich Presence data
	void UpdateActivity(const std::string& state,
	    const std::string& details,
	    const std::string& largeImage = "",
	    const std::string& largeText = "",
	    const std::string& smallImage = "",
	    const std::string& smallText = "");

	bool IsInitialized()
	{
		return _initialized;
	}

private:
	DiscordRPC();
	~DiscordRPC();
	DiscordRPC(const DiscordRPC&) = delete;
	DiscordRPC& operator=(const DiscordRPC&) = delete;

	void ThreadFunc();

	discord::Core* _core{ nullptr };
	discord::ActivityManager* _activityManager{ nullptr };
	discord::Activity _pendingActivity{};
	std::thread _thread;
	std::mutex _mutex;
	std::atomic<bool> _running{ false };
	bool _initialized{ false };
	bool _hasPending{ false };
};

#define sDiscord DiscordRPC::Get()
