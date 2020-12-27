// routine++
// Copyright (c) 2012-2021 Henry++

#pragma once

#include "routine.h"
#include "resource.h"

#include "rconfig.h"

// Structures

#if defined(APP_HAVE_SETTINGS)
typedef struct _APP_SETTINGS_PAGE
{
	HWND hwnd;
	UINT locale_id;
	INT dlg_id;
} APP_SETTINGS_PAGE, *PAPP_SETTINGS_PAGE;
#endif // APP_HAVE_SETTINGS

#if defined(APP_HAVE_UPDATES)
typedef struct _APP_UPDATE_COMPONENT
{
	PR_STRING full_name;
	PR_STRING short_name;
	PR_STRING version;
	PR_STRING new_version;
	PR_STRING temp_path;
	PR_STRING target_path;
	PR_STRING url;
	BOOLEAN is_installer;
	BOOLEAN is_haveupdate;
} APP_UPDATE_COMPONENT, *PAPP_UPDATE_COMPONENT;

typedef struct _APP_UPDATE_INFO
{
	PR_ARRAY components;
	HWND htaskdlg;
	HWND hparent;
	HANDLE hthread;
	BOOLEAN is_downloaded;
} APP_UPDATE_INFO, *PAPP_UPDATE_INFO;
#endif // APP_HAVE_UPDATES

typedef struct _APP_SHARED_IMAGE
{
	HINSTANCE hinst;
	HICON hicon;
	INT icon_id;
	INT icon_size;
} APP_SHARED_IMAGE, *PAPP_SHARED_IMAGE;

// Enums
typedef enum _LOG_LEVEL
{
	Critical = 5,
	Error = 4,
	Warning = 3,
	Information = 2,
	Debug = 1
} LOG_LEVEL, *PLOG_LEVEL;

// Global variables
DECLSPEC_SELECTANY PR_HASHTABLE app_config_array = NULL;

#if !defined(APP_CONSOLE)
DECLSPEC_SELECTANY HWND app_hwnd = NULL;
DECLSPEC_SELECTANY WNDPROC app_window_proc = NULL;

DECLSPEC_SELECTANY LONG app_window_minwidth = 0;
DECLSPEC_SELECTANY LONG app_window_minheight = 0;

DECLSPEC_SELECTANY BOOLEAN app_is_needmaximize = FALSE;

DECLSPEC_SELECTANY HANDLE app_mutex = NULL;
DECLSPEC_SELECTANY WCHAR app_mutex_name[128] = {0};

DECLSPEC_SELECTANY PR_STRING app_locale_current = NULL;
DECLSPEC_SELECTANY PR_STRING app_locale_default = NULL;
DECLSPEC_SELECTANY PR_STRING app_locale_resource = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE app_locale_array = NULL;
DECLSPEC_SELECTANY PR_LIST app_locale_names = NULL;

DECLSPEC_SELECTANY LONG64 app_locale_timetamp = 0;
#endif // !APP_CONSOLE

#if defined(APP_HAVE_UPDATES)
DECLSPEC_SELECTANY PAPP_UPDATE_INFO app_update_info = NULL;
DECLSPEC_SELECTANY PR_STRING app_update_path = NULL;
#endif // APP_HAVE_UPDATES

#if defined(APP_HAVE_SETTINGS)
DECLSPEC_SELECTANY PR_ARRAY app_settings_pages = NULL;
DECLSPEC_SELECTANY HWND app_settings_hwnd = NULL;
DECLSPEC_SELECTANY DLGPROC app_settings_proc = NULL;
#endif // APP_HAVE_SETTINGS

/*
	Configuration
*/

VOID _r_config_initialize ();

