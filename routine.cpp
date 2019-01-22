// routine++
// Copyright (c) 2012-2019 Henry++

#include "routine.hpp"
/*
	Write debug log to console
*/

void _r_dbg (LPCWSTR format, ...)
{
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

		OutputDebugString (buffer);
	}
}

void _r_dbg_write (LPCWSTR appname, LPCWSTR appversion, LPCWSTR fn, DWORD result, LPCWSTR desc)
{
	rstring path = _r_dbg_getpath (appname);

	const HANDLE hfile = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			DWORD written = 0;
			static const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (hfile, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask
		}
		else
		{
			_r_fs_setpos (hfile, 0, FILE_END);
		}

		DWORD written = 0;

		rstring buffer;
		rstring write_buffer;

		buffer.Format (_R_DBG_FORMAT, fn, result, desc ? desc : L"<empty>");
		write_buffer.Format (L"%s,%s,%s\r\n", _r_fmt_date (_r_unixtime_now (), FDTF_SHORTDATE | FDTF_LONGTIME).GetString (), buffer.GetString (), appversion);

		WriteFile (hfile, write_buffer.GetString (), DWORD (write_buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);

		CloseHandle (hfile);
	}
}

rstring _r_dbg_getpath (LPCWSTR appname)
{
	WCHAR result[MAX_PATH] = {0};

	if (_r_sys_uacstate ())
	{
		GetTempPath (_countof (result), result);
	}
	else
	{
		GetModuleFileName (GetModuleHandle (nullptr), result, _countof (result));
		PathRemoveFileSpec (result);
	}

	StringCchCat (result, _countof (result), L"\\");
	StringCchCat (result, _countof (result), appname);
	StringCchCat (result, _countof (result), L"_error.log");

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
	WCHAR buffer[128] = {0};

	SHFormatDateTime (ft, &pflags, buffer, _countof (buffer));

	return buffer;
}

rstring _r_fmt_date (const time_t ut, const DWORD flags)
{
	FILETIME ft = {0};
	_r_unixtime_to_filetime (ut, &ft);

	return _r_fmt_date (&ft, flags);
}

rstring _r_fmt_size64 (LONGLONG bytes)
{
	WCHAR buffer[128] = {0};
	bool is_success = false;

	const HMODULE hlib = GetModuleHandle (L"shlwapi.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI *SFBSE) (ULONGLONG, SFBS_FLAGS, PWSTR, UINT); // StrFormatByteSizeEx
		const SFBSE _StrFormatByteSizeEx = (SFBSE)GetProcAddress (hlib, "StrFormatByteSizeEx");

		if (_StrFormatByteSizeEx)
			is_success = (_StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, _countof (buffer)) == S_OK); // vista (sp1)+
	}

	if (!is_success)
		StrFormatByteSize64 (bytes, buffer, _countof (buffer)); // fallback

	return buffer;
}

rstring _r_fmt_interval (time_t seconds, INT digits)
{
	WCHAR buffer[128] = {0};
	StrFromTimeInterval (buffer, _countof (buffer), DWORD (seconds) * 1000, digits);

	return buffer;
}

/*
	FastLock is a port of FastResourceLock from PH 1.x.

	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.

	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.

	https://github.com/processhacker2/processhacker
*/

#ifndef _APP_HAVE_SRWLOCK
static const DWORD _r_fastlock_getspincount ()
{
	SYSTEM_INFO si = {0};
	GetNativeSystemInfo (&si);

	if (si.dwNumberOfProcessors > 1)
		return 4000;
	else
		return 0;
}

ULONG _r_fastlock_islocked (P_FASTLOCK plock)
{
	return _InterlockedCompareExchange (&plock->Value, 0, 0);
}

void _r_fastlock_initialize (P_FASTLOCK plock)
{
	plock->Value = 0;

#ifdef _APP_NO_WINXP
	plock->ExclusiveWakeEvent = CreateSemaphoreEx (nullptr, 0, MAXLONG, nullptr, 0, SEMAPHORE_ALL_ACCESS);
	plock->SharedWakeEvent = CreateSemaphoreEx (nullptr, 0, MAXLONG, nullptr, 0, SEMAPHORE_ALL_ACCESS);
#else
	plock->ExclusiveWakeEvent = CreateSemaphore (nullptr, 0, MAXLONG, nullptr);
	plock->SharedWakeEvent = CreateSemaphore (nullptr, 0, MAXLONG, nullptr);
#endif
}

void _r_fastlock_acquireexclusive (P_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	const DWORD spinCount = _r_fastlock_getspincount ();

	while (true)
	{
		value = plock->Value;

		if (!(value & (_R_FASTLOCK_OWNED | _R_FASTLOCK_EXCLUSIVE_WAKING)))
		{
			if (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED, value) == value)
				break;
		}
		else if (i >= spinCount)
		{
			if (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->ExclusiveWakeEvent, 0, FALSE) != STATUS_WAIT_0)
					break;

				do
				{
					value = plock->Value;
				}
				while (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED - _R_FASTLOCK_EXCLUSIVE_WAKING, value) != value);

				break;
			}
		}

		i++;
		YieldProcessor ();
	}
}

void _r_fastlock_acquireshared (P_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	const DWORD spinCount = _r_fastlock_getspincount ();

	while (true)
	{
		value = plock->Value;

		if (!(value & (
			_R_FASTLOCK_OWNED |
			(_R_FASTLOCK_SHARED_OWNERS_MASK << _R_FASTLOCK_SHARED_OWNERS_SHIFT) |
			_R_FASTLOCK_EXCLUSIVE_MASK
			)))
		{
			if (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if (
			(value & _R_FASTLOCK_OWNED) &&
			((value >> _R_FASTLOCK_SHARED_OWNERS_SHIFT) & _R_FASTLOCK_SHARED_OWNERS_MASK) > 0 &&
			!(value & _R_FASTLOCK_EXCLUSIVE_MASK)
			)
		{
			if (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
			{
				break;
			}
		}
		else if (i >= spinCount)
		{
			if (_InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->SharedWakeEvent, 0, FALSE) != STATUS_WAIT_0)
					break;

				continue;
			}
		}

		i++;
		YieldProcessor ();
	}
}

void _r_fastlock_releaseexclusive (P_FASTLOCK plock)
{
	ULONG value;

	while (true)
	{
		value = plock->Value;

		if ((value >> _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT) & _R_FASTLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (_InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED + _R_FASTLOCK_EXCLUSIVE_WAKING - _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				ReleaseSemaphore (plock->ExclusiveWakeEvent, 1, nullptr);
				break;
			}
		}
		else
		{
			ULONG sharedWaiters = (value >> _R_FASTLOCK_SHARED_WAITERS_SHIFT) & _R_FASTLOCK_SHARED_WAITERS_MASK;

			if (_InterlockedCompareExchange (&plock->Value, value & ~(_R_FASTLOCK_OWNED | (_R_FASTLOCK_SHARED_WAITERS_MASK << _R_FASTLOCK_SHARED_WAITERS_SHIFT)), value) == value)
			{
				if (sharedWaiters)
					ReleaseSemaphore (plock->SharedWakeEvent, sharedWaiters, nullptr);

				break;
			}
		}

		YieldProcessor ();
	}
}

void _r_fastlock_releaseshared (P_FASTLOCK plock)
{
	ULONG value;

	while (true)
	{
		value = plock->Value;

		if (((value >> _R_FASTLOCK_SHARED_OWNERS_SHIFT) & _R_FASTLOCK_SHARED_OWNERS_MASK) > 1)
		{
			if (_InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if ((value >> _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT) & _R_FASTLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (_InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED + _R_FASTLOCK_EXCLUSIVE_WAKING - _R_FASTLOCK_SHARED_OWNERS_INC - _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				ReleaseSemaphore (plock->ExclusiveWakeEvent, 1, nullptr);
				break;
			}
		}
		else
		{
			if (_InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED - _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}

		YieldProcessor ();
	}
}
#endif // _APP_HAVE_SRWLOCK

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
		TASKDIALOGCONFIG tdc = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetModuleHandle (nullptr);
		tdc.pfCallback = &_r_msg_callback;
		tdc.pszWindowTitle = title;
		tdc.pszMainInstruction = main;

		if (!buffer.IsEmpty ())
			tdc.pszContent = buffer;

		// default buttons
		if ((flags & MB_DEFMASK) == MB_DEFBUTTON2)
			tdc.nDefaultButton = IDNO;

		// buttons
		if ((flags & MB_TYPEMASK) == MB_YESNO)
			tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;

		else if ((flags & MB_TYPEMASK) == MB_YESNOCANCEL)
			tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;

		else if ((flags & MB_TYPEMASK) == MB_OKCANCEL)
			tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;

		else if ((flags & MB_TYPEMASK) == MB_RETRYCANCEL)
			tdc.dwCommonButtons = TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;

		else
			tdc.dwCommonButtons = TDCBF_OK_BUTTON;

		// icons
		if ((flags & MB_ICONMASK) == MB_USERICON)
			tdc.pszMainIcon = MAKEINTRESOURCE (100);

		else if ((flags & MB_ICONMASK) == MB_ICONASTERISK)
			tdc.pszMainIcon = TD_INFORMATION_ICON;

		else if ((flags & MB_ICONMASK) == MB_ICONEXCLAMATION)
			tdc.pszMainIcon = TD_WARNING_ICON;

		else if ((flags & MB_ICONMASK) == MB_ICONQUESTION)
			tdc.pszMainIcon = TD_INFORMATION_ICON;

		else if ((flags & MB_ICONMASK) == MB_ICONHAND)
			tdc.pszMainIcon = TD_ERROR_ICON;

		if ((flags & MB_TOPMOST) != 0)
			tdc.lpCallbackData = MAKELONG (0, 1);

		_r_msg_taskdialog (&tdc, &result, nullptr, nullptr);
	}

#ifndef _APP_NO_WINXP
	if (!result)
	{
		MSGBOXPARAMS mbp = {0};

		if (main)
		{
			if (buffer.IsEmpty ())
				buffer.Append (main);

			else
				buffer.InsertFormat (0, L"%s\r\n\r\n", main);
		}

		mbp.cbSize = sizeof (mbp);
		mbp.hwndOwner = hwnd;
		mbp.hInstance = GetModuleHandle (nullptr);
		mbp.dwStyle = flags;
		mbp.lpszCaption = title;
		mbp.lpszText = buffer;

		if ((flags & MB_ICONMASK) == MB_USERICON)
			mbp.lpszIcon = MAKEINTRESOURCE (100);

		result = MessageBoxIndirect (&mbp);
	}
#endif // _APP_NO_WINXP

	return result;
}

bool _r_msg_taskdialog (const TASKDIALOGCONFIG* ptd, INT* pbutton, INT* pradiobutton, BOOL* pcheckbox)
{
#ifndef _APP_NO_WINXP
	const HMODULE hlib = GetModuleHandle (L"comctl32.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI *TDI) (const TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect
		const TDI _TaskDialogIndirect = (TDI)GetProcAddress (hlib, "TaskDialogIndirect");

		if (_TaskDialogIndirect)
			return (_TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox) == S_OK);
	}

	return false;
#else
	return (TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox) == S_OK);
#endif // _APP_NO_WINXP
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR lpdata)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			const bool is_topmost = HIWORD (lpdata);

			if (is_topmost)
				_r_wnd_top (hwnd, true);

			_r_wnd_center (hwnd, GetParent (hwnd));

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			const bool is_donotdrawicon = LOWORD (lpdata);

			if (!is_donotdrawicon)
			{
				SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)SendMessage (GetParent (hwnd), WM_GETICON, ICON_SMALL, 0));
				SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)SendMessage (GetParent (hwnd), WM_GETICON, ICON_BIG, 0));
			}

			break;
		}

		case TDN_HYPERLINK_CLICKED:
		{
			ShellExecute (hwnd, nullptr, (LPCWSTR)lparam, nullptr, nullptr, SW_SHOWDEFAULT);
			break;
		}
	}

	return S_OK;
}

