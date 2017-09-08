// routine++
// Copyright (c) 2012-2017 Henry++

#pragma once

#include "routine.hpp"
#include "resource.hpp"

#include <comdef.h>
#include <taskschd.h>

#include "rconfig.hpp"

// libs
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "taskschd.lib")

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
	WCHAR locale_sid[32] = {0};

	APPLICATION_CALLBACK callback;
	LPARAM lparam = 0;
} *PAPP_SETTINGS_PAGE, APP_SETTINGS_PAGE;

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

#endif // _APP_NO_SETTINGS

/*
	Application class
*/

class rapp
{

public:

	rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);
	~rapp ();

	bool InitializeMutex ();
	bool UninitializeMutex ();

	bool CheckMutex (bool activate_window);

#ifdef _APP_HAVE_AUTORUN
	void AutorunEnable (bool is_enable);
	bool AutorunIsEnabled ();
#endif // _APP_HAVE_AUTORUN

#ifndef _APP_NO_UPDATES
	void CheckForUpdates (bool is_periodical);
#endif // _APP_NO_UPDATES

	rstring ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr) const;

	bool ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, bool val, LPCWSTR name = nullptr);

	void ConfigInit ();

#ifndef _APP_NO_ABOUT
	void CreateAboutWindow ();
#endif // _APP_NO_ABOUT

#ifndef _APP_NO_DONATE
	void CreateDonateWindow ();
#endif // _APP_NO_DONATE

	bool CreateMainWindow (DLGPROC proc, APPLICATION_CALLBACK callback);

#ifdef _APP_HAVE_TRAY
	bool TrayCreate (HWND hwnd, UINT uid, UINT code, HICON h, bool is_hidden);
	bool TrayDestroy (UINT uid);
	bool TrayPopup (DWORD icon, LPCWSTR title, LPCWSTR text);
	bool TraySetInfo (HICON h, LPCWSTR tooltip);
	bool TrayToggle (DWORD uid, bool is_show);
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_SETTINGS
	void CreateSettingsWindow (size_t dlg_id = LAST_VALUE);
	size_t AddSettingsPage (HINSTANCE h, UINT dlg_id, UINT locale_id, LPCWSTR locale_sid, APPLICATION_CALLBACK callback, size_t group_id = LAST_VALUE, LPARAM lparam = 0);
	void ClearSettingsPage ();
	void InitSettingsPage (HWND hwnd, bool is_restart);

#ifdef _APP_HAVE_SIMPLE_SETTINGS
	void AddSettingsItem (LPCWSTR name, LPCWSTR def_value, CfgType type, UINT locale_id, LPCWSTR locale_sid);
#endif // _APP_HAVE_SIMPLE_SETTINGS

#endif // _APP_NO_SETTINGS

	rstring GetBinaryPath () const;
	rstring GetDirectory () const;
	rstring GetProfileDirectory () const;

	rstring GetUserAgent () const;

	INT GetDPI (INT v) const;
	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;

	void SetIcon (UINT icon_id);

	bool IsAdmin () const;
	bool IsClassicUI () const;
	bool IsVistaOrLater () const;

	void LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);
	void LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, const UINT id_start);
	UINT LocaleGetCount ();
	rstring LocaleString (HINSTANCE h, UINT id, LPCWSTR name);
	void LocaleMenu (HMENU menu, LPCWSTR text, UINT item, bool by_position) const;

#ifdef _APP_HAVE_SKIPUAC
	bool SkipUacEnable (bool is_enable);
	bool SkipUacIsEnabled ();
	bool SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC

	bool RunAsAdmin ();

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

	bool ParseINI (LPCWSTR path, rstring::map_two* map);

	void LocaleInit ();

	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;

	DOUBLE dpi_percent = 0.f;

	bool is_localized = false;
	bool is_classic = false;
	bool is_vistaorlater = false;
	bool is_admin = false;

#ifdef _APP_HAVE_TRAY
	NOTIFYICONDATA nid = {0};
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_UPDATES
	bool is_update_forced = false;
	bool update_lock = false;
#endif // _APP_NO_UPDATES

#ifndef _APP_NO_ABOUT
	bool is_about_opened = false;
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

	WCHAR default_locale[LOCALE_NAME_MAX_LENGTH];

	rstring::map_two app_config_array;
	rstring::map_two app_locale_array;

#ifdef _APP_HAVE_SIZING
	INT max_width = 0;
	INT max_height = 0;
#endif // _APP_HAVE_SIZING

	UINT app_locale_count = 0;

#ifndef _APP_NO_SETTINGS
	std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
	rstring::map_two app_settings;
	size_t settings_page = 0;

#ifdef _APP_HAVE_SIMPLE_SETTINGS
	std::unordered_map<rstring, PAPP_SETTINGS_CONFIG, rstring::hash, rstring::is_equal> app_configs;
#endif // _APP_HAVE_SIMPLE_SETTINGS

#endif // _APP_NO_SETTINGS
};
