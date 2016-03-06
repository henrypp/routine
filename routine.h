// routine++
// Copyright (c) 2012-2016 Henry++

#pragma once

#undef PSAPI_VERSION
#define PSAPI_VERSION 1

#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <psapi.h>
#include <uxtheme.h>
#include <time.h>
#include <lm.h>
#include <process.h>

#include "rconfig.h"
#include "rstring.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")

/*
	Write debug log to console
*/

#define WDBG1(a) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, L"%s", a)
#define WDBG2(a, ...) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, __VA_ARGS__)

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...);

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR format, ...);

rstring _r_fmt_date (LPFILETIME ft, const DWORD flags = FDTF_DEFAULT); // see SHFormatDateTime flags definition
rstring _r_fmt_date (__time64_t ut, const DWORD flags = FDTF_DEFAULT);

rstring _r_fmt_size64 (DWORDLONG size);

/*
	System messages
*/

#define WMSG1(a) _r_msg (nullptr, 0, nullptr, L"%s", a)
#define WMSG2(a, ...) _r_msg (nullptr, 0, nullptr, a, __VA_ARGS__)

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR format, ...);
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR ref);

/*
	Clipboard operations
*/

rstring _r_clipboard_get (HWND hwnd);
VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length);

/*
	Filesystem
*/

BOOL _r_fs_exists (LPCWSTR path);
DWORD64 _r_fs_size (HANDLE h);
BOOL _r_fs_mkdir (LPCWSTR path);

/*
	Processes
*/

BOOL _r_process_is_exists (LPCWSTR path, const size_t len);

/*
	Strings
*/
INT _r_str_versioncompare (LPCWSTR v1, LPCWSTR v2);

/*
	System information
*/

BOOL _r_sys_adminstate ();

#ifndef _WIN64
	BOOL _r_sys_iswow64 ();
#endif // _WIN64

BOOL _r_sys_setprivilege (LPCWSTR privilege, BOOL is_enable);
BOOL _r_sys_uacstate ();
BOOL _r_sys_validversion (DWORD major, DWORD minor, BYTE condition = VER_GREATER_EQUAL);

/*
	Unixtime
*/

__time64_t _r_unixtime_now (BOOL is_local = TRUE);
VOID _r_unixtime_to_filetime (__time64_t ut, LPFILETIME pft);
VOID _r_unixtime_to_systemtime (__time64_t ut, LPSYSTEMTIME pst);
__time64_t _r_unixtime_from_filetime (LPFILETIME pft);
__time64_t _r_unixtime_from_systemtime (LPSYSTEMTIME pst);

/*
	Window management
*/

VOID _r_wnd_center (HWND hwnd);
BOOL _r_wnd_changemessagefilter (HWND hwnd, UINT msg, DWORD action);
VOID _r_wnd_toggle (HWND hwnd, BOOL show);
VOID _r_wnd_top (HWND hwnd, BOOL is_enable);

/*
	Other
*/

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT d);
BOOL _r_run (LPCWSTR cmdline, LPCWSTR cd = nullptr, BOOL is_wait = FALSE);
rstring _r_normalize_path (LPCWSTR path);

/*
	Control: common
*/

VOID _r_ctrl_enable (HWND hwnd, INT ctrl, BOOL is_enable);

rstring _r_ctrl_gettext (HWND hwnd, INT ctrl);
VOID _r_ctrl_settext (HWND hwnd, INT ctrl, LPCWSTR str, ...);

HWND _r_ctrl_settip (HWND hwnd, INT ctrl, LPCWSTR text);
BOOL _r_ctrl_showtip (HWND hwnd, INT ctrl, LPCWSTR title, LPCWSTR text, INT icon);

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, INT ctrl, LPCWSTR text, INT width, INT subitem, INT fmt);
INT _r_listview_addgroup (HWND hwnd, INT ctrl, INT group_id, LPCWSTR text, UINT align = 0, UINT state = 0);
INT _r_listview_additem (HWND hwnd, INT ctrl, LPCWSTR text, INT item, INT subitem, INT image = -1, INT group_id = -1, LPARAM lparam = 0);
VOID _r_listview_deleteallcolumns (HWND hwnd, INT ctrl);
VOID _r_listview_deleteallgroups (HWND hwnd, INT ctrl);
VOID _r_listview_deleteallitems (HWND hwnd, INT ctrl);
INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl, INT column);
INT _r_listview_getitemcount (HWND hwnd, INT ctrl);
INT _r_listview_getcolumncount (HWND hwnd, INT ctrl);
LPARAM _r_listview_getlparam (HWND hwnd, INT ctrl, INT item);
rstring _r_listview_gettext (HWND hwnd, INT ctrl, INT item, INT subitem);
DWORD _r_listview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle);

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl, LPCWSTR text, HTREEITEM parent = nullptr, INT image = -1, LPARAM lparam = 0);
LPARAM _r_treeview_getlparam (HWND hwnd, INT ctrl, HTREEITEM item);
DWORD _r_treeview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle, INT height);

/*
	Control: statusbar
*/

BOOL _r_status_settext (HWND hwnd, INT ctrl, INT part, LPCWSTR text);
VOID _r_status_setstyle (HWND hwnd, INT ctrl, INT height);

/*
	Exported function definitions
*/

typedef BOOL (WINAPI *CWMF) (UINT, DWORD); // ChangeWindowMessageFilter
typedef BOOL (WINAPI *CWMFEX) (HWND, UINT, DWORD, PCHANGEFILTERSTRUCT); // ChangeWindowMessageFilterEx
typedef BOOL (WINAPI *IW64P) (HANDLE, PBOOL); // IsWow64Process
typedef HRESULT (WINAPI *LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
typedef VOID (WINAPI *TDI) (TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect
typedef int (WINAPI *SHCDEX) (HWND, LPCTSTR, const SECURITY_ATTRIBUTES*); // SHCreateDirectoryEx
