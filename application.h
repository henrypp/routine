// application support
// Copyright (c) 2013-2015 Henry++

// lastmod: Nov 25, 2015

#pragma once

#include "routine.h"
#include "resource.h"

#include <wininet.h>
#include <taskschd.h>
#include <shlobj.h>
#include <comdef.h>

#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "wininet.lib")

//#define APPLICATION_NEED_PRIVILEGES // application needs high privileges
#define APPLICATION_TASKSCHD_NAME L"%sSkipUac"
#define APPLICATION_UPDATE_PERIOD 2 // update checking period (in days)

#define APPLICATION_LOCALE_DIRECTORY L"i18n"
#define APPLICATION_LOCALE_SECTION L"i18n"

/*
	Localization macroses
*/

#define I18N(ptr, id, str) ((ptr)->LocaleString(id, (str) ? (str) : L#id))

/*
	Callback function definitions
*/

typedef BOOL (*SETTINGS_SAVE_CALLBACK) (HWND hwnd, DWORD page_id);

/*
	Application class
*/

class CApplication
{

public:

	CApplication (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR author, LPCWSTR copyright, LPCWSTR website, LPCWSTR github);
	~CApplication ();

	VOID AutorunCreate (BOOL is_remove);
	BOOL AutorunIsPresent ();

	VOID CheckForUpdates (BOOL is_periodical);

	VOID ConfigInit ();

	DWORD ConfigGet (LPCWSTR key, INT def);
	CString ConfigGet (LPCWSTR key, LPCWSTR def);

	BOOL ConfigSet (LPCWSTR key, LPCWSTR val);
	BOOL ConfigSet (LPCWSTR key, DWORD val);

	VOID CreateAboutWindow ();
	BOOL CreateMainWindow (DLGPROC proc);
	VOID CreateSettingsWindow (DWORD page_count, DLGPROC page_proc, SETTINGS_SAVE_CALLBACK callback);

	CString GetDirectory ();
	CString GetProfileDirectory ();

	HWND GetHWND ();
	VOID SetHWND (HWND hwnd);

	VOID Restart ();

	VOID LocaleEnum (HWND hwnd, INT ctrl_id);
	VOID LocaleSet (LPCWSTR name);
	CString LocaleString (UINT id, LPCWSTR name);
	VOID LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position);

	BOOL SkipUacCreate (BOOL is_remove);
	BOOL SkipUacRun ();
	BOOL SkipUacIsPresent (BOOL run);

private:

	static INT_PTR CALLBACK AboutWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);

	BOOL ParseINI (LPCWSTR path, LPCWSTR section, CStringMap* map);

	BOOL is_initialized = FALSE;
	BOOL is_localized = FALSE;

#ifdef _WIN64
	const DWORD app_architecture = 64;
#else
	const DWORD app_architecture = 32;
#endif

	BOOL app_cfu_mode = FALSE;

	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;
	HICON app_logo_big = nullptr;

	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];

	WCHAR app_author[MAX_PATH];

	WCHAR app_website[MAX_PATH];
	WCHAR app_github[MAX_PATH];

	WCHAR app_copyright[MAX_PATH];

	WCHAR app_config_path[MAX_PATH];

	CStringMap app_config_array;
	CStringMap app_locale_array;

	DWORD app_settings_count = 0;
	std::unordered_map<INT, HWND> app_settings_hwnd;

	DLGPROC app_settings_proc;
	SETTINGS_SAVE_CALLBACK app_settings_save;
};
