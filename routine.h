// routine++
// Copyright (c) 2012-2017 Henry++

#pragma once

#ifndef _APP_NO_WINXP
#undef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif // _APP_NO_WINXP

#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <psapi.h>
#include <uxtheme.h>
#include <time.h>
#include <lm.h>
#include <process.h>
#include <winhttp.h>

#include "rconfig.h"
#include "rstring.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "winhttp.lib")

/*
	Macroses
*/

#define _R_COLOR_SHADE(clr, percent) RGB ((BYTE)((float)GetRValue (clr) * percent / 100.0), (BYTE)((float)GetGValue (clr) * percent / 100.0), (BYTE)((float)GetBValue (clr) * percent / 100.0))

#define _R_PERCENT_OF(val, total) INT(ceil((double(val) / double(total)) * 100.0))
#define _R_PERCENT_VAL(val, total) INT(double(total) * double(val) / 100.0)

#define _R_SPINLOCK(x) _r_spinlock(&x)
#define _R_SPINUNLOCK(x) _r_spinunlock(&x)

/*
	Write debug log to console
*/

#define WDBG1(a) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, L"%s", a)
#define WDBG2(a, ...) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, __VA_ARGS__)
#define WDBG(a, ...) _r_dbg (nullptr, nullptr, 0, a, __VA_ARGS__)

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...);
rstring _r_dbg_error (DWORD errcode = GetLastError ());

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR format, ...);

rstring _r_fmt_date (const LPFILETIME ft, const DWORD flags = FDTF_DEFAULT); // see SHFormatDateTime flags definition
rstring _r_fmt_date (const __time64_t ut, const DWORD flags = FDTF_DEFAULT);

rstring _r_fmt_size64 (DWORDLONG size);

/*
	Spinlocks
*/

VOID _r_spinlock (volatile LONG* m_ref);
VOID _r_spinunlock (volatile LONG* m_ref);

/*
	System messages
*/

#define WMSG1(a) _r_msg (nullptr, 0, nullptr, nullptr, L"%s", a)
#define WMSG2(a, ...) _r_msg (nullptr, 0, nullptr, nullptr, a, __VA_ARGS__)

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR format, ...);
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR ref);

/*
	Clipboard operations
*/

rstring _r_clipboard_get (HWND hwnd);
VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length);

/*
	Filesystem
*/

BOOL _r_fs_delete (LPCWSTR path, BOOL allowundo = FALSE);
BOOL _r_fs_exists (LPCWSTR path);
BOOL _r_fs_mkdir (LPCWSTR path);
BOOL _r_fs_rmdir (LPCWSTR path);
BOOL _r_fs_readfile (HANDLE h, LPVOID result, DWORD64 size);
DWORD64 _r_fs_size (HANDLE h);
BOOL _r_fs_move (LPCWSTR path_from, LPCWSTR path_to, DWORD flags = 0);
BOOL _r_fs_copy (LPCWSTR path_from, LPCWSTR path_to, DWORD flags = 0);

/*
	Paths
*/

rstring _r_path_expand (rstring path);
rstring _r_path_unexpand (rstring path);
rstring _r_path_compact (rstring path, UINT length);
rstring _r_path_extractdir (rstring path);
rstring _r_path_extractfile (rstring path);
rstring _r_path_dospathfromnt (LPCWSTR path);

/*
	Processes
*/

BOOL _r_process_getpath (HANDLE h, LPWSTR path, DWORD length);
BOOL _r_process_is_exists (LPCWSTR path, const size_t len);

/*
	Strings
*/
size_t _r_str_hash (LPCWSTR text);
INT _r_str_versioncompare (LPCWSTR v1, LPCWSTR v2);

/*
	System information
*/

BOOL _r_sys_adminstate ();

#ifndef _WIN64
BOOL _r_sys_iswow64 ();
#endif // _WIN64

BOOL _r_sys_setsecurityattributes (LPSECURITY_ATTRIBUTES sa, DWORD length, PSECURITY_DESCRIPTOR sd);
BOOL _r_sys_setprivilege (LPCWSTR privilege, BOOL is_enable);
BOOL _r_sys_uacstate ();
BOOL _r_sys_validversion (DWORD major, DWORD minor, DWORD build = 0, BYTE condition = VER_GREATER_EQUAL);
VOID _r_sleep (DWORD milliseconds);

/*
	Unixtime
*/

__time64_t _r_unixtime_now ();
VOID _r_unixtime_to_filetime (__time64_t ut, const LPFILETIME pft);
VOID _r_unixtime_to_systemtime (__time64_t ut, const LPSYSTEMTIME pst);
__time64_t _r_unixtime_from_filetime (const FILETIME* pft);
__time64_t _r_unixtime_from_systemtime (const LPSYSTEMTIME pst);

/*
	Window management
*/

