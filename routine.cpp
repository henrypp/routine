// routine++
// Copyright (c) 2012-2020 Henry++

#include "routine.hpp"

/*
	Debugging
*/

VOID _r_dbg_print (LPCWSTR string)
{
	if (!string)
		return;

	OutputDebugString (string);
}

VOID _r_dbg_print_v (LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_dbg_print (string->Buffer);

	_r_obj_dereference (string);
}

/*
	Format strings, dates, numbers
*/

PR_STRING _r_format_string (LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return NULL;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	return string;
}

PR_STRING _r_format_string_v (LPCWSTR format, va_list argPtr)
{
	INT length;
	PR_STRING string;

	if (!format)
		return NULL;

	length = _vscwprintf (format, argPtr);

	if (length == 0 || length == -1)
		return NULL;

	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (string->Buffer, length, format, argPtr);
#pragma warning(pop)

	return string;
}

VOID _r_format_bytesize64 (LPWSTR buffer, UINT bufferSize, ULONG64 bytes)
{
#if defined(_APP_NO_DEPRECATIONS)
	if (SUCCEEDED (StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, bufferSize))) // vista (sp1)+
		return;
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		HMODULE hshlwapi = GetModuleHandle (L"shlwapi.dll");

		if (hshlwapi)
		{
			typedef HRESULT (WINAPI *SFBSE)(ULONG64 ull, SFBS_FLAGS flags, PWSTR pszBuf, INT cchBuf); // vista+
			const SFBSE _StrFormatByteSizeEx = (SFBSE)GetProcAddress (hshlwapi, "StrFormatByteSizeEx");

			if (_StrFormatByteSizeEx)
			{
				if (SUCCEEDED (_StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, bufferSize))) // vista (sp1)+
					return;
			}
		}
	}

	if (StrFormatByteSizeW ((LONG64)bytes, buffer, bufferSize)) // fallback
		return;
#endif // _APP_NO_DEPRECATIONS

	*buffer = UNICODE_NULL;
}

VOID _r_format_dateex (LPWSTR buffer, UINT bufferSize, LPFILETIME lpft, ULONG flags)
{
	if (!buffer || !bufferSize)
		return;

	if (!SHFormatDateTime (lpft, &flags, buffer, bufferSize))
		*buffer = UNICODE_NULL;
}

VOID _r_format_dateex (LPWSTR buffer, UINT bufferSize, time_t ut, ULONG flags)
{
	if (!buffer || !bufferSize)
		return;

	FILETIME ft;
	_r_unixtime_to_filetime (ut, &ft);

	_r_format_dateex (buffer, bufferSize, &ft, flags);
}

VOID _r_format_interval (LPWSTR buffer, UINT bufferSize, LONG64 seconds, INT digits)
{
	if (!buffer || !bufferSize)
		return;

	if (!StrFromTimeInterval (buffer, bufferSize, _r_calc_seconds2milliseconds (ULONG, seconds), digits))
		_r_str_fromlong64 (buffer, bufferSize, seconds);
}

VOID _r_format_number (LPWSTR buffer, UINT bufferSize, LONG64 number, UINT fractional_digits, BOOLEAN is_groupdigits)
{
	if (!buffer || !bufferSize)
		return;

	static BOOLEAN is_initialized = FALSE;

	static WCHAR thousandSeparator[4];
	static WCHAR decimalSeparator[4];

	NUMBERFMT numberFormat;
	WCHAR numberString[128];

	if (!is_initialized)
	{
		if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimalSeparator, RTL_NUMBER_OF (decimalSeparator)))
		{
			decimalSeparator[0] = L'.';
			decimalSeparator[1] = UNICODE_NULL;
		}

		if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousandSeparator, RTL_NUMBER_OF (thousandSeparator)))
		{
			thousandSeparator[0] = L',';
			thousandSeparator[1] = UNICODE_NULL;
		}

		is_initialized = TRUE;
	}

	numberFormat.NumDigits = fractional_digits;
	numberFormat.Grouping = is_groupdigits ? 3 : 0;
	numberFormat.lpDecimalSep = decimalSeparator;
	numberFormat.lpThousandSep = thousandSeparator;
	numberFormat.NegativeOrder = 1;

	_r_str_fromlong64 (numberString, RTL_NUMBER_OF (numberString), number);

	if (!GetNumberFormat (LOCALE_USER_DEFAULT, 0, numberString, &numberFormat, buffer, bufferSize))
		_r_str_copy (buffer, bufferSize, numberString);
}

/*
	FastLock is a port of FastResourceLock from PH 1.x.
	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.
	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.
	https://github.com/processhacker2/processhacker
*/

#if !defined(_APP_NO_DEPRECATIONS)
VOID _r_fastlock_initialize (PR_FASTLOCK plock)
{
	plock->Value = 0;

	NtCreateSemaphore (&plock->ExclusiveWakeEvent, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);
	NtCreateSemaphore (&plock->SharedWakeEvent, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);
}

VOID _r_fastlock_acquireexclusive (PR_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	static ULONG spinCount = _r_fastlock_getspincount ();

	while (TRUE)
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

VOID _r_fastlock_acquireshared (PR_FASTLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	static ULONG spinCount = _r_fastlock_getspincount ();

	while (TRUE)
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

VOID _r_fastlock_releaseexclusive (PR_FASTLOCK plock)
{
	ULONG value;

	while (TRUE)
	{
		value = plock->Value;

		if ((value >> _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT) & _R_FASTLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (InterlockedCompareExchange (&plock->Value, value - _R_FASTLOCK_OWNED + _R_FASTLOCK_EXCLUSIVE_WAKING - _R_FASTLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				NtReleaseSemaphore (plock->ExclusiveWakeEvent, 1, NULL);
				break;
			}
		}
		else
		{
			ULONG sharedWaiters = (value >> _R_FASTLOCK_SHARED_WAITERS_SHIFT) & _R_FASTLOCK_SHARED_WAITERS_MASK;

			if (InterlockedCompareExchange (&plock->Value, value & ~(_R_FASTLOCK_OWNED | (_R_FASTLOCK_SHARED_WAITERS_MASK << _R_FASTLOCK_SHARED_WAITERS_SHIFT)), value) == value)
			{
				if (sharedWaiters)
					NtReleaseSemaphore (plock->SharedWakeEvent, sharedWaiters, NULL);

				break;
			}
		}

		YieldProcessor ();
	}
}

VOID _r_fastlock_releaseshared (PR_FASTLOCK plock)
{
	ULONG value;

	while (TRUE)
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
				NtReleaseSemaphore (plock->ExclusiveWakeEvent, 1, NULL);
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

BOOLEAN _r_fastlock_tryacquireexclusive (PR_FASTLOCK plock)
{
	ULONG value = plock->Value;

	if (value & (_R_FASTLOCK_OWNED | _R_FASTLOCK_EXCLUSIVE_WAKING))
		return FALSE;

	return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED, value) == value;
}

BOOLEAN _r_fastlock_tryacquireshared (PR_FASTLOCK plock)
{
	ULONG value = plock->Value;

	if (value & _R_FASTLOCK_EXCLUSIVE_MASK)
		return FALSE;

	if (!(value & _R_FASTLOCK_OWNED))
	{
		return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_OWNED + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value;
	}
	else if ((value >> _R_FASTLOCK_SHARED_OWNERS_SHIFT) & _R_FASTLOCK_SHARED_OWNERS_MASK)
	{
		return InterlockedCompareExchange (&plock->Value, value + _R_FASTLOCK_SHARED_OWNERS_INC, value) == value;
	}

	return FALSE;
}
#endif // !_APP_NO_DEPRECATIONS

/*
	Memory allocation reference
*/

HANDLE _r_mem_getheap ()
{
	static HANDLE heapHandle = NULL;
	HANDLE currentHeap = InterlockedCompareExchangePointer (&heapHandle, NULL, NULL);

	if (!currentHeap)
	{
		HANDLE newHandle = NULL;

		if (_r_sys_isosversiongreaterorequal (WINDOWS_8)) // win8+
			newHandle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1 | HEAP_CREATE_SEGMENT_HEAP, NULL, 0, 0, NULL, NULL);

		if (!newHandle)
			newHandle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1, NULL, _r_calc_megabytes2bytes (SIZE_T, 2), _r_calc_megabytes2bytes (SIZE_T, 1), NULL, NULL);

		if (newHandle)
		{
			ULONG hci = HEAP_COMPATIBILITY_LFH;
			RtlSetHeapInformation (newHandle, HeapCompatibilityInformation, &hci, sizeof (hci));

			currentHeap = InterlockedCompareExchangePointer (&heapHandle, newHandle, NULL);

			if (!currentHeap)
			{
				currentHeap = newHandle;
			}
			else
			{
				RtlDestroyHeap (newHandle);
			}
		}
	}

	return currentHeap;
}

PVOID _r_mem_allocateex (SIZE_T bytes_count, ULONG flags)
{
	HANDLE heapHandle = _r_mem_getheap ();

	return RtlAllocateHeap (heapHandle, flags, bytes_count);
}

PVOID _r_mem_reallocateex (PVOID heapMemory, SIZE_T bytes_count, ULONG flags)
{
	// If RtlReAllocateHeap fails, the original memory is not freed, and the original handle and pointer are still valid.
	// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc

	HANDLE heapHandle = _r_mem_getheap ();

	PVOID originalMemory = heapMemory;
	PVOID allocatedMemory = RtlReAllocateHeap (heapHandle, flags, heapMemory, bytes_count);

	if (allocatedMemory)
		return allocatedMemory;

	return originalMemory;
}

VOID _r_mem_free (PVOID heapMemory)
{
	HANDLE heapHandle = _r_mem_getheap ();

	RtlFreeHeap (heapHandle, 0, heapMemory);
}

/*
	Objects reference
*/

PVOID _r_obj_allocateex (SIZE_T size, _R_CALLBACK_OBJECT_CLEANUP CleanupCallback)
{
	PR_OBJECT_HEADER objectHeader = (PR_OBJECT_HEADER)_r_mem_allocatezero (UFIELD_OFFSET (R_OBJECT_HEADER, Body) + size);

	InterlockedIncrement (&objectHeader->RefCount);

	objectHeader->CleanupCallback = CleanupCallback;

	return ObjectHeaderToObject (objectHeader);
}

PVOID _r_obj_reference (PVOID pdata)
{
	PR_OBJECT_HEADER objectHeader = ObjectToObjectHeader (pdata);

	InterlockedIncrement (&objectHeader->RefCount);

	return pdata;
}

VOID _r_obj_dereferenceex (PVOID pdata, LONG ref_count)
{
	assert (!(ref_count < 0));

	PR_OBJECT_HEADER objectHeader = ObjectToObjectHeader (pdata);

	LONG old_count = InterlockedExchangeAdd (&objectHeader->RefCount, -ref_count);
	LONG new_count = old_count - ref_count;

	if (new_count == 0)
	{
		if (objectHeader->CleanupCallback)
			objectHeader->CleanupCallback (pdata);

		_r_mem_free (objectHeader);
	}
	else if (new_count < 0)
	{
		RtlRaiseStatus (STATUS_INVALID_PARAMETER);
	}
}

PR_BYTE _r_obj_createbyteex (LPSTR buffer, SIZE_T length)
{
	PR_BYTE string = (PR_BYTE)_r_obj_allocate (UFIELD_OFFSET (R_BYTE, Data) + length + sizeof (ANSI_NULL));

	string->Length = length;
	string->Buffer = string->Data;

	if (!_r_str_isempty (buffer))
	{
		RtlCopyMemory (string->Buffer, buffer, length);
		*(PCHAR)PTR_ADD_OFFSET (string->Buffer, string->Length) = ANSI_NULL; // terminate
	}
	else
	{
		string->Buffer[0] = ANSI_NULL;
	}

	return string;
}

PR_STRING _r_obj_createstringex (LPCWSTR buffer, SIZE_T length)
{
	assert (!(length & 0x01));

	PR_STRING string = (PR_STRING)_r_obj_allocate (UFIELD_OFFSET (R_STRING, Data) + length + sizeof (UNICODE_NULL));

	string->Length = length;
	string->Buffer = string->Data;

	if (!_r_str_isempty (buffer))
	{
		RtlCopyMemory (string->Buffer, buffer, length);
		_r_string_writenullterminator (string);
	}
	else
	{
		string->Buffer[0] = UNICODE_NULL;
	}

	return string;
}

VOID _r_string_appendex (PR_STRINGBUILDER string, LPCWSTR text, SIZE_T length)
{
	if (length == 0)
		return;

	if (string->AllocatedLength < string->String->Length + length)
		_r_string_resizestring (string, string->String->Length + length);

	if (!_r_str_isempty (text))
	{
		RtlCopyMemory (PTR_ADD_OFFSET (string->String->Buffer, string->String->Length), text, length);
	}

	string->String->Length += length;
	_r_string_writenullterminator (string->String);
}

VOID _r_string_appendformat_v (PR_STRINGBUILDER string, LPCWSTR format, va_list argPtr)
{
	SIZE_T lengthInBytes;
	INT length;

	if (!format)
		return;

	length = _vscwprintf (format, argPtr);

	if (length == 0 || length == -1)
		return;

	lengthInBytes = length * sizeof (WCHAR);

	if (string->AllocatedLength < string->String->Length + lengthInBytes)
		_r_string_resizestring (string, string->String->Length + lengthInBytes);

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf ((LPWSTR)PTR_ADD_OFFSET (string->String->Buffer, string->String->Length), length, format, argPtr);
#pragma warning(pop)

	string->String->Length += lengthInBytes;
	_r_string_writenullterminator (string->String);
}

VOID _r_string_insertex (PR_STRINGBUILDER string, SIZE_T index, LPCWSTR text, SIZE_T length)
{
	if (length == 0)
		return;

	if (string->AllocatedLength < string->String->Length + length)
		_r_string_resizestring (string, string->String->Length + length);

	if ((index * sizeof (WCHAR)) < string->String->Length)
		RtlMoveMemory (&string->String->Buffer[index + (length / sizeof (WCHAR))], &string->String->Buffer[index], string->String->Length - (index * sizeof (WCHAR)));

	if (!_r_str_isempty (text))
	{
		RtlCopyMemory (&string->String->Buffer[index], text, length);
	}

	string->String->Length += length;
	_r_string_writenullterminator (string->String);
}

VOID _r_string_insertformat_v (PR_STRINGBUILDER string, SIZE_T index, LPCWSTR format, va_list argPtr)
{
	SIZE_T lengthInBytes;
	INT length;

	if (!format)
		return;

	length = _vscwprintf (format, argPtr);

	if (length == 0 || length == -1)
		return;

	lengthInBytes = length * sizeof (WCHAR);

	if (string->AllocatedLength < string->String->Length + lengthInBytes)
		_r_string_resizestring (string, string->String->Length + lengthInBytes);

	if ((index * sizeof (WCHAR)) < string->String->Length)
		RtlMoveMemory (&string->String->Buffer[index + length], &string->String->Buffer[index], string->String->Length - (index * sizeof (WCHAR)));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (&string->String->Buffer[index], length, format, argPtr);
#pragma warning(pop)

	string->String->Length += lengthInBytes;
	_r_string_writenullterminator (string->String);
}

VOID _r_string_resizestring (PR_STRINGBUILDER string, SIZE_T newCapacity)
{
	PR_STRING newString;
	SIZE_T newSize;

	newSize = string->AllocatedLength * 2;

	if (newCapacity & 0x01)
		newCapacity += 1;

	if (newSize < newCapacity)
		newSize = newCapacity;

	newString = _r_obj_createstringex (NULL, newSize);

	RtlCopyMemory (newString->Buffer, string->String->Buffer, string->String->Length + sizeof (UNICODE_NULL));

	newString->Length = string->String->Length;

	_r_obj_movereference (&string->String, newString);
}

VOID _r_string_remove (PR_STRING string, SIZE_T start_pos, SIZE_T length)
{
	RtlMoveMemory (&string->Buffer[start_pos], &string->Buffer[start_pos + length], string->Length - (length + start_pos) * sizeof (WCHAR));
	string->Length -= (length * sizeof (WCHAR));

	_r_string_writenullterminator (string);
}

VOID _r_string_setsize (PR_STRING string, SIZE_T length)
{
	if (string->Length <= length)
		return;

	if (length & 0x01)
		length += 1;

	string->Length = length;
	_r_string_writenullterminator (string);
}

/*
	System messages
*/

BOOLEAN _r_msg_taskdialog (const TASKDIALOGCONFIG* ptd, PINT pbutton, PINT pradiobutton, LPBOOL pis_flagchecked)
{
#if defined(_APP_NO_DEPRECATIONS)
	return SUCCEEDED (TaskDialogIndirect (ptd, pbutton, pradiobutton, pis_flagchecked));
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		HMODULE hcomctl32 = GetModuleHandle (L"comctl32.dll");

		if (hcomctl32)
		{
			typedef HRESULT (WINAPI *TDI)(const TASKDIALOGCONFIG *pTaskConfig, PINT pnButton, PINT pnRadioButton, LPBOOL pfVerificationFlagChecked); // vista+
			const TDI _TaskDialogIndirect = (TDI)GetProcAddress (hcomctl32, "TaskDialogIndirect");

			if (_TaskDialogIndirect)
				return SUCCEEDED (_TaskDialogIndirect (ptd, pbutton, pradiobutton, pis_flagchecked));
		}
	}

	return FALSE;
#endif // _APP_NO_DEPRECATIONS
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			BOOL is_topmost = HIWORD (lpdata);

			if (is_topmost)
				_r_wnd_top (hwnd, TRUE);

			_r_wnd_center (hwnd, GetParent (hwnd));

#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			// remove window icon
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, 0);
			SendMessage (hwnd, WM_SETICON, ICON_BIG, 0);

			break;
		}

		case TDN_HYPERLINK_CLICKED:
		{
			LPCWSTR lpszlink = (LPCWSTR)lparam;

			if (!_r_str_isempty (lpszlink))
				ShellExecute (hwnd, NULL, lpszlink, NULL, NULL, SW_SHOWNORMAL);

			break;
		}
	}

	return S_OK;
}

