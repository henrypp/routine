// routine.c
// project sdk library
//
// Copyright (c) 2012-2024 Henry++

#pragma once

#include "routine.h"
#include "rconfig.h"

#include "resource.h"

#if !defined(IDC_NAV)
#define IDC_NAV 1000
#endif // IDC_NAV

#if !defined(IDC_RESET)
#define IDC_RESET 1001
#endif // IDC_RESET

#if !defined(IDC_SAVE)
#define IDC_SAVE 1002
#endif // IDC_SAVE

#if !defined(IDC_CLOSE)
#define IDC_CLOSE 1003
#endif // IDC_CLOSE

#define WND_BACKGROUND_CLR RGB(0x2A, 0x2A, 0x2A)
#define WND_BACKGROUND2_CLR RGB(0x40, 0x40, 0x40)
#define WND_BORDER_CLR RGB(0x90, 0x10, 0x20)
#define WND_BUTTON_CLR RGB(0x24, 0x24, 0x24)
#define WND_HIGHLIGHT_CLR RGB(0x80, 0x80, 0x80)
#define WND_HOT_CLR _r_dc_getcoloraccent()
#define WND_TEXT_CLR RGB(0xFF, 0xFF, 0xFF)
#define WND_GRAYTEXT_CLR RGB(0x50, 0x50, 0x50)

//
// Global variables
//

DECLSPEC_SELECTANY APP_GLOBAL_CONFIG app_global = {0};

EXTERN_C_START

//
// Configuration
//

VOID _r_config_initialize ();

BOOLEAN _r_config_getboolean (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN def_value
);

BOOLEAN _r_config_getboolean_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ BOOLEAN def_value,
	_In_opt_ LPCWSTR section_name
);

LONG _r_config_getlong (
	_In_ LPCWSTR key_name,
	_In_ LONG def_value
);

LONG _r_config_getlong_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LONG def_value,
	_In_opt_ LPCWSTR section_name
);

LONG64 _r_config_getlong64 (
	_In_ LPCWSTR key_name,
	_In_ LONG64 def_value
);

LONG64 _r_config_getlong64_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LONG64 def_value,
	_In_opt_ LPCWSTR section_name
);

ULONG _r_config_getulong (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG def_value
);

ULONG _r_config_getulong_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG def_value,
	_In_opt_ LPCWSTR section_name
);

ULONG64 _r_config_getulong64 (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG64 def_value
);

ULONG64 _r_config_getulong64_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG64 def_value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_getfont (
	_In_ LPCWSTR key_name,
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value
);

VOID _r_config_getfont_ex (
	_In_ LPCWSTR key_name,
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_getsize (
	_In_ LPCWSTR key_name,
	_Out_ PR_SIZE size,
	_In_opt_ PR_SIZE def_value,
	_In_opt_ LPCWSTR section_name
);

_Ret_maybenull_
PR_STRING _r_config_getstringexpand (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value
);

_Ret_maybenull_
PR_STRING _r_config_getstringexpand_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value,
	_In_opt_ LPCWSTR section_name
);

_Ret_maybenull_
PR_STRING _r_config_getstring (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value
);

_Ret_maybenull_
PR_STRING _r_config_getstring_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setboolean (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN value
);

