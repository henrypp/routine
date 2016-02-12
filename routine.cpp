// routine++
// Copyright (c) 2012-2016 Henry++

#include "routine.h"

/*
	Write debug log to console
*/

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...)
{
	rstring buffer;

	DWORD dwLE = GetLastError ();
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

		OutputDebugString (_r_fmt (L"[%02d:%02d:%02d] PID=%04d, TID=%04d, LE=%d (0x%x), FN=%s, FL=%s:%d, T=%s\r\n", lt.wHour, lt.wMinute, lt.wSecond, dwPID, dwTID, dwLE, dwLE, function, file, line, buffer.IsEmpty () ? L"<none>" : buffer));
	}
	else
	{
		OutputDebugString (L"\r\n");
	}
}

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR format, ...)
{
	rstring result;

	va_list args = nullptr;
	va_start (args, format);

	result.FormatV (format, args);

	va_end (args);

	return result;
}

rstring _r_fmt_date (LPFILETIME ft, const DWORD flags)
{
	DWORD pflags = flags;

	rstring result;

	SHFormatDateTime (ft, &pflags, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_fmt_date (__time64_t ut, const DWORD flags)
{
	FILETIME ft = {0};

	_r_unixtime_to_filetime (ut, &ft);

	return _r_fmt_date (&ft, flags);
}

rstring _r_fmt_size64 (DWORDLONG size)
{
	static const WCHAR *sizes[] = {L"B", L"KB", L"MB", L"GB", L"TB", L"PB"};

	rstring result;

	INT div = 0;
	SIZE_T rem = 0;

	while (size >= 1000 && div < _countof (sizes))
	{
		rem = (size % 1024);
		div++;
		size /= 1024;
	}

	double size_d = double (size) + double (rem) / 1024.0;

	size_d += 0.001; // round up


	return result.Format (L"%.2f %s", size_d, sizes[div]);
}

/*
	System messages
*/

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR format, ...)
{
	rstring buffer;

	INT result = 0;

	va_list args = nullptr;
	va_start (args, format);

	buffer.FormatV (format, args);

	va_end (args);

	if (_r_sys_validversion (6, 0))
	{
		TDI _TaskDialogIndirect = (TDI)GetProcAddress (GetModuleHandle (L"comctl32.dll"), "TaskDialogIndirect");

		if (_TaskDialogIndirect)
		{
			TASKDIALOGCONFIG tdc = {0};

			tdc.cbSize = sizeof (TASKDIALOGCONFIG);
			tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
			tdc.hwndParent = hwnd;
			tdc.pszWindowTitle = title;
			tdc.pszContent = buffer;
			tdc.pfCallback = &_r_msg_callback;

			// flags
			if (IsWindowVisible (hwnd)) { tdc.dwFlags |= TDF_POSITION_RELATIVE_TO_WINDOW; }

			// buttons
			if ((flags & MB_TYPEMASK) == MB_YESNO) { tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON; }
			if ((flags & MB_TYPEMASK) == MB_YESNOCANCEL) { tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON; }
			else if ((flags & MB_TYPEMASK) == MB_OKCANCEL) { tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON; }
			else if ((flags & MB_TYPEMASK) == MB_RETRYCANCEL) { tdc.dwCommonButtons = TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON; }

			// icons
			if ((flags & MB_ICONMASK) == MB_ICONASTERISK) { tdc.pszMainIcon = TD_INFORMATION_ICON; }
			else if ((flags & MB_ICONMASK) == MB_ICONEXCLAMATION) { tdc.pszMainIcon = TD_WARNING_ICON; }
			else if ((flags & MB_ICONMASK) == MB_ICONQUESTION) { tdc.pszMainIcon = IDI_QUESTION; }
			else if ((flags & MB_ICONMASK) == MB_ICONHAND) { tdc.pszMainIcon = TD_ERROR_ICON; }

			_TaskDialogIndirect (&tdc, &result, nullptr, nullptr);
		}
	}

	if (!result)
	{
		MSGBOXPARAMS mbp = {0};

		mbp.cbSize = sizeof (MSGBOXPARAMS);
		mbp.hwndOwner = hwnd;
		mbp.dwStyle = flags | MB_TOPMOST;
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
			_r_wnd_top (hwnd, TRUE);

			return TRUE;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, 0);
			SendMessage (hwnd, WM_SETICON, ICON_BIG, 0);

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

rstring _r_clipboard_get (HWND hwnd)
{
	rstring result;

	if (OpenClipboard (hwnd))
	{
		HGLOBAL h = GetClipboardData (CF_UNICODETEXT);

		if (h)
		{
			result = LPCWSTR (GlobalLock (h));

			GlobalUnlock (h);
		}
	}

	CloseClipboard ();

	return result;
}

VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length)
{
	if (OpenClipboard (hwnd))
	{
		if (EmptyClipboard ())
		{
			HGLOBAL h = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, (length + 1) * sizeof (WCHAR));

			if (h)
			{
				memcpy (GlobalLock (h), text, (length + 1) * sizeof (WCHAR));
				SetClipboardData (CF_UNICODETEXT, h);

				GlobalUnlock (h);
			}
		}

		CloseClipboard ();
	}
}

