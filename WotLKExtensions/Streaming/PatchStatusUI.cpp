#include "PatchStatusUI.h"

#include "BackgroundDownloader.h"

#include <ClientDetours.h>
#include <SharedDefines.h>
#include <ClientData/ClientFunctions.h>
#include <Logger.h>

#include <string>
#include <string.h>

namespace Streaming
{
	namespace
	{
		constexpr const char* kStatusLua = "Interface\\SharedXML\\StreamingPatchStatus.lua";

		constexpr const char* kBusyMessage =
		    "The game is still downloading updated files. Please wait for patching to finish before logging in.";

		constexpr const char* kRestartMessage =
		    "A game update has been downloaded. Please restart the game to finish patching before logging in.";

		// Shown when a login is refused because we are still patching. Expects msg to be set by the caller.
		constexpr const char* kBlockedScript =
		    "if ( HoTFallbackPatchStatus_LoginBlocked ) then HoTFallbackPatchStatus_LoginBlocked(msg); "
		    "elseif ( GlueDialog_Show ) then GlueDialog_Show(\"OKAY\", msg); end";

		// Self-contained status UI
		constexpr const char* kFallbackLua = R"LUA(
local IS_GLUE = HOT_PATCH_FALLBACK_GLUE;
HOT_PATCH_FALLBACK_GLUE = nil;

if ( not IsStreaming or not GetStreamingProgress or HoTFallbackPatchStatus ) then
	return;
end

local parent = IS_GLUE and GlueParent or UIParent;
if ( not parent ) then
	return;
end

local FONT = "Fonts\\FRIZQT__.TTF";
local BUTTON_COORDS = { 0, 0.625, 0, 0.6875 };

local f = CreateFrame("Frame", "HoTFallbackPatchStatus", parent);
f:SetWidth(460);
f:SetHeight(80);
f:SetPoint("TOP", parent, "TOP", 0, -12);
f:SetFrameStrata("DIALOG");

f.bg = f:CreateTexture(nil, "BACKGROUND");
f.bg:SetAllPoints(f);
f.bg:SetTexture(0, 0, 0, 0.7);

f.title = f:CreateFontString(nil, "OVERLAY");
f.title:SetFont(FONT, 14, "OUTLINE");
f.title:SetTextColor(1, 0.82, 0);
f.title:SetPoint("TOP", f, "TOP", 0, -8);

local bar = CreateFrame("StatusBar", nil, f);
bar:SetWidth(430);
bar:SetHeight(16);
bar:SetPoint("TOP", f.title, "BOTTOM", 0, -8);
bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar");
bar:SetStatusBarColor(0.2, 0.55, 1);
bar:SetMinMaxValues(0, 1);
bar:SetValue(0);
bar.bg = bar:CreateTexture(nil, "BACKGROUND");
bar.bg:SetAllPoints(bar);
bar.bg:SetTexture(0.1, 0.1, 0.1, 0.9);
f.bar = bar;

f.detail = f:CreateFontString(nil, "OVERLAY");
f.detail:SetFont(FONT, 11);
f.detail:SetPoint("TOP", bar, "BOTTOM", 0, -6);

local function MakeButton(width, text)
	local b = CreateFrame("Button", nil, f);
	b:SetWidth(width);
	b:SetHeight(22);
	b:SetNormalTexture("Interface\\Buttons\\UI-Panel-Button-Up");
	b:SetPushedTexture("Interface\\Buttons\\UI-Panel-Button-Down");
	b:SetHighlightTexture("Interface\\Buttons\\UI-Panel-Button-Highlight");
	b:GetNormalTexture():SetTexCoord(unpack(BUTTON_COORDS));
	b:GetPushedTexture():SetTexCoord(unpack(BUTTON_COORDS));
	b:GetHighlightTexture():SetTexCoord(unpack(BUTTON_COORDS));
	b:GetHighlightTexture():SetBlendMode("ADD");
	b.text = b:CreateFontString(nil, "OVERLAY");
	b.text:SetFont(FONT, 12);
	b.text:SetPoint("CENTER", b, "CENTER", 0, 0);
	b.text:SetText(text);
	b:Hide();
	return b;
end

f.action = MakeButton(150, IS_GLUE and "Refresh UI" or "Reload UI");
f.action:SetScript("OnClick", function()
	if ( StreamingRefreshUI ) then
		StreamingRefreshUI();
	end
end);

f.dismiss = MakeButton(100, "Dismiss");
f.dismiss:SetScript("OnClick", function()
	if ( IsStreamingUIRefreshPending and IsStreamingUIRefreshPending() and not f.uiDismissed ) then
		f.uiDismissed = true;
	else
		f.restartDismissed = true;
	end
	f.state = nil;
end);

local function FormatBytes(b)
	b = b or 0;
	if ( b >= 1073741824 ) then
		return string.format("%.2f GB", b / 1073741824);
	elseif ( b >= 1048576 ) then
		return string.format("%.1f MB", b / 1048576);
	elseif ( b >= 1024 ) then
		return string.format("%.0f KB", b / 1024);
	end
	return string.format("%d B", b);
end

local function BaseName(path)
	if ( not path or path == "" ) then
		return "";
	end
	return string.match(path, "([^/\\]+)$") or path;
end

local function SetLoginEnabled(enabled)
	if ( IS_GLUE and AccountLoginLoginButton ) then
		if ( enabled ) then
			AccountLoginLoginButton:Enable();
		else
			AccountLoginLoginButton:Disable();
		end
	end
end

local function Layout()
	f.detail:ClearAllPoints();
	if ( bar:IsShown() ) then
		f.detail:SetPoint("TOP", bar, "BOTTOM", 0, -6);
	else
		f.detail:SetPoint("TOP", f.title, "BOTTOM", 0, -8);
	end