/*
	Clipboard operations
*/

rstring _r_clipboard_get (HWND hwnd)
{
	rstring result;

	if (OpenClipboard (hwnd))
	{
		HGLOBAL hmem = GetClipboardData (CF_UNICODETEXT);

		if (hmem)
		{
			result = LPCWSTR (GlobalLock (hmem));
			GlobalUnlock (hmem);
		}
	}

	CloseClipboard ();

	return result;
}

void _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length)
{
	if (OpenClipboard (hwnd))
	{
		if (EmptyClipboard ())
		{
			HGLOBAL hmem = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, (length + 1) * sizeof (WCHAR));

			if (hmem)
			{
				LPVOID ptr = GlobalLock (hmem);

				if (ptr)
				{
					const size_t size = (length + 1) * sizeof (WCHAR);

					memcpy (ptr, text, size);
					SetClipboardData (CF_UNICODETEXT, hmem);

					GlobalUnlock (hmem);
				}
			}
		}

		CloseClipboard ();
	}
}

/*
	Filesystem
*/

bool _r_fs_delete (LPCWSTR path, bool allowundo)
{
	bool result = false;

	if (allowundo)
	{
		SHFILEOPSTRUCT op = {0};

		op.wFunc = FO_DELETE;
		op.pFrom = path;
		op.fFlags = FOF_NO_UI | FOF_ALLOWUNDO;

		result = (SHFileOperation (&op) == ERROR_SUCCESS);
	}
	else
	{
		if (DeleteFile (path))
			result = true;
	}

	return result;
}

bool _r_fs_exists (LPCWSTR path)
{
	return (GetFileAttributes (path) != INVALID_FILE_ATTRIBUTES);
}

bool _r_fs_readfile (HANDLE hfile, LPVOID result, DWORD64 size)
{
	if (hfile != INVALID_HANDLE_VALUE)
	{
		const HANDLE hmap = CreateFileMapping (hfile, nullptr, PAGE_READONLY, 0, 0, nullptr);

		if (hmap)
		{
			LPVOID buffer = MapViewOfFile (hmap, FILE_MAP_READ, 0, 0, 0);

			if (buffer)
			{
				CopyMemory (result, buffer, (SIZE_T)size);
				UnmapViewOfFile (buffer);
			}

			CloseHandle (hmap);

			return true;
		}
	}

	return false;
}

bool _r_fs_setpos (HANDLE hfile, LONGLONG pos, DWORD method)
{
	LARGE_INTEGER lpos = {0};
	lpos.QuadPart = pos;

	return SetFilePointerEx (hfile, lpos, nullptr, method);
}

DWORD64 _r_fs_size (HANDLE hfile)
{
	LARGE_INTEGER size = {0};

	GetFileSizeEx (hfile, &size);

	return size.QuadPart;
}

DWORD64 _r_fs_size (LPCWSTR path)
{
	const HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		const DWORD64 result = _r_fs_size (hfile);

		CloseHandle (hfile);
		return result;
	}

	return 0;
}

bool _r_fs_mkdir (LPCWSTR path)
{
	bool result = false;

	const HMODULE hlib = GetModuleHandle (L"shell32.dll");

	if (hlib)
	{
		typedef int (WINAPI *SHCDEX) (HWND, LPCTSTR, const SECURITY_ATTRIBUTES*); // SHCreateDirectoryEx
		const SHCDEX _SHCreateDirectoryEx = (SHCDEX)GetProcAddress (hlib, "SHCreateDirectoryExW");

		if (_SHCreateDirectoryEx)
			result = _SHCreateDirectoryEx (nullptr, path, nullptr) == ERROR_SUCCESS;
	}

	if (!result)
	{
		if (CreateDirectory (path, nullptr))
			result = true;
	}

	return result;
}

void _r_fs_rmdir (LPCWSTR path)
{
	WIN32_FIND_DATA wfd = {0};

	const HANDLE hfind = FindFirstFile (_r_fmt (L"%s\\*.*", path), &wfd);

	if (hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((wfd.cFileName[0] == '.' && !wfd.cFileName[1]) || (wfd.cFileName[0] == '.' && wfd.cFileName[1] == '.' && !wfd.cFileName[2]))
				continue;

			rstring full_path;
			full_path.Format (L"%s\\%s", path, wfd.cFileName);

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				_r_fs_rmdir (full_path);
			}
			else
			{
				SetFileAttributes (full_path, FILE_ATTRIBUTE_NORMAL);
				DeleteFile (full_path);
			}
		}
		while (FindNextFile (hfind, &wfd));

		FindClose (hfind);
	}

	RemoveDirectory (path);
}

bool _r_fs_move (LPCWSTR path_from, LPCWSTR path_to, DWORD flags)
{
	if (MoveFileEx (path_from, path_to, flags))
		return true;

	return false;
}

bool _r_fs_copy (LPCWSTR path_from, LPCWSTR path_to, DWORD flags)
{
	if (CopyFileEx (path_from, path_to, nullptr, nullptr, nullptr, flags))
		return true;

	return false;
}

/*
	Paths
*/

rstring _r_path_gettempfilepath (LPCWSTR directory, LPCWSTR filename)
{
	WCHAR tmp_directory[MAX_PATH] = {0};

	if (!directory)
		GetTempPath (_countof (tmp_directory), tmp_directory);
	else
		StringCchCopy (tmp_directory, _countof (tmp_directory), directory);

	WCHAR result[MAX_PATH] = {0};

	if (filename)
	{
		StringCchPrintf (result, _countof (result), L"%s\\%s.tmp", tmp_directory, filename);
	}
	else
	{
		GetTempFileName (tmp_directory, nullptr, 0, result);
	}

	return result;
}

rstring _r_path_expand (const rstring& path)
{
	if (path.IsEmpty ())
		return path;

	const size_t percent_pos = path.Find (L'%');
	const size_t separator_pos = path.Find (OBJ_NAME_PATH_SEPARATOR);

	if (separator_pos == rstring::npos && percent_pos == rstring::npos)
		return path;

	rstring result;

	if (percent_pos != rstring::npos)
	{
		if (!ExpandEnvironmentStrings (path, result.GetBuffer (4096), 4096))
		{
			result.ReleaseBuffer ();
			result = path;
		}
	}
	else
	{
		if (!PathSearchAndQualify (path, result.GetBuffer (4096), 4096))
		{
			result.ReleaseBuffer ();
			result = path;
		}
	}

	result.ReleaseBuffer ();

	if (result.Find (L'~') != rstring::npos)
	{
		GetLongPathName (result.GetString (), result.GetBuffer (4096), 4096);
		result.ReleaseBuffer ();
	}

	return result;
}