/*
	Clipboard operations
*/

PR_STRING _r_clipboard_get (HWND hwnd)
{
	if (OpenClipboard (hwnd))
	{
		PR_STRING string = NULL;
		HGLOBAL hmemory = GetClipboardData (CF_UNICODETEXT);

		if (hmemory)
		{
			LPCWSTR clipboardText = (LPCWSTR)GlobalLock (hmemory);

			if (clipboardText)
				string = _r_obj_createstring (clipboardText);

			GlobalUnlock (hmemory);
		}

		CloseClipboard ();

		return string;
	}

	return NULL;
}

VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length)
{
	if (_r_str_isempty (text) || !length)
		return;

	if (OpenClipboard (hwnd))
	{
		SIZE_T byte_size = length * sizeof (WCHAR);
		HGLOBAL hmemory = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, byte_size + sizeof (UNICODE_NULL));

		if (hmemory)
		{
			PVOID clipboardText = GlobalLock (hmemory);

			if (clipboardText)
			{
				RtlCopyMemory (clipboardText, text, byte_size);
				*(LPWSTR)PTR_ADD_OFFSET (clipboardText, byte_size) = UNICODE_NULL; // terminate

				GlobalUnlock (clipboardText);

				EmptyClipboard ();
				SetClipboardData (CF_UNICODETEXT, hmemory);
			}
			else
			{
				GlobalFree (hmemory);
			}
		}

		CloseClipboard ();
	}
}

/*
	Filesystem
*/

BOOLEAN _r_fs_makebackup (LPCWSTR path, time_t timestamp, BOOLEAN is_removesourcefile)
{
	if (_r_str_isempty (path) || !_r_fs_exists (path))
		return FALSE;

	PR_STRING newPath;
	BOOLEAN is_success;

	if (timestamp)
	{
		SYSTEMTIME st;
		WCHAR dateFormat[128];
		PR_STRING pathDirectory;

		_r_unixtime_to_systemtime (timestamp, &st);
		SystemTimeToTzSpecificLocalTime (NULL, &st, &st);

		GetDateFormat (LOCALE_USER_DEFAULT, 0, &st, L"yyyy-MM-dd", dateFormat, RTL_NUMBER_OF (dateFormat));

		pathDirectory = _r_path_getbasedirectory (path);

		newPath = _r_format_string (L"%s\\%s-%s.bak", _r_obj_getstringorempty (pathDirectory), dateFormat, _r_path_getbasename (path));

		if (pathDirectory)
			_r_obj_dereference (pathDirectory);
	}
	else
	{
		newPath = _r_format_string (L"%s.bak", path);
	}

	if (_r_fs_exists (newPath->Buffer))
		_r_fs_remove (newPath->Buffer, _R_FLAG_REMOVE_FORCE);

	if (is_removesourcefile)
	{
		is_success = _r_fs_move (path, newPath->Buffer, MOVEFILE_COPY_ALLOWED);

		if (is_success)
			_r_fs_remove (path, _R_FLAG_REMOVE_FORCE);
	}
	else
	{
		is_success = _r_fs_copy (path, newPath->Buffer, 0);
	}

	_r_obj_dereference (newPath);

	return is_success;
}

BOOLEAN _r_fs_mkdir (LPCWSTR path)
{
	if (SHCreateDirectoryEx (NULL, path, NULL) == ERROR_SUCCESS)
		return TRUE;

	return !!CreateDirectory (path, NULL); // fallback
}

PR_BYTE _r_fs_readfile (HANDLE hfile, ULONG filesize)
{
	HANDLE hmap = CreateFileMapping (hfile, NULL, PAGE_READONLY, 0, filesize, NULL);

	if (hmap)
	{
		PVOID buffer = MapViewOfFile (hmap, FILE_MAP_READ, 0, 0, 0);

		if (buffer)
		{
			PR_BYTE bytes = _r_obj_createbyteex ((PCHAR)buffer, filesize);

			UnmapViewOfFile (buffer);
			CloseHandle (hmap);

			return bytes;
		}

		CloseHandle (hmap);
	}

	return NULL;
}

BOOLEAN _r_fs_remove (LPCWSTR path, ULONG flags)
{
	if ((flags & _R_FLAG_REMOVE_FORCE) == _R_FLAG_REMOVE_FORCE)
		SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL);

	if ((flags & (_R_FLAG_REMOVE_USERECYCLER | _R_FLAG_REMOVE_USERECURSION)) != 0)
	{
		SHFILEOPSTRUCT shfop = {0};

		shfop.wFunc = FO_DELETE;
		shfop.pFrom = path;
		shfop.fFlags = FOF_NO_UI;

		if ((flags & _R_FLAG_REMOVE_USERECYCLER) == _R_FLAG_REMOVE_USERECYCLER)
			shfop.fFlags |= FOF_ALLOWUNDO;

		if ((flags & _R_FLAG_REMOVE_USERECURSION) == 0)
			shfop.fFlags |= FOF_NORECURSION;

		if (SHFileOperation (&shfop) == ERROR_SUCCESS)
			return TRUE;
	}

	if ((GetFileAttributes (path) & FILE_ATTRIBUTE_DIRECTORY) != 0)
		return !!RemoveDirectory (path); // delete folder (fallback!)

	return !!DeleteFile (path); // delete file (fallback!)
}

LONG64 _r_fs_size (LPCWSTR path)
{
	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		LONG64 result = _r_fs_size (hfile);
		CloseHandle (hfile);

		return result;
	}

	return 0;
}

/*
	Paths
*/

LPCWSTR _r_path_getbasename (LPCWSTR path)
{
	LPCWSTR last_slash = path;

	while (!_r_str_isempty (path))
	{
		if ((*path == OBJ_NAME_PATH_SEPARATOR || *path == L'/' || *path == L':') && path[1] && path[1] != OBJ_NAME_PATH_SEPARATOR && path[1] != L'/')
			last_slash = path + 1;

		path += 1;
	}

	return last_slash;
}

PR_STRING _r_path_getbasedirectory (LPCWSTR path)
{
	R_STRINGREF fullpathPart;
	R_STRINGREF pathPart;
	PR_STRING pathString;
	PR_STRING directoryString;

	pathString = _r_obj_createstring (path);

	_r_stringref_initialize2 (&fullpathPart, pathString);

	directoryString = _r_str_splitatlastchar (&fullpathPart, &pathPart, OBJ_NAME_PATH_SEPARATOR);

	if (directoryString)
	{
		_r_str_trim (directoryString, L"\\/");
		_r_obj_movereference (&pathString, directoryString);
	}

	return pathString;
}

LPCWSTR _r_path_getbaseextension (LPCWSTR path)
{
	LPCWSTR lastpoint = NULL;

	while (!_r_str_isempty (path))
	{
		if (*path == OBJ_NAME_PATH_SEPARATOR || *path == L' ')
			lastpoint = NULL;

		else if (*path == L'.')
			lastpoint = path;

		path += 1;
	}

	return lastpoint;
}

PR_STRING _r_path_getfullpath (LPCWSTR path)
{
	PR_STRING fullPath;
	ULONG length;
	ULONG returnLength;

	length = 0x100;
	fullPath = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	returnLength = RtlGetFullPathName_U (path, length, fullPath->Buffer, NULL);

	if (returnLength > length)
	{
		length = returnLength;

		_r_obj_movereference (&fullPath, _r_obj_createstringex (NULL, length * sizeof (WCHAR)));

		returnLength = RtlGetFullPathName_U (path, length, fullPath->Buffer, NULL);
	}

	if (returnLength)
	{
		_r_string_trimtonullterminator (fullPath);

		return fullPath;
	}

	_r_obj_dereference (fullPath);

	return NULL;
}

PR_STRING _r_path_getknownfolder (ULONG Folder, LPCWSTR append)
{
	PR_STRING path;
	SIZE_T appendPathLength;

	if (append)
		appendPathLength = _r_str_length (append) * sizeof (WCHAR);
	else
		appendPathLength = 0;

	path = _r_obj_createstringex (NULL, (0x100 * sizeof (WCHAR)) + appendPathLength);

	if (SUCCEEDED (SHGetFolderPath (NULL, Folder, NULL, SHGFP_TYPE_CURRENT, path->Buffer)))
	{
		_r_string_trimtonullterminator (path);

		if (append)
		{
			RtlCopyMemory (&path->Buffer[path->Length / sizeof (WCHAR)], append, appendPathLength + sizeof (UNICODE_NULL));
			path->Length += appendPathLength;
		}

		return path;
	}

	_r_obj_dereference (path);

	return NULL;
}

PR_STRING _r_path_getmodulepath (HMODULE hmodule)
{
	ULONG length;
	PR_STRING path;

	length = 0x100;
	path = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if (GetModuleFileName (hmodule, path->Buffer, length))
	{
		_r_string_trimtonullterminator (path);

		return path;
	}

	_r_obj_dereference (path);

	return NULL;
}

VOID _r_path_explore (LPCWSTR path)
{
	LPITEMIDLIST item;
	SFGAOF attributes;
	PR_STRING processPath;

	if (SUCCEEDED (SHParseDisplayName (path, NULL, &item, 0, &attributes)))
	{
		SHOpenFolderAndSelectItems (item, 0, NULL, 0);
		CoTaskMemFree (item);

		return;
	}

	// try using windows explorer arguments
	if (_r_fs_exists (path))
	{
		processPath = _r_format_string (L"\"explorer.exe\" /select,\"%s\"", path);

		if (processPath)
		{
			_r_sys_createprocess (NULL, processPath->Buffer, NULL);
			_r_obj_dereference (processPath);

			return;
		}
	}

	// try open file directory
	processPath = _r_path_getbasedirectory (path);

	if (processPath)
	{
		if (_r_fs_exists (processPath->Buffer))
			ShellExecute (NULL, NULL, processPath->Buffer, NULL, NULL, SW_SHOWNORMAL);

		_r_obj_dereference (processPath);
	}
}

PR_STRING _r_path_compact (LPCWSTR path, UINT length)
{
	if (_r_str_isempty (path) || !length)
		return NULL;

	PR_STRING string;

	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if (PathCompactPathEx (string->Buffer, path, length, 0))
	{
		_r_string_trimtonullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

PR_STRING _r_path_makeunique (LPCWSTR path)
{
	if (_r_str_isempty (path) || !_r_fs_exists (path))
		return NULL;

	if (_r_str_findchar (path, _r_str_length (path), OBJ_NAME_PATH_SEPARATOR) == INVALID_SIZE_T)
		return NULL;

	PR_STRING result = NULL;

	PR_STRING pathDirectory;
	PR_STRING pathFilename;
	PR_STRING pathExtension;

	pathDirectory = _r_path_getbasedirectory (path);
	pathFilename = _r_obj_createstring (_r_path_getbasename (path));
	pathExtension = _r_obj_createstring (_r_path_getbaseextension (path));

	if (!_r_str_isempty (pathFilename) && !_r_str_isempty (pathExtension))
		_r_string_setsize (pathFilename, (pathFilename->Length - pathExtension->Length));

	for (USHORT i = 1; i < (USHRT_MAX - 1); i++)
	{
		_r_obj_movereference (&result, _r_format_string (L"%s%c%s-%" TEXT (PRIu16) L"%s", _r_obj_getstringorempty (pathDirectory), OBJ_NAME_PATH_SEPARATOR, _r_obj_getstringorempty (pathFilename), i, _r_obj_getstringorempty (pathExtension)));

		if (!_r_fs_exists (result->Buffer))
			break;
	}

	SAFE_DELETE_REFERENCE (pathDirectory);
	SAFE_DELETE_REFERENCE (pathFilename);
	SAFE_DELETE_REFERENCE (pathExtension);

	return result;
}

PR_STRING _r_path_dospathfromnt (LPCWSTR path)
{
	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_r_str_compare_length (path, L"\\??\\", 4) == 0)
	{
		return _r_obj_createstring (path + 4);
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_r_str_compare_length (path, L"\\systemroot", 11) == 0)
	{
		WCHAR systemRoot[256];
		GetSystemDirectory (systemRoot, RTL_NUMBER_OF (systemRoot));

		return _r_format_string (L"%s%c%s", systemRoot, OBJ_NAME_PATH_SEPARATOR, path + 11 + 9);
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_r_str_compare_length (path, L"system32\\", 9) == 0)
	{
		WCHAR systemRoot[256];
		GetSystemDirectory (systemRoot, RTL_NUMBER_OF (systemRoot));

		return _r_format_string (L"%s%c%s", systemRoot, OBJ_NAME_PATH_SEPARATOR, path + 8);
	}
	else if (_r_str_compare_length (path, L"\\device\\", 8) == 0)
	{
		SIZE_T pathLen = _r_str_length (path);

		if (_r_str_compare_length (path, L"\\device\\mup", 11) == 0) // network share (win7+)
		{
			if (pathLen != 11 && path[11] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				return _r_format_string (L"%c%s", OBJ_NAME_PATH_SEPARATOR, path + 11);
			}
		}
		else if (_r_str_compare_length (path, L"\\device\\lanmanredirector", 24) == 0) // network share (winxp+)
		{
			if (pathLen != 24 && path[24] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				return _r_format_string (L"%c%s", OBJ_NAME_PATH_SEPARATOR, path + 24);
			}
		}

		// device name prefixes
#ifndef _WIN64
		PROCESS_DEVICEMAP_INFORMATION deviceMapInfo;
#else
		PROCESS_DEVICEMAP_INFORMATION_EX deviceMapInfo;
#endif

		RtlSecureZeroMemory (&deviceMapInfo, sizeof (deviceMapInfo));

		OBJECT_ATTRIBUTES oa;

		UNICODE_STRING devicePrefix;
		UNICODE_STRING deviceName;

		WCHAR deviceNameBuffer[7] = L"\\??\\ :";
		WCHAR devicePrefixBuff[_R_DEVICE_PREFIX_LENGTH] = {0};

		HANDLE linkhandle;

		if (NT_SUCCESS (NtQueryInformationProcess (NtCurrentProcess (), ProcessDeviceMap, &deviceMapInfo, sizeof (deviceMapInfo), NULL)))
		{
			for (ULONG i = 0; i < _R_DEVICE_COUNT; i++)
			{
				if (deviceMapInfo.Query.DriveMap)
				{
					if (!(deviceMapInfo.Query.DriveMap & (0x1 << i)))
						continue;
				}

				deviceName.Buffer = deviceNameBuffer;
				deviceName.Length = 6 * sizeof (WCHAR);

				deviceNameBuffer[4] = L'A' + (WCHAR)i;

				InitializeObjectAttributes (&oa, &deviceName, OBJ_CASE_INSENSITIVE, NULL, NULL);

				if (NT_SUCCESS (NtOpenSymbolicLinkObject (&linkhandle, SYMBOLIC_LINK_QUERY, &oa)))
				{
					devicePrefix.Length = 0;
					devicePrefix.Buffer = devicePrefixBuff;
					devicePrefix.MaximumLength = RTL_NUMBER_OF (devicePrefixBuff);

					if (NT_SUCCESS (NtQuerySymbolicLinkObject (linkhandle, &devicePrefix, NULL)))
					{
						SIZE_T prefixLen = devicePrefix.Length / sizeof (WCHAR);

						if (prefixLen)
						{
							if (_r_str_compare_length (devicePrefix.Buffer, path, prefixLen) == 0)
							{
								// To ensure we match the longest prefix, make sure the next character is a
								// backslash or the path is equal to the prefix.
								if (pathLen == prefixLen || path[prefixLen] == OBJ_NAME_PATH_SEPARATOR)
								{
									NtClose (linkhandle);

									// <letter>:path
									return _r_format_string (L"%c:%c%s", deviceNameBuffer[4], OBJ_NAME_PATH_SEPARATOR, path + prefixLen + 1);
								}
							}
						}
					}

					NtClose (linkhandle);
				}
			}
		}

		// network share prefixes
		// https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/support-for-unc-naming-and-mup

		HKEY hkey;
		PR_STRING providerOrder = NULL;

		if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			providerOrder = _r_reg_querystring (hkey, L"ProviderOrder");

			if (providerOrder)
			{
				WCHAR providerKey[256];
				PR_STRING providerPart;
				R_STRINGREF remainingPart;

				_r_stringref_initialize2 (&remainingPart, providerOrder);

				while (remainingPart.Length != 0)
				{
					providerPart = _r_str_splitatchar (&remainingPart, &remainingPart, L',');

					if (providerPart)
					{
						_r_str_printf (providerKey, RTL_NUMBER_OF (providerKey), L"System\\CurrentControlSet\\Services\\%s\\NetworkProvider", providerPart->Buffer);

						if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, providerKey, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
						{
							PR_STRING deviceName = _r_reg_querystring (hkey, L"DeviceName");

							if (deviceName)
							{
								SIZE_T prefixLen = _r_obj_getstringlength (deviceName);

								if (prefixLen)
								{
									if (_r_str_compare_length (deviceName->Buffer, path, prefixLen) == 0)
									{
										// To ensure we match the longest prefix, make sure the next character is a
										// backslash. Don't resolve if the name *is* the prefix. Otherwise, we will end
										// up with a useless string like "\".
										if (pathLen != prefixLen && path[prefixLen] == OBJ_NAME_PATH_SEPARATOR)
										{
											_r_obj_dereference (providerOrder);
											_r_obj_dereference (providerPart);
											_r_obj_dereference (deviceName);

											RegCloseKey (hkey);

											// \path
											return _r_obj_createstring (path + prefixLen);
										}
									}
								}

								_r_obj_dereference (deviceName);
							}

							RegCloseKey (hkey);
						}

						_r_obj_dereference (providerPart);
					}
				}

				_r_obj_dereference (providerOrder);
			}

			RegCloseKey (hkey);
		}
	}

	return _r_obj_createstring (path);
}

