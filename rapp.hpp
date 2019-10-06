// routine++
// Copyright (c) 2012-2019 Henry++

#pragma once

#include "routine.hpp"
#include "resource.hpp"

#ifdef _APP_HAVE_SKIPUAC
#include <comdef.h>
#include <taskschd.h>
#endif // _APP_HAVE_SKIPUAC

#include "rconfig.hpp"

// libs
#ifdef _APP_HAVE_SKIPUAC
#pragma comment(lib, "taskschd.lib")
#endif // _APP_HAVE_SKIPUAC

/*
	Structures
*/

#ifndef _APP_NO_SETTINGS
typedef struct _APP_SETTINGS_PAGE
{
	HTREEITEM hitem = nullptr;
	HWND hwnd = nullptr;

	INT dlg_id = 0;
	UINT locale_id = 0;

	INT group_id = 0;
} *PAPP_SETTINGS_PAGE, APP_SETTINGS_PAGE;
#endif // _APP_NO_SETTINGS

#ifdef _APP_HAVE_UPDATES
typedef struct _APP_UPDATE_COMPONENT
{
	bool is_downloaded = false;
	bool is_installer = false;
	bool is_haveupdates = false;

	LPWSTR full_name = nullptr;
	LPWSTR short_name = nullptr;
	LPWSTR version = nullptr;
	LPWSTR new_version = nullptr;
	LPWSTR target_path = nullptr;
	LPWSTR url = nullptr;
	LPWSTR filepath = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hend = nullptr;
} *PAPP_UPDATE_COMPONENT, APP_UPDATE_COMPONENT;

typedef struct
{
	bool is_downloaded = false;
	bool is_forced = false;

	std::vector<PAPP_UPDATE_COMPONENT> components;

	HWND hwnd = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hend = nullptr;

	LPVOID papp = nullptr;
} *PAPP_UPDATE_INFO, APP_UPDATE_INFO;
#endif // _APP_HAVE_UPDATES

#ifdef _APP_HAVE_SKIPUAC
struct MBSTR
{
	MBSTR (LPCWSTR asString = nullptr)
	{
		if (asString)
			ms_bstr = SysAllocString (asString);
	}

	~MBSTR ()
	{
		Free ();
	}

	operator BSTR() const
	{
		return ms_bstr;
	}

	MBSTR& operator=(LPCWSTR asString)
	{
		if (asString != ms_bstr)
		{
			Free ();
			ms_bstr = asString ? SysAllocString (asString) : nullptr;
		}

		return *this;
	}

	void Free ()
	{
		if (ms_bstr)
		{
			SysFreeString (ms_bstr);
			ms_bstr = nullptr;
		}
	}
protected:
	BSTR ms_bstr = nullptr;
};
#endif // _APP_HAVE_SKIPUAC

typedef struct _APP_SHARED_IMAGE
{
	HINSTANCE hinst = nullptr;
	HICON hicon = nullptr;
	INT icon_id = 0;
	INT icon_size = 0;
} *PAPP_SHARED_IMAGE, APP_SHARED_IMAGE;

/*
	Application class
*/

class rapp
{

public:

	rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);

	bool MutexCreate ();
	bool MutexDestroy ();
	bool MutexIsExists (bool activate_window);

#ifdef _APP_HAVE_AUTORUN
	bool AutorunIsEnabled ();
	bool AutorunEnable (bool is_enable);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_UPDATES
	void UpdateAddComponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, bool is_installer);
	bool UpdateCheck (bool is_forced);
	void UpdateInstall () const;
#endif // _APP_HAVE_UPDATES

	rstring ConfigGet (LPCWSTR key, bool def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, UINT def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, LONG def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, ULONG def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, LONG64 def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, ULONG64 def, LPCWSTR name = nullptr);
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr);

	bool ConfigSet (LPCWSTR key, bool val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, INT val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, UINT val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, ULONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONG64 val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, ULONG64 val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);

	void ConfigInit ();

	bool ConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg);

#ifndef _APP_NO_ABOUT
	void CreateAboutWindow (HWND hwnd);
#endif // _APP_NO_ABOUT

	bool CreateMainWindow (INT dlg_id, INT icon_id, DLGPROC proc);
	void RestoreWindowPosition (HWND hwnd, LPCWSTR window_name);

