// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#include "routine.h"

/*
	Debugging
*/

VOID _r_debug_v (_In_ _Printf_format_string_ LPCWSTR format, ...)
{
	WCHAR string[512];
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	_r_debug (string);
}

/*
	Format strings, dates, numbers
*/

_Ret_maybenull_
PR_STRING _r_format_string (_In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return NULL;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	return string;
}

_Ret_maybenull_
PR_STRING _r_format_string_v (_In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr)
{
	PR_STRING string;
	INT length;

	if (!format)
		return NULL;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return NULL;

	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (string->buffer, length, format, arg_ptr);
#pragma warning(pop)

	return string;
}

_Success_ (return)
BOOLEAN _r_format_bytesize64 (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ ULONG64 bytes)
{
	if (!buffer || !buffer_size)
		return FALSE;

#if defined(APP_NO_DEPRECATIONS)
	if (SUCCEEDED (StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, buffer_size))) // vista (sp1)+
		return TRUE;
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		HMODULE hshlwapi = GetModuleHandle (L"shlwapi.dll");

		if (hshlwapi)
		{
			typedef HRESULT (WINAPI *SFBSE)(ULONG64 ull, SFBS_FLAGS flags, LPWSTR pszBuf, INT cchBuf); // vista+
			const SFBSE _StrFormatByteSizeEx = (SFBSE)GetProcAddress (hshlwapi, "StrFormatByteSizeEx");

			if (_StrFormatByteSizeEx)
			{
				if (SUCCEEDED (_StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, buffer_size))) // vista (sp1)+
					return TRUE;
			}
		}
	}

	if (StrFormatByteSizeW ((LONG64)bytes, buffer, buffer_size)) // fallback
		return TRUE;
#endif // APP_NO_DEPRECATIONS

	*buffer = UNICODE_NULL;

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_format_filetimeex (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LPFILETIME file_time, _In_ ULONG flags)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (SHFormatDateTime (file_time, &flags, buffer, buffer_size))
		return TRUE;

	*buffer = UNICODE_NULL;

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_format_unixtimeex (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 unixtime, _In_ ULONG flags)
{
	if (!buffer || !buffer_size)
		return FALSE;

	FILETIME file_time;
	_r_unixtime_to_filetime (unixtime, &file_time);

	return _r_format_filetimeex (buffer, buffer_size, &file_time, flags);
}

_Success_ (return)
BOOLEAN _r_format_interval (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 seconds, _In_ INT digits)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (!StrFromTimeInterval (buffer, buffer_size, _r_calc_seconds2milliseconds ((LONG)seconds), digits))
		_r_str_fromlong64 (buffer, buffer_size, seconds);

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_format_number (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 number)
{
	if (!buffer || !buffer_size)
		return FALSE;

	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR decimal_separator[4] = {0};
	static WCHAR thousand_separator[4] = {0};

	NUMBERFMT number_format = {0};
	WCHAR number_string[128];

	if (_r_initonce_begin (&init_once))
	{
		if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimal_separator, RTL_NUMBER_OF (decimal_separator)))
		{
			decimal_separator[0] = L'.';
			decimal_separator[1] = UNICODE_NULL;
		}

		if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousand_separator, RTL_NUMBER_OF (thousand_separator)))
		{
			thousand_separator[0] = L',';
			thousand_separator[1] = UNICODE_NULL;
		}

		_r_initonce_end (&init_once);
	}

	number_format.Grouping = 3;
	number_format.lpDecimalSep = decimal_separator;
	number_format.lpThousandSep = thousand_separator;
	number_format.NegativeOrder = 1;

	_r_str_fromlong64 (number_string, RTL_NUMBER_OF (number_string), number);

	if (!GetNumberFormat (LOCALE_USER_DEFAULT, 0, number_string, &number_format, buffer, buffer_size))
		_r_str_copy (buffer, buffer_size, number_string);

	return TRUE;
}

/*
	Synchronization: Spinlock
*/

/*
	FastLock is a port of FastResourceLock from PH 1.x.
	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.
	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.
	https://github.com/processhacker2/processhacker
*/

#if !defined(APP_NO_DEPRECATIONS)
VOID _r_spinlock_initialize (PR_SPINLOCK plock)
{
	plock->value = 0;

	NtCreateSemaphore (&plock->exclusive_wake_event, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);
	NtCreateSemaphore (&plock->shared_wake_event, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);
}

VOID _r_spinlock_acquireexclusive (PR_SPINLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	ULONG spin_count = _r_spinlock_getspincount ();

	while (TRUE)
	{
		value = plock->value;

		if (!(value & (PR_SPINLOCK_OWNED | PR_SPINLOCK_EXCLUSIVE_WAKING)))
		{
			if (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_OWNED, value) == value)
				break;
		}
		else if (i >= spin_count)
		{
			_r_spinlock_ensureeventcreated (&plock->exclusive_wake_event);

			if (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->exclusive_wake_event, INFINITE, FALSE) != STATUS_WAIT_0)
					RtlRaiseStatus (STATUS_UNSUCCESSFUL);

				do
				{
					value = plock->value;
				}
				while (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_OWNED - PR_SPINLOCK_EXCLUSIVE_WAKING, value) != value);

				break;
			}
		}

		i += 1;
		YieldProcessor ();
	}
}

VOID _r_spinlock_acquireshared (PR_SPINLOCK plock)
{
	ULONG value;
	ULONG i = 0;
	ULONG spin_count = _r_spinlock_getspincount ();

	while (TRUE)
	{
		value = plock->value;

		if (!(value & (PR_SPINLOCK_OWNED | (PR_SPINLOCK_SHARED_OWNERS_MASK << PR_SPINLOCK_SHARED_OWNERS_SHIFT) | PR_SPINLOCK_EXCLUSIVE_MASK)))
		{
			if (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_OWNED + PR_SPINLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if ((value & PR_SPINLOCK_OWNED) && ((value >> PR_SPINLOCK_SHARED_OWNERS_SHIFT) & PR_SPINLOCK_SHARED_OWNERS_MASK) > 0 && !(value & PR_SPINLOCK_EXCLUSIVE_MASK))
		{
			if (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if (i >= spin_count)
		{
			_r_spinlock_ensureeventcreated (&plock->shared_wake_event);

			if (InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_SHARED_WAITERS_INC, value) == value)
			{
				if (WaitForSingleObjectEx (plock->shared_wake_event, INFINITE, FALSE) != STATUS_WAIT_0)
					RtlRaiseStatus (STATUS_UNSUCCESSFUL);

				continue;
			}
		}

		i += 1;
		YieldProcessor ();
	}
}

VOID _r_spinlock_releaseexclusive (PR_SPINLOCK plock)
{
	ULONG value;

	while (TRUE)
	{
		value = plock->value;

		if ((value >> PR_SPINLOCK_EXCLUSIVE_WAITERS_SHIFT) & PR_SPINLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (InterlockedCompareExchange (&plock->value, value - PR_SPINLOCK_OWNED + PR_SPINLOCK_EXCLUSIVE_WAKING - PR_SPINLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				NtReleaseSemaphore (plock->exclusive_wake_event, 1, NULL);
				break;
			}
		}
		else
		{
			ULONG shared_waiters = (value >> PR_SPINLOCK_SHARED_WAITERS_SHIFT) & PR_SPINLOCK_SHARED_WAITERS_MASK;

			if (InterlockedCompareExchange (&plock->value, value & ~(PR_SPINLOCK_OWNED | (PR_SPINLOCK_SHARED_WAITERS_MASK << PR_SPINLOCK_SHARED_WAITERS_SHIFT)), value) == value)
			{
				if (shared_waiters)
					NtReleaseSemaphore (plock->shared_wake_event, shared_waiters, NULL);

				break;
			}
		}

		YieldProcessor ();
	}
}

VOID _r_spinlock_releaseshared (PR_SPINLOCK plock)
{
	ULONG value;

	while (TRUE)
	{
		value = plock->value;

		if (((value >> PR_SPINLOCK_SHARED_OWNERS_SHIFT) & PR_SPINLOCK_SHARED_OWNERS_MASK) > 1)
		{
			if (InterlockedCompareExchange (&plock->value, value - PR_SPINLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}
		else if ((value >> PR_SPINLOCK_EXCLUSIVE_WAITERS_SHIFT) & PR_SPINLOCK_EXCLUSIVE_WAITERS_MASK)
		{
			if (InterlockedCompareExchange (&plock->value, value - PR_SPINLOCK_OWNED + PR_SPINLOCK_EXCLUSIVE_WAKING - PR_SPINLOCK_SHARED_OWNERS_INC - PR_SPINLOCK_EXCLUSIVE_WAITERS_INC, value) == value)
			{
				NtReleaseSemaphore (plock->exclusive_wake_event, 1, NULL);
				break;
			}
		}
		else
		{
			if (InterlockedCompareExchange (&plock->value, value - PR_SPINLOCK_OWNED - PR_SPINLOCK_SHARED_OWNERS_INC, value) == value)
				break;
		}

		YieldProcessor ();
	}
}

BOOLEAN _r_spinlock_tryacquireexclusive (PR_SPINLOCK plock)
{
	ULONG value = plock->value;

	if (value & (PR_SPINLOCK_OWNED | PR_SPINLOCK_EXCLUSIVE_WAKING))
		return FALSE;

	return InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_OWNED, value) == value;
}

BOOLEAN _r_spinlock_tryacquireshared (PR_SPINLOCK plock)
{
	ULONG value = plock->value;

	if (value & PR_SPINLOCK_EXCLUSIVE_MASK)
		return FALSE;

	if (!(value & PR_SPINLOCK_OWNED))
	{
		return InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_OWNED + PR_SPINLOCK_SHARED_OWNERS_INC, value) == value;
	}
	else if ((value >> PR_SPINLOCK_SHARED_OWNERS_SHIFT) & PR_SPINLOCK_SHARED_OWNERS_MASK)
	{
		return InterlockedCompareExchange (&plock->value, value + PR_SPINLOCK_SHARED_OWNERS_INC, value) == value;
	}

	return FALSE;
}
#endif // !APP_NO_DEPRECATIONS

/*
	Synchronization: Timer
*/

#if !defined(APP_NO_DEPRECATIONS)
VOID FASTCALL _r_event_reset (_Inout_ PR_EVENT event)
{
	assert (!event->event_handle);

	if (_r_event_test (event))
		event->value = PR_EVENT_REFCOUNT_INC;
}

VOID FASTCALL _r_event_set (_Inout_ PR_EVENT event)
{
	HANDLE event_handle;

	if (!InterlockedBitTestAndSetPointer ((PLONG_PTR) & event->value, PR_EVENT_SET_SHIFT))
	{
		event_handle = event->event_handle;

		if (event_handle)
		{
			NtSetEvent (event_handle, NULL);
		}

		_r_event_dereference (event, event_handle);
	}
}

BOOLEAN FASTCALL _r_event_waitex (_Inout_ PR_EVENT event, _In_opt_ PLARGE_INTEGER timeout)
{
	BOOLEAN result;
	ULONG_PTR value;
	HANDLE event_handle;

	value = event->value;

	if ((value & PR_EVENT_SET) != 0)
		return TRUE;

	if (timeout && timeout->QuadPart == 0)
		return FALSE;

	_r_event_reference (event);

	event_handle = event->event_handle;

	if (!event_handle)
	{
		NtCreateEvent (&event_handle, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

		assert (event_handle);

		if (InterlockedCompareExchangePointer (&event->event_handle, event_handle, NULL) != NULL)
		{
			NtClose (event_handle);

			event_handle = event->event_handle;
		}
	}

	if ((event->value & PR_EVENT_SET) == 0)
	{
		result = (NtWaitForSingleObject (event_handle, FALSE, timeout) == STATUS_WAIT_0);
	}
	else
	{
		result = TRUE;
	}

	_r_event_dereference (event, event_handle);

	return result;
}
#endif // APP_NO_DEPRECATIONS

/*
	Synchronization: One-time initialization
*/

#if !defined(APP_NO_DEPRECATIONS)
BOOLEAN FASTCALL _r_initonce_beginex (_Inout_ PR_INITONCE init_once)
{
	if (!InterlockedBitTestAndSetPointer (&init_once->event.value, PR_INITONCE_INITIALIZING_SHIFT))
		return TRUE;

	_r_event_wait (&init_once->event, NULL);

	return FALSE;
}
#endif // APP_NO_DEPRECATIONS

/*
	Synchronization: Mutex
*/

BOOLEAN _r_mutex_isexists (_In_ LPCWSTR name)
{
	if (!name)
		return FALSE;

	HANDLE hmutex = OpenMutex (MUTANT_QUERY_STATE, FALSE, name);

	if (hmutex)
	{
		CloseHandle (hmutex);

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_mutex_create (_In_ LPCWSTR name, _Inout_ PHANDLE hmutex)
{
	HANDLE original_mutex;

	_r_mutex_destroy (hmutex);

	if (name)
	{
		original_mutex = CreateMutex (NULL, FALSE, name);

		if (original_mutex)
		{
			*hmutex = original_mutex;

			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _r_mutex_destroy (_Inout_ PHANDLE hmutex)
{
	HANDLE original_mutex = *hmutex;

	if (_r_fs_isvalidhandle (original_mutex))
	{
		*hmutex = NULL;

		ReleaseMutex (original_mutex);
		CloseHandle (original_mutex);

		return TRUE;
	}

	return FALSE;
}

/*
	Memory allocation
*/

HANDLE _r_mem_getheap ()
{
	static HANDLE heap_handle = NULL;
	HANDLE current_handle = InterlockedCompareExchangePointer (&heap_handle, NULL, NULL);

	if (!current_handle)
	{
		HANDLE new_handle = NULL;

		if (_r_sys_isosversiongreaterorequal (WINDOWS_8)) // win8+
		{
			new_handle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1 | HEAP_CREATE_SEGMENT_HEAP, NULL, 0, 0, NULL, NULL);
		}

		if (!new_handle)
		{
			new_handle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1, NULL, _r_calc_megabytes2bytes (2), _r_calc_megabytes2bytes (1), NULL, NULL);
		}

		if (new_handle)
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
			{
				RtlSetHeapInformation (new_handle, HeapCompatibilityInformation, &(ULONG){HEAP_COMPATIBILITY_LFH}, sizeof (ULONG));
			}

			current_handle = InterlockedCompareExchangePointer (&heap_handle, new_handle, NULL);

			if (!current_handle)
			{
				current_handle = new_handle;
			}
			else
			{
				RtlDestroyHeap (new_handle);
			}
		}
	}

	return current_handle;
}

/*
	Objects reference
*/

#define OBJECT_HEADER_TO_OBJECT(object_header) (&((PR_OBJECT_HEADER)(object_header))->body)
#define OBJECT_TO_OBJECT_HEADER(object) (CONTAINING_RECORD((object), R_OBJECT_HEADER, body))

_Post_writable_byte_size_ (bytes_count)
PVOID _r_obj_allocateex (_In_ SIZE_T bytes_count, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	PR_OBJECT_HEADER object_header = _r_mem_allocatezero (UFIELD_OFFSET (R_OBJECT_HEADER, body) + bytes_count);

	InterlockedIncrement (&object_header->ref_count);

	object_header->cleanup_callback = cleanup_callback;

	return OBJECT_HEADER_TO_OBJECT (object_header);
}

PVOID _r_obj_reference (_In_ PVOID object_body)
{
	PR_OBJECT_HEADER object_header = OBJECT_TO_OBJECT_HEADER (object_body);

	InterlockedIncrement (&object_header->ref_count);

	return object_body;
}

VOID _r_obj_dereferenceex (_In_ PVOID object_body, _In_ LONG ref_count)
{
	assert (!(ref_count < 0));

	PR_OBJECT_HEADER object_header = OBJECT_TO_OBJECT_HEADER (object_body);

	LONG old_count = InterlockedExchangeAdd (&object_header->ref_count, -ref_count);
	LONG new_count = old_count - ref_count;

	if (new_count == 0)
	{
		if (object_header->cleanup_callback)
			object_header->cleanup_callback (object_body);

		_r_mem_free (object_header);
	}
	else if (new_count < 0)
	{
		RtlRaiseStatus (STATUS_INVALID_PARAMETER);
	}
}

/*
	8-bit string object
*/

PR_BYTE _r_obj_createbyteex (_In_opt_ LPSTR buffer, _In_ SIZE_T length)
{
	if (!length)
		length = sizeof (ANSI_NULL);

	PR_BYTE byte = _r_obj_allocate (UFIELD_OFFSET (R_BYTE, data) + length + sizeof (ANSI_NULL));

	byte->length = length;
	byte->buffer = byte->data;

	if (buffer)
	{
		memcpy (byte->buffer, buffer, length);
		_r_obj_writebytenullterminator (byte);
	}
	else
	{
		byte->buffer[0] = ANSI_NULL;
	}

	return byte;
}

/*
	16-bit string object
*/

PR_STRING _r_obj_createstringex (_In_opt_ LPCWSTR buffer, _In_ SIZE_T length)
{
	assert (!(length & 0x01));

	if (!length)
		length = sizeof (UNICODE_NULL);

	PR_STRING string = _r_obj_allocate (UFIELD_OFFSET (R_STRING, data) + length + sizeof (UNICODE_NULL));

	string->length = length;
	string->buffer = string->data;

	if (buffer)
	{
		memcpy (string->buffer, buffer, length);
		_r_obj_writestringnullterminator (string);
	}
	else
	{
		string->buffer[0] = UNICODE_NULL;
	}

	return string;
}

VOID _r_obj_removestring (_In_ PR_STRING string, _In_ SIZE_T start_pos, _In_ SIZE_T length)
{
	memmove (&string->buffer[start_pos], &string->buffer[start_pos + length], string->length - (length + start_pos) * sizeof (WCHAR));
	string->length -= (length * sizeof (WCHAR));

	_r_obj_writestringnullterminator (string);
}

/*
	String builder
*/

VOID _r_obj_appendstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ LPCWSTR text, _In_ SIZE_T length)
{
	if (!text || !length)
		return;

	if (string->allocated_length < string->string->length + length)
		_r_obj_resizestringbuilder (string, string->string->length + length);

	if (!_r_str_isempty (text))
	{
		memcpy (PTR_ADD_OFFSET (string->string->buffer, string->string->length), text, length);
	}

	string->string->length += length;
	_r_obj_writestringnullterminator (string->string);
}

VOID _r_obj_appendstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr)
{
	SIZE_T length_in_bytes;
	INT length;

	if (!format)
		return;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (string->allocated_length < string->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (string, string->string->length + length_in_bytes);

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (PTR_ADD_OFFSET (string->string->buffer, string->string->length), length, format, arg_ptr);
#pragma warning(pop)

	string->string->length += length_in_bytes;
	_r_obj_writestringnullterminator (string->string);
}

VOID _r_obj_insertstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ LPCWSTR text, _In_ SIZE_T length)
{
	if (!text || !length)
		return;

	if (string->allocated_length < string->string->length + length)
		_r_obj_resizestringbuilder (string, string->string->length + length);

	if ((index * sizeof (WCHAR)) < string->string->length)
	{
		memmove (&string->string->buffer[index + (length / sizeof (WCHAR))], &string->string->buffer[index], string->string->length - (index * sizeof (WCHAR)));
	}

	if (!_r_str_isempty (text))
	{
		memcpy (&string->string->buffer[index], text, length);
	}

	string->string->length += length;
	_r_obj_writestringnullterminator (string->string);
}

VOID _r_obj_insertstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr)
{
	SIZE_T length_in_bytes;
	INT length;

	if (!format)
		return;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (string->allocated_length < string->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (string, string->string->length + length_in_bytes);

	if ((index * sizeof (WCHAR)) < string->string->length)
	{
		memmove (&string->string->buffer[index + length], &string->string->buffer[index], string->string->length - (index * sizeof (WCHAR)));
	}

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (&string->string->buffer[index], length, format, arg_ptr);
#pragma warning(pop)

	string->string->length += length_in_bytes;
	_r_obj_writestringnullterminator (string->string);
}

VOID _r_obj_resizestringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T new_capacity)
{
	PR_STRING new_string;
	SIZE_T new_size;

	new_size = string->allocated_length * 2;

	if (new_capacity & 0x01)
		new_capacity += 1;

	if (new_size < new_capacity)
		new_size = new_capacity;

	new_string = _r_obj_createstringex (NULL, new_size);

	memcpy (new_string->buffer, string->string->buffer, string->string->length + sizeof (UNICODE_NULL));

	new_string->length = string->string->length;

	_r_obj_movereference (&string->string, new_string);
}

/*
	Array object
*/

PR_ARRAY _r_obj_createarrayex (_In_ SIZE_T item_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	PR_ARRAY array_node;

	if (!initial_capacity)
		initial_capacity = 1;

	array_node = _r_obj_allocateex (sizeof (R_ARRAY), &_r_util_dereferencearrayprocedure);

	array_node->count = 0;
	array_node->allocated_count = initial_capacity;
	array_node->item_size = item_size;
	array_node->cleanup_callback = cleanup_callback;
	array_node->items = _r_mem_allocatezero (array_node->allocated_count * item_size);

	return array_node;
}

VOID _r_obj_cleararray (_Inout_ PR_ARRAY array_node)
{
	PVOID array_item;
	SIZE_T count;

	if (!array_node->count)
		return;

	count = array_node->count;
	array_node->count = 0;

	if (array_node->cleanup_callback)
	{
		for (SIZE_T i = 0; i < count; i++)
		{
			array_item = PTR_ADD_OFFSET (array_node->items, i * array_node->item_size);

			array_node->cleanup_callback (array_item);

			RtlSecureZeroMemory (array_item, array_node->item_size);
		}
	}
}

VOID _r_obj_resizearray (_Inout_ PR_ARRAY array_node, _In_ SIZE_T new_capacity)
{
	if (array_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	array_node->allocated_count = new_capacity;
	array_node->items = _r_mem_reallocatezero (array_node->items, array_node->allocated_count * array_node->item_size);
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_obj_addarrayitem (_Inout_ PR_ARRAY array_node, _In_ PVOID item)
{
	PVOID dst;
	SIZE_T index;

	if (array_node->count == array_node->allocated_count)
		_r_obj_resizearray (array_node, array_node->allocated_count * 2);

	dst = _r_obj_getarrayitem (array_node, array_node->count);

	if (dst)
	{
		memcpy (dst, item, array_node->item_size);

		index = array_node->count++;

		return index;
	}

	return SIZE_MAX;
}

VOID _r_obj_addarrayitems (_Inout_ PR_ARRAY array_node, _In_ PVOID items, _In_ SIZE_T count)
{
	PVOID dst;

	if (array_node->allocated_count < array_node->count + count)
	{
		array_node->allocated_count *= 2;

		if (array_node->allocated_count < array_node->count + count)
			array_node->allocated_count = array_node->count + count;

		_r_obj_resizearray (array_node, array_node->allocated_count);
	}

	dst = _r_obj_getarrayitem (array_node, array_node->count);

	if (dst)
	{
		memcpy (dst, items, count * array_node->item_size);

		array_node->count += count;
	}
}

VOID _r_obj_removearrayitems (_Inout_ PR_ARRAY array_node, _In_ SIZE_T start_pos, _In_ SIZE_T count)
{
	PVOID dst = _r_obj_getarrayitem (array_node, start_pos);
	PVOID src = _r_obj_getarrayitem (array_node, start_pos + count);

	if (dst && src)
		memmove (dst, src, (array_node->count - start_pos - count) * array_node->item_size);

	array_node->count -= count;
}

/*
	List object
*/

PR_LIST _r_obj_createlistex (_In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	PR_LIST list_node;

	if (!initial_capacity)
		initial_capacity = 1;

	list_node = _r_obj_allocateex (sizeof (R_LIST), &_r_util_dereferencelistprocedure);

	list_node->count = 0;
	list_node->allocated_count = initial_capacity;
	list_node->cleanup_callback = cleanup_callback;
	list_node->items = _r_mem_allocatezero (list_node->allocated_count * sizeof (PVOID));

	return list_node;
}

SIZE_T _r_obj_addlistitem (_Inout_ PR_LIST list_node, _In_ PVOID item)
{
	SIZE_T index;

	if (list_node->count == list_node->allocated_count)
		_r_obj_resizelist (list_node, list_node->allocated_count * 2);

	list_node->items[list_node->count] = item;

	index = list_node->count++;

	return index;
}

VOID _r_obj_clearlist (_Inout_ PR_LIST list_node)
{
	PVOID list_item;
	SIZE_T count = list_node->count;

	list_node->count = 0;

	if (list_node->cleanup_callback)
	{
		for (SIZE_T i = 0; i < count; i++)
		{
			if (list_node->items[i])
			{
				list_item = list_node->items[i];
				list_node->items[i] = NULL;

				list_node->cleanup_callback (list_item);
			}
		}
	}
}

SIZE_T _r_obj_findlistitem (_In_ PR_LIST list_node, _In_ PVOID list_item)
{
	for (SIZE_T i = 0; i < list_node->count; i++)
	{
		if (list_node->items[i] == list_item)
			return i;
	}

	return SIZE_MAX;
}

VOID _r_obj_insertlistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ PVOID * items, _In_ SIZE_T count)
{
	SIZE_T new_count;

	if (list_node->allocated_count < list_node->count + count)
	{
		new_count = list_node->allocated_count * 2;

		if (new_count < list_node->count + count)
			new_count = list_node->count + count;

		_r_obj_resizelist (list_node, new_count);

	}

	if (start_pos < list_node->count)
	{
		memmove (&list_node->items[start_pos + count], &list_node->items[start_pos], (list_node->count - start_pos) * sizeof (PVOID));
	}

	memcpy (&list_node->items[start_pos], items, count * sizeof (PVOID));

	list_node->count += count;
}

VOID _r_obj_removelistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ SIZE_T count)
{
	if (list_node->cleanup_callback)
	{
		for (SIZE_T i = start_pos; i < count; i++)
		{
			if (list_node->items[i])
				list_node->cleanup_callback (list_node->items[i]);
		}
	}

	memmove (&list_node->items[start_pos], &list_node->items[start_pos + count], (list_node->count - start_pos - count) * sizeof (PVOID));

	list_node->count -= count;
}

VOID _r_obj_resizelist (_Inout_ PR_LIST list_node, _In_ SIZE_T new_capacity)
{
	if (list_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	list_node->allocated_count = new_capacity;
	list_node->items = _r_mem_reallocatezero (list_node->items, list_node->allocated_count * sizeof (PVOID));
}

/*
	Hashtable object
*/

#define HASHTABLE_ENTRY_SIZE(inner_size) (UFIELD_OFFSET(R_HASHTABLE_ENTRY, body) + (inner_size))
#define HASHTABLE_GET_ENTRY(hashtable, index) ((PR_HASHTABLE_ENTRY)PTR_ADD_OFFSET((hashtable)->entries, HASHTABLE_ENTRY_SIZE((hashtable)->entry_size) * (index)))
#define HASHTABLE_GET_ENTRY_INDEX(hashtable, entry) ((SIZE_T)(PTR_ADD_OFFSET(entry, -(hashtable)->entries) / HASHTABLE_ENTRY_SIZE((hashtable)->entry_size)))
#define HASHTABLE_INIT_VALUE 0xFF

PR_HASHTABLE _r_obj_createhashtableex (_In_ SIZE_T entry_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	PR_HASHTABLE hashtable = _r_obj_allocateex (sizeof (R_HASHTABLE), &_r_util_dereferencehashtableprocedure);

	if (!initial_capacity)
		initial_capacity = 1;

	hashtable->entry_size = entry_size;
	hashtable->cleanup_callback = cleanup_callback;

	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (initial_capacity);
	hashtable->buckets = _r_mem_allocatezero (hashtable->allocated_buckets * sizeof (SIZE_T));

	memset (hashtable->buckets, HASHTABLE_INIT_VALUE, hashtable->allocated_buckets * sizeof (SIZE_T));

	hashtable->allocated_entries = hashtable->allocated_buckets;
	hashtable->entries = _r_mem_allocatezero (hashtable->allocated_entries * HASHTABLE_ENTRY_SIZE (entry_size));

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

	return hashtable;
}

FORCEINLINE SIZE_T _r_obj_indexfromhash (_In_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code)
{
	return hash_code & (hashtable->allocated_buckets - 1);
}

FORCEINLINE SIZE_T _r_obj_validatehash (_In_ SIZE_T hash_code)
{
	return hash_code & MAXLONG;
}

VOID _r_obj_resizehashtable (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T new_capacity)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;

	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (new_capacity);

	hashtable->buckets = _r_mem_reallocatezero (hashtable->buckets, hashtable->allocated_buckets * sizeof (SIZE_T));

	memset (hashtable->buckets, HASHTABLE_INIT_VALUE, hashtable->allocated_buckets * sizeof (SIZE_T));

	hashtable->allocated_entries = hashtable->allocated_buckets;
	hashtable->entries = _r_mem_reallocatezero (hashtable->entries, HASHTABLE_ENTRY_SIZE (hashtable->entry_size) * hashtable->allocated_entries);

	hashtable_entry = hashtable->entries;

	for (SIZE_T i = 0; i < hashtable->next_entry; i++)
	{
		if (hashtable_entry->hash_code != SIZE_MAX)
		{
			index = _r_obj_indexfromhash (hashtable, hashtable_entry->hash_code);

			hashtable_entry->next = hashtable->buckets[index];
			hashtable->buckets[index] = i;
		}

		hashtable_entry = PTR_ADD_OFFSET (hashtable_entry, HASHTABLE_ENTRY_SIZE (hashtable->entry_size));
	}
}

FORCEINLINE PVOID _r_obj_addhashtableitemex (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code, _In_opt_ PVOID entry, _In_ BOOLEAN is_checkforduplicate, _Out_opt_ PBOOLEAN is_added)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;
	SIZE_T free_entry;

	hash_code = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, hash_code);

	if (is_checkforduplicate)
	{
		for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
		{
			hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, i);

			if (hashtable_entry->hash_code == hash_code)
			{
				if (is_added)
					*is_added = FALSE;

				return &hashtable_entry->body;
			}
		}
	}

	if (hashtable->free_entry != SIZE_MAX)
	{
		free_entry = hashtable->free_entry;
		hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, free_entry);
		hashtable->free_entry = hashtable_entry->next;

		if (hashtable->cleanup_callback)
			hashtable->cleanup_callback (&hashtable_entry->body);
	}
	else
	{
		if (hashtable->next_entry == hashtable->allocated_entries)
		{
			_r_obj_resizehashtable (hashtable, hashtable->allocated_buckets * 2);

			index = _r_obj_indexfromhash (hashtable, hash_code);
		}

		free_entry = hashtable->next_entry++;
		hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, free_entry);
	}

	hashtable_entry->hash_code = hash_code;
	hashtable_entry->next = hashtable->buckets[index];
	hashtable->buckets[index] = free_entry;

	if (entry)
	{
		memcpy (&hashtable_entry->body, entry, hashtable->entry_size);
	}
	else
	{
		memset (&hashtable_entry->body, 0, hashtable->entry_size);
	}

	hashtable->count += 1;

	if (is_added)
		*is_added = TRUE;

	return &hashtable_entry->body;
}

