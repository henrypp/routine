// routine++
// Copyright (c) 2012-2016 Henry++

// lastmod: Jan 17, 2016

#pragma once

#include "routine.h"
#include "resource.h"

#include <wininet.h>
#include <taskschd.h>
#include <shlobj.h>
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
	HINSTANCE h;

	HWND hwnd;

	UINT dlg_id;

	rstring title;

	APPLICATION_CALLBACK callback;
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

	VOID AutorunCreate (BOOL is_remove);
	BOOL AutorunIsPresent ();

#ifndef _APP_NO_UPDATES
	VOID CheckForUpdates (BOOL is_periodical);
#endif // _APP_NO_UPDATES

	LONGLONG ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr);

	BOOL ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name = nullptr);
	BOOL ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);

#ifndef _APP_NO_ABOUT
	VOID CreateAboutWindow ();
#endif // _APP_NO_ABOUT

	BOOL CreateMainWindow (DLGPROC proc);

#ifndef _APP_NO_SETTINGS
	VOID CreateSettingsWindow ();
	VOID AddSettingsPage (HINSTANCE h, UINT dlg_id, LPCWSTR title, APPLICATION_CALLBACK callback);
#endif // _APP_NO_SETTINGS

	rstring GetDirectory ();
	rstring GetProfileDirectory ();

	rstring GetUserAgent ();

	INT GetDPI (INT v);
	HINSTANCE GetHINSTANCE ();
	HWND GetHWND ();

	VOID SetHWND (HWND hwnd);

	VOID Restart ();

	VOID LocaleEnum (HWND hwnd, INT ctrl_id);
	rstring LocaleString (HINSTANCE h, UINT id, LPCWSTR name);
	VOID LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position);

#ifdef _APP_NO_UAC
	BOOL SkipUacCreate (BOOL is_remove);
	BOOL SkipUacRun ();
	BOOL SkipUacIsPresent (BOOL run);
#endif // _APP_NO_UAC

private:

#ifndef _APP_NO_ABOUT
	static LRESULT CALLBACK AboutWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_NO_ABOUT

#ifndef _APP_NO_SETTINGS
	static INT_PTR CALLBACK SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);
#endif // _APP_NO_UPDATES

	BOOL ParseINI (LPCWSTR path, rstring::map_two* map);

	VOID ConfigInit ();
	VOID LocaleInit ();

	DOUBLE dpi_percent = 0.f;

	BOOL is_localized = FALSE;

#ifndef _APP_NO_UPDATES
	BOOL is_update_forced = FALSE;
#endif // _APP_NO_UPDATES

	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;

#ifndef _APP_NO_ABOUT
	HICON app_logo = nullptr;
	HFONT app_font = nullptr;
#endif // _APP_NO_ABOUT

	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];

	rstring::map_two app_config_array;
	rstring::map_two app_locale_array;

#ifndef _APP_NO_SETTINGS
	std::vector<PAPPLICATION_PAGE> app_settings_pages;
	size_t app_settings_page = 0;
#endif // _APP_NO_SETTINGS
};
