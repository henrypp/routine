// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#pragma once

#include "routine.h"
#include "resource.h"

#include "rconfig.h"

//
// Global variables
//

DECLSPEC_SELECTANY APP_GLOBAL_CONFIG app_global = {0};

//
// Configuration
//

VOID _r_config_initialize ();

BOOLEAN _r_config_getboolean_ex (_In_ LPCWSTR key_name, _In_opt_ BOOLEAN def_value, _In_opt_ LPCWSTR section_name);
LONG _r_config_getlong_ex (_In_ LPCWSTR key_name, _In_opt_ LONG def_value, _In_opt_ LPCWSTR section_name);
LONG64 _r_config_getlong64_ex (_In_ LPCWSTR key_name, _In_opt_ LONG64 def_value, _In_opt_ LPCWSTR section_name);
ULONG _r_config_getulong_ex (_In_ LPCWSTR key_name, _In_opt_ ULONG def_value, _In_opt_ LPCWSTR section_name);
ULONG64 _r_config_getulong64_ex (_In_ LPCWSTR key_name, _In_opt_ ULONG64 def_value, _In_opt_ LPCWSTR section_name);
VOID _r_config_getfont_ex (_In_ LPCWSTR key_name, _Inout_ PLOGFONT logfont, _In_ LONG dpi_value, _In_opt_ LPCWSTR section_name);
VOID _r_config_getsize (_In_ LPCWSTR key_name, _Out_ PR_SIZE size, _In_opt_ PR_SIZE def_value, _In_opt_ LPCWSTR section_name);

_Ret_maybenull_
PR_STRING _r_config_getstringexpand_ex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name);

_Ret_maybenull_
PR_STRING _r_config_getstring_ex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name);

FORCEINLINE BOOLEAN _r_config_getboolean (_In_ LPCWSTR key_name, _In_ BOOLEAN def_value)
{
	return _r_config_getboolean_ex (key_name, def_value, NULL);
}

FORCEINLINE LONG _r_config_getlong (_In_ LPCWSTR key_name, _In_ LONG def_value)
{
	return _r_config_getlong_ex (key_name, def_value, NULL);
}

FORCEINLINE LONG64 _r_config_getlong64 (_In_ LPCWSTR key_name, _In_ LONG64 def_value)
{
	return _r_config_getlong64_ex (key_name, def_value, NULL);
}

FORCEINLINE ULONG _r_config_getulong (_In_ LPCWSTR key_name, _In_ ULONG def_value)
{
	return _r_config_getulong_ex (key_name, def_value, NULL);
}

FORCEINLINE ULONG64 _r_config_getulong64 (_In_ LPCWSTR key_name, _In_ ULONG64 def_value)
{
	return _r_config_getulong64_ex (key_name, def_value, NULL);
}

FORCEINLINE VOID _r_config_getfont (_In_ LPCWSTR key_name, _Inout_ PLOGFONT logfont, _In_ LONG dpi_value)
{
	_r_config_getfont_ex (key_name, logfont, dpi_value, NULL);
}

_Ret_maybenull_
FORCEINLINE PR_STRING _r_config_getstringexpand (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value)
{
	return _r_config_getstringexpand_ex (key_name, def_value, NULL);
}

_Ret_maybenull_
FORCEINLINE PR_STRING _r_config_getstring (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value)
{
	return _r_config_getstring_ex (key_name, def_value, NULL);
}

