// routine++
// Copyright (c) 2013-2015 Henry++
//
// lastmod: Oct 7, 2015

#include "routine.h"

/*
	Write debug log to console
*/

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...)
{
	CString buffer;

	DWORD dwLE = GetLastError ();
	DWORD dwTC = GetTickCount ();
	DWORD dwPID = GetCurrentProcessId ();
	DWORD dwTID = GetCurrentThreadId ();

	SYSTEMTIME lt = {0};
	GetLocalTime (&lt);

	if (format)
	{
		va_list args = nullptr;
		va_start (args, format);

		buffer.FormatV (format, args);

		va_end (args);
	}

	OutputDebugString (_r_fmt (L"[%02d:%02d:%02d] TC=%010d, PID=%04d, TID=%04d, LE=%d (0x%x), FN=%s, FL=%s:%d, T=%s\r\n", lt.wHour, lt.wMinute, lt.wSecond, dwTC, dwPID, dwTID, dwLE, dwLE, function, file, line, buffer.IsEmpty () ? L"<none>" : buffer));
}

/*
	Format strings, dates, numbers
*/

CString _r_fmt (LPCWSTR format, ...)
{
	CString buffer;

	va_list args = nullptr;
	va_start (args, format);

	StringCchVPrintf (buffer.GetBuffer (ROUTINE_BUFFER_LENGTH), ROUTINE_BUFFER_LENGTH, format, args);
	buffer.ReleaseBuffer ();

	va_end (args);

	return buffer;
}

CString _r_fmt_date (LPFILETIME ft, const DWORD flags)
{
	CString buffer;
	DWORD pflags = flags;

	SHFormatDateTime (ft, &pflags, buffer.GetBuffer (ROUTINE_BUFFER_LENGTH), ROUTINE_BUFFER_LENGTH);
	buffer.ReleaseBuffer ();

	return buffer;
}

CString _r_fmt_date (__time64_t ut, const DWORD flags)
{
	CString buffer;
	FILETIME ft = {0};

	_r_unixtime_to_filetime (ut, &ft);
	_r_fmt_date (&ft, flags);

	return buffer;
}

CString _r_fmt_size64 (DWORDLONG size)
{
	static const wchar_t *sizes[] = {L"B", L"KB", L"MB", L"GB", L"TB", L"PB"};

	INT div = 0;
	SIZE_T rem = 0;

	while (size >= 1000 && div < _countof (sizes))
	{
		rem = (size % 1024);
		div++;
		size /= 1024;
	}

	double size_d = (float)size + (float)rem / 1024.0;

	size_d += 0.001; // round up

	CString buffer;
	buffer.Format (L"%.2f %s", size_d, sizes[div]);

	return buffer;
}

/*
	System messages
*/

INT _r_msg (HWND hwnd, UINT type, LPCWSTR title, LPCWSTR format, ...)
{
	CString buffer;

	INT result = 0;

	va_list args = nullptr;
	va_start (args, format);

	buffer.FormatV (format, args);

	va_end (args);

	if (_r_system_validversion (6, 0))
	{
		TASKDIALOGCONFIG tdc = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetModuleHandle (nullptr);
		tdc.pszWindowTitle = title;
		tdc.pszContent = buffer;
		tdc.pfCallback = _r_msg_callback;

		if (IsWindowVisible (hwnd))
		{
			tdc.dwFlags |= TDF_POSITION_RELATIVE_TO_WINDOW;
		}

		TDI _TaskDialogIndirect = (TDI)GetProcAddress (GetModuleHandle (L"comctl32.dll"), "TaskDialogIndirect");

		if ((type & MB_YESNO) == MB_YESNO)
		{
			tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		}

		if ((type & MB_ICONHAND) == MB_ICONHAND)
		{
			tdc.pszMainIcon = TD_ERROR_ICON;
		}
		else if ((type & MB_ICONQUESTION) == MB_ICONQUESTION || (type & MB_ICONASTERISK) == MB_ICONASTERISK)
		{
			tdc.pszMainIcon = TD_INFORMATION_ICON;
		}
		else if ((type & MB_ICONEXCLAMATION) == MB_ICONQUESTION)
		{
			tdc.pszMainIcon = TD_WARNING_ICON;
		}

		if (_TaskDialogIndirect)
		{
			_TaskDialogIndirect (&tdc, &result, nullptr, nullptr);
		}
	}

	if (!result)
	{
		MSGBOXPARAMS mbp = {0};

		mbp.cbSize = sizeof (mbp);
		mbp.hwndOwner = hwnd;
		mbp.hInstance = GetModuleHandle (nullptr);
		mbp.dwStyle = type | MB_TOPMOST;
		mbp.lpszCaption = title;
		mbp.lpszText = buffer;

		result = MessageBoxIndirect (&mbp);
	}

	return result;
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			_r_windowtotop (hwnd, TRUE);
			return TRUE;
		}

		case TDN_HYPERLINK_CLICKED:
		{
			ShellExecute (hwnd, nullptr, (LPCWSTR)lparam, nullptr, nullptr, SW_SHOWDEFAULT);
			return TRUE;
		}
	}

	return FALSE;
}

