// routine++
// Copyright (c) 2012-2017 Henry++

#include "routine.h"

/*
	Write debug log to console
*/

VOID _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...)
{
	DWORD dwLE = GetLastError ();
	DWORD dwTC = GetTickCount ();
	DWORD dwPID = GetCurrentProcessId ();
	DWORD dwTID = GetCurrentThreadId ();

	if (!format)
	{
		OutputDebugString (L"\r\n");
	}
	else
	{
		va_list args;
		va_start (args, format);

		rstring buffer;
		buffer.FormatV (format, args);

		va_end (args);

		if (function)
		{
			OutputDebugString (_r_fmt (L"TC=%08d, PID=%04d, TID=%04d, LE=0x%.8lx, FN=%s, FL=%s:%d, T=%s\r\n", dwTC, dwPID, dwTID, dwLE, function, file, line, buffer.IsEmpty () ? L"<none>" : buffer));
		}
		else
		{
			OutputDebugString (buffer);
		}
	}
}

rstring _r_dbg_error (DWORD errcode)
{
	HLOCAL buffer = nullptr;
	rstring result;

	if (FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errcode, MAKELANGID (LANG_ENGLISH, SUBLANG_DEFAULT), (LPWSTR)&buffer, 0, nullptr))
	{
		result = (LPCWSTR)buffer;
		result.Trim (L"\r\n");
	}


	LocalFree (buffer);

	return result;
}

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR format, ...)
{
	rstring result;

	va_list args;
	va_start (args, format);

	result.FormatV (format, args);

	va_end (args);

	return result;
}