rstring _r_path_unexpand (const rstring& path)
{
	if (path.IsEmpty ())
		return path;

	if (path.Find (OBJ_NAME_PATH_SEPARATOR) == rstring::npos)
		return path;

	rstring result;

	if (!PathUnExpandEnvStrings (path, result.GetBuffer (4096), 4096))
	{
		result.ReleaseBuffer ();
		result = path;
	}

	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_compact (LPCWSTR path, size_t length)
{
	rstring result;

	PathCompactPathEx (result.GetBuffer (length), path, (UINT)length, 0);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_extractdir (LPCWSTR path)
{
	rstring result = path;
	const size_t pos = result.ReverseFind (L"\\/");

	result.Mid (0, pos);
	result.Trim (L"\\/");

	return result;
}

rstring _r_path_extractfile (LPCWSTR path)
{
	return PathFindFileName (path);
}

// Author: Elmue
// https://stackoverflow.com/a/18792477
//
// converts
// "\Device\HarddiskVolume3"                                -> "E:"
// "\Device\HarddiskVolume3\Temp"                           -> "E:\Temp"
// "\Device\HarddiskVolume3\Temp\transparent.jpeg"          -> "E:\Temp\transparent.jpeg"
// "\Device\Harddisk1\DP(1)0-0+6\foto.jpg"                  -> "I:\foto.jpg"
// "\Device\TrueCryptVolumeP\Data\Passwords.txt"            -> "P:\Data\Passwords.txt"
// "\Device\Floppy0\Autoexec.bat"                           -> "A:\Autoexec.bat"
// "\Device\CdRom1\VIDEO_TS\VTS_01_0.VOB"                   -> "H:\VIDEO_TS\VTS_01_0.VOB"
// "\Device\Serial1"                                        -> "COM1"
// "\Device\USBSER000"                                      -> "COM4"
// "\Device\Mup\ComputerName\C$\Boot.ini"                   -> "\\ComputerName\C$\Boot.ini"
// "\Device\LanmanRedirector\ComputerName\C$\Boot.ini"      -> "\\ComputerName\C$\Boot.ini"
// "\Device\LanmanRedirector\ComputerName\Shares\Dance.m3u" -> "\\ComputerName\Shares\Dance.m3u"
// returns an error for any other device type

rstring _r_path_dospathfromnt (LPCWSTR path)
{
	rstring result = path;

	// calculate device length
	size_t device_length = rstring::npos;

	for (size_t i = 0, cnt = 0; i < result.GetLength (); i++)
	{
		if (result.At (i) == OBJ_NAME_PATH_SEPARATOR)
		{
			if (++cnt >= 3)
			{
				device_length = i;
				break;
			}
		}
	}

	if (device_length == rstring::npos)
		return result;

	if (_wcsnicmp (path, L"\\Device\\Mup\\", device_length + 1) == 0) // network share (win7+)
	{
		result.Format (L"\\\\%s", path + device_length + 1);
	}
	else if (_wcsnicmp (path, L"\\Device\\LanmanRedirector\\", device_length + 1) == 0) // network share (winxp+)
	{
		result.Format (L"\\\\%s", path + device_length + 1);
	}
	else
	{
		WCHAR drives[128] = {0};
		const DWORD count = GetLogicalDriveStrings (_countof (drives), drives);

		if (count && count <= _countof (drives))
		{
			LPWSTR drv = drives;

			while (*drv)
			{
				WCHAR drive[8] = {0};
				StringCchCopy (drive, _countof (drive), drv);

				// check drive is ready
				const UINT old_errmode = SetErrorMode (SEM_FAILCRITICALERRORS);
				const bool is_ready = GetVolumeInformation (drive, nullptr, 0, nullptr, 0, 0, nullptr, 0);
				SetErrorMode (old_errmode);

				if (is_ready)
				{
					WCHAR volume[MAX_PATH] = {0};

					drive[2] = 0; // the backslash is not allowed for QueryDosDevice()

					// may return multiple strings!
					// returns very weird strings for network shares
					if (QueryDosDevice (drive, volume, _countof (volume)))
					{
						if (_wcsnicmp (path, volume, device_length) == 0)
						{
							result = drive;
							result.Append (path + device_length);

							break;
						}
					}
				}

				drv += _r_str_length (drv) + 1;
			}
		}
	}

	return result;
}

// Author: Elmue
// https://stackoverflow.com/a/18792477
//
// returns
// "\Device\HarddiskVolume3"                                (Harddisk Drive)
// "\Device\HarddiskVolume3\Temp"                           (Harddisk Directory)
// "\Device\HarddiskVolume3\Temp\transparent.jpeg"          (Harddisk File)
// "\Device\Harddisk1\DP(1)0-0+6\foto.jpg"                  (USB stick)
// "\Device\TrueCryptVolumeP\Data\Passwords.txt"            (Truecrypt Volume)
// "\Device\Floppy0\Autoexec.bat"                           (Floppy disk)
// "\Device\CdRom1\VIDEO_TS\VTS_01_0.VOB"                   (DVD drive)
// "\Device\Serial1"                                        (real COM port)
// "\Device\USBSER000"                                      (virtual COM port)
// "\Device\Mup\ComputerName\C$\Boot.ini"                   (network drive share,  Windows 7)
// "\Device\LanmanRedirector\ComputerName\C$\Boot.ini"      (network drive share,  Windows XP)
// "\Device\LanmanRedirector\ComputerName\Shares\Dance.m3u" (network folder share, Windows XP)
// "\Device\Afd"                                            (internet socket)
// "\Device\NamedPipe\Pipename"                             (named pipe)
// "\BaseNamedObjects\Objectname"                           (named mutex, named event, named semaphore)
// "\REGISTRY\MACHINE\SOFTWARE\Classes\.txt"                (HKEY_CLASSES_ROOT\.txt)

#ifdef _APP_HAVE_NTDLL
DWORD _r_path_ntpathfromdos (rstring& path)
{
	DWORD result = ERROR_BAD_ARGUMENTS;

	if (!path.IsEmpty ())
	{
		const HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

		if (hfile == INVALID_HANDLE_VALUE)
		{
			result = GetLastError ();
		}
		else
		{
			ULONG req_length = 0;
			ULONG out_length = 0;

			// IMPORTANT: The return value from NtQueryObject is bullshit! (driver bug?)
			// - The function may return STATUS_NOT_SUPPORTED although it has successfully written to the buffer.
			// - The function returns STATUS_SUCCESS although h_File == 0xFFFFFFFF
			NTSTATUS status = NtQueryObject (hfile, ObjectNameInformation, nullptr, 0, &req_length);

			if (!req_length)
			{
				result = ((status != ERROR_SUCCESS) ? status : ERROR_INVALID_FUNCTION); // see notice
			}
			else
			{
				PBYTE pbuffer = new BYTE[req_length];

				if (pbuffer)
				{
					UNICODE_STRING* pk_Info = &((OBJECT_NAME_INFORMATION*)pbuffer)->Name;
					pk_Info->Buffer = 0;
					pk_Info->Length = 0;

					status = NtQueryObject (hfile, ObjectNameInformation, pbuffer, req_length, &out_length);

					if (!pk_Info->Length || !pk_Info->Buffer || !out_length)
					{
						result = ((status != ERROR_SUCCESS) ? status : ERROR_INVALID_FUNCTION); // see notice
					}
					else
					{
						pk_Info->Buffer[pk_Info->Length / sizeof (WCHAR)] = 0; // trim buffer!

						path = pk_Info->Buffer;
						path.ToLower (); // lower is imoprtant!

						result = ERROR_SUCCESS;
					}

					SAFE_DELETE_ARRAY (pbuffer);
				}
			}

			CloseHandle (hfile);
		}
	}

	return result;
}
#endif // _APP_HAVE_NTDLL

/*
	Strings
*/

bool _r_str_alloc (LPWSTR* pwstr, size_t length, LPCWSTR text)
{
	if (pwstr)
	{
		SAFE_DELETE_ARRAY (*pwstr);

		if (length)
		{
			length += 1;

			LPWSTR new_ptr = new WCHAR[length];

			if (new_ptr)
			{
				StringCchCopy (new_ptr, length, text);
				*pwstr = new_ptr;

				return true;
			}
		}
	}

	return false;
}

rstring _r_str_fromguid (const GUID& lpguid)
{
	rstring result;
	OLECHAR* guidString = nullptr;

	if (SUCCEEDED (StringFromCLSID (lpguid, &guidString)))
	{
		result = guidString;

		CoTaskMemFree (guidString);
	}

	return result;
}

size_t _r_str_length (LPCWSTR str)
{
	size_t length = 0;
	StringCchLength (str, STRSAFE_MAX_LENGTH, &length);

	return length;
}

WCHAR _r_str_lower (WCHAR chr)
{
	WCHAR buf[] = {chr, 0};
	CharLowerBuff (buf, _countof (buf));

	return buf[0];
}

WCHAR _r_str_upper (WCHAR chr)
{
	WCHAR buf[] = {chr, 0};
	CharUpperBuff (buf, _countof (buf));

	return buf[0];
}

bool _r_str_match (LPCWSTR str, LPCWSTR pattern)
{
	// If we reach at the end of both strings, we are done
	if (*pattern == 0 && *str == 0)
		return true;

	// Make sure that the characters after '*' are present
	// in second string. This function assumes that the first
	// string will not contain two consecutive '*'
	if (*pattern == L'*' && *(pattern + 1) != 0 && *str == 0)
		return false;

	// If the first string contains '?', or current characters
	// of both strings match
	if (*pattern == L'?' || _r_str_lower (*str) == _r_str_lower (*pattern))
		return _r_str_match (str + 1, pattern + 1);

	// If there is *, then there are two possibilities
	// a) We consider current character of second string
	// b) We ignore current character of second string.
	if (*pattern == L'*')
		return _r_str_match (str + 1, pattern) || _r_str_match (str, pattern + 1);

	return false;
}

size_t _r_str_hash (LPCWSTR text)
{
	if (!text || !text[0])
		return 0;

#define InitialFNV 2166136261ULL
#define FNVMultiple 16777619

	size_t hash = InitialFNV;
	const size_t length = _r_str_length (text);

	for (size_t i = 0; i < length; i++)
	{
		hash = hash ^ (_r_str_lower (text[i])); /* xor the low 8 bits */
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

bool _r_str_unserialize (rstring string, LPCWSTR str_delimeter, WCHAR key_delimeter, rstring::map_one* lpresult)
{
	if (!lpresult)
		return false;

	const rstring::rvector vc = string.AsVector (str_delimeter);

	for (size_t i = 0; i < vc.size (); i++)
	{
		const rstring name = vc.at (i);
		const size_t pos = name.Find (key_delimeter);

		if (pos != rstring::npos)
			(*lpresult)[name.Midded (0, pos)] = name.Midded (pos + 1);
	}

	return true;
}

/*
	System information
*/

bool _r_sys_isadmin ()
{
	BOOL result = FALSE;
	DWORD status = 0, ps_size = sizeof (PRIVILEGE_SET);

	HANDLE token = nullptr;
	HANDLE impersonation_token = nullptr;

	PRIVILEGE_SET ps = {0};
	GENERIC_MAPPING gm = {0};

	PACL pacl = nullptr;
	PSID psid = nullptr;
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

		if (!AllocateAndInitializeSid (&sia, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &psid))
			__leave;

		sd = LocalAlloc (LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

		if (!sd || !InitializeSecurityDescriptor (sd, SECURITY_DESCRIPTOR_REVISION))
			__leave;

		DWORD acl_size = sizeof (ACL) + sizeof (ACCESS_ALLOWED_ACE) + (size_t)GetLengthSid (psid) - sizeof (DWORD);
		pacl = (PACL)LocalAlloc (LPTR, (SIZE_T)acl_size);

		if (!pacl || !InitializeAcl (pacl, acl_size, ACL_REVISION2) || !AddAccessAllowedAce (pacl, ACL_REVISION2, ACCESS_READ | ACCESS_WRITE, psid) || !SetSecurityDescriptorDacl (sd, TRUE, pacl, FALSE))
			__leave;

		SetSecurityDescriptorGroup (sd, psid, FALSE);
		SetSecurityDescriptorOwner (sd, psid, FALSE);

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
		SAFE_LOCAL_FREE (pacl);
		SAFE_LOCAL_FREE (sd);

		if (psid)
			FreeSid (psid);

		if (impersonation_token)
			CloseHandle (impersonation_token);

		if (token)
			CloseHandle (token);
	}

	return result ? true : false;
}

ULONGLONG _r_sys_gettickcount ()
{
	static LARGE_INTEGER s_frequency = {0};
	static const bool is_qpc = QueryPerformanceFrequency (&s_frequency);

	if (is_qpc)
	{
		LARGE_INTEGER now = {0};
		QueryPerformanceCounter (&now);

		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}

#ifdef _APP_NO_WINXP
	return GetTickCount64 ();
#else
	// Try GetTickCount64 (vista+)
	if (_r_sys_validversion (6, 0))
	{
		const HMODULE hlib = GetModuleHandle (L"kernel32.dll");

		if (hlib)
		{
			typedef ULONGLONG (WINAPI *GTC64) (VOID); // GetTickCount64
			const GTC64 _GetTickCount64 = (GTC64)GetProcAddress (hlib, "GetTickCount64");

			if (_GetTickCount64)
				return _GetTickCount64 ();
		}
	}

	return GetTickCount ();
#endif // _APP_NO_WINXP
}

void _r_sys_getusername (rstring* pdomain, rstring* pusername)
{
	LPWSTR username = nullptr;
	LPWSTR domain = nullptr;

	DWORD username_bytes = 0;
	DWORD domain_bytes = 0;

	if (
		WTSQuerySessionInformation (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSUserName, &username, &username_bytes) &&
		WTSQuerySessionInformation (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSDomainName, &domain, &domain_bytes)
		)
	{
		if (pusername)
			*pusername = username;

		if (pdomain)
			*pdomain = domain;
	}

	if (username)
		WTSFreeMemory (username);

	if (domain)
		WTSFreeMemory (domain);
}

rstring _r_sys_getusernamesid (LPCWSTR domain, LPCWSTR username)
{
	rstring result;

	DWORD sid_length = SECURITY_MAX_SID_SIZE;
	PBYTE psid = new BYTE[sid_length];

	if (psid)
	{
		WCHAR ref_domain[MAX_PATH] = {0};
		DWORD ref_length = _countof (ref_domain);

		SID_NAME_USE name_use;

		if (LookupAccountName (domain, username, psid, &sid_length, ref_domain, &ref_length, &name_use))
		{
			LPWSTR sidstring = nullptr;

			if (ConvertSidToStringSid (psid, &sidstring))
			{
				result = sidstring;

				SAFE_LOCAL_FREE (sidstring);
			}
		}

		SAFE_DELETE_ARRAY (psid);
	}

	return result;
}

#ifndef _WIN64
bool _r_sys_iswow64 ()
{
	BOOL result = FALSE;

	// IsWow64Process is not available on all supported versions of Windows.
	// Use GetModuleHandle to get a handle to the DLL that contains the function
	// and GetProcAddress to get a pointer to the function if available.

	const HMODULE hlib = GetModuleHandle (L"kernel32.dll");

	if (hlib)
	{
		typedef BOOL (WINAPI *IW64P) (HANDLE, PBOOL); // IsWow64Process
		const IW64P _IsWow64Process = (IW64P)GetProcAddress (hlib, "IsWow64Process");

		if (_IsWow64Process)
			_IsWow64Process (GetCurrentProcess (), &result);

		if (result)
			return true;
	}

	return false;
}
#endif // _WIN64

bool _r_sys_setprivilege (LPCWSTR privileges[], size_t count, bool is_enable)
{
	HANDLE token = nullptr;

	LUID luid = {0};
	TOKEN_PRIVILEGES tp = {0};

	bool result = false;

	if (OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
	{
		for (size_t i = 0; i < count; i++)
		{
			if (LookupPrivilegeValue (nullptr, privileges[i], &luid))
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Luid = luid;
				tp.Privileges[0].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;

				if (AdjustTokenPrivileges (token, FALSE, &tp, sizeof (tp), nullptr, nullptr))
					result = true;
			}
		}

		CloseHandle (token);
	}

	return result;
}

bool _r_sys_uacstate ()
{
	HANDLE token = nullptr;
	DWORD out_length = 0;
	TOKEN_ELEVATION_TYPE tet;
	bool result = false;

	if (_r_sys_validversion (6, 0))
	{
		if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token) && GetTokenInformation (token, TokenElevationType, &tet, sizeof (tet), &out_length) && tet == TokenElevationTypeLimited)
		{
			result = true;
		}

		if (token)
		{
			CloseHandle (token);
		}
	}

	return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ms725494(v=vs.85).aspx
bool _r_sys_validversion (DWORD major, DWORD minor, DWORD build, BYTE condition)
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

	if (VerifyVersionInfo (&osvi, type_mask, mask))
		return true;

	return false;
}

void _r_sleep (DWORD milliseconds)
{
	if (!milliseconds || milliseconds == INFINITE)
		return;

	WaitForSingleObjectEx (GetCurrentThread (), milliseconds, FALSE);
}

/*
	Unixtime
*/

time_t _r_unixtime_now ()
{
	SYSTEMTIME st = {0};
	GetSystemTime (&st);

	return _r_unixtime_from_systemtime (&st);
}

void _r_unixtime_to_filetime (time_t ut, const LPFILETIME pft)
{
	if (pft)
	{
		const LONGLONG ll = Int32x32To64 (ut, 10000000) + 116444736000000000; // 64-bit value

		pft->dwLowDateTime = DWORD (ll);
		pft->dwHighDateTime = ll >> 32;
	}
}

void _r_unixtime_to_systemtime (time_t ut, const LPSYSTEMTIME pst)
{
	FILETIME ft = {0};

	_r_unixtime_to_filetime (ut, &ft);

	FileTimeToSystemTime (&ft, pst);
}

time_t _r_unixtime_from_filetime (const FILETIME* pft)
{
	ULARGE_INTEGER ull = {0};

	if (pft)
	{
		ull.LowPart = pft->dwLowDateTime;
		ull.HighPart = pft->dwHighDateTime;
	}

	return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

time_t _r_unixtime_from_systemtime (const LPSYSTEMTIME pst)
{
	FILETIME ft = {0};
	SystemTimeToFileTime (pst, &ft);

	return _r_unixtime_from_filetime (&ft);
}

/*
	Optimized version of WinAPI function "FillRect"
*/

COLORREF _r_dc_getcolorbrightness (COLORREF clr)
{
	ULONG r = clr & 0xff;
	ULONG g = (clr >> 8) & 0xff;
	ULONG b = (clr >> 16) & 0xff;
	ULONG min = 0;
	ULONG max = 0;

	min = r;
	if (g < min) min = g;
	if (b < min) min = b;

	max = r;
	if (g > max) max = g;
	if (b > max) max = b;

	if (((min + max) / 2) > 100)
		return RGB (0x00, 0x00, 0x00);

	return RGB (0xff, 0xff, 0xff);
}

void _r_dc_fillrect (HDC hdc, LPRECT lprc, COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (hdc, clr);
	ExtTextOut (hdc, 0, 0, ETO_OPAQUE, lprc, nullptr, 0, nullptr);
	SetBkColor (hdc, clr_prev);
}

int _r_dc_fontsizetoheight (INT size)
{
	HDC hdc = GetDC (nullptr);
	int result = -MulDiv (size, GetDeviceCaps (hdc, LOGPIXELSY), 72);
	ReleaseDC (nullptr, hdc);

	return result;
}

int _r_dc_fontheighttosize (INT size)
{
	HDC hdc = GetDC (nullptr);
	int result = MulDiv (-size, 72, GetDeviceCaps (hdc, LOGPIXELSY));
	ReleaseDC (nullptr, hdc);

	return result;
}

/*
	Window management
*/

void _r_wnd_addstyle (HWND hwnd, UINT ctrl_id, LONG_PTR mask, LONG_PTR stateMask, INT index)
{
	if (ctrl_id)
		hwnd = GetDlgItem (hwnd, ctrl_id);

	LONG_PTR style = (GetWindowLongPtr (hwnd, index) & ~stateMask) | mask;

	SetWindowLongPtr (hwnd, index, style);

	SetWindowPos (hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void _r_wnd_adjustwindowrect (HWND hwnd, LPRECT lprect)
{
	MONITORINFO monitorInfo = {0};
	monitorInfo.cbSize = sizeof (monitorInfo);

	const HMONITOR hmonitor = hwnd ? MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST) : MonitorFromRect (lprect, MONITOR_DEFAULTTONEAREST);

	if (GetMonitorInfo (hmonitor, &monitorInfo))
	{
		LPRECT lpbounds = &monitorInfo.rcWork;

		const int original_width = _R_RECT_WIDTH (lprect);
		const int original_height = _R_RECT_HEIGHT (lprect);

		if (lprect->left + original_width > lpbounds->left + _R_RECT_WIDTH (lpbounds))
			lprect->left = lpbounds->left + _R_RECT_WIDTH (lpbounds) - original_width;

		if (lprect->top + original_height > lpbounds->top + _R_RECT_HEIGHT (lpbounds))
			lprect->top = lpbounds->top + _R_RECT_HEIGHT (lpbounds) - original_height;

		if (lprect->left < lpbounds->left)
			lprect->left = lpbounds->left;

		if (lprect->top < lpbounds->top)
			lprect->top = lpbounds->top;

		lprect->right = lprect->left + original_width;
		lprect->bottom = lprect->top + original_height;
	}
}

void _r_wnd_centerwindowrect (LPRECT lprect, LPRECT lpparent)
{
	lprect->left = lpparent->left + (_R_RECT_WIDTH (lpparent) - _R_RECT_WIDTH (lprect)) / 2;
	lprect->top = lpparent->top + (_R_RECT_HEIGHT (lpparent) - _R_RECT_HEIGHT (lprect)) / 2;
}

void _r_wnd_center (HWND hwnd, HWND hparent)
{
	if (hparent && IsWindowVisible (hparent) && !IsIconic (hparent))
	{
		RECT rect = {0}, parentRect = {0};

		GetWindowRect (hwnd, &rect);
		GetWindowRect (hparent, &parentRect);

		_r_wnd_centerwindowrect (&rect, &parentRect);
		_r_wnd_adjustwindowrect (hwnd, &rect);

		SetWindowPos (hwnd, nullptr, rect.left, rect.top, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE);
	}
	else
	{
		MONITORINFO monitorInfo = {0};
		monitorInfo.cbSize = sizeof (monitorInfo);

		if (GetMonitorInfo (MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo))
		{
			RECT rect = {0};
			GetWindowRect (hwnd, &rect);

			_r_wnd_centerwindowrect (&rect, &monitorInfo.rcWork);

			SetWindowPos (hwnd, nullptr, rect.left, rect.top, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE);
		}
	}
}

void _r_wnd_changemessagefilter (HWND hwnd, UINT msg, DWORD action)
{
	if (_r_sys_validversion (6, 0))
	{
		const HMODULE hlib = GetModuleHandle (L"user32.dll");

		if (hlib)
		{
			typedef BOOL (WINAPI *CWMFEX) (HWND, UINT, DWORD, PVOID); // ChangeWindowMessageFilterEx
			const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (hlib, "ChangeWindowMessageFilterEx"); // win7+

			if (_ChangeWindowMessageFilterEx)
			{
				_ChangeWindowMessageFilterEx (hwnd, msg, action, nullptr);
			}
			else
			{
				typedef BOOL (WINAPI *CWMF) (UINT, DWORD); // ChangeWindowMessageFilter
				const CWMF _ChangeWindowMessageFilter = (CWMF)GetProcAddress (hlib, "ChangeWindowMessageFilter"); // vista fallback

				if (_ChangeWindowMessageFilter)
					_ChangeWindowMessageFilter (msg, action);
			}
		}
	}
}

void _r_wnd_toggle (HWND hwnd, bool show)
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

void _r_wnd_top (HWND hwnd, bool is_enable)
{
	SetWindowPos (hwnd, (is_enable ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Author: Mikhail
// https://stackoverflow.com/a/9126096

bool _r_wnd_undercursor (HWND hwnd)
{
	if (!IsWindowVisible (hwnd))
		return false;

	POINT pt = {0};
	RECT rect = {0};

	GetCursorPos (&pt);
	GetWindowRect (hwnd, &rect);

	if (PtInRect (&rect, pt))
		return true;

	return false;
}

static bool _r_wnd_isplatformfullscreenmode ()
{
	// SHQueryUserNotificationState is only available for Vista+
	if (_r_sys_validversion (6, 0))
	{
		typedef HRESULT (WINAPI *SHQueryUserNotificationStatePtr)(QUERY_USER_NOTIFICATION_STATE* state);

		const HMODULE hlib = GetModuleHandle (L"shell32.dll");

		if (hlib)
		{
			const SHQueryUserNotificationStatePtr _SHQueryUserNotificationState = (SHQueryUserNotificationStatePtr)GetProcAddress (hlib, "SHQueryUserNotificationState");

			if (_SHQueryUserNotificationState)
			{
				QUERY_USER_NOTIFICATION_STATE state;

				if (_SHQueryUserNotificationState (&state) == S_OK)
					return (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE);
			}
		}
	}

	return false;
}

static bool _r_wnd_isfullscreenwindowmode ()
{
	// Get the foreground window which the user is currently working on.
	HWND wnd = GetForegroundWindow ();
	if (!wnd)
		return false;

	// Get the monitor where the window is located.
	RECT wnd_rect;
	if (!GetWindowRect (wnd, &wnd_rect))
		return false;
	HMONITOR monitor = MonitorFromRect (&wnd_rect, MONITOR_DEFAULTTONULL);
	if (!monitor)
		return false;
	MONITORINFO monitor_info = {sizeof (monitor_info)};
	if (!GetMonitorInfo (monitor, &monitor_info))
		return false;

	// It should be the main monitor.
	if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
		return false;

	// The window should be at least as large as the monitor.
	if (!IntersectRect (&wnd_rect, &wnd_rect, &monitor_info.rcMonitor))
		return false;
	if (!EqualRect (&wnd_rect, &monitor_info.rcMonitor))
		return false;

	// At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
	// its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
	LONG_PTR style = GetWindowLongPtr (wnd, GWL_STYLE);
	LONG_PTR ext_style = GetWindowLongPtr (wnd, GWL_EXSTYLE);

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
		(ext_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

static bool _r_wnd_isfullscreenconsolemode ()
{
	// We detect this by attaching the current process to the console of the
	// foreground window and then checking if it is in full screen mode.
	DWORD pid = 0;
	GetWindowThreadProcessId (GetForegroundWindow (), &pid);
	if (!pid)
		return false;

	if (!AttachConsole (pid))
		return false;

	DWORD modes = 0;
	GetConsoleDisplayMode (&modes);
	FreeConsole ();

	return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

bool _r_wnd_isfullscreenmode ()
{
	return _r_wnd_isplatformfullscreenmode () ||
		_r_wnd_isfullscreenwindowmode () ||
		_r_wnd_isfullscreenconsolemode ();
}

void _r_wnd_resize (HDWP* hdefer, HWND hwnd, HWND hwnd_after, INT left, INT right, INT width, INT height, UINT flags)
{
	flags |= SWP_NOZORDER | SWP_NOACTIVATE;

	if (!width && !height)
		flags |= SWP_NOSIZE;

	if (hdefer && *hdefer)
		*hdefer = DeferWindowPos (*hdefer, hwnd, hwnd_after, left, right, width, height, flags);

	else
		SetWindowPos (hwnd, hwnd_after, left, right, width, height, flags);
}

#ifndef _APP_NO_DARKTHEME
#ifdef _APP_HAVE_DARKTHEME_SUBCLASS
LRESULT CALLBACK DarkExplorerSubclassProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	const WNDPROC orig_proc = (WNDPROC)GetProp (hwnd, L"orig_proc");
	const bool is_darktheme = _r_wnd_isdarktheme ();

	switch (msg)
	{
		case WM_DESTROY:
		{
			RemoveProp (hwnd, L"orig_proc");
			SetWindowLongPtr (hwnd, GWLP_WNDPROC, (LONG_PTR)orig_proc);

			break;
		}

		//case WM_ERASEBKGND:
		//{
		//	HDC hDC = (HDC)wparam;
		//	RECT rc = {0};
		//	GetClientRect (hwnd, &rc);

		//	if (is_darktheme)
		//	{
		//		// in dark mode, just paint the whole background in black
		//		_r_dc_fillrect (hDC, &rc, GetSysColor (COLOR_WINDOWTEXT));
		//	}

		//	break;
		//}

		case WM_CTLCOLOREDIT:
		{
			SetBkMode ((HDC)wparam, TRANSPARENT);

			if (is_darktheme)
			{
				SetTextColor ((HDC)wparam, DarkThemeTextColor);
				SetDCBrushColor ((HDC)wparam, RGB (60, 60, 60));
			}
			else
			{
				SetTextColor ((HDC)wparam, RGB (0x0, 0x0, 0x0));
				SetDCBrushColor ((HDC)wparam, DarkThemeTextColor);
			}

			return (INT_PTR)GetStockObject (DC_BRUSH);
		}

		case WM_CTLCOLORBTN:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		{
			SetBkMode ((HDC)wparam, TRANSPARENT);

			if (is_darktheme)
			{
				SetTextColor ((HDC)wparam, DarkThemeTextColor);
				SetDCBrushColor ((HDC)wparam, RGB (30, 30, 30));
			}
			else
			{
				SetTextColor ((HDC)wparam, RGB (0x0, 0x0, 0x0));
				SetDCBrushColor ((HDC)wparam, DarkThemeTextColor);
			}

			return (INT_PTR)GetStockObject (DC_BRUSH);
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lparam;
					WCHAR classname[128] = {0};

					if (GetClassName (lpnmcd->hdr.hwndFrom, classname, _countof (classname)))
					{
						if (_wcsicmp (classname, WC_BUTTON) == 0)
						{
							//return DrawButton (lpnmcd);
						}
						else if (_wcsicmp (classname, WC_LISTVIEW) == 0)
						{
							LPNMLVCUSTOMDRAW listViewCustomDraw = (LPNMLVCUSTOMDRAW)lpnmcd;

							if (listViewCustomDraw->dwItemType == LVCDI_GROUP && is_darktheme)
							{
								//return DrawListViewGroup (listViewCustomDraw);
							}
						}
					}

					break;
				}
			}
		}
	}

	if (!orig_proc) // check (required!)
		return FALSE;

	return CallWindowProc (orig_proc, hwnd, msg, wparam, lparam);
}
#endif // _APP_HAVE_DARKTHEME_SUBCLASS

BOOL CALLBACK DarkExplorerChildProc (HWND hwnd, LPARAM lparam)
{
	const bool is_darktheme = (bool)lparam;

	typedef bool (WINAPI *ADMFW) (HWND window, bool allow); // AllowDarkModeForWindow
	const ADMFW _AllowDarkModeForWindow = (ADMFW)GetProcAddress (GetModuleHandle (L"uxtheme.dll"), MAKEINTRESOURCEA (133));

	if (!IsWindow (hwnd))
		return TRUE;

	if (_AllowDarkModeForWindow)
		_AllowDarkModeForWindow (hwnd, is_darktheme);

#ifdef _APP_HAVE_DARKTHEME_SUBCLASS
	const WNDPROC defaultWindowProc = (WNDPROC)GetWindowLongPtr (hwnd, GWLP_WNDPROC);

	if (defaultWindowProc != DarkExplorerSubclassProc)
	{
		SetProp (hwnd, L"orig_proc", defaultWindowProc);
		SetWindowLongPtr (hwnd, GWLP_WNDPROC, (LONG_PTR)&DarkExplorerSubclassProc);
	}
#endif // _APP_HAVE_DARKTHEME_SUBCLASS

	WCHAR classname[128] = {0};

	if (GetClassName (hwnd, classname, _countof (classname)))
	{
		if (_wcsicmp (classname, WC_LISTVIEW) == 0)
		{
			//if (is_darktheme)
			//{
			//	ListView_SetBkColor (hwnd, RGB (30, 30, 30));
			//	ListView_SetTextBkColor (hwnd, RGB (30, 30, 30));
			//	ListView_SetTextColor (hwnd, DarkThemeTextColor);
			//}
			//else
			//{
			//	ListView_SetBkColor (hwnd, DarkThemeTextColor);
			//	ListView_SetTextBkColor (hwnd, DarkThemeTextColor);
			//	ListView_SetTextColor (hwnd, RGB (0x0, 0x0, 0x0));
			//}
		}
		else if (_wcsicmp (classname, WC_TREEVIEW) == 0)
		{
			//if (is_darktheme)
			//{
			//	TreeView_SetBkColor (hwnd, RGB (30, 30, 30));
			//	TreeView_SetLineColor (hwnd, RGB (30, 30, 30));
			//	TreeView_SetTextColor (hwnd, RGB (30, 30, 30));
			//}
			//else
			//{
			//	TreeView_SetBkColor (hwnd, RGB (0xff, 0xff, 0xff));
			//	TreeView_SetLineColor (hwnd, RGB (0xff, 0xff, 0xff));
			//	TreeView_SetTextColor (hwnd, RGB (0x0, 0x0, 0x0));
			//}
		}
	}

	InvalidateRect (hwnd, nullptr, TRUE);

	return TRUE;
}

bool _r_wnd_isdarktheme ()
{
	if (!_r_sys_validversion (10, 0, 17763)) // win10rs5+
		return false;

	HIGHCONTRAST info = {0};
	info.cbSize = sizeof (HIGHCONTRAST);

	if (SystemParametersInfo (SPI_GETHIGHCONTRAST, 0, &info, 0))
	{
		// no dark mode in high-contrast mode
		if ((info.dwFlags & HCF_HIGHCONTRASTON) != 0)
			return false;
	}

	typedef bool (WINAPI *SAUDM) (); // ShouldAppsUseDarkMode

	const SAUDM _ShouldAppsUseDarkMode = (SAUDM)GetProcAddress (GetModuleHandle (L"uxtheme.dll"), MAKEINTRESOURCEA (132));

	if (_ShouldAppsUseDarkMode)
		return _ShouldAppsUseDarkMode ();

	return false;
}

bool _r_wnd_setdarktheme (HWND hwnd)
{
	if (!_r_sys_validversion (10, 0, 17763)) // win10rs5+
		return false;

	const bool is_darktheme = _r_wnd_isdarktheme ();

	/*
		Ordinal 104: void WINAPI RefreshImmersiveColorPolicyState()
		Ordinal 132: bool WINAPI ShouldAppsUseDarkMode()
		Ordinal 133: bool WINAPI AllowDarkModeForWindow(HWND__ *window, bool allow)
		Ordinal 135: bool WINAPI AllowDarkModeForApp(bool allow)
		Ordinal 136: void WINAPI FlushMenuThemes()
		Ordinal 137: bool WINAPI IsDarkModeAllowedForWindow(HWND__ *window)
	*/

	typedef bool (WINAPI *ADMFA) (bool allow); // AllowDarkModeForApp
	typedef bool (WINAPI *ADMFW) (HWND window, bool allow); // AllowDarkModeForWindow
	typedef void (WINAPI *FMT) (); // FlushMenuThemes
	typedef HRESULT (WINAPI *DSWA) (HWND, DWORD, LPCVOID, DWORD); // DwmSetWindowAttribute

	const HMODULE hlib1 = GetModuleHandle (L"uxtheme.dll");
	const HMODULE hlib2 = GetModuleHandle (L"dwmapi.dll");

	if (hlib1 && hlib2)
	{
		const ADMFA _AllowDarkModeForApp = (ADMFA)GetProcAddress (hlib1, MAKEINTRESOURCEA (135));
		const ADMFW _AllowDarkModeForWindow = (ADMFW)GetProcAddress (hlib1, MAKEINTRESOURCEA (133));
		const DSWA _DwmSetWindowAttribute = (DSWA)GetProcAddress (hlib2, "DwmSetWindowAttribute");

		if (_AllowDarkModeForApp && _AllowDarkModeForWindow && _DwmSetWindowAttribute)
		{
			_AllowDarkModeForApp (is_darktheme);
			_AllowDarkModeForWindow (hwnd, is_darktheme);

			// Set dark window frame
			// https://social.msdn.microsoft.com/Forums/en-US/e36eb4c0-4370-4933-943d-b6fe22677e6c/dark-mode-apis?forum=windowssdk
			BOOL is_dwmdarkmode = is_darktheme;
			_DwmSetWindowAttribute (hwnd, 0x13, &is_dwmdarkmode, sizeof (is_dwmdarkmode));

			EnumChildWindows (hwnd, &DarkExplorerChildProc, is_darktheme);

			const FMT _FlushMenuThemes = (FMT)GetProcAddress (hlib1, MAKEINTRESOURCEA (136));

			if (_FlushMenuThemes)
				_FlushMenuThemes ();

			//RedrawWindow (hwnd, nullptr, nullptr, RDW_FRAME | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

			//SetClassLongPtr (hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetSysColorBrush (BLACK_BRUSH));

			return true;
		}
	}

	return false;
}
#endif // _APP_NO_DARKTHEME

/*
	Inernet access (WinHTTP)
*/

rstring _r_inet_getproxyconfiguration (LPCWSTR custom_proxy)
{
	rstring result;

	if (custom_proxy && custom_proxy[0])
	{
		result = custom_proxy;
	}
	else
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig = {0};
		WinHttpGetIEProxyConfigForCurrentUser (&proxyConfig);

		if (proxyConfig.lpszProxy && proxyConfig.lpszProxy[0])
			result = proxyConfig.lpszProxy;

		SAFE_GLOBAL_FREE (proxyConfig.lpszProxy);
		SAFE_GLOBAL_FREE (proxyConfig.lpszProxyBypass);
		SAFE_GLOBAL_FREE (proxyConfig.lpszAutoConfigUrl);
	}

	return result;
}

HINTERNET _r_inet_createsession (LPCWSTR useragent, rstring proxy_config)
{
	static const bool is_win81 = _r_sys_validversion (6, 3);
	const bool is_proxyset = !proxy_config.IsEmpty ();

	const HINTERNET hsession = WinHttpOpen (useragent, (is_win81 && !is_proxyset) ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return nullptr;

	// enable secure protocols
	{
		DWORD option = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &option, sizeof (option));
	}

	// enable compression feature (win81+)
	if (is_win81)
	{
		DWORD option = WINHTTP_DECOMPRESSION_FLAG_ALL;
		WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &option, sizeof (option));
	}

	return hsession;
}

bool _r_inet_openurl (HINTERNET hsession, LPCWSTR url, rstring proxy_config, HINTERNET* pconnect, HINTERNET* prequest, PDWORD ptotallength)
{
	if (!hsession)
		return false;

	WCHAR url_host[MAX_PATH] = {0};
	WCHAR url_path[MAX_PATH] = {0};
	WORD url_port = 0;
	INT url_scheme = 0;

	HINTERNET hconnect = nullptr;
	HINTERNET hrequest = nullptr;

	if (_r_inet_parseurl (url, &url_scheme, url_host, &url_port, url_path, nullptr, nullptr))
	{
		hconnect = WinHttpConnect (hsession, url_host, url_port, 0);

		if (hconnect)
		{
			DWORD flags = WINHTTP_FLAG_REFRESH;

			if (url_scheme == INTERNET_SCHEME_HTTPS)
				flags |= WINHTTP_FLAG_SECURE;

			hrequest = WinHttpOpenRequest (hconnect, nullptr, url_path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

			if (hrequest)
			{
				// disable "keep-alive" feature (win7+)
				if (_r_sys_validversion (6, 1))
				{
					DWORD option = WINHTTP_DISABLE_KEEP_ALIVE;
					WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof (option));
				}

				// set proxy configuration (if available)
				{
					WCHAR proxy_host[MAX_PATH] = {0};
					WCHAR proxy_user[MAX_PATH] = {0};
					WCHAR proxy_pass[MAX_PATH] = {0};
					WORD proxy_port = 0;

					if (_r_inet_parseurl (proxy_config, nullptr, proxy_host, &proxy_port, nullptr, proxy_user, proxy_pass))
					{
						WINHTTP_PROXY_INFO proxy_info = {0};

						WCHAR proxy_addr[MAX_PATH] = {0};

						StringCchPrintf (proxy_addr, _countof (proxy_addr), L"%s:%d", proxy_host, proxy_port);

						proxy_info.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
						proxy_info.lpszProxy = proxy_addr;

						WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY, &proxy_info, sizeof (proxy_info));

						// set proxy credentials (if exists)
						if (proxy_user[0] && proxy_pass[0])
						{
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_USERNAME, proxy_user, (DWORD)_r_str_length (proxy_user));
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_PASSWORD, proxy_pass, (DWORD)_r_str_length (proxy_pass));
						}
					}
				}

				USHORT retry_count = 5;

				while (--retry_count)
				{
					if (WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0))
					{
						if (WinHttpReceiveResponse (hrequest, nullptr))
						{
							DWORD http_code = 0;
							DWORD length = sizeof (DWORD);

							if (WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &http_code, &length, WINHTTP_NO_HEADER_INDEX))
							{
								if (http_code == HTTP_STATUS_OK)
								{
									if (ptotallength)
									{
										length = sizeof (DWORD);
										WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, ptotallength, &length, WINHTTP_NO_HEADER_INDEX);
									}

									if (pconnect)
										*pconnect = hconnect;

									if (prequest)
										*prequest = hrequest;

									return true;
								}
								else if (
									http_code == HTTP_STATUS_DENIED ||
									http_code == HTTP_STATUS_FORBIDDEN
									)
								{
									break;
								}
							}
						}
					}
					else
					{
						const DWORD err = GetLastError ();

						if (
							err == ERROR_WINHTTP_SECURE_FAILURE ||
							err == ERROR_WINHTTP_CONNECTION_ERROR
							)
						{
							// allow unknown certificates
							DWORD option = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
								SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE /*|
								SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
								SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
								*/;

							if (!WinHttpSetOption (hrequest, WINHTTP_OPTION_SECURITY_FLAGS, &option, sizeof (option)))
								break;
						}
						//else if(err == ERROR_WINHTTP_RESEND_REQUEST)
						//{
						//	continue;
						//}
						else
						{
							break;
						}
					}
				}
			}
		}
	}

	if (hconnect)
		_r_inet_close (hconnect);

	if (hrequest)
		_r_inet_close (hrequest);

	return false;
}

