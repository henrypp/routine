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
	HWND hwnd;
	UINT locale_id;
	INT dlg_id;
} APP_SETTINGS_PAGE, *PAPP_SETTINGS_PAGE;
#endif // _APP_HAVE_SETTINGS

#if defined(_APP_HAVE_UPDATES)
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
	std::vector<PAPP_UPDATE_COMPONENT> *components;
	HWND htaskdlg;
	HWND hparent;
	HANDLE hthread;
	BOOLEAN is_downloaded;
} APP_UPDATE_INFO, *PAPP_UPDATE_INFO;
#endif // _APP_HAVE_UPDATES

typedef struct _APP_SHARED_IMAGE
{
	HINSTANCE hinst;
	HICON hicon;
	INT icon_id;
	INT icon_size;
} APP_SHARED_IMAGE, *PAPP_SHARED_IMAGE;

// Global variables
inline OBJECTS_STRINGS_MAP2 app_config_array;

inline PR_STRING app_name = NULL;
inline PR_STRING app_name_short = NULL;
inline PR_STRING app_copyright = NULL;
inline PR_STRING app_version = NULL;
inline PR_STRING app_directory = NULL;
inline PR_STRING app_profile_directory = NULL;
inline PR_STRING app_config_path = NULL;
inline PR_STRING app_log_path = NULL;
inline PR_STRING app_useragent = NULL;

#if !defined(_APP_CONSOLE)
inline HWND app_hwnd = NULL;
inline WNDPROC app_window_proc = NULL;

inline LONG app_window_minwidth = 0;
inline LONG app_window_minheight = 0;

inline BOOLEAN is_needmaximize = FALSE;

inline HANDLE app_mutex = NULL;
inline PR_STRING app_mutex_name = NULL;

inline PR_STRING app_locale_path = NULL;
inline PR_STRING app_locale_current = NULL;
inline PR_STRING app_locale_default = NULL;
inline PR_STRING app_locale_resource = NULL;

inline OBJECTS_STRINGS_VEC app_locale_names;
inline OBJECTS_STRINGS_MAP2 app_locale_array;

inline time_t app_locale_timetamp = 0;

inline std::vector<PAPP_SHARED_IMAGE> app_shared_icons;
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_UPDATES)
inline PAPP_UPDATE_INFO app_update_info = NULL;
inline PR_STRING app_update_path = NULL;
#endif // _APP_HAVE_UPDATES

#if defined(_APP_HAVE_SETTINGS)
inline std::vector<PAPP_SETTINGS_PAGE> app_settings_pages;
inline HWND app_settings_hwnd = NULL;
inline DLGPROC app_settings_proc = NULL;
#endif // _APP_HAVE_SETTINGS

/*
	Config
*/

VOID _r_config_initialize ();

BOOLEAN _r_config_getboolean (LPCWSTR key, BOOLEAN def, LPCWSTR name = NULL);
INT _r_config_getinteger (LPCWSTR key, INT def, LPCWSTR name = NULL);
UINT _r_config_getuinteger (LPCWSTR key, UINT def, LPCWSTR name = NULL);
LONG _r_config_getlong (LPCWSTR key, LONG def, LPCWSTR name = NULL);
LONG64 _r_config_getlong64 (LPCWSTR key, LONG64 def, LPCWSTR name = NULL);
ULONG _r_config_getulong (LPCWSTR key, ULONG def, LPCWSTR name = NULL);
ULONG64 _r_config_getulong64 (LPCWSTR key, ULONG64 def, LPCWSTR name = NULL);
VOID _r_config_getfont (LPCWSTR key, HWND hwnd, PLOGFONT pfont, LPCWSTR name = NULL);
LPCWSTR _r_config_getstring (LPCWSTR key, LPCWSTR def, LPCWSTR name = NULL);

VOID _r_config_setboolean (LPCWSTR key, BOOLEAN val, LPCWSTR name = NULL);
VOID _r_config_setinteger (LPCWSTR key, INT val, LPCWSTR name = NULL);
VOID _r_config_setuinteger (LPCWSTR key, UINT val, LPCWSTR name = NULL);
VOID _r_config_setlong (LPCWSTR key, LONG val, LPCWSTR name = NULL);
VOID _r_config_setlong64 (LPCWSTR key, LONG64 val, LPCWSTR name = NULL);
VOID _r_config_setulong (LPCWSTR key, ULONG val, LPCWSTR name = NULL);
VOID _r_config_setulong64 (LPCWSTR key, ULONG64 val, LPCWSTR name = NULL);
VOID _r_config_setfont (LPCWSTR key, HWND hwnd, PLOGFONT pfont, LPCWSTR name = NULL);
VOID _r_config_setstring (LPCWSTR key, LPCWSTR val, LPCWSTR name = NULL);

FORCEINLINE LPCWSTR _r_config_getpath ()
{
	return _r_obj_getstring (app_config_path);
}

/*
	Locale
*/

#if !defined(_APP_CONSOLE)
VOID _r_locale_initialize ();

#if defined(_APP_HAVE_SETTINGS)
VOID _r_locale_applyfromcontrol (HWND hwnd, INT ctrl_id);
#endif // !_APP_HAVE_SETTINGS

VOID _r_locale_applyfrommenu (HMENU hmenu, UINT selected_id);
VOID _r_locale_enum (HWND hwnd, INT ctrl_id, UINT menu_id);

FORCEINLINE LPCWSTR _r_locale_getpath ()
{
	return _r_obj_getstring (app_locale_path);
}

