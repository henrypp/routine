// application support
// © 2013-2015 Henry++
//
// lastmod: Oct 3, 2015

#pragma once

#define _WIN32_DCOM

#include <windows.h>
#include <wininet.h>
#include <commctrl.h>
#include <lm.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <process.h>
#include <time.h>
#include <math.h>
#include <atlstr.h>
#include <strsafe.h>
#include <comdef.h>
#include <wincred.h>
#include <taskschd.h>

#include "routine.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "credui.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "wininet.lib")

//#define APPLICATION_NEED_PRIVILEGES // application needs high privileges
#define APPLICATION_TASKSCHD_NAME L"%sSkipUAC"
#define APPLICATION_UPDATE_PERIOD 2 // update checking period (in days)

class CApplication
{
public:

	CApplication (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR author);
	~CApplication ();

	VOID AutorunCreate (BOOL is_remove);
	BOOL AutorunIsPresent ();

	VOID CheckForUpdates (BOOL is_periodical);

	UINT ConfigGet (LPCWSTR key, INT def);
	CString ConfigGet (LPCWSTR key, LPCWSTR def);

	BOOL ConfigSet (LPCWSTR key, LPCWSTR val);
	BOOL ConfigSet (LPCWSTR key, DWORD val);

	VOID CreateAboutWindow ();
	BOOL CreateMainWindow (DLGPROC proc);

	HWND GetHWND ();

	VOID Restart ();

	VOID SetLinks (LPCWSTR website, LPCWSTR github);
	VOID SetCopyright (LPCWSTR copyright);

	VOID LocaleSet (LPCWSTR name);
	CString LocaleString (UINT id);

private:

	static INT_PTR CALLBACK AboutWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);

	BOOL is_initialized = FALSE;

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

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];

	WCHAR app_author[MAX_PATH];

	WCHAR app_website[MAX_PATH];
	WCHAR app_github[MAX_PATH];

	WCHAR app_copyright[MAX_PATH];

	WCHAR app_config_path[MAX_PATH];
	WCHAR app_locale[MAX_PATH];
};
