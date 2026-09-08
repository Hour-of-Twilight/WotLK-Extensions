#include <DiscordRPC.h>
#include <Util.h>
#include <chrono>
#include <windows.h>
#include <tlhelp32.h>
#include <string>

DiscordRPC::DiscordRPC() = default;

DiscordRPC::~DiscordRPC()
{
	Shutdown();
}

static const long long CLIENT_ID = 1420551644671901780;

static bool IsDiscordRunning()
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return false;

	static const char* processNames[] = { "Discord", "DiscordPTB", "DiscordCanary", "DiscordDevelopment" };

	PROCESSENTRY32 pe{};
	pe.dwSize = sizeof(pe);
	bool found = false;

	if (Process32First(snap, &pe))
	{
		do
		{
			std::string name = pe.szExeFile;
			if (name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".exe") == 0)
				name.resize(name.size() - 4);
			for (const char* candidate : processNames)
			{
				if (_stricmp(name.c_str(), candidate) == 0)
				{
					found = true;
					break;
				}
			}
		} while (!found && Process32Next(snap, &pe));
	}

	CloseHandle(snap);
	return found;
}

void DiscordRPC::Init()
{
	if (_initialized || Util::IsWine())
		return;

	_running = true;
	_thread = std::thread([this]()
	{
		this->ThreadFunc();
	});
	_initialized = true;
}

void DiscordRPC::Shutdown()
{
	if (!_initialized)
		return;

	_running = false;

	if (_thread.joinable())
		_thread.join();

	if (_core)
	{
		if (_activityManager)
			_activityManager->ClearActivity(nullptr);
		_core->RunCallbacks();
	}

	Disconnect();
	_initialized = false;
}

bool DiscordRPC::TryConnect()
{
	discord::Core* core{};
	auto result = discord::Core::Create(CLIENT_ID, DiscordCreateFlags_Default, &core);
	if (!core || result != discord::Result::Ok)
		return false;

	_core = core;
	_activityManager = &core->ActivityManager();

	std::lock_guard<std::mutex> lock(_mutex);
	if (_hasActivity)
		_hasPending = true;

	return true;
}

void DiscordRPC::Disconnect()
{
	delete _core;
	_core = nullptr;
	_activityManager = nullptr;
}

void DiscordRPC::UpdateActivity(const std::string& state,
    const std::string& details,
    const std::string& largeImage,
    const std::string& largeText,
    const std::string& smallImage,
    const std::string& smallText)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_pendingActivity.SetState(state.c_str());
	_pendingActivity.SetDetails(details.c_str());

	if (!largeImage.empty())
		_pendingActivity.GetAssets().SetLargeImage(largeImage.c_str());

	if (!largeText.empty())
		_pendingActivity.GetAssets().SetLargeText(largeText.c_str());

	if (!smallImage.empty())
		_pendingActivity.GetAssets().SetSmallImage(smallImage.c_str());

	if (!smallText.empty())
		_pendingActivity.GetAssets().SetSmallText(smallText.c_str());

	_hasActivity = true;
	_hasPending = true;
}

void DiscordRPC::FlushPendingActivity()
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (!_hasPending)
		return;

	_activityManager->UpdateActivity(_pendingActivity, [](discord::Result)
	{
	});
	_hasPending = false;
}

void DiscordRPC::ThreadFunc()
{
	using Clock = std::chrono::steady_clock;
	Clock::time_point nextDetect{};

	while (_running)
	{
		if (!_core)
		{
			auto now = Clock::now();
			if (now >= nextDetect)
			{
				nextDetect = now + std::chrono::seconds(15);
				if (IsDiscordRunning())
					TryConnect();
			}

			if (!_core)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
		}

		if (_core->RunCallbacks() != discord::Result::Ok)
		{
			Disconnect();
			continue;
		}

		if (_activityManager)
			FlushPendingActivity();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