/*
	Clipboard operations
*/

CString _r_clipboard_get (HWND hwnd)
{
	CString buffer;

	if (OpenClipboard (hwnd))
	{
		HGLOBAL h = GetClipboardData (CF_UNICODETEXT);

		if (h)
		{
			buffer = (LPCWSTR)GlobalLock (h);

			GlobalUnlock (h);
		}
	}

	CloseClipboard ();

	return buffer;
}

VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length)
{
	if (OpenClipboard (hwnd))
	{
		if (EmptyClipboard ())
		{
			HGLOBAL h = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, length * sizeof (WCHAR));

			if (h)
			{
				memcpy (GlobalLock (h), text, length * sizeof (WCHAR));
				SetClipboardData (CF_UNICODETEXT, h);

				GlobalUnlock (h);
			}
		}

		CloseClipboard ();
	}
}







/*

VOID _r_locale_set (LCID locale)
{
	_r_lcid = locale;

	if (_r_system_validversion (6, 0))
	{
		SetThreadUILanguage ((LANGID)locale);
	}
	else
	{
		SetThreadLocale (locale);
	}

	_r_cfg_write (L"Language", _r_lcid);
}

BOOL CALLBACK _r_locale_enum (HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR lparam)
{
	WCHAR buffer[MAX_PATH] = {0};

	INT item = max (0, (INT)SendMessage ((HWND)lparam, CB_GETCOUNT, 0, NULL));

	if (GetLocaleInfo (language, _r_system_validversion (6, 1) ? LOCALE_SENGLISHDISPLAYNAME : LOCALE_SLANGUAGE, buffer, MAX_PATH))
	{
		SendMessage ((HWND)lparam, CB_INSERTSTRING, item, (LPARAM)buffer);
		SendMessage ((HWND)lparam, CB_SETITEMDATA, item, (LPARAM)language);

		if (language == _r_lcid)
		{
			SendMessage ((HWND)lparam, CB_SETCURSEL, item, 0);
		}
	}

	return TRUE;
}
*/

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, INT ctrl, LPCWSTR text, INT width, INT subitem, INT fmt)
{
	LVCOLUMN lvc = {0};

	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl), &rc);

	if (width > 100)
	{
		width = ROUTINE_PERCENT_OF (width, rc.right);
	}

	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
	lvc.pszText = (LPWSTR)text;
	lvc.fmt = fmt;
	lvc.cx = (INT)ROUTINE_PERCENT_VAL (width, rc.right);
	lvc.iSubItem = subitem;

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_INSERTCOLUMN, (WPARAM)subitem, (LPARAM)&lvc);
}

INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl, INT column)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl), &rc);

	return ROUTINE_PERCENT_OF (SendDlgItemMessage (hwnd, ctrl, LVM_GETCOLUMNWIDTH, column, NULL), rc.right);
}

INT _r_listview_addgroup (HWND hwnd, INT ctrl, INT group_id, LPCWSTR text, UINT align, UINT state)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (LVGROUP);
	lvg.mask = LVGF_HEADER | LVGF_GROUPID;
	lvg.pszHeader = (LPWSTR)text;
	lvg.iGroupId = group_id;

	if (align)
	{
		lvg.mask |= LVGF_ALIGN;
		lvg.uAlign = align;
	}

	if (state)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
	}

	SendDlgItemMessage (hwnd, ctrl, LVM_ENABLEGROUPVIEW, TRUE, NULL);

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_INSERTGROUP, (WPARAM)-1, (LPARAM)&lvg);
}