/*
	Filesystem
*/

BOOL _r_fs_exists (LPCWSTR path)
{
	return (GetFileAttributes (path) != INVALID_FILE_ATTRIBUTES);
}

DWORD64 _r_fs_size (HANDLE h)
{
	LARGE_INTEGER size = {0};

	GetFileSizeEx (h, &size);

	return size.QuadPart;
}

BOOL _r_fs_mkdir (LPCWSTR path)
{
	BOOL result = FALSE;
	SHCDEX _SHCreateDirectoryEx = (SHCDEX)GetProcAddress (GetModuleHandle (L"shell32.dll"), "SHCreateDirectoryExW");

	if (_SHCreateDirectoryEx)
	{
		result = _SHCreateDirectoryEx (nullptr, path, nullptr) == ERROR_SUCCESS;
	}

	if (!result)
	{
		result = CreateDirectory (path, nullptr);
	}

	return result;
}

/*
	Processes
*/

BOOL _r_process_is_exists (LPCWSTR path, const size_t len)
{
	BOOL result = FALSE;
	DWORD pid[1024] = {0}, cb = 0;

	WCHAR buff[MAX_PATH] = {0};

	_r_sys_setprivilege (SE_DEBUG_NAME, TRUE);

	if (EnumProcesses (pid, sizeof (pid), &cb))
	{
		for (DWORD i = 0; i < (cb / sizeof (DWORD)); i++)
		{
			if (pid[i])
			{
				HANDLE h = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid[i]);

				if (h)
				{
					GetModuleFileNameEx (h, nullptr, buff, _countof (buff));
					CloseHandle (h);

					if (_wcsnicmp (path, buff, len) == 0)
					{
						result = TRUE;
						break;
					}
				}
			}
		}
	}

	return result;
}

/*
	Strings
*/

/*
	return 1 if v1 > v2
	return 0 if v1 = v2
	return -1 if v1 < v2
*/

INT _r_str_versioncompare (LPCWSTR v1, LPCWSTR v2)
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

/*
	System information
*/

BOOL _r_sys_adminstate ()
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

#ifndef _WIN64
BOOL _r_sys_iswow64 ()
{
	BOOL result = FALSE;

	// IsWow64Process is not available on all supported versions of Windows.
	// Use GetModuleHandle to get a handle to the DLL that contains the function
	// and GetProcAddress to get a pointer to the function if available.

	IW64P _IsWow64Process = (IW64P)GetProcAddress (GetModuleHandle (L"kernel32.dll"), "IsWow64Process");

	if (_IsWow64Process)
	{
		_IsWow64Process (GetCurrentProcess (), &result);
	}

	return result;
}
#endif // _WIN64

BOOL _r_sys_setprivilege (LPCWSTR privilege, BOOL is_enable)
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
			tp.Privileges[0].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;

			if (AdjustTokenPrivileges (token, FALSE, &tp, sizeof (TOKEN_PRIVILEGES), nullptr, nullptr) && GetLastError () == ERROR_SUCCESS)
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

BOOL _r_sys_uacstate ()
{
	HANDLE token = nullptr;
	DWORD out_length = 0;
	TOKEN_ELEVATION_TYPE tet;
	BOOL result = FALSE;

	if (_r_sys_validversion (6, 0))
	{
		if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token) && GetTokenInformation (token, TokenElevationType, &tet, sizeof (TOKEN_ELEVATION_TYPE), &out_length) && tet == TokenElevationTypeLimited)
		{
			result = TRUE;
		}

		if (token)
		{
			CloseHandle (token);
		}
	}

	return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ms725494(v=vs.85).aspx
BOOL _r_sys_validversion (DWORD major, DWORD minor, BYTE condition)
{
	OSVERSIONINFOEX osvi = {0};
	DWORDLONG mask = 0;

	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
	osvi.dwMajorVersion = major;
	osvi.dwMinorVersion = minor;

	VER_SET_CONDITION (mask, VER_MAJORVERSION, condition);
	VER_SET_CONDITION (mask, VER_MINORVERSION, condition);

	return VerifyVersionInfo (&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask);
}

