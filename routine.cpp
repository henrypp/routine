// routine++
// Copyright (c) 2012-2019 Henry++

#include "routine.hpp"

/*
	Debugging
*/

void _r_dbg (LPCWSTR fn, DWORD errcode, LPCWSTR desc)
{
	static const rstring path = _r_dbg_getpath ();

	rstring write_buffer;
	write_buffer.Format (L"\"%s\"," _R_DEBUG_BODY L",\"%s\"\r\n", _r_fmt_date (_r_unixtime_now (), FDTF_SHORTDATE | FDTF_LONGTIME).GetString (), fn, errcode, desc, APP_VERSION);

	_r_dbg_print (L"%s", write_buffer.GetString ());

	_r_dbg_write (path, write_buffer);
}

void _r_dbg_print (LPCWSTR text, ...)
{
	va_list args;
	va_start (args, text);

	rstring buffer;
	buffer.FormatV (text, args);

	va_end (args);

	OutputDebugString (buffer);
}

void _r_dbg_write (LPCWSTR path, LPCWSTR text)
{
	SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL); // HACK!!!
	HANDLE hfile = CreateFile (path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			DWORD written = 0;
			static const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (hfile, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask

			WriteFile (hfile, _R_DEBUG_HEADER, DWORD (_r_str_length (_R_DEBUG_HEADER) * sizeof (WCHAR)), &written, nullptr); // adds csv header
		}
		else
		{
			_r_fs_setpos (hfile, 0, FILE_END);
		}

		DWORD written = 0;

		WriteFile (hfile, text, DWORD (_r_str_length (text) * sizeof (WCHAR)), &written, nullptr);

		SAFE_DELETE_HANDLE (hfile);
	}
}

rstring _r_dbg_getpath ()
{
	WCHAR result[MAX_PATH] = {0};

	if (!_r_sys_iselevated ())
	{
		GetTempPath (_countof (result), result);
	}
	else
	{
		GetModuleFileName (GetModuleHandle (nullptr), result, _countof (result));
		PathRemoveFileSpec (result);
	}

	_r_str_cat (result, _countof (result), L"\\");
#ifdef APP_NAME_SHORT
	_r_str_cat (result, _countof (result), APP_NAME_SHORT);
#else
	_r_str_cat (result, _countof (result), L"unnamed");
#pragma _R_WARNING(APP_NAME_SHORT)
#endif
	_r_str_cat (result, _countof (result), L"_debug.log");

	return result;
}

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR text, ...)
{
	rstring result;

	va_list args;
	va_start (args, text);

	result.FormatV (text, args);

	va_end (args);

	return result;
}

rstring _r_fmt_date (const LPFILETIME ft, DWORD flags)
{
	DWORD pflags = flags;
	WCHAR buffer[128] = {0};

	SHFormatDateTime (ft, &pflags, buffer, _countof (buffer));

	return buffer;
}

rstring _r_fmt_date (time_t ut, DWORD flags)
{
	FILETIME ft = {0};
	_r_unixtime_to_filetime (ut, &ft);

	return _r_fmt_date (&ft, flags);
}

rstring _r_fmt_size64 (ULONG64 bytes)
{
	WCHAR buffer[128] = {0};

#ifdef _APP_NO_WINXP
	if (SUCCEEDED (StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, _countof (buffer)))) // vista (sp1)+
		return buffer;
#else
	const HMODULE hlib = GetModuleHandle (L"shlwapi.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI * SFBSE) (ULONG64, SFBS_FLAGS, PWSTR, UINT); // StrFormatByteSizeEx
		const SFBSE _StrFormatByteSizeEx = (SFBSE)GetProcAddress (hlib, "StrFormatByteSizeEx");

		if (_StrFormatByteSizeEx)
		{
			if (SUCCEEDED (_StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, _countof (buffer)))) // vista (sp1)+
				return buffer;
		}
	}

	if (StrFormatByteSizeW ((LONG64)bytes, buffer, _countof (buffer))) // fallback
		return buffer;
#endif // _APP_NO_WINXP

	return nullptr;
}

rstring _r_fmt_interval (time_t seconds, INT digits)
{
	WCHAR buffer[128] = {0};
	StrFromTimeInterval (buffer, _countof (buffer), DWORD (seconds) * 1000UL, digits);

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

#ifndef _APP_NO_WINXP
void _r_fastlock_initialize (P_FASTLOCK plock)
{
	plock->Value = 0;

	NtCreateSemaphore (&plock->ExclusiveWakeEvent, SEMAPHORE_ALL_ACCESS, nullptr, 0, MAXLONG);
	NtCreateSemaphore (&plock->SharedWakeEvent, SEMAPHORE_ALL_ACCESS, nullptr, 0, MAXLONG);
}

void _r_fastlock_acquireexclusive (P_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	static const DWORD spinCount = _r_fastlock_getspincount ();

	while (true)
	{
		value = plock->Value;

		if (!(value & (_R_FASTLOCK_OWNED | _R_FASTLOCK_EXCLUSIVE_WAKING)))
		{
			if (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED, value) == value)
				break;
		}
		else if (i >= spinCount)
		{
			_r_fastlock_ensureeventcreated (&plock->ExclusiveWakeEvent);

			if (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->ExclusiveWakeEvent, INFINITE, FALSE) != STATUS_WAIT_0)
					RtlRaiseStatus (STATUS_UNSUCCESSFUL);

				do
				{
					value = plock->Value;
				}
				while (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED - _R_FASTLOCK_EXCLUSIVE_WAKING, value) != value);

				break;
			}
		}

		i += 1;
		YieldProcessor ();
	}
}

void _r_fastlock_acquireshared (P_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	static const DWORD spinCount = _r_fastlock_getspincount ();

	while (true)
	{
		value = plock->Value;

		if (!(value & (_R_FASTLOCK_OWNED | (_R_FASTLOCK_SHARED_OWNERS_MASK << _R_FASTLOCK_SHARED_OWNERS_SHIFT) | _R_FASTLOCK_EXCLUSIVE_MASK)))
		{
			if (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if ((value & _R_FASTLOCK_OWNED) && ((value >> _R_FASTLOCK_SHARED_OWNERS_SHIFT) & _R_FASTLOCK_SHARED_OWNERS_MASK) > 0 && !(value & _R_FASTLOCK_EXCLUSIVE_MASK))
		{
			if (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if (i >= spinCount)
		{
			_r_fastlock_ensureeventcreated (&plock->SharedWakeEvent);

			if (InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->SharedWakeEvent, INFINITE, FALSE) != STATUS_WAIT_0)
					RtlRaiseStatus (STATUS_UNSUCCESSFUL);

				continue;
			}
		}

		i += 1;
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
			if (InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED + _R_FASTLOCK_EXCLUSIVE_WAKING - _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				NtReleaseSemaphore (plock->ExclusiveWakeEvent, 1, nullptr);
				break;
			}
		}
		else
		{
			const ULONG sharedWaiters = (value >> _R_FASTLOCK_SHARED_WAITERS_SHIFT) & _R_FASTLOCK_SHARED_WAITERS_MASK;

			if (InterlockedCompareExchange (&plock->Value, value & ~(_R_FASTLOCK_OWNED | (_R_FASTLOCK_SHARED_WAITERS_MASK << _R_FASTLOCK_SHARED_WAITERS_SHIFT)), value) == value)
			{
				if (sharedWaiters)
					NtReleaseSemaphore (plock->SharedWakeEvent, sharedWaiters, nullptr);

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
			if (InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if ((value >> _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT) & _R_FASTLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED + _R_FASTLOCK_EXCLUSIVE_WAKING - _R_FASTLOCK_SHARED_OWNERS_INC - _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				NtReleaseSemaphore (plock->ExclusiveWakeEvent, 1, nullptr);
				break;
			}
		}
		else
		{
			if (InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED - _R_FASTLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}

		YieldProcessor ();
	}
}

bool _r_fastlock_tryacquireexclusive (P_FASTLOCK plock)
{
	const ULONG value = plock->Value;

	if (value & (_R_FASTLOCK_OWNED | _R_FASTLOCK_EXCLUSIVE_WAKING))
		return false;

	return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED, value) == value;
}

bool _r_fastlock_tryacquireshared (P_FASTLOCK plock)
{
	const ULONG value = plock->Value;

	if (value & _R_FASTLOCK_EXCLUSIVE_MASK)
		return false;

	if (!(value & _R_FASTLOCK_OWNED))
	{
		return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value;
	}
	else if ((value >> _R_FASTLOCK_SHARED_OWNERS_SHIFT) & _R_FASTLOCK_SHARED_OWNERS_MASK)
	{
		return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value;
	}

	return false;
}
#endif // _APP_NO_WINXP

/*
	Objects reference
*/

PR_OBJECT _r_obj_allocate (PVOID pdata, _R_CALLBACK_OBJECT_CLEANUP cleanup_callback)
{
	if (!cleanup_callback)
		return nullptr;

	PR_OBJECT pobj = new R_OBJECT;

	InterlockedIncrement (&pobj->ref_count);

	pobj->pdata = pdata;
	pobj->cleanup_callback = cleanup_callback;

	return pobj;
}

PR_OBJECT _r_obj_reference (PR_OBJECT pobj)
{
	if (!pobj)
		return nullptr;

	InterlockedIncrement (&pobj->ref_count);

	return pobj;
}

void _r_obj_dereference (PR_OBJECT pobj)
{
	_r_obj_dereferenceex (pobj, 1);
}

void _r_obj_dereferenceex (PR_OBJECT pobj, LONG ref_count)
{
	if (!pobj)
		return;

	assert (!(ref_count < 0));

	const LONG old_count = InterlockedExchangeAdd (&pobj->ref_count, -ref_count);
	const LONG new_count = old_count - ref_count;

	if (new_count == 0)
	{
		if (pobj->pdata)
		{
			if (pobj->cleanup_callback)
				pobj->cleanup_callback (pobj->pdata);

			pobj->pdata = nullptr;
		}

		SAFE_DELETE (pobj);
	}
	else if (new_count < 0)
	{
		RtlRaiseStatus (STATUS_INVALID_PARAMETER);
	}
}

/*
	System messages
*/

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR text, ...)
{
	rstring buffer;

	INT result = 0;

	if (text)
	{
		va_list args;
		va_start (args, text);

		buffer.FormatV (text, args);

		va_end (args);
	}

#ifndef _APP_NO_WINXP
	if (_r_sys_validversion (6, 0))
	{
#endif // _APP_NO_WINXP
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
			tdc.lpCallbackData = MAKELONG (0, TRUE);

		if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
			return result;
#ifndef _APP_NO_WINXP
	}
#endif // _APP_NO_WINXP

#ifndef _APP_NO_WINXP
	if (!result)
	{
		MSGBOXPARAMS mbp = {0};

		if (main)
		{
			if (buffer.IsEmpty ())
				buffer = main;

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

		return MessageBoxIndirect (&mbp);
	}
#endif // _APP_NO_WINXP

	return 0;
}

bool _r_msg_taskdialog (const TASKDIALOGCONFIG * ptd, INT * pbutton, INT * pradiobutton, BOOL * pcheckbox)
{
#ifdef _APP_NO_WINXP
	return SUCCEEDED (TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox));
#else
	const HMODULE hlib = GetModuleHandle (L"comctl32.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI * TDI) (const TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect
		const TDI _TaskDialogIndirect = (TDI)GetProcAddress (hlib, "TaskDialogIndirect");

		if (_TaskDialogIndirect)
			return SUCCEEDED (_TaskDialogIndirect (ptd, pbutton, pradiobutton, pcheckbox));
	}

	return false;
#endif // _APP_NO_WINXP
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM, LPARAM lparam, LONG_PTR lpdata)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			const BOOL is_topmost = HIWORD (lpdata);

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
			const BOOL is_donotdrawicon = LOWORD (lpdata);

			if (!is_donotdrawicon)
			{
				HWND hparent = GetParent (hwnd);

				if (hparent)
				{
					SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)SendMessage (hparent, WM_GETICON, ICON_SMALL, 0));
					SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)SendMessage (hparent, WM_GETICON, ICON_BIG, 0));
				}
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
	if (OpenClipboard (hwnd))
	{
		rstring result;
		HGLOBAL hmemory = GetClipboardData (CF_UNICODETEXT);

		if (hmemory)
		{
			LPCWSTR text = (LPCWSTR)GlobalLock (hmemory);
			const size_t length = GlobalSize (hmemory);

			if (text && length)
				result = rstring (text, length);

			GlobalUnlock (hmemory);
		}

		CloseClipboard ();

		return result;
	}

	return nullptr;
}