_Ret_maybenull_
PVOID _r_obj_addhashtableitem (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code, _In_opt_ PVOID entry)
{
	PVOID hashtable_entry;
	BOOLEAN is_added;

	hashtable_entry = _r_obj_addhashtableitemex (hashtable, hash_code, entry, TRUE, &is_added);

	if (is_added)
		return hashtable_entry;

	return NULL;
}

VOID _r_obj_clearhashtable (_Inout_ PR_HASHTABLE hashtable)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T next_entry;
	SIZE_T index;

	if (!hashtable->count)
		return;

	next_entry = hashtable->next_entry;

	memset (hashtable->buckets, HASHTABLE_INIT_VALUE, hashtable->allocated_buckets * sizeof (SIZE_T));

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

	if (hashtable->cleanup_callback)
	{
		index = 0;

		while (index < next_entry)
		{
			hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, index);

			index += 1;

			if (hashtable_entry->hash_code != SIZE_MAX)
			{
				hashtable->cleanup_callback (&hashtable_entry->body);

				RtlSecureZeroMemory (&hashtable_entry->body, hashtable->entry_size);
			}
		}
	}
}

_Success_ (return)
BOOLEAN _r_obj_enumhashtable (_In_ PR_HASHTABLE hashtable, _Outptr_ PVOID * entry, _Out_opt_ PSIZE_T hash_code, _Inout_ PSIZE_T enum_key)
{
	PR_HASHTABLE_ENTRY hashtable_entry;

	while (*enum_key < hashtable->next_entry)
	{
		hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, *enum_key);

		(*enum_key) += 1;

		if (hashtable_entry->hash_code != SIZE_MAX)
		{
			if (hash_code)
				*hash_code = hashtable_entry->hash_code;

			*entry = &hashtable_entry->body;

			return TRUE;
		}
	}

	return FALSE;
}

_Ret_maybenull_
PVOID _r_obj_findhashtable (_In_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;

	hash_code = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, hash_code);

	for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, i);

		if (hashtable_entry->hash_code == hash_code)
			return &hashtable_entry->body;
	}

	return NULL;
}

BOOLEAN _r_obj_removehashtableentry (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;
	SIZE_T previous_index;

	hash_code = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, hash_code);
	previous_index = SIZE_MAX;

	for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = HASHTABLE_GET_ENTRY (hashtable, i);

		if (hashtable_entry->hash_code == hash_code)
		{
			if (previous_index == SIZE_MAX)
			{
				hashtable->buckets[index] = hashtable_entry->next;
			}
			else
			{
				HASHTABLE_GET_ENTRY (hashtable, previous_index)->next = hashtable_entry->next;
			}

			hashtable_entry->hash_code = SIZE_MAX;
			hashtable_entry->next = hashtable->free_entry;
			hashtable->free_entry = i;

			hashtable->count -= 1;

			if (hashtable->cleanup_callback)
			{
				hashtable->cleanup_callback (&hashtable_entry->body);

				RtlSecureZeroMemory (&hashtable_entry->body, hashtable->entry_size);
			}

			return TRUE;
		}

		previous_index = i;
	}

	return FALSE;
}

/*
	System messages
*/

_Success_ (return)
BOOLEAN _r_msg_taskdialog (_In_ const TASKDIALOGCONFIG * ptd, _Out_opt_ PINT pbutton, _Out_opt_ PINT pradiobutton, _Out_opt_ LPBOOL pis_flagchecked)
{
#if defined(APP_NO_DEPRECATIONS)
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
			{
				return SUCCEEDED (_TaskDialogIndirect (ptd, pbutton, pradiobutton, pis_flagchecked));
			}
		}
	}

	return FALSE;
#endif // APP_NO_DEPRECATIONS
}

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			BOOL is_topmost = HIWORD (lpdata);

			if (is_topmost)
			{
				_r_wnd_top (hwnd, TRUE);
			}

			_r_wnd_center (hwnd, GetParent (hwnd));

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			// remove window icon
			_r_wnd_seticon (hwnd, NULL, NULL);

			break;
		}

		case TDN_HYPERLINK_CLICKED:
		{
			LPCWSTR link = (LPCWSTR)lparam;

			if (!_r_str_isempty (link))
			{
				_r_shell_opendefault (link);
			}

			break;
		}
	}

	return S_OK;
}

/*
	Clipboard operations
*/

_Ret_maybenull_
PR_STRING _r_clipboard_get (_In_opt_ HWND hwnd)
{
	if (OpenClipboard (hwnd))
	{
		PR_STRING string = NULL;
		HGLOBAL hmemory = GetClipboardData (CF_UNICODETEXT);

		if (hmemory)
		{
			LPCWSTR clipboard_text = GlobalLock (hmemory);

			if (clipboard_text)
			{
				string = _r_obj_createstring (clipboard_text);
			}

			GlobalUnlock (hmemory);
		}

		CloseClipboard ();

		return string;
	}

	return NULL;
}