DWORD _r_path_ntpathfromdos (LPCWSTR path, PR_STRING* ntPath)
{
	NTSTATUS status;
	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (!_r_fs_isvalidhandle (hfile))
	{
		return GetLastError ();
	}
	else
	{
		ULONG attempts = 6;
		ULONG bufferSize = 0x200;

		POBJECT_NAME_INFORMATION pbuffer = (POBJECT_NAME_INFORMATION)_r_mem_allocatezero (bufferSize);

		do
		{
			status = NtQueryObject (hfile, ObjectNameInformation, pbuffer, bufferSize, &bufferSize);

			if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL)
			{
				pbuffer = (POBJECT_NAME_INFORMATION)_r_mem_reallocatezero (pbuffer, bufferSize);
			}
			else
			{
				break;
			}
		}
		while (--attempts);

		if (NT_SUCCESS (status))
		{
			PR_STRING string = _r_string_fromunicodestring (&pbuffer->Name);

			if (string)
			{
				_r_str_tolower (string->Buffer); // lower is important!

				*ntPath = string;

				status = ERROR_SUCCESS;
			}
		}

		_r_mem_free (pbuffer);

		CloseHandle (hfile);
	}

	return status;
}

/*
	Strings
*/

BOOLEAN _r_str_isnumeric (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return FALSE;

	while (*text != UNICODE_NULL)
	{
		if (iswdigit (*text) == 0)
			return FALSE;

		text += 1;
	}

	return TRUE;
}

VOID _r_str_append (LPWSTR buffer, SIZE_T length, LPCWSTR text)
{
	if (!buffer || !length)
		return;

	if (length <= _R_STR_MAX_LENGTH)
	{
		SIZE_T dest_length = _r_str_length (buffer);

		_r_str_copy (buffer + dest_length, length - dest_length, text);
	}
}

VOID _r_str_appendformat (LPWSTR buffer, SIZE_T length, LPCWSTR format, ...)
{
	if (!buffer || !length || _r_str_isempty (format) || (length > _R_STR_MAX_LENGTH))
		return;

	va_list argPtr;
	SIZE_T dest_length;

	dest_length = _r_str_length (buffer);

	va_start (argPtr, format);
	_r_str_printf_v (buffer + dest_length, length - dest_length, format, argPtr);
	va_end (argPtr);
}

VOID _r_str_copy (LPWSTR buffer, SIZE_T length, LPCWSTR text)
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

SIZE_T _r_str_length (LPCWSTR text)
{
	if (_r_str_isempty (text))
		return 0;

	if (USER_SHARED_DATA->ProcessorFeatures[PF_XMMI64_INSTRUCTIONS_AVAILABLE]) // check sse2 feature
	{
		LPWSTR p = (LPWSTR)((ULONG_PTR)text & ~0xE); // string should be 2 byte aligned
		ULONG unaligned = PtrToUlong (text) & 0xF;

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

		while (TRUE)
		{
			b = _mm_load_si128 ((__m128i*)p);
			b = _mm_cmpeq_epi16 (b, z);
			mask = _mm_movemask_epi8 (b);

			if (_BitScanForward (&index, mask))
				return (SIZE_T)(p - text) + index / sizeof (WCHAR);

			p += 0x10 / sizeof (WCHAR);
		}
	}
	else
	{
		return wcsnlen_s (text, _R_STR_MAX_LENGTH);
	}
}

VOID _r_str_printf (LPWSTR buffer, SIZE_T length, LPCWSTR format, ...)
{
	if (!buffer || !length)
		return;

	if (_r_str_isempty (format) || (length > _R_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return;
	}

	va_list argPtr;

	va_start (argPtr, format);
	_r_str_printf_v (buffer, length, format, argPtr);
	va_end (argPtr);
}

VOID _r_str_printf_v (LPWSTR buffer, SIZE_T length, LPCWSTR format, va_list argPtr)
{
	if (!buffer || !length)
		return;

	if (_r_str_isempty (format) || (length > _R_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return;
	}

	SIZE_T max_length = length - 1; // leave the last space for the null terminator

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	INT bufferSize = _vsnwprintf (buffer, max_length, format, argPtr);
#pragma warning(pop)

	if (bufferSize == -1 || (SIZE_T)bufferSize >= max_length)
		buffer[max_length] = UNICODE_NULL; // need to null terminate the string
}

SIZE_T _r_str_hash (LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0;

	SIZE_T hash = 0x811C9DC5; // FNV_offset_basis

	// FNV-1a hash
	while (*string != UNICODE_NULL)
	{
		hash ^= (SIZE_T)_r_str_upper (*string++);
		hash *= 0x01000193; // FNV_prime
	}

	return hash;
}

INT _r_str_compare (LPCWSTR string1, LPCWSTR string2)
{
	if (!string1 && !string2)
		return 0;

	if (!string1)
		return -1;

	if (!string2)
		return 1;

	return _wcsicmp (string1, string2);
}

INT _r_str_compare_length (LPCWSTR string1, LPCWSTR string2, SIZE_T length)
{
	if (!string1 && !string2)
		return 0;

	if (!string1)
		return -1;

	if (!string2)
		return 1;

	return _wcsnicmp (string1, string2, length);
}

PR_STRING _r_str_expandenvironmentstring (PR_STRING string)
{
	NTSTATUS status;
	UNICODE_STRING inputString;
	UNICODE_STRING outputString;
	PR_STRING bufferString;
	ULONG bufferLength;

	if (!_r_string_tounicodestring (string, &inputString))
		return NULL;

	bufferLength = 0x200;
	bufferString = _r_obj_createstringex (NULL, bufferLength);

	outputString.MaximumLength = (USHORT)bufferLength;
	outputString.Length = 0;
	outputString.Buffer = bufferString->Buffer;

	status = RtlExpandEnvironmentStrings_U (NULL, &inputString, &outputString, &bufferLength);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		_r_obj_movereference (&bufferString, _r_obj_createstringex (NULL, bufferLength));

		outputString.MaximumLength = (USHORT)bufferLength;
		outputString.Length = 0;
		outputString.Buffer = bufferString->Buffer;

		status = RtlExpandEnvironmentStrings_U (NULL, &inputString, &outputString, &bufferLength);
	}

	if (NT_SUCCESS (status))
	{
		bufferString->Length = outputString.Length;
		_r_string_writenullterminator (bufferString); // terminate

		return bufferString;
	}

	_r_obj_dereference (bufferString);

	return NULL;
}

PR_STRING _r_str_expandenvironmentstring (LPCWSTR string)
{
	PR_STRING result;
	PR_STRING expandedString;

	result = _r_obj_createstring (string);
	expandedString = _r_str_expandenvironmentstring (result);

	if (expandedString)
		_r_obj_movereference (&result, expandedString);

	return result;
}

PR_STRING _r_str_unexpandenvironmentstring (LPCWSTR string)
{
	PR_STRING buffer;
	SIZE_T bufferLength;
	SIZE_T stringLength;

	stringLength = _r_str_length (string);

	if (_r_str_findchar (string, stringLength, L'%') != INVALID_SIZE_T)
	{
		bufferLength = 0x200;
		buffer = _r_obj_createstringex (NULL, bufferLength * sizeof (WCHAR));

		if (PathUnExpandEnvStrings (string, buffer->Buffer, (UINT)bufferLength))
		{
			_r_string_trimtonullterminator (buffer);

			return buffer;
		}

		_r_obj_dereference (buffer);
	}

	return _r_obj_createstringex (string, stringLength * sizeof (WCHAR));
}

PR_STRING _r_str_fromguid (LPGUID lpguid)
{
	PR_STRING string;
	UNICODE_STRING unicodeString;

	if (NT_SUCCESS (RtlStringFromGUID (lpguid, &unicodeString)))
	{
		string = _r_string_fromunicodestring (&unicodeString);

		RtlFreeUnicodeString (&unicodeString);

		return string;
	}

	return NULL;
}

PR_STRING _r_str_fromsecuritydescriptor (PSECURITY_DESCRIPTOR lpsd, SECURITY_INFORMATION information)
{
	LPWSTR securityString;
	ULONG securityStringLength;
	PR_STRING securityDescriptorString;

	if (ConvertSecurityDescriptorToStringSecurityDescriptor (lpsd, SDDL_REVISION, information, &securityString, &securityStringLength))
	{
		securityDescriptorString = _r_obj_createstringex (securityString, securityStringLength * sizeof (WCHAR));

		LocalFree (securityString);

		return securityDescriptorString;
	}

	return NULL;
}

PR_STRING _r_str_fromsid (PSID lpsid)
{
	PR_STRING string;
	UNICODE_STRING unicodeString;

	string = _r_obj_createstringex (NULL, SECURITY_MAX_SID_STRING_CHARACTERS * sizeof (WCHAR));

	_r_string_tounicodestring (string, &unicodeString);

	if (NT_SUCCESS (RtlConvertSidToUnicodeString (&unicodeString, lpsid, FALSE)))
	{
		string->Length = unicodeString.Length;
		_r_string_writenullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

BOOLEAN _r_str_toboolean (LPCWSTR string)
{
	if (_r_str_isempty (string))
		return FALSE;

	if (_r_str_tointeger (string) > 0 || _r_str_compare (string, L"true") == 0)
		return TRUE;

	return FALSE;
}

INT _r_str_tointeger (LPCWSTR string)
{
	return (INT)_r_str_tolong (string);
}

UINT _r_str_touinteger (LPCWSTR string)
{
	return (UINT)_r_str_toulong (string);
}

LONG _r_str_tolongex (LPCWSTR string, INT radix)
{
	if (_r_str_isempty (string))
		return 0L;

	return wcstol (string, NULL, radix);
}

LONG64 _r_str_tolong64 (LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0LL;

	return wcstoll (string, NULL, 10);
}

ULONG _r_str_toulongex (LPCWSTR string, INT radix)
{
	if (_r_str_isempty (string))
		return 0UL;

	return wcstoul (string, NULL, radix);
}

ULONG64 _r_str_toulong64 (LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0ULL;

	return wcstoull (string, NULL, 10);
}

SIZE_T _r_str_findchar (LPCWSTR string, SIZE_T length, WCHAR character)
{
	if (_r_str_isempty (string))
		return INVALID_SIZE_T;

	LPWSTR buffer;
	SIZE_T bufferSize;

	buffer = (LPWSTR)string;
	bufferSize = length * sizeof (WCHAR);

	if (length != 0)
	{
		WCHAR chr;

		chr = _r_str_upper (character);

		do
		{
			if (_r_str_upper (*buffer) == chr)
				return bufferSize / sizeof (WCHAR) - length;

			buffer += 1;
		}
		while (--length != 0);
	}

	return INVALID_SIZE_T;
}

SIZE_T _r_str_findlastchar (LPCWSTR string, SIZE_T length, WCHAR character)
{
	if (_r_str_isempty (string))
		return INVALID_SIZE_T;

	LPWSTR buffer;
	SIZE_T bufferSize;

	bufferSize = length * sizeof (WCHAR);
	buffer = (LPWSTR)PTR_ADD_OFFSET (string, bufferSize);

	if (length != 0)
	{
		WCHAR chr;

		chr = _r_str_upper (character);
		buffer -= 1;

		do
		{
			if (_r_str_upper (*buffer) == chr)
				return length - 1;

			buffer -= 1;
		}
		while (--length != 0);
	}

	return INVALID_SIZE_T;
}

VOID _r_str_replacechar (LPWSTR string, WCHAR char_from, WCHAR char_to)
{
	if (_r_str_isempty (string))
		return;

	while (*string != UNICODE_NULL)
	{
		if (*string == char_from)
			*string = char_to;

		string += 1;
	}
}

BOOLEAN _r_str_match (LPCWSTR string, LPCWSTR pattern, BOOLEAN is_ignorecase)
{
	LPCWSTR s, p;
	BOOLEAN star = FALSE;

	// Code is from http://xoomer.virgilio.it/acantato/dev/wildcard/wildmatch.html

LoopStart:
	for (s = string, p = pattern; *s; s++, p++)
	{
		switch (*p)
		{
			case L'?':
			{
				break;
			}

			case L'*':
			{
				star = TRUE;

				string = s;
				pattern = p;

				do
				{
					pattern += 1;
				}
				while (*pattern == L'*');

				if (*pattern == UNICODE_NULL)
					return TRUE;

				goto LoopStart;
			}

			default:
			{
				if (!is_ignorecase)
				{
					if (*s != *p)
						goto StarCheck;
				}
				else
				{
					if (_r_str_upper (*s) != _r_str_upper (*p))
						goto StarCheck;
				}

				break;
			}
		}
	}

	while (*p == L'*')
		p++;

	return (!*p);

StarCheck:
	if (!star)
		return FALSE;

	string += 1;
	goto LoopStart;
}

VOID _r_str_trim (LPWSTR string, LPCWSTR trim)
{
	if (!_r_str_isempty (string))
		StrTrim (string, trim);
}

VOID _r_str_trim (PR_STRING string, LPCWSTR trim)
{
	if (!_r_str_isempty (string))
	{
		_r_str_trim (string->Buffer, trim);
		_r_string_trimtonullterminator (string);
	}
}

VOID _r_str_trim (PR_STRINGBUILDER string, LPCWSTR trim)
{
	if (!_r_str_isempty (string))
		_r_str_trim (string->String, trim);
}

VOID _r_str_tolower (LPWSTR string)
{
	while (!_r_str_isempty (string))
	{
		*string = _r_str_lower (*string);

		string += 1;
	}
}

VOID _r_str_toupper (LPWSTR string)
{
	while (!_r_str_isempty (string))
	{
		*string = _r_str_upper (*string);

		string += 1;
	}
}

PR_STRING _r_str_extractex (LPCWSTR string, SIZE_T length, SIZE_T start_pos, SIZE_T extract_length)
{
	if (start_pos == 0 && extract_length >= length)
		return _r_obj_createstringex (string, length * sizeof (WCHAR));

	if (start_pos >= length)
		return NULL;

	return _r_obj_createstringex (&string[start_pos], extract_length * sizeof (WCHAR));
}

PR_STRING _r_str_multibyte2unicode (LPCSTR string)
{
	NTSTATUS status;
	PR_STRING outputString;
	ULONG unicodeSize;
	ULONG stringSize = (ULONG)strnlen_s (string, _R_STR_MAX_LENGTH);

	status = RtlMultiByteToUnicodeSize (&unicodeSize, string, stringSize);

	if (!NT_SUCCESS (status))
		return NULL;

	outputString = _r_obj_createstringex (NULL, unicodeSize);
	status = RtlMultiByteToUnicodeN (outputString->Buffer, (ULONG)outputString->Length, NULL, string, stringSize);

	if (NT_SUCCESS (status))
	{
		return outputString;
	}

	_r_obj_dereference (outputString);

	return NULL;
}

PR_BYTE _r_str_unicode2multibyte (LPCWSTR string)
{
	NTSTATUS status;
	PR_BYTE outputString;
	ULONG multiByteSize;
	ULONG stringSize = (ULONG)(_r_str_length (string) * sizeof (WCHAR));

	status = RtlUnicodeToMultiByteSize (&multiByteSize, string, stringSize);

	if (!NT_SUCCESS (status))
		return NULL;

	outputString = _r_obj_createbyteex (NULL, multiByteSize);
	status = RtlUnicodeToMultiByteN (outputString->Buffer, (ULONG)outputString->Length, NULL, string, stringSize);

	if (NT_SUCCESS (status))
	{
		return outputString;
	}

	_r_obj_dereference (outputString);

	return NULL;
}

PR_STRING _r_str_splitatchar (PR_STRINGREF string, PR_STRINGREF token, WCHAR separator)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findchar (input.Buffer, input.Length / sizeof (WCHAR), separator);

	if (index == INVALID_SIZE_T)
	{
		token->Buffer = NULL;
		token->Length = 0;

		return _r_obj_createstringex (input.Buffer, input.Length);
	}

	PR_STRING output = _r_obj_createstringex (input.Buffer, index * sizeof (WCHAR));

	token->Buffer = (LPWSTR)PTR_ADD_OFFSET (input.Buffer, output->Length + sizeof (UNICODE_NULL));
	token->Length = input.Length - (index * sizeof (WCHAR)) - sizeof (UNICODE_NULL);

	return output;
}

PR_STRING _r_str_splitatlastchar (PR_STRINGREF string, PR_STRINGREF token, WCHAR separator)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findlastchar (input.Buffer, input.Length / sizeof (WCHAR), separator);

	if (index == INVALID_SIZE_T)
	{
		token->Buffer = NULL;
		token->Length = 0;

		return _r_obj_createstringex (input.Buffer, input.Length);
	}

	PR_STRING output = _r_obj_createstringex (input.Buffer, index * sizeof (WCHAR));

	token->Buffer = (LPWSTR)PTR_ADD_OFFSET (input.Buffer, output->Length + sizeof (UNICODE_NULL));
	token->Length = input.Length - (index * sizeof (WCHAR)) - sizeof (UNICODE_NULL);

	return output;
}

VOID _r_str_unserialize (PR_STRING string, WCHAR key_delimeter, WCHAR value_delimeter, OBJECTS_STRINGS_MAP1* valuesMap)
{
	R_STRINGREF remainingPart;
	PR_STRING valuesPart;
	PR_STRING keyName;
	PR_STRING keyValue;
	SIZE_T delimeter_pos;

	_r_stringref_initialize2 (&remainingPart, string);

	while (remainingPart.Length != 0)
	{
		valuesPart = _r_str_splitatchar (&remainingPart, &remainingPart, key_delimeter);

		if (valuesPart)
		{
			delimeter_pos = _r_str_findchar (valuesPart->Buffer, _r_obj_getstringlength (valuesPart), value_delimeter);

			if (delimeter_pos != INVALID_SIZE_T)
			{
				keyName = _r_str_extract (valuesPart, 0, delimeter_pos);

				if (valuesMap->find (keyName) == valuesMap->end ())
				{
					keyValue = _r_str_extract (valuesPart, delimeter_pos + 1, _r_obj_getstringlength (valuesPart) - delimeter_pos - 1);

					valuesMap->emplace (keyName, keyValue);
				}
				else
				{
					_r_obj_dereference (keyName);
				}
			}

			_r_obj_dereference (valuesPart);
		}
	}
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

	swscanf_s (v1, L"%i.%i.%i.%i", &oct_v1[0], &oct_v1[1], &oct_v1[2], &oct_v1[3]);
	swscanf_s (v2, L"%i.%i.%i.%i", &oct_v2[0], &oct_v2[1], &oct_v2[2], &oct_v2[3]);

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (oct_v1); i++)
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

BOOLEAN _r_sys_iselevated ()
{
	static BOOLEAN is_initialized = FALSE;
	static BOOLEAN is_elevated = FALSE;

	if (!is_initialized)
	{
#if !defined(_APP_NO_DEPRECATIONS)
		// winxp compatibility
		if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		{
			is_elevated = !!IsUserAnAdmin ();
			is_initialized = TRUE;

			return is_elevated;
		}
#endif // !_APP_NO_DEPRECATIONS

		HANDLE htoken;

		if (OpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &htoken))
		{
			TOKEN_ELEVATION elevation;
			ULONG returnLength;

			if (NT_SUCCESS (NtQueryInformationToken (htoken, TokenElevation, &elevation, sizeof (elevation), &returnLength)))
				is_elevated = !!elevation.TokenIsElevated;

			CloseHandle (htoken);
		}

		is_initialized = TRUE;
	}

	return is_elevated;
}