BOOLEAN _r_config_getbooleanex (_In_ LPCWSTR key, _In_ BOOLEAN def, _In_opt_ LPCWSTR name);
INT _r_config_getintegerex (_In_ LPCWSTR key, _In_ INT def, _In_opt_ LPCWSTR name);
UINT _r_config_getuintegerex (_In_ LPCWSTR key, UINT def, _In_opt_ LPCWSTR name);
LONG _r_config_getlongex (_In_ LPCWSTR key, LONG def, _In_opt_ LPCWSTR name);
LONG64 _r_config_getlong64ex (_In_ LPCWSTR key, LONG64 def, _In_opt_ LPCWSTR name);
ULONG _r_config_getulongex (_In_ LPCWSTR key, ULONG def, _In_opt_ LPCWSTR name);
ULONG64 _r_config_getulong64ex (_In_ LPCWSTR key, _In_ ULONG64 def, _In_opt_ LPCWSTR name);
VOID _r_config_getfont (_In_ LPCWSTR key, _In_ HWND hwnd, _Inout_ PLOGFONT logfont, _In_opt_ LPCWSTR name);

_Ret_maybenull_
LPCWSTR _r_config_getstringex (_In_ LPCWSTR key, _In_opt_ LPCWSTR def, _In_opt_ LPCWSTR name);

FORCEINLINE BOOLEAN _r_config_getboolean (_In_ LPCWSTR key, _In_ BOOLEAN def)
{
	return _r_config_getbooleanex (key, def, NULL);
}

FORCEINLINE INT _r_config_getinteger (_In_ LPCWSTR key, _In_ INT def)
{
	return _r_config_getintegerex (key, def, NULL);
}

FORCEINLINE UINT _r_config_getuinteger (_In_ LPCWSTR key, _In_ UINT def)
{
	return _r_config_getuintegerex (key, def, NULL);
}

FORCEINLINE LONG _r_config_getlong (_In_ LPCWSTR key, _In_ LONG def)
{
	return _r_config_getlongex (key, def, NULL);
}

FORCEINLINE LONG64 _r_config_getlong64 (_In_ LPCWSTR key, _In_ LONG64 def)
{
	return _r_config_getlong64ex (key, def, NULL);
}

FORCEINLINE ULONG _r_config_getulong (_In_ LPCWSTR key, _In_ ULONG def)
{
	return _r_config_getulongex (key, def, NULL);
}

FORCEINLINE ULONG64 _r_config_getulong64 (_In_ LPCWSTR key, _In_ ULONG64 def)
{
	return _r_config_getulong64ex (key, def, NULL);
}

_Ret_maybenull_
FORCEINLINE LPCWSTR _r_config_getstring (_In_ LPCWSTR key, _In_opt_ LPCWSTR def)
{
	return _r_config_getstringex (key, def, NULL);
}

VOID _r_config_setbooleanex (_In_ LPCWSTR key, _In_ BOOLEAN val, _In_opt_ LPCWSTR name);
VOID _r_config_setintegerex (_In_ LPCWSTR key, _In_ INT val, _In_opt_ LPCWSTR name);
VOID _r_config_setuintegerex (_In_ LPCWSTR key, _In_ UINT val, _In_opt_ LPCWSTR name);
VOID _r_config_setlongex (_In_ LPCWSTR key, _In_ LONG val, _In_opt_ LPCWSTR name);
VOID _r_config_setlong64ex (_In_ LPCWSTR key, _In_ LONG64 val, _In_opt_ LPCWSTR name);
VOID _r_config_setulongex (_In_ LPCWSTR key, _In_ ULONG val, _In_opt_ LPCWSTR name);
VOID _r_config_setulong64ex (_In_ LPCWSTR key, _In_ ULONG64 val, _In_opt_ LPCWSTR name);
VOID _r_config_setfont (_In_ LPCWSTR key, _In_ HWND hwnd, _In_ PLOGFONT logfont, _In_opt_ LPCWSTR name);
VOID _r_config_setstringex (_In_ LPCWSTR key, _In_opt_ LPCWSTR val, _In_opt_ LPCWSTR name);