VOID _r_clipboard_set (_In_opt_ HWND hwnd, _In_ LPCWSTR string, _In_ SIZE_T length)
{
	if (!string || !length)
		return;

	if (OpenClipboard (hwnd))
	{
		SIZE_T byte_size = length * sizeof (WCHAR);
		HGLOBAL hmemory = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, byte_size + sizeof (UNICODE_NULL));

		if (hmemory)
		{
			PVOID clipboard_text = GlobalLock (hmemory);

			if (clipboard_text)
			{
				memcpy (clipboard_text, string, byte_size);
				*(LPWSTR)PTR_ADD_OFFSET (clipboard_text, byte_size) = UNICODE_NULL; // terminate

				GlobalUnlock (clipboard_text);

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

BOOLEAN _r_fs_deletefile (_In_ LPCWSTR path, _In_ BOOLEAN is_force)
{
	if (is_force)
		SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL);

	return !!DeleteFile (path);
}

BOOLEAN _r_fs_deletedirectory (_In_ LPCWSTR path, _In_ BOOLEAN is_recurse)
{
	SHFILEOPSTRUCT shfop;
	PR_STRING string;
	SIZE_T length;
	BOOLEAN result;

	if ((GetFileAttributes (path) & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return FALSE;

	length = _r_str_length (path) + 1;
	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR)); // required to set 2 nulls at end

	_r_str_copy (string->buffer, length, path);

	memset (&shfop, 0, sizeof (shfop));

	shfop.wFunc = FO_DELETE;
	shfop.fFlags = FOF_NO_UI;
	shfop.pFrom = string->buffer;

	if (!is_recurse)
		shfop.fFlags |= FOF_NORECURSION;

	result = (SHFileOperation (&shfop) == ERROR_SUCCESS);

	_r_obj_dereference (string);

	return result;
}

LONG64 _r_fs_getfilesize (_In_ LPCWSTR path)
{
	HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		LONG64 result = _r_fs_getsize (hfile);
		CloseHandle (hfile);

		return result;
	}

	return 0;
}

BOOLEAN _r_fs_makebackup (_In_ LPCWSTR path, _In_opt_ LONG64 timestamp, _In_ BOOLEAN is_removesourcefile)
{
	if (!path || !_r_fs_exists (path))
		return FALSE;

	PR_STRING new_path;
	BOOLEAN is_success;

	if (timestamp)
	{
		SYSTEMTIME system_time;
		WCHAR date_format[128];
		PR_STRING directory_path;

		_r_unixtime_to_systemtime (timestamp, &system_time);
		SystemTimeToTzSpecificLocalTime (NULL, &system_time, &system_time);

		GetDateFormat (LOCALE_USER_DEFAULT, 0, &system_time, L"yyyy-MM-dd", date_format, RTL_NUMBER_OF (date_format));

		directory_path = _r_path_getbasedirectory (path);

		new_path = _r_format_string (L"%s\\%s-%s.bak", _r_obj_getstringorempty (directory_path), date_format, _r_path_getbasename (path));

		if (directory_path)
			_r_obj_dereference (directory_path);
	}
	else
	{
		new_path = _r_format_string (L"%s.bak", path);
	}

	if (!new_path)
		return FALSE;

	if (_r_fs_exists (new_path->buffer))
		_r_fs_deletefile (new_path->buffer, TRUE);

	if (is_removesourcefile)
	{
		is_success = _r_fs_movefile (path, new_path->buffer, MOVEFILE_COPY_ALLOWED);

		if (is_success)
			_r_fs_deletefile (path, TRUE);
	}
	else
	{
		is_success = _r_fs_copyfile (path, new_path->buffer, 0);
	}

	_r_obj_dereference (new_path);

	return is_success;
}

BOOLEAN _r_fs_mkdir (_In_ LPCWSTR path)
{
	if (SHCreateDirectoryEx (NULL, path, NULL) == ERROR_SUCCESS)
		return TRUE;

	return !!CreateDirectory (path, NULL); // fallback
}

PR_BYTE _r_fs_readfile (_In_ HANDLE hfile, _In_ ULONG file_size)
{
	HANDLE hmap;
	PR_BYTE buffer;
	PVOID file_map;

	hmap = CreateFileMapping (hfile, NULL, PAGE_READONLY, 0, file_size, NULL);

	if (hmap)
	{
		file_map = MapViewOfFile (hmap, FILE_MAP_READ, 0, 0, 0);

		if (file_map)
		{
			buffer = _r_obj_createbyteex (file_map, file_size);

			UnmapViewOfFile (file_map);
			CloseHandle (hmap);

			return buffer;
		}

		CloseHandle (hmap);
	}

	return NULL;
}

/*
	Paths
*/

LPCWSTR _r_path_getbasename (_In_ LPCWSTR path)
{
	LPCWSTR last_slash = path;

	while (!_r_str_isempty (path))
	{
		if ((*path == L'\\' || *path == L'/' || *path == L':') && path[1] && path[1] != OBJ_NAME_PATH_SEPARATOR && path[1] != L'/')
		{
			last_slash = path + 1;
		}

		path += 1;
	}

	return last_slash;
}

PR_STRING _r_path_getbasedirectory (_In_ LPCWSTR path)
{
	R_STRINGREF fullpath_part;
	R_STRINGREF path_part;
	PR_STRING path_string;
	PR_STRING directory_string;

	path_string = _r_obj_createstring (path);

	_r_obj_initializestringref2 (&fullpath_part, path_string);

	directory_string = _r_str_splitatlastchar (&fullpath_part, &path_part, OBJ_NAME_PATH_SEPARATOR);

	if (directory_string)
	{
		_r_obj_trimstring (directory_string, L"\\/");

		_r_obj_movereference (&path_string, directory_string);
	}

	return path_string;
}

_Ret_maybenull_
LPCWSTR _r_path_getbaseextension (_In_ LPCWSTR path)
{
	LPCWSTR last_point = NULL;

	while (!_r_str_isempty (path))
	{
		if (*path == OBJ_NAME_PATH_SEPARATOR || *path == L' ')
		{
			last_point = NULL;
		}
		else if (*path == L'.')
		{
			last_point = path;
		}

		path += 1;
	}

	return last_point;
}

PR_STRING _r_path_getfullpath (_In_ LPCWSTR path)
{
	PR_STRING full_path;
	ULONG length;
	ULONG return_length;

	length = 256;
	full_path = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	return_length = RtlGetFullPathName_U (path, length, full_path->buffer, NULL);

	if (return_length > length)
	{
		length = return_length;

		_r_obj_movereference (&full_path, _r_obj_createstringex (NULL, length * sizeof (WCHAR)));

		return_length = RtlGetFullPathName_U (path, length, full_path->buffer, NULL);
	}

	if (return_length)
	{
		_r_obj_trimstringtonullterminator (full_path);

		return full_path;
	}

	_r_obj_dereference (full_path);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_getknownfolder (_In_ ULONG folder, _In_opt_ LPCWSTR append)
{
	PR_STRING string;
	SIZE_T append_length;

	append_length = append ? _r_str_length (append) * sizeof (WCHAR) : 0;
	string = _r_obj_createstringex (NULL, (256 * sizeof (WCHAR)) + append_length);

	if (SUCCEEDED (SHGetFolderPath (NULL, folder, NULL, SHGFP_TYPE_CURRENT, string->buffer)))
	{
		_r_obj_trimstringtonullterminator (string);

		if (append)
		{
			memcpy (&string->buffer[string->length / sizeof (WCHAR)], append, append_length + sizeof (UNICODE_NULL));
			string->length += append_length;
		}

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (_In_opt_ HMODULE hmodule)
{
	ULONG length;
	PR_STRING string;

	length = 256;
	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if (GetModuleFileName (hmodule, string->buffer, length))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_compact (_In_ LPCWSTR path, _In_ UINT length)
{
	if (!path || !length)
		return NULL;

	PR_STRING string;

	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if (PathCompactPathEx (string->buffer, path, length, 0))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_makeunique (_In_ LPCWSTR path)
{
	if (!path || !_r_fs_exists (path))
		return NULL;

	if (_r_str_findchar (path, OBJ_NAME_PATH_SEPARATOR) == SIZE_MAX)
		return NULL;

	PR_STRING result = NULL;

	PR_STRING path_directory;
	PR_STRING path_filename;
	PR_STRING path_extension;

	path_directory = _r_path_getbasedirectory (path);
	path_filename = _r_obj_createstring (_r_path_getbasename (path));
	path_extension = _r_obj_createstring (_r_path_getbaseextension (path));

	if (!_r_obj_isstringempty (path_filename) && !_r_obj_isstringempty (path_extension))
		_r_obj_setstringsize (path_filename, (path_filename->length - path_extension->length));

	for (USHORT i = 1; i < (USHRT_MAX - 1); i++)
	{
		_r_obj_movereference (&result, _r_format_string (L"%s%c%s-%" PRIu16 L"%s", _r_obj_getstringorempty (path_directory), OBJ_NAME_PATH_SEPARATOR, _r_obj_getstringorempty (path_filename), i, _r_obj_getstringorempty (path_extension)));

		if (!_r_fs_exists (result->buffer))
			break;
	}

	SAFE_DELETE_REFERENCE (path_directory);
	SAFE_DELETE_REFERENCE (path_filename);
	SAFE_DELETE_REFERENCE (path_extension);

	return result;
}

_Ret_maybenull_
PR_STRING _r_path_search (_In_ LPCWSTR path)
{
	if (!path)
		return NULL;

	PR_STRING string;
	SIZE_T length;

	length = 1024;
	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if (PathSearchAndQualify (path, string->buffer, (UINT)length))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

PR_STRING _r_path_dospathfromnt (_In_ LPCWSTR path)
{
	SIZE_T path_length = _r_str_length (path);

	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_r_str_compare_length (path, L"\\??\\", 4) == 0)
	{
		return _r_obj_createstring (path + 4);
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_r_str_compare_length (path, L"\\systemroot", 11) == 0)
	{
		if (path_length != 11 && path[11] == OBJ_NAME_PATH_SEPARATOR)
		{
			WCHAR system_root[256];
			GetSystemDirectory (system_root, RTL_NUMBER_OF (system_root));

			return _r_format_string (L"%s%c%s", system_root, OBJ_NAME_PATH_SEPARATOR, path + 11 + 9);
		}
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_r_str_compare_length (path, L"system32\\", 9) == 0)
	{
		if (path_length != 9 && path[9] == OBJ_NAME_PATH_SEPARATOR)
		{
			WCHAR system_root[256];
			GetSystemDirectory (system_root, RTL_NUMBER_OF (system_root));

			return _r_format_string (L"%s%c%s", system_root, OBJ_NAME_PATH_SEPARATOR, path + 8);
		}
	}
	else if (_r_str_compare_length (path, L"\\device\\", 8) == 0)
	{
		if (_r_str_compare_length (path, L"\\device\\mup", 11) == 0) // network share (win7+)
		{
			if (path_length != 11 && path[11] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				return _r_format_string (L"%c%s", OBJ_NAME_PATH_SEPARATOR, path + 11);
			}
		}
		else if (_r_str_compare_length (path, L"\\device\\lanmanredirector", 24) == 0) // network share (winxp+)
		{
			if (path_length != 24 && path[24] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				return _r_format_string (L"%c%s", OBJ_NAME_PATH_SEPARATOR, path + 24);
			}
		}

		// device name prefixes
		OBJECT_ATTRIBUTES oa;
		UNICODE_STRING device_name;
		UNICODE_STRING device_prefix;
		WCHAR device_name_buffer[7] = L"\\??\\ :";
		WCHAR device_prefix_buffer[PR_DEVICE_PREFIX_LENGTH] = {0};
		HANDLE link_handle;
		HANDLE directory_handle;
		SIZE_T prefix_length;
		ULONG query_context = 0;

#ifndef _WIN64
		PROCESS_DEVICEMAP_INFORMATION device_map;
#else
		PROCESS_DEVICEMAP_INFORMATION_EX device_map;
#endif

		memset (&device_map, 0, sizeof (device_map));

		if (NT_SUCCESS (NtQueryInformationProcess (NtCurrentProcess (), ProcessDeviceMap, &device_map, sizeof (device_map), NULL)))
		{
			for (ULONG i = 0; i < PR_DEVICE_COUNT; i++)
			{
				if (device_map.Query.DriveMap)
				{
					if (!(device_map.Query.DriveMap & (0x1 << i)))
						continue;
				}

				device_name.Length = (RTL_NUMBER_OF (device_name_buffer) - 1) * sizeof (WCHAR);
				device_name.Buffer = device_name_buffer;

				device_name_buffer[4] = L'A' + (WCHAR)i;

				InitializeObjectAttributes (&oa, &device_name, OBJ_CASE_INSENSITIVE, NULL, NULL);

				if (NT_SUCCESS (NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &oa)))
				{
					device_prefix.Length = 0;
					device_prefix.Buffer = device_prefix_buffer;
					device_prefix.MaximumLength = RTL_NUMBER_OF (device_prefix_buffer);

					if (NT_SUCCESS (NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL)))
					{
						prefix_length = device_prefix.Length / sizeof (WCHAR);

						if (prefix_length)
						{
							if (_r_str_compare_length (device_prefix.Buffer, path, prefix_length) == 0)
							{
								// To ensure we match the longest prefix, make sure the next character is a
								// backslash or the path is equal to the prefix.
								if (path_length == prefix_length || path[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
								{
									NtClose (link_handle);

									// <letter>:path
									return _r_format_string (L"%c:%c%s", device_name.Buffer[4], OBJ_NAME_PATH_SEPARATOR, path + prefix_length + 1);
								}
							}
						}
					}

					NtClose (link_handle);
				}
			}
		}

		// Device map link resolution does not resolve custom FS.
		// Workaround this thing using \\GLOBAL?? objects enumeration. (HACK!!!)
		//
		// https://github.com/henrypp/simplewall/issues/817
		// https://github.com/maharmstone/btrfs/issues/324

		RtlInitUnicodeString (&device_prefix, L"\\GLOBAL??");
		InitializeObjectAttributes (&oa, &device_prefix, OBJ_CASE_INSENSITIVE, NULL, NULL);

		if (NT_SUCCESS (NtOpenDirectoryObject (&directory_handle, DIRECTORY_QUERY, &oa)))
		{
			NTSTATUS status;
			POBJECT_DIRECTORY_INFORMATION directory_entry = NULL;
			POBJECT_DIRECTORY_INFORMATION directory_info;
			SIZE_T i = 0;
			ULONG buffer_size;
			BOOLEAN is_firsttime = TRUE;

			buffer_size = 512;
			directory_entry = _r_mem_allocatezero (buffer_size);

			while (TRUE)
			{
				while ((status = NtQueryDirectoryObject (directory_handle, directory_entry, buffer_size, FALSE, is_firsttime, &query_context, NULL)) == STATUS_MORE_ENTRIES)
				{
					if (directory_entry[0].Name.Buffer)
						break;

					if (buffer_size > PR_SIZE_BUFFER_OVERFLOW)
						break;

					buffer_size *= 2;
					directory_entry = _r_mem_reallocatezero (directory_entry, buffer_size);
				}

				if (!NT_SUCCESS (status))
				{
					_r_mem_free (directory_entry);
					directory_entry = NULL;

					break;
				}

				while (TRUE)
				{
					directory_info = &directory_entry[i];

					if (!directory_info->Name.Buffer)
						break;

					if (directory_info->Name.Length == (2 * sizeof (WCHAR)) && directory_info->Name.Buffer[1] == L':')
					{
						InitializeObjectAttributes (&oa, &directory_info->Name, OBJ_CASE_INSENSITIVE, directory_handle, NULL);

						if (NT_SUCCESS (NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &oa)))
						{
							device_prefix.Length = 0;
							device_prefix.Buffer = device_prefix_buffer;
							device_prefix.MaximumLength = (RTL_NUMBER_OF (device_prefix_buffer) - 1) * sizeof (WCHAR);

							if (NT_SUCCESS (NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL)))
							{
								prefix_length = device_prefix.Length / sizeof (WCHAR);

								if (_r_str_compare_length (path, device_prefix.Buffer, prefix_length) == 0)
								{
									if (path_length == prefix_length || path[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
									{
										WCHAR drive_letter = directory_info->Name.Buffer[0];

										NtClose (link_handle);
										NtClose (directory_handle);

										_r_mem_free (directory_entry);

										// <letter>:path
										return _r_format_string (L"%c:%c%s", drive_letter, OBJ_NAME_PATH_SEPARATOR, path + prefix_length + 1);
									}
								}
							}

							NtClose (link_handle);
						}
					}

					i += 1;
				}

				if (status != STATUS_MORE_ENTRIES)
					break;

				is_firsttime = FALSE;
			}

			if (directory_entry)
				_r_mem_free (directory_entry);

			NtClose (directory_handle);
		}

		// network share prefixes
		// https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/support-for-unc-naming-and-mup

		HKEY hkey;
		PR_STRING provider_order = NULL;

		if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			provider_order = _r_reg_querystring (hkey, NULL, L"ProviderOrder");

			if (provider_order)
			{
				WCHAR provider_key[256];
				R_STRINGREF remaining_part;
				PR_STRING provider_part;

				_r_obj_initializestringref2 (&remaining_part, provider_order);

				while (remaining_part.length != 0)
				{
					provider_part = _r_str_splitatchar (&remaining_part, &remaining_part, L',');

					if (provider_part)
					{
						_r_str_printf (provider_key, RTL_NUMBER_OF (provider_key), L"System\\CurrentControlSet\\Services\\%s\\NetworkProvider", provider_part->buffer);

						if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, provider_key, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
						{
							PR_STRING device_name = _r_reg_querystring (hkey, NULL, L"DeviceName");

							if (device_name)
							{
								SIZE_T prefix_length = _r_obj_getstringlength (device_name);

								if (prefix_length)
								{
									if (_r_str_compare_length (device_name->buffer, path, prefix_length) == 0)
									{
										// To ensure we match the longest prefix, make sure the next character is a
										// backslash. Don't resolve if the name *is* the prefix. Otherwise, we will end
										// up with a useless string like "\".
										if (path_length != prefix_length && path[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
										{
											_r_obj_dereference (provider_order);
											_r_obj_dereference (provider_part);
											_r_obj_dereference (device_name);

											RegCloseKey (hkey);

											// \path
											return _r_obj_createstring (path + prefix_length);
										}
									}
								}

								_r_obj_dereference (device_name);
							}

							RegCloseKey (hkey);
						}

						_r_obj_dereference (provider_part);
					}
				}

				_r_obj_dereference (provider_order);
			}

			RegCloseKey (hkey);
		}
	}

	return _r_obj_createstringex (path, path_length * sizeof (WCHAR));
}

_Ret_maybenull_
PR_STRING _r_path_ntpathfromdos (_In_ LPCWSTR path, _Out_opt_ PULONG error_code)
{
	NTSTATUS status;
	POBJECT_NAME_INFORMATION obj_name_info;
	HANDLE hfile;

	hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (!_r_fs_isvalidhandle (hfile))
	{
		if (error_code)
			*error_code = GetLastError ();

		return NULL;
	}

	PR_STRING string = NULL;
	ULONG buffer_size = 512;
	ULONG attempts = 6;

	obj_name_info = _r_mem_allocatezero (buffer_size);

	do
	{
		status = NtQueryObject (hfile, ObjectNameInformation, obj_name_info, buffer_size, &buffer_size);

		if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL)
		{
			if (buffer_size > PR_SIZE_BUFFER_OVERFLOW)
				break;

			obj_name_info = _r_mem_reallocatezero (obj_name_info, buffer_size);
		}
		else
		{
			break;
		}
	}
	while (--attempts);

	if (NT_SUCCESS (status))
	{
		string = _r_obj_createstringfromunicodestring (&obj_name_info->Name);

		if (string)
			_r_str_tolower (string->buffer); // lower is important!

		if (error_code)
			*error_code = ERROR_SUCCESS;
	}
	else
	{
		if (error_code)
			*error_code = status;
	}

	_r_mem_free (obj_name_info);

	CloseHandle (hfile);

	return string;
}

/*
	Shell
*/

VOID _r_shell_openfile (_In_ LPCWSTR path)
{
	LPITEMIDLIST item = NULL;
	PR_STRING string = NULL;
	SFGAOF attributes = 0;
	HRESULT hr;

	hr = SHParseDisplayName (path, NULL, &item, 0, &attributes);

	if (SUCCEEDED (hr))
	{
		hr = SHOpenFolderAndSelectItems (item, 0, NULL, 0);

		CoTaskMemFree (item);

		if (SUCCEEDED (hr))
			return;
	}

	// try using windows explorer arguments
	if (_r_fs_exists (path))
	{
		string = _r_format_string (L"\"explorer.exe\" /select,\"%s\"", path);

		if (string)
		{
			if (_r_sys_createprocess (NULL, string->buffer, NULL))
			{
				_r_obj_dereference (string);

				return;
			}
		}
	}

	// try open file directory
	_r_obj_movereference (&string, _r_path_getbasedirectory (path));

	if (string)
	{
		if (_r_fs_exists (string->buffer))
			_r_shell_opendefault (string->buffer);

		_r_obj_dereference (string);
	}
}

/*
	Strings
*/

_Check_return_
BOOLEAN _r_str_isnumeric (_In_ LPCWSTR string)
{
	if (!string)
		return FALSE;

	while (!_r_str_isempty (string))
	{
		if (iswdigit (*string) == 0)
			return FALSE;

		string += 1;
	}

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_str_append (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (buffer_size <= PR_STR_MAX_LENGTH)
	{
		SIZE_T dest_length = _r_str_length (buffer);

		_r_str_copy (buffer + dest_length, buffer_size - dest_length, string);

		return TRUE;
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_str_appendformat (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	if (!buffer || !buffer_size || _r_str_isempty (format) || (buffer_size > PR_STR_MAX_LENGTH))
		return FALSE;

	va_list arg_ptr;
	SIZE_T dest_length;

	dest_length = _r_str_length (buffer);

	va_start (arg_ptr, format);
	_r_str_printf_v (buffer + dest_length, buffer_size - dest_length, format, arg_ptr);
	va_end (arg_ptr);

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_str_copy (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (buffer_size <= PR_STR_MAX_LENGTH)
	{
		if (!_r_str_isempty (string))
		{
			while (buffer_size && (*string != UNICODE_NULL))
			{
				*buffer++ = *string++;
				--buffer_size;
			}

			if (!buffer_size)
				--buffer; // truncate buffer
		}
	}

	*buffer = UNICODE_NULL;

	return TRUE;
}

_Check_return_
SIZE_T _r_str_length (_In_ LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0;

	if (USER_SHARED_DATA->ProcessorFeatures[PF_XMMI64_INSTRUCTIONS_AVAILABLE]) // check sse2 feature
	{
		LPWSTR p = (LPWSTR)((ULONG_PTR)string & ~0xE); // string should be 2 byte aligned
		ULONG unaligned = PtrToUlong (string) & 0xF;

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
				return (SIZE_T)(index / sizeof (WCHAR));

			p += 16 / sizeof (WCHAR);
		}

		while (TRUE)
		{
			b = _mm_load_si128 ((__m128i*)p);
			b = _mm_cmpeq_epi16 (b, z);
			mask = _mm_movemask_epi8 (b);

			if (_BitScanForward (&index, mask))
				return (SIZE_T)(p - string) + index / sizeof (WCHAR);

			p += 0x10 / sizeof (WCHAR);
		}
	}
	else
	{
		return wcsnlen_s (string, PR_STR_MAX_LENGTH);
	}
}

_Success_ (return)
BOOLEAN _r_str_printf (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (_r_str_isempty (format) || (buffer_size > PR_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return FALSE;
	}

	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_str_printf_v (buffer, buffer_size, format, arg_ptr);
	va_end (arg_ptr);

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_str_printf_v (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr)
{
	if (!buffer || !buffer_size)
		return FALSE;

	if (_r_str_isempty (format) || (buffer_size > PR_STR_MAX_LENGTH))
	{
		*buffer = UNICODE_NULL;
		return FALSE;
	}

	SIZE_T max_length = buffer_size - 1; // leave the last space for the null terminator

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	INT format_size = _vsnwprintf (buffer, max_length, format, arg_ptr);
#pragma warning(pop)

	if (format_size == -1 || (SIZE_T)format_size >= max_length)
		buffer[max_length] = UNICODE_NULL; // need to null terminate the string

	return TRUE;
}

_Check_return_
SIZE_T _r_str_hash (_In_ LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0;

	// http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
#ifdef _WIN64
#define FNV_prime 1099511628211ULL
#define FNV_offset_basis 14695981039346656037ULL
#else
#define FNV_prime 16777619UL
#define FNV_offset_basis 2166136261UL
#endif

	SIZE_T hash = FNV_offset_basis; // FNV_offset_basis

	// FNV-1a hash
	while (*string != UNICODE_NULL)
	{
		hash ^= (SIZE_T)_r_str_upper (*string);
		hash *= FNV_prime; // FNV_prime

		string += 1;
	}

	return hash;
}

_Check_return_
INT _r_str_compare (_In_ LPCWSTR string1, _In_ LPCWSTR string2)
{
	if (!string1 && !string2)
		return 0;

	if (!string1)
		return -1;

	if (!string2)
		return 1;

	return _wcsicmp (string1, string2);
}

_Check_return_
INT _r_str_compare_length (_In_ LPCWSTR string1, _In_ LPCWSTR string2, _In_ SIZE_T length)
{
	if (!string1 && !string2)
		return 0;

	if (!string1)
		return -1;

	if (!string2)
		return 1;

	return _wcsnicmp (string1, string2, length);
}

_Ret_maybenull_
PR_STRING _r_str_expandenvironmentstring (_In_ LPCWSTR string)
{
	NTSTATUS status;
	UNICODE_STRING input_string;
	UNICODE_STRING output_string;
	PR_STRING buffer_string;
	ULONG buffer_size;

	if (!string)
		return NULL;

	input_string.Length = (USHORT)_r_str_length (string) * sizeof (WCHAR);
	input_string.MaximumLength = (USHORT)input_string.Length + sizeof (UNICODE_NULL);
	input_string.Buffer = (LPWSTR)string;

	buffer_size = 512 * sizeof (WCHAR);
	buffer_string = _r_obj_createstringex (NULL, buffer_size);

	output_string.Length = 0;
	output_string.MaximumLength = (USHORT)buffer_size;
	output_string.Buffer = buffer_string->buffer;

	status = RtlExpandEnvironmentStrings_U (NULL, &input_string, &output_string, &buffer_size);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		_r_obj_movereference (&buffer_string, _r_obj_createstringex (NULL, buffer_size));

		output_string.Length = 0;
		output_string.MaximumLength = (USHORT)buffer_size;
		output_string.Buffer = buffer_string->buffer;

		status = RtlExpandEnvironmentStrings_U (NULL, &input_string, &output_string, &buffer_size);
	}

	if (NT_SUCCESS (status))
	{
		buffer_string->length = output_string.Length;
		_r_obj_writestringnullterminator (buffer_string); // terminate

		return buffer_string;
	}

	_r_obj_dereference (buffer_string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_str_unexpandenvironmentstring (_In_ LPCWSTR string)
{
	PR_STRING buffer;
	SIZE_T buffer_length;

	if (!string)
		return NULL;

	buffer_length = 512;
	buffer = _r_obj_createstringex (NULL, buffer_length * sizeof (WCHAR));

	if (PathUnExpandEnvStrings (string, buffer->buffer, (UINT)buffer_length))
	{
		_r_obj_trimstringtonullterminator (buffer);

		return buffer;
	}

	_r_obj_movereference (&buffer, _r_obj_createstring (string));

	return buffer;
}

_Ret_maybenull_
PR_STRING _r_str_fromguid (_In_ LPCGUID lpguid)
{
	PR_STRING string;
	UNICODE_STRING us;

	if (NT_SUCCESS (RtlStringFromGUID ((LPGUID)lpguid, &us)))
	{
		string = _r_obj_createstringfromunicodestring (&us);

		RtlFreeUnicodeString (&us);

		return string;
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_str_fromsecuritydescriptor (_In_ PSECURITY_DESCRIPTOR lpsd, _In_ SECURITY_INFORMATION information)
{
	PR_STRING security_descriptor_string;
	LPWSTR security_string;
	ULONG security_string_length;

	if (ConvertSecurityDescriptorToStringSecurityDescriptor (lpsd, SDDL_REVISION, information, &security_string, &security_string_length))
	{
		security_descriptor_string = _r_obj_createstringex (security_string, security_string_length * sizeof (WCHAR));

		LocalFree (security_string);

		return security_descriptor_string;
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_str_fromsid (_In_ PSID lpsid)
{
	UNICODE_STRING us;
	PR_STRING string;

	string = _r_obj_createstringex (NULL, SECURITY_MAX_SID_STRING_CHARACTERS * sizeof (WCHAR));

	_r_obj_createunicodestringfromstring (&us, string);

	if (NT_SUCCESS (RtlConvertSidToUnicodeString (&us, lpsid, FALSE)))
	{
		string->length = us.Length;
		_r_obj_writestringnullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

BOOLEAN _r_str_toboolean (_In_ LPCWSTR string)
{
	if (_r_str_isempty (string))
		return FALSE;

	if (_r_str_tointeger (string) > 0 || _r_str_compare (string, L"true") == 0)
		return TRUE;

	return FALSE;
}

LONG _r_str_tolongex (_In_ LPCWSTR string, _In_ INT radix)
{
	if (_r_str_isempty (string))
		return 0;

	return wcstol (string, NULL, radix);
}

LONG64 _r_str_tolong64 (_In_ LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0;

	return wcstoll (string, NULL, 10);
}

ULONG _r_str_toulongex (_In_ LPCWSTR string, _In_ INT radix)
{
	if (_r_str_isempty (string))
		return 0;

	return wcstoul (string, NULL, radix);
}

ULONG64 _r_str_toulong64 (_In_ LPCWSTR string)
{
	if (_r_str_isempty (string))
		return 0;

	return wcstoull (string, NULL, 10);
}

BOOLEAN _r_str_toboolean_a (_In_ LPCSTR string)
{
	if (_r_str_isempty_a (string))
		return FALSE;

	if (_r_str_tointeger_a (string) > 0 || _stricmp (string, "true") == 0)
		return TRUE;

	return FALSE;
}

LONG _r_str_tolong_a (_In_ LPCSTR string)
{
	if (_r_str_isempty_a (string))
		return 0;

	return strtol (string, NULL, 10);
}

LONG64 _r_str_tolong64_a (_In_ LPCSTR string)
{
	if (_r_str_isempty_a (string))
		return 0;

	return strtoll (string, NULL, 10);
}

ULONG _r_str_toulong_a (_In_ LPCSTR string)
{
	if (_r_str_isempty_a (string))
		return 0;

	return strtoul (string, NULL, 10);
}

ULONG64 _r_str_toulong64_a (_In_ LPCSTR string)
{
	if (_r_str_isempty_a (string))
		return 0;

	return strtoull (string, NULL, 10);
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findchar (_In_ LPCWSTR string, _In_ WCHAR character)
{
	SIZE_T index = 0;

	character = _r_str_upper (character);

	while (!_r_str_isempty (string))
	{
		if (_r_str_upper (*string) == character)
			return index;

		string += 1;
		index += 1;
	}

	return SIZE_MAX;
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findlastchar (_In_ LPCWSTR string, _In_ SIZE_T length, _In_ WCHAR character)
{
	if (_r_str_isempty (string) || !length)
		return SIZE_MAX;

	character = _r_str_upper (character);

	for (SIZE_T i = length - 1; i != SIZE_MAX; i--)
	{
		if (_r_str_upper (string[i]) == character)
			return i;
	}

	return SIZE_MAX;
}

VOID _r_str_replacechar (_Inout_ LPWSTR string, _In_ WCHAR char_from, _In_ WCHAR char_to)
{
	while (!_r_str_isempty (string))
	{
		if (*string == char_from)
			*string = char_to;

		string += 1;
	}
}

BOOLEAN _r_str_match (_In_ LPCWSTR string, _In_ LPCWSTR pattern, _In_ BOOLEAN is_ignorecase)
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

VOID _r_str_tolower (_Inout_ LPWSTR string)
{
	while (!_r_str_isempty (string))
	{
		*string = _r_str_lower (*string);

		string += 1;
	}
}

VOID _r_str_toupper (_Inout_ LPWSTR string)
{
	while (!_r_str_isempty (string))
	{
		*string = _r_str_upper (*string);

		string += 1;
	}
}

_Ret_maybenull_
PR_STRING _r_str_extractex (_In_ LPCWSTR string, _In_ SIZE_T length, _In_ SIZE_T start_pos, _In_ SIZE_T extract_length)
{
	if (start_pos == 0 && extract_length >= length)
		return _r_obj_createstringex (string, length * sizeof (WCHAR));

	if (start_pos >= length)
		return NULL;

	return _r_obj_createstringex (&string[start_pos], extract_length * sizeof (WCHAR));
}

_Ret_maybenull_
PR_STRING _r_str_multibyte2unicodeex (_In_ LPCSTR string, _In_ SIZE_T string_size)
{
	if (!string || !string_size)
		return NULL;

	NTSTATUS status;
	PR_STRING output_string;
	ULONG unicode_size;
	ULONG current_size;

	current_size = (ULONG)min (string_size, ULONG_MAX - 1);

	status = RtlMultiByteToUnicodeSize (&unicode_size, string, current_size);

	if (!NT_SUCCESS (status))
		return NULL;

	output_string = _r_obj_createstringex (NULL, unicode_size);
	status = RtlMultiByteToUnicodeN (output_string->buffer, (ULONG)output_string->length, NULL, string, current_size);

	if (!NT_SUCCESS (status))
	{
		_r_obj_dereference (output_string);

		return NULL;
	}

	return output_string;
}

_Ret_maybenull_
PR_BYTE _r_str_unicode2multibyteex (_In_ LPCWSTR string, _In_ SIZE_T string_size)
{
	if (!string || !string_size)
		return NULL;

	NTSTATUS status;
	PR_BYTE output_string;
	ULONG multibyte_size;
	ULONG current_size;

	current_size = (ULONG)min (string_size, ULONG_MAX - 1);

	status = RtlUnicodeToMultiByteSize (&multibyte_size, string, current_size);

	if (!NT_SUCCESS (status))
		return NULL;

	output_string = _r_obj_createbyteex (NULL, multibyte_size);
	status = RtlUnicodeToMultiByteN (output_string->buffer, (ULONG)output_string->length, NULL, string, current_size);

	if (!NT_SUCCESS (status))
	{
		_r_obj_dereference (output_string);

		return NULL;
	}

	return output_string;
}

PR_STRING _r_str_splitatchar (_In_ PR_STRINGREF string, _Out_ PR_STRINGREF token, _In_ WCHAR separator)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findchar (input.buffer, separator);

	if (index == SIZE_MAX)
	{
		token->buffer = NULL;
		token->length = 0;

		return _r_obj_createstringex (input.buffer, input.length);
	}

	PR_STRING output = _r_obj_createstringex (input.buffer, index * sizeof (WCHAR));

	token->buffer = PTR_ADD_OFFSET (input.buffer, output->length + sizeof (UNICODE_NULL));
	token->length = input.length - (index * sizeof (WCHAR)) - sizeof (UNICODE_NULL);

	return output;
}

PR_STRING _r_str_splitatlastchar (_In_ PR_STRINGREF string, _Out_ PR_STRINGREF token, _In_ WCHAR separator)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findlastchar (input.buffer, input.length / sizeof (WCHAR), separator);

	if (index == SIZE_MAX)
	{
		token->buffer = NULL;
		token->length = 0;

		return _r_obj_createstringex (input.buffer, input.length);
	}

	PR_STRING output = _r_obj_createstringex (input.buffer, index * sizeof (WCHAR));

	token->buffer = PTR_ADD_OFFSET (input.buffer, output->length + sizeof (UNICODE_NULL));
	token->length = input.length - (index * sizeof (WCHAR)) - sizeof (UNICODE_NULL);

	return output;
}

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (_In_ PR_STRING string, _In_ WCHAR key_delimeter, _In_ WCHAR value_delimeter)
{
	R_STRINGREF remaining_part;
	R_HASHSTORE hashstore;
	PR_HASHTABLE hashtable;
	PR_STRING values_part;
	PR_STRING key_string;
	SIZE_T delimeter_pos;
	SIZE_T hash_code;

	_r_obj_initializestringref2 (&remaining_part, string);

	hashtable = _r_obj_createhashtable (sizeof (hashstore), &_r_util_dereferencehashstoreprocedure);

	while (remaining_part.length != 0)
	{
		values_part = _r_str_splitatchar (&remaining_part, &remaining_part, key_delimeter);

		if (values_part)
		{
			delimeter_pos = _r_str_findchar (values_part->buffer, value_delimeter);

			if (delimeter_pos != SIZE_MAX)
			{
				key_string = _r_str_extract (values_part, 0, delimeter_pos);

				if (key_string)
				{
					hash_code = _r_obj_getstringhash (key_string);

					if (hash_code)
					{
						_r_obj_initializehashstore (&hashstore, _r_str_extract (values_part, delimeter_pos + 1, _r_obj_getstringlength (values_part) - delimeter_pos - 1), 0);

						_r_obj_addhashtableitem (hashtable, hash_code, &hashstore);
					}

					_r_obj_dereference (key_string);
				}
			}

			_r_obj_dereference (values_part);
		}
	}

	if (!_r_obj_gethashtablesize (hashtable))
	{
		_r_obj_dereference (hashtable);

		return NULL;
	}

	return hashtable;
}

/*
	return 1 if v1 > v2
	return 0 if v1 = v2
	return -1 if v1 < v2
*/

#define MAKE_VERSION_ULONG64(major, minor, build, revision) \
	(((ULONG64)(major) << 48) | \
	((ULONG64)(minor) << 32) | \
	((ULONG64)(build) << 16) | \
	((ULONG64)(revision) << 0))

ULONG64 _r_str_versiontoulong64 (_In_ LPCWSTR version)
{
	R_STRINGREF remaining;
	ULONG major_number;
	ULONG minor_number;
	ULONG build_number;
	ULONG revision_number;

	_r_obj_initializestringref (&remaining, (LPWSTR)version);

	PR_STRING major_string = _r_str_splitatchar (&remaining, &remaining, L'.');
	PR_STRING minor_string = _r_str_splitatchar (&remaining, &remaining, L'.');
	PR_STRING build_string = _r_str_splitatchar (&remaining, &remaining, L'.');
	PR_STRING revision_string = _r_str_splitatchar (&remaining, &remaining, L'.');

	major_number = _r_str_toulong (major_string->buffer);
	minor_number = _r_str_toulong (minor_string->buffer);
	build_number = _r_str_toulong (build_string->buffer);
	revision_number = _r_str_toulong (revision_string->buffer);

	_r_obj_dereference (major_string);
	_r_obj_dereference (minor_string);
	_r_obj_dereference (build_string);
	_r_obj_dereference (revision_string);

	return MAKE_VERSION_ULONG64 (major_number, minor_number, build_number, revision_number);
}

INT _r_str_versioncompare (_In_ LPCWSTR v1, _In_ LPCWSTR v2)
{
	ULONG64 value_v1;
	ULONG64 value_v2;

	if (!_r_str_isnumeric (v1))
	{
		value_v1 = _r_str_versiontoulong64 (v1);
		value_v2 = _r_str_versiontoulong64 (v2);
	}
	else
	{
		value_v1 = _r_str_toulong64 (v1);
		value_v2 = _r_str_toulong64 (v2);
	}

	if (value_v1 > value_v2)
	{
		return 1;
	}
	else if (value_v1 < value_v2)
	{
		return -1;
	}

	return 0;
}

/*
	System information
*/

_Check_return_
BOOLEAN _r_sys_iselevated ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_elevated = FALSE;

	if (_r_initonce_begin (&init_once))
	{
#if !defined(APP_NO_DEPRECATIONS)
		// winxp compatibility
		if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		{
			is_elevated = !!IsUserAnAdmin ();

			_r_initonce_end (&init_once);

			return is_elevated;
		}
#endif // !APP_NO_DEPRECATIONS

		HANDLE htoken;

		if (OpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &htoken))
		{
			TOKEN_ELEVATION elevation;
			ULONG return_length;

			if (NT_SUCCESS (NtQueryInformationToken (htoken, TokenElevation, &elevation, sizeof (elevation), &return_length)))
			{
				is_elevated = !!elevation.TokenIsElevated;
			}

			CloseHandle (htoken);
		}

		_r_initonce_end (&init_once);
	}

	return is_elevated;
}

_Check_return_
ULONG _r_sys_getwindowsversion ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ULONG windows_version = 0;

	if (_r_initonce_begin (&init_once))
	{
		RTL_OSVERSIONINFOEXW version_info = {0};

		version_info.dwOSVersionInfoSize = sizeof (version_info);

		if (NT_SUCCESS (RtlGetVersion (&version_info)))
		{
			if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 1)
			{
				windows_version = WINDOWS_XP;
			}
			else if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 2)
			{
				windows_version = WINDOWS_XP_64;
			}
			else if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 0)
			{
				windows_version = WINDOWS_VISTA;
			}
			else if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 1)
			{
				windows_version = WINDOWS_7;
			}
			else if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 2)
			{
				windows_version = WINDOWS_8;
			}
			else if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 3)
			{
				windows_version = WINDOWS_8_1;
			}
			else if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 4)
			{
				windows_version = WINDOWS_10; // earlier versions of windows 10 have 6.4 version number
			}
			else if (version_info.dwMajorVersion == 10 && version_info.dwMinorVersion == 0)
			{
				if (version_info.dwBuildNumber >= 19043)
				{
					windows_version = WINDOWS_10_21H1;
				}
				else if (version_info.dwBuildNumber >= 19042)
				{
					windows_version = WINDOWS_10_20H2;
				}
				else if (version_info.dwBuildNumber >= 19041)
				{
					windows_version = WINDOWS_10_2004;
				}
				else if (version_info.dwBuildNumber >= 18363)
				{
					windows_version = WINDOWS_10_1909;
				}
				else if (version_info.dwBuildNumber >= 18362)
				{
					windows_version = WINDOWS_10_1903;
				}
				else if (version_info.dwBuildNumber >= 17763)
				{
					windows_version = WINDOWS_10_1809;
				}
				else if (version_info.dwBuildNumber >= 17134)
				{
					windows_version = WINDOWS_10_1803;
				}
				else if (version_info.dwBuildNumber >= 16299)
				{
					windows_version = WINDOWS_10_1709;
				}
				else if (version_info.dwBuildNumber >= 15063)
				{
					windows_version = WINDOWS_10_1703;
				}
				else if (version_info.dwBuildNumber >= 14393)
				{
					windows_version = WINDOWS_10_1607;
				}
				else if (version_info.dwBuildNumber >= 10586)
				{
					windows_version = WINDOWS_10_1511;
				}
				else if (version_info.dwBuildNumber >= 10240)
				{
					windows_version = WINDOWS_10;
				}
				else
				{
					windows_version = WINDOWS_10;
				}
			}
		}

		_r_initonce_end (&init_once);
	}

	return windows_version;
}

BOOLEAN _r_sys_createprocessex (_In_opt_ LPCWSTR file_name, _In_opt_ LPCWSTR command_line, _In_opt_ LPCWSTR current_directory, _In_ WORD show_state, _In_ ULONG flags)
{
	BOOLEAN is_success;

	STARTUPINFO startup_info = {0};
	PROCESS_INFORMATION process_info = {0};

	PR_STRING file_name_string = NULL;
	PR_STRING command_line_string = NULL;
	PR_STRING current_directory_string = NULL;

	startup_info.cb = sizeof (startup_info);

	if (show_state != SW_SHOWDEFAULT)
	{
		startup_info.dwFlags |= STARTF_USESHOWWINDOW;
		startup_info.wShowWindow = show_state;
	}

	if (command_line)
	{
		command_line_string = _r_obj_createstring (command_line);
	}

	if (file_name)
	{
		file_name_string = _r_obj_createstring (file_name);
	}
	else
	{
		if (command_line_string)
		{
			INT numargs;
			LPWSTR* arga;

			arga = CommandLineToArgvW (command_line_string->buffer, &numargs);

			if (arga)
			{
				_r_obj_movereference (&file_name_string, _r_obj_createstring (arga[0]));

				LocalFree (arga);
			}
		}
	}

	if (!_r_obj_isstringempty (file_name_string) && !_r_fs_exists (file_name_string->buffer))
	{
		PR_STRING new_path = _r_path_search (file_name_string->buffer);

		if (new_path)
		{
			_r_obj_movereference (&file_name_string, new_path);
		}
		else
		{
			_r_obj_clearreference (&file_name_string);
		}
	}

	if (current_directory)
	{
		current_directory_string = _r_obj_createstring (current_directory);
	}
	else
	{
		if (!_r_obj_isstringempty (file_name_string))
			current_directory_string = _r_path_getbasedirectory (file_name_string->buffer);
	}

	is_success = !!CreateProcess (_r_obj_getstring (file_name_string), command_line_string ? command_line_string->buffer : NULL, NULL, NULL, FALSE, flags, NULL, _r_obj_getstring (current_directory_string), &startup_info, &process_info);

	SAFE_DELETE_HANDLE (process_info.hProcess);
	SAFE_DELETE_HANDLE (process_info.hThread);

	SAFE_DELETE_REFERENCE (file_name_string);
	SAFE_DELETE_REFERENCE (command_line_string);
	SAFE_DELETE_REFERENCE (current_directory_string);

	return is_success;
}

ULONG _r_sys_getthreadindex ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ULONG tls_index = 0;

	if (_r_initonce_begin (&init_once))
	{
		tls_index = TlsAlloc ();

		_r_initonce_end (&init_once);
	}

	return tls_index;
}

PR_THREAD_DATA _r_sys_getthreaddata ()
{
	PR_THREAD_DATA tls;
	ULONG err_code;
	ULONG tls_index;

	err_code = GetLastError ();
	tls_index = _r_sys_getthreadindex ();

	tls = TlsGetValue (tls_index);

	if (!tls)
	{
		tls = _r_mem_allocatezero (sizeof (R_THREAD_DATA));

		if (!TlsSetValue (tls_index, tls))
			return NULL;

		tls->tid = GetCurrentThreadId ();
		tls->handle = NULL;
	}

	SetLastError (err_code);

	return tls;
}

THREAD_API _r_sys_basethreadstart (_In_ PVOID arglist)
{
	R_THREAD_CONTEXT context;
	PR_THREAD_DATA tls;
	NTSTATUS status;
	HRESULT hr;

	memcpy (&context, arglist, sizeof (context));

	_r_mem_free (arglist);

	tls = _r_sys_getthreaddata ();

	if (tls)
	{
		tls->handle = context.thread;
		tls->is_handleused = context.is_handleused;
	}

	hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	status = context.start_address (context.arglist);

	if (hr == S_OK || hr == S_FALSE)
		CoUninitialize ();

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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_createthreadex (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist, _Out_opt_ PHANDLE hthread, _In_ INT priority)
{
	NTSTATUS status;
	HANDLE thread_current = NULL;
	PR_THREAD_CONTEXT context;

	context = _r_mem_allocatezero (sizeof (R_THREAD_CONTEXT));

	status = RtlCreateUserThread (NtCurrentProcess (), NULL, TRUE, 0, 0, 0, &_r_sys_basethreadstart, context, &thread_current, NULL);

	if (NT_SUCCESS (status))
	{
		context->start_address = start_address;
		context->arglist = arglist;
		context->thread = thread_current;
		context->is_handleused = (hthread != NULL); // user need to be destroy thread handle by himself

		SetThreadPriority (thread_current, priority);

		if (!hthread)
			status = _r_sys_resumethread (thread_current);
	}

	if (NT_SUCCESS (status))
	{
		if (hthread)
			*hthread = thread_current;
	}
	else
	{
		if (thread_current)
			NtClose (thread_current);

		_r_mem_free (context);
	}

	return status;
}

PR_STRING _r_sys_getsessioninfo (_In_ WTS_INFO_CLASS info)
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

PR_STRING _r_sys_getusernamefromsid (_In_ PSID sid)
{
	LSA_OBJECT_ATTRIBUTES oa;
	LSA_HANDLE policy_handle;
	PLSA_REFERENCED_DOMAIN_LIST referenced_domains;
	PLSA_TRANSLATED_NAME names;
	R_STRINGBUILDER string = {0};

	InitializeObjectAttributes (&oa, NULL, 0, NULL, NULL);

	if (NT_SUCCESS (LsaOpenPolicy (NULL, &oa, POLICY_LOOKUP_NAMES, &policy_handle)))
	{
		if (NT_SUCCESS (LsaLookupSids (policy_handle, 1, &sid, &referenced_domains, &names)))
		{
			if (names && names[0].Use != SidTypeInvalid && names[0].Use != SidTypeUnknown)
			{
				BOOLEAN is_hasdomain = referenced_domains && names[0].DomainIndex >= 0;
				BOOLEAN is_hasname = names[0].Name.Buffer != NULL;

				if (is_hasdomain || is_hasname)
					_r_obj_initializestringbuilder (&string);

				if (is_hasdomain)
				{
					PLSA_TRUST_INFORMATION trust_info = &referenced_domains->Domains[names[0].DomainIndex];

					if (trust_info->Name.Buffer)
					{
						_r_obj_appendstringbuilderex (&string, trust_info->Name.Buffer, trust_info->Name.Length);

						if (is_hasname)
						{
							_r_obj_appendstringbuilder (&string, L"\\");
						}
					}
				}

				if (is_hasname)
				{
					_r_obj_appendstringbuilderex (&string, names[0].Name.Buffer, names[0].Name.Length);
				}
			}

			if (referenced_domains)
				LsaFreeMemory (referenced_domains);

			if (names)
				LsaFreeMemory (names);
		}

		LsaClose (policy_handle);
	}

	if (!_r_obj_isstringempty (string.string))
		return _r_obj_finalstringbuilder (&string);

	_r_obj_deletestringbuilder (&string);

	return NULL;
}

_Success_ (return)
BOOLEAN _r_sys_getopt (_In_ LPCWSTR args, _In_ LPCWSTR option_key, _Outptr_opt_ PR_STRING * option_value)
{
	LPWSTR key;
	LPWSTR value;
	SIZE_T key_length;
	INT numargs;
	BOOLEAN result;
	LPWSTR *arga;

	arga = CommandLineToArgvW (args, &numargs);

	if (!arga)
		return FALSE;

	result = FALSE;
	key_length = _r_str_length (option_key);

	for (INT i = 0; i < numargs; i++)
	{
		key = arga[i];

		if (_r_str_isempty (key) || (*key != L'/' && *key != L'-'))
			continue;

		key += 1;

		if (_r_str_compare_length (key, option_key, key_length) != 0)
			continue;

		if (key[key_length] != UNICODE_NULL)
		{
			if (key[key_length] == L':' || key[key_length] == L'=')
			{
				value = key + key_length + 1;
			}
			else
			{
				continue;
			}
		}
		else
		{
			value = (numargs >= (i + 1)) ? arga[i + 1] : NULL;
		}

		if (option_value)
		{
			if (_r_str_isempty (value) || *value == L'/' || *value == L'-')
				continue;

			*option_value = _r_obj_createstring (value);

			result = TRUE;
			break;
		}
		else
		{
			result = TRUE;
			break;
		}
	}

	LocalFree (arga);

	return result;
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

VOID _r_sys_setprivilege (_In_ PULONG privileges, _In_ ULONG count, _In_ BOOLEAN is_enable)
{
	HANDLE htoken;
	PVOID privileges_buffer;
	PTOKEN_PRIVILEGES token_privileges;

	if (!OpenProcessToken (NtCurrentProcess (), TOKEN_ADJUST_PRIVILEGES, &htoken))
		return;

	privileges_buffer = _r_mem_allocatezero (FIELD_OFFSET (TOKEN_PRIVILEGES, Privileges) + (sizeof (LUID_AND_ATTRIBUTES) * count));

	token_privileges = privileges_buffer;
	token_privileges->PrivilegeCount = count;

	for (ULONG i = 0; i < count; i++)
	{
		//token_privileges->Privileges[i].Luid.HighPart = 0;
		token_privileges->Privileges[i].Luid.LowPart = privileges[i];
		token_privileges->Privileges[i].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;
	}

	NtAdjustPrivilegesToken (htoken, FALSE, token_privileges, 0, NULL, NULL);

	_r_mem_free (privileges_buffer);

	CloseHandle (htoken);
}

/*
	Unixtime
*/

LONG64 _r_unixtime_now ()
{
	SYSTEMTIME system_time;

	GetSystemTime (&system_time);

	return _r_unixtime_from_systemtime (&system_time);
}

VOID _r_unixtime_to_filetime (_In_ LONG64 unixtime, _Out_ PFILETIME file_time)
{
	const LONG64 UNIX_TIME_START = 116444736000000000; // January 1, 1970 (start of Unix epoch) in "ticks"
	const LONG64 TICKS_PER_SECOND = 10000000; // 100ns tick

	LONG64 ll = (unixtime * TICKS_PER_SECOND) + UNIX_TIME_START;

	file_time->dwLowDateTime = (ULONG)ll;
	file_time->dwHighDateTime = ll >> 32;
}

_Success_ (return)
BOOLEAN _r_unixtime_to_systemtime (_In_ LONG64 unixtime, _Out_ PSYSTEMTIME system_time)
{
	FILETIME file_time;

	_r_unixtime_to_filetime (unixtime, &file_time);

	return !!FileTimeToSystemTime (&file_time, system_time);
}

LONG64 _r_unixtime_from_filetime (_In_ const FILETIME * file_time)
{
	const LONG64 UNIX_TIME_START = 116444736000000000; // January 1, 1970 (start of Unix epoch) in "ticks"
	const LONG64 TICKS_PER_SECOND = 10000000; // 100ns tick

	LARGE_INTEGER li = {0};
	li.LowPart = file_time->dwLowDateTime;
	li.HighPart = file_time->dwHighDateTime;

	return (li.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

LONG64 _r_unixtime_from_systemtime (_In_ const SYSTEMTIME * system_time)
{
	FILETIME file_time;

	if (SystemTimeToFileTime (system_time, &file_time))
		return _r_unixtime_from_filetime (&file_time);

	return 0;
}

/*
	Device context (Draw/Calculation etc...)
*/

_Success_ (return)
BOOLEAN _r_dc_adjustwindowrect (_In_opt_ HWND hwnd, _Inout_ LPRECT rect, _In_ ULONG style, _In_ ULONG style_ex, _In_ BOOL is_menu)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static AWRFD _AdjustWindowRectExForDpi = NULL;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			if (huser32)
			{
				_AdjustWindowRectExForDpi = (AWRFD)GetProcAddress (huser32, "AdjustWindowRectExForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_AdjustWindowRectExForDpi)
		return !!_AdjustWindowRectExForDpi (rect, style, is_menu, style_ex, _r_dc_getdpivalue (hwnd, NULL));

	return !!AdjustWindowRectEx (rect, style, is_menu, style_ex);
}

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (_In_opt_ HWND hwnd, _In_ UINT action, _In_ UINT param1, _In_ PVOID param2)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SPIFP _SystemParametersInfoForDpi = NULL;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			if (huser32)
			{
				_SystemParametersInfoForDpi = (SPIFP)GetProcAddress (huser32, "SystemParametersInfoForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_SystemParametersInfoForDpi)
		return !!_SystemParametersInfoForDpi (action, param1, param2, 0, _r_dc_getdpivalue (hwnd, NULL));

	return !!SystemParametersInfo (action, param1, param2, 0);
}

// Optimized version of WinAPI function "FillRect"
VOID _r_dc_fillrect (_In_ HDC hdc, _In_ PRECT lprc, _In_ COLORREF clr)
{
	COLORREF clr_prev = SetBkColor (hdc, clr);
	ExtTextOut (hdc, 0, 0, ETO_OPAQUE, lprc, NULL, 0, NULL);
	SetBkColor (hdc, clr_prev);
}

COLORREF _r_dc_getcolorbrightness (_In_ COLORREF clr)
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

COLORREF _r_dc_getcolorshade (_In_ COLORREF clr, _In_ INT percent)
{
	COLORREF r = ((GetRValue (clr) * percent) / 100);
	COLORREF g = ((GetGValue (clr) * percent) / 100);
	COLORREF b = ((GetBValue (clr) * percent) / 100);

	return RGB (r, g, b);
}

LONG _r_dc_getdpivalue (_In_opt_ HWND hwnd, _In_opt_ PRECT rect)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GDFM _GetDpiForMonitor = NULL;
	static GDFW _GetDpiForWindow = NULL;
	static GDFS _GetDpiForSystem = NULL;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
		{
			HMODULE hshcore = LoadLibraryEx (L"shcore.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
			HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

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

		_r_initonce_end (&init_once);
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		HMONITOR hmonitor = NULL;

		if (rect)
		{
			hmonitor = MonitorFromRect (rect, MONITOR_DEFAULTTONEAREST);
		}
		else if (hwnd)
		{
			hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
		}

		if (hmonitor)
		{
			UINT dpi_x;
			UINT dpi_y;

			if (SUCCEEDED (_GetDpiForMonitor (hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
			{
				return dpi_x;
			}
		}

		if (hwnd)
		{
			if (_GetDpiForWindow)
			{
				return _GetDpiForWindow (hwnd);
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
		INT dpi_value = GetDeviceCaps (hdc, LOGPIXELSX);

		ReleaseDC (NULL, hdc);

		return dpi_value;
	}

	return USER_DEFAULT_SCREEN_DPI;
}

INT _r_dc_getsystemmetrics (_In_opt_ HWND hwnd, _In_ INT index)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GSMFD _GetSystemMetricsForDpi = NULL;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			if (huser32)
			{
				_GetSystemMetricsForDpi = (GSMFD)GetProcAddress (huser32, "GetSystemMetricsForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_GetSystemMetricsForDpi)
		return _GetSystemMetricsForDpi (index, _r_dc_getdpivalue (hwnd, NULL));

	return GetSystemMetrics (index);
}

LONG _r_dc_getfontwidth (_In_ HDC hdc, _In_ LPCWSTR string, _In_ SIZE_T length)
{
	SIZE size;

	if (!GetTextExtentPoint32 (hdc, string, (INT)length, &size))
		return 200; // fallback

	return size.cx;
}

/*
	File dialog
*/

_Success_ (return)
BOOLEAN _r_filedialog_initialize (_Out_ PR_FILE_DIALOG file_dialog, _In_ ULONG flags)
{
	IFileDialog *ifd;

	if (SUCCEEDED (CoCreateInstance ((flags & (PR_FILEDIALOG_OPENFILE | PR_FILEDIALOG_OPENDIR)) ? &CLSID_FileOpenDialog : &CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, &ifd)))
	{
		file_dialog->flags = flags | PR_FILEDIALOG_ISIFILEDIALOG;
		file_dialog->u.ifd = ifd;

		return TRUE;
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		LPOPENFILENAME ofn;
		SIZE_T struct_size;
		SIZE_T file_length;

		struct_size = sizeof (OPENFILENAME);
		file_length = 1024;

		file_dialog->flags = flags;

		ofn = _r_mem_allocatezero (struct_size);

		ofn->lStructSize = (ULONG)struct_size;
		ofn->nMaxFile = (ULONG)file_length + 1;
		ofn->lpstrFile = _r_mem_allocatezero (file_length * sizeof (WCHAR));

		if ((flags & PR_FILEDIALOG_OPENFILE))
		{
			ofn->Flags = OFN_DONTADDTORECENT | OFN_ENABLESIZING | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		}
		else
		{
			ofn->Flags = OFN_DONTADDTORECENT | OFN_ENABLESIZING | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_OVERWRITEPROMPT;
		}

		file_dialog->u.ofn = ofn;

		return TRUE;
	}
#endif // !APP_NO_DEPRECATIONS

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_filedialog_show (_In_opt_ HWND hwnd, _In_ PR_FILE_DIALOG file_dialog)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // !APP_NO_DEPRECATIONS
	{
		FILEOPENDIALOGOPTIONS options = 0;

		IFileDialog_SetDefaultExtension (file_dialog->u.ifd, L"");

		IFileDialog_GetOptions (file_dialog->u.ifd, &options);

		if ((file_dialog->flags & PR_FILEDIALOG_OPENDIR))
			options |= FOS_PICKFOLDERS;

		IFileDialog_SetOptions (file_dialog->u.ifd, options | FOS_DONTADDTORECENT);

		return SUCCEEDED (IFileDialog_Show (file_dialog->u.ifd, hwnd));
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		LPOPENFILENAME ofn = file_dialog->u.ofn;

		ofn->hwndOwner = hwnd;

		if ((file_dialog->flags & PR_FILEDIALOG_OPENFILE))
		{
			return !!GetOpenFileName (ofn);
		}
		else
		{
			return !!GetSaveFileName (ofn);
		}
	}
#endif // APP_NO_DEPRECATIONS
}

PR_STRING _r_filedialog_getpath (_In_ PR_FILE_DIALOG file_dialog)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // APP_NO_DEPRECATIONS
	{
		IShellItem *result;
		PR_STRING file_name = NULL;
		LPWSTR name;

		if (SUCCEEDED (IFileDialog_GetResult (file_dialog->u.ifd, &result)))
		{
			if (SUCCEEDED (IShellItem_GetDisplayName (result, SIGDN_FILESYSPATH, &name)))
			{
				file_name = _r_obj_createstring (name);

				CoTaskMemFree (name);
			}

			IShellItem_Release (result);
		}

		if (!file_name)
		{
			if (SUCCEEDED (IFileDialog_GetFileName (file_dialog->u.ifd, &name)))
			{
				file_name = _r_obj_createstring (name);

				CoTaskMemFree (name);
			}
		}

		return file_name;
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		return _r_obj_createstring (file_dialog->u.ofn->lpstrFile);
	}
#endif // !APP_NO_DEPRECATIONS
}

VOID _r_filedialog_setpath (_Inout_ PR_FILE_DIALOG file_dialog, _In_ LPCWSTR path)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // !APP_NO_DEPRECATIONS
	{
		IShellItem *shell_item = NULL;
		PR_STRING directory = _r_path_getbasedirectory (path);
		LPCWSTR file_name = _r_path_getbasename (path);

		if (directory)
		{
			LPITEMIDLIST item;
			SFGAOF attributes;

			if (SUCCEEDED (SHParseDisplayName (directory->buffer, NULL, &item, 0, &attributes)))
			{
				SHCreateShellItem (NULL, NULL, item, &shell_item);

				CoTaskMemFree (item);
			}

			_r_obj_dereference (directory);
		}

		if (shell_item)
		{
			IFileDialog_SetFolder (file_dialog->u.ifd, shell_item);
			IFileDialog_SetFileName (file_dialog->u.ifd, file_name);

			IShellItem_Release (shell_item);
		}
		else
		{
			IFileDialog_SetFileName (file_dialog->u.ifd, path);
		}
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		LPOPENFILENAME ofn = file_dialog->u.ofn;
		SIZE_T path_length;

		if (_r_str_findchar (path, L'/') != SIZE_MAX || _r_str_findchar (path, L'\"') != SIZE_MAX)
			return;

		path_length = _r_str_length (path);

		ofn->nMaxFile = (ULONG)max (path_length + 1, 1024);
		ofn->lpstrFile = _r_mem_reallocatezero (ofn->lpstrFile, ofn->nMaxFile * sizeof (WCHAR));

		memcpy (ofn->lpstrFile, path, (path_length + 1) * sizeof (WCHAR));
	}
#endif // !APP_NO_DEPRECATIONS
}

VOID _r_filedialog_setfilter (_Inout_ PR_FILE_DIALOG file_dialog, _In_ COMDLG_FILTERSPEC * filters, _In_ ULONG count)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // !APP_NO_DEPRECATIONS
	{
		IFileDialog_SetFileTypes (file_dialog->u.ifd, count, filters);
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		LPOPENFILENAME ofn = file_dialog->u.ofn;
		PR_STRING filter_string;
		R_STRINGBUILDER filter_builder;

		_r_obj_initializestringbuilder (&filter_builder);

		for (ULONG i = 0; i < count; i++)
		{
			_r_obj_appendstringbuilderformat (&filter_builder, L"%s%c%s%c", filters[i].pszName, UNICODE_NULL, filters[i].pszSpec, UNICODE_NULL);
		}

		filter_string = _r_obj_finalstringbuilder (&filter_builder);

		if (ofn->lpstrFilter)
			_r_mem_free ((PVOID)ofn->lpstrFilter);

		ofn->lpstrFilter = _r_mem_allocateandcopy (filter_string->buffer, filter_string->length + sizeof (UNICODE_NULL));

		_r_obj_dereference (filter_string);
	}
#endif // !APP_NO_DEPRECATIONS
}

VOID _r_filedialog_destroy (_In_ PR_FILE_DIALOG file_dialog)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // !APP_NO_DEPRECATIONS
	{
		IFileDialog_Release (file_dialog->u.ifd);
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		LPOPENFILENAME ofn = file_dialog->u.ofn;

		if (ofn->lpstrFilter)
			_r_mem_free ((PVOID)ofn->lpstrFilter);

		if (ofn->lpstrFile)
			_r_mem_free (ofn->lpstrFile);

		_r_mem_free (ofn);
	}
#endif // !APP_NO_DEPRECATIONS
}

/*
	Window layout
*/

static BOOL CALLBACK _r_layout_enumcontrolscallback (_In_ HWND hwnd, _In_ LPARAM lparam)
{
	PR_LAYOUT_ENUM layout_enum = (PR_LAYOUT_ENUM)lparam;
	PR_LAYOUT_ITEM layout_item;

	if (!IsWindow (hwnd))
		return TRUE;

	// Ensure child is a direct descendent of parent (mandatory for DeferWindowPos)
	if (GetParent (hwnd) != layout_enum->root_hwnd)
		return TRUE;

	if (_r_wnd_isdialog (hwnd))
	{
		layout_item = _r_layout_additem (layout_enum->layout_manager, layout_enum->layout_item, hwnd, 0);

		if (layout_item)
		{
			R_LAYOUT_ENUM layout_child_data;

			layout_child_data.root_hwnd = hwnd;
			layout_child_data.layout_manager = layout_enum->layout_manager;
			layout_child_data.layout_item = layout_item;

			EnumChildWindows (hwnd, &_r_layout_enumcontrolscallback, (LPARAM)&layout_child_data);
		}
	}
	else
	{
		_r_layout_additem (layout_enum->layout_manager, layout_enum->layout_item, hwnd, 0);
	}

	return TRUE;
}

FORCEINLINE VOID _r_layout_enumcontrols (_Inout_ PR_LAYOUT_MANAGER layout_manager)
{
	R_LAYOUT_ENUM layout_enum = {0};

	layout_enum.root_hwnd = layout_manager->root_item.hwnd;
	layout_enum.layout_manager = layout_manager;
	layout_enum.layout_item = &layout_manager->root_item;

	EnumChildWindows (layout_manager->root_item.hwnd, &_r_layout_enumcontrolscallback, (LPARAM)&layout_enum);
}

FORCEINLINE ULONG _r_layout_getcontrolflags (_In_ HWND hwnd)
{
	WCHAR class_name[128];

	if (GetClassName (hwnd, class_name, RTL_NUMBER_OF (class_name)))
	{
		if (_r_str_compare (class_name, WC_STATIC) == 0)
		{
			return PR_LAYOUT_FORCE_INVALIDATE;
		}
		else if (_r_str_compare (class_name, STATUSCLASSNAME) == 0)
		{
			return PR_LAYOUT_SEND_NOTIFY | PR_LAYOUT_NO_ANCHOR;
		}
		else if (_r_str_compare (class_name, REBARCLASSNAME) == 0 || _r_str_compare (class_name, TOOLBARCLASSNAME) == 0)
		{
			return PR_LAYOUT_SEND_NOTIFY;
		}
	}

	return 0;
}

BOOLEAN _r_layout_initializemanager (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND hwnd)
{
	R_RECTANGLE rect;

	if (!_r_wnd_getposition (hwnd, &rect))
		return FALSE;

	layout_manager->original_size.x = rect.width;
	layout_manager->original_size.y = rect.height;

	layout_manager->root_item.hwnd = hwnd;
	layout_manager->root_item.defer_handle = NULL;
	layout_manager->root_item.parent_item = NULL;
	layout_manager->root_item.number_of_children = 0;
	layout_manager->root_item.flags = 0;

	GetClientRect (hwnd, &layout_manager->root_item.rect);

	SetRectEmpty (&layout_manager->root_item.anchor);
	SetRectEmpty (&layout_manager->root_item.margin);

	layout_manager->list = _r_obj_createlistex (6, &_r_mem_free);

	// Enumerate child control and windows
	_r_layout_enumcontrols (layout_manager);

	layout_manager->is_initialized = TRUE;

	return TRUE;
}

_Ret_maybenull_
PR_LAYOUT_ITEM _r_layout_additem (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ PR_LAYOUT_ITEM parent_item, _In_ HWND hwnd, _In_ ULONG flags)
{
	PR_LAYOUT_ITEM layout_item;
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return NULL;

	layout_item = _r_mem_allocatezero (sizeof (R_LAYOUT_ITEM));

	layout_item->hwnd = hwnd;
	layout_item->parent_item = parent_item;
	layout_item->flags = flags | _r_layout_getcontrolflags (hwnd);

	MapWindowPoints (NULL, parent_item->hwnd, (PPOINT)&rect, 2);

	CopyRect (&layout_item->rect, &rect);

	// Compute control edge points if it did not set
	if (!(layout_item->flags & PR_LAYOUT_NO_ANCHOR))
	{
		if (!(layout_item->flags & PR_LAYOUT_ANCHOR_ALL))
		{
			LONG horz_break = _r_calc_percentval (48, _r_calc_rectwidth (&layout_item->parent_item->rect));
			LONG vert_break = _r_calc_percentval (78, _r_calc_rectheight (&layout_item->parent_item->rect));

			// If the top-edge of the control is within the upper-half of the client area, set a top-anchor.
			if (rect.top < vert_break)
				layout_item->flags |= PR_LAYOUT_ANCHOR_TOP;

			// If the bottom-edge of the control is within the lower-half of the client area, set a bottom-anchor.
			if (rect.bottom > vert_break)
				layout_item->flags |= PR_LAYOUT_ANCHOR_BOTTOM;

			// If the left-edge of the control is within the left-half of the client area, set a left-anchor.
			if (rect.left < horz_break)
				layout_item->flags |= PR_LAYOUT_ANCHOR_LEFT;

			// If the right-edge of the control is within the right-half of the client area, set a right-anchor.
			if (rect.right > horz_break)
				layout_item->flags |= PR_LAYOUT_ANCHOR_RIGHT;
		}

		// set control anchor points
		_r_layout_setitemanchor (layout_manager, layout_item, layout_item->flags);
	}

	layout_item->parent_item->number_of_children += 1;

	_r_obj_addlistitem (layout_manager->list, layout_item);

	return layout_item;
}

VOID _r_layout_edititemanchors (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND * hwnds, _In_ PULONG anchors, _In_ SIZE_T count)
{
	PR_LAYOUT_ITEM layout_item;
	SIZE_T i;
	SIZE_T j;

	if (!layout_manager->is_initialized)
		return;

	for (i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		for (j = 0; j < count; j++)
		{
			if (layout_item->hwnd != hwnds[j])
				continue;

			_r_layout_setitemanchor (layout_manager, layout_item, anchors[j]);
		}
	}
}

VOID _r_layout_resizeitem (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item)
{
	RECT rect;
	LONG width;
	LONG height;

	if (layout_item->number_of_children > 0 && !layout_item->defer_handle)
		layout_item->defer_handle = BeginDeferWindowPos (layout_item->number_of_children);

	if (!layout_item->parent_item)
		return;

	_r_layout_resizeitem (layout_manager, layout_item->parent_item);

	width = layout_manager->root_item.rect.right;
	height = layout_manager->root_item.rect.bottom;

	SetRect (&rect,
			 layout_item->margin.left + _r_calc_percentval (layout_item->anchor.left, width),
			 layout_item->margin.top + _r_calc_percentval (layout_item->anchor.top, height),
			 layout_item->margin.right + _r_calc_percentval (layout_item->anchor.right, width),
			 layout_item->margin.bottom + _r_calc_percentval (layout_item->anchor.bottom, height)
	);

	layout_item->parent_item->defer_handle = DeferWindowPos (layout_item->parent_item->defer_handle,
															 layout_item->hwnd,
															 NULL,
															 rect.left,
															 rect.top,
															 _r_calc_rectwidth (&rect),
															 _r_calc_rectheight (&rect),
															 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER
	);

	CopyRect (&layout_item->rect, &rect);
}

BOOLEAN _r_layout_resize (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ WPARAM wparam)
{
	PR_LAYOUT_ITEM layout_item;
	SIZE_T i;

	if (!layout_manager->is_initialized)
		return FALSE;

	if (wparam != SIZE_RESTORED && wparam != SIZE_MAXIMIZED)
		return FALSE;

	if (!GetClientRect (layout_manager->root_item.hwnd, &layout_manager->root_item.rect))
		return FALSE;

	for (i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		if ((layout_item->flags & PR_LAYOUT_SEND_NOTIFY))
		{
			SendMessage (layout_item->hwnd, WM_SIZE, 0, 0);
		}

		if (!(layout_item->flags & PR_LAYOUT_NO_ANCHOR))
		{
			_r_layout_resizeitem (layout_manager, layout_item);
		}
	}

	for (i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		if (layout_item->defer_handle)
		{
			EndDeferWindowPos (layout_item->defer_handle);

			layout_item->defer_handle = NULL;
		}

		if ((layout_item->flags & PR_LAYOUT_FORCE_INVALIDATE))
		{
			InvalidateRect (layout_item->hwnd, NULL, FALSE);
		}
	}

	if (layout_manager->root_item.defer_handle)
	{
		EndDeferWindowPos (layout_manager->root_item.defer_handle);

		layout_manager->root_item.defer_handle = NULL;
	}

	return TRUE;
}

VOID _r_layout_setitemanchor (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item, _In_ ULONG flags)
{
	LONG width;
	LONG height;

	width = layout_manager->root_item.rect.right;
	height = layout_manager->root_item.rect.bottom;

	layout_item->flags &= ~(PR_LAYOUT_ANCHOR_ALL);
	layout_item->flags |= flags; // set new anchor flags

	// set control anchor points
	SetRect (&layout_item->anchor,
			 (layout_item->flags & PR_LAYOUT_ANCHOR_LEFT) ? 0 : 100,
			 (layout_item->flags & PR_LAYOUT_ANCHOR_TOP) ? 0 : 100,
			 (layout_item->flags & PR_LAYOUT_ANCHOR_RIGHT) ? 100 : 0,
			 (layout_item->flags & PR_LAYOUT_ANCHOR_BOTTOM) ? 100 : 0
	);

	SetRect (&layout_item->margin,
			 layout_item->rect.left - _r_calc_percentval (layout_item->anchor.left, width),
			 layout_item->rect.top - _r_calc_percentval (layout_item->anchor.top, height),
			 layout_item->rect.right - _r_calc_percentval (layout_item->anchor.right, width),
			 layout_item->rect.bottom - _r_calc_percentval (layout_item->anchor.bottom, height)
	);
}

/*
	Window management
*/

VOID _r_wnd_addstyle (_In_ HWND hwnd, INT _In_opt_ ctrl_id, _In_ LONG_PTR mask, _In_ LONG_PTR state_mask, _In_ INT index)
{
	if (ctrl_id)
		hwnd = GetDlgItem (hwnd, ctrl_id);

	LONG_PTR style = (GetWindowLongPtr (hwnd, index) & ~state_mask) | mask;

	SetWindowLongPtr (hwnd, index, style);

	SetWindowPos (hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

VOID _r_wnd_adjustworkingarea (_In_opt_ HWND hwnd, _Inout_ PR_RECTANGLE rectangle)
{
	MONITORINFO monitor_info = {0};
	HMONITOR hmonitor = NULL;

	monitor_info.cbSize = sizeof (monitor_info);

	if (hwnd)
	{
		hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
	}
	else
	{
		RECT rect;

		_r_wnd_rectangletorect (&rect, rectangle);

		hmonitor = MonitorFromRect (&rect, MONITOR_DEFAULTTONEAREST);
	}

	if (GetMonitorInfo (hmonitor, &monitor_info))
	{
		R_RECTANGLE bounds;

		_r_wnd_recttorectangle (&bounds, &monitor_info.rcWork);
		_r_wnd_adjustrectangletobounds (rectangle, &bounds);
	}
}

VOID _r_wnd_center (_In_ HWND hwnd, _In_opt_ HWND hparent)
{
	R_RECTANGLE rect;
	R_RECTANGLE parent_rect;

	if (hparent)
	{
		if (IsWindowVisible (hparent) && !IsIconic (hparent))
		{
			if (_r_wnd_getposition (hwnd, &rect) && _r_wnd_getposition (hparent, &parent_rect))
			{
				_r_wnd_centerwindowrect (&rect, &parent_rect);
				_r_wnd_adjustworkingarea (hwnd, &rect);

				_r_wnd_setposition (hwnd, &rect.position, NULL);

				return;
			}
		}
	}

	MONITORINFO monitor_info = {0};
	monitor_info.cbSize = sizeof (monitor_info);

	if (GetMonitorInfo (MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info))
	{
		if (_r_wnd_getposition (hwnd, &rect))
		{
			_r_wnd_recttorectangle (&parent_rect, &monitor_info.rcWork);
			_r_wnd_centerwindowrect (&rect, &parent_rect);

			_r_wnd_setposition (hwnd, &rect.position, NULL);
		}
	}
}

VOID _r_wnd_changemessagefilter (_In_ HWND hwnd, _In_count_ (count) PUINT messages, _In_ SIZE_T count, _In_ ULONG action)
{
#if defined(APP_NO_DEPRECATIONS)

	for (SIZE_T i = 0; i < count; i++)
		ChangeWindowMessageFilterEx (hwnd, messages[i], action, NULL);

#else

	HMODULE huser32 = LoadLibraryEx (L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (!huser32)
		return;

	typedef BOOL (WINAPI *CWMFEX)(HWND hwnd, UINT message, ULONG action, PCHANGEFILTERSTRUCT pChangeFilterStruct); // win7+
	const CWMFEX _ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (huser32, "ChangeWindowMessageFilterEx");

	if (_ChangeWindowMessageFilterEx)
	{
		for (SIZE_T i = 0; i < count; i++)
			_ChangeWindowMessageFilterEx (hwnd, messages[i], action, NULL);
	}

	FreeLibrary (huser32);
#endif // APP_NO_DEPRECATIONS
}

VOID _r_wnd_changesettings (_In_ HWND hwnd, _In_opt_ WPARAM wparam, _In_opt_ LPARAM lparam)
{
	LPCWSTR type = (LPCWSTR)lparam;

	if (_r_str_isempty (type))
		return;

	if (_r_str_compare (type, L"WindowMetrics") == 0)
	{
		SendMessage (hwnd, RM_LOCALIZE, 0, 0);
		SendMessage (hwnd, WM_THEMECHANGED, 0, 0);
	}
}

VOID _r_wnd_enablenonclientscaling (_In_ HWND hwnd)
{
	// Note: Applications running at a DPI_AWARENESS_CONTEXT of DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
	// automatically scale their non-client areas by default. They do not need to call this function.

	if (!_r_sys_isosversionequal (WINDOWS_10_1607)) // win10rs1 only
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
	QUERY_USER_NOTIFICATION_STATE state = 0;

	// SHQueryUserNotificationState is only available for Vista+
#if defined(APP_NO_DEPRECATIONS)
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
#endif // APP_NO_DEPRECATIONS

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
	LONG_PTR style = _r_wnd_getstyle (hwnd);
	LONG_PTR exstyle = _r_wnd_getstyle_ex (hwnd);

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) || (exstyle & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
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

BOOLEAN _r_wnd_isoverlapped (_In_ HWND hwnd)
{
	RECT rect_original;
	RECT rect_current;
	RECT rect_intersection;
	HWND hwnd_current = hwnd;

	if (!GetWindowRect (hwnd, &rect_original))
		return FALSE;

	SetRectEmpty (&rect_intersection);

	while ((hwnd_current = GetWindow (hwnd_current, GW_HWNDPREV)) && hwnd_current != hwnd)
	{
		if (!(_r_wnd_getstyle (hwnd_current) & WS_VISIBLE))
			continue;

		if (!GetWindowRect (hwnd_current, &rect_current))
			continue;

		if (!(_r_wnd_getstyle_ex (hwnd_current) & WS_EX_TOPMOST) && IntersectRect (&rect_intersection, &rect_original, &rect_current))
			return TRUE;
	}

	return FALSE;
}

// Author: Mikhail
// https://stackoverflow.com/a/9126096

BOOLEAN _r_wnd_isundercursor (_In_ HWND hwnd)
{
	if (!IsWindowVisible (hwnd) || IsIconic (hwnd))
		return FALSE;

	RECT rect;
	POINT point;

	if (!GetCursorPos (&point) || !GetWindowRect (hwnd, &rect))
		return FALSE;

	return !!PtInRect (&rect, point);
}

VOID _r_wnd_setposition (_In_ HWND hwnd, _In_opt_ PSIZE position, _In_opt_ PSIZE size)
{
	R_RECTANGLE rectangle;
	UINT swp_flags;

	swp_flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	memset (&rectangle, 0, sizeof (rectangle));

	if (position)
	{
		rectangle.left = position->cx;
		rectangle.top = position->cy;
	}
	else
	{
		swp_flags |= SWP_NOMOVE;
	}

	if (size)
	{
		rectangle.width = size->cx;
		rectangle.height = size->cy;
	}
	else
	{
		swp_flags |= SWP_NOSIZE;
	}

	SetWindowPos (hwnd, NULL, rectangle.left, rectangle.top, rectangle.width, rectangle.height, swp_flags);
}

VOID _r_wnd_toggle (_In_ HWND hwnd, _In_ BOOLEAN is_show)
{
	BOOLEAN is_success;
	BOOLEAN is_minimized;

	is_minimized = !!IsIconic (hwnd);

	if (is_show || !IsWindowVisible (hwnd) || is_minimized || _r_wnd_isoverlapped (hwnd))
	{
		is_success = !!ShowWindow (hwnd, is_minimized ? SW_RESTORE : SW_SHOW);

		if (!is_success && GetLastError () == ERROR_ACCESS_DENIED)
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_RESTORE, 0); // uipi fix
		}

		SetForegroundWindow (hwnd);
	}
	else
	{
		ShowWindow (hwnd, SW_HIDE);
	}
}

_Success_ (return)
BOOLEAN _r_wnd_getposition (_In_ HWND hwnd, _Out_ PR_RECTANGLE rectangle)
{
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return FALSE;

	_r_wnd_recttorectangle (rectangle, &rect);

	return TRUE;
}

/*
	Inernet access (WinHTTP)
*/

_Check_return_
_Ret_maybenull_
HINTERNET _r_inet_createsession (_In_opt_ LPCWSTR useragent)
{
	BOOLEAN is_win81 = _r_sys_isosversiongreaterorequal (WINDOWS_8_1);
	HINTERNET hsession = WinHttpOpen (useragent, is_win81 ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return NULL;

	if (!is_win81)
	{
		WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2}, sizeof (ULONG));
	}
	else
	{
		// enable secure protocols
		WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3}, sizeof (ULONG));

		// enable compression feature (win81+)
		WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &(ULONG){WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE}, sizeof (ULONG));

		// enable http2 protocol (win10rs1+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			WinHttpSetOption (hsession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &(ULONG){WINHTTP_PROTOCOL_FLAG_HTTP2}, sizeof (ULONG));
		}
	}

	return hsession;
}

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Out_ LPHINTERNET pconnect, _Out_ LPHINTERNET prequest, _Out_opt_ PULONG total_length)
{
	R_URLPARTS url_parts;
	HINTERNET hconnect;
	HINTERNET hrequest;
	ULONG flags;
	ULONG code;

	code = _r_inet_parseurlparts (url, &url_parts, PR_URLPARTS_SCHEME | PR_URLPARTS_PORT | PR_URLPARTS_HOST | PR_URLPARTS_PATH);

	if (code != ERROR_SUCCESS)
	{
		goto CleanupExit;
	}
	else
	{
		hconnect = WinHttpConnect (hsession, url_parts.host->buffer, url_parts.port, 0);

		if (!hconnect)
		{
			code = GetLastError ();

			goto CleanupExit;
		}

		flags = WINHTTP_FLAG_REFRESH;

		if (url_parts.scheme == INTERNET_SCHEME_HTTPS)
			flags |= WINHTTP_FLAG_SECURE;

		hrequest = WinHttpOpenRequest (hconnect, NULL, url_parts.path->buffer, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

		if (!hrequest)
		{
			code = GetLastError ();

			_r_inet_close (hconnect);

			goto CleanupExit;
		}

		// disable "keep-alive" feature (win7+)
#if !defined(APP_NO_DEPRECATIONS)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
#endif // !APP_NO_DEPRECATIONS
		{
			WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &(ULONG){WINHTTP_DISABLE_KEEP_ALIVE}, sizeof (ULONG));
		}

		ULONG attempts = 6;

		do
		{
			if (!WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0))
			{
				code = GetLastError ();

				if (code == ERROR_WINHTTP_RESEND_REQUEST)
				{
					continue;
				}
				else if (code == ERROR_WINHTTP_NAME_NOT_RESOLVED || code == ERROR_NOT_ENOUGH_MEMORY)
				{
					break;
				}
				else
				{
#if !defined(APP_NO_DEPRECATIONS)
					// win7 and lower fix
					if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
						break;

					if (code == ERROR_WINHTTP_CONNECTION_ERROR)
					{
						if (!WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2}, sizeof (ULONG)))
							break;
					}
					else if (code == ERROR_WINHTTP_SECURE_FAILURE)
					{
						if (!WinHttpSetOption (hrequest, WINHTTP_OPTION_SECURITY_FLAGS, &(ULONG){SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID}, sizeof (ULONG)))
							break;
					}
#else
					// ERROR_WINHTTP_CANNOT_CONNECT etc.
					break;
#endif
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
					if (total_length)
					{
						ULONG param_size = sizeof (ULONG);

						if (!WinHttpQueryHeaders (hrequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, total_length, &param_size, WINHTTP_NO_HEADER_INDEX))
							*total_length = 0;
					}

					*pconnect = hconnect;
					*prequest = hrequest;

					code = ERROR_SUCCESS;

					goto CleanupExit;
				}
			}
		}
		while (--attempts);

		_r_inet_close (hrequest);
		_r_inet_close (hconnect);
	}

CleanupExit:

	_r_inet_destroyurlparts (&url_parts);

	return code;
}

_Check_return_
_Success_ (return)
BOOLEAN _r_inet_readrequest (_In_ HINTERNET hrequest, _Out_writes_bytes_ (buffer_size) PVOID buffer, _In_ ULONG buffer_size, _Out_opt_ PULONG readed, _Inout_opt_ PULONG total_readed)
{
	ULONG bytes_count;

	if (!WinHttpReadData (hrequest, buffer, buffer_size, &bytes_count))
		return FALSE;

	if (!bytes_count)
		return FALSE;

	if (readed)
		*readed = bytes_count;

	if (total_readed)
		*total_readed += bytes_count;

	return TRUE;
}

VOID _r_inet_destroyurlparts (_Inout_ PR_URLPARTS url_parts)
{
	if ((url_parts->flags & PR_URLPARTS_HOST))
		_r_obj_clearreference (&url_parts->host);

	if ((url_parts->flags & PR_URLPARTS_PATH))
		_r_obj_clearreference (&url_parts->path);

	if ((url_parts->flags & PR_URLPARTS_USER))
		_r_obj_clearreference (&url_parts->user);

	if ((url_parts->flags & PR_URLPARTS_PASS))
		_r_obj_clearreference (&url_parts->pass);
}

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_parseurlparts (_In_ LPCWSTR url, _Inout_ PR_URLPARTS url_parts, _In_ ULONG flags)
{
	if (_r_str_isempty (url))
		return ERROR_BAD_ARGUMENTS;

	URL_COMPONENTS url_comp = {0};

	url_comp.dwStructSize = sizeof (url_comp);

	memset (url_parts, 0, sizeof (R_URLPARTS));

	url_parts->flags = flags;

	if ((flags & PR_URLPARTS_HOST))
	{
		url_parts->host = _r_obj_createstringex (NULL, MAX_PATH);

		url_comp.lpszHostName = url_parts->host->buffer;
		url_comp.dwHostNameLength = (ULONG)(url_parts->host->length / sizeof (WCHAR));
	}

	if ((flags & PR_URLPARTS_PATH))
	{
		url_parts->path = _r_obj_createstringex (NULL, MAX_PATH);

		url_comp.lpszUrlPath = url_parts->path->buffer;
		url_comp.dwUrlPathLength = (ULONG)(url_parts->path->length / sizeof (WCHAR));
	}

	if ((flags & PR_URLPARTS_USER))
	{
		url_parts->user = _r_obj_createstringex (NULL, MAX_PATH);

		url_comp.lpszUserName = url_parts->user->buffer;
		url_comp.dwUserNameLength = (ULONG)(url_parts->user->length / sizeof (WCHAR));
	}

	if ((flags & PR_URLPARTS_PASS))
	{
		url_parts->pass = _r_obj_createstringex (NULL, MAX_PATH);

		url_comp.lpszPassword = url_parts->pass->buffer;
		url_comp.dwPasswordLength = (ULONG)(url_parts->pass->length / sizeof (WCHAR));
	}

	if (!WinHttpCrackUrl (url, (ULONG)_r_str_length (url), ICU_DECODE, &url_comp))
		return  GetLastError ();

	if ((url_parts->flags & PR_URLPARTS_SCHEME))
		url_parts->scheme = url_comp.nScheme;

	if ((url_parts->flags & PR_URLPARTS_PORT))
		url_parts->port = url_comp.nPort;

	if ((url_parts->flags & PR_URLPARTS_HOST))
		_r_obj_trimstringtonullterminator (url_parts->host);

	if ((url_parts->flags & PR_URLPARTS_PATH))
		_r_obj_trimstringtonullterminator (url_parts->path);

	if ((url_parts->flags & PR_URLPARTS_USER))
		_r_obj_trimstringtonullterminator (url_parts->user);

	if ((url_parts->flags & PR_URLPARTS_PASS))
		_r_obj_trimstringtonullterminator (url_parts->pass);

	return ERROR_SUCCESS;
}

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_begindownload (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Inout_ PR_DOWNLOAD_INFO pdi)
{
	HINTERNET hconnect;
	HINTERNET hrequest;
	R_STRINGBUILDER buffer_string = {0};
	PR_STRING decoded_string;
	PR_BYTE content_bytes;
	ULONG readed_current;
	ULONG readed_total = 0;
	ULONG length_total = 0;
	ULONG unused;
	ULONG code;

	code = _r_inet_openurl (hsession, url, &hconnect, &hrequest, &length_total);

	if (code != ERROR_SUCCESS)
		return code;

	if (!pdi->hfile)
		_r_obj_initializestringbuilder (&buffer_string);

	content_bytes = _r_obj_createbyteex (NULL, 65536);

	while (_r_inet_readrequest (hrequest, content_bytes->buffer, (ULONG)content_bytes->length, &readed_current, &readed_total))
	{
		*(LPSTR)PTR_ADD_OFFSET (content_bytes->buffer, readed_current) = ANSI_NULL; // terminate

		_r_sys_setthreadexecutionstate (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

		if (pdi->hfile)
		{
			if (!WriteFile (pdi->hfile, content_bytes->buffer, readed_current, &unused, NULL))
			{
				code = GetLastError ();
				break;
			}
		}
		else
		{
			decoded_string = _r_str_multibyte2unicodeex (content_bytes->buffer, readed_current);

			if (decoded_string)
			{
				_r_obj_appendstringbuilder2 (&buffer_string, decoded_string);
				_r_obj_dereference (decoded_string);
			}
			else
			{
				code = ERROR_CANCELLED;
				break;
			}
		}

		if (pdi->download_callback && !pdi->download_callback (readed_total, max (readed_total, length_total), pdi->data))
		{
			code = ERROR_CANCELLED;
			break;
		}
	}

	if (!pdi->hfile)
	{
		if (code == ERROR_SUCCESS)
		{
			pdi->string = _r_obj_finalstringbuilder (&buffer_string);
		}
		else
		{
			_r_obj_deletestringbuilder (&buffer_string);
		}
	}

	_r_sys_setthreadexecutionstate (ES_CONTINUOUS);

	_r_obj_dereference (content_bytes);

	_r_inet_close (hrequest);
	_r_inet_close (hconnect);

	return code;
}

VOID _r_inet_destroydownload (_In_ PR_DOWNLOAD_INFO pdi)
{
	if (_r_fs_isvalidhandle (pdi->hfile))
		CloseHandle (pdi->hfile);

	if (pdi->string)
		_r_obj_dereference (pdi->string);
}

/*
	Registry
*/

_Ret_maybenull_
PR_BYTE _r_reg_querybinary (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value)
{
	ULONG type = 0;
	ULONG size = 0;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_BINARY)
		{
			PR_BYTE bytes = _r_obj_createbyteex (NULL, size);

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)bytes->buffer, &size) == ERROR_SUCCESS)
				return bytes;

			_r_obj_dereference (bytes); // cleanup
		}
	}

	return NULL;
}

ULONG _r_reg_queryulong (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value)
{
	ULONG type = 0;
	ULONG size = 0;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_DWORD)
		{
			ULONG buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (ULONG)buffer;
		}
	}

	return 0;
}

ULONG64 _r_reg_queryulong64 (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value)
{
	ULONG type = 0;
	ULONG size = 0;

	if (RegQueryValueEx (hkey, value, NULL, &type, NULL, &size) == ERROR_SUCCESS)
	{
		if (type == REG_DWORD)
		{
			ULONG buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return (ULONG64)buffer;
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
				return buffer;
		}
	}

	return 0;
}

_Ret_maybenull_
PR_STRING _r_reg_querystring (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value)
{
	ULONG type = 0;
	ULONG size = 0;
	ULONG code;

	code = RegGetValue (hkey, subkey, value, RRF_RT_ANY, &type, NULL, &size);

	if (code == ERROR_SUCCESS)
	{
		if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
		{
			PR_STRING value_string = _r_obj_createstringex (NULL, size * sizeof (WCHAR));

			size = (ULONG)value_string->length;
			code = RegGetValue (hkey, subkey, value, RRF_RT_ANY, NULL, (PBYTE)value_string->buffer, &size);

			if (code == ERROR_MORE_DATA)
			{
				_r_obj_movereference (&value_string, _r_obj_createstringex (NULL, size * sizeof (WCHAR)));

				size = (ULONG)value_string->length;
				code = RegGetValue (hkey, subkey, value, RRF_RT_ANY, NULL, (PBYTE)value_string->buffer, &size);
			}

			if (code == ERROR_SUCCESS)
			{
				_r_obj_trimstringtonullterminator (value_string);

				return value_string;
			}

			_r_obj_dereference (value_string);
		}
		else if (type == REG_DWORD)
		{
			ULONG buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
			{
				return _r_format_string (L"%" PR_ULONG, buffer);
			}
		}
		else if (type == REG_QWORD)
		{
			ULONG64 buffer = 0;

			if (RegQueryValueEx (hkey, value, NULL, NULL, (PBYTE)&buffer, &size) == ERROR_SUCCESS)
			{
				return _r_format_string (L"%" PR_ULONG64, buffer);
			}
		}
	}

	return NULL;
}

ULONG _r_reg_querysubkeylength (_In_ HKEY hkey)
{
	ULONG max_subkey_length = 0;

	if (RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, &max_subkey_length, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		return max_subkey_length;

	return 0;
}

LONG64 _r_reg_querytimestamp (_In_ HKEY hkey)
{
	FILETIME file_time = {0};

	if (RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &file_time) == ERROR_SUCCESS)
		return _r_unixtime_from_filetime (&file_time);

	return 0;
}

/*
	Math
*/

ULONG _r_math_exponentiate (_In_ ULONG base, _In_ ULONG exponent)
{
	ULONG result = 1;

	while (exponent)
	{
		if (exponent & 1)
			result *= base;

		exponent >>= 1;
		base *= base;
	}

	return result;
}

ULONG64 _r_math_exponentiate64 (_In_ ULONG64 base, _In_ ULONG exponent)
{
	ULONG64 result = 1;

	while (exponent)
	{
		if (exponent & 1)
			result *= base;

		exponent >>= 1;
		base *= base;
	}

	return result;
}

ULONG _r_math_rand (_In_ ULONG min_number, _In_ ULONG max_number)
{
	static ULONG seed = 0; // save seed

	return min_number + (RtlRandomEx (&seed) % (max_number - min_number + 1));
}

SIZE_T _r_math_rounduptopoweroftwo (_In_ SIZE_T number)
{
	number -= 1;

	number |= number >> 1;
	number |= number >> 2;
	number |= number >> 4;
	number |= number >> 8;
	number |= number >> 16;

	return number + 1;
}

/*
	Resources
*/

_Ret_maybenull_
PR_STRING _r_res_getbinaryversion (_In_ LPCWSTR path)
{
	PVOID ver_data;
	ULONG ver_size = GetFileVersionInfoSize (path, NULL);

	if (!ver_size)
		return NULL;

	ver_data = _r_mem_allocatezero (ver_size);

	if (GetFileVersionInfo (path, 0, ver_size, ver_data))
	{
		PVOID buffer;
		UINT size;

		if (VerQueryValue (ver_data, L"\\", &buffer, &size))
		{
			VS_FIXEDFILEINFO *ver_info = (VS_FIXEDFILEINFO*)buffer;

			if (ver_info->dwSignature == 0xFEEF04BD)
			{
				// Doesn't matter if you are on 32 bit or 64 bit,
				// DWORD is always 32 bits, so first two revision numbers
				// come from dwFileVersionMS, last two come from dwFileVersionLS

				PR_STRING string = _r_format_string (L"%d.%d.%d.%d", (ver_info->dwFileVersionMS >> 16) & 0xFFFF, (ver_info->dwFileVersionMS >> 0) & 0xFFFF, (ver_info->dwFileVersionLS >> 16) & 0xFFFF, (ver_info->dwFileVersionLS >> 0) & 0xFFFF);

				_r_mem_free (ver_data);

				return string;
			}
		}
	}

	_r_mem_free (ver_data);

	return NULL;
}

_Success_ (return != NULL)
PVOID _r_res_loadresource (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ LPCWSTR type, _Out_opt_ PULONG buffer_size)
{
	HRSRC hres = FindResource (hinst, name, type);

	if (hres)
	{
		HGLOBAL hloaded = LoadResource (hinst, hres);

		if (hloaded)
		{
			PVOID res = LockResource (hloaded);

			if (res)
			{
				if (buffer_size)
					*buffer_size = SizeofResource (hinst, hres);

				return res;
			}
		}
	}

	return NULL;
}

/*
	Other
*/

_Ret_maybenull_
HICON _r_loadicon (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ INT size)
{
	HICON hicon;

#if defined(APP_NO_DEPRECATIONS)

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

#endif // APP_NO_DEPRECATIONS
}

PR_HASHTABLE _r_parseini (_In_ LPCWSTR path, _Inout_opt_ PR_LIST section_list)
{
	WCHAR buffer[256];
	R_HASHSTORE hashstore;
	PR_HASHTABLE hashtable;
	PR_STRING sections_string;
	PR_STRING values_string;
	PR_STRING section_string;
	PR_STRING key_name;
	LPCWSTR section_buffer;
	LPCWSTR value_buffer;
	SIZE_T section_length;
	SIZE_T value_length;
	SIZE_T delimeter_pos;
	SIZE_T hash_code;
	ULONG out_length;
	ULONG allocation_length;

	hashtable = _r_obj_createhashtable (sizeof (hashstore), &_r_util_dereferencehashstoreprocedure);

	// get section names
	allocation_length = 0x0800; // maximum length for GetPrivateProfileSectionNames

	sections_string = _r_obj_createstringex (NULL, allocation_length * sizeof (WCHAR));
	out_length = GetPrivateProfileSectionNames (sections_string->buffer, allocation_length, path);

	if (!out_length)
	{
		_r_obj_dereference (sections_string);

		return hashtable;
	}

	_r_obj_setstringsize (sections_string, out_length * sizeof (WCHAR));

	allocation_length = 0x7FFF; // maximum length for GetPrivateProfileSection
	values_string = _r_obj_createstringex (NULL, allocation_length * sizeof (WCHAR));

	section_buffer = sections_string->buffer;

	// get section values
	while (!_r_str_isempty (section_buffer))
	{
		section_length = _r_str_length (section_buffer);
		section_string = _r_obj_createstringex (section_buffer, section_length * sizeof (WCHAR));

		out_length = GetPrivateProfileSection (section_buffer, values_string->buffer, allocation_length, path);
		_r_obj_setstringsize (values_string, out_length * sizeof (WCHAR));

		value_buffer = values_string->buffer;

		if (section_list)
			_r_obj_addlistitem (section_list, _r_obj_reference (section_string));

		while (!_r_str_isempty (value_buffer))
		{
			value_length = _r_str_length (value_buffer);
			delimeter_pos = _r_str_findchar (value_buffer, L'=');

			if (delimeter_pos != SIZE_MAX)
			{
				// do not load comments
				if (value_buffer[0] != L'#')
				{
					key_name = _r_str_extractex (value_buffer, value_length, 0, delimeter_pos);

					if (key_name)
					{
						// set hash code in table to "section\key" string
						_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", section_string->buffer, key_name->buffer);

						hash_code = _r_str_hash (buffer);

						if (hash_code)
						{
							_r_obj_initializehashstore (&hashstore, _r_str_extractex (value_buffer, value_length, delimeter_pos + 1, value_length - delimeter_pos - 1), 0);

							_r_obj_addhashtableitem (hashtable, hash_code, &hashstore);
						}

						_r_obj_dereference (key_name);
					}
				}
			}

			value_buffer += value_length + 1; // go next value
		}

		_r_obj_dereference (section_string);

		section_buffer += section_length + 1; // go next section
	}

	_r_obj_dereference (sections_string);
	_r_obj_dereference (values_string);

	return hashtable;
}

VOID _r_sleep (_In_ LONG64 milliseconds)
{
	if (!milliseconds || milliseconds == INFINITE)
		return;

	LARGE_INTEGER interval = {0};
	interval.QuadPart = -(LONG64)UInt32x32To64 (milliseconds, ((1 * 10) * 1000));

	NtDelayExecution (FALSE, &interval);
}

/*
	Xml library
*/

_Success_ (return == S_OK)
HRESULT _r_xml_initializelibrary (_Out_ PR_XML_LIBRARY xml_library, _In_ BOOLEAN is_reader, _In_opt_ PR_XML_STREAM_CALLBACK stream_callback)
{
	HRESULT hr;

	memset (xml_library, 0, sizeof (R_XML_LIBRARY));

	if (is_reader)
	{
		hr = CreateXmlReader (&IID_IXmlReader, (void**)&xml_library->reader, NULL);

		if (FAILED (hr))
			return hr;

		IXmlReader_SetProperty (xml_library->reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
	}
	else
	{
		hr = CreateXmlWriter (&IID_IXmlWriter, (void**)&xml_library->writer, NULL);

		if (FAILED (hr))
			return S_FALSE;

		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_Indent, TRUE);
		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_CompactEmptyElement, TRUE);
	}

	xml_library->is_initialized = TRUE;
	xml_library->is_reader = is_reader;

	xml_library->stream = NULL;
	xml_library->stream_callback = stream_callback;

	return S_OK;
}

_Success_ (return == S_OK)
HRESULT _r_xml_parsefile (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR file_path)
{
	PR_XML_STREAM stream;
	HRESULT hr;

	if (!xml_library->is_initialized)
		return S_FALSE;

	if (xml_library->stream)
	{
		_r_xml_setlibrarystream (xml_library, NULL);

		IStream_Release (xml_library->stream);
		xml_library->stream = NULL;
	}

	hr = SHCreateStreamOnFileEx (file_path, xml_library->is_reader ? STGM_READ : STGM_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, xml_library->is_reader ? FALSE : TRUE, NULL, &stream);

	if (hr != S_OK)
		return hr;

	return _r_xml_setlibrarystream (xml_library, stream);
}

_Success_ (return == S_OK)
HRESULT _r_xml_parsestring (_Inout_ PR_XML_LIBRARY xml_library, _In_ PVOID buffer, _In_ ULONG buffer_size)
{
	PR_XML_STREAM stream;

	if (!xml_library->is_initialized)
		return S_FALSE;

	if (xml_library->stream)
	{
		_r_xml_setlibrarystream (xml_library, NULL);

		IStream_Release (xml_library->stream);
		xml_library->stream = NULL;
	}

	stream = SHCreateMemStream (buffer, buffer_size);

	if (!stream)
		return S_FALSE;

	return _r_xml_setlibrarystream (xml_library, stream);
}

_Success_ (return)
BOOLEAN _r_xml_getattribute (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name, _Out_ LPCWSTR * attrib_value, _Out_opt_ PUINT attrib_length)
{
	LPCWSTR value_string;
	UINT value_length;
	HRESULT hr;

	if (!xml_library->is_initialized || !xml_library->is_reader)
		return FALSE;

	if (IXmlReader_MoveToAttributeByName (xml_library->reader, attrib_name, NULL) != S_OK)
		return FALSE;

	hr = IXmlReader_GetValue (xml_library->reader, &value_string, &value_length);

	// restore position before exit function
	IXmlReader_MoveToElement (xml_library->reader);

	if (hr != S_OK)
		return FALSE;

	if (_r_str_isempty (value_string) || !value_length)
		return FALSE;

	*attrib_value = value_string;

	if (attrib_length)
		*attrib_length = value_length;

	return TRUE;
}

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name)
{
	LPCWSTR text_value;
	UINT text_length;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value, &text_length))
		return NULL;

	return _r_obj_createstringex (text_value, text_length * sizeof (WCHAR));
}