/*
	Unixtime
*/

__time64_t _r_unixtime_now ()
{
	__time64_t t = 0;
	_time64 (&t);

	return t;
}

VOID _r_unixtime_to_filetime (__time64_t ut, LPFILETIME pft)
{
	if (ut && pft)
	{
		LONGLONG ll = Int32x32To64 (ut, 10000000) + 116444736000000000ui64; // 64 bit value

		pft->dwLowDateTime = (DWORD)ll;
		pft->dwHighDateTime = (DWORD)(ll >> 32);
	}
}

VOID _r_unixtime_to_systemtime (__time64_t ut, LPSYSTEMTIME pst)
{
	FILETIME ft = {0};

	_r_unixtime_to_filetime (ut, &ft);

	FileTimeToSystemTime (&ft, pst);
}

/*
	Window management
*/

VOID _r_wnd_center (HWND hwnd)
{
	HWND parent = GetWindow (hwnd, GW_OWNER);
	RECT rc_child = {0}, rc_parent = {0};

	if (!IsWindowVisible (parent))
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

BOOL _r_wnd_changemessagefilter (HWND hwnd, UINT msg, DWORD action)
{
	BOOL result = FALSE;

	if (_r_sys_validversion (6, 0))
	{
		CWMFEX _cwmfex = (CWMFEX)GetProcAddress (GetModuleHandle (L"user32.dll"), "ChangeWindowMessageFilterEx"); // win 7

		if (_cwmfex)
		{
			result = _cwmfex (hwnd, msg, action, nullptr);
		}
		else
		{
			CWMF _cwmf = (CWMF)GetProcAddress (GetModuleHandle (L"user32.dll"), "ChangeWindowMessageFilter"); // vista

			if (_cwmf)
			{
				result = _cwmf (msg, action);
			}
		}
	}

	return result;
}

VOID _r_wnd_toggle (HWND hwnd, BOOL show)
{
	if (show || !IsWindowVisible (hwnd))
	{
		ShowWindow (hwnd, SW_SHOW);

		if (GetLastError () == ERROR_ACCESS_DENIED)
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_RESTORE, 0); // uipi fix
		}

		SetForegroundWindow (hwnd);
		SwitchToThisWindow (hwnd, TRUE);
	}
	else
	{
		ShowWindow (hwnd, SW_HIDE);
	}
}

VOID _r_wnd_top (HWND hwnd, BOOL is_enable)
{
	SetWindowPos (hwnd, (is_enable ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE);
}

/*
	Other
*/

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT d)
{
	HICON result = nullptr;

	LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (GetModuleHandle (L"comctl32.dll"), "LoadIconWithScaleDown");

	if (_LoadIconWithScaleDown)
	{
		_LoadIconWithScaleDown (h, name, d, d, &result);
	}

	if (!result)
	{
		result = (HICON)LoadImage (h, name, IMAGE_ICON, d, d, 0);
	}

	return result;
}

BOOL _r_run (LPCWSTR cmdline, LPCWSTR cd, BOOL is_wait)
{
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	HANDLE job = nullptr;

	si.cb = sizeof (si);

	if (is_wait)
	{
		job = CreateJobObject (nullptr, nullptr);
	}

	rstring _intptr = cmdline;

	BOOL result = CreateProcess (nullptr, _intptr.GetBuffer (), nullptr, nullptr, FALSE, is_wait ? (CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED) : 0, nullptr, cd, &si, &pi);
	_intptr.ReleaseBuffer ();

	if (is_wait)
	{
		AssignProcessToJobObject (job, pi.hProcess);
		ResumeThread (pi.hThread);
		CloseHandle (pi.hThread);

		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION jbai = {0};
		DWORD out_length = 0;

		while (QueryInformationJobObject (job, JobObjectBasicAccountingInformation, &jbai, sizeof (jbai), &out_length) && jbai.ActiveProcesses);

		CloseHandle (job);
		CloseHandle (pi.hProcess);
	}

	return result;
}

rstring _r_normalize_path (LPCWSTR path)
{
	rstring result;

	PathSearchAndQualify (path, result.GetBuffer (MAX_PATH), MAX_PATH);
	result.ReleaseBuffer ();

	return result;
}

/*
	Control: common
*/

VOID _r_ctrl_enable (HWND hwnd, INT ctrl, BOOL is_enable)
{
	EnableWindow (GetDlgItem (hwnd, ctrl), is_enable);
}