void _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length)
{
	if (OpenClipboard (hwnd))
	{
		if (EmptyClipboard ())
		{
			if (!_r_str_isempty (text) && length)
			{
				size_t byte_size = length * sizeof (WCHAR);
				HGLOBAL hmemory = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, byte_size + sizeof (WCHAR));

				if (hmemory)
				{
					LPVOID ptr_lock = GlobalLock (hmemory);

					memcpy (ptr_lock, text, byte_size);
					*(PWCHAR)PTR_ADD_OFFSET (ptr_lock, byte_size) = UNICODE_NULL; // terminate

					GlobalUnlock (ptr_lock);

					SetClipboardData (CF_UNICODETEXT, hmemory);
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
	if (_r_str_isempty (path))
		return false;

	if (allowundo)
	{
		SHFILEOPSTRUCT op = {0};

		op.wFunc = FO_DELETE;
		op.pFrom = path;
		op.fFlags = FOF_NO_UI | FOF_ALLOWUNDO;

		if (SHFileOperation (&op) == ERROR_SUCCESS)
			return true;
	}

	return !!DeleteFile (path);
}

bool _r_fs_makebackup (LPCWSTR path, time_t timestamp)
{
	if (_r_str_isempty (path) || !_r_fs_exists (path))
		return false;

	rstring dest_path;

	if (timestamp)
	{
		SYSTEMTIME st = {0};

		_r_unixtime_to_systemtime (timestamp, &st);
		SystemTimeToTzSpecificLocalTime (nullptr, &st, &st);

		WCHAR date_format[128] = {0};

		GetDateFormat (LOCALE_USER_DEFAULT, 0, &st, L"yyyy-MM-dd", date_format, _countof (date_format));

		dest_path.Format (L"%s\\%s-%s.bak", _r_path_getdirectory (path).GetString (), date_format, _r_path_getfilename (path));

		if (_r_fs_exists (dest_path))
			_r_path_makeunique (dest_path);

		return _r_fs_move (path, dest_path, 0);
	}

	dest_path.Format (L"%s.bak", path);

	if (_r_fs_exists (dest_path))
		_r_path_makeunique (dest_path);

	return _r_fs_move (path, dest_path, 0);
}

bool _r_fs_mkdir (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return false;

	if (SHCreateDirectoryEx (nullptr, path, nullptr) == ERROR_SUCCESS)
		return true;

	return !!CreateDirectory (path, nullptr); // fallback
}

bool _r_fs_readfile (HANDLE hfile, LPVOID result, DWORD64 size)
{
	if (!hfile || hfile == INVALID_HANDLE_VALUE)
		return false;

	HANDLE hmap = CreateFileMapping (hfile, nullptr, PAGE_READONLY, 0, 0, nullptr);

	if (hmap)
	{
		LPVOID pbuffer = MapViewOfFile (hmap, FILE_MAP_READ, 0, 0, 0);

		if (pbuffer)
		{
			RtlCopyMemory (result, pbuffer, (size_t)size);

			UnmapViewOfFile (pbuffer);
			SAFE_DELETE_HANDLE (hmap);

			return true;
		}

		SAFE_DELETE_HANDLE (hmap);
	}

	return false;
}

void _r_fs_rmdir (LPCWSTR path, bool is_recurse)
{
	if (_r_str_isempty (path))
		return;

	if (is_recurse)
	{
		WIN32_FIND_DATA wfd = {0};

		HANDLE hfind = FindFirstFile (_r_fmt (L"%s\\*.*", path), &wfd);

		if (hfind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (wfd.cFileName[0] == UNICODE_NULL || (wfd.cFileName[0] == L'.' && wfd.cFileName[1] == UNICODE_NULL) || (wfd.cFileName[0] == L'.' && wfd.cFileName[1] == L'.' && wfd.cFileName[2] == UNICODE_NULL))
					continue;

				rstring full_path;
				full_path.Format (L"%s\\%s", path, wfd.cFileName);

				if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
				{
					_r_fs_rmdir (full_path, true);
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
	}

	RemoveDirectory (path);
}

bool _r_fs_setpos (HANDLE hfile, LONG64 pos, DWORD method)
{
	LARGE_INTEGER lpos = {0};

	lpos.QuadPart = pos;

	return !!SetFilePointerEx (hfile, lpos, nullptr, method);
}

LONG64 _r_fs_size (HANDLE hfile)
{
	LARGE_INTEGER size = {0};

	GetFileSizeEx (hfile, &size);

	return size.QuadPart;
}

LONG64 _r_fs_size (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return 0;

	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		const LONG64 result = _r_fs_size (hfile);

		SAFE_DELETE_HANDLE (hfile);
		return result;
	}

	return 0;
}

/*
	Paths
*/

rstring _r_path_getdirectory (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return path;

	rstring result = path;

	const size_t pos = _r_str_findlast (result, result.GetLength (), OBJ_NAME_PATH_SEPARATOR);

	if (pos != INVALID_SIZE_T)
	{
		result.SetLength (pos);
		_r_str_trim (result, L"\\/");
	}

	return result;
}

LPCWSTR _r_path_getextension (LPCWSTR path)
{
	LPCWSTR lastpoint = nullptr;

	while (*path != UNICODE_NULL)
	{
		if (*path == OBJ_NAME_PATH_SEPARATOR || *path == L' ')
			lastpoint = nullptr;

		else if (*path == L'.')
			lastpoint = path;

		path++;
	}

	return lastpoint;
}

LPCWSTR _r_path_getfilename (LPCWSTR path)
{
	LPCWSTR last_slash = path;

	while (*path != UNICODE_NULL)
	{
		if ((*path == OBJ_NAME_PATH_SEPARATOR || *path == L'/' || *path == L':') && path[1] && path[1] != OBJ_NAME_PATH_SEPARATOR && path[1] != L'/')
			last_slash = path + 1;

		path++;
	}

	return last_slash;
}

void _r_path_explore (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return;

	if (_r_fs_exists (path))
	{
		_r_run (nullptr, _r_fmt (L"\"explorer.exe\" /select,\"%s\"", path));
	}
	else
	{
		rstring dir = _r_path_getdirectory (path);

		if (_r_fs_exists (dir))
			ShellExecute (nullptr, nullptr, dir, nullptr, nullptr, SW_SHOWDEFAULT);
	}
}

rstring _r_path_compact (LPCWSTR path, UINT length)
{
	if (_r_str_isempty (path))
		return path;

	if (!length)
		return nullptr;

	rstring result;

	PathCompactPathEx (result.GetBuffer (length), path, length, 0);
	result.ReleaseBuffer ();

	return result;
}

rstring _r_path_expand (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return nullptr;

	const size_t length = _r_str_length (path);
	const size_t percent_pos = _r_str_find (path, length, L'%');
	const size_t separator_pos = _r_str_find (path, length, OBJ_NAME_PATH_SEPARATOR);

	if (percent_pos == INVALID_SIZE_T && separator_pos == INVALID_SIZE_T)
		return path;

	rstring result;

	if (percent_pos != INVALID_SIZE_T)
	{
		if (!ExpandEnvironmentStrings (path, result.GetBuffer (1024), 1024))
		{
			result.Release ();
			return path;
		}
		else
		{
			result.ReleaseBuffer ();
		}
	}
	else
	{
		if (!PathSearchAndQualify (path, result.GetBuffer (1024), 1024))
		{
			result.Release ();
			return path;
		}
		else
		{
			result.ReleaseBuffer ();
		}
	}

	return result;
}

rstring _r_path_unexpand (LPCWSTR path)
{
	if (_r_str_isempty (path))
		return nullptr;

	if (_r_str_find (path, INVALID_SIZE_T, OBJ_NAME_PATH_SEPARATOR) == INVALID_SIZE_T)
		return path;

	rstring result;

	if (!PathUnExpandEnvStrings (path, result.GetBuffer (1024), 1024))
	{
		result.Release ();
		return path;
	}
	else
	{
		result.ReleaseBuffer ();
	}

	return result;
}

rstring _r_path_makeunique (LPCWSTR path)
{
	if (_r_str_isempty (path) || !_r_fs_exists (path))
		return path;

	if (_r_str_find (path, INVALID_SIZE_T, OBJ_NAME_PATH_SEPARATOR) == INVALID_SIZE_T)
		return path;

	rstring directory = _r_path_getdirectory (path);
	rstring filename = _r_path_getfilename (path);
	rstring extension = _r_path_getextension (path);

	if (!extension.IsEmpty ())
		filename.SetLength (filename.GetLength () - extension.GetLength ());

	rstring result;

	for (USHORT i = 1; i < USHRT_MAX; i++)
	{
		result.Format (L"%s\\%s-%" PRIu16 L"%s", directory.GetString (), filename.GetString (), i, extension.GetString ());

		if (!_r_fs_exists (result))
			return result;
	}

	return path;
}

rstring _r_path_dospathfromnt (LPCWSTR path)
{
	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_r_str_compare (path, L"\\??\\", 4) == 0)
	{
		return path + 4;
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_r_str_compare (path, L"\\SystemRoot", 11) == 0)
	{
		WCHAR systemRoot[MAX_PATH] = {0};
		GetSystemDirectory (systemRoot, _countof (systemRoot));

		return _r_fmt (L"%s\\%s", systemRoot, path + 11 + 9);
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_r_str_compare (path, L"system32\\", 9) == 0)
	{
		WCHAR systemRoot[MAX_PATH] = {0};
		GetSystemDirectory (systemRoot, _countof (systemRoot));

		return _r_fmt (L"%s\\%s", systemRoot, path + 8);
	}
	else if (_r_str_compare (path, L"\\device\\", 8) == 0)
	{
		const size_t pathLen = _r_str_length (path);

		if (_r_str_compare (path, L"\\device\\mup", 11) == 0)
		{
			if (pathLen != 11 && path[11] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \path
				return (path + 11);
			}
		}

		// device name prefixes
#ifndef _WIN64
		PROCESS_DEVICEMAP_INFORMATION deviceMapInfo;
#else
		PROCESS_DEVICEMAP_INFORMATION_EX deviceMapInfo;
#endif

		RtlSecureZeroMemory (&deviceMapInfo, sizeof (deviceMapInfo));

		UNICODE_STRING devicePrefix;

		WCHAR deviceNameBuffer[7] = L"\\??\\ :";
		static WCHAR devicePrefixBuff[_R_DEVICE_PREFIX_LENGTH] = {0};

		if (NT_SUCCESS (NtQueryInformationProcess (NtCurrentProcess (), ProcessDeviceMap, &deviceMapInfo, sizeof (deviceMapInfo), nullptr)))
		{
			for (DWORD i = 0; i < _R_DEVICE_COUNT; i++)
			{
				if (deviceMapInfo.Query.DriveMap)
				{
					if (!(deviceMapInfo.Query.DriveMap & (0x1 << i)))
						continue;
				}

				OBJECT_ATTRIBUTES oa;
				UNICODE_STRING deviceName;

				deviceName.Buffer = deviceNameBuffer;
				deviceName.Length = 6 * sizeof (WCHAR);

				deviceNameBuffer[4] = L'A' + WCHAR (i);

				InitializeObjectAttributes (&oa, &deviceName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);

				HANDLE hslink = nullptr;

				if (NT_SUCCESS (NtOpenSymbolicLinkObject (&hslink, SYMBOLIC_LINK_QUERY, &oa)))
				{
					devicePrefix.Length = 0;
					devicePrefix.Buffer = devicePrefixBuff;
					devicePrefix.MaximumLength = _countof (devicePrefixBuff);

					if (NT_SUCCESS (NtQuerySymbolicLinkObject (hslink, &devicePrefix, nullptr)))
					{
						const size_t prefixLen = devicePrefix.Length / sizeof (WCHAR);

						if (prefixLen)
						{
							if (_r_str_compare (devicePrefix.Buffer, path, prefixLen) == 0)
							{
								// To ensure we match the longest prefix, make sure the next character is a
								// backslash or the path is equal to the prefix.
								if (pathLen == prefixLen || path[prefixLen] == OBJ_NAME_PATH_SEPARATOR)
								{
									NtClose (hslink);

									// <letter>:path
									return _r_fmt (L"%c:\\%s", L'A' + WCHAR (i), path + prefixLen + 1);
								}
							}
						}
					}

					NtClose (hslink);
				}
			}
		}

		// network share prefixes
		// https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/support-for-unc-naming-and-mup
		HKEY hkey = nullptr;
		rstring providerOrder;

		if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			providerOrder = _r_reg_querystring (hkey, L"ProviderOrder");
			RegCloseKey (hkey);
		}

		if (!providerOrder.IsEmpty ())
		{
			rstringvec rvc;
			_r_str_split (providerOrder, providerOrder.GetLength (), L',', rvc);

			for (size_t i = 0; i < rvc.size (); i++)
			{
				rstring& rlink = rvc.at (i);

				if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, _r_fmt (L"System\\CurrentControlSet\\Services\\%s\\NetworkProvider", rlink.GetString ()), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					rstring deviceName = _r_reg_querystring (hkey, L"DeviceName");

					const size_t prefixLen = deviceName.GetLength ();

					if (prefixLen)
					{
						if (_r_str_compare (deviceName.GetString (), path, prefixLen) == 0)
						{
							// To ensure we match the longest prefix, make sure the next character is a
							// backslash. Don't resolve if the name *is* the prefix. Otherwise, we will end
							// up with a useless string like "\".
							if (pathLen != prefixLen && path[prefixLen] == OBJ_NAME_PATH_SEPARATOR)
							{
								RegCloseKey (hkey);

								// \path
								return (path + prefixLen);
							}
						}
					}

					RegCloseKey (hkey);
				}
			}
		}
	}

	return path;
}

DWORD _r_path_ntpathfromdos (rstring& path)
{
	if (path.IsEmpty ())
		return STATUS_UNSUCCESSFUL;

	NTSTATUS status;

	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (hfile == INVALID_HANDLE_VALUE)
	{
		return GetLastError ();
	}
	else
	{
		DWORD attempts = 6;
		DWORD bufferSize = 0x200;

		POBJECT_NAME_INFORMATION pbuffer = (POBJECT_NAME_INFORMATION)new BYTE[bufferSize];

		do
		{
			status = NtQueryObject (hfile, ObjectNameInformation, pbuffer, bufferSize, &bufferSize);

			if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL)
			{
				SAFE_DELETE_ARRAY (pbuffer);
				pbuffer = (POBJECT_NAME_INFORMATION)new BYTE[bufferSize];
			}
			else
			{
				break;
			}
		}
		while (--attempts);

		if (NT_SUCCESS (status) && bufferSize)
		{
			pbuffer->Name.Buffer[pbuffer->Name.Length / sizeof (WCHAR)] = UNICODE_NULL; // trim buffer!

			path = pbuffer->Name.Buffer;

			_r_str_tolower (path.GetBuffer ()); // lower is important!
		}

		SAFE_DELETE_ARRAY (pbuffer);

		SAFE_DELETE_HANDLE (hfile);
	}

	return status;
}