BOOLEAN _r_xml_getattribute_boolean (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name)
{
	LPCWSTR text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value, NULL))
		return FALSE;

	return _r_str_toboolean (text_value);
}

INT _r_xml_getattribute_integer (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name)
{
	LPCWSTR text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value, NULL))
		return 0;

	return _r_str_tointeger (text_value);
}

LONG64 _r_xml_getattribute_long64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name)
{
	LPCWSTR text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value, NULL))
		return 0;

	return _r_str_tolong64 (text_value);
}

VOID _r_xml_setattribute_integer (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ INT value)
{
	WCHAR value_text[64];
	_r_str_frominteger (value_text, RTL_NUMBER_OF (value_text), value);

	_r_xml_setattribute (xml_library, name, value_text);
}

VOID _r_xml_setattribute_long64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ LONG64 value)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_xml_setattribute (xml_library, name, value_text);
}

_Success_ (return)
BOOLEAN _r_xml_enumchilditemsbytagname (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR tag_name)
{
	XmlNodeType node_type;
	LPCWSTR qualified_name;

	if (!xml_library->is_initialized || !xml_library->is_reader)
		return FALSE;

	while (TRUE)
	{
		if (IXmlReader_IsEOF (xml_library->reader))
			return FALSE;

		if (IXmlReader_Read (xml_library->reader, &node_type) != S_OK)
			return FALSE;

		if (node_type == XmlNodeType_Element)
		{
			if (IXmlReader_GetQualifiedName (xml_library->reader, &qualified_name, NULL) != S_OK)
				return FALSE;

			if (_r_str_compare (qualified_name, tag_name) == 0)
				return TRUE;
		}
		else if (node_type == XmlNodeType_EndElement)
		{
			return FALSE;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_xml_findchildbytagname (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR tag_name)
{
	LPCWSTR qualified_name;
	XmlNodeType node_type;

	if (!xml_library->is_initialized || !xml_library->is_reader)
		return FALSE;

	_r_xml_resetlibrarystream (xml_library);

	while (!IXmlReader_IsEOF (xml_library->reader))
	{
		if (IXmlReader_Read (xml_library->reader, &node_type) != S_OK)
			break;

		if (node_type == XmlNodeType_Element)
		{
			// do not return empty elements
			if (IXmlReader_IsEmptyElement (xml_library->reader))
				continue;

			if (IXmlReader_GetQualifiedName (xml_library->reader, &qualified_name, NULL) != S_OK)
				break;

			if (_r_str_compare (qualified_name, tag_name) == 0)
				return TRUE;
		}
	}

	return FALSE;
}

_Success_ (return == S_OK)
HRESULT _r_xml_resetlibrarystream (_Inout_ PR_XML_LIBRARY xml_library)
{
	if (!xml_library->is_initialized)
		return S_FALSE;

	if (!xml_library->stream)
		return S_FALSE;

	IStream_Reset (xml_library->stream);

	return _r_xml_setlibrarystream (xml_library, xml_library->stream);
}

_Success_ (return == S_OK)
HRESULT _r_xml_setlibrarystream (_Inout_ PR_XML_LIBRARY xml_library, _In_ PR_XML_STREAM stream)
{
	HRESULT hr;

	if (!xml_library->is_initialized)
		return S_FALSE;

	xml_library->stream = stream;

	if (xml_library->is_reader)
	{
		if (xml_library->stream_callback)
			xml_library->stream_callback (stream);

		hr = IXmlReader_SetInput (xml_library->reader, (IUnknown*)stream);
	}
	else
	{
		hr = IXmlWriter_SetOutput (xml_library->writer, (IUnknown*)stream);
	}

	return hr;
}

VOID _r_xml_destroylibrary (_Inout_ PR_XML_LIBRARY xml_library)
{
	if (!xml_library->is_initialized)
		return;

	xml_library->is_initialized = FALSE;

	if (xml_library->is_reader)
	{
		if (xml_library->reader)
		{
			IXmlReader_Release (xml_library->reader);
			xml_library->reader = NULL;
		}
	}
	else
	{
		if (xml_library->writer)
		{
			IXmlWriter_Release (xml_library->writer);
			xml_library->writer = NULL;
		}
	}

	if (xml_library->stream)
	{
		IStream_Release (xml_library->stream);
		xml_library->stream = NULL;
	}
}

/*
	System tray
*/

BOOLEAN _r_tray_create (_In_ HWND hwnd, _In_ UINT uid, _In_ UINT code, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip, _In_ BOOLEAN is_hidden)
{
	NOTIFYICONDATA nid = {0};

#if defined(APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // APP_NO_DEPRECATIONS

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
#if defined(APP_NO_DEPRECATIONS)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // APP_NO_DEPRECATIONS

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
#if defined(APP_NO_DEPRECATIONS)
		nid.uVersion = NOTIFYICON_VERSION_4;
#else
		nid.uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // APP_NO_DEPRECATIONS

		Shell_NotifyIcon (NIM_SETVERSION, &nid);

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_tray_popup (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_ LPCWSTR text)
{
	NOTIFYICONDATA nid = {0};

#if defined(APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	nid.cbSize = (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA) ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // APP_NO_DEPRECATIONS

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

BOOLEAN _r_tray_popupformat (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;
	BOOLEAN status;

	if (!format)
		return FALSE;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return FALSE;

	status = _r_tray_popup (hwnd, uid, icon_id, title, string->buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_setinfo (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip)
{
	NOTIFYICONDATA nid = {0};

#if defined(APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	BOOLEAN is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid.cbSize = (is_vistaorlater ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // APP_NO_DEPRECATIONS

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (hicon)
	{
		nid.uFlags |= NIF_ICON;
		nid.hIcon = hicon;
	}

	if (tooltip)
	{
#if defined(APP_NO_DEPRECATIONS)
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;
#else
		nid.uFlags |= NIF_TIP;

		if (is_vistaorlater)
			nid.uFlags |= NIF_SHOWTIP;
#endif // APP_NO_DEPRECATIONS

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_setinfoformat (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ HICON hicon, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;
	BOOLEAN status;

	if (!format)
		return FALSE;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return FALSE;

	status = _r_tray_setinfo (hwnd, uid, hicon, string->buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_toggle (_In_ HWND hwnd, _In_ UINT uid, _In_ BOOLEAN is_show)
{
	NOTIFYICONDATA nid = {0};

#if defined(APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	nid.cbSize = (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA) ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // APP_NO_DEPRECATIONS

	nid.uFlags = NIF_STATE;
	nid.hWnd = hwnd;
	nid.uID = uid;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_destroy (_In_ HWND hwnd, _In_ UINT uid)
{
	NOTIFYICONDATA nid = {0};

#if defined(APP_NO_DEPRECATIONS)
	nid.cbSize = sizeof (nid);
#else
	nid.cbSize = (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA) ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
#endif // APP_NO_DEPRECATIONS

	nid.hWnd = hwnd;
	nid.uID = uid;

	return !!Shell_NotifyIcon (NIM_DELETE, &nid);
}

/*
	Control: common
*/

INT _r_ctrl_isradiobuttonchecked (_In_ HWND hwnd, _In_ INT start_id, _In_ INT end_id)
{
	for (INT i = start_id; i <= end_id; i++)
	{
		if (IsDlgButtonChecked (hwnd, i) == BST_CHECKED)
			return i;
	}

	return 0;
}

_Ret_maybenull_
PR_STRING _r_ctrl_gettext (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	PR_STRING string;
	ULONG length;

	length = _r_ctrl_gettextlength (hwnd, ctrl_id);

	if (!length)
		return NULL;

	string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

	if ((ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXT, (WPARAM)length + 1, (LPARAM)string->buffer))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;

	}

	_r_obj_dereference (string);

	return NULL;
}

VOID _r_ctrl_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return;

	_r_ctrl_settext (hwnd, ctrl_id, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_setbuttonmargins (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	BUTTON_SPLITINFO bsi;
	RECT padding_rect;
	HWND hbutton;
	LONG padding;

	hbutton = GetDlgItem (hwnd, ctrl_id);

	if (!hbutton)
		return;

	// set button text margin
	padding = _r_dc_getdpi (hwnd, 4);

	SetRect (&padding_rect, padding, 0, padding, 0);

	SendMessage (hbutton, BCM_SETTEXTMARGIN, 0, (LPARAM)&padding_rect);

	// set button split margin
	if ((_r_wnd_getstyle (hbutton) & BS_SPLITBUTTON) == BS_SPLITBUTTON)
	{
		memset (&bsi, 0, sizeof (bsi));

		bsi.mask = BCSIF_SIZE;

		bsi.size.cx = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON) + _r_dc_getdpi (hwnd, 2);
		//bsi.size.cy = 0;

		SendMessage (hbutton, BCM_SETSPLITINFO, 0, (LPARAM)&bsi);
	}
}

VOID _r_ctrl_settabletext (_In_ HWND hwnd, _In_ INT ctrl_id1, _In_ LPCWSTR text1, _In_ INT ctrl_id2, _In_ LPCWSTR text2)
{
	RECT window_rect = {0};
	RECT control_rect = {0};
	HDWP hdefer;
	HWND hctrl1;
	HWND hctrl2;
	HDC hdc;
	INT wnd_spacing;
	INT wnd_width;
	INT ctrl1_width;
	INT ctrl2_width;

	hdc = GetDC (hwnd);

	if (!hdc)
		return;

	hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	SelectObject (hdc, (HFONT)SendDlgItemMessage (hwnd, ctrl_id1, WM_GETFONT, 0, 0)); // fix
	SelectObject (hdc, (HFONT)SendDlgItemMessage (hwnd, ctrl_id2, WM_GETFONT, 0, 0)); // fix

	GetClientRect (hwnd, &window_rect);
	GetWindowRect (hctrl1, &control_rect);

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&control_rect, 2);

	wnd_spacing = control_rect.left;
	wnd_width = window_rect.right - (wnd_spacing * 2);

	ctrl1_width = _r_dc_getfontwidth (hdc, text1, _r_str_length (text1)) + wnd_spacing;
	ctrl2_width = _r_dc_getfontwidth (hdc, text2, _r_str_length (text2)) + wnd_spacing;

	ctrl2_width = min (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);
	ctrl1_width = min (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing); // note: changed order for correct priority!

	_r_ctrl_settext (hwnd, ctrl_id1, text1);
	_r_ctrl_settext (hwnd, ctrl_id2, text2);

	hdefer = BeginDeferWindowPos (2);

	if (hdefer)
	{
		hdefer = DeferWindowPos (hdefer, hctrl1, NULL, wnd_spacing, control_rect.top, ctrl1_width, _r_calc_rectheight (&control_rect), SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
		hdefer = DeferWindowPos (hdefer, hctrl2, NULL, wnd_width - ctrl2_width, control_rect.top, ctrl2_width + wnd_spacing, _r_calc_rectheight (&control_rect), SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

		EndDeferWindowPos (hdefer);
	}

	ReleaseDC (hwnd, hdc);
}

_Ret_maybenull_
HWND _r_ctrl_createtip (_In_opt_ HWND hparent)
{
	HWND htip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_CHILD | WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hparent, NULL, GetModuleHandle (NULL), NULL);

	if (htip)
	{
		_r_ctrl_settipstyle (htip);

		SendMessage (htip, TTM_ACTIVATE, TRUE, 0);
	}

	return htip;
}

VOID _r_ctrl_settiptext (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ LPCWSTR text)
{
	TOOLINFO ti = {0};

	ti.cbSize = sizeof (ti);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = hparent;
	ti.hinst = _r_sys_getimagebase ();
	ti.uId = (UINT_PTR)GetDlgItem (hparent, ctrl_id);
	ti.lpszText = (LPWSTR)text;

	GetClientRect (hparent, &ti.rect);

	SendMessage (htip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

VOID _r_ctrl_settiptextformat (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return;

	_r_ctrl_settiptext (htip, hparent, ctrl_id, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_settipstyle (_In_ HWND htip)
{
	SendMessage (htip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);
	SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

	_r_wnd_top (htip, TRUE); // HACK!!!
}

VOID _r_ctrl_showballoontip (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ LPCWSTR text)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon_id;

	SendDlgItemMessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

VOID _r_ctrl_showballoontipformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return;

	_r_ctrl_showballoontip (hwnd, ctrl_id, icon_id, title, string->buffer);

	_r_obj_dereference (string);
}

/*
	Control: menu
*/

VOID _r_menu_checkitem (_In_ HMENU hmenu, _In_ UINT item_id_start, _In_opt_ UINT item_id_end, _In_ UINT position_flag, _In_ UINT check_id)
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

VOID _r_menu_clearitems (_In_ HMENU hmenu)
{
	while (TRUE)
	{
		if (!DeleteMenu (hmenu, 0, MF_BYPOSITION))
		{
			DeleteMenu (hmenu, 0, MF_BYPOSITION); // delete separator
			break;
		}
	}
}

VOID _r_menu_setitembitmap (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ HBITMAP hbitmap)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbitmap;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtext (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ LPCWSTR text)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = (LPWSTR)text;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtextformat (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return;

	_r_menu_setitemtext (hmenu, item_id, is_byposition, string->buffer);

	_r_obj_dereference (string);
}

INT _r_menu_popup (_In_ HMENU hmenu, _In_ HWND hwnd, _In_opt_ PPOINT point, _In_ BOOLEAN is_sendmessage)
{
	POINT pt;
	INT command_id;

	if (!point)
	{
		GetCursorPos (&pt);
		point = &pt;
	}

	command_id = TrackPopupMenuEx (hmenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON, point->x, point->y, hwnd, NULL);

	if (is_sendmessage && command_id && hwnd)
		PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

	return command_id;
}

/*
	Control: tab
*/

VOID _r_tab_adjustchild (_In_ HWND hwnd, _In_ INT tab_id, _In_ HWND hchild)
{
	HWND htab = GetDlgItem (hwnd, tab_id);

	RECT rect;
	RECT child_rect;

	if (!htab || !GetWindowRect (htab, &rect) || !GetClientRect (htab, &child_rect))
		return;

	MapWindowPoints (NULL, hwnd, (PPOINT)&rect, 2);

	SetRect (&rect, child_rect.left + rect.left, child_rect.top + rect.top, child_rect.right + rect.left, child_rect.bottom + rect.top);

	SendDlgItemMessage (hwnd, tab_id, TCM_ADJUSTRECT, FALSE, (LPARAM)&rect);

	SetWindowPos (hchild, NULL, rect.left, rect.top, _r_calc_rectwidth (&rect), _r_calc_rectheight (&rect), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

INT _r_tab_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam)
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

LPARAM _r_tab_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index)
{
	if (index == -1)
	{
		index = _r_tab_getcurrentitem (hwnd, ctrl_id);

		if (index == -1)
			return 0;
	}

	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	if (!!((INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEM, (WPARAM)index, (LPARAM)&tci)))
		return tci.lParam;

	return 0;
}

INT _r_tab_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam)
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

VOID _r_tab_selectitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index)
{
	NMHDR hdr = {0};

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

INT _r_listview_addcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR title, _In_opt_ INT width, _In_opt_ INT fmt)
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
		if (width < LVSCW_AUTOSIZE_USEHEADER)
		{
			RECT rect;
			HWND hlistview;

			hlistview = GetDlgItem (hwnd, ctrl_id);

			if (hlistview && GetClientRect (hlistview, &rect))
				width = (INT)_r_calc_percentval (-width, rect.right);
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

INT _r_listview_addgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT align, _In_opt_ UINT state, _In_opt_ UINT state_mask)
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

INT _r_listview_additemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_ INT group_id, _In_opt_ LPARAM lparam)
{
	if (item == -1)
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

_Success_ (return != -1)
INT _r_listview_finditem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT start_pos, _In_ LPARAM lparam)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	return (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)start_pos, (LPARAM)&lvfi);
}

VOID _r_listview_deleteallcolumns (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	INT column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	for (INT i = column_count; i >= 0; i--)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
}

INT _r_listview_getcolumncount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	HWND header = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (header)
		return (INT)SendMessage (header, HDM_GETITEMCOUNT, 0, 0);

	return 0;
}

PR_STRING _r_listview_getcolumntext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id)
{
	LVCOLUMN lvc = {0};
	PR_STRING string;
	SIZE_T allocated_count;

	allocated_count = 0x200;
	string = _r_obj_createstringex (NULL, allocated_count * sizeof (WCHAR));

	lvc.mask = LVCF_TEXT;
	lvc.pszText = string->buffer;
	lvc.cchTextMax = (INT)allocated_count + 1;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);

	_r_obj_trimstringtonullterminator (string);

	return string;
}

INT _r_listview_getcolumnwidth (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id)
{
	RECT rect;
	HWND hlistview;
	INT total_width;
	INT column_width;

	hlistview = GetDlgItem (hwnd, ctrl_id);

	if (!hlistview || !GetClientRect (hlistview, &rect))
		return 0;

	total_width = rect.right;
	column_width = (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0);

	return (INT)_r_calc_percentof (column_width, total_width);
}

INT _r_listview_getitemcheckedcount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	INT total_count = _r_listview_getitemcount (hwnd, ctrl_id);
	INT checks_count = 0;

	for (INT i = 0; i < total_count; i++)
	{
		if (_r_listview_isitemchecked (hwnd, ctrl_id, i))
			checks_count += 1;
	}

	return checks_count;
}

INT _r_listview_getitemgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_GROUPID;
	lvi.iItem = item;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.iGroupId;
}

LPARAM _r_listview_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

PR_STRING _r_listview_getitemtext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem)
{
	LVITEM lvi = {0};
	PR_STRING string = NULL;
	ULONG allocated_count;
	ULONG count;

	allocated_count = 0x100;
	count = allocated_count;

	while (count >= allocated_count)
	{
		allocated_count *= 2;
		_r_obj_movereference (&string, _r_obj_createstringex (NULL, allocated_count * sizeof (WCHAR)));

		lvi.iSubItem = subitem;
		lvi.pszText = string->buffer;
		lvi.cchTextMax = (INT)allocated_count + 1;

		count = (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMTEXT, (WPARAM)item, (LPARAM)&lvi);
	}

	_r_obj_trimstringtonullterminator (string);

	return string;
}

VOID _r_listview_redraw (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	if (item_id != -1)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, (WPARAM)item_id, (LPARAM)item_id);
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, 0, (LPARAM)_r_listview_getitemcount (hwnd, ctrl_id));
	}
}