FORCEINLINE SIZE_T _r_locale_getcount ()
{
	return app_locale_array.size ();
}

FORCEINLINE time_t _r_locale_getversion ()
{
	return app_locale_timetamp;
}

LPCWSTR _r_locale_getstring (UINT uid);
#endif // !_APP_CONSOLE

/*
	Settings window
*/

#if defined(_APP_HAVE_SETTINGS)
VOID _r_settings_addpage (INT dlg_id, UINT locale_id);
VOID _r_settings_createwindow (HWND hwnd, DLGPROC dlg_proc, INT dlg_id = 0);
INT_PTR CALLBACK _r_settings_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

FORCEINLINE HWND _r_settings_getwindow ()
{
	return app_settings_hwnd;
}
#endif // _APP_HAVE_SETTINGS

#if !defined(_APP_CONSOLE)
BOOLEAN _r_mutex_create (PR_STRING name, PHANDLE pmutex);
BOOLEAN _r_mutex_destroy (PHANDLE pmutex);
BOOLEAN _r_mutex_isexists (PR_STRING name);
#endif // _APP_CONSOLE

/*
	Autorun (optional feature)
*/

#if defined(_APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ();
BOOLEAN _r_autorun_enable (HWND hwnd, BOOLEAN is_enable);
#endif // _APP_HAVE_AUTORUN

/*
	Skip UAC (optional feature)
*/

#if defined(_APP_HAVE_SKIPUAC)
BOOLEAN _r_skipuac_isenabled ();
HRESULT _r_skipuac_enable (HWND hwnd, BOOLEAN is_enable);
BOOLEAN _r_skipuac_run ();
#endif // _APP_HAVE_SKIPUAC

/*
	Update checker (optional feature)
*/

#if defined(_APP_HAVE_UPDATES)
VOID _r_update_addcomponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, BOOLEAN is_installer);
VOID _r_update_check (HWND hparent);
THREAD_FN _r_update_checkthread (PVOID lparam);

BOOLEAN NTAPI _r_update_downloadcallback (ULONG total_written, ULONG total_length, LONG_PTR lpdata);
THREAD_FN _r_update_downloadthread (PVOID lparam);

HRESULT CALLBACK _r_update_pagecallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);
INT _r_update_pagenavigate (HWND htaskdlg, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata);

VOID _r_update_install ();

FORCEINLINE LPCWSTR _r_update_getpath ()
{
	return _r_obj_getstring (app_update_path);
}
#endif // _APP_HAVE_UPDATES

VOID _r_logerror (UINT tray_id, LPCWSTR fn, ULONG code, LPCWSTR description);
VOID _r_logerror_v (UINT tray_id, LPCWSTR fn, ULONG code, LPCWSTR format, ...);

#if !defined(_APP_CONSOLE)
VOID _r_show_aboutmessage (HWND hwnd);
VOID _r_show_errormessage (HWND hwnd, LPCWSTR main, ULONG errcode, LPCWSTR description, HINSTANCE hmodule);
BOOLEAN _r_show_confirmmessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR configKey);
INT _r_show_message (HWND hwnd, ULONG flags, LPCWSTR title, LPCWSTR main, LPCWSTR content);

VOID _r_window_restoreposition (HWND hwnd, LPCWSTR window_name);
VOID _r_window_saveposition (HWND hwnd, LPCWSTR window_name);
#endif // _APP_CONSOLE

/*
	Application
*/

BOOLEAN _r_app_initialize (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright);

#if !defined(_APP_CONSOLE)
BOOLEAN _r_app_createwindow (INT dlg_id, INT icon_id, DLGPROC dlg_proc);
LRESULT CALLBACK _r_app_mainwindowproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // _APP_CONSOLE

PR_STRING _r_app_getproxyconfiguration ();

#if !defined(_APP_CONSOLE)
HICON _r_app_getsharedimage (HINSTANCE hinst, INT icon_id, INT icon_size);
BOOLEAN _r_app_runasadmin ();
VOID  _r_app_restart (HWND hwnd);
#endif // _APP_CONSOLE

FORCEINLINE LPCWSTR _r_app_getname ()
{
	return _r_obj_getstring (app_name);
}

FORCEINLINE LPCWSTR _r_app_getnameshort ()
{
	return _r_obj_getstring (app_name_short);
}

FORCEINLINE LPCWSTR _r_app_getversion ()
{
	return _r_obj_getstring (app_version);
}

FORCEINLINE LPCWSTR _r_app_getcopyright ()
{
	return _r_obj_getstring (app_copyright);
}

FORCEINLINE LPCWSTR _r_app_getdirectory ()
{
	return _r_obj_getstring (app_directory);
}

FORCEINLINE LPCWSTR _r_app_getprofiledirectory ()
{
	return _r_obj_getstring (app_profile_directory);
}

FORCEINLINE LPCWSTR _r_app_getlogpath ()
{
	return _r_obj_getstring (app_log_path);
}

#if !defined(_APP_CONSOLE)
FORCEINLINE HWND _r_app_gethwnd ()
{
	return app_hwnd;
}
#endif // !_APP_CONSOLE

FORCEINLINE LPCWSTR _r_app_getuseragent ()
{
	return _r_obj_getstring (app_useragent);
}

#if !defined(_APP_CONSOLE)
FORCEINLINE BOOLEAN _r_app_isclassicui ()
{
	return _r_config_getboolean (L"ClassicUI", _APP_CLASSICUI) || !IsAppThemed ();
}
#endif // !_APP_CONSOLE