/*
	Strings
*/

bool _r_str_isnumeric (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return false;

	while (*text != UNICODE_NULL)
	{
		if (iswdigit (*text) == 0)
			return false;

		++text;
	}

	return true;
}

bool _r_str_alloc (LPWSTR* pbuffer, size_t length, LPCWSTR text)
{
	if (!pbuffer)
		return false;

	SAFE_DELETE_ARRAY (*pbuffer);

	if (!length)
		return false;

	if (length == INVALID_SIZE_T)
		length = _r_str_length (text);

	length += 1;

	LPWSTR new_ptr = new WCHAR[length];

	_r_str_copy (new_ptr, length, text);

	*pbuffer = new_ptr;

	return true;
}

void _r_str_cat (LPWSTR buffer, size_t length, LPCWSTR text)
{
	if (!buffer || !length)
		return;

	if (length <= _R_STR_MAX_LENGTH)
	{
		size_t dest_length = _r_str_length (buffer);

		_r_str_copy (buffer + dest_length, length - dest_length, text);
	}
}

void _r_str_copy (LPWSTR buffer, size_t length, LPCWSTR text)
{
	if (!buffer || !length)
		return;

	if (length <= _R_STR_MAX_LENGTH)
	{
		if (!_r_str_isempty (text))
		{
			while (length && (*text != UNICODE_NULL))
			{
				*buffer++ = *text++;
				--length;
			}

			if (!length)
				--buffer; // truncate buffer
		}
	}

	*buffer = UNICODE_NULL;
}

size_t _r_str_length (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return 0;

	if (IsProcessorFeaturePresent (PF_XMMI64_INSTRUCTIONS_AVAILABLE)) // check sse2 feature
	{
		LPWSTR p = (LPWSTR)((ULONG_PTR)text & ~0xE); // string should be 2 byte aligned
		DWORD unaligned = PtrToUlong (text) & 0xF;

		__m128i b;
		__m128i z = _mm_setzero_si128 ();

		ULONG index;
		ULONG mask;

		if (unaligned != 0)
		{
			b = _mm_load_si128 ((__m128i*)p);
			b = _mm_cmpeq_epi16 (b, z);
			mask = _mm_movemask_epi8 (b) >> unaligned;

			if (_BitScanForward (&index, mask))
				return index / sizeof (WCHAR);

			p += 16 / sizeof (WCHAR);
		}

		while (true)
		{
			b = _mm_load_si128 ((__m128i*)p);
			b = _mm_cmpeq_epi16 (b, z);
			mask = _mm_movemask_epi8 (b);

			if (_BitScanForward (&index, mask))
				return (size_t)(p - text) + index / sizeof (WCHAR);

			p += 0x10 / sizeof (WCHAR);
		}
	}
	else
	{
		return wcsnlen_s (text, _R_STR_MAX_LENGTH);
	}
}