BOOLEAN _r_sys_isosversiongreaterorequal (ULONG requiredVersion)
{
	static BOOLEAN is_initialized = FALSE;
	static ULONG windowsVersion = 0;

	if (!is_initialized)
	{
		RTL_OSVERSIONINFOEXW versionInfo;
		RtlSecureZeroMemory (&versionInfo, sizeof (versionInfo));

		versionInfo.dwOSVersionInfoSize = sizeof (versionInfo);

		if (NT_SUCCESS (RtlGetVersion (&versionInfo)))
		{
			if (versionInfo.dwMajorVersion == 5 && versionInfo.dwMinorVersion == 1)
			{
				windowsVersion = WINDOWS_XP;
			}
			else if (versionInfo.dwMajorVersion == 5 && versionInfo.dwMinorVersion == 2)
			{
				windowsVersion = WINDOWS_XP_64;
			}
			else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 0)
			{
				windowsVersion = WINDOWS_VISTA;
			}
			else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 1)
			{
				windowsVersion = WINDOWS_7;
			}
			else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 2)
			{
				windowsVersion = WINDOWS_8;
			}
			else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 3)
			{
				windowsVersion = WINDOWS_8_1;
			}
			else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 4)
			{
				windowsVersion = WINDOWS_10; // earlier versions of windows 10 have 6.4 version number
			}
			else if (versionInfo.dwMajorVersion == 10 && versionInfo.dwMinorVersion == 0)
			{
				if (versionInfo.dwBuildNumber >= 19042)
				{
					windowsVersion = WINDOWS_10_20H2;
				}
				else if (versionInfo.dwBuildNumber >= 19041)
				{
					windowsVersion = WINDOWS_10_20H1;
				}
				else if (versionInfo.dwBuildNumber >= 18363)
				{
					windowsVersion = WINDOWS_10_19H2;
				}
				else if (versionInfo.dwBuildNumber >= 18362)
				{
					windowsVersion = WINDOWS_10_19H1;
				}
				else if (versionInfo.dwBuildNumber >= 17763)
				{
					windowsVersion = WINDOWS_10_RS5;
				}
				else if (versionInfo.dwBuildNumber >= 17134)
				{
					windowsVersion = WINDOWS_10_RS4;
				}
				else if (versionInfo.dwBuildNumber >= 16299)
				{
					windowsVersion = WINDOWS_10_RS3;
				}
				else if (versionInfo.dwBuildNumber >= 15063)
				{
					windowsVersion = WINDOWS_10_RS2;
				}
				else if (versionInfo.dwBuildNumber >= 14393)
				{
					windowsVersion = WINDOWS_10_RS1;
				}
				else if (versionInfo.dwBuildNumber >= 10586)
				{
					windowsVersion = WINDOWS_10_TH2;
				}
				else if (versionInfo.dwBuildNumber >= 10240)
				{
					windowsVersion = WINDOWS_10;
				}
				else
				{
					windowsVersion = WINDOWS_10;
				}
			}
		}

		is_initialized = TRUE;
	}

	return windowsVersion >= requiredVersion;
}

BOOLEAN _r_sys_createprocessex (LPCWSTR fileName, LPCWSTR commandLine, LPCWSTR currentDirectory, WORD showState, ULONG flags)
{
	BOOLEAN is_success;

	STARTUPINFO startupInfo = {0};
	PROCESS_INFORMATION processInfo = {0};

	PR_STRING fileNameString = NULL;
	PR_STRING commandLineString = NULL;
	PR_STRING currentDirectoryString = NULL;

	startupInfo.cb = sizeof (startupInfo);

	if (showState != SW_SHOWDEFAULT)
	{
		startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
		startupInfo.wShowWindow = showState;
	}

	if (commandLine)
	{
		commandLineString = _r_obj_createstring (commandLine);
	}

	if (fileName)
	{
		fileNameString = _r_obj_createstring (fileName);
	}
	else
	{
		if (commandLineString)
		{
			INT numargs;
			LPWSTR* arga;

			arga = CommandLineToArgvW (commandLineString->Buffer, &numargs);

			if (arga)
			{
				_r_obj_movereference (&fileNameString, _r_obj_createstring (arga[0]));

				LocalFree (arga);
			}
		}
	}

	if (fileNameString && !_r_fs_exists (fileNameString->Buffer))
	{
		_r_obj_clearreference (&fileNameString);
	}

	if (currentDirectory)
	{
		currentDirectoryString = _r_obj_createstring (currentDirectory);
	}
	else
	{
		if (!_r_str_isempty (fileNameString))
			currentDirectoryString = _r_path_getbasedirectory (fileNameString->Buffer);
	}

	is_success = !!CreateProcess (_r_obj_getstring (fileNameString), (LPWSTR)_r_obj_getstring (commandLineString), NULL, NULL, FALSE, flags, NULL, _r_obj_getstring (currentDirectoryString), &startupInfo, &processInfo);

	SAFE_DELETE_HANDLE (processInfo.hProcess);
	SAFE_DELETE_HANDLE (processInfo.hThread);

	SAFE_DELETE_REFERENCE (fileNameString);
	SAFE_DELETE_REFERENCE (commandLineString);
	SAFE_DELETE_REFERENCE (currentDirectoryString);

	return is_success;
}

ULONG _r_sys_getthreadindex ()
{
	static ULONG tls_index = 0;

	if (!tls_index)
		tls_index = TlsAlloc ();

	return tls_index;
}

PR_THREAD_DATA _r_sys_getthreaddata ()
{
	PR_THREAD_DATA tls;
	ULONG err_code;
	ULONG tls_index;

	err_code = GetLastError ();
	tls_index = _r_sys_getthreadindex ();

	tls = (PR_THREAD_DATA)TlsGetValue (tls_index);

	if (!tls)
	{
		tls = (PR_THREAD_DATA)_r_mem_allocatezero (sizeof (R_THREAD_DATA));

		if (!TlsSetValue (tls_index, tls))
			return NULL;

		tls->tid = GetCurrentThreadId ();
		tls->handle = NULL;
	}

	SetLastError (err_code);

	return tls;
}

THREAD_API _r_sys_basethreadstart (PVOID arglist)
{
	NTSTATUS status;
	HRESULT hr;
	PR_THREAD_CONTEXT context;
	PR_THREAD_DATA tls;

	context = (PR_THREAD_CONTEXT)arglist;

	tls = _r_sys_getthreaddata ();

	if (tls)
	{
		tls->handle = context->thread;
		tls->is_handleused = context->is_handleused;
	}

	hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	status = context->start_address (context->arglist);

	if (hr == S_OK || hr == S_FALSE)
		CoUninitialize ();

	_r_mem_free (arglist);

	if (tls)
	{
		if (!tls->is_handleused)
		{
			if (tls->handle)
			{
				NtClose (tls->handle);
				tls->handle = NULL;
			}
		}
	}

	RtlExitUserThread (status);

	return status;
}

NTSTATUS _r_sys_createthreadex (THREAD_CALLBACK start_address, PVOID arglist, PHANDLE pthread, INT priority)
{
	NTSTATUS status;
	HANDLE thread = NULL;
	PR_THREAD_CONTEXT context;

	context = (PR_THREAD_CONTEXT)_r_mem_allocatezero (sizeof (R_THREAD_CONTEXT));

	status = RtlCreateUserThread (NtCurrentProcess (), NULL, TRUE, 0, 0, 0, &_r_sys_basethreadstart, context, &thread, NULL);

	if (NT_SUCCESS (status))
	{
		context->start_address = start_address;
		context->arglist = arglist;
		context->thread = thread;
		context->is_handleused = (pthread != NULL); // user need to be destroy thread handle by himself

		SetThreadPriority (thread, priority);

		if (!pthread)
		{
			status = _r_sys_resumethread (thread);
		}
	}

	if (NT_SUCCESS (status))
	{
		if (pthread)
		{
			*pthread = thread;
		}
	}
	else
	{
		if (thread)
		{
			NtClose (thread);
		}

		if (context)
		{
			_r_mem_free (context);
		}
	}

	return status;
}

PR_STRING _r_sys_getsessioninfo (WTS_INFO_CLASS info)
{
	PR_STRING string;
	LPWSTR buffer;
	ULONG length;

	if (WTSQuerySessionInformation (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, info, &buffer, &length))
	{
		string = _r_obj_createstringex (buffer, length * sizeof (WCHAR));

		WTSFreeMemory (buffer);

		return string;
	}

	return NULL;
}

PR_STRING _r_sys_getusernamefromsid (PSID psid)
{
	LSA_OBJECT_ATTRIBUTES objectAttributes;
	LSA_HANDLE policyHandle;
	PLSA_REFERENCED_DOMAIN_LIST referencedDomains;
	PLSA_TRANSLATED_NAME names;
	R_STRINGBUILDER string = {0};

	InitializeObjectAttributes (&objectAttributes, NULL, 0, NULL, NULL);

	if (NT_SUCCESS (LsaOpenPolicy (NULL, &objectAttributes, POLICY_LOOKUP_NAMES, &policyHandle)))
	{
		if (NT_SUCCESS (LsaLookupSids (policyHandle, 1, &psid, &referencedDomains, &names)))
		{
			if (names && names[0].Use != SidTypeInvalid && names[0].Use != SidTypeUnknown)
			{
				BOOLEAN is_hasdomain = referencedDomains && names[0].DomainIndex >= 0;
				BOOLEAN is_hasname = !_r_str_isempty (&names[0].Name);

				if (is_hasdomain || is_hasname)
					_r_obj_createstringbuilder (&string);

				if (is_hasdomain)
				{
					PLSA_TRUST_INFORMATION trustInfo = &referencedDomains->Domains[names[0].DomainIndex];

					if (!_r_str_isempty (&trustInfo->Name))
					{
						_r_string_appendex (&string, trustInfo->Name.Buffer, trustInfo->Name.Length);

						if (is_hasname)
						{
							_r_string_append (&string, L"\\");
						}
					}
				}

				if (is_hasname)
				{
					_r_string_appendex (&string, names[0].Name.Buffer, names[0].Name.Length);
				}
			}

			if (referencedDomains)
				LsaFreeMemory (referencedDomains);

			if (names)
				LsaFreeMemory (names);
		}

		LsaClose (policyHandle);
	}

	if (!_r_str_isempty (&string))
		return _r_obj_finalstringbuilder (&string);

	_r_obj_deletestringbuilder (&string);

	return NULL;
}