VOID _r_listview_setcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR text, _In_opt_ INT width)
{
	LVCOLUMN lvc = {0};

	if (text)
	{
		lvc.mask |= LVCF_TEXT;
		lvc.pszText = (LPWSTR)text;
	}

	if (width)
	{
		if (width < LVSCW_AUTOSIZE_USEHEADER)
		{
			RECT rect;
			HWND hlistview;

			hlistview = GetDlgItem (hwnd, ctrl_id);

			if (hlistview && GetClientRect (hlistview, &rect))
				width = (INT)_r_calc_percentval (-width, rect.right);
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

VOID _r_listview_setcolumnsortindex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_ INT arrow)
{
	HWND header = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (!header)
		return;

	HDITEM hitem = {0};

	hitem.mask = HDI_FORMAT;

	if (!Header_GetItem (header, column_id, &hitem))
		return;

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

VOID _r_listview_setitemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_ INT group_id, _In_opt_ LPARAM lparam)
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

VOID _r_listview_setitemcheck (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ BOOLEAN is_check)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.state = INDEXTOSTATEIMAGEMASK (is_check ? 2 : 1);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item, (LPARAM)&lvi);
}

VOID _r_listview_setitemvisible (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	LVITEM lvi = {0};

	lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;

	lvi.state = 0;
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&lvi);

	lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item, (LPARAM)&lvi);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_ENSUREVISIBLE, (WPARAM)item, TRUE);
}