void _r_str_printf (LPWSTR buffer, size_t length, LPCWSTR text, ...)
{
	if (!buffer || !length)
		return;

	if (_r_str_isempty (text) || (length > _R_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return;
	}

	va_list args;
	va_start (args, text);

	_r_str_vprintf (buffer, length, text, args);

	va_end (args);
}

void _r_str_vprintf (LPWSTR buffer, size_t length, LPCWSTR text, va_list args)
{
	if (!buffer || !length)
		return;

	if (_r_str_isempty (text) || (length > _R_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return;
	}

	size_t max_length = length - 1; // leave the last space for the null terminator

#pragma warning(push)
#pragma warning(disable: 4996) // _CRT_SECURE_NO_WARNINGS
	INT res = _vsnwprintf (buffer, max_length, text, args);
#pragma warning(pop)

	if (res < 0 || size_t (res) >= max_length)
		buffer[max_length] = UNICODE_NULL; // need to null terminate the string
}

size_t _r_str_hash (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return 0;

#ifdef _WIN64
#define InitialFNV 2166136261ULL
#define FNVMultiple 16777619ULL
#else
#define InitialFNV 2166136261U
#define FNVMultiple 16777619U
#endif

	size_t hash = InitialFNV;

	while (*text != UNICODE_NULL)
	{
		hash = hash ^ (_r_str_upper (*text)); /* xor the low 8 bits */
		hash = hash * FNVMultiple; /* multiply by the magic number */

		++text;
	}

	return hash;
}

INT _r_str_compare (LPCWSTR str1, LPCWSTR str2, size_t length)
{
	if (!length)
		return 0;

	if (str1 == str2)
		return 0;

	const bool is_empty1 = _r_str_isempty (str1);
	const bool is_empty2 = _r_str_isempty (str2);

	if (is_empty1 && is_empty2)
		return 0;

	if (!is_empty1 && is_empty2)
		return 1;

	if (is_empty1 && !is_empty2)
		return -1;

	WCHAR ch1 = _r_str_upper (*str1);
	WCHAR ch2 = _r_str_upper (*str2);

	if (ch1 != ch2)
		return ch1 > ch2 ? 1 : -1;

	if (length != INVALID_SIZE_T)
		return _wcsnicmp (str1, str2, length); // by length

	return _wcsicmp (str1, str2);
}

INT _r_str_compare_logical (LPCWSTR str1, LPCWSTR str2)
{
	if (str1 == str2)
		return 0;

	const bool is_empty1 = _r_str_isempty (str1);
	const bool is_empty2 = _r_str_isempty (str2);

	if (is_empty1 && is_empty2)
		return 0;

	if (!is_empty1 && is_empty2)
		return 1;

	if (is_empty1 && !is_empty2)
		return -1;

	return StrCmpLogicalW (str1, str2);
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

rstring _r_str_fromsid (const PSID lpsid)
{
	rstring result;
	LPWSTR sidString;

	if (ConvertSidToStringSid (lpsid, &sidString))
	{
		result = sidString;

		SAFE_LOCAL_FREE (sidString);
	}

	return result;
}

size_t _r_str_find (LPCWSTR text, size_t length, WCHAR char_find, size_t start_pos)
{
	if (_r_str_isempty (text) || !length)
		return INVALID_SIZE_T;

	if (length == INVALID_SIZE_T)
		length = _r_str_length (text);

	if (start_pos >= length)
		return INVALID_SIZE_T;

	char_find = _r_str_upper (char_find);

	for (size_t i = start_pos; i != length; i++)
	{
		if (text[i] == UNICODE_NULL)
			return INVALID_SIZE_T;

		if (_r_str_upper (text[i]) == char_find)
			return i;
	}

	return INVALID_SIZE_T;
}

size_t _r_str_findlast (LPCWSTR text, size_t length, WCHAR char_find, size_t start_pos)
{
	if (_r_str_isempty (text) || !length)
		return INVALID_SIZE_T;

	if (length == INVALID_SIZE_T)
		length = _r_str_length (text);

	if (!start_pos || start_pos >= length)
		start_pos = length - 1;

	char_find = _r_str_upper (char_find);

	do
	{
		if (text[start_pos] == UNICODE_NULL)
			return INVALID_SIZE_T;

		if (_r_str_upper (text[start_pos]) == char_find)
			return start_pos;
	}
	while (start_pos--);

	return INVALID_SIZE_T;
}

bool _r_str_match (LPCWSTR text, LPCWSTR pattern)
{
	if (!text || !pattern)
		return false;

	// If we reach at the end of both strings, we are done
	if (*pattern == UNICODE_NULL && *text == UNICODE_NULL)
		return true;

	// Make sure that the characters after '*' are present
	// in second string. This function assumes that the first
	// string will not contain two consecutive '*'
	if (*pattern == L'*' && *(pattern + 1) != UNICODE_NULL && *text == UNICODE_NULL)
		return false;

	// If the first string contains '?', or current characters
	// of both strings match
	if (*pattern == L'?' || _r_str_upper (*text) == _r_str_upper (*pattern))
		return _r_str_match (text + 1, pattern + 1);

	// If there is *, then there are two possibilities
	// a) We consider current character of second string
	// b) We ignore current character of second string.
	if (*pattern == L'*')
		return _r_str_match (text + 1, pattern) || _r_str_match (text, pattern + 1);

	return false;
}

void _r_str_replace (LPWSTR text, WCHAR char_from, WCHAR char_to)
{
	if (!text)
		return;

	while (*text != UNICODE_NULL)
	{
		if (*text == char_from)
			*text = char_to;

		text += 1;
	}
}

void _r_str_trim (rstring& text, LPCWSTR trim)
{
	if (!text.IsEmpty ())
	{
		_r_str_trim (text.GetBuffer (), trim);
		text.ReleaseBuffer ();
	}
}

void _r_str_trim (LPWSTR text, LPCWSTR trim)
{
	if (!_r_str_isempty (text))
		StrTrim (text, trim);
}

void _r_str_tolower (LPWSTR text)
{
	if (!text)
		return;

	while (*text != UNICODE_NULL)
	{
		*text = _r_str_lower (*text);

		text += 1;
	}
}

void _r_str_toupper (LPWSTR text)
{
	if (!text)
		return;

	while (*text != UNICODE_NULL)
	{
		*text = _r_str_upper (*text);

		text += 1;
	}
}

rstring _r_str_extract (LPCWSTR text, size_t length, size_t start_pos, size_t extract_length)
{
	if (_r_str_isempty (text) || !length)
		return nullptr;

	if ((start_pos == 0) && (extract_length >= length))
		return text;

	if (start_pos >= length)
		return nullptr;

	if (extract_length > (length - start_pos))
		extract_length = (length - start_pos);

	return rstring (&text[start_pos], extract_length);
}

rstring& _r_str_extract_ref (rstring& text, size_t start_pos, size_t extract_length)
{
	if (text.IsEmpty ())
		return text;

	const size_t length = text.GetLength ();

	if ((start_pos == 0) && (extract_length >= length))
		return text;

	if (start_pos >= length)
	{
		text.Release ();
		return text;
	}

	if (extract_length > (length - start_pos))
		extract_length = (length - start_pos);

	LPWSTR buff = text.GetBuffer ();

	wmemmove (buff, &buff[start_pos], extract_length);

	return text.SetLength (extract_length);
}

LPWSTR _r_str_utf8_to_utf16 (LPCSTR text)
{
	if (!text || *text == ANSI_NULL)
		return nullptr;

	const INT length = static_cast<INT>(strnlen_s (text, _R_STR_MAX_LENGTH));
	INT ret_length = MultiByteToWideChar (CP_UTF8, 0, text, length, nullptr, 0);

	if (ret_length <= 0)
		return nullptr;

	LPWSTR buffer = new WCHAR[ret_length + 1]; // utilization required!

	ret_length = MultiByteToWideChar (CP_UTF8, 0, text, length, buffer, ret_length);

	buffer[ret_length] = UNICODE_NULL;

	return buffer;
}

void _r_str_split (LPCWSTR text, size_t length, WCHAR delimiter, rstringvec& rvc)
{
	if (_r_str_isempty (text) || !length)
		return;

	if (length == INVALID_SIZE_T)
		length = _r_str_length (text);

	LPCWSTR start = text;
	LPCWSTR end = start;
	LPCWSTR thisEnd = start + length;

	while (end < thisEnd)
	{
		if (*end == delimiter)
		{
			if (start < end)
				rvc.emplace_back (start, size_t (end - start));

			start = end + 1;
		}

		end++;
	}

	if (start < end)
		rvc.emplace_back (start, size_t (end - start));
}

bool _r_str_unserialize (LPCWSTR text, WCHAR delimeter, WCHAR key_delimeter, rstringmap1 * lpresult)
{
	if (_r_str_isempty (text) || !lpresult)
		return false;

	rstringvec rvc;
	_r_str_split (text, INVALID_SIZE_T, delimeter, rvc);

	for (size_t i = 0; i < rvc.size (); i++)
	{
		rstring& rlink = rvc.at (i);

		const size_t pos = _r_str_find (rlink, rlink.GetLength (), key_delimeter);

		if (pos != INVALID_SIZE_T)
			(*lpresult)[_r_str_extract (rlink, rlink.GetLength (), 0, pos)] = _r_str_extract (rlink, rlink.GetLength (), pos + 1);
	}

	return true;
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

	for (size_t i = 0; i < _countof (oct_v1); i++)
	{
		if (oct_v1[i] > oct_v2[i])
			return 1;

		else if (oct_v1[i] < oct_v2[i])
			return -1;
	}

	return 0;
}

/*
	System information
*/

bool _r_sys_iselevated ()
{
	static bool is_initialized = false;
	static bool is_elevated = false;

	if (!is_initialized)
	{

#if !defined(_APP_NO_WINXP)
		// winxp compatibility
		if (!_r_sys_validversion (6, 0))
		{
			is_initialized = true;
			is_elevated = !!IsUserAnAdmin ();

			return is_elevated;
		}
#endif // _APP_NO_WINXP

		const bool is_win8 = _r_sys_validversion (6, 2);

		HANDLE htoken;

		// win8+
		if (is_win8)
			htoken = NtCurrentProcessToken ();

		else
			OpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &htoken);

		TOKEN_ELEVATION elevation;
		ULONG returnLength;

		if (NT_SUCCESS (NtQueryInformationToken (htoken, TokenElevation, &elevation, sizeof (TOKEN_ELEVATION), &returnLength)))
		{
			is_elevated = !!elevation.TokenIsElevated;
		}

		if (!is_win8)
			SAFE_DELETE_HANDLE (htoken);

		is_initialized = true;
	}

	return is_elevated;
}

rstring _r_sys_getsessioninfo (WTS_INFO_CLASS info)
{
	LPWSTR buffer;
	DWORD length;

	if (WTSQuerySessionInformation (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, info, &buffer, &length))
	{
		rstring result = buffer;

		WTSFreeMemory (buffer);

		return result;
	}

	return nullptr;
}

rstring _r_sys_getusernamesid (LPCWSTR domain, LPCWSTR username)
{
	rstring result;

	DWORD sid_length = SECURITY_MAX_SID_SIZE;
	PBYTE psid = new BYTE[sid_length];

	WCHAR ref_domain[MAX_PATH] = {0};
	DWORD ref_length = _countof (ref_domain);

	SID_NAME_USE name_use;

	if (LookupAccountName (domain, username, psid, &sid_length, ref_domain, &ref_length, &name_use))
		result = _r_str_fromsid (psid);

	SAFE_DELETE_ARRAY (psid);

	return result;
}

#if !defined(_WIN64)
bool _r_sys_iswow64 ()
{
	// IsWow64Process is not available on all supported versions of Windows.
	// Use GetModuleHandle to get a handle to the DLL that contains the function
	// and GetProcAddress to get a pointer to the function if available.

	const HMODULE hlib = GetModuleHandle (L"kernel32.dll");

	if (hlib)
	{
		typedef BOOL (WINAPI * IW64P) (HANDLE, PBOOL); // IsWow64Process
		const IW64P _IsWow64Process = (IW64P)GetProcAddress (hlib, "IsWow64Process");

		if (_IsWow64Process)
		{
			BOOL result = FALSE;

			if (_IsWow64Process (NtCurrentProcess (), &result))
				return !!result;
		}
	}

	return false;
}
#endif // _WIN64

bool _r_sys_setprivilege (LPCWSTR* pprivileges, size_t count, bool is_enable)
{
	HANDLE token = nullptr;

	LUID luid = {0};
	TOKEN_PRIVILEGES tp = {0};

	bool result = false;

	if (OpenProcessToken (NtCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
	{
		for (size_t i = 0; i < count; i++)
		{
			if (LookupPrivilegeValue (nullptr, pprivileges[i], &luid))
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Luid = luid;
				tp.Privileges[0].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;

				if (AdjustTokenPrivileges (token, FALSE, &tp, sizeof (tp), nullptr, nullptr))
					result = true;
			}
		}

		SAFE_DELETE_HANDLE (token);
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

	return !!VerifyVersionInfo (&osvi, type_mask, mask);
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
		const LONG64 ll = Int32x32To64 (ut, 10000000LL) + 116444736000000000LL; // 64-bit value

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

time_t _r_unixtime_from_filetime (const FILETIME * pft)
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
	Device context (Draw/Calculation etc...)
*/

INT _r_dc_getdpivalue (HWND hwnd)
{
	static const bool is_win81 = _r_sys_validversion (6, 3); // win81+

	if (is_win81)
	{
		static const bool is_win10rs1 = _r_sys_validversion (10, 0, 14393); //win10rs1+

		HMODULE huser32 = GetModuleHandle (L"user32.dll");

		if (hwnd)
		{
			// GetDpiForWindow (win10rs1+)
			if (is_win10rs1 && huser32)
			{
				typedef UINT (WINAPI * GDFW) (HWND); // GetDpiForWindow (win10rs1+)
				const GDFW _GetDpiForWindow = (GDFW)GetProcAddress (huser32, "GetDpiForWindow");

				if (_GetDpiForWindow)
					return _GetDpiForWindow (hwnd);
			}

			// GetDpiForMonitor (win81+)
			HMODULE hshcore = LoadLibraryEx (L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

			if (hshcore)
			{
				HMONITOR hmon = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

				if (hmon)
				{
					UINT dpix = 0, dpiy = 0;

					typedef HRESULT (WINAPI * GDFM) (HMONITOR, MONITOR_DPI_TYPE, PUINT, PUINT); // GetDpiForMonitor
					const GDFM _GetDpiForMonitor = (GDFM)GetProcAddress (hshcore, "GetDpiForMonitor"); // win81+

					if (_GetDpiForMonitor && SUCCEEDED (_GetDpiForMonitor (hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy)))
					{
						FreeLibrary (hshcore);
						return dpix;
					}
				}

				FreeLibrary (hshcore);
			}
		}

		if (is_win10rs1 && huser32)
		{
			// GetDpiForSystem (win10rs1+)
			typedef UINT (WINAPI * GDFS) (); // GetDpiForSystem
			const GDFS _GetDpiForSystem = (GDFS)GetProcAddress (huser32, "GetDpiForSystem"); // win10rs1+

			if (_GetDpiForSystem)
				return _GetDpiForSystem ();
		}
	}

	// fallback (pre-win10)
	HDC hdc = GetDC (nullptr);

	if (hdc)
	{
		INT result = GetDeviceCaps (hdc, LOGPIXELSX);
		ReleaseDC (nullptr, hdc);

		return result;
	}

	return USER_DEFAULT_SCREEN_DPI;
}

COLORREF _r_dc_getcolorbrightness (COLORREF clr)
{
	DWORD r = clr & 0xff;
	DWORD g = (clr >> 8) & 0xff;
	DWORD b = (clr >> 16) & 0xff;

	DWORD min = r;
	DWORD max = r;

	if (g < min)
		min = g;

	if (b < min)
		min = b;

	if (g > max)
		max = g;

	if (b > max)
		max = b;

	if (((min + max) / 2) > 100)
		return RGB (0x00, 0x00, 0x00);

	return RGB (0xff, 0xff, 0xff);
}

COLORREF _r_dc_getcolorshade (COLORREF clr, INT percent)
{
	COLORREF r = ((GetRValue (clr) * percent) / 100);
	COLORREF g = ((GetGValue (clr) * percent) / 100);
	COLORREF b = ((GetBValue (clr) * percent) / 100);

	return RGB (r, g, b);
}

INT _r_dc_getsystemmetrics (HWND hwnd, INT index)
{
	static const bool is_win10rs1 = _r_sys_validversion (10, 0, 14393); // win10rs1+

	if (is_win10rs1 && hwnd)
	{
		const HMODULE hlib = GetModuleHandle (L"user32.dll");

		if (hlib)
		{
			typedef INT (WINAPI * GSMFD) (INT, UINT); // GetSystemMetricsForDpi
			const GSMFD _GetSystemMetricsForDpi = (GSMFD)GetProcAddress (hlib, "GetSystemMetricsForDpi"); // win10rs1+

			if (_GetSystemMetricsForDpi)
			{
				INT metrics = _GetSystemMetricsForDpi (index, _r_dc_getdpivalue (hwnd));

				if (metrics)
					return metrics;
			}
		}
	}

	return GetSystemMetrics (index);
}

// Optimized version of WinAPI function "FillRect"
void _r_dc_fillrect (HDC hdc, const LPRECT lprc, COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (hdc, clr);
	ExtTextOut (hdc, 0, 0, ETO_OPAQUE, lprc, nullptr, 0, nullptr);
	SetBkColor (hdc, clr_prev);
}

LONG _r_dc_fontwidth (HDC hdc, LPCWSTR text, size_t length)
{
	if (_r_str_isempty (text) || !length)
		return 0;

	SIZE size = {0};

	if (!GetTextExtentPoint32 (hdc, text, (INT)length, &size))
		return 200; // fallback

	return size.cx;
}

/*
	Window management
*/

void _r_wnd_addstyle (HWND hwnd, INT ctrl_id, LONG_PTR mask, LONG_PTR stateMask, INT index)
{
	if (ctrl_id)
		hwnd = GetDlgItem (hwnd, ctrl_id);

	const LONG_PTR style = (GetWindowLongPtr (hwnd, index) & ~stateMask) | mask;

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

void _r_wnd_centerwindowrect (LPRECT lprect, const LPRECT lpparent)
{
	lprect->left = lpparent->left + (_R_RECT_WIDTH (lpparent) - _R_RECT_WIDTH (lprect)) / 2;
	lprect->top = lpparent->top + (_R_RECT_HEIGHT (lpparent) - _R_RECT_HEIGHT (lprect)) / 2;
}

void _r_wnd_center (HWND hwnd, HWND hparent)
{
	if (hparent && (IsWindow (hparent) && IsWindowVisible (hparent) && !IsIconic (hparent)))
	{
		RECT rect = {0}, parentRect = {0};

		GetWindowRect (hwnd, &rect);
		GetWindowRect (hparent, &parentRect);

		_r_wnd_centerwindowrect (&rect, &parentRect);
		_r_wnd_adjustwindowrect (hwnd, &rect);

		_r_wnd_resize (nullptr, hwnd, nullptr, rect.left, rect.top, 0, 0, 0);
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

			_r_wnd_resize (nullptr, hwnd, nullptr, rect.left, rect.top, 0, 0, 0);
		}
	}
}

void _r_wnd_changemessagefilter (HWND hwnd, PUINT pmsg, size_t count, DWORD action)
{
	const HMODULE hlib = GetModuleHandle (L"user32.dll");

	if (!hlib)
		return;

#ifdef _APP_NO_WINXP
	typedef BOOL (WINAPI * CWMFEX) (HWND, UINT, DWORD, PVOID); // ChangeWindowMessageFilterEx
	const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (hlib, "ChangeWindowMessageFilterEx"); // win7+

	if (_ChangeWindowMessageFilterEx)
	{
		for (size_t i = 0; i < count; i++)
			_ChangeWindowMessageFilterEx (hwnd, pmsg[i], action, nullptr);

		return;
	}

	for (size_t i = 0; i < count; i++)
		ChangeWindowMessageFilter (pmsg[i], action); // vista fallback
#else
	typedef BOOL (WINAPI * CWMFEX) (HWND, UINT, DWORD, PVOID); // ChangeWindowMessageFilterEx
	const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (hlib, "ChangeWindowMessageFilterEx"); // win7+

	if (_ChangeWindowMessageFilterEx)
	{
		for (size_t i = 0; i < count; i++)
			_ChangeWindowMessageFilterEx (hwnd, pmsg[i], action, nullptr);
	}
	else
	{
		typedef BOOL (WINAPI * CWMF) (UINT, DWORD); // ChangeWindowMessageFilter
		const CWMF _ChangeWindowMessageFilter = (CWMF)GetProcAddress (hlib, "ChangeWindowMessageFilter"); // vista fallback

		if (_ChangeWindowMessageFilter)
		{
			for (size_t i = 0; i < count; i++)
				_ChangeWindowMessageFilter (pmsg[i], action);
		}
	}
#endif // _APP_NO_WINXP
}

void _r_wnd_enablenonclientscaling (HWND hwnd)
{
	if (!_r_sys_validversion (10, 0, 14393)) // win10rs1+
		return;

	HMODULE hlib = GetModuleHandle (L"user32.dll");

	if (hlib)
	{
		typedef BOOL (WINAPI * ENCDS) (HWND); // EnableNonClientDpiScaling
		const ENCDS _EnableNonClientDpiScaling = (ENCDS)GetProcAddress (hlib, "EnableNonClientDpiScaling");

		if (_EnableNonClientDpiScaling)
			_EnableNonClientDpiScaling (hwnd);
	}
}

void _r_wnd_toggle (HWND hwnd, bool is_show)
{
	if (is_show || !IsWindowVisible (hwnd))
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
	if (!IsWindow (hwnd) || !IsWindowVisible (hwnd) || IsIconic (hwnd))
		return false;

	POINT pt = {0};
	RECT rect = {0};

	GetCursorPos (&pt);
	GetWindowRect (hwnd, &rect);

	return !!PtInRect (&rect, pt);
}

static bool _r_wnd_isplatformfullscreenmode ()
{
	QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;

	// SHQueryUserNotificationState is only available for Vista+
#ifdef _APP_NO_WINXP
	if (FAILED (SHQueryUserNotificationState (&state)))
		return false;
#else
	if (_r_sys_validversion (6, 0))
	{
		const HINSTANCE hlib = GetModuleHandle (L"shell32.dll");

		if (hlib)
		{
			typedef HRESULT (WINAPI * SHQueryUserNotificationStatePtr)(QUERY_USER_NOTIFICATION_STATE * state);
			const SHQueryUserNotificationStatePtr _SHQueryUserNotificationState = (SHQueryUserNotificationStatePtr)GetProcAddress (hlib, "SHQueryUserNotificationState");

			if (_SHQueryUserNotificationState)
			{
				if (FAILED (_SHQueryUserNotificationState (&state)))
					return false;
			}
		}
	}
#endif _APP_NO_WINXP

	return (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE);
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

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) || (ext_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

static bool _r_wnd_isfullscreenconsolemode ()
{
	// We detect this by attaching the current process to the console of the
	// foreground window and then checking if it is in full screen mode.
	HWND hwnd = GetForegroundWindow ();

	if (!hwnd)
		return false;

	DWORD pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

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
	return _r_wnd_isplatformfullscreenmode () || _r_wnd_isfullscreenwindowmode () || _r_wnd_isfullscreenconsolemode ();
}

void _r_wnd_resize (HDWP* hdefer, HWND hwnd, HWND hwnd_after, INT left, INT right, INT width, INT height, UINT flags)
{
	flags |= SWP_NOACTIVATE;

	if (!width && !height)
		flags |= SWP_NOSIZE;

	if (!hwnd_after)
		flags |= SWP_NOZORDER | SWP_NOOWNERZORDER;

	if (hdefer && *hdefer)
		*hdefer = DeferWindowPos (*hdefer, hwnd, hwnd_after, left, right, width, height, flags);

	else
		SetWindowPos (hwnd, hwnd_after, left, right, width, height, flags);
}

#ifndef _APP_NO_DARKTHEME
BOOL CALLBACK DarkExplorerChildProc (HWND hwnd, LPARAM lparam)
{
	if (!IsWindow (hwnd))
		return TRUE;

	const BOOL is_darktheme = LOWORD (lparam);
	const HMODULE huxtheme = GetModuleHandle (L"uxtheme.dll");

	if (!huxtheme)
		return FALSE;

	typedef BOOL (WINAPI * ADMFW) (HWND, BOOL); // AllowDarkModeForWindow
	const ADMFW _AllowDarkModeForWindow = (ADMFW)GetProcAddress (huxtheme, MAKEINTRESOURCEA (133));

	if (!_AllowDarkModeForWindow)
		return FALSE;

	_AllowDarkModeForWindow (hwnd, is_darktheme);

	return TRUE;
}

bool _r_wnd_isdarkmessage (LPCWSTR type)
{
	if (!_r_sys_validversion (10, 0, 17763)) // win10rs5+
		return false;

	if (_r_str_compare (type, L"ImmersiveColorSet") == 0)
	{
		const HMODULE huxtheme = GetModuleHandle (L"uxtheme.dll");

		if (huxtheme)
		{
			// RefreshImmersiveColorPolicyState
			typedef VOID (WINAPI * RICPS) (VOID);
			const RICPS _RefreshImmersiveColorPolicyState = (RICPS)GetProcAddress (huxtheme, MAKEINTRESOURCEA (104));

			if (_RefreshImmersiveColorPolicyState)
				_RefreshImmersiveColorPolicyState ();

			// GetIsImmersiveColorUsingHighContrast
			typedef BOOL (WINAPI * GIICUHC) (IMMERSIVE_HC_CACHE_MODE);
			const GIICUHC _GetIsImmersiveColorUsingHighContrast = (GIICUHC)GetProcAddress (huxtheme, MAKEINTRESOURCEA (106));

			if (_GetIsImmersiveColorUsingHighContrast)
				_GetIsImmersiveColorUsingHighContrast (IHCM_REFRESH);
		}

		return true;
	}

	return false;
}

bool _r_wnd_isdarktheme ()
{
	if (!_r_sys_validversion (10, 0, 17763)) // win10rs5+
		return false;

	HIGHCONTRAST hci = {0};
	hci.cbSize = sizeof (hci);

	if (SystemParametersInfo (SPI_GETHIGHCONTRAST, 0, &hci, 0))
	{
		// no dark mode in high-contrast mode
		if ((hci.dwFlags & HCF_HIGHCONTRASTON) != 0)
			return false;
	}

	const HMODULE huxtheme = LoadLibraryEx (L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huxtheme)
	{
		typedef BOOL (WINAPI * SAUDM) (VOID); // ShouldAppsUseDarkMode
		const SAUDM _ShouldAppsUseDarkMode = (SAUDM)GetProcAddress (huxtheme, MAKEINTRESOURCEA (132));

		if (_ShouldAppsUseDarkMode)
		{
			const bool result = !!_ShouldAppsUseDarkMode ();
			FreeLibrary (huxtheme);

			return result;
		}

		FreeLibrary (huxtheme);
	}

	return false;
}

void _r_wnd_setdarkframe (HWND hwnd, BOOL is_enable)
{
	// Set dark window frame
// https://social.msdn.microsoft.com/Forums/en-US/e36eb4c0-4370-4933-943d-b6fe22677e6c/dark-mode-apis?forum=windowssdk
#if defined(_APP_NO_WINXP)
	DwmSetWindowAttribute (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_enable, sizeof (is_enable));
#else
	const HMODULE hdwmapi = LoadLibraryEx (L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (hdwmapi)
	{
		typedef HRESULT (WINAPI * DSWA) (HWND, DWORD, LPCVOID, DWORD); // DwmSetWindowAttribute
		const DSWA _DwmSetWindowAttribute = (DSWA)GetProcAddress (hdwmapi, "DwmSetWindowAttribute");

		if (_DwmSetWindowAttribute)
			_DwmSetWindowAttribute (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_enable, sizeof (is_enable));

		FreeLibrary (hdwmapi);
	}
#endif // _APP_NO_WINXP
}

void _r_wnd_setdarktheme (HWND hwnd)
{
	if (!_r_sys_validversion (10, 0, 17763)) // win10rs5+
		return;

	const BOOL is_darktheme = _r_wnd_isdarktheme ();

	/*
		Ordinal 104: VOID WINAPI RefreshImmersiveColorPolicyState()
		Ordinal 106: BOOL WINAPI GetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE mode)
		Ordinal 132: BOOL WINAPI ShouldAppsUseDarkMode()
		Ordinal 133: BOOL WINAPI AllowDarkModeForWindow(HWND, BOOL allow)
		Ordinal 135: BOOL WINAPI AllowDarkModeForApp(bool allow)
		Ordinal 136: VOID WINAPI FlushMenuThemes()
		Ordinal 137: BOOL WINAPI IsDarkModeAllowedForWindow(HWND)
	*/

	_r_wnd_setdarkframe (hwnd, is_darktheme);

	const HMODULE huxtheme = LoadLibraryEx (L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huxtheme)
	{
		auto ord135 = GetProcAddress (huxtheme, MAKEINTRESOURCEA (135));

		if (ord135)
		{
			typedef BOOL (WINAPI * ADMFA) (BOOL); // AllowDarkModeForApp
			typedef PreferredAppMode (WINAPI * SPM) (PreferredAppMode); // SetPreferredAppMode

			ADMFA _AllowDarkModeForApp = nullptr;
			SPM _SetPreferredAppMode = nullptr;

			if (_r_sys_validversion (10, 0, 18334)) // win10rs6+
				_SetPreferredAppMode = reinterpret_cast<SPM>(ord135);

			else
				_AllowDarkModeForApp = reinterpret_cast<ADMFA>(ord135);

			if (_AllowDarkModeForApp || _SetPreferredAppMode)
			{
				if (_SetPreferredAppMode)
					_SetPreferredAppMode (is_darktheme ? AllowDark : Default);

				else if (_AllowDarkModeForApp)
					_AllowDarkModeForApp (is_darktheme);

				EnumChildWindows (hwnd, &DarkExplorerChildProc, MAKELPARAM (is_darktheme, 0));

				typedef VOID (WINAPI * RICPS) (VOID); // RefreshImmersiveColorPolicyState
				const RICPS _RefreshImmersiveColorPolicyState = (RICPS)GetProcAddress (huxtheme, MAKEINTRESOURCEA (104));

				if (_RefreshImmersiveColorPolicyState)
					_RefreshImmersiveColorPolicyState ();

				typedef void (WINAPI * FMT) (VOID); // FlushMenuThemes
				const FMT _FlushMenuThemes = (FMT)GetProcAddress (huxtheme, MAKEINTRESOURCEA (136));

				if (_FlushMenuThemes)
					_FlushMenuThemes ();

				InvalidateRect (hwnd, nullptr, TRUE); // HACK!!!
			}
		}

		FreeLibrary (huxtheme);
	}
}
#endif // _APP_NO_DARKTHEME

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent, LPCWSTR proxy_addr)
{
	const bool is_win81 = _r_sys_validversion (6, 3);

	HINTERNET hsession = WinHttpOpen (useragent, (is_win81 && _r_str_isempty (proxy_addr)) ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return nullptr;

	DWORD option;

	// enable secure protocols
	option = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

	if (is_win81)
		option |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3; // tls 1.3 for win81+

	WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &option, sizeof (option));

	// enable compression feature (win81+)
	if (is_win81)
	{
		option = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
		WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &option, sizeof (option));
	}

	// enable http2 protocol (win10rs1+)
	if (_r_sys_validversion (10, 0, 14393))
	{
		option = WINHTTP_PROTOCOL_FLAG_HTTP2;
		WinHttpSetOption (hsession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &option, sizeof (option));
	}

	// set connections per-server
	option = 1;
	WinHttpSetOption (hsession, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &option, sizeof (option));

	return hsession;
}

DWORD _r_inet_openurl (HINTERNET hsession, LPCWSTR url, LPCWSTR proxy_addr, LPHINTERNET pconnect, LPHINTERNET prequest, PDWORD ptotallength)
{
	if (!hsession || !pconnect || !prequest)
		return ERROR_BAD_ARGUMENTS;

	WCHAR url_host[MAX_PATH] = {0};
	WCHAR url_path[MAX_PATH] = {0};
	WORD url_port = 0;
	INT url_scheme = 0;

	DWORD rc = _r_inet_parseurl (url, &url_scheme, url_host, &url_port, url_path, nullptr, nullptr);

	if (rc != ERROR_SUCCESS)
	{
		return rc;
	}
	else
	{
		HINTERNET hconnect = WinHttpConnect (hsession, url_host, url_port, 0);

		if (!hconnect)
		{
			return GetLastError ();
		}
		else
		{
			DWORD flags = WINHTTP_FLAG_REFRESH;

			if (url_scheme == INTERNET_SCHEME_HTTPS)
				flags |= WINHTTP_FLAG_SECURE;

			HINTERNET hrequest = WinHttpOpenRequest (hconnect, L"GET", url_path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

			if (!hrequest)
			{
				_r_inet_close (hconnect);
				return GetLastError ();
			}
			else
			{
				// disable "keep-alive" feature (win7+)
				if (_r_sys_validversion (6, 1))
				{
					DWORD option = WINHTTP_DISABLE_KEEP_ALIVE;
					WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof (option));
				}

				// set proxy configuration (if available)
				if (!_r_str_isempty (proxy_addr))
				{
					WCHAR proxy_host[MAX_PATH] = {0};
					WCHAR proxy_user[MAX_PATH] = {0};
					WCHAR proxy_pass[MAX_PATH] = {0};
					WORD proxy_port = 0;

					if (_r_inet_parseurl (proxy_addr, nullptr, proxy_host, &proxy_port, nullptr, proxy_user, proxy_pass) == ERROR_SUCCESS)
					{
						WINHTTP_PROXY_INFO wpi = {0};

						WCHAR buffer[MAX_PATH] = {0};
						_r_str_printf (buffer, _countof (buffer), L"%s:%" PRIu16, proxy_host, proxy_port);

						wpi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
						wpi.lpszProxy = buffer;
						wpi.lpszProxyBypass = nullptr;

						WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY, &wpi, sizeof (wpi));

						// set proxy credentials (if exists)
						if (!_r_str_isempty (proxy_user))
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_USERNAME, proxy_user, (DWORD)_r_str_length (proxy_user));

						if (!_r_str_isempty (proxy_pass))
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_PASSWORD, proxy_pass, (DWORD)_r_str_length (proxy_pass));
					}
				}

				DWORD attempts = 6;
				DWORD option, size;

				do
				{
					if (!WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0))
					{
						rc = GetLastError ();

						if (rc == ERROR_WINHTTP_CONNECTION_ERROR)
						{
							option = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

							if (!WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &option, sizeof (option)))
								break;
						}
						else if (rc == ERROR_WINHTTP_RESEND_REQUEST)
						{
							continue;
						}
						else if (rc == ERROR_WINHTTP_SECURE_FAILURE)
						{
							option = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;

							if (!WinHttpSetOption (hrequest, WINHTTP_OPTION_SECURITY_FLAGS, &option, sizeof (option)))
								break;
						}
						else
						{
							// ERROR_WINHTTP_CANNOT_CONNECT etc.
							break;
						}
					}
					else
					{
						if (!WinHttpReceiveResponse (hrequest, nullptr))
						{
							rc = GetLastError ();
						}
						else
						{
							if (ptotallength)
							{
								size = sizeof (DWORD);
								WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, ptotallength, &size, WINHTTP_NO_HEADER_INDEX);
							}

							*pconnect = hconnect;
							*prequest = hrequest;

							return ERROR_SUCCESS;
						}
					}
				}
				while (--attempts);

				_r_inet_close (hrequest);
			}

			_r_inet_close (hconnect);
		}
	}

	return rc;
}

