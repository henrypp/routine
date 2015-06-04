// _routine (library)
//
// copyright (c) henry++
// lastmod: Apr 30, 2015

#ifndef __ROUTINE_H__
#define __ROUTINE_H__

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
#define ROUTINE_BUFFER_LENGTH 256
#define ROUTINE_UPDATE_PERIOD 2

#define ROUTINE_PERCENT_OF(val, total) (INT)ceil((double(val) / double(total)) * 100.0)
#define ROUTINE_PERCENT_VAL(val, total) (double(total) * double(val) / 100.0)

typedef VOID (WINAPI *PTDI) (TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect

VOID mb(INT code);

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

BOOL _r_helper_fileexists(LPCWSTR path);
CString _r_helper_format(LPCWSTR format, ...);
CString _r_helper_formatsize64(DWORDLONG size);
__time64_t _r_helper_unixtime64();
BOOL _r_helper_unixtime2systemtime(__time64_t t, LPSYSTEMTIME pst);
CString _r_helper_formatdate(LPSYSTEMTIME pst);
INT _r_helper_versioncompare(LPCWSTR v1, LPCWSTR v2);

BOOL _r_skipuac_run();
BOOL _r_skipuac_is_present(BOOL checkandrun);
BOOL _r_skipuac_cancer(BOOL remove);

#endif // __ROUTINE_H__
