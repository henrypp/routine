// routine++
// Copyright (c) 2012-2018 Henry++

#pragma once

#include "routine.hpp"
#include "resource.hpp"

#include <comdef.h>
#include <taskschd.h>

#include "rconfig.hpp"

// libs
#ifdef _APP_HAVE_SKIPUAC
#pragma comment(lib, "taskschd.lib")
#endif // _APP_HAVE_SKIPUAC

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
	HTREEITEM item = nullptr;
	HWND hwnd = nullptr;

	UINT dlg_id = 0;
	UINT locale_id = 0;

	size_t group_id = 0;
} *PAPP_SETTINGS_PAGE, APP_SETTINGS_PAGE;
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
	bool AutorunEnable (bool is_enable);
	bool AutorunIsEnabled ();
#endif // _APP_HAVE_AUTORUN

#ifndef _APP_NO_UPDATES
	void CheckForUpdates (bool is_periodical);
#endif // _APP_NO_UPDATES

	rstring ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr);

	bool ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, bool val, LPCWSTR name = nullptr);

	void ConfigInit ();

	bool ConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg);

#ifndef _APP_NO_ABOUT
	void CreateAboutWindow (HWND hwnd, LPCWSTR donate_text);
#endif // _APP_NO_ABOUT

	bool CreateMainWindow (UINT dlg_id, UINT icon_id, DLGPROC proc, APPLICATION_CALLBACK callback);

#ifdef _APP_HAVE_TRAY
	bool TrayCreate (HWND hwnd, UINT uid, UINT code, HICON hicon, bool is_hidden);
	bool TrayDestroy (UINT uid);
	bool TrayPopup (UINT uid, DWORD icon_id, LPCWSTR title, LPCWSTR text);
	bool TraySetInfo (UINT uid, HICON h, LPCWSTR tooltip);
	bool TrayToggle (UINT uid, bool is_show);
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_SETTINGS
	size_t AddSettingsPage (UINT dlg_id, UINT locale_id, APPLICATION_CALLBACK callback, size_t group_id = LAST_VALUE);
	void CreateSettingsWindow (size_t dlg_id = LAST_VALUE);

	HWND SettingsGetWindow ();
	void SettingsInitialize ();
	void SettingsPageInitialize (UINT dlg_id, bool is_initialize, bool is_localize);
#endif // _APP_NO_SETTINGS

	LPCWSTR GetBinaryPath () const;
	LPCWSTR GetDirectory () const;
	LPCWSTR GetProfileDirectory () const;

	rstring GetUserAgent () const;

	INT GetDPI (INT v) const;
	HICON GetHICON (bool is_big) const;
	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;

	void SetIcon (HWND hwnd, UINT icon_id, bool is_forced);

	bool IsAdmin () const;
	bool IsClassicUI () const;
	bool IsVistaOrLater () const;

	void LocaleApplyFromControl (HWND hwnd, UINT ctrl_id);
	void LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);
	void LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, const UINT id_start);
	size_t LocaleGetCount ();
	rstring LocaleString (UINT id, LPCWSTR append);
	void LocaleMenu (HMENU menu, UINT id, UINT item, bool by_position, LPCWSTR append);

#ifdef _APP_HAVE_SKIPUAC
	bool SkipUacEnable (bool is_enable);
	bool SkipUacIsEnabled ();
	bool SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC

	bool RunAsAdmin ();

private:

#ifndef _APP_NO_SETTINGS
	static INT_PTR CALLBACK SettingsPageProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
	static UINT WINAPI CheckForUpdatesProc (LPVOID lparam);
#endif // _APP_NO_UPDATES

	static LRESULT CALLBACK MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static BOOL CALLBACK ActivateWindowCallback (HWND hwnd, LPARAM lparam);

	bool ParseINI (LPCWSTR path, rstring::map_two* pmap, std::vector<rstring>* psections);

	void LocaleInit ();

	DOUBLE dpi_percent = 0.f;

	bool is_classic = false;
	bool is_vistaorlater = false;
	bool is_admin = false;
	bool is_needmaximize = false;

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

	INT max_width = 0;
	INT max_height = 0;

	WNDPROC app_wndproc = nullptr;
	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;
	APPLICATION_CALLBACK app_callback = nullptr;

	HICON app_icon_small = nullptr;
	HICON app_icon_big = nullptr;

	WCHAR app_binary[MAX_PATH];
	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];
	WCHAR app_useragent[MAX_PATH];

	WCHAR locale_default[LOCALE_NAME_MAX_LENGTH];
	WCHAR locale_current[LOCALE_NAME_MAX_LENGTH];

	rstring::map_two app_config_array;
	rstring::map_two app_locale_array;
	std::vector<rstring> app_locale_names;

#ifndef _APP_NO_SETTINGS
	std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
	APPLICATION_CALLBACK app_settings_callback;
	size_t settings_page_id = 0;
	HWND settings_hwnd = nullptr;
#endif // _APP_NO_SETTINGS
};