#if !defined(_WIN64)
BOOLEAN _r_sys_iswow64 ()
{
	// IsWow64Process is not available on all supported versions of Windows.
	// Use GetModuleHandle to get a handle to the DLL that contains the function
	// and GetProcAddress to get a pointer to the function if available.

	HMODULE hkernel32 = GetModuleHandle (L"kernel32.dll");

	if (hkernel32)
	{
		typedef BOOL (WINAPI *IW64P)(HANDLE hProcess, PBOOL Wow64Process);
		const IW64P _IsWow64Process = (IW64P)GetProcAddress (hkernel32, "IsWow64Process");

		if (_IsWow64Process)
		{
			BOOL iswow64;

			if (_IsWow64Process (NtCurrentProcess (), &iswow64))
				return !!iswow64;
		}
	}

	return FALSE;
}
#endif // _WIN64

VOID _r_sys_setprivilege (const PULONG privileges, ULONG count, BOOLEAN is_enable)
{
	HANDLE htoken;
	PVOID privilegesBuffer;
	PTOKEN_PRIVILEGES tokenPrivileges;

	if (OpenProcessToken (NtCurrentProcess (), TOKEN_ADJUST_PRIVILEGES, &htoken))
	{
		privilegesBuffer = _r_mem_allocatezero (FIELD_OFFSET (TOKEN_PRIVILEGES, Privileges) + (sizeof (LUID_AND_ATTRIBUTES) * count));

		tokenPrivileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
		tokenPrivileges->PrivilegeCount = count;

		for (ULONG i = 0; i < count; i++)
		{
			tokenPrivileges->Privileges[i].Luid.HighPart = 0;
			tokenPrivileges->Privileges[i].Luid.LowPart = privileges[i];
			tokenPrivileges->Privileges[i].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;
		}

		NtAdjustPrivilegesToken (htoken, FALSE, tokenPrivileges, 0, NULL, NULL);

		_r_mem_free (privilegesBuffer);

		CloseHandle (htoken);
	}
}

/*
	Unixtime
*/

time_t _r_unixtime_now ()
{
	SYSTEMTIME systemTime = {0};
	GetSystemTime (&systemTime);

	return _r_unixtime_from_systemtime (&systemTime);
}

VOID _r_unixtime_to_filetime (time_t ut, FILETIME * pfileTime)
{
	LONG64 ll = Int32x32To64 (ut, 10000000LL) + 116444736000000000LL; // 64-bit value

	pfileTime->dwLowDateTime = (ULONG)ll;
	pfileTime->dwHighDateTime = ll >> 32;
}

VOID _r_unixtime_to_systemtime (time_t ut, SYSTEMTIME * psystemTime)
{
	FILETIME fileTime = {0};

	_r_unixtime_to_filetime (ut, &fileTime);
	FileTimeToSystemTime (&fileTime, psystemTime);
}

time_t _r_unixtime_from_filetime (const FILETIME * pfileTime)
{
	ULARGE_INTEGER ul = {0};

	ul.LowPart = pfileTime->dwLowDateTime;
	ul.HighPart = pfileTime->dwHighDateTime;

	return ul.QuadPart / 10000000ULL - 11644473600ULL;
}

time_t _r_unixtime_from_systemtime (const SYSTEMTIME * psystemTime)
{
	FILETIME fileTime = {0};

	if (SystemTimeToFileTime (psystemTime, &fileTime))
		return _r_unixtime_from_filetime (&fileTime);

	return 0;
}

/*
	Device context (Draw/Calculation etc...)
*/

INT _r_dc_getdpivalue (HWND hwnd)
{
	static BOOLEAN is_initialized = FALSE;

	static GDFM _GetDpiForMonitor = NULL;
	static GDFW _GetDpiForWindow = NULL;
	static GDFS _GetDpiForSystem = NULL;

	// initialize library calls
	if (!is_initialized)
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
		{
			HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
			HMODULE hshcore = LoadLibraryEx (L"shcore.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			if (hshcore)
			{
				_GetDpiForMonitor = (GDFM)GetProcAddress (hshcore, "GetDpiForMonitor");

				FreeLibrary (hshcore);
			}

			if (huser32)
			{
				_GetDpiForWindow = (GDFW)GetProcAddress (huser32, "GetDpiForWindow");
				_GetDpiForSystem = (GDFS)GetProcAddress (huser32, "GetDpiForSystem");

				FreeLibrary (huser32);
			}
		}

		is_initialized = TRUE;
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		if (hwnd)
		{
			if (_GetDpiForWindow)
			{
				return _GetDpiForWindow (hwnd);
			}

			if (_GetDpiForMonitor)
			{
				HMONITOR hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

				if (hmonitor)
				{
					UINT dpix;
					UINT dpiy;

					if (SUCCEEDED (_GetDpiForMonitor (hmonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy)))
					{
						return dpix;
					}
				}
			}
		}

		if (_GetDpiForSystem)
		{
			return _GetDpiForSystem ();
		}
	}

	// fallback
	HDC hdc = GetDC (NULL);

	if (hdc)
	{
		INT dpiValue = GetDeviceCaps (hdc, LOGPIXELSX);

		ReleaseDC (NULL, hdc);

		return dpiValue;
	}

	return USER_DEFAULT_SCREEN_DPI;
}

COLORREF _r_dc_getcolorbrightness (COLORREF clr)
{
	COLORREF r = clr & 0xFF;
	COLORREF g = (clr >> 8) & 0xFF;
	COLORREF b = (clr >> 16) & 0xFF;

	COLORREF min = r;
	COLORREF max = r;

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

	return RGB (0xFF, 0xFF, 0xFF);
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
	// win10rs1+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS1))
	{
		HMODULE huser32 = GetModuleHandle (L"user32.dll");

		if (huser32)
		{
			typedef INT (WINAPI *GSMFD)(INT nIndex, UINT dpi); // win10rs1+
			const GSMFD _GetSystemMetricsForDpi = (GSMFD)GetProcAddress (huser32, "GetSystemMetricsForDpi");

			if (_GetSystemMetricsForDpi)
				return _GetSystemMetricsForDpi (index, _r_dc_getdpivalue (hwnd));
		}
	}

	return GetSystemMetrics (index);
}

// Optimized version of WinAPI function "FillRect"
VOID _r_dc_fillrect (HDC hdc, const LPRECT lprc, COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (hdc, clr);
	ExtTextOut (hdc, 0, 0, ETO_OPAQUE, lprc, NULL, 0, NULL);
	SetBkColor (hdc, clr_prev);
}

LONG _r_dc_fontwidth (HDC hdc, LPCWSTR text, SIZE_T length)
{
	SIZE size;

	if (!GetTextExtentPoint32 (hdc, text, (INT)length, &size))
		return 200; // fallback

	return size.cx;
}

/*
	Window management
*/

VOID _r_wnd_addstyle (HWND hwnd, INT ctrl_id, LONG_PTR mask, LONG_PTR stateMask, INT index)
{
	if (ctrl_id)
		hwnd = GetDlgItem (hwnd, ctrl_id);

	LONG_PTR style = (GetWindowLongPtr (hwnd, index) & ~stateMask) | mask;

	SetWindowLongPtr (hwnd, index, style);

	SetWindowPos (hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

VOID _r_wnd_adjustwindowrect (HWND hwnd, LPRECT lprect)
{
	MONITORINFO monitorInfo = {0};
	monitorInfo.cbSize = sizeof (monitorInfo);

	HMONITOR hmonitor = hwnd ? MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST) : MonitorFromRect (lprect, MONITOR_DEFAULTTONEAREST);

	if (hmonitor)
	{
		if (GetMonitorInfo (hmonitor, &monitorInfo))
		{
			LPRECT lpbounds = &monitorInfo.rcWork;

			LONG original_width = _r_calc_rectwidth (LONG, lprect);
			LONG original_height = _r_calc_rectheight (LONG, lprect);

			if (lprect->left + original_width > lpbounds->left + _r_calc_rectwidth (LONG, lpbounds))
				lprect->left = lpbounds->left + _r_calc_rectwidth (LONG, lpbounds) - original_width;

			if (lprect->top + original_height > lpbounds->top + _r_calc_rectheight (LONG, lpbounds))
				lprect->top = lpbounds->top + _r_calc_rectheight (LONG, lpbounds) - original_height;

			if (lprect->left < lpbounds->left)
				lprect->left = lpbounds->left;

			if (lprect->top < lpbounds->top)
				lprect->top = lpbounds->top;

			lprect->right = lprect->left + original_width;
			lprect->bottom = lprect->top + original_height;
		}
	}
}

VOID _r_wnd_center (HWND hwnd, HWND hparent)
{
	if (hparent)
	{
		if (IsWindowVisible (hparent) && !IsIconic (hparent))
		{
			RECT wndRect;
			RECT parentRect;

			if (GetWindowRect (hwnd, &wndRect) && GetWindowRect (hparent, &parentRect))
			{
				_r_wnd_centerwindowrect (&wndRect, &parentRect);
				_r_wnd_adjustwindowrect (hwnd, &wndRect);

				SetWindowPos (hwnd, NULL, wndRect.left, wndRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

				return;
			}
		}
	}

	MONITORINFO monitorInfo = {0};
	monitorInfo.cbSize = sizeof (monitorInfo);

	HMONITOR hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

	if (hmonitor)
	{
		if (GetMonitorInfo (hmonitor, &monitorInfo))
		{
			RECT wndRect;

			if (GetWindowRect (hwnd, &wndRect))
			{
				_r_wnd_centerwindowrect (&wndRect, &monitorInfo.rcWork);

				SetWindowPos (hwnd, NULL, wndRect.left, wndRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
			}
		}
	}
}

VOID _r_wnd_changemessagefilter (HWND hwnd, PUINT pmsg, SIZE_T count, ULONG action)
{
	HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (!huser32)
		return;

#if defined(_APP_NO_DEPRECATIONS)

	typedef BOOL (WINAPI *CWMFEX)(HWND hwnd, UINT message, ULONG action, PCHANGEFILTERSTRUCT pChangeFilterStruct); // win7+
	const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (huser32, "ChangeWindowMessageFilterEx");

	if (_ChangeWindowMessageFilterEx)
	{
		for (SIZE_T i = 0; i < count; i++)
			_ChangeWindowMessageFilterEx (hwnd, pmsg[i], action, NULL);
	}

#else

	typedef BOOL (WINAPI *CWMFEX)(HWND hwnd, UINT message, ULONG action, PCHANGEFILTERSTRUCT pChangeFilterStruct); // win7+
	const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (huser32, "ChangeWindowMessageFilterEx");

	if (_ChangeWindowMessageFilterEx)
	{
		for (SIZE_T i = 0; i < count; i++)
			_ChangeWindowMessageFilterEx (hwnd, pmsg[i], action, NULL);
	}
	else
	{
		typedef BOOL (WINAPI *CWMF)(UINT message, ULONG dwFlag); // vista fallback
		const CWMF _ChangeWindowMessageFilter = (CWMF)GetProcAddress (huser32, "ChangeWindowMessageFilter");

		if (_ChangeWindowMessageFilter)
		{
			for (SIZE_T i = 0; i < count; i++)
				_ChangeWindowMessageFilter (pmsg[i], action);
		}
	}

#endif // _APP_NO_DEPRECATIONS

	FreeLibrary (huser32);
}

VOID _r_wnd_changesettings (HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	LPCWSTR type = (LPCWSTR)lparam;

	if (_r_str_isempty (type))
		return;

	if (_r_str_compare (type, L"WindowMetrics") == 0)
	{
		SendMessage (hwnd, RM_LOCALIZE, 0, 0);
		SendMessage (hwnd, WM_THEMECHANGED, 0, 0);
	}
#if defined(_APP_HAVE_DARKTHEME)
	// win10rs5+
	else if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5) && _r_str_compare (type, L"ImmersiveColorSet") == 0)
	{
		_r_wnd_setdarktheme (hwnd);

		SendMessage (hwnd, WM_THEMECHANGED, 0, 0);
	}
#endif // _APP_HAVE_DARKTHEME
}

VOID _r_wnd_enablenonclientscaling (HWND hwnd)
{
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_10_RS1)) // win10rs1+
		return;

	HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huser32)
	{
		typedef BOOL (WINAPI *ENCDS)(HWND hwnd); // win10rs1+
		const ENCDS _EnableNonClientDpiScaling = (ENCDS)GetProcAddress (huser32, "EnableNonClientDpiScaling");

		if (_EnableNonClientDpiScaling)
			_EnableNonClientDpiScaling (hwnd);

		FreeLibrary (huser32);
	}
}

static BOOLEAN _r_wnd_isplatformfullscreenmode ()
{
	QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;

	// SHQueryUserNotificationState is only available for Vista+
#if defined(_APP_NO_DEPRECATIONS)
	if (FAILED (SHQueryUserNotificationState (&state)))
		return FALSE;
#else
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return FALSE;

	HINSTANCE hshell32 = GetModuleHandle (L"shell32.dll");

	if (hshell32)
	{
		typedef HRESULT (WINAPI *SHQUNS)(QUERY_USER_NOTIFICATION_STATE *pquns); // vista+
		const SHQUNS _SHQueryUserNotificationState = (SHQUNS)GetProcAddress (hshell32, "SHQueryUserNotificationState");

		if (_SHQueryUserNotificationState)
		{
			if (FAILED (_SHQueryUserNotificationState (&state)))
				return FALSE;
		}
	}
#endif // _APP_NO_DEPRECATIONS

	return (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE);
}