bool _r_inet_parseurl (LPCWSTR url, INT *scheme_ptr, LPWSTR host_ptr, WORD *port_ptr, LPWSTR path_ptr, LPWSTR user_ptr, LPWSTR pass_ptr)
{
	if (!url || !url[0] || (!scheme_ptr && !host_ptr && !port_ptr && !path_ptr && !user_ptr && !pass_ptr))
		return false;

	URL_COMPONENTS urls = {0};

	WCHAR host_part[MAX_PATH] = {0};
	WCHAR path_part[MAX_PATH] = {0};
	WCHAR user_part[MAX_PATH] = {0};
	WCHAR pass_part[MAX_PATH] = {0};

	urls.dwStructSize = sizeof (urls);

	urls.lpszHostName = host_part;
	urls.dwHostNameLength = _countof (host_part);

	urls.lpszUrlPath = path_part;
	urls.dwUrlPathLength = _countof (path_part);

	urls.lpszUserName = user_part;
	urls.dwUserNameLength = _countof (user_part);

	urls.lpszPassword = pass_part;
	urls.dwPasswordLength = _countof (pass_part);

	rstring url_copy = url;

	if (!WinHttpCrackUrl (url_copy, DWORD (url_copy.GetLength ()), ICU_DECODE, &urls))
	{
		if (GetLastError () != ERROR_WINHTTP_UNRECOGNIZED_SCHEME)
			return false;

		url_copy.Insert (0, L"https://");

		if (!WinHttpCrackUrl (url_copy, DWORD (url_copy.GetLength ()), ICU_DECODE, &urls))
			return false;
	}

	if (host_ptr)
		StringCchCopy (host_ptr, _countof (host_part), host_part);

	if (path_ptr)
		StringCchCopy (path_ptr, _countof (path_part), path_part);

	if (user_ptr)
		StringCchCopy (user_ptr, _countof (user_part), user_part);

	if (pass_ptr)
		StringCchCopy (pass_ptr, _countof (pass_part), pass_part);

	if (scheme_ptr)
		*scheme_ptr = urls.nScheme;

	if (port_ptr)
		*port_ptr = urls.nPort;

	return true;
}

