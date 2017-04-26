// routine++
// Copyright (c) 2012-2017 Henry++

#pragma once

#include "routine.h"
#include "resource.h"

#include <wininet.h>
#include <taskschd.h>
#include <comdef.h>

#include "rconfig.h"

// libs
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "wininet.lib")

/*
	Localization macros
*/

#define I18N(ptr, id, str) ((ptr)->LocaleString(nullptr, id, (str) ? (str) : L#id))

/*
	Callback functions
*/

typedef BOOL (*APPLICATION_CALLBACK) (HWND hwnd, DWORD msg, LPVOID lpdata1, LPVOID lpdata2);

/*
	Structures
*/

#ifndef _APP_NO_SETTINGS

typedef struct
{
	HINSTANCE h = nullptr;
	HTREEITEM item = nullptr;
	HWND hwnd = nullptr;

	size_t group_id = 0;
	UINT dlg_id = 0;

	UINT locale_id = 0;
	WCHAR locale_sid[64] = {0};

	APPLICATION_CALLBACK callback;
	LPARAM lparam = 0;
} *PAPP_SETTINGS_PAGE, APP_SETTINGS_PAGE;

#endif // _APP_NO_SETTINGS

#ifdef _APP_HAVE_SIMPLE_SETTINGS
enum CfgType
{
	Boolean,
	Integer,
	Long,
	String,
};

typedef struct
{
	CfgType type;

	WCHAR def_value[128] = {0};

	UINT locale_id = 0;
	WCHAR locale_sid[64] = {0};
} *PAPP_SETTINGS_CONFIG, APP_SETTINGS_CONFIG;
#endif // _APP_HAVE_SIMPLE_SETTINGS

/*
	Application class
*/

class rapp
{

public:

	rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);
	~rapp ();

	BOOL InitializeMutex ();
	BOOL UninitializeMutex ();

	BOOL CheckMutex (BOOL activate_window);

#ifdef _APP_HAVE_AUTORUN
	VOID AutorunEnable (BOOL is_enable);
	BOOL AutorunIsEnabled ();
#endif // _APP_HAVE_AUTORUN

#ifndef _APP_NO_UPDATES
	VOID CheckForUpdates (BOOL is_periodical);
#endif // _APP_NO_UPDATES

	rstring ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr) const;

	BOOL ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name = nullptr);
	BOOL ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);

#ifndef _APP_NO_ABOUT
	VOID CreateAboutWindow ();
#endif // _APP_NO_ABOUT

	BOOL CreateMainWindow (DLGPROC proc, APPLICATION_CALLBACK callback);

#ifdef _APP_HAVE_TRAY
	BOOL TrayCreate (HWND hwnd, UINT uid, UINT code, HICON h, BOOL is_hidden);
	BOOL TrayDestroy (UINT uid);
	BOOL TrayPopup (DWORD icon, LPCWSTR title, LPCWSTR text);
	BOOL TraySetInfo (HICON h, LPCWSTR tooltip);
	BOOL TrayToggle (DWORD uid, BOOL is_show);
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_SETTINGS
	VOID CreateSettingsWindow ();
	size_t AddSettingsPage (HINSTANCE h, UINT dlg_id, UINT locale_id, LPCWSTR locale_sid, APPLICATION_CALLBACK callback, size_t group_id = LAST_VALUE, LPARAM lparam = 0);
	VOID ClearSettingsPage ();
	VOID InitSettingsPage (HWND hwnd, BOOL is_restart);
#endif // _APP_NO_SETTINGS

#ifdef _APP_HAVE_SIMPLE_SETTINGS
	VOID AddSettingsItem (LPCWSTR name, LPCWSTR def_value, CfgType type, UINT locale_id, LPCWSTR locale_sid);
#endif // _APP_HAVE_SIMPLE_SETTINGS

	rstring GetBinaryPath () const;
	rstring GetDirectory () const;
	rstring GetProfileDirectory () const;

	rstring GetUserAgent () const;

	INT GetDPI (INT v) const;
	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;

	VOID SetIcon (UINT icon_id);

	BOOL IsAdmin ();
	BOOL IsClassicUI ();
	BOOL IsVistaOrLater ();

	VOID LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);
	VOID LocaleEnum (HWND hwnd, INT ctrl_id, BOOL is_menu, const UINT id_start);
	UINT LocaleGetCount ();
	rstring LocaleString (HINSTANCE h, UINT id, LPCWSTR name);
	VOID LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position) const;

#ifdef _APP_HAVE_SKIPUAC
	BOOL SkipUacEnable (BOOL is_enable);
	BOOL SkipUacIsEnabled ();
	BOOL SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC
	BOOL RunAsAdmin ();

private:

#ifndef _APP_NO_SETTINGS
	static INT_PTR CALLBACK SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);
#endif // _APP_NO_UPDATES

	static LRESULT CALLBACK MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static BOOL CALLBACK ActivateWindowCallback (HWND hwnd, LPARAM lparam);

	BOOL ParseINI (LPCWSTR path, rstring::map_two* map);

	VOID ConfigInit ();
	VOID LocaleInit ();

	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;

	DOUBLE dpi_percent = 0.f;

	BOOL is_localized = FALSE;
	BOOL is_classic = FALSE;
	BOOL is_vistaorlater = FALSE;
	BOOL is_admin = FALSE;

#ifdef _APP_HAVE_TRAY
	NOTIFYICONDATA nid = {0};
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_UPDATES
	BOOL is_update_forced = FALSE;
#endif // _APP_NO_UPDATES

#ifndef _APP_NO_ABOUT
	BOOL is_about_opened = FALSE;
#endif // _APP_NO_ABOUT

	WNDPROC app_wndproc = nullptr;
	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;
	APPLICATION_CALLBACK app_callback = nullptr;

	HICON app_icon_1 = nullptr;
	HICON app_icon_2 = nullptr;

	WCHAR app_binary[MAX_PATH];
	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];

	rstring::map_two app_config_array;
	rstring::map_two app_locale_array;

#ifdef _APP_HAVE_SIZING
	INT max_width = 0;
	INT max_height = 0;
#endif // _APP_HAVE_SIZING

#ifdef _APP_HAVE_SIMPLE_SETTINGS
	std::unordered_map<rstring, PAPP_SETTINGS_CONFIG, rstring::hash, rstring::is_equal> app_configs;
#endif // _APP_HAVE_SIMPLE_SETTINGS

	UINT app_locale_count = 0;

#ifndef _APP_NO_SETTINGS
	std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
	rstring::map_two app_settings;
#endif // _APP_NO_SETTINGS
};