	f.action:ClearAllPoints();
	f.dismiss:ClearAllPoints();
	if ( f.action:IsShown() ) then
		f.action:SetPoint("TOP", f.detail, "BOTTOM", -54, -8);
		f.dismiss:SetPoint("TOP", f.detail, "BOTTOM", 74, -8);
	else
		f.dismiss:SetPoint("TOP", f.detail, "BOTTOM", 0, -8);
	end
end

local function ShowPanel(title, detail, showBar, showAction, showDismiss, height)
	f.title:SetText(title);
	f.detail:SetText(detail);
	if ( showBar ) then
		bar:Show();
	else
		bar:Hide();
	end
	if ( showAction ) then
		f.action:Show();
	else
		f.action:Hide();
	end
	if ( showDismiss ) then
		f.dismiss:Show();
	else
		f.dismiss:Hide();
	end
	f.bg:Show();
	f.title:Show();
	f.detail:Show();
	f:SetHeight(height);
	Layout();
end

local function HidePanel()
	f.bg:Hide();
	f.title:Hide();
	f.detail:Hide();
	bar:Hide();
	f.action:Hide();
	f.dismiss:Hide();
end

local WAIT_TEXT = IS_GLUE and "Logging in is disabled until this finishes." or "";
local RESTART_TEXT = IS_GLUE and "Logging in is disabled until you restart the game." or "";

local function RestartPending()
	return (IsStreamingRestartPending and IsStreamingRestartPending()) or false;
end

-- Nothing to lose on the login screen, so apply new interface files without asking.
local function AutoRefresh()
	if ( not IS_GLUE or not StreamingRefreshUI ) then
		return false;
	end
	if ( AccountLogin and not AccountLogin:IsShown() ) then
		return false; -- past the login screen, don't yank the UI out from under them
	end
	ShowPanel("Applying interface update...", "", false, false, false, 60);
	SetLoginEnabled(false);
	StreamingRefreshUI();
	return true;
end

local function Apply(state)
	if ( state == "s" ) then
		f.uiDismissed = false;
		f.restartDismissed = false;
		ShowPanel("Downloading game files...", "", true, false, false, IS_GLUE and 94 or 80);
		SetLoginEnabled(false);
	elseif ( state == "c" ) then
		ShowPanel("Checking for game updates...", WAIT_TEXT, false, false, false, 60);
		SetLoginEnabled(false);
	elseif ( state == "u" ) then
		if ( not AutoRefresh() ) then
			ShowPanel("Interface update ready",
				"New interface files were downloaded. Refresh the UI to apply them.",
				false, true, true, 100);
			SetLoginEnabled(not RestartPending());
		end
	elseif ( state == "r" ) then
		if ( IS_GLUE ) then
			ShowPanel("Restart required",
				"A game update has been downloaded. Restart the game to finish patching.\n" .. RESTART_TEXT,
				false, false, false, 100);
			SetLoginEnabled(false);
		else
			ShowPanel("Update downloaded", "Please restart the game to finish updating.",
				false, false, true, 100);
		end
	else
		HidePanel();
		SetLoginEnabled(true);
	end
end

local function CurrentState()
	if ( IsStreaming() ) then
		return "s";
	elseif ( IsStreamingBusy and IsStreamingBusy() ) then
		return "c";
	elseif ( IsStreamingUIRefreshPending and IsStreamingUIRefreshPending() and not f.uiDismissed ) then
		return "u";
	elseif ( RestartPending() and (IS_GLUE or not f.restartDismissed) ) then
		return "r";
	end
	return "h";
end

f:SetScript("OnUpdate", function(self, elapsed)
	self.elapsed = (self.elapsed or 0) + (elapsed or 0);
	if ( self.elapsed < 0.1 ) then
		return;
	end
	self.elapsed = 0;

	local state = CurrentState();
	if ( state ~= self.state ) then
		self.state = state;
		Apply(state);
	end

	if ( state ~= "s" ) then
		return;
	end

	local done, total, doneBytes, totalBytes, file = GetStreamingProgress();
	done = done or 0;
	total = total or 0;
	doneBytes = doneBytes or 0;
	totalBytes = totalBytes or 0;

	local frac = 0;
	if ( totalBytes > 0 ) then
		frac = doneBytes / totalBytes;
		if ( frac < 0 ) then
			frac = 0;
		elseif ( frac > 1 ) then
			frac = 1;
		end
	end
	bar:SetValue(frac);

	local text = string.format("%s / %s  (%d%%)", FormatBytes(doneBytes), FormatBytes(totalBytes), math.floor(frac * 100));
	if ( total > 1 ) then
		local current = done + 1;
		if ( current > total ) then
			current = total;
		end
		text = string.format("%s   -   file %d of %d", text, current, total);
	end
	local name = BaseName(file);
	if ( name ~= "" ) then
		text = string.format("%s   -   %s", text, name);
	end
	if ( WAIT_TEXT ~= "" ) then
		text = text .. "\n" .. WAIT_TEXT;
	end
	f.detail:SetText(text);
end);

function HoTFallbackPatchStatus_LoginBlocked(msg)
	f.state = nil;
	if ( GlueDialog_Show ) then
		GlueDialog_Show("OKAY", msg or "The game is still patching. Please wait before logging in.");
	end
end

HidePanel();
)LUA";
	}

	bool PatchStatusUI::ClientUIMissing()
	{
		return SFile::FileExistsEx(kStatusLua, 1) == 0;
	}

	void PatchStatusUI::Load(const char* state)
	{
		if (!state || !ClientUIMissing())
			return;

		const bool glue = _stricmp(state, "glue") == 0;
		LOG_DEBUG << "Missing " << kStatusLua << ", loading built-in patch status UI (" << state << ")";

		std::string script = std::string("HOT_PATCH_FALLBACK_GLUE = ") + (glue ? "true" : "false") + ";\n";
		script += kFallbackLua;
		FrameScript::Execute(script.c_str(), "HoTPatchStatus", 0);
	}

	CLIENT_DETOUR(Script_DefaultServerLogin, 0x004DC260, __cdecl, int, (lua_State * L))
	{
		const char* message = nullptr;
		if (sBackgroundDownloader.IsBusy())
			message = kBusyMessage;
		else if (sBackgroundDownloader.IsRestartPending())
			message = kRestartMessage;

		if (message)
		{
			LOG_DEBUG << "Refused login: " << message;
			std::string script = "local msg = \"";
			script += message;
			script += "\";\n";
			script += kBlockedScript;
			FrameScript::Execute(script.c_str(), "HoTPatchStatus", 0);
			return 0;
		}

		return Script_DefaultServerLogin(L);
	}
}