bool _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, DWORD buffer_length, PDWORD preaded, PDWORD ptotalreaded)
{
	DWORD readed = 0;
	const bool is_readed = !!WinHttpReadData (hrequest, buffer, buffer_length, &readed);

	if (preaded)
		*preaded = readed;

	if (ptotalreaded)
		*ptotalreaded += readed;

	return is_readed;
}

DWORD _r_inet_parseurl (LPCWSTR url, PINT scheme_ptr, LPWSTR host_ptr, LPWORD port_ptr, LPWSTR path_ptr, LPWSTR user_ptr, LPWSTR pass_ptr)
{
	if (_r_str_isempty (url) || (!scheme_ptr && !host_ptr && !port_ptr && !path_ptr && !user_ptr && !pass_ptr))
		return ERROR_BAD_ARGUMENTS;

	URL_COMPONENTS url_comp = {0};
	RtlSecureZeroMemory (&url_comp, sizeof (url_comp));

	url_comp.dwStructSize = sizeof (url_comp);

	const size_t url_length = _r_str_length (url);
	const DWORD max_length = MAX_PATH;

	if (host_ptr)
	{
		url_comp.lpszHostName = host_ptr;
		url_comp.dwHostNameLength = max_length;
	}

	if (path_ptr)
	{
		url_comp.lpszUrlPath = path_ptr;
		url_comp.dwUrlPathLength = max_length;
	}

	if (user_ptr)
	{
		url_comp.lpszUserName = user_ptr;
		url_comp.dwUserNameLength = max_length;
	}

	if (pass_ptr)
	{
		url_comp.lpszPassword = pass_ptr;
		url_comp.dwPasswordLength = max_length;
	}

	if (!WinHttpCrackUrl (url, DWORD (url_length), ICU_DECODE, &url_comp))
	{
		DWORD rc = GetLastError ();

		if (rc != ERROR_WINHTTP_UNRECOGNIZED_SCHEME)
			return rc;

		if (!WinHttpCrackUrl (_r_fmt (L"https://%s", url), DWORD (url_length + 8), ICU_DECODE, &url_comp))
			return GetLastError ();
	}

	if (scheme_ptr)
		*scheme_ptr = url_comp.nScheme;

	if (port_ptr)
		*port_ptr = url_comp.nPort;

	return ERROR_SUCCESS;
}

