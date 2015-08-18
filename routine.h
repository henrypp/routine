// routine++
// Copyright © 2013, 2015 Henry++
//
// lastmod: Aug 18, 2015

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

#include "main.h"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "credui.lib")

extern HWND _r_hwnd;
extern LCID _r_lcid;
extern WCHAR _r_cfg_path[MAX_PATH];
extern HANDLE _r_hmutex;

#define ROUTINE_TASKSCHD_NAME APP_NAME_SHORT L"SkipUAC"
#define ROUTINE_BUFFER_LENGTH 4096
#define ROUTINE_UPDATE_PERIOD 2

#define ROUTINE_PERCENT_OF(val, total) (INT)ceil((double(val) / double(total)) * 100.0)
#define ROUTINE_PERCENT_VAL(val, total) (double(total) * double(val) / 100.0)

#define WDEBUG0() _r_dbgW(__FUNCTIONW__, TEXT(__FILE__), __LINE__, nullptr)
#define WDEBUG1(a) _r_dbgW(__FUNCTIONW__, TEXT(__FILE__), __LINE__, L"%s", a)
#define WDEBUG2(a, b) _r_dbgW(__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, b)
#define WDEBUG3(a, b, c) _r_dbgW(__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, b, c)
#define WDEBUG4(a, b, c, d) _r_dbgW(__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, b, c, d)

#define ADEBUG0() _r_dbgA(__FUNCTION__, __FILE__, __LINE__, nullptr)
#define ADEBUG1(a) _r_dbgA(__FUNCTION__, __FILE__, __LINE__, "%s", a)
#define ADEBUG2(a, b) _r_dbgA(__FUNCTION__, __FILE__, __LINE__, a, b)
#define ADEBUG3(a, b, c) _r_dbgA(__FUNCTION__, __FILE__, __LINE__, a, b, c)
#define ADEBUG4(a, b, c, d) _r_dbgA(__FUNCTION__, __FILE__, __LINE__, a, b, c, d)

typedef VOID (WINAPI *PTDI) (TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect

VOID _r_dbgA (LPCSTR function, LPCSTR file, DWORD line, LPCSTR format, ...);
VOID _r_dbgW (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...);

BOOL _r_initialize(DLGPROC proc);
VOID _r_uninitialize(BOOL restart);

BOOL _r_aboutbox(HWND hwnd);

INT _r_msg(LPCWSTR text);
INT _r_msg(DWORD style, LPCWSTR format, ...);
HRESULT CALLBACK _r_msg_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR ref);

VOID _r_autorun_cancer(LPCWSTR name, BOOL remove);
BOOL _r_autorun_is_present(LPCWSTR name);

VOID _r_cfg_init();
BOOL _r_cfg_is_portable();
VOID _r_cfg_cancer(BOOL makeportable);
UINT _r_cfg_read(LPCWSTR key, INT def);
CString _r_cfg_read(LPCWSTR key, LPCWSTR def);

BOOL _r_cfg_write(LPCWSTR key, LPCWSTR val);
BOOL _r_cfg_write(LPCWSTR key, DWORD val);

CString _r_clipboard_get(VOID);
VOID _r_clipboard_set(LPCWSTR text, SIZE_T length);

VOID _r_locale_set(LCID locale);
CString _r_locale(UINT id);
BOOL CALLBACK _r_locale_enum(HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR lparam);

UINT WINAPI _r_updatecheckcallback(LPVOID lparam);
BOOL _r_updatecheck(BOOL is_periodical);

INT _r_listview_addcolumn(HWND hwnd, INT ctrl, LPCWSTR text, INT width, INT subitem, INT fmt);
INT _r_listview_addgroup(HWND hwnd, INT ctrl, INT group_id, LPCWSTR text, UINT align = 0, UINT state = 0);
INT _r_listview_additem(HWND hwnd, INT ctrl, LPCWSTR text, INT item, INT subitem, INT image = -1, INT group_id = -1, LPARAM lparam = 0);
INT _r_listview_getcolumnwidth(HWND hwnd, INT ctrl, INT column);
LPARAM _r_listview_getlparam(HWND hwnd, INT ctrl, INT item);
CString _r_listview_gettext(HWND hwnd, INT ctrl, INT item, INT subitem);
DWORD _r_listview_setstyle(HWND hwnd, INT ctrl, DWORD exstyle);

BOOL _r_status_settext(HWND hwnd, INT ctrl, INT part, LPCWSTR text);
VOID _r_status_setstyle(HWND hwnd, INT ctrl, INT height);

HTREEITEM _r_treeview_additem(HWND hwnd, INT ctrl, LPCWSTR text, INT image = -1, LPARAM lparam = 0);
DWORD _r_treeview_setstyle(HWND hwnd, INT ctrl, DWORD exstyle, INT height);

BOOL _r_system_adminstate(VOID);
BOOL _r_system_uacstate(VOID);
BOOL _r_system_setprivilege(LPCWSTR privilege, BOOL enable);
BOOL _r_system_validversion(DWORD major, DWORD minor);

VOID _r_windowcenter(HWND hwnd);
VOID _r_windowtoggle(HWND hwnd, BOOL show);
VOID _r_windowtotop(HWND hwnd, BOOL enable);

HWND _r_setcontroltip(HWND hwnd, INT ctrl, LPWSTR text);
BOOL _r_seteditbaloontip(HWND hwnd, INT ctrl, LPCWSTR title, LPCWSTR text, INT icon);

BOOL _r_file_is_exists (LPCWSTR path);
DWORD64 _r_file_size (HANDLE h);

CStringA _r_fmtA (LPCSTR format, ...);
CStringW _r_fmtW (LPCWSTR format, ...);

CString _r_fmt_date (LPFILETIME ft, const DWORD flags = FDTF_DEFAULT); // see SHFormatDateTime flags definition
CString _r_fmt_date (__time64_t ut, const DWORD flags = FDTF_DEFAULT);

CString _r_fmt_size64 (DWORDLONG size);

__time64_t _r_unixtime ();
VOID _r_unixtime_to_filetime (__time64_t t, LPFILETIME pft);
VOID _r_unixtime_to_systemtime (__time64_t t, LPSYSTEMTIME pst);

INT _r_versioncompare (LPCWSTR v1, LPCWSTR v2);

BOOL _r_skipuac_run();
BOOL _r_skipuac_is_present(BOOL checkandrun);
BOOL _r_skipuac_cancer(BOOL remove);

#ifdef UNICODE
#define _r_dbg _r_dbgW
#define _r_fmt _r_fmtW
#else
#define _r_dbg _r_dbgA
#define _r_fmt _r_fmtA
#endif