rstring _r_ctrl_gettext (HWND hwnd, INT ctrl)
{
	rstring result;

	INT length = (INT)SendDlgItemMessage (hwnd, ctrl, WM_GETTEXTLENGTH, 0, 0) + 1;

	GetDlgItemText (hwnd, ctrl, result.GetBuffer (length), length);
	result.ReleaseBuffer ();

	return result;
}

VOID _r_ctrl_settext (HWND hwnd, INT ctrl, LPCWSTR str, ...)
{
	rstring buffer;

	va_list args = nullptr;
	va_start (args, str);

	buffer.FormatV (str, args);
	SetDlgItemText (hwnd, ctrl, buffer);

	va_end (args);
}

HWND _r_ctrl_settip (HWND hwnd, INT ctrl, LPCWSTR text)
{
	HWND tip = CreateWindowEx (0, TOOLTIPS_CLASS, nullptr, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, GetModuleHandle (nullptr), nullptr);

	TOOLINFO ti = {0};

	ti.cbSize = sizeof (TOOLINFO);
	ti.hwnd = hwnd;
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.lpszText = (LPWSTR)text;
	ti.uId = (UINT_PTR)GetDlgItem (hwnd, ctrl);

	SendMessage (tip, TTM_ADDTOOL, 0, (LPARAM)&ti);

	return tip;
}

BOOL _r_ctrl_showtip (HWND hwnd, INT ctrl, LPCWSTR title, LPCWSTR text, INT icon)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (EDITBALLOONTIP);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon;

	return (BOOL)SendDlgItemMessage (hwnd, ctrl, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

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
		width = _R_PERCENT_OF (width, rc.right);
	}

	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
	lvc.pszText = (LPWSTR)text;
	lvc.fmt = fmt;
	lvc.cx = _R_PERCENT_VAL (width, rc.right);
	lvc.iSubItem = subitem;

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_INSERTCOLUMN, (WPARAM)subitem, (LPARAM)&lvc);
}

INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl, INT column)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl), &rc);

	return _R_PERCENT_OF (SendDlgItemMessage (hwnd, ctrl, LVM_GETCOLUMNWIDTH, column, NULL), rc.right);
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
		lvi.iItem = _r_listview_getitemcount (hwnd, ctrl);

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

INT _r_listview_getcolumncount (HWND hwnd, INT ctrl)
{
	HWND hdr = (HWND)SendDlgItemMessage (hwnd, ctrl, LVM_GETHEADER, 0, 0);

	return (INT)SendMessage (hdr, HDM_GETITEMCOUNT, 0, 0);
}

INT _r_listview_getitemcount (HWND hwnd, INT ctrl)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMCOUNT, 0, NULL);
}

LPARAM _r_listview_getlparam (HWND hwnd, INT ctrl, INT item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;

	SendDlgItemMessage (hwnd, ctrl, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

rstring _r_listview_gettext (HWND hwnd, INT ctrl, INT item, INT subitem)
{
	rstring result;

	size_t length = 0;
	size_t out_length = 0;

	LVITEM lvi = {0};

	lvi.iSubItem = subitem;

	do
	{
		length += _R_BUFFER_LENGTH;

		lvi.pszText = result.GetBuffer (length);
		lvi.cchTextMax = static_cast<int>(length);

		out_length = (size_t)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMTEXT, item, (LPARAM)&lvi);
		result.ReleaseBuffer ();
	}
	while (out_length == (length - 1));

	return result;
}

DWORD _r_listview_setstyle (HWND hwnd, INT ctrl, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl), L"Explorer", nullptr);

	return (DWORD)SendDlgItemMessage (hwnd, ctrl, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl, LPCWSTR text, HTREEITEM parent, INT image, LPARAM lparam)
{
	TVINSERTSTRUCT tvi = {0};

	tvi.itemex.mask = TVIF_TEXT;
	tvi.itemex.pszText = (LPWSTR)text;

	if (parent)
	{
		tvi.hParent = parent;
	}

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

/*
	Control: trayicon
*/

VOID _r_tray_showtip (PNOTIFYICONDATA pnid, DWORD icon, LPCWSTR title, LPCWSTR text)
{
	pnid->uFlags = NIF_INFO;
	pnid->dwInfoFlags = NIIF_RESPECT_QUIET_TIME | NIIF_LARGE_ICON | icon;

	StringCchCopy (pnid->szInfoTitle, _countof (pnid->szInfoTitle), title);
	StringCchCopy (pnid->szInfo, _countof (pnid->szInfo), text);

	Shell_NotifyIcon (NIM_MODIFY, pnid);

	pnid->szInfo[0] = pnid->szInfoTitle[0] = 0; // clear
}