DWORD _r_inet_downloadurl (HINTERNET hsession, LPCWSTR proxy_addr, LPCWSTR url, LONG_PTR lpdest, bool is_filepath, _R_CALLBACK_HTTP_DOWNLOAD _callback, LONG_PTR lpdata)
{
	if (!hsession || !lpdest)
		return ERROR_BAD_ARGUMENTS;

	HINTERNET hconnect = nullptr;
	HINTERNET hrequest = nullptr;

	DWORD total_length = 0;

	DWORD rc = _r_inet_openurl (hsession, url, proxy_addr, &hconnect, &hrequest, &total_length);

	if (rc != ERROR_SUCCESS)
	{
		return rc;
	}
	else
	{
		const size_t buffer_length = _R_BUFFER_NET_LENGTH;

		LPSTR content_buffer = new CHAR[buffer_length];

		rstring* lpbuffer = reinterpret_cast<rstring*>(lpdest);
		HANDLE hfile = reinterpret_cast<HANDLE>(lpdest);

		DWORD notneed = 0;
		DWORD readed = 0;
		DWORD total_readed = 0;

		while (_r_inet_readrequest (hrequest, content_buffer, buffer_length - 1, &readed, &total_readed))
		{
			if (!readed)
			{
				rc = ERROR_SUCCESS;
				break;
			}

			if (is_filepath)
			{
				if (!WriteFile (hfile, content_buffer, readed, &notneed, nullptr))
				{
					rc = GetLastError ();
					break;
				}
			}
			else
			{
				content_buffer[readed] = ANSI_NULL;

				LPWSTR buffer = _r_str_utf8_to_utf16 (content_buffer);

				if (!buffer)
				{
					rc = GetLastError ();
					break;
				}

				lpbuffer->Append (buffer);

				SAFE_DELETE_ARRAY (buffer);
			}

			if (_callback && !_callback (total_readed, (std::max) (total_readed, total_length), lpdata))
			{
				rc = ERROR_CANCELLED;
				break;
			}
		}

		SAFE_DELETE_ARRAY (content_buffer);
	}

	_r_inet_close (hrequest);
	_r_inet_close (hconnect);

	return rc;
}

/*
	Registry
*/

PBYTE _r_reg_querybinary (HKEY hkey, LPCWSTR value)
{
	DWORD type = 0;
	DWORD size = 0;

	if (RegQueryValueEx (hkey, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size)
	{
		if (type == REG_BINARY)
		{
			PBYTE pbuffer = new BYTE[size]; // utilization required!

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, pbuffer, &size) == ERROR_SUCCESS)
				return pbuffer;
		}
	}

	return nullptr;
}