#ifdef _APP_HAVE_SETTINGS
	void CreateSettingsWindow (DLGPROC proc, INT dlg_id = INVALID_INT);

	INT SettingsAddPage (INT dlg_id, UINT locale_id, INT group_id = INVALID_INT);
	HWND GetSettingsWindow ();
	void SettingsInitialize ();
#endif // _APP_HAVE_SETTINGS

	LPCWSTR GetBinaryPath () const;
	LPCWSTR GetDirectory () const;
	LPCWSTR GetProfileDirectory () const;

	LPCWSTR GetConfigPath () const;
	LPCWSTR GetLocalePath () const;

#ifdef _APP_HAVE_UPDATES
	LPCWSTR GetUpdatePath () const;
#endif // _APP_HAVE_UPDATES

	rstring GetProxyConfiguration ();
	rstring GetUserAgent ();

	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;
	HICON GetSharedImage (HINSTANCE hinst, INT icon_id, INT icon_size);

	bool IsAdmin () const;
	bool IsClassicUI () const;

#ifndef _APP_NO_WINXP
	bool IsVistaOrLater () const;
#endif // _APP_NO_WINXP

	void LocaleApplyFromControl (HWND hwnd, INT ctrl_id);
	void LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);
	void LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, UINT id_start);
	size_t LocaleGetCount () const;
	time_t LocaleGetVersion () const;
	rstring LocaleString (UINT uid, LPCWSTR append);
	void LocaleMenu (HMENU menu, UINT id, UINT item, bool by_position, LPCWSTR append);

#ifdef _APP_HAVE_SKIPUAC
	bool SkipUacIsEnabled ();
	bool SkipUacEnable (bool is_enable);
	bool SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC

	bool RunAsAdmin ();

private:

#ifdef _APP_HAVE_SETTINGS
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_HAVE_SETTINGS

#ifdef _APP_HAVE_UPDATES
	static bool UpdateDownloadCallback (DWORD total_written, DWORD total_length, LONG_PTR lpdata);
	static UINT WINAPI UpdateDownloadThread (LPVOID lparam);
	static HRESULT CALLBACK UpdateDialogCallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);
	INT UpdateDialogNavigate (HWND hwnd, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata);
	static UINT WINAPI UpdateCheckThread (LPVOID lparam);
#endif // _APP_HAVE_UPDATES

	static LRESULT CALLBACK MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static BOOL CALLBACK ActivateWindowCallback (HWND hwnd, LPARAM lparam);

	void LocaleInit ();

#ifdef _APP_HAVE_UPDATES
	PAPP_UPDATE_INFO pupdateinfo;
#endif // _APP_HAVE_UPDATES

#ifdef _APP_HAVE_DARKTHEME
	bool is_darktheme = false;
#endif // _APP_HAVE_DARKTHEME

#ifndef _APP_NO_WINXP
	bool is_vistaorlater = false;
#endif // _APP_NO_WINXP

	bool is_classic = false;
	bool is_admin = false;
	bool is_needmaximize = false;

	LONG max_width = 0;
	LONG max_height = 0;

	WNDPROC app_wndproc = nullptr;
	HWND app_hwnd = nullptr;
	HANDLE app_mutex = nullptr;
	HINSTANCE app_hinstance = nullptr;

	WCHAR app_binary[MAX_PATH];
	WCHAR app_directory[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	WCHAR app_name[MAX_PATH];
	WCHAR app_name_short[MAX_PATH];
	WCHAR app_version[MAX_PATH];
	WCHAR app_copyright[MAX_PATH];
	WCHAR app_localepath[MAX_PATH];

#ifdef _APP_HAVE_UPDATES
	WCHAR app_updatepath[MAX_PATH];
#endif // _APP_HAVE_UPDATES

	WCHAR locale_default[LOCALE_NAME_MAX_LENGTH];
	WCHAR locale_current[LOCALE_NAME_MAX_LENGTH];

	rstringmap2 app_config_array;
	rstringmap2 app_locale_array;
	rstringvec app_locale_names;
	std::vector<PAPP_SHARED_IMAGE> app_shared_icons;

	time_t app_locale_timetamp = 0;

#ifdef _APP_HAVE_SETTINGS
	std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
	size_t settings_page_id = 0;
	HWND settings_hwnd = nullptr;
	DLGPROC app_settings_proc = nullptr;
#endif // _APP_HAVE_SETTINGS
};
