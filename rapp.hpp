// routine++
// Copyright (c) 2012-2020 Henry++

#pragma once

#include "routine.hpp"
#include "resource.hpp"

#if defined(_APP_HAVE_SKIPUAC)
#include <comdef.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#endif // _APP_HAVE_SKIPUAC

#include "rconfig.hpp"

/*
	Structures
*/

#if defined(_APP_HAVE_SETTINGS)
typedef struct _APP_SETTINGS_PAGE
{
	HWND hwnd = nullptr;

	INT dlg_id = 0;
	UINT locale_id = 0;

} *PAPP_SETTINGS_PAGE, APP_SETTINGS_PAGE;
#endif // _APP_HAVE_SETTINGS

#if defined(_APP_HAVE_UPDATES)
typedef struct _APP_UPDATE_COMPONENT
{
	~_APP_UPDATE_COMPONENT ()
	{
		SAFE_DELETE_ARRAY (full_name);
		SAFE_DELETE_ARRAY (short_name);
		SAFE_DELETE_ARRAY (version);
		SAFE_DELETE_ARRAY (new_version);
		SAFE_DELETE_ARRAY (target_path);
		SAFE_DELETE_ARRAY (url);
		SAFE_DELETE_ARRAY (filepath);
	}

	LPWSTR full_name = nullptr;
	LPWSTR short_name = nullptr;
	LPWSTR version = nullptr;
	LPWSTR new_version = nullptr;
	LPWSTR target_path = nullptr;
	LPWSTR url = nullptr;
	LPWSTR filepath = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hend = nullptr;

	bool is_installer = false;
	bool is_downloaded = false;
	bool is_haveupdate = false;
} *PAPP_UPDATE_COMPONENT, APP_UPDATE_COMPONENT;

typedef struct _APP_UPDATE_INFO
{
	~_APP_UPDATE_INFO ()
	{
		for (size_t i = 0; i < components.size (); i++)
			SAFE_DELETE (components.at (i));
	}

	bool is_downloaded = false;

	std::vector<PAPP_UPDATE_COMPONENT> components;

	HWND htaskdlg = nullptr;
	HWND hparent = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hend = nullptr;

	LPVOID papp = nullptr;
} *PAPP_UPDATE_INFO, APP_UPDATE_INFO;
#endif // _APP_HAVE_UPDATES

#if defined(_APP_HAVE_SKIPUAC)
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

	rapp ();
	~rapp ();

	bool Initialize (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);

#if !defined(_APP_CONSOLE)
	bool MutexCreate ();
	bool MutexDestroy ();
	bool MutexIsExists (bool activate_window);
#endif // _APP_CONSOLE

#if defined(_APP_HAVE_AUTORUN)
	bool AutorunIsEnabled ();
	LSTATUS AutorunEnable (HWND hwnd, bool is_enable);
#endif // _APP_HAVE_AUTORUN

#if defined(_APP_HAVE_UPDATES)
	void UpdateAddComponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, bool is_installer);
	void UpdateCheck (HWND hparent);
	void UpdateInstall () const;
#endif // _APP_HAVE_UPDATES

	rstring ConfigGet (LPCWSTR key, bool def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, INT def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, UINT def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, LONG def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, ULONG def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, LONG64 def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, ULONG64 def, LPCWSTR name = nullptr) const;
	rstring ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name = nullptr) const;

	bool ConfigSet (LPCWSTR key, bool val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, INT val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, UINT val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, ULONG val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LONG64 val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, ULONG64 val, LPCWSTR name = nullptr);
	bool ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name = nullptr);

	void ConfigInit ();

	void LogError (LPCWSTR fn, DWORD errcode, LPCWSTR desc, UINT tray_id);

#if !defined(_APP_CONSOLE)
	bool ShowConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg);
	void ShowErrorMessage (HWND hwnd, LPCWSTR main, DWORD errcode, HINSTANCE hmodule);
	INT ShowMessage (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR content) const;

	void CreateAboutWindow (HWND hwnd);

	bool CreateMainWindow (INT dlg_id, INT icon_id, DLGPROC dlg_proc);
	void RestoreWindowPosition (HWND hwnd, LPCWSTR window_name);
#endif // _APP_CONSOLE

#if defined(_APP_HAVE_SETTINGS)
	void CreateSettingsWindow (HWND hwnd, DLGPROC dlg_proc, INT dlg_id = 0);
	void SettingsAddPage (INT dlg_id, UINT locale_id);

	HWND GetSettingsWindow ();