static BOOLEAN _r_wnd_isfullscreenwindowmode ()
{
	// Get the foreground window which the user is currently working on.
	HWND hwnd = GetForegroundWindow ();

	if (!hwnd)
		return FALSE;

	// Get the monitor where the window is located.
	RECT wnd_rect;

	if (!GetWindowRect (hwnd, &wnd_rect))
		return FALSE;

	HMONITOR hmonitor = MonitorFromRect (&wnd_rect, MONITOR_DEFAULTTONULL);

	if (!hmonitor)
		return FALSE;

	MONITORINFO monitor_info = {0};
	monitor_info.cbSize = sizeof (monitor_info);

	if (!GetMonitorInfo (hmonitor, &monitor_info))
		return FALSE;

	// It should be the main monitor.
	if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
		return FALSE;

	// The window should be at least as large as the monitor.
	if (!IntersectRect (&wnd_rect, &wnd_rect, &monitor_info.rcMonitor))
		return FALSE;

	if (!EqualRect (&wnd_rect, &monitor_info.rcMonitor))
		return FALSE;

	// At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
	// its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
	LONG_PTR style = GetWindowLongPtr (hwnd, GWL_STYLE);
	LONG_PTR ex_style = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) || (ex_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

static BOOLEAN _r_wnd_isfullscreenconsolemode ()
{
	// We detect this by attaching the current process to the console of the
	// foreground window and then checking if it is in full screen mode.
	HWND hwnd = GetForegroundWindow ();

	if (!hwnd)
		return FALSE;

	ULONG pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

	if (!pid)
		return FALSE;

	if (!AttachConsole (pid))
		return FALSE;

	ULONG modes = 0;
	GetConsoleDisplayMode (&modes);
	FreeConsole ();

	return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

BOOLEAN _r_wnd_isfullscreenmode ()
{
	return _r_wnd_isplatformfullscreenmode () || _r_wnd_isfullscreenwindowmode () || _r_wnd_isfullscreenconsolemode ();
}

// Author: Mikhail
// https://stackoverflow.com/a/9126096

BOOLEAN _r_wnd_isundercursor (HWND hwnd)
{
	if (!hwnd || !IsWindowVisible (hwnd) || IsIconic (hwnd))
		return FALSE;

	POINT cursorPoint;
	RECT wndRect;

	if (!GetCursorPos (&cursorPoint))
		return FALSE;

	if (!GetWindowRect (hwnd, &wndRect))
		return FALSE;

	return !!PtInRect (&wndRect, cursorPoint);
}

VOID _r_wnd_toggle (HWND hwnd, BOOLEAN is_show)
{
	if (is_show || !IsWindowVisible (hwnd))
	{
		if (!ShowWindow (hwnd, SW_SHOW))
		{
			if (GetLastError () == ERROR_ACCESS_DENIED)
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

#if defined(_APP_HAVE_DARKTHEME)
BOOL CALLBACK _r_wnd_darkthemechildproc (HWND hwnd, LPARAM lparam)
{
	if (!IsWindow (hwnd))
		return TRUE;

	WCHAR class_name[64];

	if (!GetClassName (hwnd, class_name, RTL_NUMBER_OF (class_name)))
		return TRUE;

	BOOL is_darktheme = LOWORD (lparam);

	_r_wnd_setdarkwindow (hwnd, is_darktheme);

	if (_r_str_compare (class_name, WC_LISTVIEW) == 0)
	{
		// general
		//SendMessage (hwnd, LVM_SETBKCOLOR, 0, is_darktheme ? rinternal::black_bg : rinternal::white_bg);
		//SendMessage (hwnd, LVM_SETTEXTBKCOLOR, 0, is_darktheme ? rinternal::black_bg : rinternal::white_bg);
		//SendMessage (hwnd, LVM_SETTEXTCOLOR, 0, is_darktheme ? rinternal::black_text : rinternal::white_text);

		//SetWindowTheme (hwnd, is_darktheme ? L"DarkMode_Explorer" : L"Explorer", NULL);

		// tooltips
		HWND htip = (HWND)SendMessage (hwnd, LVM_GETTOOLTIPS, 0, 0);

		if (htip)
			SetWindowTheme (htip, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}
	else if (_r_str_compare (class_name, WC_TREEVIEW) == 0)
	{
		// general
		//SendMessage (hwnd, TVM_SETBKCOLOR, 0, is_darktheme ? rinternal::black_bg : rinternal::white_bg);
		//SendMessage (hwnd, TVM_SETTEXTCOLOR, 0, is_darktheme ? rinternal::black_text : rinternal::white_text);

		//SetWindowTheme (hwnd, is_darktheme ? L"DarkMode_Explorer" : L"Explorer", NULL);

		// tooltips
		HWND htip = (HWND)SendMessage (hwnd, TVM_GETTOOLTIPS, 0, 0);

		if (htip)
			SetWindowTheme (htip, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}
	else if (_r_str_compare (class_name, TOOLBARCLASSNAME) == 0)
	{
		HWND htip = (HWND)SendMessage (hwnd, TB_GETTOOLTIPS, 0, 0);

		if (htip)
			SetWindowTheme (htip, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}
	else if (_r_str_compare (class_name, WC_TABCONTROL) == 0)
	{
		HWND htip = (HWND)SendMessage (hwnd, TCM_GETTOOLTIPS, 0, 0);

		if (htip)
			SetWindowTheme (htip, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}
	else if (_r_str_compare (class_name, TOOLTIPS_CLASS) == 0)
	{
		SetWindowTheme (hwnd, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}
	else if (_r_str_compare (class_name, WC_SCROLLBAR) == 0)
	{
		SetWindowTheme (hwnd, is_darktheme ? L"DarkMode_Explorer" : L"", NULL);
	}

	SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER); // HACK!!!

	return TRUE;
}

BOOL _r_wnd_isdarktheme ()
{
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5)) // 1809+
		return FALSE;

	HIGHCONTRAST hci = {0};
	hci.cbSize = sizeof (hci);

	if (SystemParametersInfo (SPI_GETHIGHCONTRAST, 0, &hci, 0))
	{
		// no dark mode in high-contrast mode
		if ((hci.dwFlags & HCF_HIGHCONTRASTON) != 0)
			return FALSE;
	}

	HMODULE huxtheme = LoadLibraryEx (L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huxtheme)
	{
		typedef BOOL (WINAPI* SAUDM) (VOID);
		const SAUDM _ShouldAppsUseDarkMode = (SAUDM)GetProcAddress (huxtheme, MAKEINTRESOURCEA (132)); // ShouldAppsUseDarkMode

		// Seems the undocumented ShouldAppsUseDarkMode() API does not return a proper BOOL.
		// Only the first bit of the returned value indicates TRUE/FALSE.
		// https://github.com/stefankueng/tools/issues/14#issuecomment-495864391

		if (_ShouldAppsUseDarkMode)
		{
			if ((_ShouldAppsUseDarkMode () & 0x01) != 0)
			{
				FreeLibrary (huxtheme);
				return TRUE;
			}
		}

		FreeLibrary (huxtheme);
	}

	return FALSE;
}

VOID _r_wnd_setdarkframe (HWND hwnd, BOOL is_enable)
{
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_19H1)) // 1903+
	{
		HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (huser32)
		{
			typedef BOOL (WINAPI* SWCA) (HWND, PWINDOWCOMPOSITIONATTRIBDATA); // SetWindowCompositionAttribute
			const SWCA _SetWindowCompositionAttribute = (SWCA)GetProcAddress (huser32, "SetWindowCompositionAttribute");

			if (_SetWindowCompositionAttribute)
			{
				WINDOWCOMPOSITIONATTRIBDATA data;

				data.Attrib = WCA_USEDARKMODECOLORS;
				data.pvData = &is_enable;
				data.cbData = sizeof (is_enable);

				if (_SetWindowCompositionAttribute (hwnd, &data))
				{
					FreeLibrary (huser32);
					return;
				}
			}

			FreeLibrary (huser32);
		}
	}

	SetProp (hwnd, L"UseImmersiveDarkModeColors", (HANDLE)(LONG_PTR)is_enable); // 1809 fallback
}

VOID _r_wnd_setdarkwindow (HWND hwnd, BOOL is_enable)
{
	HMODULE huxtheme = LoadLibraryEx (L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huxtheme)
	{
		typedef BOOL (WINAPI* ADMFW) (HWND, BOOL);
		const ADMFW _AllowDarkModeForWindow = (ADMFW)GetProcAddress (huxtheme, MAKEINTRESOURCEA (133)); // AllowDarkModeForWindow

		if (_AllowDarkModeForWindow)
			_AllowDarkModeForWindow (hwnd, is_enable);

		FreeLibrary (huxtheme);
	}
}

VOID _r_wnd_setdarktheme (HWND hwnd)
{
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5)) // 1809+
		return;

	BOOL is_darktheme = _r_wnd_isdarktheme ();

	// Ordinal 104: VOID WINAPI RefreshImmersiveColorPolicyState(VOID)
	// Ordinal 106: BOOL WINAPI GetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE mode)
	// Ordinal 132: BOOL WINAPI ShouldAppsUseDarkMode(VOID)
	// Ordinal 135: BOOL WINAPI AllowDarkModeForApp(BOOL allow)
	// Ordinal 135: BOOL WINAPI SetPreferredAppMode(PreferredAppMode mode) // 1903+
	// Ordinal 136: VOID WINAPI FlushMenuThemes (VOID);

	_r_wnd_setdarkframe (hwnd, is_darktheme);

	HMODULE huxtheme = LoadLibraryEx (L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (huxtheme)
	{
		const auto ord135 = GetProcAddress (huxtheme, MAKEINTRESOURCEA (135));

		if (ord135)
		{
			typedef BOOL (WINAPI* ADMFA) (BOOL); // AllowDarkModeForApp
			typedef PREFERRED_APP_MODE (WINAPI* SPM) (PREFERRED_APP_MODE); // SetPreferredAppMode (1903+)

			ADMFA _AllowDarkModeForApp = NULL;
			SPM _SetPreferredAppMode = NULL;

			if (_r_sys_isosversiongreaterorequal (WINDOWS_10_19H1)) // 1903+
				_SetPreferredAppMode = (SPM)ord135;
			else
				_AllowDarkModeForApp = (ADMFA)ord135;

			if (_SetPreferredAppMode)
				_SetPreferredAppMode (is_darktheme ? AllowDark : Default);

			else if (_AllowDarkModeForApp)
				_AllowDarkModeForApp (is_darktheme);

			EnumChildWindows (hwnd, &_r_wnd_darkthemechildproc, MAKELPARAM (is_darktheme, 0));

			//typedef VOID (WINAPI* RICPS) (VOID); // RefreshImmersiveColorPolicyState
			//const RICPS _RefreshImmersiveColorPolicyState = (RICPS)GetProcAddress (huxtheme, MAKEINTRESOURCEA (104));

			//if (_RefreshImmersiveColorPolicyState)
			//	_RefreshImmersiveColorPolicyState ();

			//typedef BOOL (WINAPI* GIICUHC) (IMMERSIVE_HC_CACHE_MODE); // GetIsImmersiveColorUsingHighContrast
			//const GIICUHC _GetIsImmersiveColorUsingHighContrast = (GIICUHC)GetProcAddress (huxtheme, MAKEINTRESOURCEA (106));

			//if (_GetIsImmersiveColorUsingHighContrast)
			//	_GetIsImmersiveColorUsingHighContrast (IHCM_REFRESH);

			InvalidateRect (hwnd, NULL, TRUE); // HACK!!!
		}

		FreeLibrary (huxtheme);
	}
}
#endif // _APP_HAVE_DARKTHEME

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent, LPCWSTR proxy_addr)
{
	BOOLEAN is_win81 = _r_sys_isosversiongreaterorequal (WINDOWS_8_1);
	HINTERNET hsession = WinHttpOpen (useragent, (is_win81 && _r_str_isempty (proxy_addr)) ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return NULL;

	// enable secure protocols
	ULONG option = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

	if (is_win81)
		option |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3; // tls 1.3 for win81+

	WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &option, sizeof (option));

	// enable compression feature (win81+)
	if (is_win81)
	{
		option = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
		WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &option, sizeof (option));

		// enable http2 protocol (win10rs1+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS1))
		{
			option = WINHTTP_PROTOCOL_FLAG_HTTP2;
			WinHttpSetOption (hsession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &option, sizeof (option));
		}
	}

	return hsession;
}

ULONG _r_inet_openurl (HINTERNET hsession, LPCWSTR url, LPCWSTR proxy_addr, LPHINTERNET pconnect, LPHINTERNET prequest, PULONG ptotallength)
{
	if (!pconnect || !prequest)
		return ERROR_BAD_ARGUMENTS;

	WCHAR url_host[MAX_PATH];
	WCHAR url_path[MAX_PATH];
	WORD url_port;
	INT url_scheme;

	ULONG code = _r_inet_parseurl (url, &url_scheme, url_host, &url_port, url_path, NULL, NULL);

	if (code != ERROR_SUCCESS)
	{
		return code;
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
			ULONG flags = WINHTTP_FLAG_REFRESH;

			if (url_scheme == INTERNET_SCHEME_HTTPS)
				flags |= WINHTTP_FLAG_SECURE;

			HINTERNET hrequest = WinHttpOpenRequest (hconnect, NULL, url_path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

			if (!hrequest)
			{
				_r_inet_close (hconnect);
				return GetLastError ();
			}
			else
			{
				// disable "keep-alive" feature (win7+)
#if !defined(_APP_NO_DEPRECATIONS)
				if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
				{
					ULONG option = WINHTTP_DISABLE_KEEP_ALIVE;
					WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof (option));
				}
#else
				ULONG option = WINHTTP_DISABLE_KEEP_ALIVE;
				WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof (option));
#endif // !_APP_NO_DEPRECATIONS

				// set proxy configuration (if available)
				if (!_r_str_isempty (proxy_addr))
				{
					WCHAR proxy_host[MAX_PATH];
					WCHAR proxy_user[MAX_PATH];
					WCHAR proxy_pass[MAX_PATH];
					WORD proxy_port;

					if (_r_inet_parseurl (proxy_addr, NULL, proxy_host, &proxy_port, NULL, proxy_user, proxy_pass) == ERROR_SUCCESS)
					{
						WCHAR proxyAddress[MAX_PATH];
						_r_str_printf (proxyAddress, RTL_NUMBER_OF (proxyAddress), L"%s:%" TEXT (PRIu16), proxy_host, proxy_port);

						WINHTTP_PROXY_INFO wpi;

						wpi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
						wpi.lpszProxy = proxyAddress;
						wpi.lpszProxyBypass = NULL;

						WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY, &wpi, sizeof (wpi));

						// set proxy credentials (if exists)
						if (!_r_str_isempty (proxy_user))
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_USERNAME, proxy_user, (ULONG)_r_str_length (proxy_user));

						if (!_r_str_isempty (proxy_pass))
							WinHttpSetOption (hrequest, WINHTTP_OPTION_PROXY_PASSWORD, proxy_pass, (ULONG)_r_str_length (proxy_pass));
					}
				}

				ULONG attempts = 6;
				ULONG param;
				ULONG param_size;

				do
				{
					if (!WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0))
					{
						code = GetLastError ();

						if (code == ERROR_WINHTTP_CONNECTION_ERROR)
						{
							param = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

							if (!WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &param, sizeof (param)))
								break;
						}
						else if (code == ERROR_WINHTTP_RESEND_REQUEST)
						{
							continue;
						}
						else if (code == ERROR_WINHTTP_SECURE_FAILURE)
						{
							param = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;

							if (!WinHttpSetOption (hrequest, WINHTTP_OPTION_SECURITY_FLAGS, &param, sizeof (param)))
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
						if (!WinHttpReceiveResponse (hrequest, NULL))
						{
							code = GetLastError ();
						}
						else
						{
							param_size = sizeof (param);
							WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &param, &param_size, NULL);

							if (param >= HTTP_STATUS_OK && param <= HTTP_STATUS_PARTIAL_CONTENT)
							{
								if (ptotallength)
								{
									*ptotallength = 0;

									param_size = sizeof (ULONG);
									WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, ptotallength, &param_size, WINHTTP_NO_HEADER_INDEX);
								}

								*pconnect = hconnect;
								*prequest = hrequest;

								return ERROR_SUCCESS;
							}
						}
					}
				}
				while (--attempts);

				_r_inet_close (hrequest);
			}

			_r_inet_close (hconnect);
		}
	}

	return code;
}

BOOLEAN _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, ULONG buffer_length, PULONG preaded, PULONG ptotalreaded)
{
	ULONG readed;

	if (WinHttpReadData (hrequest, buffer, buffer_length, &readed))
	{
		if (preaded)
			*preaded = readed;

		if (ptotalreaded)
			*ptotalreaded += readed;

		if (readed)
			return TRUE;
	}

	return FALSE;
}

ULONG _r_inet_parseurl (LPCWSTR url, PINT scheme_ptr, LPWSTR host_ptr, LPWORD port_ptr, LPWSTR path_ptr, LPWSTR user_ptr, LPWSTR pass_ptr)
{
	if (_r_str_isempty (url) || (!scheme_ptr && !host_ptr && !port_ptr && !path_ptr && !user_ptr && !pass_ptr))
		return ERROR_BAD_ARGUMENTS;

	URL_COMPONENTS url_comp = {0};
	url_comp.dwStructSize = sizeof (url_comp);

	ULONG url_length = (ULONG)_r_str_length (url);
	ULONG max_length = MAX_PATH;

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

	if (!WinHttpCrackUrl (url, url_length, ICU_DECODE, &url_comp))
	{
		ULONG code = GetLastError ();

		if (code != ERROR_WINHTTP_UNRECOGNIZED_SCHEME)
			return code;

		PR_STRING addressFixed = _r_format_string (L"https://%s", url);

		if (addressFixed)
		{
			if (!WinHttpCrackUrl (addressFixed->Buffer, (ULONG)_r_obj_getstringlength (addressFixed), ICU_DECODE, &url_comp))
			{
				_r_obj_dereference (addressFixed);

				return GetLastError ();
			}

			_r_obj_dereference (addressFixed);
		}
	}

	if (scheme_ptr)
		*scheme_ptr = url_comp.nScheme;

	if (port_ptr)
		*port_ptr = url_comp.nPort;

	return ERROR_SUCCESS;
}

ULONG _r_inet_downloadurl (HINTERNET hsession, LPCWSTR proxy_addr, LPCWSTR url, LPVOID* lpdest, BOOLEAN is_filepath, _R_CALLBACK_HTTP_DOWNLOAD Callback, LONG_PTR lpdata)
{
	if (!hsession || !lpdest)
		return ERROR_BAD_ARGUMENTS;

	HINTERNET hconnect;
	HINTERNET hrequest;
	R_STRINGBUILDER bufferString = {0};
	HANDLE bufferHandle = NULL;
	PR_STRING decodedString;
	PR_BYTE contentBytes;
	ULONG readedValue;
	ULONG readedTotal;
	ULONG unused;
	ULONG lengthTotal;
	ULONG code;

	code = _r_inet_openurl (hsession, url, proxy_addr, &hconnect, &hrequest, &lengthTotal);

	if (code != ERROR_SUCCESS)
	{
		return code;
	}

	if (is_filepath)
	{
		bufferHandle = (HANDLE)*lpdest;
	}
	else
	{
		_r_obj_createstringbuilder (&bufferString);
	}

	contentBytes = _r_obj_createbyteex (NULL, 0x8000);

	while (_r_inet_readrequest (hrequest, contentBytes->Buffer, (ULONG)contentBytes->Length, &readedValue, &readedTotal))
	{
		*(PCHAR)PTR_ADD_OFFSET (contentBytes->Buffer, readedValue) = ANSI_NULL; // terminate

		if (is_filepath)
		{
			if (!WriteFile (bufferHandle, contentBytes->Buffer, readedValue, &unused, NULL))
			{
				code = GetLastError ();
				break;
			}
		}
		else
		{
			decodedString = _r_str_multibyte2unicode (contentBytes->Buffer);

			if (decodedString)
			{
				_r_string_append2 (&bufferString, decodedString);
				_r_obj_dereference (decodedString);
			}
			else
			{
				code = ERROR_CANCELLED;
				break;
			}
		}

		if (Callback && !Callback (readedTotal, max (readedTotal, lengthTotal), lpdata))
		{
			code = ERROR_CANCELLED;
			break;
		}
	}

	if (!is_filepath)
	{
		if (code == ERROR_SUCCESS)
		{
			*lpdest = _r_obj_finalstringbuilder (&bufferString);
		}
		else
		{
			_r_obj_deletestringbuilder (&bufferString);
		}
	}

	_r_obj_dereference (contentBytes);

	_r_inet_close (hrequest);
	_r_inet_close (hconnect);

	return code;
}