VOID _r_listview_setgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT state, _In_opt_ UINT state_mask)
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

VOID _r_listview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_ BOOL is_groupview)
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

HTREEITEM _r_treeview_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ LPCWSTR text, _In_opt_ HTREEITEM hparent, _In_ INT image, _In_opt_ LPARAM lparam)
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

LPARAM _r_treeview_getlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = hitem;

	SendDlgItemMessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

VOID _r_treeview_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam)
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

VOID _r_treeview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_opt_ INT height, _In_opt_ INT indent)
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

VOID _r_status_settext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part, _In_ LPCWSTR text)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part, 0), (LPARAM)text);
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTIPTEXT, (WPARAM)part, (LPARAM)text);
}

VOID _r_status_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;
	PR_STRING string;

	if (!format)
		return;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	if (!string)
		return;

	_r_status_settext (hwnd, ctrl_id, part, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_status_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ INT height)
{
	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, SB_SETMINHEIGHT, (WPARAM)height, 0);

	SendDlgItemMessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

/*
	Control: toolbar
*/

VOID _r_toolbar_addbutton (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id, _In_ INT style, _In_opt_ INT_PTR text, _In_ INT state, _In_ INT image)
{
	TBBUTTON tbi = {0};

	tbi.idCommand = command_id;
	tbi.fsStyle = (BYTE)style;
	tbi.iString = text;
	tbi.fsState = (BYTE)state;
	tbi.iBitmap = image;

	SendDlgItemMessage (hwnd, ctrl_id, TB_INSERTBUTTON, (WPARAM)_r_toolbar_getbuttoncount (hwnd, ctrl_id), (LPARAM)&tbi);
}