#endif // _APP_HAVE_SETTINGS

	LPCWSTR GetBinaryPath () const;
	LPCWSTR GetDirectory () const;
	LPCWSTR GetProfileDirectory () const;

	LPCWSTR GetConfigPath () const;
	LPCWSTR GetLogPath () const;

#if !defined(_APP_CONSOLE)
	LPCWSTR GetLocalePath () const;
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_UPDATES)
	LPCWSTR GetUpdatePath () const;
#endif // _APP_HAVE_UPDATES

	rstring GetProxyConfiguration () const;
	rstring GetUserAgent ();

	HINSTANCE GetHINSTANCE () const;
	HWND GetHWND () const;
	HICON GetSharedImage (HINSTANCE hinst, INT icon_id, INT icon_size);

#if !defined(_APP_CONSOLE)
	bool IsClassicUI () const;
#endif // !_APP_CONSOLE

#if !defined(_APP_NO_WINXP)
	bool IsVistaOrLater () const;
#endif // _APP_NO_WINXP

#if !defined(_APP_CONSOLE)
#if defined(_APP_HAVE_SETTINGS)
	void LocaleApplyFromControl (HWND hwnd, INT ctrl_id);
#endif // _APP_HAVE_SETTINGS

	void LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id);

	void LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, UINT id_start);
	size_t LocaleGetCount () const;
	time_t LocaleGetVersion () const;
	rstring LocaleString (UINT uid, LPCWSTR append);
	void LocaleMenu (HMENU hmenu, UINT uid, UINT item, BOOL by_position, LPCWSTR append);
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_SKIPUAC) && !defined(_APP_CONSOLE)
	bool SkipUacIsEnabled () const;
	HRESULT SkipUacEnable (HWND hwnd, bool is_enable);
	bool SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC && !_APP_CONSOLE

#if !defined(_APP_CONSOLE)
	void Restart (HWND hwnd);
	bool RunAsAdmin ();
#endif // !_APP_CONSOLE

private:

#if defined(_APP_HAVE_SETTINGS)
	static INT_PTR CALLBACK SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_HAVE_SETTINGS

#if defined(_APP_HAVE_UPDATES)
	static bool UpdateDownloadCallback (DWORD total_written, DWORD total_length, LONG_PTR lpdata);
	static UINT WINAPI UpdateDownloadThread (LPVOID lparam);
	static HRESULT CALLBACK UpdateDialogCallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);
	static UINT WINAPI UpdateCheckThread (LPVOID lparam);
	INT UpdateDialogNavigate (HWND htaskdlg, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata);
#endif // _APP_HAVE_UPDATES

#if !defined(_APP_CONSOLE)
	static LRESULT CALLBACK MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void LocaleInit ();
#endif

#if defined(_APP_HAVE_UPDATES)
	PAPP_UPDATE_INFO pupdateinfo = nullptr;
#endif // _APP_HAVE_UPDATES

#if !defined(_APP_NO_WINXP)
	bool is_vistaorlater = false;
#endif // !_APP_NO_WINXP

	bool is_needmaximize = false;

	LONG max_width = 0;
	LONG max_height = 0;

	WNDPROC app_wndproc = nullptr;
	HWND app_hwnd = nullptr;

#if !defined(_APP_CONSOLE)
	HANDLE app_mutex = nullptr;
#endif // _APP_CONSOLE

	HINSTANCE app_hinstance = nullptr;

	WCHAR app_binary[MAX_PATH];
	WCHAR app_profile_directory[MAX_PATH];
	WCHAR app_config_path[MAX_PATH];

	LPWSTR app_name = nullptr;
	LPWSTR app_name_short = nullptr;
	LPWSTR app_version = nullptr;
	LPWSTR app_copyright = nullptr;

	LPWSTR app_directory = nullptr;
	LPWSTR app_logpath = nullptr;

#if defined(_APP_HAVE_UPDATES)
	LPWSTR app_updatepath = nullptr;
#endif // _APP_HAVE_UPDATES

#if !defined(_APP_CONSOLE)
	LPWSTR app_localepath = nullptr;
	LPWSTR app_locale_current = nullptr;

	WCHAR locale_default[LOCALE_NAME_MAX_LENGTH];

	rstringmap2 app_locale_array;
	rstringvec app_locale_names;

	time_t app_locale_timetamp = 0;
#endif // !_APP_CONSOLE

	rstringmap2 app_config_array;
	std::vector<PAPP_SHARED_IMAGE> app_shared_icons;

#if defined(_APP_HAVE_SETTINGS)
	std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
	HWND settings_hwnd = nullptr;
	DLGPROC app_settings_proc = nullptr;
#endif // _APP_HAVE_SETTINGS
};