/*
	Registry
*/

PR_BYTE _r_reg_querybinary (HKEY hkey, LPCWSTR value)
{
	ULONG type;
	ULONG size;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_BINARY)
		{
			PR_BYTE pbuffer = _r_obj_createbyteex (NULL, size);

			if (RegQueryValueEx (hkey, value, NULL, NULL, (LPBYTE)pbuffer->Buffer, &size) == ERROR_SUCCESS)
				return pbuffer;

			_r_obj_dereference (pbuffer); // cleanup
		}
	}

	return NULL;
}

ULONG _r_reg_queryulong (HKEY hkey, LPCWSTR value)
{
	ULONG type;
	ULONG size;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_DWORD)
		{
			ULONG buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (ULONG)buffer;
		}
	}

	return 0;
}

ULONG64 _r_reg_queryulong64 (HKEY hkey, LPCWSTR value)
{
	ULONG type;
	ULONG size;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_DWORD)
		{
			ULONG buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (ULONG64)buffer;
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
	}

	return 0;
}

PR_STRING _r_reg_querystring (HKEY hkey, LPCWSTR value)
{
	ULONG type;
	ULONG size;

	if (RegGetValue (hkey, NULL, value, RRF_RT_ANY, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
		{
			PR_STRING valueString = _r_obj_createstringex (NULL, size * sizeof (WCHAR));
			ULONG code = RegGetValue (hkey, NULL, value, RRF_RT_ANY, NULL, (PBYTE)valueString->Buffer, &size);

			if (code == ERROR_MORE_DATA)
			{
				_r_obj_movereference (&valueString, _r_obj_createstringex (NULL, size * sizeof (WCHAR)));

				code = RegGetValue (hkey, NULL, value, RRF_RT_ANY, NULL, (PBYTE)valueString->Buffer, &size);
			}

			if (code == ERROR_SUCCESS)
			{
				_r_string_trimtonullterminator (valueString);

				return valueString;
			}

			_r_obj_dereference (valueString);
		}
		else if (type == REG_DWORD)
		{
			ULONG buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
			{
				return _r_format_string (L"%" TEXT (PR_ULONG), buffer);
			}
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
			{
				return _r_format_string (L"%" TEXT (PR_ULONG64), buffer);
			}
		}
	}

	return NULL;
}

ULONG _r_reg_querysubkeylength (HKEY hkey)
{
	ULONG max_subkey_length;

	if (RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, &max_subkey_length, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		return max_subkey_length;

	return 0;
}

time_t _r_reg_querytimestamp (HKEY hkey)
{
	FILETIME ft;

	if (RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &ft) == ERROR_SUCCESS)
		return _r_unixtime_from_filetime (&ft);

	return 0;
}

/*
	Other
*/

HICON _r_loadicon (HINSTANCE hinst, LPCWSTR name, INT size)
{
	HICON hicon;

#if defined(_APP_NO_DEPRECATIONS)

	if (SUCCEEDED (LoadIconWithScaleDown (hinst, name, size, size, &hicon)))
		return hicon;

	return NULL;

#else

	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		HMODULE hcomctl32 = GetModuleHandle (L"comctl32.dll");

		if (hcomctl32)
		{
			typedef HRESULT (WINAPI *LIWSD)(HINSTANCE hinst, LPCWSTR pszName, INT cx, INT cy, HICON *phico); // vista+
			const LIWSD _LoadIconWithScaleDown = (LIWSD)GetProcAddress (hcomctl32, "LoadIconWithScaleDown");

			if (_LoadIconWithScaleDown)
			{
				if (SUCCEEDED (_LoadIconWithScaleDown (hinst, name, size, size, &hicon)))
					return hicon;
			}
		}
	}

	return (HICON)LoadImage (hinst, name, IMAGE_ICON, size, size, 0);

#endif // _APP_NO_DEPRECATIONS
}

PVOID _r_loadresource (HINSTANCE hinst, LPCWSTR name, LPCWSTR type, PULONG psize)
{
	HRSRC hres = FindResource (hinst, name, type);

	if (hres)
	{
		HGLOBAL hloaded = LoadResource (hinst, hres);

		if (hloaded)
		{
			PVOID pLockedResource = LockResource (hloaded);

			if (pLockedResource)
			{
				if (psize)
					*psize = SizeofResource (hinst, hres);

				UnlockResource (pLockedResource);

				return pLockedResource;
			}
		}
	}

	return NULL;
}

VOID _r_parseini (LPCWSTR path, OBJECTS_STRINGS_MAP2* outputMap, OBJECTS_STRINGS_VEC* sectionArr)
{
	OBJECTS_STRINGS_MAP1* valuesMap;
	PR_STRING sectionsString;
	PR_STRING valuesString;
	PR_STRING sectionString;
	PR_STRING keyName;
	PR_STRING keyValue;
	LPCWSTR sectionBuffer;
	LPCWSTR valueBuffer;
	SIZE_T sectionLength;
	SIZE_T valueLength;
	SIZE_T delimeterPos;
	ULONG outLength;
	ULONG allocationLength;

	// get section names
	allocationLength = 0x0800;

	sectionsString = _r_obj_createstringex (NULL, allocationLength * sizeof (WCHAR));
	outLength = GetPrivateProfileSectionNames (sectionsString->Buffer, allocationLength, path);

	if (!outLength)
	{
		_r_obj_dereference (sectionsString);
		return;
	}

	_r_string_setsize (sectionsString, outLength * sizeof (WCHAR));

	allocationLength = 0x7FFF; // maximum length for GetPrivateProfileSection
	valuesString = _r_obj_createstringex (NULL, allocationLength * sizeof (WCHAR));

	sectionBuffer = sectionsString->Buffer;

	// get section values
	while (!_r_str_isempty (sectionBuffer))
	{
		sectionLength = _r_str_length (sectionBuffer);
		sectionString = _r_obj_createstringex (sectionBuffer, sectionLength * sizeof (WCHAR));

		if (outputMap->find (sectionString) == outputMap->end ())
		{
			outputMap->emplace (_r_obj_reference (sectionString), NULL);

			if (sectionArr)
				sectionArr->push_back (_r_obj_reference (sectionString));
		}

		valuesMap = &outputMap->at (sectionString);

		outLength = GetPrivateProfileSection (sectionBuffer, valuesString->Buffer, allocationLength, path);
		_r_string_setsize (valuesString, outLength * sizeof (WCHAR));

		valueBuffer = valuesString->Buffer;

		while (!_r_str_isempty (valueBuffer))
		{
			valueLength = _r_str_length (valueBuffer);
			delimeterPos = _r_str_findchar (valueBuffer, valueLength, L'=');

			if (delimeterPos != INVALID_SIZE_T)
			{
				// do not load comments
				if (valueBuffer[0] != L'#')
				{
					keyName = _r_str_extractex (valueBuffer, valueLength, 0, delimeterPos);

					// check for duplicates
					if (valuesMap->find (keyName) == valuesMap->end ())
					{
						keyValue = _r_str_extractex (valueBuffer, valueLength, delimeterPos + 1, valueLength - delimeterPos - 1);

						valuesMap->emplace (keyName, keyValue); // set
					}
					else
					{
						_r_obj_dereference (keyName);
					}
				}
			}

			valueBuffer += valueLength + 1; // go next value
		}

		_r_obj_dereference (sectionString);

		sectionBuffer += sectionLength + 1; // go next section
	}

	_r_obj_dereference (sectionsString);
	_r_obj_dereference (valuesString);
}

ULONG _r_rand (ULONG min_number, ULONG max_number)
{
	static ULONG seed = 0; // save seed

	return min_number + (RtlRandomEx (&seed) % (max_number - min_number + 1));
}

VOID _r_sleep (LONG64 milliseconds)
{
	if (!milliseconds || milliseconds == INFINITE)
		return;

	LARGE_INTEGER interval = {0};
	interval.QuadPart = -(LONG64)UInt32x32To64 (milliseconds, ((1 * 10) * 1000));

	NtDelayExecution (FALSE, &interval);
}

/*
	System tray
*/