rstring _r_fmt_date (const LPFILETIME ft, const DWORD flags)
{
	DWORD pflags = flags;

	rstring result;

	SHFormatDateTime (ft, &pflags, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_fmt_date (const __time64_t ut, const DWORD flags)
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
	Spinlocks
*/

VOID _r_spinlock (volatile LONG* m_ref)
{
	if (!m_ref)
		return;

	LONG ref = 0, newRef = 0, icxref = 0;

	while (newRef == 1 && ref != 0xFFFF)
	{
		do
		{
			ref = *m_ref;

			newRef = (0xFFFF == ref) ? 1 : ++ref;
			icxref = InterlockedCompareExchange (m_ref, newRef, ref);
		}
		while (icxref != ref);
	}
}

VOID _r_spinunlock (volatile LONG* m_ref)
{
	if (!m_ref)
		return;

	InterlockedExchange (m_ref, 0);
}

/*
	System messages
*/

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR format, ...)
{
	rstring buffer;

	INT result = 0;

	if (format)
	{
		va_list args;
		va_start (args, format);

		buffer.FormatV (format, args);

		va_end (args);
	}

	if (_r_sys_validversion (6, 0))
	{
		TDI _TaskDialogIndirect = (TDI)GetProcAddress (GetModuleHandle (L"comctl32.dll"), "TaskDialogIndirect");

		if (_TaskDialogIndirect)
		{
			TASKDIALOGCONFIG tdc = {0};

			tdc.cbSize = sizeof (tdc);
			tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
			tdc.hwndParent = hwnd;
			tdc.pfCallback = &_r_msg_callback;
			tdc.pszWindowTitle = title;
			tdc.pszMainInstruction = main;

			if (!buffer.IsEmpty ())
				tdc.pszContent = buffer;

			if ((flags & MB_ICONMASK) == MB_USERICON)
			{
				tdc.hInstance = GetModuleHandle (nullptr);
				tdc.pszMainIcon = MAKEINTRESOURCE (100);
			}

			tdc.pfCallback = &_r_msg_callback;

			// default buttons
			if ((flags & MB_DEFMASK) == MB_DEFBUTTON2)
			{
				tdc.nDefaultButton = IDNO;
			}

			// buttons
			if ((flags & MB_TYPEMASK) == MB_YESNO)
			{
				tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
			}
			else if ((flags & MB_TYPEMASK) == MB_YESNOCANCEL)
			{
				tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
			}
			else if ((flags & MB_TYPEMASK) == MB_OKCANCEL)
			{
				tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
			}
			else if ((flags & MB_TYPEMASK) == MB_RETRYCANCEL)
			{
				tdc.dwCommonButtons = TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;
			}
			else
			{
				tdc.dwCommonButtons = TDCBF_OK_BUTTON;
			}

			// icons
			if ((flags & MB_ICONMASK) == MB_ICONASTERISK)
			{
				tdc.pszMainIcon = TD_INFORMATION_ICON;
			}
			else if ((flags & MB_ICONMASK) == MB_ICONEXCLAMATION)
			{
				tdc.pszMainIcon = TD_WARNING_ICON;
			}
			else if ((flags & MB_ICONMASK) == MB_ICONQUESTION)
			{
				tdc.pszMainIcon = TD_INFORMATION_ICON;
			}
			else if ((flags & MB_ICONMASK) == MB_ICONHAND)
			{
				tdc.pszMainIcon = TD_ERROR_ICON;
			}

			_TaskDialogIndirect (&tdc, &result, nullptr, nullptr);
		}
	}

	if (!result)
	{
		MSGBOXPARAMS mbp = {0};

		if (main)
		{
			if (buffer.IsEmpty ())
			{
				buffer.Append (main);
			}
			else
			{
				buffer.Insert (L"\r\n\r\n", 0);
				buffer.Insert (main, 0);
			}
		}

		mbp.cbSize = sizeof (mbp);
		mbp.hwndOwner = hwnd;
		mbp.dwStyle = flags | MB_TOPMOST;
		mbp.lpszCaption = title;
		mbp.lpszText = buffer;

		if ((flags & MB_ICONMASK) == MB_USERICON)
		{
			mbp.hInstance = GetModuleHandle (nullptr);
			mbp.lpszIcon = MAKEINTRESOURCE (100);
		}

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
			_r_wnd_center (hwnd);

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

INT _r_fs_delete (LPCWSTR path, BOOL allowundo)
{
	SHFILEOPSTRUCT op = {0};

	op.wFunc = FO_DELETE;
	op.pFrom = path;
	op.fFlags = FOF_NOERRORUI | FOF_NO_UI | FOF_SILENT;

	if (allowundo)
		op.fFlags |= FOF_ALLOWUNDO;

	return SHFileOperation (&op);
}

BOOL _r_fs_exists (LPCWSTR path)
{
	return (GetFileAttributes (path) != INVALID_FILE_ATTRIBUTES);
}

BOOL _r_fs_readfile (HANDLE h, LPVOID result, DWORD64 size)
{
	if (h != INVALID_HANDLE_VALUE)
	{
		HANDLE fm = CreateFileMapping (h, nullptr, PAGE_READONLY, 0, 0, nullptr);

		if (fm)
		{
			LPVOID buffer = MapViewOfFile (fm, FILE_MAP_READ, 0, 0, 0);

			if (buffer)
			{
				CopyMemory (result, buffer, (SIZE_T)size);
				UnmapViewOfFile (buffer);
			}

			CloseHandle (fm);

			return TRUE;
		}
	}

	return FALSE;
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

BOOL _r_fs_rmdir (LPCWSTR path)
{
	WIN32_FIND_DATA wfd = {0};

	HANDLE h = FindFirstFile (_r_fmt (L"%s\\*.*", path), &wfd);

	if (h != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((wfd.cFileName[0] == '.' && !wfd.cFileName[1]) || (wfd.cFileName[0] == '.' && wfd.cFileName[1] == '.' && !wfd.cFileName[2]))
				continue;

			rstring r;
			r.Format (L"%s\\%s", path, wfd.cFileName);

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				_r_fs_rmdir (r);
			}
			else
			{
				SetFileAttributes (r, FILE_ATTRIBUTE_NORMAL);
				DeleteFile (r);
			}
		}
		while (FindNextFile (h, &wfd) == TRUE);

		FindClose (h);
	}

	RemoveDirectory (path);

	return 0;
}

BOOL _r_fs_ren (LPCWSTR path_from, LPCWSTR path_to)
{
	return MoveFileEx (path_from, path_to, 0);
}

/*
	Paths
*/

rstring _r_path_expand (rstring path)
{
	if (path.Find (L'\\') == rstring::npos)
		return path;

	rstring result;

	if (path.Find (L'%') != rstring::npos)
	{
		if (!ExpandEnvironmentStrings (path, result.GetBuffer (4096), 4096))
			result = path;
	}
	else
	{
		if (!PathSearchAndQualify (path, result.GetBuffer (4096), 4096))
			result = path;
	}

	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_unexpand (rstring path)
{
	if (path.Find (L'\\') == rstring::npos)
		return path;

	rstring result;

	if (!PathUnExpandEnvStrings (path, result.GetBuffer (4096), 4096))
		result = path;

	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_compact (rstring path, UINT length)
{
	rstring result;

	PathCompactPathEx (result.GetBuffer (length), path, length, 0);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_extractdir (rstring path)
{
	rstring buffer = path;
	const size_t pos = buffer.ReverseFind (L"\\/");

	buffer.Mid (0, pos);
	buffer.Trim (L"\\/");

	return buffer;
}

rstring _r_path_extractfile (rstring path)
{
	return PathFindFileName (path);
}

/*
	Processes
*/

BOOL _r_process_is_exists (LPCWSTR path, const size_t len)
{
	BOOL result = FALSE;
	DWORD pid[1024] = {0}, cb = 0;

	if (EnumProcesses (pid, sizeof (pid), &cb))
	{
		for (DWORD i = 0; i < (cb / sizeof (DWORD)); i++)
		{
			if (pid[i])
			{
				HANDLE h = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid[i]);

				if (h)
				{
					WCHAR buffer[512] = {0};
					DWORD size = _countof (buffer);

					if (QueryFullProcessImageName (h, 0, buffer, &size))
					{
						if (_wcsnicmp (path, buffer, len) == 0)
							result = TRUE;
					}

					CloseHandle (h);

					if (result)
						break;
				}
			}
		}
	}

	return result;
}

/*
	Strings
*/

size_t _r_str_hash (LPCWSTR text)
{
	static const size_t InitialFNV = 2166136261U;
	static const size_t FNVMultiple = 16777619;

	size_t hash = InitialFNV;
	const size_t length = wcslen (text);

	if (!length)
		return 0;

	for (size_t i = 0; i < length; i++)
	{
		hash = hash ^ (towupper (text[i])); /* xor the low 8 bits */
		hash = hash * FNVMultiple; /* multiply by the magic number */
	}

	return hash;
}

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
	DWORD status = 0, ps_size = sizeof (PRIVILEGE_SET);

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
				__leave;
		}

		if (!DuplicateToken (token, SecurityImpersonation, &impersonation_token))
			__leave;

		if (!AllocateAndInitializeSid (&sia, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
			__leave;

		sd = LocalAlloc (LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

		if (!sd || !InitializeSecurityDescriptor (sd, SECURITY_DESCRIPTOR_REVISION))
			__leave;

		DWORD acl_size = sizeof (ACL) + sizeof (ACCESS_ALLOWED_ACE) + GetLengthSid (sid) - sizeof (DWORD);
		acl = (PACL)LocalAlloc (LPTR, acl_size);

		if (!acl || !InitializeAcl (acl, acl_size, ACL_REVISION2) || !AddAccessAllowedAce (acl, ACL_REVISION2, ACCESS_READ | ACCESS_WRITE, sid) || !SetSecurityDescriptorDacl (sd, TRUE, acl, FALSE))
			__leave;

		SetSecurityDescriptorGroup (sd, sid, FALSE);
		SetSecurityDescriptorOwner (sd, sid, FALSE);

		if (!IsValidSecurityDescriptor (sd))
			__leave;

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
			LocalFree (acl);

		if (sd)
			LocalFree (sd);

		if (sid)
			FreeSid (sid);

		if (impersonation_token)
			CloseHandle (impersonation_token);

		if (token)
			CloseHandle (token);
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

BOOL _r_sys_securitydescriptor (LPSECURITY_ATTRIBUTES sa, DWORD length, PSECURITY_DESCRIPTOR sd)
{
	if (!sa || !InitializeSecurityDescriptor (sd, SECURITY_DESCRIPTOR_REVISION) || !SetSecurityDescriptorDacl (sd, TRUE, nullptr, FALSE))
	{
		return FALSE;
	}

	sa->nLength = length;
	sa->lpSecurityDescriptor = sd;
	sa->bInheritHandle = TRUE;

	return TRUE;
}

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
		if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token) && GetTokenInformation (token, TokenElevationType, &tet, sizeof (tet), &out_length) && tet == TokenElevationTypeLimited)
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
BOOL _r_sys_validversion (DWORD major, DWORD minor, DWORD build, BYTE condition)
{
	OSVERSIONINFOEX osvi = {0};
	DWORDLONG mask = 0;
	DWORD type_mask = VER_MAJORVERSION | VER_MINORVERSION;

	osvi.dwOSVersionInfoSize = sizeof (osvi);
	osvi.dwMajorVersion = major;
	osvi.dwMinorVersion = minor;
	osvi.dwBuildNumber = build;

	VER_SET_CONDITION (mask, VER_MAJORVERSION, condition);
	VER_SET_CONDITION (mask, VER_MINORVERSION, condition);

	if (build)
	{
		VER_SET_CONDITION (mask, VER_BUILDNUMBER, condition);
		type_mask |= VER_BUILDNUMBER;
	}

	return VerifyVersionInfo (&osvi, type_mask, mask);
}

VOID _r_sleep (DWORD milliseconds)
{
	if (!milliseconds || milliseconds == INFINITE)
		return;

	static HANDLE evt = CreateEvent (nullptr, FALSE, FALSE, nullptr);

	WaitForSingleObjectEx (evt, milliseconds, FALSE);
}

/*
	Unixtime
*/

__time64_t _r_unixtime_now ()
{
	SYSTEMTIME st = {0};

	GetSystemTime (&st);

	return _r_unixtime_from_systemtime (&st);
}

VOID _r_unixtime_to_filetime (__time64_t ut, const LPFILETIME pft)
{
	if (ut && pft)
	{
		__time64_t ll = ut * 10000000ULL + 116444736000000000; // 64 bit value

		pft->dwLowDateTime = (DWORD)ll;
		pft->dwHighDateTime = ll >> 32;
	}
}

VOID _r_unixtime_to_systemtime (__time64_t ut, const LPSYSTEMTIME pst)
{
	FILETIME ft = {0};

	_r_unixtime_to_filetime (ut, &ft);

	FileTimeToSystemTime (&ft, pst);
}

__time64_t _r_unixtime_from_filetime (const FILETIME* pft)
{
	ULARGE_INTEGER ull = {0};

	if (pft)
	{
		ull.LowPart = pft->dwLowDateTime;
		ull.HighPart = pft->dwHighDateTime;
	}

	return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

__time64_t _r_unixtime_from_systemtime (const LPSYSTEMTIME pst)
{
	FILETIME ft = {0};

	SystemTimeToFileTime (pst, &ft);

	return _r_unixtime_from_filetime (&ft);
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

/*
	Optimized version of WinAPI function "FillRect"
*/

VOID _r_wnd_fillrect (HDC dc, LPRECT rc, COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (dc, clr);
	ExtTextOut (dc, 0, 0, ETO_OPAQUE, rc, nullptr, 0, nullptr);
	SetBkColor (dc, clr_prev);
}

VOID _r_wnd_toggle (HWND hwnd, BOOL show)
{
	if (show || !IsWindowVisible (hwnd))
	{
		ShowWindow (hwnd, SW_SHOW);

		if (GetLastError () == ERROR_ACCESS_DENIED)
			SendMessage (hwnd, WM_SYSCOMMAND, SC_RESTORE, 0); // uipi fix

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

VOID _r_wnd_addstyle (HWND hwnd, UINT ctrl_id, LONG mask, LONG stateMask, INT index)
{
	if (ctrl_id)
		hwnd = GetDlgItem (hwnd, ctrl_id);

	LONG_PTR style = (GetWindowLongPtr (hwnd, index) & ~stateMask) | mask;

	SetWindowLongPtr (hwnd, index, style);

	SetWindowPos (hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
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

BOOL _r_run (LPCWSTR filename, LPCWSTR cmdline, LPCWSTR cd, BOOL is_wait)
{
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	HANDLE job = nullptr;

	si.cb = sizeof (si);

	if (is_wait)
		job = CreateJobObject (nullptr, nullptr);

	rstring _intptr = cmdline ? cmdline : filename;
	BOOL result = CreateProcess (filename, _intptr.GetBuffer (), nullptr, nullptr, FALSE, is_wait ? (CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED) : 0, nullptr, cd, &si, &pi);
	_intptr.Clear ();

	if (is_wait)
	{
		AssignProcessToJobObject (job, pi.hProcess);
		ResumeThread (pi.hThread);

		CloseHandle (pi.hThread);
		pi.hThread = nullptr;

		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION jbai = {0};
		DWORD out_length = 0;

		while (QueryInformationJobObject (job, JobObjectBasicAccountingInformation, &jbai, sizeof (jbai), &out_length) && jbai.ActiveProcesses);
	}

	if (job)
		CloseHandle (job);

	if (pi.hThread)
		CloseHandle (pi.hThread);

	if (pi.hProcess)
		CloseHandle (pi.hProcess);

	return result;
}

size_t _r_rnd (size_t start, size_t end)
{
	srand (GetTickCount ());

	return rand () % end + start;
}

/*
	Control: common
*/

VOID _r_ctrl_enable (HWND hwnd, UINT ctrl, BOOL is_enable)
{
	EnableWindow (GetDlgItem (hwnd, ctrl), is_enable);
}

rstring _r_ctrl_gettext (HWND hwnd, UINT ctrl)
{
	rstring result;

	INT length = (INT)SendDlgItemMessage (hwnd, ctrl, WM_GETTEXTLENGTH, 0, 0);

	if (!length)
		return L"";

	length += 1;

	GetDlgItemText (hwnd, ctrl, result.GetBuffer (length), length);
	result.ReleaseBuffer ();

	return result;
}

VOID _r_ctrl_settext (HWND hwnd, UINT ctrl, LPCWSTR str, ...)
{
	rstring buffer;

	va_list args;
	va_start (args, str);

	buffer.FormatV (str, args);
	SetDlgItemText (hwnd, ctrl, buffer);

	va_end (args);
}

HWND _r_ctrl_settip (HWND hwnd, UINT ctrl, LPCWSTR text)
{
	HINSTANCE hinst = GetModuleHandle (nullptr);
	HWND h = CreateWindowEx (0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, hinst, nullptr);

	TOOLINFO ti = {0};

	ti.cbSize = sizeof (ti);
	ti.hwnd = hwnd;
	ti.hinst = hinst;
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.lpszText = (LPWSTR)text;
	ti.uId = (UINT_PTR)GetDlgItem (hwnd, ctrl);

	SendMessage (h, TTM_ACTIVATE, TRUE, 0);
	SendMessage (h, TTM_ADDTOOL, 0, (LPARAM)&ti);

	return h;
}

BOOL _r_ctrl_showtip (HWND hwnd, UINT ctrl, LPCWSTR title, LPCWSTR text, INT icon)
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

INT _r_listview_addcolumn (HWND hwnd, UINT ctrl, LPCWSTR text, UINT width, size_t subitem, INT fmt)
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
	lvc.iSubItem = static_cast<INT>(subitem);

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_INSERTCOLUMN, (WPARAM)subitem, (LPARAM)&lvc);
}

INT _r_listview_getcolumnwidth (HWND hwnd, UINT ctrl, INT column)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl), &rc);

	return _R_PERCENT_OF (SendDlgItemMessage (hwnd, ctrl, LVM_GETCOLUMNWIDTH, column, NULL), rc.right);
}

INT _r_listview_addgroup (HWND hwnd, UINT ctrl, size_t group_id, LPCWSTR text, UINT align, UINT state)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (LVGROUP);
	lvg.mask = LVGF_HEADER | LVGF_GROUPID;
	lvg.pszHeader = (LPWSTR)text;
	lvg.iGroupId = static_cast<INT>(group_id);

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

INT _r_listview_additem (HWND hwnd, UINT ctrl, LPCWSTR text, size_t item, size_t subitem, size_t image, size_t group_id, LPARAM lparam)
{
	if (item == LAST_VALUE)
	{
		item = static_cast<INT>(_r_listview_getitemcount (hwnd, ctrl));

		if (subitem)
			item -= 1;
	}

	if (subitem)
	{
		if (lparam)
			_r_listview_setitem (hwnd, ctrl, nullptr, item, 0, LAST_VALUE, LAST_VALUE, lparam);

		return _r_listview_setitem (hwnd, ctrl, text, item, subitem);
	}

	WCHAR buffer[MAX_PATH] = {0};

	LVITEM lvi = {0};

	lvi.iItem = static_cast<INT>(item);
	lvi.iSubItem = static_cast<INT>(subitem);

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = buffer;

		StringCchCopy (buffer, _countof (buffer), text);
	}

	if (image != LAST_VALUE)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = static_cast<INT>(image);
	}

	if (group_id != LAST_VALUE)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = static_cast<INT>(group_id);
	}

	if (lparam && !subitem)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

BOOL _r_listview_getcheckstate (HWND hwnd, UINT ctrl, size_t item)
{
	return (((UINT)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMSTATE, item, LVIS_STATEIMAGEMASK)) >> 12) - 1;
}

VOID _r_listview_deleteallcolumns (HWND hwnd, UINT ctrl)
{
	const INT column_count = _r_listview_getcolumncount (hwnd, ctrl);

	for (INT i = column_count; i >= 0; i--)
	{
		SendDlgItemMessage (hwnd, ctrl, LVM_DELETECOLUMN, i, 0);
	}
}

VOID _r_listview_deleteallgroups (HWND hwnd, UINT ctrl)
{
	SendDlgItemMessage (hwnd, ctrl, LVM_REMOVEALLGROUPS, 0, 0);
	SendDlgItemMessage (hwnd, ctrl, LVM_ENABLEGROUPVIEW, FALSE, NULL);
}

VOID _r_listview_deleteallitems (HWND hwnd, UINT ctrl)
{
	SendDlgItemMessage (hwnd, ctrl, LVM_DELETEALLITEMS, 0, 0);
}

INT _r_listview_getcolumncount (HWND hwnd, UINT ctrl)
{
	HWND hdr = (HWND)SendDlgItemMessage (hwnd, ctrl, LVM_GETHEADER, 0, 0);

	return (INT)SendMessage (hdr, HDM_GETITEMCOUNT, 0, 0);
}

size_t _r_listview_getitemcount (HWND hwnd, UINT ctrl, BOOL list_checked)
{
	if (list_checked)
	{
		INT item = -1;
		size_t count = 0;

		while ((item = (INT)SendDlgItemMessage (hwnd, ctrl, LVM_GETNEXTITEM, item, LVNI_ALL)) != -1)
		{
			if (_r_listview_getcheckstate (hwnd, ctrl, item))
				++count;
		}

		return count;
	}

	return (size_t)SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMCOUNT, 0, NULL);
}

LPARAM _r_listview_getlparam (HWND hwnd, UINT ctrl, size_t item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = (INT)item;

	SendDlgItemMessage (hwnd, ctrl, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

rstring _r_listview_gettext (HWND hwnd, UINT ctrl, size_t item, size_t subitem)
{
	rstring result;

	size_t length = 0;
	size_t out_length = 0;

	LVITEM lvi = {0};

	lvi.iSubItem = static_cast<int>(subitem);

	do
	{
		length += _R_BUFFER_LENGTH;

		lvi.pszText = result.GetBuffer (length);
		lvi.cchTextMax = static_cast<int>(length);

		out_length = static_cast<size_t>(SendDlgItemMessage (hwnd, ctrl, LVM_GETITEMTEXT, item, (LPARAM)&lvi));
		result.ReleaseBuffer ();
	}
	while (out_length == (length - 1));

	return result;
}

BOOL _r_listview_setcheckstate (HWND hwnd, UINT ctrl, size_t item, BOOL state)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (state ? 2 : 1);

	return (BOOL)SendDlgItemMessage (hwnd, ctrl, LVM_SETITEMSTATE, (item == LAST_VALUE) ? -1 : item, (LPARAM)&lvi);
}

BOOL _r_listview_setcolumnsortindex (HWND hwnd, UINT ctrl, INT column, INT arrow)
{
	HWND header = (HWND)SendDlgItemMessage (hwnd, ctrl, LVM_GETHEADER, 0, 0);

	if (header)
	{
		HDITEM hitem = {0};

		hitem.mask = HDI_FORMAT;

		if (Header_GetItem (header, column, &hitem))
		{
			if (arrow == 1)
			{
				hitem.fmt = (hitem.fmt & ~HDF_SORTDOWN) | HDF_SORTUP;
			}
			else if (arrow == -1)
			{
				hitem.fmt = (hitem.fmt & ~HDF_SORTUP) | HDF_SORTDOWN;
			}
			else
			{
				hitem.fmt = hitem.fmt & ~(HDF_SORTDOWN | HDF_SORTUP);
			}

			return Header_SetItem (header, column, &hitem);
		}
	}

	return FALSE;
}

INT _r_listview_setitem (HWND hwnd, UINT ctrl, LPCWSTR text, size_t item, size_t subitem, size_t image, size_t group_id, LPARAM lparam)
{
	WCHAR buffer[MAX_PATH] = {0};

	LVITEM lvi = {0};

	lvi.iItem = static_cast<INT>(item);
	lvi.iSubItem = static_cast<INT>(subitem);

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = buffer;

		StringCchCopy (buffer, _countof (buffer), text);
	}

	if (image != LAST_VALUE)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = static_cast<INT>(image);
	}

	if (group_id != LAST_VALUE)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = static_cast<INT>(group_id);
	}

	if (lparam)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl, LVM_SETITEM, 0, (LPARAM)&lvi);
}