bool _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, DWORD length, PDWORD preaded, PDWORD ptotalreaded)
{
	DWORD readed = 0;

	if (!WinHttpReadData (hrequest, buffer, length, &readed))
		return false;

	buffer[readed] = 0;

	if (preaded)
		*preaded = readed;

	if (ptotalreaded)
		*ptotalreaded += readed;

	if (!readed)
		return false;

	return true;
}

/*
	Other
*/

HICON _r_loadicon (HINSTANCE hinst, LPCWSTR name, INT cx_width)
{
	HICON result = nullptr;

	const HMODULE hlib = GetModuleHandle (L"comctl32.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI *LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
		const LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (hlib, "LoadIconWithScaleDown");

		if (_LoadIconWithScaleDown)
			_LoadIconWithScaleDown (hinst, name, cx_width, cx_width, &result);
	}

#ifndef _APP_NO_WINXP
	if (!result)
		result = (HICON)LoadImage (hinst, name, IMAGE_ICON, cx_width, cx_width, 0);
#endif // _APP_NO_WINXP

	return result;
}

bool _r_run (LPCWSTR filename, LPCWSTR cmdline, LPCWSTR cd, WORD sw)
{
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	bool result = false;
	si.cb = sizeof (si);

	if (sw != SW_SHOWDEFAULT)
	{
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = sw;
	}

	rstring _intptr = cmdline ? cmdline : filename;

	if (CreateProcess (filename, _intptr.GetBuffer (), nullptr, nullptr, FALSE, 0, nullptr, cd, &si, &pi))
		result = true;

	_intptr.Clear ();

	if (pi.hThread)
		CloseHandle (pi.hThread);

	if (pi.hProcess)
		CloseHandle (pi.hProcess);

	return result;
}