VOID _r_config_setboolean_ex (_In_ LPCWSTR key_name, _In_ BOOLEAN value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setlong_ex (_In_ LPCWSTR key_name, _In_ LONG value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setlong64_ex (_In_ LPCWSTR key_name, _In_ LONG64 value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setulong_ex (_In_ LPCWSTR key_name, _In_ ULONG value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setulong64_ex (_In_ LPCWSTR key_name, _In_ ULONG64 value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setfont_ex (_In_ LPCWSTR key_name, _In_ PLOGFONT logfont, _In_ LONG dpi_value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setsize (_In_ LPCWSTR key_name, _In_ PR_SIZE size, _In_opt_ LPCWSTR section_name);
VOID _r_config_setstringexpand_ex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value, _In_opt_ LPCWSTR section_name);
VOID _r_config_setstring_ex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value, _In_opt_ LPCWSTR section_name);

FORCEINLINE VOID _r_config_setboolean (_In_ LPCWSTR key_name, _In_ BOOLEAN value)
{
	_r_config_setboolean_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setlong (_In_ LPCWSTR key_name, _In_ LONG value)
{
	_r_config_setlong_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setlong64 (_In_ LPCWSTR key_name, _In_ LONG64 value)
{
	_r_config_setlong64_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setulong (_In_ LPCWSTR key_name, _In_ ULONG value)
{
	_r_config_setulong_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setulong64 (_In_ LPCWSTR key_name, _In_ ULONG64 value)
{
	_r_config_setulong64_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setfont (_In_ LPCWSTR key_name, _In_ PLOGFONT logfont, _In_ LONG dpi_value)
{
	_r_config_setfont_ex (key_name, logfont, dpi_value, NULL);
}

FORCEINLINE VOID _r_config_setstringexpand (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value)
{
	_r_config_setstringexpand_ex (key_name, value, NULL);
}

FORCEINLINE VOID _r_config_setstring (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value)
{
	_r_config_setstring_ex (key_name, value, NULL);
}

//
// Localization
//

#if !defined(APP_CONSOLE)
VOID _r_locale_initialize ();

VOID _r_locale_apply (_In_ PVOID hwnd, _In_ INT ctrl_id, _In_opt_ UINT menu_id);
VOID _r_locale_enum (_In_ PVOID hwnd, _In_ INT ctrl_id, _In_opt_ UINT menu_id);

SIZE_T _r_locale_getcount ();

_Ret_maybenull_
PR_STRING _r_locale_getstring_ex (_In_ UINT uid);

LPCWSTR _r_locale_getstring (_In_ UINT uid);

LONG64 _r_locale_getversion ();
#endif // !APP_CONSOLE

//
// Settings window
//

#if defined(APP_HAVE_SETTINGS)
VOID _r_settings_addpage (_In_ INT dlg_id, _In_ UINT locale_id);
VOID _r_settings_adjustchild (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HWND hchild);
VOID _r_settings_createwindow (_In_ HWND hwnd, _In_opt_ DLGPROC dlg_proc, _In_opt_ LONG dlg_id);
INT_PTR CALLBACK _r_settings_wndproc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam);

FORCEINLINE HWND _r_settings_getwindow ()
{
	return app_global.settings.hwnd;
}
#endif // APP_HAVE_SETTINGS

//
// Autorun (optional feature)
//

#if defined(APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ();
BOOLEAN _r_autorun_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable);
#endif // APP_HAVE_AUTORUN

//
// Skip UAC (optional feature)
//

#if defined(APP_HAVE_SKIPUAC)
BOOLEAN _r_skipuac_isenabled ();
HRESULT _r_skipuac_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable);
BOOLEAN _r_skipuac_run ();
#endif // APP_HAVE_SKIPUAC

//
// Update checker (optional feature)
//

#if defined(APP_HAVE_UPDATES)
VOID _r_update_check (_In_opt_ HWND hparent);

BOOLEAN NTAPI _r_update_downloadcallback (_In_ ULONG total_written, _In_ ULONG total_length, _In_ PVOID param);
NTSTATUS NTAPI _r_update_downloadthread (_In_ PVOID arglist);

HRESULT CALLBACK _r_update_pagecallback (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam, _In_ LONG_PTR pdata);
VOID _r_update_pagenavigate (_In_opt_ HWND htaskdlg, _In_opt_ LPCWSTR main_icon, _In_ TASKDIALOG_FLAGS flags, _In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons, _In_opt_ LPCWSTR main, _In_opt_ LPCWSTR content, _In_ LONG_PTR param);

VOID _r_update_addcomponent (_In_opt_ LPCWSTR full_name, _In_opt_ LPCWSTR short_name, _In_opt_ LPCWSTR version, _In_opt_ PR_STRING target_path, _In_ BOOLEAN is_installer);
VOID _r_update_install (_In_ PR_STRING install_path);

#endif // APP_HAVE_UPDATES

VOID _r_log (_In_ R_LOG_LEVEL log_level, _In_opt_ LPCGUID tray_guid, _In_ LPCWSTR title, _In_ ULONG code, _In_opt_ LPCWSTR description);
VOID _r_log_v (_In_ R_LOG_LEVEL log_level, _In_opt_ LPCGUID tray_guid, _In_ LPCWSTR title, _In_ ULONG code, _In_ _Printf_format_string_ LPCWSTR format, ...);

#if !defined(APP_CONSOLE)
VOID _r_show_aboutmessage (_In_opt_ HWND hwnd);
VOID _r_show_errormessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ ULONG error_code, _In_opt_ PR_ERROR_INFO error_info_ptr);
BOOLEAN _r_show_confirmmessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ LPCWSTR text, _In_opt_ LPCWSTR config_key);
INT _r_show_message (_In_opt_ HWND hwnd, _In_ ULONG flags, _In_opt_ LPCWSTR title, _In_opt_ LPCWSTR main, _In_ LPCWSTR content);

VOID _r_window_restoreposition (_In_ HWND hwnd, _In_ LPCWSTR window_name);
VOID _r_window_saveposition (_In_ HWND hwnd, _In_ LPCWSTR window_name);

#endif // APP_CONSOLE

//
// Application
//

BOOLEAN _r_app_initialize ();

PR_STRING _r_app_getdirectory ();
PR_STRING _r_app_getconfigpath ();

PR_STRING _r_app_getcrashdirectory ();

#if !defined(APP_CONSOLE)
PR_STRING _r_app_getlocalepath ();
#endif // !APP_CONSOLE

PR_STRING _r_app_getlogpath ();
PR_STRING _r_app_getprofiledirectory ();
PR_STRING _r_app_getuseragent ();

#if !defined(APP_CONSOLE)
HWND _r_app_createwindow (_In_ LPCWSTR dlg_name, _In_opt_ LPCWSTR icon_name, _In_ DLGPROC dlg_proc);

BOOLEAN _r_app_runasadmin ();
VOID _r_app_restart (_In_ HWND hwnd);
#endif // !APP_CONSOLE

#if !defined(APP_CONSOLE)
FORCEINLINE HWND _r_app_gethwnd ()
{
	return app_global.main.hwnd;
}

FORCEINLINE VOID _r_app_sethwnd (_In_opt_ HWND hwnd)
{
	app_global.main.hwnd = hwnd;
}
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