BOOL _r_listview_setlparam (HWND hwnd, UINT ctrl, UINT item, LPARAM param)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;
	lvi.lParam = param;

	return static_cast<BOOL>(SendDlgItemMessage (hwnd, ctrl, LVM_SETITEM, 0, (LPARAM)&lvi));
}

BOOL _r_listview_setgroup (HWND hwnd, UINT ctrl, UINT item, size_t group_id)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_GROUPID;
	lvi.iItem = item;
	lvi.iGroupId = (INT)group_id;

	return static_cast<BOOL>(SendDlgItemMessage (hwnd, ctrl, LVM_SETITEM, 0, (LPARAM)&lvi));
}

DWORD _r_listview_setstyle (HWND hwnd, UINT ctrl, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl), L"Explorer", nullptr);

	_r_wnd_top ((HWND)SendDlgItemMessage (hwnd, ctrl, LVM_GETTOOLTIPS, 0, 0), TRUE); // listview-tooltip-HACK!!!

	SendDlgItemMessage (hwnd, ctrl, LVM_SETUNICODEFORMAT, TRUE, 0);

	return (DWORD)SendDlgItemMessage (hwnd, ctrl, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);
}

VOID _r_listview_resizeonecolumn (HWND hwnd, UINT ctrl_id)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMNWIDTH, 0, (rc.right - rc.left));
}

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, UINT ctrl, LPCWSTR text, HTREEITEM parent, size_t image, LPARAM lparam)
{
	TVINSERTSTRUCT tvi = {0};

	tvi.itemex.mask = TVIF_TEXT | TVIF_STATE;
	tvi.itemex.pszText = (LPWSTR)text;
	tvi.itemex.state = TVIS_EXPANDED;
	tvi.itemex.stateMask = TVIS_EXPANDED;

	if (parent)
		tvi.hParent = parent;

	if (image != LAST_VALUE)
	{
		tvi.itemex.mask |= (TVIF_IMAGE | TVIF_SELECTEDIMAGE);
		tvi.itemex.iImage = static_cast<INT>(image);
		tvi.itemex.iSelectedImage = static_cast<INT>(image);
	}

	if (lparam)
	{
		tvi.itemex.mask |= TVIF_PARAM;
		tvi.itemex.lParam = lparam;
	}

	return (HTREEITEM)SendDlgItemMessage (hwnd, ctrl, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getlparam (HWND hwnd, UINT ctrl, HTREEITEM item)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = item;

	SendDlgItemMessage (hwnd, ctrl, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

DWORD _r_treeview_setstyle (HWND hwnd, UINT ctrl, DWORD exstyle, INT height)
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

BOOL _r_status_settext (HWND hwnd, UINT ctrl, INT part, LPCWSTR text)
{
	return (BOOL)SendDlgItemMessage (hwnd, ctrl, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
}

VOID _r_status_setstyle (HWND hwnd, UINT ctrl, INT height)
{
	SendDlgItemMessage (hwnd, ctrl, SB_SETMINHEIGHT, (WPARAM)height, 0);
	SendDlgItemMessage (hwnd, ctrl, WM_SIZE, 0, 0);
}