size_t _r_rand (size_t min_number, size_t max_number)
{
	size_t rnd_number = 0;

	const HMODULE hlib = GetModuleHandle (L"advapi32.dll");

	if (hlib)
	{
		typedef BOOLEAN (WINAPI *RGR) (PVOID, ULONG); // RtlGenRandom
		const RGR _RtlGenRandom = (RGR)GetProcAddress (hlib, "SystemFunction036"); // RtlGenRandom

		if (_RtlGenRandom)
			_RtlGenRandom (&rnd_number, sizeof (rnd_number));
	}

	if (!rnd_number)
	{
		srand ((UINT)_r_sys_gettickcount ());
		rnd_number = rand ();
	}

	return (min_number + (rnd_number % (max_number - min_number + 1)));
}

HANDLE _r_createthread (_beginthreadex_proc_type proc, void* args, bool is_suspended)
{
	const HANDLE hthread = (HANDLE)_beginthreadex (nullptr, 0, proc, args, CREATE_SUSPENDED, nullptr);

	if (hthread)
	{
		SetThreadPriority (hthread, THREAD_PRIORITY_ABOVE_NORMAL);

		if (!is_suspended)
			ResumeThread (hthread);

		return hthread;
	}

	return nullptr;
}

/*
	Control: common
*/