BOOLEAN _r_tray_create (HWND hwnd, UINT uid, UINT code, HICON hicon, LPCWSTR tooltip, BOOLEAN is_hidden)
{
	NOTIFYICONDATA nid = {0};

#if defined(_APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	static BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // _APP_NO_DEPRECATIONS

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
#if defined(_APP_NO_DEPRECATIONS)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // _APP_NO_DEPRECATIONS

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	if (is_hidden)
	{
		nid.uFlags |= NIF_STATE;

		nid.dwState = NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
	}

	if (Shell_NotifyIcon (NIM_ADD, &nid))
	{
#if defined(_APP_NO_DEPRECATIONS)
		nid.uVersion = NOTIFYICON_VERSION_4;
#else
		nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_DEPRECATIONS

		Shell_NotifyIcon (NIM_SETVERSION, &nid);

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_tray_popup (HWND hwnd, UINT uid, ULONG icon_id, LPCWSTR title, LPCWSTR text)
{
	NOTIFYICONDATA nid = {0};

#if defined(_APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	static BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // _APP_NO_DEPRECATIONS

	nid.uFlags = NIF_INFO | NIF_REALTIME;
	nid.dwInfoFlags = icon_id;
	nid.hWnd = hwnd;
	nid.uID = uid;

	if (title)
		_r_str_copy (nid.szInfoTitle, RTL_NUMBER_OF (nid.szInfoTitle), title);

	if (text)
		_r_str_copy (nid.szInfo, RTL_NUMBER_OF (nid.szInfo), text);

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_popupformat (HWND hwnd, UINT uid, ULONG icon_id, LPCWSTR title, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;
	BOOLEAN status;

	if (!format)
		return FALSE;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	status = _r_tray_popup (hwnd, uid, icon_id, title, string->Buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_setinfo (HWND hwnd, UINT uid, HICON hicon, LPCWSTR tooltip)
{
	NOTIFYICONDATA nid = {0};

#if defined(_APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	static BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // _APP_NO_DEPRECATIONS

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (hicon)
	{
		nid.uFlags |= NIF_ICON;
		nid.hIcon = hicon;
	}

	if (tooltip)
	{
#if defined(_APP_NO_DEPRECATIONS)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // _APP_NO_DEPRECATIONS

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_setinfoformat (HWND hwnd, UINT uid, HICON hicon, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;
	BOOLEAN status;

	if (!format)
		return FALSE;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	status = _r_tray_setinfo (hwnd, uid, hicon, string->Buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_toggle (HWND hwnd, UINT uid, BOOLEAN is_show)
{
	NOTIFYICONDATA nid = {0};

#if defined(_APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	static BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // _APP_NO_DEPRECATIONS

	nid.uFlags = NIF_STATE;
	nid.hWnd = hwnd;
	nid.uID = uid;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_destroy (HWND hwnd, UINT uid)
{
	NOTIFYICONDATA nid = {0};

#if defined(_APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	static BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // _APP_NO_DEPRECATIONS

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

PR_STRING _r_ctrl_gettext (HWND hwnd, INT ctrl_id)
{
	PR_STRING string;
	ULONG length;

	length = (ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);

	if (length)
	{
		string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

		if ((ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXT, (WPARAM)(length + 1), (LPARAM)string->Buffer))
		{
			_r_string_setsize (string, length * sizeof (WCHAR));
			return string;
		}

		_r_obj_dereference (string);
	}

	return NULL;
}

VOID _r_ctrl_settextformat (HWND hwnd, INT ctrl_id, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_ctrl_settext (hwnd, ctrl_id, string->Buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_setbuttonmargins (HWND hwnd, INT ctrl_id)
{
	// set button text margin
	{
		INT padding = _r_dc_getdpi (hwnd, 4);
		RECT paddingRect;

		SetRect (&paddingRect, padding, 0, padding, 0);

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)&paddingRect);
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

VOID _r_ctrl_settabletext (HWND hwnd, INT ctrl_id1, LPCWSTR text1, INT ctrl_id2, LPCWSTR text2)
{
	RECT windowRect;
	RECT controlRect;

	HWND hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	HWND hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	HDC hdc = GetDC (hwnd);

	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendDlgItemMessage (hwnd, ctrl_id1, WM_GETFONT, 0, 0)); // fix
		SelectObject (hdc, (HFONT)SendDlgItemMessage (hwnd, ctrl_id2, WM_GETFONT, 0, 0)); // fix
	}

	GetClientRect (hwnd, &windowRect);
	GetWindowRect (hctrl1, &controlRect);

	MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)&controlRect, 2);

	INT wnd_spacing = controlRect.left;
	INT wnd_width = _r_calc_rectwidth (INT, &windowRect) - (wnd_spacing * 2);

	INT ctrl1_width = _r_dc_fontwidth (hdc, text1, _r_str_length (text1)) + wnd_spacing;
	INT ctrl2_width = _r_dc_fontwidth (hdc, text2, _r_str_length (text2)) + wnd_spacing;

	ctrl2_width = min (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);
	ctrl1_width = min (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing); // note: changed order for correct priority!

	_r_ctrl_settext (hwnd, ctrl_id1, text1);
	_r_ctrl_settext (hwnd, ctrl_id2, text2);

	HDWP hdefer = BeginDeferWindowPos (2);

	hdefer = DeferWindowPos (hdefer, hctrl1, NULL, wnd_spacing, controlRect.top, ctrl1_width, _r_calc_rectheight (INT, &controlRect), SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
	hdefer = DeferWindowPos (hdefer, hctrl2, NULL, wnd_width - ctrl2_width, controlRect.top, ctrl2_width + wnd_spacing, _r_calc_rectheight (INT, &controlRect), SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

	EndDeferWindowPos (hdefer);

	if (hdc)
		ReleaseDC (hwnd, hdc);
}

HWND _r_ctrl_createtip (HWND hparent)
{
	HWND htip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_CHILD | WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hparent, NULL, GetModuleHandle (NULL), NULL);

	if (htip)
	{
		_r_ctrl_settipstyle (htip);

		SendMessage (htip, TTM_ACTIVATE, TRUE, 0);
	}

	return htip;
}

VOID _r_ctrl_settiptext (HWND htip, HWND hparent, INT ctrl_id, LPCWSTR text)
{
	TOOLINFO ti = {0};

	ti.cbSize = sizeof (ti);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = hparent;
	ti.hinst = GetModuleHandle (NULL);
	ti.uId = (UINT_PTR)GetDlgItem (hparent, ctrl_id);
	ti.lpszText = (LPWSTR)text;

	GetClientRect (hparent, &ti.rect);

	SendMessage (htip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

VOID _r_ctrl_settiptextformat (HWND htip, HWND hparent, INT ctrl_id, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_ctrl_settiptext (htip, hparent, ctrl_id, string->Buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_settipstyle (HWND htip)
{
	SendMessage (htip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);
	SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

	_r_wnd_top (htip, TRUE); // HACK!!!
}

VOID _r_ctrl_showballoontip (HWND hwnd, INT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR text)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon_id;

	SendDlgItemMessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

VOID _r_ctrl_showballoontipformat (HWND hwnd, INT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_ctrl_showballoontip (hwnd, ctrl_id, icon_id, title, string->Buffer);

	_r_obj_dereference (string);
}

/*
	Menu
*/

VOID _r_menu_checkitem (HMENU hmenu, UINT item_id_start, UINT item_id_end, UINT position_flag, UINT check_id)
{
	if (item_id_end)
	{
		CheckMenuRadioItem (hmenu, min (item_id_start, item_id_end), max (item_id_start, item_id_end), check_id, position_flag);
	}
	else
	{
		CheckMenuItem (hmenu, item_id_start, position_flag | ((check_id != 0) ? MF_CHECKED : MF_UNCHECKED));
	}
}

VOID _r_menu_setitembitmap (HMENU hmenu, UINT item_id, BOOL is_byposition, HBITMAP hbitmap)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbitmap;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtext (HMENU hmenu, UINT item_id, BOOL is_byposition, LPCWSTR text)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = (LPWSTR)text;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtextformat (HMENU hmenu, UINT item_id, BOOL is_byposition, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_menu_setitemtext (hmenu, item_id, is_byposition, string->Buffer);

	_r_obj_dereference (string);
}

INT _r_menu_popup (HMENU hmenu, HWND hwnd, LPPOINT lpmouse, BOOLEAN is_sendmessage)
{
	POINT pt;

	if (!lpmouse)
	{
		GetCursorPos (&pt);
		lpmouse = &pt;
	}

	INT command_id = TrackPopupMenu (hmenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON, lpmouse->x, lpmouse->y, 0, hwnd, NULL);

	if (is_sendmessage && command_id && hwnd)
		PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

	return command_id;
}

/*
	Control: tab
*/

VOID _r_tab_adjustchild (HWND hwnd, INT tab_id, HWND hchild)
{
	HWND htab = GetDlgItem (hwnd, tab_id);

	RECT tabRect;
	RECT listviewRect;

	if (!GetWindowRect (htab, &tabRect))
		return;

	MapWindowPoints (NULL, hwnd, (LPPOINT)&tabRect, 2);

	if (!GetClientRect (htab, &listviewRect))
		return;

	SetRect (&listviewRect, listviewRect.left + tabRect.left, listviewRect.top + tabRect.top, listviewRect.right + tabRect.left, listviewRect.bottom + tabRect.top);

	SendMessage (htab, TCM_ADJUSTRECT, FALSE, (LPARAM)&listviewRect);

	SetWindowPos (hchild, NULL, listviewRect.left, listviewRect.top, _r_calc_rectwidth (INT, &listviewRect), _r_calc_rectheight (INT, &listviewRect), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

INT _r_tab_additem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = (LPWSTR)text;
	}

	if (image != I_IMAGENONE)
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
	if (index == INVALID_INT)
	{
		index = (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETCURSEL, 0, 0);

		if (index == INVALID_INT)
			return 0;
	}

	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	if (!!SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEM, (WPARAM)index, (LPARAM)&tci))
		return tci.lParam;

	return 0;
}

INT _r_tab_setitem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = (LPWSTR)text;
	}

	if (image != I_IMAGENONE)
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

VOID _r_tab_selectitem (HWND hwnd, INT ctrl_id, INT index)
{
	NMHDR hdr;

	hdr.hwndFrom = GetDlgItem (hwnd, ctrl_id);
	hdr.idFrom = (UINT_PTR)(UINT)ctrl_id;

	hdr.code = TCN_SELCHANGING;
	SendMessage (hwnd, WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);

	SendDlgItemMessage (hwnd, ctrl_id, TCM_SETCURSEL, (WPARAM)index, 0);

	hdr.code = TCN_SELCHANGE;
	SendMessage (hwnd, WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
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
		lvc.pszText = (LPWSTR)title;
	}

	if (width)
	{
		if (width < 0 && (width != LVSCW_AUTOSIZE && width != LVSCW_AUTOSIZE_USEHEADER))
		{
			RECT listviewRect;

			if (GetClientRect (GetDlgItem (hwnd, ctrl_id), &listviewRect))
				width = _r_calc_percentval (INT, -width, _r_calc_rectwidth (INT, &listviewRect));
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

INT _r_listview_addgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT align, UINT state, UINT state_mask)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (lvg);
	lvg.mask = LVGF_GROUPID;
	lvg.iGroupId = group_id;

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = (LPWSTR)title;
	}

	if (align)
	{
		lvg.mask |= LVGF_ALIGN;
		lvg.uAlign = align;
	}

	if (state || state_mask)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
		lvg.stateMask = state_mask;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTGROUP, (WPARAM)group_id, (LPARAM)&lvg);
}

INT _r_listview_additemex (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam)
{
	if (item == INVALID_INT)
	{
		item = _r_listview_getitemcount (hwnd, ctrl_id, FALSE);

		if (subitem)
			item -= 1;
	}

	LVITEM lvi = {0};

	lvi.iItem = item;
	lvi.iSubItem = subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = (LPWSTR)text;
	}

	if (!subitem)
	{
		if (image != I_IMAGENONE)
		{
			lvi.mask |= LVIF_IMAGE;
			lvi.iImage = image;
		}

		if (group_id != I_GROUPIDNONE)
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

VOID _r_listview_deleteallcolumns (HWND hwnd, INT ctrl_id)
{
	INT column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	for (INT i = column_count; i >= 0; i--)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
}

VOID _r_listview_deleteallgroups (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
}

VOID _r_listview_deleteallitems (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

INT _r_listview_getcolumncount (HWND hwnd, INT ctrl_id)
{
	HWND hheader = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (hheader)
		return (INT)SendMessage (hheader, HDM_GETITEMCOUNT, 0, 0);

	return 0;
}

PR_STRING _r_listview_getcolumntext (HWND hwnd, INT ctrl_id, INT column_id)
{
	LVCOLUMN lvc = {0};
	PR_STRING string;
	SIZE_T allocatedCount;

	allocatedCount = 0x200;
	string = _r_obj_createstringex (NULL, allocatedCount * sizeof (WCHAR));

	lvc.mask = LVCF_TEXT;
	lvc.pszText = string->Buffer;
	lvc.cchTextMax = (INT)allocatedCount + 1;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);

	_r_string_trimtonullterminator (string);

	return string;
}

INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl_id, INT column_id)
{
	RECT listviewRect;

	if (!GetClientRect (GetDlgItem (hwnd, ctrl_id), &listviewRect))
		return 0;

	INT total_width = _r_calc_rectwidth (INT, &listviewRect);
	INT column_width = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0);

	return _r_calc_percentof (INT, column_width, total_width);
}

INT _r_listview_getitemcount (HWND hwnd, INT ctrl_id, BOOLEAN is_listchecked)
{
	INT total_count = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, 0);

	if (is_listchecked)
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

PR_STRING _r_listview_getitemtext (HWND hwnd, INT ctrl_id, INT item, INT subitem)
{
	LVITEM lvi = {0};
	PR_STRING string = NULL;
	SIZE_T allocatedCount;
	SIZE_T count;

	allocatedCount = 0x100;
	count = allocatedCount;

	while (count >= allocatedCount)
	{
		allocatedCount *= 2;
		_r_obj_movereference (&string, _r_obj_createstringex (NULL, allocatedCount * sizeof (WCHAR)));

		lvi.iSubItem = subitem;
		lvi.pszText = string->Buffer;
		lvi.cchTextMax = (INT)allocatedCount + 1;

		count = (SIZE_T)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMTEXT, (WPARAM)item, (LPARAM)&lvi);
	}

	_r_string_trimtonullterminator (string);

	return string;
}

BOOLEAN _r_listview_isitemchecked (HWND hwnd, INT ctrl_id, INT item)
{
	return !!(((INT)(SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item, LVIS_STATEIMAGEMASK)) == INDEXTOSTATEIMAGEMASK (2)));
}

BOOLEAN _r_listview_isitemvisible (HWND hwnd, INT ctrl_id, INT item)
{
	return !!((INT)(SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, (WPARAM)item, 0)));
}

VOID _r_listview_redraw (HWND hwnd, INT ctrl_id, INT item_id)
{
	if (item_id != INVALID_INT)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, (WPARAM)item_id, (LPARAM)item_id);
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, 0, (LPARAM)_r_listview_getitemcount (hwnd, ctrl_id, FALSE));
	}
}

VOID _r_listview_setcolumn (HWND hwnd, INT ctrl_id, INT column_id, LPCWSTR text, INT width)
{
	LVCOLUMN lvc = {0};

	if (text)
	{
		lvc.mask |= LVCF_TEXT;
		lvc.pszText = (LPWSTR)text;
	}

	if (width)
	{
		if (width < 0 && width != LVSCW_AUTOSIZE && width != LVSCW_AUTOSIZE_USEHEADER)
		{
			RECT listviewRect;

			if (GetClientRect (GetDlgItem (hwnd, ctrl_id), &listviewRect))
				width = _r_calc_percentval (INT, -width, _r_calc_rectwidth (INT, &listviewRect));
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

VOID _r_listview_setcolumnsortindex (HWND hwnd, INT ctrl_id, INT column_id, INT arrow)
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

VOID _r_listview_setitemex (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam)
{
	LVITEM lvi = {0};

	lvi.iItem = item;
	lvi.iSubItem = subitem;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = (LPWSTR)text;
	}

	if (!subitem)
	{
		if (image != I_IMAGENONE)
		{
			lvi.mask |= LVIF_IMAGE;
			lvi.iImage = image;
		}

		if (group_id != I_GROUPIDNONE)
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

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEM, 0, (LPARAM)&lvi);
}

VOID _r_listview_setitemcheck (HWND hwnd, INT ctrl_id, INT item, BOOLEAN is_check)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (is_check ? 2 : 1);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item, (LPARAM)&lvi);
}

VOID _r_listview_setgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT state, UINT state_mask)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (lvg);

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = (LPWSTR)title;
	}

	if (state || state_mask)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
		lvg.stateMask = state_mask;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETGROUPINFO, (WPARAM)group_id, (LPARAM)&lvg);
}

VOID _r_listview_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle, BOOL is_groupview)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", NULL);

	HWND htip = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	if (exstyle)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)exstyle);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, (WPARAM)is_groupview, 0);
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
		tvi.itemex.pszText = (LPWSTR)text;
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

VOID _r_treeview_setitem (HWND hwnd, INT ctrl_id, HTREEITEM hitem, LPCWSTR text, INT image, LPARAM lparam)
{
	TVITEMEX tvi = {0};

	tvi.hItem = hitem;

	if (text)
	{
		tvi.mask |= TVIF_TEXT;
		tvi.pszText = (LPWSTR)text;
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

VOID _r_treeview_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle, INT height, INT indent)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", NULL);

	HWND htip = (HWND)SendDlgItemMessage (hwnd, ctrl_id, TVM_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	if (exstyle)
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);

	if (indent)
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETINDENT, (WPARAM)indent, 0);

	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, TVM_SETITEMHEIGHT, (WPARAM)height, 0);
}

/*
	Control: statusbar
*/

VOID _r_status_settext (HWND hwnd, INT ctrl_id, INT part, LPCWSTR text)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTIPTEXT, (WPARAM)part, (LPARAM)text);
}

VOID _r_status_settextformat (HWND hwnd, INT ctrl_id, INT part, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
		return;

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_status_settext (hwnd, ctrl_id, part, string->Buffer);

	_r_obj_dereference (string);
}

VOID _r_status_setstyle (HWND hwnd, INT ctrl_id, INT height)
{
	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, SB_SETMINHEIGHT, (WPARAM)height, 0);

	SendDlgItemMessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

/*
	Control: toolbar
*/

VOID _r_toolbar_addbutton (HWND hwnd, INT ctrl_id, UINT command_id, INT style, INT_PTR text, INT state, INT image)
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
		width += _r_calc_rectwidth (INT, &rc);
	}

	return width;
}

VOID _r_toolbar_setbutton (HWND hwnd, INT ctrl_id, UINT command_id, LPCWSTR text, INT style, INT state, INT image)
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
		tbi.pszText = (LPWSTR)text;
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

VOID _r_toolbar_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", NULL);

	HWND htip = (HWND)SendDlgItemMessage (hwnd, ctrl_id, TB_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);
	SendDlgItemMessage (hwnd, ctrl_id, TB_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: progress bar
*/

VOID _r_progress_setmarquee (HWND hwnd, INT ctrl_id, BOOL is_enable)
{
	SendDlgItemMessage (hwnd, ctrl_id, PBM_SETMARQUEE, (WPARAM)is_enable, (LPARAM)10);

	_r_wnd_addstyle (hwnd, ctrl_id, is_enable ? PBS_MARQUEE : 0, PBS_MARQUEE, GWL_STYLE);
}

/*
	Util
*/

VOID _r_util_templatewritestring (BYTE **pPtr, LPCWSTR string)
{
	SIZE_T length = _r_str_length (string) * sizeof (WCHAR);

	*(PWCHAR)PTR_ADD_OFFSET (*pPtr, length) = UNICODE_NULL; // terminate

	_r_util_templatewrite (pPtr, string, length + sizeof (UNICODE_NULL));
}

VOID _r_util_templatewritecontrol (BYTE **pPtr, ULONG ctrl_id, ULONG style, SHORT x, SHORT y, SHORT cx, SHORT cy, LPCWSTR class_name)
{
	*pPtr = (LPBYTE)((ULONG_PTR)(*pPtr + 3) & ~3); // align as DWORD

	// fill DLGITEMTEMPLATEEX
	_r_util_templatewriteulong (pPtr, 0); // helpID
	_r_util_templatewriteulong (pPtr, 0); // exStyle
	_r_util_templatewriteulong (pPtr, style); // style

	_r_util_templatewriteshort (pPtr, x); // x
	_r_util_templatewriteshort (pPtr, y); // y
	_r_util_templatewriteshort (pPtr, cx); // cx
	_r_util_templatewriteshort (pPtr, cy); // cy

	_r_util_templatewriteulong (pPtr, ctrl_id); // id

	_r_util_templatewritestring (pPtr, class_name); // windowClass
	_r_util_templatewritestring (pPtr, L""); // title

	_r_util_templatewriteshort (pPtr, 0); // extraCount
}

PR_STRING _r_util_versionformat (PR_STRING string)
{
	if (_r_str_isnumeric (_r_obj_getstring (string)))
	{
		UINT length = 0x100;
		PR_STRING dateString = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

		_r_format_dateex (dateString->Buffer, length, _r_str_tolong64 (string->Buffer), FDTF_SHORTDATE | FDTF_SHORTTIME);
		_r_string_trimtonullterminator (dateString);

		return dateString;
	}

	return _r_obj_createstring2 (string);
}

BOOL CALLBACK _r_util_activate_window_callback (HWND hwnd, LPARAM lparam)
{
	LPCWSTR app_name = (LPCWSTR)lparam;

	if (_r_str_isempty (app_name))
		return FALSE;

	ULONG pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

	if (GetCurrentProcessId () == pid)
		return TRUE;

	// check window class
	WCHAR class_name[64];

	if (GetClassName (hwnd, class_name, RTL_NUMBER_OF (class_name)) && _r_str_compare_length (class_name, L"#32770", 6) == 0)
	{
		// check window title
		WCHAR window_title[128];

		if (GetWindowText (hwnd, window_title, RTL_NUMBER_OF (window_title)) && _r_str_compare_length (window_title, app_name, _r_str_length (app_name)) == 0)
		{
			// check window prop
			if (GetProp (hwnd, app_name))
			{
				_r_wnd_toggle (hwnd, TRUE);
				return FALSE;
			}
		}
	}

	return TRUE;
}

VOID _r_util_clear_objects_strings_map1 (OBJECTS_STRINGS_MAP1* ptr_map)
{
	PVOID pdata1;
	PVOID pdata2;

	for (auto it = ptr_map->begin (); it != ptr_map->end ();)
	{
		pdata1 = it->first;
		pdata2 = it->second;

		it = ptr_map->erase (it);

		if (pdata1)
			_r_obj_dereference (pdata1);

		if (pdata2)
			_r_obj_dereference (pdata2);
	}
}

VOID _r_util_clear_objects_strings_map2 (OBJECTS_STRINGS_MAP2* ptr_map)
{
	PVOID pdata;

	for (auto it = ptr_map->begin (); it != ptr_map->end ();)
	{
		pdata = it->first;

		_r_util_clear_objects_strings_map1 (&it->second);

		it = ptr_map->erase (it);

		if (pdata)
			_r_obj_dereference (pdata);
	}
}

VOID _r_util_clear_objects_strings_vector (OBJECTS_STRINGS_VEC* ptr_vec)
{
	PVOID pdata;

	for (auto it = ptr_vec->begin (); it != ptr_vec->end ();)
	{
		pdata = *it;

		it = ptr_vec->erase (it);

		if (pdata)
			_r_obj_dereference (pdata);
	}
}