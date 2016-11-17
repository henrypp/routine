// routine++
// Copyright (c) 2012-2016 Henry++

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
} *PAPPLICATION_PAGE, APPLICATION_PAGE;

#endif // _APP_NO_SETTINGS

/*
	Application class
*/

class rapp
{

public:

	rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);
	~rapp ();

	BOOL Initialize ();

	static BOOL CALLBACK ActivateWindowCallback (HWND hwnd, LPARAM lparam);
	VOID ActivateWindow ();

#ifdef _APP_HAVE_AUTORUN
	VOID AutorunCreate (BOOL is_remove);
	BOOL AutorunIsPresent ();
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
	BOOL TrayCreate (UINT id, UINT code, HICON h);
	BOOL TrayDestroy (UINT id);
	BOOL TrayPopup (DWORD icon, LPCWSTR title, LPCWSTR text);
	BOOL TraySetInfo (HICON h, LPCWSTR tooltip);
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_SETTINGS
	VOID CreateSettingsWindow ();
	size_t AddSettingsPage (HINSTANCE h, UINT dlg_id, UINT locale_id, LPCWSTR locale_sid, APPLICATION_CALLBACK callback, size_t group_id = LAST_VALUE, LPARAM lparam = 0);
	VOID ClearSettingsPage ();
	VOID InitSettingsPage (HWND hwnd, BOOL is_restart);
#endif // _APP_NO_SETTINGS

	rstring GetProfileDirectory () const;

	rstring GetUserAgent () const;

	INT GetDPI (INT v) const;
	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;

	VOID SetIcon (UINT icon_id);

	BOOL IsVistaOrLater ();
	BOOL IsClassicUI ();

	VOID LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);
	VOID LocaleEnum (HWND hwnd, INT ctrl_id, BOOL is_menu, const UINT id_start);
	UINT LocaleGetCount ();
	rstring LocaleString (HINSTANCE h, UINT id, LPCWSTR name);
	VOID LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position) const;

#ifdef _APP_HAVE_SKIPUAC
	BOOL SkipUacCreate (BOOL is_remove);
	BOOL SkipUacIsPresent (BOOL run);
#endif // _APP_HAVE_SKIPUAC
	BOOL SkipUacRun ();

private:

#ifndef _APP_NO_SETTINGS
	static INT_PTR CALLBACK SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);
#endif // _APP_NO_UPDATES

	static LRESULT CALLBACK MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	BOOL ParseINI (LPCWSTR path, rstring::map_two* map);

	VOID ConfigInit ();
	VOID LocaleInit ();

	DOUBLE dpi_percent = 0.f;

	BOOL is_localized = FALSE;
	BOOL is_classic = FALSE;
	BOOL is_vistaorlater = FALSE;

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

	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];

	rstring::map_two app_config_array;
	rstring::map_two app_locale_array;

	UINT app_locale_count = 0;

#ifndef _APP_NO_SETTINGS
	std::vector<PAPPLICATION_PAGE> app_settings_pages;
#endif // _APP_NO_SETTINGS
};
