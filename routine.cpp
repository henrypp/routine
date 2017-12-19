// routine++
// Copyright (c) 2012-2018 Henry++

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

	HANDLE h = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);

	if (h != INVALID_HANDLE_VALUE)
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			DWORD written = 0;
			static const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (h, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask
		}
		else
		{
			SetFilePointer (h, 0, nullptr, FILE_END);
		}

		DWORD written = 0;

		rstring buffer;
		rstring write_buffer;

		buffer.Format (_R_DBG_FORMAT, fn, result, desc ? desc : L"<empty>");
		write_buffer.Format (L"[%s] %s [%s]\r\n", _r_fmt_date (_r_unixtime_now (), FDTF_SHORTDATE | FDTF_LONGTIME).GetString (), buffer.GetString (), appversion);

		WriteFile (h, write_buffer.GetString (), DWORD (write_buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);

		CloseHandle (h);
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

	rstring result;

	SHFormatDateTime (ft, &pflags, result.GetBuffer (256), 256);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_fmt_date (const __time64_t ut, const DWORD flags)
{
	FILETIME ft = {0};
	_r_unixtime_to_filetime (ut, &ft);

	return _r_fmt_date (&ft, flags);
}

rstring _r_fmt_size64 (ULONGLONG size)
{
	static LPCWSTR sizes[] = {L"B", L"kB", L"MB", L"GB", L"TB", L"PB", L"EB"};

	rstring result;

	INT div = 0;
	ULONGLONG rem = 0;

	while (size >= 1024 && div < _countof (sizes))
	{
		rem = (size % 1024);
		div++;
		size /= 1024;
	}

	// round up
	double size_d = double (size) + double (rem) / 1024.0;
	size_d += 0.001;

	return result.Format (L"%.2f %s", size_d, sizes[div]);
}

/*
	FastLock is a port of FastResourceLock from PH 1.x.

	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.

	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.

	https://github.com/processhacker2/processhacker
*/

ULONG _r_fastlock_islocked (P_FASTLOCK plock)
{
	const ULONG value = plock->Value;

	return value;
}

static const DWORD _r_fastlock_getspincount ()
{
	SYSTEM_INFO si = {0};
	GetNativeSystemInfo (&si);

	if (si.dwNumberOfProcessors > 1)
		return 4000;
	else
		return 0;
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

		tdc.pfCallback = &_r_msg_callback;

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
			tdc.lpCallbackData = 1; // always on top

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
		const TDI _TaskDialogIndirect = (TDI)GetProcAddress (hlib, "TaskDialogIndirect");

		if (_TaskDialogIndirect)
			return (_TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox) == S_OK);
	}

	return false;
#else
	return (TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox) == S_OK);
#endif // _APP_NO_WINXP
}

bool _r_msg_checkbox (HWND hwnd, LPCWSTR title, LPCWSTR text, LPCWSTR flag, PBOOL pis_checked)
{
#ifndef _APP_NO_WINXP
	if (_r_sys_validversion (6, 0))
	{
#endif // _APP_NO_WINXP

		INT result = 0;

		TASKDIALOGCONFIG tdc = {0};

		WCHAR str_title[64] = {0};
		WCHAR str_content[512] = {0};
		WCHAR str_flag[64] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetModuleHandle (nullptr);
		tdc.pfCallback = &_r_msg_callback;
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		tdc.pszWindowTitle = str_title;
		tdc.pszContent = str_content;
		tdc.pszVerificationText = str_flag;

		StringCchCopy (str_title, _countof (str_title), title);
		StringCchCopy (str_content, _countof (str_content), text);
		StringCchCopy (str_flag, _countof (str_flag), flag);

		if (_r_msg_taskdialog (&tdc, &result, nullptr, pis_checked))
		{
			if (result == IDYES)
				return true;
		}
#ifndef _APP_NO_WINXP
	}
	else
	{
		return _r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION, title, nullptr, L"%s", text) == IDYES;
	}
