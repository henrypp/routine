// application support
// Copyright (c) 2013-2015 Henry++

// lastmod: Dec 3, 2015

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

/*
	Localization macros
*/

#define I18N(ptr, id, str) ((ptr)->LocaleString(nullptr, id, (str) ? (str) : L#id))

/*
	Configuration
*/

//#define APPLICATION_NO_ABOUT // cut about window management at compile time
//#define APPLICATION_NO_SETTINGS // cut settings window management at compile time
//#define APPLICATION_NO_UPDATES // cut update checking at compile time
//#define APPLICATION_NO_UAC // enable skip uac feature

#define APPLICATION_GITHUB_URL L"https://github.com/henrypp"
#define APPLICATION_WEBSITE_URL L"http://www.henrypp.org"

#define APPLICATION_I18N_DIRECTORY L"i18n"
#define APPLICATION_I18N_SECTION L"i18n"

#define APPLICATION_TASKSCHD_NAME L"%sSkipUac"
#define APPLICATION_UPDATE_PERIOD 2 // update checking period (in days)

/*
	Callback message codes
*/

#define _A_CALLBACK_INITIALIZE 0x1
#define _A_CALLBACK_UNINITIALIZE 0x2

#define _A_CALLBACK_SETTINGS_SAVE 0x4
#define _A_CALLBACK_SETTINGS_INIT 0x8

#define _A_CALLBACK_MESSAGE 0x16

/*
	Callback functions
*/

typedef BOOL (*APPLICATION_CALLBACK) (HWND hwnd, DWORD msg, LPVOID lpdata1, LPVOID lpdata2);

/*
	Structures
*/

#ifndef APPLICATION_NO_SETTINGS

typedef struct
{
	HINSTANCE h;

	HWND hwnd;

	UINT dlg_id;

	CString title;

	APPLICATION_CALLBACK callback;
} *PAPPLICATION_PAGE, APPLICATION_PAGE;

#endif // APPLICATION_NO_SETTINGS

/*
	Application class
*/

class CApplication
{

public:

	CApplication (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR author, LPCWSTR copyright);
	~CApplication ();

	VOID AutorunCreate (BOOL is_remove);
	BOOL AutorunIsPresent ();

#ifndef APPLICATION_NO_UPDATES
	VOID CheckForUpdates (BOOL is_periodical);
#endif // APPLICATION_NO_UPDATES

	DWORD ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr);
	CString ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr);

	BOOL ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name = nullptr);
	BOOL ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);

#ifndef APPLICATION_NO_ABOUT
	VOID CreateAboutWindow ();
#endif // APPLICATION_NO_ABOUT

	BOOL CreateMainWindow (DLGPROC proc);

#ifndef APPLICATION_NO_SETTINGS
	VOID CreateSettingsWindow ();
	VOID AddSettingsPage (HINSTANCE h, UINT dlg_id, LPCWSTR title, APPLICATION_CALLBACK callback);
#endif // APPLICATION_NO_SETTINGS

	CString GetGithubUrl ();
	CString GetWebsiteUrl ();

	CString GetDirectory ();
	CString GetProfileDirectory ();

	CString GetUserAgent ();

	INT GetDPI (INT val);
	HINSTANCE GetHINSTANCE ();
	HWND GetHWND ();

	VOID SetHWND (HWND hwnd);

	VOID Restart ();

	VOID LocaleEnum (HWND hwnd, INT ctrl_id);
	VOID LocaleInit (LPCWSTR name);
	CString LocaleString (HINSTANCE h, UINT id, LPCWSTR name);
	VOID LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position);

#ifdef APPLICATION_NO_UAC
	BOOL SkipUacCreate (BOOL is_remove);
	BOOL SkipUacRun ();
	BOOL SkipUacIsPresent (BOOL run);
#endif // APPLICATION_NO_UAC

private:

#ifndef APPLICATION_NO_ABOUT
	static LRESULT CALLBACK AboutWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // APPLICATION_NO_ABOUT

#ifndef APPLICATION_NO_SETTINGS
	static INT_PTR CALLBACK SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // APPLICATION_NO_SETTINGS

#ifndef APPLICATION_NO_UPDATES
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);
#endif // APPLICATION_NO_UPDATES

	BOOL ParseINI (LPCWSTR path, IniMap* map);

	VOID ConfigInit ();

	DOUBLE dpi_percent = 0.f;

	BOOL is_initialized = FALSE;
	BOOL is_localized = FALSE;
	BOOL app_update_mode = FALSE;

	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;
	HICON app_logo = nullptr;

	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];

	WCHAR app_author[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];

	IniMap app_config_array;
	IniMap app_locale_array;

#ifndef APPLICATION_NO_SETTINGS

	std::vector<PAPPLICATION_PAGE> app_settings_pages;
	size_t app_settings_page = 0;

#endif // APPLICATION_NO_SETTINGS
};