bool _r_ctrl_isenabled (HWND hwnd, UINT ctrl_id)
{
	return IsWindowEnabled (GetDlgItem (hwnd, ctrl_id));
}

void _r_ctrl_enable (HWND hwnd, UINT ctrl_id, bool is_enable)
{
	EnableWindow (GetDlgItem (hwnd, ctrl_id), is_enable);
}

rstring _r_ctrl_gettext (HWND hwnd, UINT ctrl_id)
{
	rstring result = L"";

	size_t length = (size_t)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);

	if (length)
	{
		length += 1;

		GetDlgItemText (hwnd, ctrl_id, result.GetBuffer (length), (INT)length);
		result.ReleaseBuffer ();
	}

	return result;
}

void _r_ctrl_settext (HWND hwnd, UINT ctrl_id, LPCWSTR str, ...)
{
	rstring buffer;

	va_list args;
	va_start (args, str);

	buffer.FormatV (str, args);
	SetDlgItemText (hwnd, ctrl_id, buffer);

	va_end (args);
}

bool _r_ctrl_settip (HWND hwnd, UINT ctrl_id, LPWSTR text)
{
	const HINSTANCE hinst = GetModuleHandle (nullptr);
	const HWND htip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, hinst, nullptr);

	if (htip)
	{
		TOOLINFO ti = {0};

		ti.cbSize = sizeof (ti);
		ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		ti.hwnd = hwnd;
		ti.hinst = hinst;
		ti.uId = (UINT_PTR)GetDlgItem (hwnd, ctrl_id);
		ti.lpszText = text;

		SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, 512);
		SendMessage (htip, TTM_ACTIVATE, TRUE, 0);
		SendMessage (htip, TTM_ADDTOOL, 0, (LPARAM)&ti);

		return true;
	}

	return false;
}