VOID _r_wnd_center (HWND hwnd);
BOOL _r_wnd_changemessagefilter (HWND hwnd, UINT msg, DWORD action);
VOID _r_wnd_fillrect (HDC dc, LPRECT rc, COLORREF clr);
VOID _r_wnd_toggle (HWND hwnd, BOOL show);
VOID _r_wnd_top (HWND hwnd, BOOL is_enable);
VOID _r_wnd_addstyle (HWND hwnd, UINT ctrl_id, LONG mask, LONG stateMask, INT index);

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent);
BOOL _r_inet_openurl (HINTERNET h, LPCWSTR url, HINTERNET* pconnect, HINTERNET* prequest, PDWORD contentlength);
BOOL _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, DWORD length, PDWORD written);
BOOL _r_inet_close (HINTERNET h);

/*
	Other
*/

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT d);
BOOL _r_run (LPCWSTR filename, LPCWSTR cmdline, LPCWSTR cd = nullptr, WORD sw = SW_SHOWDEFAULT);
size_t _r_rnd (size_t start, size_t end);

/*
	Control: common
*/

VOID _r_ctrl_enable (HWND hwnd, UINT ctrl, BOOL is_enable);

rstring _r_ctrl_gettext (HWND hwnd, UINT ctrl);
VOID _r_ctrl_settext (HWND hwnd, UINT ctrl, LPCWSTR str, ...);

HWND _r_ctrl_settip (HWND hwnd, UINT ctrl, LPCWSTR text);
BOOL _r_ctrl_showtip (HWND hwnd, UINT ctrl, LPCWSTR title, LPCWSTR text, INT icon);

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, UINT ctrl, LPCWSTR text, UINT width, size_t subitem, INT fmt);
INT _r_listview_addgroup (HWND hwnd, UINT ctrl, size_t group_id, LPCWSTR text, UINT align = 0, UINT state = 0);
INT _r_listview_additem (HWND hwnd, UINT ctrl, LPCWSTR text, size_t item, size_t subitem, size_t image = LAST_VALUE, size_t group_id = LAST_VALUE, LPARAM lparam = 0);

VOID _r_listview_deleteallcolumns (HWND hwnd, UINT ctrl);
VOID _r_listview_deleteallgroups (HWND hwnd, UINT ctrl);
VOID _r_listview_deleteallitems (HWND hwnd, UINT ctrl);

BOOL _r_listview_getcheckstate (HWND hwnd, UINT ctrl, size_t item);
INT _r_listview_getcolumnwidth (HWND hwnd, UINT ctrl, INT column);
size_t _r_listview_getitemcount (HWND hwnd, UINT ctrl, BOOL list_checked = FALSE);
INT _r_listview_getcolumncount (HWND hwnd, UINT ctrl);
LPARAM _r_listview_getlparam (HWND hwnd, UINT ctrl, size_t item);
rstring _r_listview_gettext (HWND hwnd, UINT ctrl, size_t item, size_t subitem);

BOOL _r_listview_setcheckstate (HWND hwnd, UINT ctrl, size_t item, BOOL state);
BOOL _r_listview_setcolumnsortindex (HWND hwnd, UINT ctrl, INT column, INT arrow);
BOOL _r_listview_setgroup (HWND hwnd, UINT ctrl, UINT item, size_t group_id);
INT _r_listview_setitem (HWND hwnd, UINT ctrl, LPCWSTR text, size_t item, size_t subitem, size_t image = LAST_VALUE, size_t group_id = LAST_VALUE, LPARAM lparam = 0);
BOOL _r_listview_setlparam (HWND hwnd, UINT ctrl, UINT item, LPARAM param);
DWORD _r_listview_setstyle (HWND hwnd, UINT ctrl, DWORD exstyle);

VOID _r_listview_resizeonecolumn (HWND hwnd, UINT ctrl_id);

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, UINT ctrl, LPCWSTR text, HTREEITEM parent = nullptr, size_t image = LAST_VALUE, LPARAM lparam = 0);
LPARAM _r_treeview_getlparam (HWND hwnd, UINT ctrl, HTREEITEM item);
DWORD _r_treeview_setstyle (HWND hwnd, UINT ctrl, DWORD exstyle, INT height);

/*
	Control: statusbar
*/

BOOL _r_status_settext (HWND hwnd, UINT ctrl, INT part, LPCWSTR text);
VOID _r_status_setstyle (HWND hwnd, UINT ctrl, INT height);

/*
	Exported function definitions
*/

typedef BOOL (WINAPI *CWMF) (UINT, DWORD); // ChangeWindowMessageFilter
typedef BOOL (WINAPI *CWMFEX) (HWND, UINT, DWORD, PCHANGEFILTERSTRUCT); // ChangeWindowMessageFilterEx
typedef BOOL (WINAPI *IW64P) (HANDLE, PBOOL); // IsWow64Process
typedef HRESULT (WINAPI *LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
typedef VOID (WINAPI *TDI) (TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect
typedef int (WINAPI *SHCDEX) (HWND, LPCTSTR, const SECURITY_ATTRIBUTES*); // SHCreateDirectoryEx
typedef BOOL (WINAPI *QFPIN) (HANDLE, DWORD, LPWSTR, PDWORD); // QueryFullProcessImageName
typedef BOOL (WINAPI *MFACS) (PCWSTR, PDWORD, PDWORD); // MapFileAndCheckSumW