FORCEINLINE VOID _r_config_setboolean (_In_ LPCWSTR key, _In_ BOOLEAN val)
{
	_r_config_setbooleanex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setinteger (_In_ LPCWSTR key, _In_ INT val)
{
	_r_config_setintegerex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setuinteger (_In_ LPCWSTR key, _In_ UINT val)
{
	_r_config_setuintegerex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setlong (_In_ LPCWSTR key, _In_ LONG val)
{
	_r_config_setlongex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setlong64 (_In_ LPCWSTR key, _In_ LONG64 val)
{
	_r_config_setlong64ex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setulong (_In_ LPCWSTR key, _In_ ULONG val)
{
	_r_config_setulongex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setulong64 (_In_ LPCWSTR key, _In_ ULONG64 val)
{
	_r_config_setulong64ex (key, val, NULL);
}

FORCEINLINE VOID _r_config_setstring (_In_ LPCWSTR key, _In_opt_ LPCWSTR val)
{
	_r_config_setstringex (key, val, NULL);
}

/*
	Localization
*/

#if !defined(APP_CONSOLE)
VOID _r_locale_initialize ();

#if defined(APP_HAVE_SETTINGS)
VOID _r_locale_applyfromcontrol (_In_ HWND hwnd, _In_ INT ctrl_id);
#endif // !APP_HAVE_SETTINGS

VOID _r_locale_applyfrommenu (_In_ HMENU hmenu, _In_ UINT selected_id);
VOID _r_locale_enum (_In_ PVOID hwnd, _In_ INT ctrl_id, _In_opt_ UINT menu_id);

FORCEINLINE SIZE_T _r_locale_getcount ()
{
	return _r_obj_gethashtablecount (app_locale_array);
}

FORCEINLINE LONG64 _r_locale_getversion ()
{
	return app_locale_timetamp;
}

_Ret_maybenull_
LPCWSTR _r_locale_getstring (_In_ UINT uid);
#endif // !APP_CONSOLE

/*
	Settings window
*/

#if defined(APP_HAVE_SETTINGS)
VOID _r_settings_addpage (_In_ INT dlg_id, _In_ UINT locale_id);
VOID _r_settings_createwindow (_In_ HWND hwnd, _In_opt_ DLGPROC dlg_proc, _In_opt_ INT dlg_id);
INT_PTR CALLBACK _r_settings_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

FORCEINLINE HWND _r_settings_getwindow ()
{
	return app_settings_hwnd;
}
#endif // APP_HAVE_SETTINGS

#if !defined(APP_CONSOLE)
BOOLEAN _r_mutex_create (_In_ LPCWSTR name, _Inout_ PHANDLE mutex);
BOOLEAN _r_mutex_destroy (_Inout_ PHANDLE mutex);
BOOLEAN _r_mutex_isexists (_In_ LPCWSTR name);
#endif // APP_CONSOLE

/*
	Autorun (optional feature)
*/

#if defined(APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ();
BOOLEAN _r_autorun_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable);
#endif // APP_HAVE_AUTORUN

/*
	Skip UAC (optional feature)
*/

#if defined(APP_HAVE_SKIPUAC)
BOOLEAN _r_skipuac_isenabled ();
HRESULT _r_skipuac_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable);
BOOLEAN _r_skipuac_run ();
#endif // APP_HAVE_SKIPUAC

/*
	Update checker (optional feature)
*/

#if defined(APP_HAVE_UPDATES)
VOID _r_update_addcomponent (_In_opt_ LPCWSTR full_name, _In_opt_ LPCWSTR short_name, _In_opt_ LPCWSTR version, _In_opt_ LPCWSTR target_path, _In_ BOOLEAN is_installer);
VOID _r_update_check (_In_opt_ HWND hparent);
THREAD_API _r_update_checkthread (PVOID lparam);

BOOLEAN NTAPI _r_update_downloadcallback (ULONG total_written, ULONG total_length, PVOID pdata);
THREAD_API _r_update_downloadthread (PVOID lparam);

HRESULT CALLBACK _r_update_pagecallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR pdata);
INT _r_update_pagenavigate (_In_opt_ HWND htaskdlg, _In_opt_ LPCWSTR main_icon, _In_ TASKDIALOG_FLAGS flags, _In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons, _In_opt_ LPCWSTR main, _In_opt_ LPCWSTR content, _In_opt_ LONG_PTR lpdata);

VOID _r_update_install ();

FORCEINLINE LPCWSTR _r_update_getpath ()
{
	return _r_obj_getstring (app_update_path);
}
#endif // APP_HAVE_UPDATES

VOID _r_log (_In_ LOG_LEVEL log_level, _In_ UINT tray_id, _In_ LPCWSTR fn, _In_ ULONG code, _In_opt_ LPCWSTR description);
VOID _r_log_v (_In_ LOG_LEVEL log_level, _In_ UINT tray_id, _In_ LPCWSTR fn, _In_ ULONG code, _In_ LPCWSTR format, ...);

#if !defined(APP_CONSOLE)
VOID _r_show_aboutmessage (_In_opt_ HWND hwnd);
VOID _r_show_errormessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ ULONG errcode, _In_opt_ LPCWSTR description, _In_opt_ HINSTANCE hmodule);
BOOLEAN _r_show_confirmmessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ LPCWSTR text, _In_opt_ LPCWSTR config_key);
INT _r_show_message (_In_opt_ HWND hwnd, _In_ ULONG flags, _In_opt_ LPCWSTR title, _In_opt_ LPCWSTR main, _In_ LPCWSTR content);

VOID _r_window_restoreposition (_In_ HWND hwnd, _In_ LPCWSTR window_name);
VOID _r_window_saveposition (_In_ HWND hwnd, _In_ LPCWSTR window_name);
#endif // APP_CONSOLE

/*
	Application
*/

BOOLEAN _r_app_initialize ();

LPCWSTR _r_app_getdirectory ();
LPCWSTR _r_app_getconfigpath ();

#if !defined(APP_CONSOLE)
LPCWSTR _r_app_getlocalepath ();
#endif // !APP_CONSOLE

LPCWSTR _r_app_getlogpath ();
LPCWSTR _r_app_getprofiledirectory ();
LPCWSTR _r_app_getuseragent ();

#if !defined(APP_CONSOLE)
BOOLEAN _r_app_createwindow (_In_ INT dlg_id, _In_opt_ INT icon_id, _In_opt_ DLGPROC dlg_proc);
LRESULT CALLBACK _r_app_maindlgproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

_Ret_maybenull_
HICON _r_app_getsharedimage (_In_opt_ HINSTANCE hinst, _In_ INT icon_id, _In_ INT icon_size);

BOOLEAN _r_app_runasadmin ();
VOID _r_app_restart (_In_opt_ HWND hwnd);
#endif // !APP_CONSOLE

FORCEINLINE LPCWSTR _r_app_getname ()
{
	return APP_NAME;
}

FORCEINLINE LPCWSTR _r_app_getnameshort ()
{
	return APP_NAME_SHORT;
}

FORCEINLINE LPCWSTR _r_app_getauthor ()
{
	return APP_AUTHOR;
}

FORCEINLINE LPCWSTR _r_app_getcopyright ()
{
	return APP_COPYRIGHT;
}

FORCEINLINE LPCWSTR _r_app_getdonate_url ()
{
	return L"https://www.henrypp.org/donate?from=" APP_NAME_SHORT;
}

FORCEINLINE LPCWSTR _r_app_getsources_url ()
{
	return L"https://github.com/henrypp";
}

FORCEINLINE LPCWSTR _r_app_getwebsite_url ()
{
	return L"https://www.henrypp.org";
}

FORCEINLINE LPCWSTR _r_app_getversiontype ()
{
#if defined(_DEBUG) || defined(APP_BETA)
	return L"Pre-release";
#else
	return L"Release";
#endif // _DEBUG || APP_BETA
}

FORCEINLINE INT _r_app_getarch ()
{
#ifdef _WIN64
	return 64;
#else
	return 32;
#endif // _WIN64
}

FORCEINLINE LPCWSTR _r_app_getversion ()
{
	return APP_VERSION;
}

#if !defined(APP_CONSOLE)
FORCEINLINE HWND _r_app_gethwnd ()
{
	return app_hwnd;
}
#endif // !APP_CONSOLE

#if !defined(APP_CONSOLE)
FORCEINLINE BOOLEAN _r_app_isclassicui ()
{
	return !IsAppThemed () || _r_config_getboolean (L"ClassicUI", APP_CLASSICUI);
}
#endif // !APP_CONSOLE