#endif // _APP_NO_WINXP

	return false;
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR lpdata)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			if (lpdata)
				_r_wnd_top (hwnd, true);

			_r_wnd_center (hwnd, GetParent (hwnd));

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, 0);
			SendMessage (hwnd, WM_SETICON, ICON_BIG, 0);

			break;
		}

		case TDN_HYPERLINK_CLICKED:
		{
			ShellExecute (hwnd, nullptr, (LPCWSTR)lparam, nullptr, nullptr, SW_SHOWDEFAULT);
			break;
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
				const size_t size = (length * sizeof (WCHAR)) + sizeof (WCHAR);

				memcpy_s (ptr, size, text, size);
				SetClipboardData (CF_UNICODETEXT, hmem);

				GlobalUnlock (hmem);
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

bool _r_fs_readfile (HANDLE h, LPVOID result, DWORD64 size)
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

			return true;
		}
	}

	return false;
}

DWORD64 _r_fs_size (HANDLE h)
{
	LARGE_INTEGER size = {0};

	GetFileSizeEx (h, &size);

	return size.QuadPart;
}

bool _r_fs_mkdir (LPCWSTR path)
{
	bool result = false;

	const HMODULE hlib = GetModuleHandle (L"shell32.dll");

	if (hlib)
	{
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

	HANDLE h = FindFirstFile (_r_fmt (L"%s\\*.*", path), &wfd);

	if (h != INVALID_HANDLE_VALUE)
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
		while (FindNextFile (h, &wfd));

		FindClose (h);
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

rstring _r_path_expand (rstring path)
{
	if (path.IsEmpty ())
		return path;

	if (path.Find (L'\\') == rstring::npos)
		return path;

	if (path.At (0) == L'\\')
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
	if (path.IsEmpty ())
		return path;

	if (path.Find (L'\\') == rstring::npos)
		return path;

	if (path.At (0) == L'\\')
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
		if (result.At (i) == L'\\')
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

	if (_wcsnicmp (path, L"\\Device\\Mup\\", device_length) == 0) // Win7
	{
		result = L"\\\\";
		result.Append (path + device_length + 1);
	}
	else if (_wcsnicmp (path, L"\\Device\\LanmanRedirector\\", device_length) == 0) // WinXP
	{
		result = L"\\\\";
		result.Append (path + device_length + 1);
	}
	else
	{
		WCHAR drives[256] = {0};

		if (GetLogicalDriveStrings (_countof (drives), drives))
		{
			LPWSTR drv = drives;

			while (drv[0])
			{
				LPWSTR drv_next = drv + wcslen (drv) + 1;

				drv[2] = 0; // the backslash is not allowed for QueryDosDevice()

				WCHAR volume[_R_BYTESIZE_KB] = {0};

				// may return multiple strings!
				// returns very weird strings for network shares
				if (QueryDosDevice (drv, volume, _countof (volume)))
				{
					if (_wcsnicmp (path, volume, device_length) == 0)
					{
						result = drv;
						result.Append (path + device_length);

						break;
					}
				}

				drv = drv_next;
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
	if (path.IsEmpty ())
		return ERROR_BAD_ARGUMENTS;

	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_NO_BUFFERING, nullptr);

	if (hfile == INVALID_HANDLE_VALUE)
	{
		return GetLastError ();
	}
	else
	{
		BYTE buffer[2048] = {0};
		DWORD required_length = 0;

		PUNICODE_STRING pk_Info = &((OBJECT_NAME_INFORMATION*)buffer)->Name;
		pk_Info->Buffer = nullptr;
		pk_Info->Length = 0;

		// IMPORTANT: The return value from NtQueryObject is bullshit! (driver bug?)
		// - The function may return STATUS_NOT_SUPPORTED although it has successfully written to the buffer.
		// - The function returns STATUS_SUCCESS although h_File == 0xFFFFFFFF
		NtQueryObject (hfile, ObjectNameInformation, buffer, sizeof (buffer), &required_length);

		if (!pk_Info->Length || !pk_Info->Buffer)
		{
			CloseHandle (hfile);
			return ERROR_FILE_NOT_FOUND;
		}
		else
		{
			pk_Info->Buffer[pk_Info->Length / sizeof (WCHAR)] = 0; // trim buffer!

			path = pk_Info->Buffer;
			path.ToLower (); // lower is imoprtant!
		}

		CloseHandle (hfile);
	}

	return ERROR_SUCCESS;
}
#endif // _APP_HAVE_NTDLL

/*
	Processes
*/

bool _r_process_getpath (HANDLE h, LPWSTR path, DWORD length)
{
	bool result = false;

	if (path)
	{
		const HMODULE hlib = GetModuleHandle (L"kernel32.dll");

		if (hlib)
		{
			const QFPIN _QueryFullProcessImageName = (QFPIN)GetProcAddress (hlib, "QueryFullProcessImageNameW");

			if (_QueryFullProcessImageName)
			{
				if (_QueryFullProcessImageName (h, 0, path, &length)) // vista and later
					result = true;
			}
		}
#ifndef _APP_NO_WINXP
		else
		{
			WCHAR buffer[_R_BYTESIZE_KB] = {0};

			if (GetProcessImageFileName (h, buffer, _countof (buffer))) // winxp
			{
				StringCchCopy (path, length, _r_path_dospathfromnt (buffer));
				result = true;
			}
		}
#endif //_APP_NO_WINXP
	}

	return result;
}

BOOL _r_process_is_exists (LPCWSTR path, const size_t len)
{
	BOOL result = FALSE;
	DWORD pid[_R_BYTESIZE_KB] = {0}, cb = 0;

	DWORD access_rights = PROCESS_QUERY_LIMITED_INFORMATION; // vista and later

#ifndef _APP_NO_WINXP
	if (!_r_sys_validversion (6, 0))
		access_rights = PROCESS_QUERY_INFORMATION; // winxp
#endif //_APP_NO_WINXP

	if (EnumProcesses (pid, sizeof (pid), &cb))
	{
		for (DWORD i = 0; i < (cb / sizeof (DWORD)); i++)
		{
			if (pid[i])
			{
				HANDLE h = OpenProcess (access_rights, FALSE, pid[i]);

				if (h)
				{
					WCHAR buffer[_R_BYTESIZE_KB] = {0};

					if (_r_process_getpath (h, buffer, _countof (buffer)))
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

WCHAR _r_str_lower (WCHAR chr)
{
	WCHAR buf[1] = {chr};
	CharLowerBuff (buf, _countof (buf));

	return buf[0];
}

WCHAR _r_str_upper (WCHAR chr)
{
	WCHAR buf[1] = {chr};
	CharUpperBuff (buf, _countof (buf));

	return buf[0];
}

size_t _r_str_hash (LPCWSTR text)
{
	if (!text)
		return 0;

	static const size_t InitialFNV = 2166136261U;
	static const size_t FNVMultiple = 16777619;

	size_t hash = InitialFNV;
	const size_t length = wcslen (text);

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

/*
	System information
*/

bool _r_sys_adminstate ()
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

	return result ? true : false;
}

LONGLONG _r_sys_gettickcount ()
{
	static LARGE_INTEGER s_frequency;
	static BOOL is_qpc = QueryPerformanceFrequency (&s_frequency);

	if (is_qpc)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter (&now);

		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}

	return GetTickCount ();
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
		const IW64P _IsWow64Process = (IW64P)GetProcAddress (hlib, "IsWow64Process");

		if (_IsWow64Process)
			_IsWow64Process (GetCurrentProcess (), &result);

		if (result)
			return true;
	}

	return false;
}
#endif // _WIN64

bool _r_sys_setprivilege (LPCWSTR privileges[], UINT count, bool is_enable)
{
	HANDLE token = nullptr;

	LUID luid = {0};
	TOKEN_PRIVILEGES tp = {0};

	bool result = false;

	if (OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
	{
		for (UINT i = 0; i < count; i++)
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

__time64_t _r_unixtime_now ()
{
	SYSTEMTIME st = {0};
	GetSystemTime (&st);

	return _r_unixtime_from_systemtime (&st);
}

void _r_unixtime_to_filetime (__time64_t ut, const LPFILETIME pft)
{
	if (ut && pft)
	{
		__time64_t ll = ut * 10000000ULL + 116444736000000000; // 64-bit value

		pft->dwLowDateTime = (DWORD)ll;
		pft->dwHighDateTime = ll >> 32;
	}
}

void _r_unixtime_to_systemtime (__time64_t ut, const LPSYSTEMTIME pst)
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

void _r_dc_fillrect (HDC dc, LPRECT rc, COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (dc, clr);
	ExtTextOut (dc, 0, 0, ETO_OPAQUE, rc, nullptr, 0, nullptr);
	SetBkColor (dc, clr_prev);
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

void _r_wnd_addstyle (HWND hwnd, UINT ctrl_id, LONG mask, LONG stateMask, INT index)
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
			const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (hlib, "ChangeWindowMessageFilterEx"); // win7 and later

			if (_ChangeWindowMessageFilterEx)
			{
				_ChangeWindowMessageFilterEx (hwnd, msg, action, nullptr);
			}
			else
			{
				const CWMF _ChangeWindowMessageFilter = (CWMF)GetProcAddress (hlib, "ChangeWindowMessageFilter"); // vista

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

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent)
{
	HINTERNET hsession = nullptr;

	DWORD flags = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;

	const bool is_win81 = _r_sys_validversion (6, 3);

	if (is_win81)
		flags = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;

	hsession = WinHttpOpen (useragent, flags, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return nullptr;

	// enable compression feature (win81 and above)
	if (is_win81)
	{
		DWORD option = WINHTTP_DECOMPRESSION_FLAG_ALL;
		WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &option, sizeof (option));
	}

	return hsession;
}

bool _r_inet_openurl (HINTERNET hsession, LPCWSTR url, HINTERNET* pconnect, HINTERNET* prequest, PDWORD ptotallength)
{
	if (!hsession)
		return false;

	URL_COMPONENTS urlcomp = {0};

	WCHAR host[MAX_PATH] = {0};
	WCHAR url_path[MAX_PATH] = {0};

	urlcomp.dwStructSize = sizeof (urlcomp);

	urlcomp.lpszHostName = host;
	urlcomp.dwHostNameLength = _countof (host);

	urlcomp.lpszUrlPath = url_path;
	urlcomp.dwUrlPathLength = _countof (url_path);

	HINTERNET hconnect = nullptr;
	HINTERNET hrequest = nullptr;

	if (WinHttpCrackUrl (url, DWORD (wcslen (url)), ICU_ESCAPE, &urlcomp))
	{
		hconnect = WinHttpConnect (hsession, host, urlcomp.nPort, 0);

		if (hconnect)
		{
			DWORD flags = WINHTTP_FLAG_REFRESH;

			if (urlcomp.nScheme == INTERNET_SCHEME_HTTPS)
				flags |= WINHTTP_FLAG_SECURE;

			hrequest = WinHttpOpenRequest (hconnect, nullptr, url_path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

			if (hrequest)
			{
				// disable "keep-alive" feature (win7 and above)
				if (_r_sys_validversion (6, 1))
				{
					DWORD option = WINHTTP_DISABLE_KEEP_ALIVE;
					WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof (option));
				}

				UINT retry_count = 0;

				do
				{
					if (WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0))
					{
						if (WinHttpReceiveResponse (hrequest, nullptr))
						{
							if (ptotallength)
							{
								DWORD length = sizeof (DWORD);
								WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr, ptotallength, &length, nullptr);
							}

							if (pconnect)
								*pconnect = hconnect;

							if (prequest)
								*prequest = hrequest;

							return true;
						}
					}
					else
					{
						const DWORD err = GetLastError ();

						if (err == ERROR_WINHTTP_CONNECTION_ERROR || err == ERROR_WINHTTP_SECURE_FAILURE)
						{
							DWORD old_option = 0;
							DWORD old_length = 0;
							DWORD new_option = 0;
							DWORD flag = 0;
							HINTERNET hinet = hrequest;

							if (err == ERROR_WINHTTP_CONNECTION_ERROR)
							{
								// allow tls 1.2 secure protocol
								flag = WINHTTP_OPTION_SECURE_PROTOCOLS;
								new_option = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
								hinet = hsession;
							}
							else if (err == ERROR_WINHTTP_SECURE_FAILURE)
							{
								// allow unknown certificates
								flag = WINHTTP_OPTION_SECURITY_FLAGS;
								new_option = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
							}

							if (new_option)
							{
								if (WinHttpQueryOption (hinet, flag, &old_option, &old_length))
									new_option |= old_option;

								if (new_option != old_option)
								{
									if (!WinHttpSetOption (hinet, flag, &new_option, sizeof (new_option)))
										break;
								}
								else
								{
									break;
								}
							}
						}
						else
						{
							break;
						}
					}
				}
				while (retry_count++ < 3);
			}
		}
	}

	if (hconnect)
		_r_inet_close (hconnect);

	if (hrequest)
		_r_inet_close (hrequest);

	return false;
}

bool _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, DWORD length, PDWORD ptotallength)
{
	DWORD written = 0;

	if (!WinHttpReadData (hrequest, buffer, length, &written))
		return false;

	if (!written)
		return false;

	buffer[written] = 0;

	if (ptotallength)
		*ptotallength += written;

	return true;
}

void _r_inet_close (HINTERNET hinet)
{
	WinHttpCloseHandle (hinet);
}

/*
	Other
*/

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT d)
{
	HICON result = nullptr;

	const HMODULE hlib = GetModuleHandle (L"comctl32.dll");

	if (hlib)
	{
		const LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (hlib, "LoadIconWithScaleDown");

		if (_LoadIconWithScaleDown)
			_LoadIconWithScaleDown (h, name, d, d, &result);
	}

#ifndef _APP_NO_WINXP
	if (!result)
		result = (HICON)LoadImage (h, name, IMAGE_ICON, d, d, 0);
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

size_t _r_rand (size_t start, size_t end)
{
	srand ((unsigned int)_r_sys_gettickcount ());

	return rand () % end + start;
}

/*
	Control: common
*/

void _r_ctrl_enable (HWND hwnd, UINT ctrl_id, bool is_enable)
{
	EnableWindow (GetDlgItem (hwnd, ctrl_id), is_enable);
}

rstring _r_ctrl_gettext (HWND hwnd, UINT ctrl_id)
{
	rstring result = L"";

	INT length = (INT)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);

	if (length)
	{
		length += 1;

		GetDlgItemText (hwnd, ctrl_id, result.GetBuffer (length), length);
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
	const HWND htip = CreateWindowEx (0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, nullptr, hinst, nullptr);

	if (htip)
	{
		TOOLINFO ti = {0};

		ti.cbSize = sizeof (ti);
		ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		ti.hwnd = hwnd;
		ti.hinst = hinst;
		ti.uId = (UINT_PTR)GetDlgItem (hwnd, ctrl_id);
		ti.lpszText = text;

		SendMessage (htip, TTM_ACTIVATE, TRUE, 0);
		SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, _R_BUFFER_LENGTH);
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

	if (width > 100)
		width = _R_PERCENT_OF (width, rc.right);

	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
	lvc.pszText = (LPWSTR)text;
	lvc.fmt = fmt;
	lvc.cx = _R_PERCENT_VAL (width, rc.right);
	lvc.iSubItem = (INT)column_id;

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

INT _r_listview_getcolumnwidth (HWND hwnd, UINT ctrl_id, INT column_id)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

	return _R_PERCENT_OF (SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, column_id, NULL), rc.right);
}

INT _r_listview_addgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR text, UINT align, UINT state)
{
	LVGROUP lvg = {0};

	WCHAR hdr[MAX_PATH] = {0};

	lvg.cbSize = sizeof (lvg);
	lvg.mask = LVGF_GROUPID;
	lvg.iGroupId = (INT)group_id;

	if (text)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = hdr;
		StringCchCopy (hdr, _countof (hdr), text);
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
		item = (INT)_r_listview_getitemcount (hwnd, ctrl_id);

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
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, i, 0);
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
		INT item = -1;
		size_t count = 0;

		while ((item = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETNEXTITEM, item, LVNI_ALL)) != -1)
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
	LVCOLUMN lvc = {0};

	if (text)
	{
		WCHAR buffer[MAX_PATH] = {0};
		StringCchCopy (buffer, _countof (buffer), text);

		lvc.mask |= LVCF_TEXT;
		lvc.pszText = buffer;
	}

	if (width)
	{
		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	if (lvc.mask)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, column_id, (LPARAM)&lvc);
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

INT _r_listview_setitem (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem, LPCWSTR text, size_t image, size_t group_id, LPARAM lparam)
{
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

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEM, 0, (LPARAM)&lvi);
}

BOOL _r_listview_setitemcheck (HWND hwnd, UINT ctrl_id, size_t item, bool state)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (state ? 2 : 1);

	return (BOOL)SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (item == LAST_VALUE) ? -1 : item, (LPARAM)&lvi);
}

INT _r_listview_setgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR title)
{
	LVGROUP lvg = {0};

	WCHAR hdr[MAX_PATH] = {0};

	lvg.cbSize = sizeof (lvg);

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = hdr;
		StringCchCopy (hdr, _countof (hdr), title);
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