DWORD _r_reg_querydword (HKEY hkey, LPCWSTR value)
{
	DWORD type = 0;
	DWORD size = 0;

	if (RegQueryValueEx (hkey, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size)
	{
		if (type == REG_DWORD)
		{
			DWORD buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
		else if (type == REG_QWORD)
		{
			DWORD64 buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (DWORD)buffer;
		}
	}

	return 0;
}

DWORD64 _r_reg_querydword64 (HKEY hkey, LPCWSTR value)
{
	DWORD type = 0;
	DWORD size = 0;

	if (RegQueryValueEx (hkey, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size)
	{
		if (type == REG_DWORD)
		{
			DWORD buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (DWORD64)buffer;
		}
		else if (type == REG_QWORD)
		{
			DWORD64 buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
	}

	return 0;
}

rstring _r_reg_querystring (HKEY hkey, LPCWSTR value)
{
	rstring result;

	DWORD type = 0;
	DWORD size = 0;

	if (RegQueryValueEx (hkey, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size)
	{
		if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
		{
			size_t buffer_length = size / sizeof (WCHAR);
			DWORD rc = RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)result.GetBuffer (buffer_length), &size);

			if (rc == ERROR_MORE_DATA)
			{
				buffer_length = size / sizeof (WCHAR);
				rc = RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)result.GetBuffer (buffer_length), &size);
			}

			if (rc == ERROR_SUCCESS)
			{
				result.SetLength (buffer_length - 1);

				if (type == REG_EXPAND_SZ)
				{
					rstring buffer;

					if (!ExpandEnvironmentStrings (result, buffer.GetBuffer (1024), 1024))
					{
						buffer.Release ();
					}
					else
					{
						result = std::move (buffer.ReleaseBuffer ());
					}
				}
			}
			else
			{
				result.Release ();
			}
		}
		else if (type == REG_DWORD)
		{
			DWORD buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				result.Format (L"%" PRId32, buffer);
		}
		else if (type == REG_QWORD)
		{
			DWORD64 buffer = 0;

			if (RegQueryValueEx (hkey, value, nullptr, nullptr, (LPBYTE)&buffer, &size) == ERROR_SUCCESS)
				result.Format (L"%" PRId64, buffer);
		}
	}

	return result;
}

DWORD _r_reg_querysubkeylength (HKEY hkey)
{
	DWORD max_subkey_length = 0;

	if (RegQueryInfoKey (hkey, nullptr, nullptr, nullptr, nullptr, &max_subkey_length, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		return max_subkey_length + 1;

	return 0;
}

time_t _r_reg_querytimestamp (HKEY hkey)
{
	FILETIME ft = {0};

	if (RegQueryInfoKey (hkey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS)
		return _r_unixtime_from_filetime (&ft);

	return 0;
}

/*
	Other
*/

HANDLE _r_createthread (_beginthreadex_proc_type proc, void* args, bool is_suspended, INT priority)
{
	HANDLE hthread = (HANDLE)_beginthreadex (nullptr, 0, proc, args, CREATE_SUSPENDED, nullptr);

	// On an error:
	//		_beginthread returns -1L
	//		_beginthreadex returns 0

	if (!hthread || hthread == INVALID_HANDLE_VALUE)
		return nullptr;

	SetThreadPriority (hthread, priority);

	if (!is_suspended)
		ResumeThread (hthread);

	return hthread;
}

HICON _r_loadicon (HINSTANCE hinst, LPCWSTR name, INT size)
{
	HICON hicon;

#ifdef _APP_NO_WINXP
	if (SUCCEEDED (LoadIconWithScaleDown (hinst, name, size, size, &hicon)))
		return hicon;

	return nullptr;
#else
	const HMODULE hlib = GetModuleHandle (L"comctl32.dll");

	if (hlib)
	{
		typedef HRESULT (WINAPI * LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
		const LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (hlib, "LoadIconWithScaleDown");

		if (_LoadIconWithScaleDown)
		{
			if (SUCCEEDED (_LoadIconWithScaleDown (hinst, name, size, size, &hicon)))
				return hicon;
		}
	}

	return (HICON)LoadImage (hinst, name, IMAGE_ICON, size, size, 0);
#endif // _APP_NO_WINXP
}

LPVOID _r_loadresource (HINSTANCE hinst, LPCWSTR res, LPCWSTR type, PDWORD psize)
{
	HRSRC hres = FindResource (hinst, res, type);

	if (hres)
	{
		HGLOBAL hloaded = LoadResource (hinst, hres);

		if (hloaded)
		{
			LPVOID pLockedResource = LockResource (hloaded);

			if (pLockedResource)
			{
				if (psize)
					*psize = SizeofResource (hinst, hres);

				UnlockResource (pLockedResource);

				return pLockedResource;
			}
		}
	}

	return nullptr;
}

bool _r_parseini (LPCWSTR path, rstringmap2& pmap, rstringvec * psections)
{
	if (_r_str_isempty (path) || !_r_fs_exists (path))
		return false;

	rstring section_ptr;

	size_t delimeter_pos;

	// get section names
	size_t alloc_length = 2048;
	size_t out_length = GetPrivateProfileSectionNames (section_ptr.GetBuffer (alloc_length), (DWORD)alloc_length, path);

	if (!out_length)
	{
		section_ptr.Release ();
		return false;
	}

	section_ptr.SetLength (out_length);

	LPCWSTR section_name = section_ptr.GetString ();

	alloc_length = 0x00007FFF; // maximum length for GetPrivateProfileSection
	LPWSTR value_buff = new WCHAR[alloc_length];

	// get section values
	while (!_r_str_isempty (section_name))
	{
		if (psections)
			psections->push_back (section_name);

		out_length = GetPrivateProfileSection (section_name, value_buff, (DWORD)alloc_length, path);

		if (out_length && !_r_str_isempty (value_buff))
		{
			LPCWSTR values = value_buff;

			while (!_r_str_isempty (values))
			{
				const size_t values_length = _r_str_length (values);

				delimeter_pos = _r_str_find (values, values_length, L'=');

				if (delimeter_pos != INVALID_SIZE_T)
					pmap[section_name][_r_str_extract (values, values_length, 0, delimeter_pos)] = _r_str_extract (values, values_length, delimeter_pos + 1); // set

				values += values_length + 1; // go next item
			}
		}

		section_name += _r_str_length (section_name) + 1; // go next section
	}

	SAFE_DELETE_ARRAY (value_buff);

	return true;
}

DWORD _r_rand (DWORD min_number, DWORD max_number)
{
	static DWORD seed = 0; // save seed

	return min_number + (RtlRandomEx (&seed) % (max_number - min_number + 1));
}

bool _r_run (LPCWSTR filename, LPCWSTR cmdline, LPCWSTR dir, WORD show_state, DWORD flags)
{
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};

	RtlSecureZeroMemory (&si, sizeof (si));
	RtlSecureZeroMemory (&pi, sizeof (pi));

	bool result = false;
	si.cb = sizeof (si);

	if (show_state != SW_SHOWDEFAULT)
	{
		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = show_state;
	}

	rstring _intptr = !_r_str_isempty (cmdline) ? cmdline : filename;

	if (CreateProcess (filename, _intptr.GetBuffer (), nullptr, nullptr, FALSE, flags, nullptr, dir, &si, &pi))
	{
		result = true;

		SAFE_DELETE_HANDLE (pi.hThread);
		SAFE_DELETE_HANDLE (pi.hProcess);
	}

	_intptr.Release ();

	return result;
}

void _r_sleep (LONG64 milliseconds)
{
	if (!milliseconds || milliseconds == INFINITE)
		return;

	constexpr LONG64 tps = ((1LL * 10LL) * 1000LL);

	LARGE_INTEGER interval;

	interval.QuadPart = -(LONG64)UInt32x32To64 (milliseconds, tps);

	NtDelayExecution (FALSE, &interval);
}

/*
	System tray
*/

bool _r_tray_create (HWND hwnd, UINT uid, UINT code, HICON hicon, LPCWSTR tooltip, bool is_hidden)
{
	NOTIFYICONDATA nid = {0};
	RtlSecureZeroMemory (&nid, sizeof (nid));

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	static bool is_vistaorlater = _r_sys_validversion (6, 0);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (code)
	{
		nid.uFlags |= NIF_MESSAGE;
		nid.uCallbackMessage = code;
	}

	if (hicon)
	{
		nid.uFlags |= NIF_ICON;
		nid.hIcon = hicon;
	}

	if (tooltip)
	{
#if defined(_APP_NO_WINXP)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // _APP_NO_WINXP

		_r_str_copy (nid.szTip, _countof (nid.szTip), tooltip);
}

	if (is_hidden)
	{
		nid.uFlags |= NIF_STATE;
		nid.dwState = NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
	}

	if (Shell_NotifyIcon (NIM_ADD, &nid))
	{
		Shell_NotifyIcon (NIM_SETVERSION, &nid);
		return true;
	}

	return false;
}

bool _r_tray_popup (HWND hwnd, UINT uid, DWORD icon_id, LPCWSTR title, LPCWSTR text)
{
	NOTIFYICONDATA nid = {0};
	RtlSecureZeroMemory (&nid, sizeof (nid));

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	static bool is_vistaorlater = _r_sys_validversion (6, 0);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.uFlags = NIF_INFO | NIF_REALTIME;
	//nid.dwInfoFlags = NIIF_LARGE_ICON | icon_id;
	nid.dwInfoFlags = icon_id;
	nid.hWnd = hwnd;
	nid.uID = uid;

	if (title)
		_r_str_copy (nid.szInfoTitle, _countof (nid.szInfoTitle), title);

	if (text)
		_r_str_copy (nid.szInfo, _countof (nid.szInfo), text);

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

bool _r_tray_setinfo (HWND hwnd, UINT uid, HICON hicon, LPCWSTR tooltip)
{
	NOTIFYICONDATA nid = {0};
	RtlSecureZeroMemory (&nid, sizeof (nid));

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	static bool is_vistaorlater = _r_sys_validversion (6, 0);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (hicon)
	{
		nid.uFlags |= NIF_ICON;
		nid.hIcon = hicon;
	}

	if (tooltip)
	{
#if defined(_APP_NO_WINXP)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // _APP_NO_WINXP

		_r_str_copy (nid.szTip, _countof (nid.szTip), tooltip);
}

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

bool _r_tray_toggle (HWND hwnd, UINT uid, bool is_show)
{
	NOTIFYICONDATA nid = {0};
	RtlSecureZeroMemory (&nid, sizeof (nid));

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	static bool is_vistaorlater = _r_sys_validversion (6, 0);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.uFlags = NIF_STATE;
	nid.hWnd = hwnd;
	nid.uID = uid;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

bool _r_tray_destroy (HWND hwnd, UINT uid)
{
	NOTIFYICONDATA nid = {0};
	RtlSecureZeroMemory (&nid, sizeof (nid));

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	static bool is_vistaorlater = _r_sys_validversion (6, 0);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;

	return !!Shell_NotifyIcon (NIM_DELETE, &nid);
}

/*
	Control: common
*/

INT _r_ctrl_isradiobuttonchecked (HWND hwnd, INT start_id, INT end_id)
{
	for (INT i = start_id; i <= end_id; i++)
	{
		if (IsDlgButtonChecked (hwnd, i) == BST_CHECKED)
			return i;
	}

	return 0;
}

rstring _r_ctrl_gettext (HWND hwnd, INT ctrl_id)
{
	INT length = (INT)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);

	if (length)
	{
		rstring result;

		if (!GetDlgItemText (hwnd, ctrl_id, result.GetBuffer (length), length + 1))
			result.Release ();

		return result;
	}

	return nullptr;
}

void _r_ctrl_settext (HWND hwnd, INT ctrl_id, LPCWSTR text, ...)
{
	rstring buffer;

	va_list args;
	va_start (args, text);

	buffer.FormatV (text, args);
	SetDlgItemText (hwnd, ctrl_id, buffer);

	va_end (args);
}

void _r_ctrl_setbuttonmargins (HWND hwnd, INT ctrl_id)
{
	// set button text margin
	{
		RECT rc = {0};

		rc.left = rc.right = _r_dc_getdpi (hwnd, 4);

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
	}

	// set button split margin
	if ((GetWindowLongPtr (GetDlgItem (hwnd, ctrl_id), GWL_STYLE) & BS_SPLITBUTTON) != 0)
	{
		BUTTON_SPLITINFO bsi = {0};

		bsi.mask = BCSIF_SIZE;

		bsi.size.cx = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON) + _r_dc_getdpi (hwnd, 2);
		bsi.size.cy = 0;

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETSPLITINFO, 0, (LPARAM)&bsi);
	}
}

void _r_ctrl_settabletext (HWND hwnd, INT ctrl_id1, LPCWSTR text1, INT ctrl_id2, LPCWSTR text2)
{
	RECT rc_wnd = {0};
	RECT rc_ctrl = {0};

	const HWND hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	const HWND hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	const HDC hdc1 = GetDC (hctrl1);
	const HDC hdc2 = GetDC (hctrl2);

	if (hdc1)
		SelectObject (hdc1, (HFONT)SendMessage (hctrl1, WM_GETFONT, 0, 0)); // fix

	if (hdc2)
		SelectObject (hdc2, (HFONT)SendMessage (hctrl2, WM_GETFONT, 0, 0)); // fix

	GetClientRect (hwnd, &rc_wnd);
	GetWindowRect (hctrl1, &rc_ctrl);

	MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)&rc_ctrl, 2);

	const INT wnd_spacing = rc_ctrl.left;
	const INT wnd_width = _R_RECT_WIDTH (&rc_wnd) - (wnd_spacing * 2);

	INT ctrl1_width = _r_dc_fontwidth (hdc1, text1, _r_str_length (text1)) + wnd_spacing;
	INT ctrl2_width = _r_dc_fontwidth (hdc2, text2, _r_str_length (text2)) + wnd_spacing;

	ctrl2_width = (std::min) (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);
	ctrl1_width = (std::min) (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing); // note: changed order for correct priority!

	SetWindowText (hctrl1, text1);
	SetWindowText (hctrl2, text2);

	HDWP hdefer = BeginDeferWindowPos (2);

	_r_wnd_resize (&hdefer, hctrl1, nullptr, wnd_spacing, rc_ctrl.top, ctrl1_width, _R_RECT_HEIGHT (&rc_ctrl), SWP_FRAMECHANGED);
	_r_wnd_resize (&hdefer, hctrl2, nullptr, wnd_width - ctrl2_width, rc_ctrl.top, ctrl2_width + wnd_spacing, _R_RECT_HEIGHT (&rc_ctrl), SWP_FRAMECHANGED);

	EndDeferWindowPos (hdefer);

	if (hdc1)
		ReleaseDC (hctrl1, hdc1);

	if (hdc2)
		ReleaseDC (hctrl2, hdc2);
}

HWND _r_ctrl_createtip (HWND hparent)
{
	HWND htip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_CHILD | WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hparent, nullptr, GetModuleHandle (nullptr), nullptr);

	if (htip)
	{
		_r_ctrl_settipstyle (htip);

		SendMessage (htip, TTM_ACTIVATE, TRUE, 0);
	}

	return htip;
}

void _r_ctrl_settip (HWND htip, HWND hparent, INT ctrl_id, LPCWSTR text)
{
	if (!htip)
		return;

	TOOLINFO ti = {0};

	ti.cbSize = sizeof (ti);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = hparent;
	ti.hinst = GetModuleHandle (nullptr);
	ti.uId = reinterpret_cast<UINT_PTR>(GetDlgItem (hparent, ctrl_id));
	ti.lpszText = const_cast<LPWSTR>(text);

	GetClientRect (hparent, &ti.rect);

	SendMessage (htip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

void _r_ctrl_settipstyle (HWND htip)
{
	if (!htip)
		return;

	SendMessage (htip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);
	SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

	_r_wnd_top (htip, true); // HACK!!!
}

void _r_ctrl_showtip (HWND hwnd, INT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR text)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon_id;

	SendDlgItemMessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

/*
	Control: tab
*/

INT _r_tab_additem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = const_cast<LPWSTR>(text);
	}

	if (image != INVALID_INT)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image;
	}

	if (lparam)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_INSERTITEM, (WPARAM)index, (LPARAM)&tci);
}

LPARAM _r_tab_getlparam (HWND hwnd, INT ctrl_id, INT index)
{
	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEM, (WPARAM)index, (LPARAM)&tci);

	return tci.lParam;
}

INT _r_tab_setitem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = const_cast<LPWSTR>(text);
	}

	if (image != INVALID_INT)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image;
	}

	if (lparam)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_SETITEM, (WPARAM)index, (LPARAM)&tci);
}

void _r_tab_selectitem (HWND hwnd, INT ctrl_id, INT index)
{
	NMHDR hdr = {0};

	hdr.hwndFrom = GetDlgItem (hwnd, ctrl_id);
	hdr.idFrom = ctrl_id;

	hdr.code = TCN_SELCHANGING;
	SendMessage (hwnd, WM_NOTIFY, 0, (LPARAM)&hdr);

	SendDlgItemMessage (hwnd, ctrl_id, TCM_SETCURSEL, (WPARAM)index, 0);

	hdr.code = TCN_SELCHANGE;
	SendMessage (hwnd, WM_NOTIFY, 0, (LPARAM)&hdr);
}

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, INT ctrl_id, INT column_id, LPCWSTR title, INT width, INT fmt)
{
	LVCOLUMN lvc = {0};

	lvc.mask = LVCF_SUBITEM;
	lvc.iSubItem = column_id;

	if (title)
	{
		lvc.mask |= LVCF_TEXT;
		lvc.pszText = const_cast<LPWSTR>(title);
	}

	if (width)
	{
		if (width < 0 && (width != LVSCW_AUTOSIZE && width != LVSCW_AUTOSIZE_USEHEADER))
		{
			RECT rc = {0};
			GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

			width = -width;
			width = (INT)_R_PERCENT_VAL (width, _R_RECT_WIDTH (&rc));
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	if (fmt)
	{
		lvc.mask |= LVCF_FMT;
		lvc.fmt = fmt;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

INT _r_listview_addgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT align, UINT state)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (lvg);
	lvg.mask = LVGF_GROUPID;
	lvg.iGroupId = group_id;

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = const_cast<LPWSTR>(title);
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
		lvg.stateMask = state;
	}

	if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_ISGROUPVIEWENABLED, 0, 0))
		SendDlgItemMessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, TRUE, 0);

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTGROUP, (WPARAM)group_id, (LPARAM)&lvg);
}