VOID _r_config_setboolean_ex (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setlong (
	_In_ LPCWSTR key_name,
	_In_ LONG value
);

VOID _r_config_setlong_ex (
	_In_ LPCWSTR key_name,
	_In_ LONG value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setlong64 (
	_In_ LPCWSTR key_name,
	_In_ LONG64 value
);

VOID _r_config_setlong64_ex (
	_In_ LPCWSTR key_name,
	_In_ LONG64 value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setulong (
	_In_ LPCWSTR key_name,
	_In_ ULONG value
);

VOID _r_config_setulong_ex (
	_In_ LPCWSTR key_name,
	_In_ ULONG value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setulong64 (
	_In_ LPCWSTR key_name,
	_In_ ULONG64 value
);

VOID _r_config_setulong64_ex (
	_In_ LPCWSTR key_name,
	_In_ ULONG64 value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setfont (
	_In_ LPCWSTR key_name,
	_In_ PLOGFONT logfont,
	_In_ LONG dpi_value
);

VOID _r_config_setfont_ex (
	_In_ LPCWSTR key_name,
	_In_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setsize (
	_In_ LPCWSTR key_name,
	_In_ PR_SIZE size,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setstringexpand (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value
);

VOID _r_config_setstringexpand_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value,
	_In_opt_ LPCWSTR section_name
);

VOID _r_config_setstring (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value
);

VOID _r_config_setstring_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value,
	_In_opt_ LPCWSTR section_name
);

//
// Localization
//

VOID _r_locale_initialize ();

VOID _r_locale_apply (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
);

VOID _r_locale_enum (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
);

ULONG_PTR _r_locale_getcount ();

_Ret_maybenull_
PR_STRING _r_locale_getstring_ex (
	_In_ ULONG uid
);

LPWSTR _r_locale_getstring (
	_In_ ULONG uid
);

LONG64 _r_locale_getversion ();

//
// Settings
//

VOID _r_settings_addpage (
	_In_ INT dlg_id,
	_In_ UINT locale_id
);

VOID _r_settings_adjustchild (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HWND hchild
);

VOID _r_settings_createwindow (
	_In_opt_ HWND hparent,
	_In_opt_ DLGPROC dlg_proc,
	_In_opt_ LONG dlg_id
);

INT_PTR CALLBACK _r_settings_wndproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

FORCEINLINE HWND _r_settings_getwindow ()
{
	return app_global.settings.hwnd;
}

//
// Autorun (optional feature)
//

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_autorun_enable (
	_In_opt_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

BOOLEAN _r_autorun_isenabled ();

//
// Skip UAC (optional feature)
//

HRESULT _r_skipuac_checkmodulepath (
	_In_ IRegisteredTaskPtr registered_task
);

BOOLEAN _r_skipuac_isenabled ();

HRESULT _r_skipuac_enable (
	_In_opt_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

BOOLEAN _r_skipuac_run ();

//
// Theme
//

VOID _r_theme_cleanup (
	_In_ HWND hwnd,
	_In_ PR_THEME_CONTEXT context
);

VOID _r_theme_enable (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

BOOL CALLBACK _r_theme_enumchildwindows (
	_In_ HWND hwnd,
	_In_ LPARAM context
);

VOID _r_theme_initialize (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

VOID _r_theme_initializecontext (
	_In_ HWND hwnd,
	_In_opt_ LPCWSTR class_list,
	_In_ WNDPROC subclass_proc,
	_In_ BOOLEAN is_enable
);

BOOLEAN _r_theme_isenabled ();

VOID _r_theme_setwindowframe (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

LRESULT CALLBACK _r_theme_combobox_subclassproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_drawbutton (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ LONG_PTR style
);

LRESULT CALLBACK _r_theme_drawcheckbox (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ LONG_PTR style
);

LRESULT CALLBACK _r_theme_edit_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_groupbox_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_header_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_rebar_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_statusbar_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_tabcontrol_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

LRESULT CALLBACK _r_theme_subclassproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

FORCEINLINE HRESULT _r_theme_setdarkmode (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	return SetWindowTheme (hwnd, is_enable ? L"DarkMode_Explorer" : L"Explorer", NULL);
}

//
// Update checker (optional feature)
//

BOOLEAN NTAPI _r_update_downloadcallback (
	_In_ ULONG total_written,
	_In_ ULONG total_length,
	_In_ PVOID lparam
);

NTSTATUS _r_update_downloadupdate (
	_In_ PR_UPDATE_INFO update_info,
	_Inout_ PR_UPDATE_COMPONENT update_component
);

NTSTATUS NTAPI _r_update_downloadthread (
	_In_ PVOID arglist
);

NTSTATUS NTAPI _r_update_checkthread (
	_In_ PVOID arglist
);

VOID _r_update_enable (
	_In_ BOOLEAN is_enable
);

_Success_ (return)
BOOLEAN _r_update_isenabled (
	_In_ BOOLEAN is_checktimestamp
);

_Success_ (return)
BOOLEAN _r_update_check (
	_In_opt_ HWND hparent
);

HRESULT CALLBACK _r_update_pagecallback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam,
	_In_ LONG_PTR pdata
);

VOID _r_update_navigate (
	_In_ PR_UPDATE_INFO update_info,
	_In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
	_In_opt_ TASKDIALOG_FLAGS flags,
	_In_opt_ LPCWSTR icon,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR content,
	_In_opt_ LONG error_code,
	_In_ R_ERROR_TYPE type
);

VOID _r_update_addcomponent (
	_In_ LPCWSTR full_name,
	_In_ LPCWSTR short_name,
	_In_ LPCWSTR version,
	_In_ PR_STRING target_path,
	_In_ BOOLEAN is_installer
);

VOID _r_update_applyconfig ();

VOID _r_update_install (
	_In_opt_ HWND hwnd,
	_In_ PR_UPDATE_COMPONENT update_component
);

//
// Logging
//

BOOLEAN _r_log_isenabled (
	_In_ R_LOG_LEVEL log_level_check
);

_Ret_maybenull_
HANDLE _r_log_getfilehandle ();

VOID _r_log (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ LONG error_code,
	_In_opt_ LPCWSTR description
);

VOID _r_log_v (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ LONG error_code,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

LPCWSTR _r_log_leveltostring (
	_In_ R_LOG_LEVEL log_level
);

ULONG _r_log_leveltrayicon (
	_In_ R_LOG_LEVEL log_level
);

//
// Messages
//

VOID _r_report_error (
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR string,
	_In_ LONG status,
	_In_ R_ERROR_TYPE type
);

VOID _r_show_aboutmessage (
	_In_opt_ HWND hwnd
);

BOOLEAN _r_show_confirmmessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR content,
	_In_opt_ LPCWSTR config_key
);

NTSTATUS _r_show_errormessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR title,
	_In_ LONG error_code,
	_In_opt_ LPCWSTR description,
	_In_ R_ERROR_TYPE type
);

INT _r_show_message (
	_In_opt_ HWND hwnd,
	_In_ ULONG flags,
	_In_opt_ LPCWSTR title,
	_In_ LPCWSTR content
);

VOID _r_window_restoreposition (
	_In_ HWND hwnd,
	_In_ LPCWSTR window_name
);

VOID _r_window_saveposition (
	_In_ HWND hwnd,
	_In_ LPCWSTR window_name
);

//
// Application: seh
//

ULONG NTAPI _r_app_exceptionfilter_callback (
	_In_ PEXCEPTION_POINTERS exception_ptr
);

//
// Application: common
//

LPWSTR _r_app_getmutexname ();

BOOLEAN _r_app_isportable ();

BOOLEAN _r_app_initialize_com ();

VOID _r_app_initialize_components ();

VOID _r_app_initialize_controls ();

VOID _r_app_initialize_dll ();

VOID _r_app_initialize_locale ();

VOID _r_app_initialize_seh ();

typedef enum _R_CMDLINE_INFO_CLASS
{
	CmdlineHelp,
	CmdlineClean,
	CmdlineInstall,
	CmdlineUninstall,
} R_CMDLINE_INFO_CLASS;

typedef BOOLEAN (NTAPI *PR_CMDLINE_CALLBACK) (
	_In_ R_CMDLINE_INFO_CLASS info_class
	);

BOOLEAN _r_app_initialize (
	_In_opt_ PR_CMDLINE_CALLBACK cmd_callback
);

PR_STRING _r_app_getdirectory ();

PR_STRING _r_app_getconfigpath ();

PR_STRING _r_app_getcachedirectory (
	_In_ BOOLEAN is_create
);

PR_STRING _r_app_getcrashdirectory (
	_In_ BOOLEAN is_create
);

PR_STRING _r_app_getlocalepath ();

PR_STRING _r_app_getlogpath ();

PR_STRING _r_app_getprofiledirectory ();

_Ret_maybenull_
PR_STRING _r_app_getproxyconfiguration ();

PR_STRING _r_app_getuseragent ();

LRESULT CALLBACK _r_app_maindlgproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

_Ret_maybenull_
HWND _r_app_createwindow (
	_In_ PVOID hinst,
	_In_ LPCWSTR dlg_name,
	_In_opt_ LPWSTR icon_name,
	_In_ DLGPROC dlg_proc
);

ULONG _r_app_getshowcode (
	_In_ HWND hwnd
);

BOOLEAN _r_app_runasadmin ();

VOID _r_app_restart (
	_In_opt_ HWND hwnd
);

EXTERN_C_END

FORCEINLINE HWND _r_app_gethwnd ()
{
	return app_global.main.hwnd;
}

FORCEINLINE VOID _r_app_sethwnd (
	_In_opt_ HWND hwnd
)
{
	app_global.main.hwnd = hwnd;
}

FORCEINLINE LPWSTR _r_app_getname ()
{
	return (LPWSTR)APP_NAME;
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
	return L"https://github.com/henrypp/" APP_NAME_SHORT L"#donate";
}

FORCEINLINE LPCWSTR _r_app_getwebsite_url ()
{
	return L"https://github.com/henrypp/" APP_NAME_SHORT;
}

FORCEINLINE LPCWSTR _r_app_getupdate_url ()
{
	return L"https://raw.githubusercontent.com/henrypp/" APP_NAME_SHORT L"/master/VERSION";
}

FORCEINLINE LPCWSTR _r_app_getversiontype ()
{
#if defined(_DEBUG) || defined(APP_BETA)
	return L"Pre-release";
#else
	return L"Release";
#endif // _DEBUG || APP_BETA
}

FORCEINLINE LONG _r_app_getarch ()
{
#if defined(_WIN64)
	return 64L;
#else
	return 32L;
#endif // _WIN64
}

FORCEINLINE LPCWSTR _r_app_getversion ()
{
	return APP_VERSION;
}
