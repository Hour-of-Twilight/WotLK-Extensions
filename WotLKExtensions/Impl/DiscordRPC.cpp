#include <DiscordRPC.h>
#include <chrono>
#include <windows.h>
#include <tlhelp32.h>
#include <string>

DiscordRPC::DiscordRPC() = default;

static bool IsDiscordRunning()
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return false;

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
			if (_stricmp(name.c_str(), "Discord") == 0)
			{
				found = true;
				break;
			}
		} while (Process32Next(snap, &pe));
	}

	CloseHandle(snap);
	return found;
}

DiscordRPC::~DiscordRPC()
{
	Shutdown();
}

static const long long CLIENT_ID = 1420551644671901780;

void DiscordRPC::Init()
{
	if (_initialized)
		return;

	if (!IsDiscordRunning())
		return;

	discord::Core* core{};
	auto result = discord::Core::Create(CLIENT_ID, DiscordCreateFlags_Default, &core);
	if (!core || result != discord::Result::Ok)
	{
		return;
	}

	_core = core;
	_activityManager = &core->ActivityManager();
	_running = true;

	// Start background thread
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

	ClearActivity();
	delete _activityManager;
	delete _core;
	_core = nullptr;
	_activityManager = nullptr;
	_initialized = false;
}

void DiscordRPC::ClearActivity()
{
	if (!_initialized || !_activityManager)
		return;

	_activityManager->ClearActivity(nullptr);
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

	_hasPending = true;
}

void DiscordRPC::ThreadFunc()
{
	while (_running)
	{
		if (_core)
			_core->RunCallbacks();

		if (_hasPending && _activityManager)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_activityManager->UpdateActivity(_pendingActivity, [](discord::Result res)
			{
				if (res != discord::Result::Ok)
				{
				}
			});
			_hasPending = false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