INT _r_listview_additem (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam)
{
	if (item == INVALID_INT)
	{
		item = _r_listview_getitemcount (hwnd, ctrl_id);

		if (subitem)
			item -= 1;
	}

	LVITEM lvi = {0};

	lvi.iItem = item;
	lvi.iSubItem = subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = const_cast<LPWSTR>(text);
	}

	if (!subitem)
	{
		if (image != INVALID_INT)
		{
			lvi.mask |= LVIF_IMAGE;
			lvi.iImage = image;
		}

		if (group_id != INVALID_INT)
		{
			lvi.mask |= LVIF_GROUPID;
			lvi.iGroupId = group_id;
		}

		if (lparam)
		{
			lvi.mask |= LVIF_PARAM;
			lvi.lParam = lparam;
		}
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

void _r_listview_deleteallcolumns (HWND hwnd, INT ctrl_id)
{
	const INT column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	for (INT i = column_count; i >= 0; i--)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
}

void _r_listview_deleteallgroups (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
	SendDlgItemMessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, FALSE, 0);
}

void _r_listview_deleteallitems (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

INT _r_listview_getcolumncount (HWND hwnd, INT ctrl_id)
{
	const HWND hheader = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	return (INT)SendMessage (hheader, HDM_GETITEMCOUNT, 0, 0);
}

rstring _r_listview_getcolumntext (HWND hwnd, INT ctrl_id, INT column_id)
{
	LVCOLUMN lvc = {0};

	WCHAR buffer[MAX_PATH] = {0};

	lvc.mask = LVCF_TEXT;
	lvc.pszText = buffer;
	lvc.cchTextMax = _countof (buffer);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);

	return buffer;
}

INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl_id, INT column_id)
{
	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

	return (INT)_R_PERCENT_OF ((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0), _R_RECT_WIDTH (&rc));
}

INT _r_listview_getitemcount (HWND hwnd, INT ctrl_id, bool list_checked)
{
	const INT total_count = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, 0);

	if (list_checked)
	{
		INT checks_count = 0;

		for (INT i = 0; i < total_count; i++)
		{
			if (_r_listview_isitemchecked (hwnd, ctrl_id, i))
				checks_count += 1;
		}

		return checks_count;
	}

	return total_count;
}

LPARAM _r_listview_getitemlparam (HWND hwnd, INT ctrl_id, INT item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

rstring _r_listview_getitemtext (HWND hwnd, INT ctrl_id, INT item, INT subitem)
{
	rstring result;

	INT alloc_length = 256;
	INT out_length = alloc_length;

	LVITEM lvi = {0};

	while (out_length >= alloc_length)
	{
		alloc_length += 256;

		lvi.iSubItem = subitem;
		lvi.pszText = result.GetBuffer (alloc_length);
		lvi.cchTextMax = alloc_length + 1;

		out_length = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMTEXT, (WPARAM)item, (LPARAM)&lvi);

		if (!out_length)
		{
			result.Release ();
			break;
		}

		result.SetLength (out_length);
	}

	return result;
}

bool _r_listview_isitemchecked (HWND hwnd, INT ctrl_id, INT item)
{
	return !!(((SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item, LVIS_STATEIMAGEMASK)) >> 12) - 1);
}

bool _r_listview_isitemvisible (HWND hwnd, INT ctrl_id, INT item)
{
	return !!(SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, (WPARAM)item, 0));
}

void _r_listview_redraw (HWND hwnd, INT ctrl_id, INT start_id, INT end_id)
{
	if (start_id != INVALID_INT && end_id != INVALID_INT)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, (WPARAM)start_id, (LPARAM)end_id);
	}
	else
	{
		const INT total_count = _r_listview_getitemcount (hwnd, ctrl_id);

		for (INT i = 0; i < total_count; i++)
		{
			if (_r_listview_isitemvisible (hwnd, ctrl_id, i))
				SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, (WPARAM)i, (LPARAM)i);
		}
	}
}

void _r_listview_setcolumn (HWND hwnd, INT ctrl_id, INT column_id, LPCWSTR text, INT width)
{
	LVCOLUMN lvc = {0};

	if (text)
	{
		lvc.mask |= LVCF_TEXT;
		lvc.pszText = const_cast<LPWSTR>(text);
	}

	if (width)
	{
		if (width < 0 && width != LVSCW_AUTOSIZE && width != LVSCW_AUTOSIZE_USEHEADER)
		{
			RECT rc = {0};
			GetClientRect (GetDlgItem (hwnd, ctrl_id), &rc);

			width = -width;
			width = (INT)_R_PERCENT_VAL (width, _R_RECT_WIDTH (&rc));
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

void _r_listview_setcolumnsortindex (HWND hwnd, INT ctrl_id, INT column_id, INT arrow)
{
	HWND hheader = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (hheader)
	{
		HDITEM hitem = {0};

		hitem.mask = HDI_FORMAT;

		if (Header_GetItem (hheader, column_id, &hitem))
		{
			if (arrow == 1)
				hitem.fmt = (hitem.fmt & ~HDF_SORTDOWN) | HDF_SORTUP;

			else if (arrow == -1)
				hitem.fmt = (hitem.fmt & ~HDF_SORTUP) | HDF_SORTDOWN;

			else
				hitem.fmt = hitem.fmt & ~(HDF_SORTDOWN | HDF_SORTUP);

			Header_SetItem (hheader, column_id, &hitem);
		}
	}
}

void _r_listview_setitem (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam)
{
	LVITEM lvi = {0};

	lvi.iItem = item;
	lvi.iSubItem = subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = const_cast<LPWSTR>(text);
	}

	if (!lvi.iSubItem && image != INVALID_INT)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = image;
	}

	if (!lvi.iSubItem && group_id != INVALID_INT)
	{
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = group_id;
	}

	if (!lvi.iSubItem && lparam)
	{
		lvi.mask |= LVIF_PARAM;
		lvi.lParam = lparam;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEM, 0, (LPARAM)&lvi);
}

void _r_listview_setitemcheck (HWND hwnd, INT ctrl_id, INT item, bool state)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (state ? 2 : 1);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item, (LPARAM)&lvi);
}

void _r_listview_setgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT state, UINT state_mask)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (lvg);

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = const_cast<LPWSTR>(title);
	}

	if (state || state_mask)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
		lvg.stateMask = state_mask;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETGROUPINFO, (WPARAM)group_id, (LPARAM)&lvg);
}

void _r_listview_setstyle (HWND hwnd, INT ctrl_id, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", nullptr);

	_r_ctrl_settipstyle ((HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETTOOLTIPS, 0, 0));

	if (exstyle)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl_id, LPCWSTR text, HTREEITEM hparent, INT image, LPARAM lparam)
{
	TVINSERTSTRUCT tvi = {0};

	tvi.itemex.mask = TVIF_STATE;
	tvi.itemex.state = TVIS_EXPANDED;
	tvi.itemex.stateMask = TVIS_EXPANDED;

	if (text)
	{
		tvi.itemex.mask |= TVIF_TEXT;
		tvi.itemex.pszText = const_cast<LPWSTR>(text);
	}

	if (hparent)
		tvi.hParent = hparent;

	if (image != I_IMAGENONE)
	{
		tvi.itemex.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.itemex.iImage = image;
		tvi.itemex.iSelectedImage = image;
	}

	if (lparam)
	{
		tvi.itemex.mask |= TVIF_PARAM;
		tvi.itemex.lParam = lparam;
	}

	return (HTREEITEM)SendDlgItemMessage (hwnd, ctrl_id, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getlparam (HWND hwnd, INT ctrl_id, HTREEITEM hitem)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = hitem;

	SendDlgItemMessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

void _r_treeview_setitem (HWND hwnd, INT ctrl_id, HTREEITEM hitem, LPCWSTR text, INT image, LPARAM lparam)
{
	TVITEMEX tvi = {0};

	tvi.hItem = hitem;

	if (text)
	{
		tvi.mask |= TVIF_TEXT;
		tvi.pszText = const_cast<LPWSTR>(text);
	}

	if (image != I_IMAGENONE)
	{
		tvi.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.iImage = image;
		tvi.iSelectedImage = image;
	}

	if (lparam)
	{
		tvi.mask |= TVIF_PARAM;
		tvi.lParam = lparam;
	}

	SendDlgItemMessage (hwnd, ctrl_id, TVM_SETITEM, 0, (LPARAM)&tvi);
}

void _r_treeview_setstyle (HWND hwnd, INT ctrl_id, DWORD exstyle, INT height)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", nullptr);

	_r_ctrl_settipstyle ((HWND)SendDlgItemMessage (hwnd, ctrl_id, TVM_GETTOOLTIPS, 0, 0));

	if (exstyle)
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);

	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETITEMHEIGHT, (WPARAM)height, 0);
}

/*
	Control: statusbar
*/

void _r_status_settext (HWND hwnd, INT ctrl_id, INT part, LPCWSTR text)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTIPTEXT, (WPARAM)part, (LPARAM)text);
}

void _r_status_setstyle (HWND hwnd, INT ctrl_id, INT height)
{
	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, SB_SETMINHEIGHT, (WPARAM)height, 0);

	SendDlgItemMessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

/*
	Control: toolbar
*/

void _r_toolbar_addbutton (HWND hwnd, INT ctrl_id, UINT command_id, INT style, INT_PTR text, INT state, INT image)
{
	TBBUTTON tbi = {0};

	tbi.idCommand = command_id;
	tbi.fsStyle = (BYTE)style;
	tbi.iString = text;
	tbi.fsState = (BYTE)state;
	tbi.iBitmap = image;

	INT item = (INT)SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONCOUNT, 0, 0);

	SendDlgItemMessage (hwnd, ctrl_id, TB_INSERTBUTTON, (WPARAM)item, (LPARAM)&tbi);
}

INT _r_toolbar_getwidth (HWND hwnd, INT ctrl_id)
{
	RECT rc;
	INT width = 0;

	for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONCOUNT, 0, 0); i++)
	{
		SendDlgItemMessage (hwnd, ctrl_id, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&rc);
		width += _R_RECT_WIDTH (&rc);
	}

	return width;
}

void _r_toolbar_setbutton (HWND hwnd, INT ctrl_id, UINT command_id, LPCWSTR text, INT style, INT state, INT image)
{
	TBBUTTONINFO tbi = {0};

	tbi.cbSize = sizeof (tbi);

	if (style)
	{
		tbi.dwMask |= TBIF_STYLE;
		tbi.fsStyle = (BYTE)style;
	}

	if (text)
	{
		tbi.dwMask |= TBIF_TEXT;
		tbi.pszText = const_cast<LPWSTR>(text);
	}

	if (state)
	{
		tbi.dwMask |= TBIF_STATE;
		tbi.fsState = (BYTE)state;
	}

	if (image != I_IMAGENONE)
	{
		tbi.dwMask |= TBIF_IMAGE;
		tbi.iImage = image;
	}

	SendDlgItemMessage (hwnd, ctrl_id, TB_SETBUTTONINFO, (WPARAM)command_id, (LPARAM)&tbi);
}

void _r_toolbar_setstyle (HWND hwnd, INT ctrl_id, DWORD exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", nullptr);

	SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);
	SendDlgItemMessage (hwnd, ctrl_id, TB_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);

	_r_ctrl_settipstyle ((HWND)SendDlgItemMessage (hwnd, ctrl_id, TB_GETTOOLTIPS, 0, 0));
}

/*
	Control: progress bar
*/

void _r_progress_setmarquee (HWND hwnd, INT ctrl_id, BOOL is_enable)
{
	SendDlgItemMessage (hwnd, ctrl_id, PBM_SETMARQUEE, (WPARAM)is_enable, (LPARAM)10);

	_r_wnd_addstyle (hwnd, ctrl_id, is_enable ? PBS_MARQUEE : 0, PBS_MARQUEE, GWL_STYLE);
}