INT _r_toolbar_getwidth (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	RECT rect = {0};
	INT width = 0;

	for (INT i = 0; i < _r_toolbar_getbuttoncount (hwnd, ctrl_id); i++)
	{
		if (SendDlgItemMessage (hwnd, ctrl_id, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&rect) != 0)
		{
			width += _r_calc_rectwidth (&rect);
		}
	}

	return width;
}

VOID _r_toolbar_setbutton (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id, _In_opt_ LPCWSTR text, _In_opt_ INT style, _In_opt_ INT state, _In_ INT image)
{
	TBBUTTONINFO tbi = {0};

	tbi.cbSize = sizeof (tbi);

	if (text)
	{
		tbi.dwMask |= TBIF_TEXT;
		tbi.pszText = (LPWSTR)text;
	}

	if (style)
	{
		tbi.dwMask |= TBIF_STYLE;
		tbi.fsStyle = (BYTE)style;
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

VOID _r_toolbar_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle)
{
	SetWindowTheme (GetDlgItem (hwnd, ctrl_id), L"Explorer", NULL);

	HWND htip = (HWND)SendDlgItemMessage (hwnd, ctrl_id, TB_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);

	if (exstyle)
		SendDlgItemMessage (hwnd, ctrl_id, TB_SETEXTENDEDSTYLE, 0, (LPARAM)exstyle);
}

/*
	Control: progress bar
*/

VOID _r_progress_setmarquee (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOL is_enable)
{
	SendDlgItemMessage (hwnd, ctrl_id, PBM_SETMARQUEE, (WPARAM)is_enable, (LPARAM)10);

	_r_wnd_addstyle (hwnd, ctrl_id, is_enable ? PBS_MARQUEE : 0, PBS_MARQUEE, GWL_STYLE);
}

/*
	Util
*/

VOID _r_util_templatewritestring (_Inout_ PBYTE * ptr, _In_ LPCWSTR string)
{
	SIZE_T length = _r_str_length (string) * sizeof (WCHAR);

	*(PWCHAR)PTR_ADD_OFFSET (*ptr, length) = UNICODE_NULL; // terminate

	_r_util_templatewrite (ptr, string, length + sizeof (UNICODE_NULL));
}

VOID _r_util_templatewritecontrol (_Inout_ PBYTE * ptr, _In_ ULONG ctrl_id, _In_ ULONG style, _In_ SHORT x, _In_ SHORT y, _In_ SHORT cx, _In_ SHORT cy, _In_ LPCWSTR class_name)
{
	*ptr = (PBYTE)ALIGN_UP (*ptr, ULONG); // align as ULONG

	// fill DLGITEMTEMPLATEEX
	_r_util_templatewriteulong (ptr, 0); // helpID
	_r_util_templatewriteulong (ptr, 0); // exStyle
	_r_util_templatewriteulong (ptr, style); // style

	_r_util_templatewriteshort (ptr, x); // x
	_r_util_templatewriteshort (ptr, y); // y
	_r_util_templatewriteshort (ptr, cx); // cx
	_r_util_templatewriteshort (ptr, cy); // cy

	_r_util_templatewriteulong (ptr, ctrl_id); // id

	_r_util_templatewritestring (ptr, class_name); // windowClass
	_r_util_templatewritestring (ptr, L""); // title

	_r_util_templatewriteshort (ptr, 0); // extraCount
}

PR_STRING _r_util_versionformat (_In_ PR_STRING string)
{
	if (_r_str_isnumeric (string->buffer))
	{
		UINT length = 256;
		PR_STRING date_string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

		_r_format_unixtimeex (date_string->buffer, length, _r_str_tolong64 (string->buffer), FDTF_SHORTDATE | FDTF_SHORTTIME);
		_r_obj_trimstringtonullterminator (date_string);

		return date_string;
	}

	return _r_obj_reference (string);
}

BOOL CALLBACK _r_util_activate_window_callback (_In_ HWND hwnd, _In_ LPARAM lparam)
{
	LPCWSTR app_name = (LPCWSTR)lparam;
	WCHAR window_title[128];
	ULONG pid;

	if (!_r_wnd_isdialog (hwnd))
		return TRUE;

	GetWindowThreadProcessId (hwnd, &pid);

	if (GetCurrentProcessId () == pid)
		return TRUE;

	// check window title
	if (GetWindowText (hwnd, window_title, RTL_NUMBER_OF (window_title)) && _r_str_compare_length (window_title, app_name, _r_str_length (app_name)) == 0)
	{
		// check window prop
		if (GetProp (hwnd, app_name))
		{
			_r_wnd_toggle (hwnd, TRUE);
			return FALSE;
		}
	}

	return TRUE;
}

VOID NTAPI _r_util_dereferencearrayprocedure (_In_ PVOID entry)
{
	PR_ARRAY array_node = entry;
	PVOID array_items;

	_r_obj_cleararray (array_node);

	if (array_node->items)
	{
		array_items = array_node->items;
		array_node->items = NULL;

		_r_mem_free (array_items);
	}
}

VOID NTAPI _r_util_dereferencelistprocedure (_In_ PVOID entry)
{
	PR_LIST list_node = entry;
	PVOID* list_items;

	_r_obj_clearlist (list_node);

	if (list_node->items)
	{
		list_items = list_node->items;
		list_node->items = NULL;

		_r_mem_free (list_items);
	}
}

VOID NTAPI _r_util_dereferencehashtableprocedure (_In_ PVOID entry)
{
	PR_HASHTABLE hashtable = entry;
	PVOID hashtable_entry;

	hashtable->count = 0;

	_r_obj_clearhashtable (hashtable);

	if (hashtable->buckets)
	{
		hashtable_entry = hashtable->buckets;
		hashtable->buckets = NULL;

		_r_mem_free (hashtable_entry);
	}

	if (hashtable->entries)
	{
		hashtable_entry = hashtable->entries;
		hashtable->entries = NULL;

		_r_mem_free (hashtable_entry);
	}
}

VOID NTAPI _r_util_dereferencehashstoreprocedure (_In_ PVOID entry)
{
	PR_HASHSTORE hashstore = entry;

	SAFE_DELETE_REFERENCE (hashstore->value_string);
}