bool _r_ctrl_showtip (HWND hwnd, UINT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR text)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon_id;

	return SendDlgItemMessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt) ? true : false;
}

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, UINT ctrl_id, size_t column_id, LPCWSTR text, UINT width, INT fmt)
{
	LVCOLUMN lvc = {0};

	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

	if (width <= 100)
		width = _R_PERCENT_VAL (width, _R_RECT_WIDTH (&rc));

	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
	lvc.pszText = (LPWSTR)text;
	lvc.fmt = fmt;
	lvc.cx = width;
	lvc.iSubItem = (INT)column_id;

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

INT _r_listview_getcolumnwidth (HWND hwnd, UINT ctrl_id, INT column_id)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

	return _R_PERCENT_OF (SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0), _R_RECT_WIDTH (&rc));
}

INT _r_listview_addgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR title, UINT align, UINT state)
{
	LVGROUP lvg = {0};

	WCHAR hdr[MAX_PATH] = {0};

	lvg.cbSize = sizeof (lvg);
	lvg.mask = LVGF_GROUPID;
	lvg.iGroupId = (INT)group_id;

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = hdr;
		StringCchCopy (hdr, _countof (hdr), title);
	}

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

	if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_ISGROUPVIEWENABLED, 0, 0))
		SendDlgItemMessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, TRUE, 0);

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTGROUP, (WPARAM)group_id, (LPARAM)&lvg);
}

INT _r_listview_additem (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem, LPCWSTR text, size_t image, size_t group_id, LPARAM lparam)
{
	if (item == LAST_VALUE)
	{
		item = _r_listview_getitemcount (hwnd, ctrl_id);

		if (subitem)
			item -= 1;
	}

	WCHAR txt[MAX_PATH] = {0};

	LVITEM lvi = {0};

	lvi.iItem = (INT)item;
	lvi.iSubItem = (INT)subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = txt;

		StringCchCopy (txt, _countof (txt), text);
	}

	if (!subitem && image != LAST_VALUE)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = (INT)image;
	}

	if (!subitem && group_id != LAST_VALUE)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = (INT)group_id;
	}

	if (!subitem && lparam)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

void _r_listview_deleteallcolumns (HWND hwnd, UINT ctrl_id)
{
	const INT column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	for (INT i = column_count; i >= 0; i--)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
	}
}

void _r_listview_deleteallgroups (HWND hwnd, UINT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
	SendDlgItemMessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, FALSE, 0);
}

void _r_listview_deleteallitems (HWND hwnd, UINT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

INT _r_listview_getcolumncount (HWND hwnd, UINT ctrl_id)
{
	HWND hdr = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	return (INT)SendMessage (hdr, HDM_GETITEMCOUNT, 0, 0);
}

size_t _r_listview_getitemcount (HWND hwnd, UINT ctrl_id, bool list_checked)
{
	if (list_checked)
	{
		size_t item = LAST_VALUE;
		size_t count = 0;

		while ((item = (size_t)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETNEXTITEM, item, LVNI_ALL)) != LAST_VALUE)
		{
			if (_r_listview_isitemchecked (hwnd, ctrl_id, item))
				++count;
		}

		return count;
	}

	return (size_t)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, NULL);
}

LPARAM _r_listview_getitemlparam (HWND hwnd, UINT ctrl_id, size_t item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = (INT)item;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

rstring _r_listview_getitemtext (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem)
{
	rstring result;

	size_t length = 0;
	size_t out_length = 0;

	LVITEM lvi = {0};

	lvi.iSubItem = (INT)subitem;

	do
	{
		length += _R_BUFFER_LENGTH;

		lvi.pszText = result.GetBuffer (length);
		lvi.cchTextMax = (INT)length;

		out_length = (size_t)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMTEXT, item, (LPARAM)&lvi);
		result.ReleaseBuffer ();
	}
	while (out_length == (length - 1));

	return result;
}

bool _r_listview_isitemchecked (HWND hwnd, UINT ctrl_id, size_t item)
{
	return (((SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, item, LVIS_STATEIMAGEMASK)) >> 12) - 1) ? true : false;
}

bool _r_listview_isitemvisible (HWND hwnd, UINT ctrl_id, size_t item)
{
	return (SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, item, 0)) ? true : false;
}

void _r_listview_redraw (HWND hwnd, UINT ctrl_id)
{
	const size_t count = _r_listview_getitemcount (hwnd, ctrl_id);

	for (size_t i = 0; i < count; i++)
	{
		if (_r_listview_isitemvisible (hwnd, ctrl_id, i))
			SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, i, i);
	}
}

void _r_listview_setcolumn (HWND hwnd, UINT ctrl_id, UINT column_id, LPCWSTR text, INT width)
{
	if (!text && !width)
		return;

	LVCOLUMN lvc = {0};
	WCHAR buffer[MAX_PATH] = {0};

	if (text)
	{
		StringCchCopy (buffer, _countof (buffer), text);

		lvc.mask |= LVCF_TEXT;
		lvc.pszText = buffer;
	}

	if (width)
	{
		if (width < 0)
		{
			RECT rc = {0};
			GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

			width = _R_PERCENT_VAL (-width, _R_RECT_WIDTH (&rc));
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	if (lvc.mask)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

void _r_listview_setcolumnsortindex (HWND hwnd, UINT ctrl_id, INT column_id, INT arrow)
{
	HWND header = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (header)
	{
		HDITEM hitem = {0};

		hitem.mask = HDI_FORMAT;

		if (Header_GetItem (header, column_id, &hitem))
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

			Header_SetItem (header, column_id, &hitem);
		}
	}
}

void _r_listview_setitem (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem, LPCWSTR text, size_t image, size_t group_id, LPARAM lparam)
{
	if (!text && image == LAST_VALUE && group_id == LAST_VALUE && !lparam)
		return;

	WCHAR txt[MAX_PATH] = {0};

	LVITEM lvi = {0};

	lvi.iItem = (INT)item;
	lvi.iSubItem = (INT)subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = txt;

		StringCchCopy (txt, _countof (txt), text);
	}

	if (!lvi.iSubItem && image != LAST_VALUE)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = (INT)image;
	}

	if (!lvi.iSubItem && group_id != LAST_VALUE)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = (INT)group_id;
	}

	if (!lvi.iSubItem && lparam)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEM, 0, (LPARAM)&lvi);
}

BOOL _r_listview_setitemcheck (HWND hwnd, UINT ctrl_id, size_t item, bool state)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (state ? 2 : 1);

	return (BOOL)SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (item == LAST_VALUE) ? -1 : item, (LPARAM)&lvi);
}

INT _r_listview_setgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR title, UINT state, UINT state_mask)
{
	LVGROUP lvg = {0};
	lvg.cbSize = sizeof (lvg);

	WCHAR hdr[MAX_PATH] = {0};

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = hdr;
		StringCchCopy (hdr, _countof (hdr), title);
	}

	if (state || state_mask)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
		lvg.stateMask = state_mask;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_SETGROUPINFO, (WPARAM)group_id, (LPARAM)&lvg);
}

DWORD _r_listview_setstyle (HWND hwnd, UINT ctrl_id, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", nullptr);

	_r_wnd_top ((HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETTOOLTIPS, 0, 0), true); // listview-tooltip-HACK!!!

	return (DWORD)SendDlgItemMessage (hwnd, ctrl_id, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, UINT ctrl_id, LPCWSTR text, HTREEITEM parent, size_t image, LPARAM lparam)
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
		tvi.itemex.iImage = (INT)image;
		tvi.itemex.iSelectedImage = (INT)image;
	}

	if (lparam)
	{
		tvi.itemex.mask |= TVIF_PARAM;
		tvi.itemex.lParam = lparam;
	}

	return (HTREEITEM)SendDlgItemMessage (hwnd, ctrl_id, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getlparam (HWND hwnd, UINT ctrl_id, HTREEITEM item)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = item;

	SendDlgItemMessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

DWORD _r_treeview_setstyle (HWND hwnd, UINT ctrl_id, DWORD exstyle, INT height)
{
	if (height)
	{
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETITEMHEIGHT, (WPARAM)height, 0);
	}

	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", nullptr);

	return (DWORD)SendDlgItemMessage (hwnd, ctrl_id, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: statusbar
*/

void _r_status_settext (HWND hwnd, UINT ctrl_id, INT part, LPCWSTR text)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
}

void _r_status_setstyle (HWND hwnd, UINT ctrl_id, INT height)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETMINHEIGHT, (WPARAM)height, 0);
	SendDlgItemMessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

/*
	Control: progress bar
*/

void _r_status_pbsetmarquee (HWND hwnd, UINT ctrl_id, bool is_enable)
{
	SendDlgItemMessage (hwnd, ctrl_id, PBM_SETMARQUEE, (WPARAM)is_enable, (LPARAM)18);

	_r_wnd_addstyle (hwnd, ctrl_id, is_enable ? PBS_MARQUEE : 0, PBS_MARQUEE, GWL_STYLE);
}