INT _r_listview_additem (HWND hwnd, INT ctrl, LPCWSTR text, INT item, INT subitem, INT image, INT group_id, LPARAM lparam)
{
	LVITEM lvi = {0};

	if (item == -1)
	{
		lvi.iItem = (INT)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMCOUNT, 0, NULL);

		if (subitem)
		{
			lvi.iItem -= 1;
		}
	}
	else
	{
		lvi.iItem = item;
	}

	lvi.iSubItem = subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = (LPWSTR)text;
	}

	if (image != -1)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = image;
	}

	if (group_id != -1)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = group_id;
	}

	if (lparam && !subitem)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}
	else if (lparam && subitem)
	{
		LVITEM lvi_param = {0};

		lvi_param.mask = LVIF_PARAM;
		lvi_param.iItem = item;
		lvi_param.lParam = lparam;

		SendDlgItemMessage (hwnd, ctrl, LVM_SETITEM, 0, (LPARAM)&lvi_param);
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl, (subitem > 0) ? LVM_SETITEM : LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

LPARAM _r_listview_getlparam (HWND hwnd, INT ctrl, INT item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;

	SendDlgItemMessage (hwnd, ctrl, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

CString _r_listview_gettext (HWND hwnd, INT ctrl, INT item, INT subitem)
{
	CString buffer;

	DWORD length = 0;
	DWORD out_length = 0;

	LVITEM lvi = {0};

	lvi.iSubItem = subitem;

	do
	{
		length += ROUTINE_BUFFER_LENGTH;

		lvi.pszText = buffer.GetBuffer (length);
		lvi.cchTextMax = length;

		out_length = (DWORD)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMTEXT, item, (LPARAM)&lvi);

		buffer.ReleaseBuffer ();
	}
	while (out_length == (length - 1));

	return buffer;
}

DWORD _r_listview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl), L"Explorer", nullptr);

	return (DWORD)SendDlgItemMessage (hwnd, ctrl, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl, LPCWSTR text, INT image, LPARAM lparam)
{
	TVINSERTSTRUCT tvi = {0};

	tvi.itemex.mask = TVIF_TEXT;
	tvi.itemex.pszText = (LPWSTR)text;

	if (image != -1)
	{
		tvi.itemex.mask |= (TVIF_IMAGE | TVIF_SELECTEDIMAGE);
		tvi.itemex.iImage = image;
		tvi.itemex.iSelectedImage = image;
	}

	if (lparam)
	{
		tvi.itemex.mask |= TVIF_PARAM;
		tvi.itemex.lParam = lparam;
	}

	return (HTREEITEM)SendDlgItemMessage (hwnd, ctrl, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getlparam (HWND hwnd, INT ctrl, HTREEITEM item)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = item;

	SendDlgItemMessage (hwnd, ctrl, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

DWORD _r_treeview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle, INT height)
{
	if (height)
	{
		SendDlgItemMessage (hwnd, ctrl, TVM_SETITEMHEIGHT, (WPARAM)height, 0);
	}

	SetWindowTheme (GetDlgItem (hwnd, ctrl), L"Explorer", nullptr);

	return (DWORD)SendDlgItemMessage (hwnd, ctrl, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: statusbar
*/

BOOL _r_status_settext (HWND hwnd, INT ctrl, INT part, LPCWSTR text)
{
	return (BOOL)SendDlgItemMessage (hwnd, ctrl, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
}

VOID _r_status_setstyle (HWND hwnd, INT ctrl, INT height)
{
	SendDlgItemMessage (hwnd, ctrl, SB_SETMINHEIGHT, (WPARAM)height, NULL);
	SendDlgItemMessage (hwnd, ctrl, WM_SIZE, 0, NULL);
}




BOOL _r_system_adminstate ()
{
	BOOL result = FALSE;
	DWORD status = 0, acl_size = 0, ps_size = sizeof (PRIVILEGE_SET);

	HANDLE token = nullptr, impersonation_token = nullptr;

	PRIVILEGE_SET ps = {0};
	GENERIC_MAPPING gm = {0};

	PACL acl = nullptr;
	PSID sid = nullptr;
	PSECURITY_DESCRIPTOR sd = nullptr;

	SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;

	__try
	{
		if (!OpenThreadToken (GetCurrentThread (), TOKEN_DUPLICATE | TOKEN_QUERY, TRUE, &token))
		{
			if (GetLastError () != ERROR_NO_TOKEN || !OpenProcessToken (GetCurrentProcess (), TOKEN_DUPLICATE | TOKEN_QUERY, &token))
			{
				__leave;
			}
		}

		if (!DuplicateToken (token, SecurityImpersonation, &impersonation_token))
		{
			__leave;
		}

		if (!AllocateAndInitializeSid (&sia, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
		{
			__leave;
		}

		sd = LocalAlloc (LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

		if (!sd || !InitializeSecurityDescriptor (sd, SECURITY_DESCRIPTOR_REVISION))
		{
			__leave;
		}

		acl_size = sizeof (ACL) + sizeof (ACCESS_ALLOWED_ACE) + GetLengthSid (sid) - sizeof (DWORD);
		acl = (PACL)LocalAlloc (LPTR, acl_size);

		if (!acl || !InitializeAcl (acl, acl_size, ACL_REVISION2) || !AddAccessAllowedAce (acl, ACL_REVISION2, ACCESS_READ | ACCESS_WRITE, sid) || !SetSecurityDescriptorDacl (sd, TRUE, acl, FALSE))
		{
			__leave;
		}

		SetSecurityDescriptorGroup (sd, sid, FALSE);
		SetSecurityDescriptorOwner (sd, sid, FALSE);

		if (!IsValidSecurityDescriptor (sd))
		{
			__leave;
		}

		gm.GenericRead = ACCESS_READ;
		gm.GenericWrite = ACCESS_WRITE;
		gm.GenericExecute = 0;
		gm.GenericAll = ACCESS_READ | ACCESS_WRITE;

		if (!AccessCheck (sd, impersonation_token, ACCESS_READ, &gm, &ps, &ps_size, &status, &result))
		{
			result = FALSE;
			__leave;
		}
	}

	__finally
	{
		if (acl)
		{
			LocalFree (acl);
		}

		if (sd)
		{
			LocalFree (sd);
		}

		if (sid)
		{
			FreeSid (sid);
		}

		if (impersonation_token)
		{
			CloseHandle (impersonation_token);
		}

		if (token)
		{
			CloseHandle (token);
		}
	}

	return result;
}

BOOL _r_system_uacstate ()
{
	HANDLE token = nullptr;
	DWORD out_length = 0;
	TOKEN_ELEVATION_TYPE tet;

	if (!_r_system_validversion (6, 0))
	{
		return FALSE;
	}

	if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token) && GetTokenInformation (token, TokenElevationType, &tet, sizeof (TOKEN_ELEVATION_TYPE), &out_length) && tet == TokenElevationTypeLimited)
	{
		return TRUE;
	}

	if (token)
	{
		CloseHandle (token);
	}

	return FALSE;
}

BOOL _r_system_setprivilege (LPCWSTR privilege, BOOL enable)
{
	HANDLE token = nullptr;

	LUID luid = {0};
	TOKEN_PRIVILEGES tp = {0};

	BOOL result = FALSE;

	if (OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
	{
		if (LookupPrivilegeValue (nullptr, privilege, &luid))
		{
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = luid;
			tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;

			if (AdjustTokenPrivileges (token, FALSE, &tp, sizeof (tp), nullptr, nullptr) && GetLastError () == ERROR_SUCCESS)
			{
				result = TRUE;
			}
		}
	}

	if (token)
	{
		CloseHandle (token);
	}

	return result;
}

BOOL _r_system_validversion (DWORD major, DWORD minor)
{
	OSVERSIONINFOEX osvi = {0};
	DWORDLONG mask = 0;

	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
	osvi.dwMajorVersion = major;
	osvi.dwMinorVersion = minor;

	VER_SET_CONDITION (mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION (mask, VER_MINORVERSION, VER_GREATER_EQUAL);

	return VerifyVersionInfo (&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask);
}

VOID _r_windowcenter (HWND hwnd)
{
	HWND parent = GetParent (hwnd);
	RECT rc_child = {0}, rc_parent = {0};

	if (!parent || !IsWindowVisible (parent))
	{
		parent = GetDesktopWindow ();
	}

	GetWindowRect (hwnd, &rc_child);
	GetWindowRect (parent, &rc_parent);

	INT width = rc_child.right - rc_child.left, height = rc_child.bottom - rc_child.top;
	INT x = ((rc_parent.right - rc_parent.left) - width) / 2 + rc_parent.left, y = ((rc_parent.bottom - rc_parent.top) - height) / 2 + rc_parent.top;
	INT screen_width = GetSystemMetrics (SM_CXSCREEN), screen_height = GetSystemMetrics (SM_CYSCREEN);

	x = max (0, x);
	y = max (0, y);

	if (x + width > screen_width) x = screen_width - width;
	if (y + height > screen_height) y = screen_height - height;

	MoveWindow (hwnd, x, y, width, height, FALSE);
}

VOID _r_windowtoggle (HWND hwnd, BOOL show)
{
	if (show || !IsWindowVisible (hwnd))
	{
		ShowWindow (hwnd, SW_SHOW);

		if (GetLastError () == ERROR_ACCESS_DENIED)
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_RESTORE, NULL); // uipi fix
		}

		SetForegroundWindow (hwnd);
		SwitchToThisWindow (hwnd, TRUE);
	}
	else
	{
		ShowWindow (hwnd, SW_HIDE);
	}
}

VOID _r_windowtotop (HWND hwnd, BOOL enable)
{
	SetWindowPos (hwnd, (enable ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE);
}

HWND _r_setcontroltip (HWND hwnd, INT ctrl, LPWSTR text)
{
	HWND tip = CreateWindowEx (0, TOOLTIPS_CLASS, nullptr, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, GetModuleHandle (nullptr), nullptr);

	TOOLINFO ti = {0};

	ti.cbSize = sizeof (TOOLINFO);
	ti.hwnd = hwnd;
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.lpszText = text;
	ti.uId = (UINT_PTR)GetDlgItem (hwnd, ctrl);

	SendMessage (tip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	return tip;
}

BOOL _r_seteditbaloontip (HWND hwnd, INT ctrl, LPCWSTR title, LPCWSTR text, INT icon)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (EDITBALLOONTIP);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon;

	return (BOOL)SendDlgItemMessage (hwnd, ctrl, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

BOOL _r_file_is_exists (LPCWSTR path)
{
	return (GetFileAttributes (path) != INVALID_FILE_ATTRIBUTES);
}

DWORD64 _r_file_size (HANDLE h)
{
	LARGE_INTEGER size = {0};

	GetFileSizeEx (h, &size);

	return size.QuadPart;
}

__time64_t _r_unixtime_now ()
{
	__time64_t t = 0;
	_time64 (&t);

	return t;
}

VOID _r_unixtime_to_filetime (__time64_t t, LPFILETIME pft)
{
	if (t)
	{
		LONGLONG ll = Int32x32To64 (t, 10000000) + 116444736000000000ui64; // 64 bit value

		pft->dwLowDateTime = (DWORD)ll;
		pft->dwHighDateTime = (DWORD)(ll >> 32);
	}
}

VOID _r_unixtime_to_systemtime (__time64_t t, LPSYSTEMTIME pst)
{
	FILETIME ft = {0};

	_r_unixtime_to_filetime (t, &ft);

	FileTimeToSystemTime (&ft, pst);
}

/*
	return 1 if v1 > v2
	return 0 if v1 = v2
	return -1 if v1 < v2
	*/

INT _r_string_versioncompare (LPCWSTR v1, LPCWSTR v2)
{
	INT oct_v1[4] = {0};
	INT oct_v2[4] = {0};

	swscanf_s (v1, L"%d.%d.%d.%d", &oct_v1[0], &oct_v1[1], &oct_v1[2], &oct_v1[3]);
	swscanf_s (v2, L"%d.%d.%d.%d", &oct_v2[0], &oct_v2[1], &oct_v2[2], &oct_v2[3]);

	for (INT i = 0; i < _countof (oct_v1); i++)
	{
		if (oct_v1[i] > oct_v2[i])
		{
			return 1;
		}
		else if (oct_v1[i] < oct_v2[i])
		{
			return -1;
		}
	}

	return 0;
}

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT width, INT height)
{
	HICON result = nullptr;

	LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (GetModuleHandle (L"comctl32.dll"), "LoadIconWithScaleDown");

	if (_LoadIconWithScaleDown)
	{
		_LoadIconWithScaleDown (h, name, width, height, &result);
	}

	if (!result)
	{
		result = (HICON)LoadImage (h, name, IMAGE_ICON, width, height, 0);
	}

	return result;
}




/*
BOOL _r_skipuac_run ()
{
	CloseHandle (_r_hmutex);

	if (_r_skipuac_is_present (TRUE))
	{
		return TRUE;
	}
	else
	{
		WCHAR buffer[MAX_PATH] = {0};

		SHELLEXECUTEINFO shex = {0};

		shex.cbSize = sizeof (shex);
		shex.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI;
		shex.lpVerb = L"runas";
		shex.nShow = SW_NORMAL;

		GetModuleFileName (nullptr, buffer, MAX_PATH);
		shex.lpFile = buffer;

		if (ShellExecuteEx (&shex))
		{
			return TRUE;
		}
	}

	_r_hmutex = CreateMutex (nullptr, FALSE, _r_app_name_short);

	return FALSE;
}

BOOL _r_skipuac_is_present (BOOL checkandrun)
{
	BOOL result = FALSE;

	if (_r_system_validversion (6, 0))
	{
		ITaskService* service = nullptr;
		ITaskFolder* folder = nullptr;
		IRegisteredTask* registered_task = nullptr;

		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (SUCCEEDED (folder->GetTask (_r_fmt (ROUTINE_TASKSCHD_NAME, _r_app_name_short).GetBuffer (), &registered_task)))
					{
						if (checkandrun)
						{
							INT numargs = 0;
							LPWSTR* args = CommandLineToArgvW (GetCommandLine (), &numargs);

							CString buffer;

							for (INT i = 1; i < numargs; i++)
							{
								buffer.Append (args[i]);
								buffer.Append (L" ");
							}

							LocalFree (args);

							variant_t ticker = buffer.Trim ().GetBuffer ();

							IRunningTask* ppRunningTask = nullptr;

							if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_AS_SELF, 0, nullptr, &ppRunningTask)) && ppRunningTask)
							{
								TASK_STATE state;

								if (SUCCEEDED (ppRunningTask->get_State (&state)))
								{
									if (state == TASK_STATE_QUEUED || state == TASK_STATE_READY)
									{
										Sleep (2000);
										ppRunningTask->get_State (&state);
									}

									if (state == TASK_STATE_RUNNING)
									{
										result = TRUE;
									}
									else if (state == TASK_STATE_UNKNOWN || state == TASK_STATE_DISABLED)
									{
										result = FALSE;
									}
								}

								ppRunningTask->Release ();
							}
						}
						else
						{
							result = TRUE;
						}

						registered_task->Release ();
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}

BOOL _r_skipuac_cancer (BOOL remove)
{
	BOOL result = FALSE;
	BOOL action_result = FALSE;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	ITaskDefinition* task = nullptr;
	IRegistrationInfo* reginfo = nullptr;
	IPrincipal* principal = nullptr;
	ITaskSettings* settings = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;
	IRegisteredTask* registered_task = nullptr;

	if (_r_system_validversion (6, 0))
	{
		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (VOID**)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (remove)
					{
						result = (folder->DeleteTask (_r_fmt (ROUTINE_TASKSCHD_NAME, _r_app_name_short).GetBuffer (), 0) == S_OK);
					}
					else
					{
						if (SUCCEEDED (service->NewTask (0, &task)))
						{
							if (SUCCEEDED (task->get_RegistrationInfo (&reginfo)))
							{
								reginfo->put_Author (_r_app_author);
								reginfo->Release ();
							}

							if (SUCCEEDED (task->get_Principal (&principal)))
							{
								principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
								principal->Release ();
							}

							if (SUCCEEDED (task->get_Settings (&settings)))
							{
								settings->put_StartWhenAvailable (VARIANT_BOOL (FALSE));
								settings->put_DisallowStartIfOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_StopIfGoingOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
								settings->put_ExecutionTimeLimit (L"PT0S");
								settings->Release ();
							}

							if (SUCCEEDED (task->get_Actions (&action_collection)))
							{
								if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
								{
									if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (VOID**)&exec_action)))
									{
										WCHAR path[MAX_PATH] = {0};
										GetModuleFileName (nullptr, path, MAX_PATH);
										PathQuoteSpaces (path);

										if (SUCCEEDED (exec_action->put_Path (path)) && SUCCEEDED (exec_action->put_Arguments (L"$(Arg0)")))
										{
											action_result = TRUE;
										}

										exec_action->Release ();
									}

									action->Release ();
								}

								action_collection->Release ();
							}

							if (action_result && SUCCEEDED (folder->RegisterTaskDefinition (_r_fmt (ROUTINE_TASKSCHD_NAME, _r_app_name_short).GetBuffer (), task, TASK_CREATE_OR_UPDATE, _variant_t (), _variant_t (), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t (), &registered_task)))
							{
								result = TRUE;
								registered_task->Release ();
							}

							task->Release ();
						}
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}
*/
