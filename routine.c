// routine.c
// project sdk library
//
// Copyright (c) 2012-2022 Henry++

#include "routine.h"

//
// Vars
//

static R_QUEUED_LOCK _r_context_lock = PR_QUEUED_LOCK_INIT;

//
// Debugging
//

VOID _r_debug (
	_In_ LPCWSTR string
)
{
	OutputDebugString (string);
}

VOID _r_debug_v (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	WCHAR string[512];
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	_r_debug (string);
}

VOID _r_error_initialize (
	_Out_ PR_ERROR_INFO error_info,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ LPCWSTR description
)
{
	_r_error_initialize_ex (error_info, hinstance, description, NULL);
}

VOID _r_error_initialize_ex (
	_Out_ PR_ERROR_INFO error_info,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ LPCWSTR description,
	_In_opt_ PEXCEPTION_POINTERS exception_ptr
)
{
	error_info->description = description;
	error_info->exception_ptr = exception_ptr;
	error_info->hinst = hinstance;
}

//
// Console
//

_Ret_maybenull_
HANDLE _r_console_gethandle ()
{
	HANDLE hconsole;

	hconsole = _r_sys_getstdout ();

	if (hconsole == INVALID_HANDLE_VALUE)
		return NULL;

	return hconsole;
}

WORD _r_console_getcolor ()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hconsole;

	hconsole = _r_console_gethandle ();

	if (!hconsole)
		return 0;

	if (GetConsoleScreenBufferInfo (hconsole, &csbi))
		return csbi.wAttributes;

	return 0;
}

VOID _r_console_setcolor (
	_In_ WORD clr
)
{
	HANDLE hconsole;

	hconsole = _r_console_gethandle ();

	if (!hconsole)
		return;

	SetConsoleTextAttribute (hconsole, clr);
}

VOID _r_console_writestring (
	_In_ LPCWSTR string
)
{
	_r_console_writestring_ex (string, (ULONG)_r_str_getlength (string));
}

VOID _r_console_writestring2 (
	_In_ PR_STRING string
)
{
	_r_console_writestring_ex (string->buffer, (ULONG)_r_str_getlength2 (string));
}

VOID _r_console_writestring3 (
	_In_ PR_STRINGREF string
)
{
	_r_console_writestring_ex (string->buffer, (ULONG)_r_str_getlength3 (string));
}

VOID _r_console_writestring_ex (
	_In_ LPCWSTR string,
	_In_ ULONG length
)
{
	HANDLE hconsole;

	hconsole = _r_console_gethandle ();

	if (!hconsole)
		return;

	WriteConsole (hconsole, string, length, NULL, NULL);
}

VOID _r_console_writestringformat (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_console_writestring2 (string);

	_r_obj_dereference (string);
}

//
// Format strings, dates, numbers
//

PR_STRING _r_format_string (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	return string;
}

PR_STRING _r_format_string_v (
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	PR_STRING string;
	LONG length;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return _r_obj_referenceemptystring ();

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (string->buffer, length, format, arg_ptr);
#pragma warning(pop)

	return string;
}

_Success_ (return)
BOOLEAN _r_format_bytesize64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ UINT buffer_size,
	_In_ ULONG64 bytes
)
{
	HRESULT hr;

#if defined(APP_NO_DEPRECATIONS)
	// vista (sp1)+
	hr = StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, buffer_size);

	if (hr == S_OK)
		return TRUE;
#else
	HINSTANCE hshlwapi;
	SFBSE _StrFormatByteSizeEx;

	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		hshlwapi = _r_sys_loadlibrary (L"shlwapi.dll");

		if (hshlwapi)
		{
			// vista (sp1+)
			_StrFormatByteSizeEx = (SFBSE)GetProcAddress (hshlwapi, "StrFormatByteSizeEx");

			FreeLibrary (hshlwapi);

			if (_StrFormatByteSizeEx)
			{
				hr = _StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, buffer_size);

				if (hr == S_OK)
					return TRUE;
			}
		}
	}
#endif // APP_NO_DEPRECATIONS

	if (StrFormatByteSize64 (bytes, buffer, buffer_size)) // fallback
		return TRUE;

	*buffer = UNICODE_NULL;

	return FALSE;
}

_Ret_maybenull_
PR_STRING _r_format_filetime_ex (
	_In_ LPFILETIME file_time,
	_In_ ULONG flags
)
{
	PR_STRING string;
	ULONG buffer_length;
	ULONG return_length;

	buffer_length = 128;
	string = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = SHFormatDateTime (file_time, &flags, string->buffer, buffer_length);

	if (return_length)
	{
		_r_obj_setstringlength (string, return_length * sizeof (WCHAR));

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_format_interval (
	_In_ LONG64 seconds,
	_In_ INT digits
)
{
	PR_STRING string;
	LONG seconds32;
	ULONG buffer_length;
	ULONG return_length;
	HRESULT hr;

	seconds = _r_calc_seconds2milliseconds64 (seconds);

	hr = LongLongToLong (seconds, &seconds32);

	if (hr != S_OK)
		return NULL;

	buffer_length = 128;
	string = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = StrFromTimeInterval (string->buffer, buffer_length, seconds32, digits);

	if (return_length)
	{
		_r_obj_setstringlength (string, return_length * sizeof (WCHAR));

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Success_ (return)
BOOLEAN _r_format_number (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ ULONG buffer_size,
	_In_ LONG64 number
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR decimal_separator[4] = {0};
	static WCHAR thousand_separator[4] = {0};

	NUMBERFMT number_format = {0};
	WCHAR number_string[64];
	LONG result;

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

	result = GetNumberFormat (LOCALE_USER_DEFAULT, 0, number_string, &number_format, buffer, buffer_size);

	if (!result)
		_r_str_copy (buffer, buffer_size, number_string);

	return TRUE;
}

_Ret_maybenull_
PR_STRING _r_format_unixtime (
	_In_ LONG64 unixtime
)
{
	return _r_format_unixtime_ex (unixtime, FDTF_DEFAULT);
}

_Ret_maybenull_
PR_STRING _r_format_unixtime_ex (
	_In_ LONG64 unixtime,
	_In_ ULONG flags
)
{
	FILETIME file_time;

	_r_unixtime_to_filetime (unixtime, &file_time);

	return _r_format_filetime_ex (&file_time, flags);
}

//
// Calculation
//

LONG _r_calc_clamp (
	_In_ LONG value,
	_In_ LONG min_value,
	_In_ LONG max_value
)
{
	if (value < min_value)
		value = min_value;

	if (value > max_value)
		value = max_value;

	return value;
}

LONG64 _r_calc_clamp64 (
	_In_ LONG64 value,
	_In_ LONG64 min_value,
	_In_ LONG64 max_value
)
{
	if (value < min_value)
		value = min_value;

	if (value > max_value)
		value = max_value;

	return value;
}

ULONG _r_calc_countbits (
	_In_ ULONG value
)
{
	ULONG count;

	count = 0;

	while (value)
	{
		count += 1;
		value &= value - 1;
	}

	return count;
}

VOID _r_calc_millisecondstolargeinteger (
	_Out_ PLARGE_INTEGER timeout,
	_In_ ULONG milliseconds
)
{
	if (milliseconds == 0 || milliseconds == INFINITE)
	{
		timeout->QuadPart = 0;
	}
	else
	{
		timeout->QuadPart = -(LONG64)UInt32x32To64 (milliseconds, 10000);
	}
}

ULONG _r_calc_multipledivide (
	_In_ ULONG number,
	_In_ ULONG numerator,
	_In_ ULONG denominator
)
{
	ULONG value;

	value = (ULONG)(((ULONG64)number * (ULONG64)numerator + denominator / 2) / (ULONG64)denominator);

	return value;

}

LONG _r_calc_multipledividesigned (
	_In_ LONG number,
	_In_ ULONG numerator,
	_In_ ULONG denominator
)
{
	LONG value;

	if (number >= 0)
	{
		value = _r_calc_multipledivide (number, numerator, denominator);
	}
	else
	{
		value = -(LONG)_r_calc_multipledivide (-number, numerator, denominator);
	}

	return value;
}

LONG _r_calc_percentof (
	_In_ LONG length,
	_In_ LONG total_length
)
{
	LONG value;

	value = (LONG)(((DOUBLE)length / (DOUBLE)total_length) * 100.0);

	return value;
}

LONG _r_calc_percentof64 (
	_In_ LONG64 length,
	_In_ LONG64 total_length
)
{
	LONG value;

	value = (LONG)(((DOUBLE)length / (DOUBLE)total_length) * 100.0);

	return value;
}

LONG _r_calc_percentval (
	_In_ LONG percent,
	_In_ LONG total_length
)
{
	LONG value;

	value = (total_length * percent) / 100;

	return value;
}

LONG64 _r_calc_percentval64 (
	_In_ LONG64 percent,
	_In_ LONG64 total_length
)
{
	LONG64 value;

	value = (total_length * percent) / 100;

	return value;
}

LONG _r_calc_rectheight (
	_In_ LPCRECT rect
)
{
	return rect->bottom - rect->top;
}

LONG _r_calc_rectwidth (
	_In_ LPCRECT rect
)
{
	return rect->right - rect->left;
}

ULONG64 _r_calc_roundnumber (
	_In_ ULONG64 value,
	_In_ ULONG64 granularity
)
{
	return (value + granularity / 2) / granularity * granularity;
}

//
// Synchronization: Auto-dereference pool
//

VOID _r_autopool_initialize (
	_Out_ PR_AUTO_POOL auto_pool
)
{
	auto_pool->static_count = 0;
	auto_pool->dynamic_count = 0;
	auto_pool->dynamic_allocated = 0;
	auto_pool->dynamic_objects = NULL;

	// Add the pool to the stack.
	auto_pool->next_pool = _r_autopool_getcurrentdata ();

	_r_autopool_setcurrentdata (auto_pool);
}

VOID _r_autopool_destroy (
	_Inout_ PR_AUTO_POOL auto_pool
)
{
	_r_autopool_drain (auto_pool);

	if (_r_autopool_getcurrentdata () != auto_pool)
		RtlRaiseStatus (STATUS_UNSUCCESSFUL);

	// Remove the pool from the stack
	_r_autopool_setcurrentdata (auto_pool->next_pool);

	// Free the dynamic array if it hasn't been freed yet
	SAFE_DELETE_MEMORY (auto_pool->dynamic_objects);
}

VOID _r_autopool_drain (
	_Inout_ PR_AUTO_POOL auto_pool
)
{
	if (auto_pool->static_count)
	{
		_r_obj_dereferencelist (auto_pool->static_objects, auto_pool->static_count);
		auto_pool->static_count = 0;
	}

	if (auto_pool->dynamic_count)
	{
		_r_obj_dereferencelist (auto_pool->dynamic_objects, auto_pool->dynamic_count);
		auto_pool->dynamic_count = 0;

		if (auto_pool->dynamic_allocated > PR_AUTO_POOL_DYNAMIC_BIG_SIZE)
		{
			auto_pool->dynamic_allocated = 0;

			_r_mem_free (auto_pool->dynamic_objects);
			auto_pool->dynamic_objects = NULL;
		}
	}
}

PR_AUTO_POOL _r_autopool_getcurrentdata ()
{
	ULONG tls_index;

	tls_index = _r_autopool_getthreadindex ();

	return (PR_AUTO_POOL)TlsGetValue (tls_index);
}

ULONG _r_autopool_getthreadindex ()
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

VOID _r_autopool_setcurrentdata (
	_In_ PR_AUTO_POOL auto_pool
)
{
	ULONG tls_index;

	tls_index = _r_autopool_getthreadindex ();

	if (!TlsSetValue (tls_index, auto_pool))
		RtlRaiseStatus (STATUS_UNSUCCESSFUL);
}

//
// Synchronization: A fast event object.
//

VOID FASTCALL _r_event_intialize (
	_Out_ PR_EVENT event_object
)
{
	event_object->value = PR_EVENT_REFCOUNT_INC;
	event_object->event_handle = NULL;
}

VOID FASTCALL _r_event_set (
	_Inout_ PR_EVENT event_object
)
{
	HANDLE event_handle;

	// Only proceed if the event isn't set already.
	if (!_InterlockedBitTestAndSetPointer ((PLONG_PTR)&event_object->value, PR_EVENT_SET_SHIFT))
	{
		event_handle = event_object->event_handle;

		if (event_handle)
			NtSetEvent (event_handle, NULL);

		_r_event_dereference (event_object, event_handle);
	}
}

VOID FASTCALL _r_event_reset (
	_Inout_ PR_EVENT event_object
)
{
	assert (!event_object->event_handle);

	if (_r_event_test (event_object))
		event_object->value = PR_EVENT_REFCOUNT_INC;
}

BOOLEAN FASTCALL _r_event_wait (
	_Inout_ PR_EVENT event_object,
	_In_opt_ PLARGE_INTEGER timeout
)
{
	if (_r_event_test (event_object))
		return TRUE;

	return _r_event_wait_ex (event_object, timeout);
}

BOOLEAN FASTCALL _r_event_wait_ex (
	_Inout_ PR_EVENT event_object,
	_In_opt_ PLARGE_INTEGER timeout
)
{
	HANDLE event_handle;
	ULONG_PTR value;
	BOOLEAN result;

	value = event_object->value;

	// Shortcut: if the event is set, return immediately.
	if (value & PR_EVENT_SET)
		return TRUE;

	// Shortcut: if the timeout is 0, return immediately if the event isn't set.
	if (timeout && timeout->QuadPart == 0)
		return FALSE;

	// Prevent the event from being invalidated.
	_r_event_reference (event_object);

	event_handle = event_object->event_handle;

	if (!event_handle)
	{
		NtCreateEvent (&event_handle, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

		assert (event_handle);

		// Try to set the event handle to our event.
		if (InterlockedCompareExchangePointer (&event_object->event_handle, event_handle, NULL) != NULL)
		{
			// Someone else set the event before we did.
			NtClose (event_handle);
			event_handle = event_object->event_handle;
		}
	}

	// Essential: check the event one last time to see if it is set.
	if (!(event_object->value & PR_EVENT_SET))
	{
		result = (NtWaitForSingleObject (event_handle, FALSE, timeout) == STATUS_WAIT_0);
	}
	else
	{
		result = TRUE;
	}

	_r_event_dereference (event_object, event_handle);

	return result;
}

//
// Synchronization: One-time initialization
//

#if defined(APP_NO_DEPRECATIONS)
BOOLEAN _r_initonce_begin (
	_Inout_ PR_INITONCE init_once
)
{
	NTSTATUS status;

	status = RtlRunOnceBeginInitialize (init_once, RTL_RUN_ONCE_CHECK_ONLY, NULL);

	if (NT_SUCCESS (status))
		return FALSE;

	status = RtlRunOnceBeginInitialize (init_once, 0, NULL);

	return (status == STATUS_PENDING);
}
#else
BOOLEAN FASTCALL _r_initonce_begin (
	_Inout_ PR_INITONCE init_once
)
{
	if (_r_event_test (&init_once->event_object))
		return FALSE;

	return _r_initonce_begin_ex (init_once);
}

BOOLEAN FASTCALL _r_initonce_begin_ex (
	_Inout_ PR_INITONCE init_once
)
{
	if (!_InterlockedBitTestAndSetPointer (&init_once->event_object.value, PR_INITONCE_INITIALIZING_SHIFT))
		return TRUE;

	_r_event_wait (&init_once->event_object, NULL);

	return FALSE;
}

VOID FASTCALL _r_initonce_end (
	_Inout_ PR_INITONCE init_once
)
{
	_r_event_set (&init_once->event_object);
}
#endif // APP_NO_DEPRECATIONS

//
// Synchronization: Free list
//

VOID _r_freelist_initialize (
	_Out_ PR_FREE_LIST free_list,
	_In_ SIZE_T size,
	_In_ ULONG maximum_count
)
{
	RtlInitializeSListHead (&free_list->list_head);

	free_list->size = size;
	free_list->count = 0;
	free_list->maximum_count = maximum_count;
}

VOID _r_freelist_destroy (
	_Inout_ PR_FREE_LIST free_list
)
{
	PSLIST_ENTRY list_entry;
	PR_FREE_LIST_ENTRY entry;

	list_entry = RtlInterlockedFlushSList (&free_list->list_head);

	while (list_entry)
	{
		entry = CONTAINING_RECORD (list_entry, R_FREE_LIST_ENTRY, list_entry);
		list_entry = list_entry->Next;

		_r_mem_free (entry);
	}
}

PVOID _r_freelist_allocateitem (
	_Inout_ PR_FREE_LIST free_list
)
{
	PSLIST_ENTRY list_entry;
	PR_FREE_LIST_ENTRY entry;

	list_entry = RtlInterlockedPopEntrySList (&free_list->list_head);

	if (list_entry)
	{
		InterlockedDecrement (&free_list->count);
		entry = CONTAINING_RECORD (list_entry, R_FREE_LIST_ENTRY, list_entry);

		RtlSecureZeroMemory (&entry->body, free_list->size);
	}
	else
	{
		entry = _r_mem_allocatezero (UFIELD_OFFSET (R_FREE_LIST_ENTRY, body) + free_list->size);
	}

	return &entry->body;
}

VOID _r_freelist_deleteitem (
	_Inout_ PR_FREE_LIST free_list,
	_In_ PVOID base_address
)
{
	PR_FREE_LIST_ENTRY entry;

	entry = CONTAINING_RECORD (base_address, R_FREE_LIST_ENTRY, body);

	// We don't enforce count <= maximum_count (that would require locking), but we do check it.
	if (free_list->count < free_list->maximum_count)
	{
		RtlInterlockedPushEntrySList (&free_list->list_head, &entry->list_entry);

		InterlockedIncrement (&free_list->count);
	}
	else
	{
		_r_mem_free (entry);
	}
}

//
// Synchronization: Queued lock
//

// Queued lock, a.k.a. push lock (kernel-mode) or slim reader-writer lock (user-mode).

// The queued lock is:
// - Around 10% faster than the fast lock.
// - Only the size of a pointer.
// - Low on resource usage (no additional kernel objects are created for blocking).

// The usual flags are used for contention-free acquire/release. When there is contention,
// stack-based wait blocks are chained. The first wait block contains the shared owners count which
// is decremented by shared releasers.

// Naturally these wait blocks would be chained in FILO order, but list optimization is done for two purposes:
// - Finding the last wait block (where the shared owners count is stored). This is implemented by the Last pointer.
// - Unblocking the wait blocks in FIFO order. This is implemented by the Previous pointer.

// The optimization is incremental - each optimization run will stop at the first optimized wait
// block. Any needed optimization is completed just before waking waiters.
//
// The waiters list/chain has the following restrictions:
// - At any time wait blocks may be pushed onto the list.
// - While waking waiters, the list may not be traversed nor optimized.
// - When there are multiple shared owners, shared releasers may traverse the list (to find the last
//   wait block). This is not an issue because waiters wouldn't be woken until there are no more
//   shared owners.
// - List optimization may be done at any time except for when someone else is waking waiters. This
//   is controlled by the traversing bit.

// The traversing bit has the following rules:
// - The list may be optimized only after the traversing bit is set, checking that it wasn't set
//   already. If it was set, it would indicate that someone else is optimizing the list or waking
//   waiters.
// - Before waking waiters the traversing bit must be set. If it was set already, just clear the
//   owned bit.
// - If during list optimization the owned bit is detected to be cleared, the function begins waking
//   waiters. This is because the owned bit is cleared when a releaser fails to set the traversing
//   bit.

// Blocking is implemented through a process-wide keyed event. A spin count is also used before
// blocking on the keyed event.

// Queued locks can act as condition variables, with wait, pulse and pulse all support. Waiters are
// released in FIFO order.

// Queued locks can act as wake events. These are designed for tiny one-bit locks which share a
// single event to block on. Spurious wake-ups are a part of normal operation.

// Source: https://github.com/processhacker2/processhacker

FORCEINLINE BOOLEAN _r_queuedlock_pushwaitblock (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_exclusive,
	_Out_ PR_QUEUED_WAIT_BLOCK wait_block,
	_Out_ PBOOLEAN is_optimize_ptr,
	_Out_ PULONG_PTR new_value_ptr,
	_Out_ PULONG_PTR current_value_ptr
)
{
	ULONG_PTR new_value;
	BOOLEAN is_optimize;

	wait_block->previous_block = NULL; // set later by optimization

	is_optimize = FALSE;

	if (is_exclusive)
	{
		wait_block->flags = PR_QUEUED_WAITER_EXCLUSIVE | PR_QUEUED_WAITER_SPINNING;
	}
	else
	{
		wait_block->flags = PR_QUEUED_WAITER_SPINNING;
	}

	if (value & PR_QUEUED_LOCK_WAITERS)
	{
		// We're not the first waiter.
		wait_block->last_block = NULL; // set later by optimization
		wait_block->next_block = _r_queuedlock_getwaitblock (value);
		wait_block->shared_owners = 0;

		// Push our wait block onto the list.
		// Set the traversing bit because we'll be optimizing the list.
		new_value = ((ULONG_PTR)wait_block) | (value & PR_QUEUED_LOCK_FLAGS) | PR_QUEUED_LOCK_TRAVERSING;

		if (!(value & PR_QUEUED_LOCK_TRAVERSING))
			is_optimize = TRUE;
	}
	else
	{
		// We're the first waiter.
		wait_block->last_block = wait_block; // indicate that this is the last wait block

		if (is_exclusive)
		{
			// We're the first waiter. Save the shared owners count.
			wait_block->shared_owners = (ULONG)_r_queuedlock_getsharedowners (value);

			if (wait_block->shared_owners > 1)
			{
				new_value = ((ULONG_PTR)wait_block) | PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_WAITERS | PR_QUEUED_LOCK_MULTIPLE_SHARED;
			}
			else
			{
				new_value = ((ULONG_PTR)wait_block) | PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_WAITERS;
			}
		}
		else
		{
			// We're waiting in shared mode, which means there can't be any shared owners (otherwise
			// we would've acquired the lock already).
			wait_block->shared_owners = 0;

			new_value = ((ULONG_PTR)wait_block) | PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_WAITERS;
		}
	}

	*current_value_ptr = new_value;
	*is_optimize_ptr = is_optimize;

	new_value = (ULONG_PTR)InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

	*new_value_ptr = new_value;

	return (new_value == value);
}

FORCEINLINE VOID _r_queuedlock_optimizelist_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_ignoreowned
)
{
	PR_QUEUED_WAIT_BLOCK wait_block;
	PR_QUEUED_WAIT_BLOCK first_wait_block;
	PR_QUEUED_WAIT_BLOCK last_wait_block;
	PR_QUEUED_WAIT_BLOCK previous_wait_block;
	ULONG_PTR current_value;
	ULONG_PTR new_value;

	current_value = value;

	while (TRUE)
	{
		assert (value & PR_QUEUED_LOCK_TRAVERSING);

		if (!is_ignoreowned && !(current_value & PR_QUEUED_LOCK_OWNED))
		{
			// Someone has requested that we wake waiters.
			_r_queuedlock_wake (queued_lock, current_value);
			break;
		}

		// Perform the optimization.
		wait_block = _r_queuedlock_getwaitblock (current_value);
		first_wait_block = wait_block;

		while (TRUE)
		{
			last_wait_block = wait_block->last_block;

			if (last_wait_block)
			{
				// Save a pointer to the last wait block in the first wait block and stop
				// optimizing.
				//
				// We don't need to continue setting Previous pointers because the last optimization
				// run would have set them already.

				first_wait_block->last_block = last_wait_block;
				break;
			}

			previous_wait_block = wait_block;
			wait_block = wait_block->next_block;
			wait_block->previous_block = previous_wait_block;
		}

		// Try to clear the traversing bit.
		new_value = current_value - PR_QUEUED_LOCK_TRAVERSING;
		new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
			(PVOID_PTR)&queued_lock->value,
			(PVOID)new_value,
			(PVOID)current_value
		);

		if (new_value == current_value)
			break;

		// Either someone pushed a wait block onto the list or someone released ownership. In either
		// case we need to go back.
		current_value = new_value;
	}
}

HANDLE _r_queuedlock_getevent ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE hevent = NULL;

	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = NtCreateKeyedEvent (&hevent, KEYEDEVENT_ALL_ACCESS, NULL, 0);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		_r_initonce_end (&init_once);
	}

	return hevent;
}

FORCEINLINE ULONG _r_queuedlock_getspincount ()
{
	if (NtCurrentPeb ()->NumberOfProcessors > 1)
		return 4000;

	return 0;
}

FORCEINLINE PR_QUEUED_WAIT_BLOCK _r_queuedlock_preparetowake (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR current_value,
	_In_ BOOLEAN is_ignoreowned,
	_In_ BOOLEAN is_wakeall
)
{
	PR_QUEUED_WAIT_BLOCK wait_block;
	PR_QUEUED_WAIT_BLOCK first_wait_block;
	PR_QUEUED_WAIT_BLOCK last_wait_block;
	PR_QUEUED_WAIT_BLOCK previous_wait_block;
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = current_value;

	while (TRUE)
	{
		// If there are multiple shared owners, no one is going to wake waiters since the lock would
		// still be owned. Also if there are multiple shared owners they may be traversing the list.
		// While that is safe when done concurrently with list optimization, we may be removing and
		// waking waiters.
		assert (!(value & PR_QUEUED_LOCK_MULTIPLE_SHARED));
		assert (is_ignoreowned || (value & PR_QUEUED_LOCK_TRAVERSING));

		// There's no point in waking a waiter if the lock is owned. Clear the traversing bit.
		while (!is_ignoreowned && (value & PR_QUEUED_LOCK_OWNED))
		{
			new_value = value - PR_QUEUED_LOCK_TRAVERSING;

			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
				return NULL;

			value = new_value;
		}

		// Finish up any needed optimization (setting the Previous pointers) while finding the last
		// wait block.
		wait_block = _r_queuedlock_getwaitblock (value);
		first_wait_block = wait_block;

		while (TRUE)
		{
			last_wait_block = wait_block->last_block;

			if (last_wait_block)
			{
				wait_block = last_wait_block;
				break;
			}

			previous_wait_block = wait_block;
			wait_block = wait_block->next_block;
			wait_block->previous_block = previous_wait_block;
		}

		// Unlink the relevant wait blocks and clear the traversing bit before we wake waiters.
		if (!is_wakeall &&
			(wait_block->flags & PR_QUEUED_WAITER_EXCLUSIVE) &&
			(previous_wait_block = wait_block->previous_block))
		{
			// We have an exclusive waiter and there are multiple waiters. We'll only be waking this
			// waiter.

			// Unlink the wait block from the list. Although other wait blocks may have their Last
			// pointers set to this wait block, the algorithm to find the last wait block will stop
			// here. Likewise the Next pointers are never followed beyond this point, so we don't
			// need to clear those.
			first_wait_block->last_block = previous_wait_block;

			// Make sure we only wake this waiter.
			wait_block->previous_block = NULL;

			if (!is_ignoreowned)
			{
				// Clear the traversing bit.
				InterlockedExchangeAddPointer ((PLONG_PTR)&queued_lock->value, -(LONG_PTR)PR_QUEUED_LOCK_TRAVERSING);
			}

			break;
		}
		else
		{
			// We're waking an exclusive waiter and there is only one waiter, or we are waking a
			// shared waiter and possibly others.
			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				IntToPtr (0),
				(PVOID)value
			);

			if (new_value == value)
				break;

			// Someone changed the lock (acquired it or pushed a wait block).
			value = new_value;
		}
	}

	return wait_block;
}

FORCEINLINE PR_QUEUED_WAIT_BLOCK _r_queuedlock_findlastwaitblock (
	_In_ ULONG_PTR value
)
{
	PR_QUEUED_WAIT_BLOCK wait_block;
	PR_QUEUED_WAIT_BLOCK last_wait_block;

	wait_block = _r_queuedlock_getwaitblock (value);

	// Traverse the list until we find the last wait block.
	// The Last pointer should be set by list optimization, allowing us to skip all, if not most of
	// the wait blocks.
	while (TRUE)
	{
		last_wait_block = wait_block->last_block;

		if (last_wait_block)
		{
			// Follow the Last pointer. This can mean two things: the pointer was set by list
			// optimization, or this wait block is actually the last wait block (set when it was
			// pushed onto the list).
			wait_block = last_wait_block;
			break;
		}

		wait_block = wait_block->next_block;
	}

	return wait_block;
}

FORCEINLINE NTSTATUS _r_queuedlock_blockwaitblock (
	_Inout_ PR_QUEUED_WAIT_BLOCK wait_block,
	_In_ BOOLEAN is_spin,
	_In_opt_ PLARGE_INTEGER timeout
)
{
	HANDLE hevent;
	NTSTATUS status;

	if (is_spin)
	{
		for (ULONG i = _r_queuedlock_getspincount (); i != 0; i--)
		{
			if (!(*(volatile ULONG *)&wait_block->flags & PR_QUEUED_WAITER_SPINNING))
				return STATUS_SUCCESS;

			YieldProcessor ();
		}
	}

	if (_interlockedbittestandreset ((PLONG)&wait_block->flags, PR_QUEUED_WAITER_SPINNING_SHIFT))
	{
		hevent = _r_queuedlock_getevent ();

		status = NtWaitForKeyedEvent (hevent, wait_block, FALSE, timeout);

		// If an error occurred (timeout is not an error), raise an exception as it is nearly
		// impossible to recover from this situation.
		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);
	}
	else
	{
		status = STATUS_SUCCESS;
	}

	return status;
}

FORCEINLINE VOID _r_queuedlock_unblockwaitblock (
	_Inout_ PR_QUEUED_WAIT_BLOCK wait_block
)
{
	HANDLE hevent;
	NTSTATUS status;

	if (!_interlockedbittestandreset ((PLONG)&wait_block->flags, PR_QUEUED_WAITER_SPINNING_SHIFT))
	{
		hevent = _r_queuedlock_getevent ();

		status = NtReleaseKeyedEvent (hevent, wait_block, FALSE, NULL);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);
	}
}

VOID FASTCALL _r_queuedlock_acquireexclusive_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	R_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR value;
	ULONG_PTR new_value;
	ULONG_PTR current_value;
	BOOLEAN is_optimize;

	value = queued_lock->value;

	while (TRUE)
	{
		if (!(value & PR_QUEUED_LOCK_OWNED))
		{
			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)(value + PR_QUEUED_LOCK_OWNED),
				(PVOID)value
			);

			if (new_value == value)
				break;
		}
		else
		{
			if (_r_queuedlock_pushwaitblock (
				queued_lock,
				value,
				TRUE,
				&wait_block,
				&is_optimize,
				&new_value,
				&current_value))
			{
				if (is_optimize)
					_r_queuedlock_optimizelist (queued_lock, current_value);

				_r_queuedlock_blockwaitblock (&wait_block, TRUE, NULL);
			}
		}

		value = new_value;
	}
}

VOID FASTCALL _r_queuedlock_acquireshared_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	R_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR value;
	ULONG_PTR new_value;
	ULONG_PTR current_value;
	BOOLEAN is_optimize;

	value = queued_lock->value;

	while (TRUE)
	{
		// We can't acquire if there are waiters for two reasons:
		//
		// We want to prioritize exclusive acquires over shared acquires. There's currently no fast,
		// safe way of finding the last wait block and incrementing the shared owners count here.
		if (!(value & PR_QUEUED_LOCK_WAITERS) &&
			(!(value & PR_QUEUED_LOCK_OWNED) || (_r_queuedlock_getsharedowners (value) > 0)))
		{
			new_value = (value + PR_QUEUED_LOCK_SHARED_INC) | PR_QUEUED_LOCK_OWNED;

			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
				break;
		}
		else
		{
			if (_r_queuedlock_pushwaitblock (
				queued_lock,
				value,
				FALSE,
				&wait_block,
				&is_optimize,
				&new_value,
				&current_value))
			{
				if (is_optimize)
					_r_queuedlock_optimizelist (queued_lock, current_value);

				_r_queuedlock_blockwaitblock (&wait_block, TRUE, NULL);
			}
		}

		value = new_value;
	}
}

VOID FASTCALL _r_queuedlock_releaseexclusive_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;
	ULONG_PTR current_value;

	value = queued_lock->value;

	while (TRUE)
	{
		assert (value & PR_QUEUED_LOCK_OWNED);
		assert ((value & PR_QUEUED_LOCK_WAITERS) || (_r_queuedlock_getsharedowners (value) == 0));

		if ((value & (PR_QUEUED_LOCK_WAITERS | PR_QUEUED_LOCK_TRAVERSING)) != PR_QUEUED_LOCK_WAITERS)
		{
			// There are no waiters or someone is traversing the list.
			//
			// If there are no waiters, we're simply releasing ownership. If someone is traversing
			// the list, clearing the owned bit is a signal for them to wake waiters.
			new_value = value - PR_QUEUED_LOCK_OWNED;
			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
				break;
		}
		else
		{
			// We need to wake waiters and no one is traversing the list.
			// Try to set the traversing bit and wake waiters.
			new_value = (value - PR_QUEUED_LOCK_OWNED + PR_QUEUED_LOCK_TRAVERSING);
			current_value = new_value;

			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
			{
				_r_queuedlock_wake (queued_lock, current_value);
				break;
			}
		}

		value = new_value;
	}
}

VOID FASTCALL _r_queuedlock_releaseshared_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;
	ULONG_PTR current_value;
	PR_QUEUED_WAIT_BLOCK wait_lock;

	value = queued_lock->value;

	while (!(value & PR_QUEUED_LOCK_WAITERS))
	{
		assert (value & PR_QUEUED_LOCK_OWNED);
		assert ((value & PR_QUEUED_LOCK_WAITERS) || (_r_queuedlock_getsharedowners (value) > 0));

		if (_r_queuedlock_getsharedowners (value) > 1)
		{
			new_value = value - PR_QUEUED_LOCK_SHARED_INC;
		}
		else
		{
			new_value = 0;
		}

		new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
			(PVOID_PTR)&queued_lock->value,
			(PVOID)new_value,
			(PVOID)value
		);

		if (new_value == value)
			return;

		value = new_value;
	}

	if (value & PR_QUEUED_LOCK_MULTIPLE_SHARED)
	{
		// Unfortunately we have to find the last wait block and decrement the shared owners count.
		wait_lock = _r_queuedlock_findlastwaitblock (value);

		if ((ULONG)InterlockedDecrement ((PLONG)&wait_lock->shared_owners) > 0)
			return;
	}

	while (TRUE)
	{
		if (value & PR_QUEUED_LOCK_TRAVERSING)
		{
			new_value = value & ~(PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_MULTIPLE_SHARED);

			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
				break;
		}
		else
		{
			new_value = (value & ~(PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_MULTIPLE_SHARED)) | PR_QUEUED_LOCK_TRAVERSING;
			current_value = new_value;

			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&queued_lock->value,
				(PVOID)new_value,
				(PVOID)value
			);

			if (new_value == value)
			{
				_r_queuedlock_wake (queued_lock, current_value);
				break;
			}
		}

		value = new_value;
	}
}

VOID FASTCALL _r_queuedlock_optimizelist (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
)
{
	_r_queuedlock_optimizelist_ex (queued_lock, value, FALSE);
}

VOID FASTCALL _r_queuedlock_wake (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
)
{
	PR_QUEUED_WAIT_BLOCK wait_block;
	PR_QUEUED_WAIT_BLOCK previous_wait_block;

	wait_block = _r_queuedlock_preparetowake (queued_lock, value, FALSE, FALSE);

	// Wake waiters.
	while (wait_block)
	{
		previous_wait_block = wait_block->previous_block;
		_r_queuedlock_unblockwaitblock (wait_block);
		wait_block = previous_wait_block;
	};
}

VOID FASTCALL _r_queuedlock_wake_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_ignoreowned,
	_In_ BOOLEAN is_wakeall
)
{
	PR_QUEUED_WAIT_BLOCK wait_block;
	PR_QUEUED_WAIT_BLOCK previous_wait_block;

	wait_block = _r_queuedlock_preparetowake (queued_lock, value, is_ignoreowned, is_wakeall);

	// Wake waiters.
	while (wait_block)
	{
		previous_wait_block = wait_block->previous_block;
		_r_queuedlock_unblockwaitblock (wait_block);
		wait_block = previous_wait_block;
	}
}

VOID FASTCALL _r_queuedlock_wakeforrelease (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
)
{
	ULONG_PTR new_value;
	ULONG_PTR current_value;

	new_value = value + PR_QUEUED_LOCK_TRAVERSING;

	current_value = (ULONG_PTR)InterlockedCompareExchangePointer (
		(PVOID_PTR)&queued_lock->value,
		(PVOID)new_value,
		(PVOID)value
	);

	if (current_value == value)
		_r_queuedlock_wake (queued_lock, new_value);
}

//
// Synchronization: Condition
//

VOID FASTCALL _r_condition_pulse (
	_Inout_ PR_CONDITION condition
)
{
	if (condition->value & PR_QUEUED_LOCK_WAITERS)
		_r_queuedlock_wake_ex (condition, condition->value, TRUE, FALSE);
}

VOID FASTCALL _r_condition_waitfor (
	_Inout_ PR_CONDITION condition,
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	R_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR value;
	ULONG_PTR current_value;
	BOOLEAN is_optimize;

	value = condition->value;

	while (TRUE)
	{
		if (_r_queuedlock_pushwaitblock (condition, value, TRUE, &wait_block, &is_optimize, &value, &current_value))
		{
			if (is_optimize)
				_r_queuedlock_optimizelist_ex (condition, current_value, TRUE);

			_r_queuedlock_releaseexclusive (queued_lock);

			_r_queuedlock_blockwaitblock (&wait_block, FALSE, NULL);

			// Don't use the inline variant; it is extremely likely that the lock is still owned.
			_r_queuedlock_acquireexclusive_ex (queued_lock);

			break;
		}
	}
}

//
// Synchronization: Rundown protection
//

BOOLEAN FASTCALL _r_protection_acquire_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	// Increment the reference count only if rundown has not started.
	while (TRUE)
	{
		value = protection->value;

		if (value & PR_RUNDOWN_ACTIVE)
			return FALSE;

		new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
			(PVOID_PTR)&protection->value,
			(PVOID)(value + PR_RUNDOWN_REF_INC),
			(PVOID)value
		);

		if (new_value == value)
			return TRUE;
	}
}

VOID FASTCALL _r_protection_release_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;
	PR_RUNDOWN_WAIT_BLOCK wait_block;

	while (TRUE)
	{
		value = protection->value;

		if (value & PR_RUNDOWN_ACTIVE)
		{
			// Since rundown is active, the reference count has been moved to the waiter's wait
			// block. If we are the last user, we must wake up the waiter.
			wait_block = (PR_RUNDOWN_WAIT_BLOCK)(value & ~PR_RUNDOWN_ACTIVE);

			if (InterlockedDecrementPointer (&wait_block->count) == 0)
				_r_event_set (&wait_block->wake_event);

			break;
		}
		else
		{
			// Decrement the reference count normally.
			new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
				(PVOID_PTR)&protection->value,
				(PVOID)(value - PR_RUNDOWN_REF_INC),
				(PVOID)value
			);

			if (new_value == value)
				break;
		}
	}
}

VOID FASTCALL _r_protection_waitfor_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	R_RUNDOWN_WAIT_BLOCK wait_block = {0};
	ULONG_PTR value;
	ULONG_PTR new_value;
	ULONG_PTR count;

	// Fast path. If the reference count is 0 or rundown has already been completed, return.
	value = (ULONG_PTR)InterlockedCompareExchangePointer (
		(PVOID_PTR)&protection->value,
		IntToPtr (PR_RUNDOWN_ACTIVE),
		IntToPtr (0)
	);

	if (value == 0 || value == PR_RUNDOWN_ACTIVE)
		return;

	// Initialize the wait block.
	_r_event_intialize (&wait_block.wake_event);

	while (TRUE)
	{
		value = protection->value;
		count = value >> PR_RUNDOWN_REF_SHIFT;

		// Save the existing reference count.
		wait_block.count = count;

		new_value = (ULONG_PTR)InterlockedCompareExchangePointer (
			(PVOID_PTR)&protection->value,
			(PVOID)((ULONG_PTR)&wait_block | PR_RUNDOWN_ACTIVE),
			(PVOID)value
		);

		if (new_value == value)
		{
			if (count != 0)
				_r_event_wait (&wait_block.wake_event, NULL);

			break;
		}
	}
}

//
// Synchronization: Workqueue
//

PR_FREE_LIST _r_workqueue_getfreelist ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_FREE_LIST free_list = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_freelist_initialize (&free_list, sizeof (R_WORKQUEUE_ITEM), 32);

		_r_initonce_end (&init_once);
	}

	return &free_list;
}

VOID _r_workqueue_initialize (
	_Out_ PR_WORKQUEUE work_queue,
	_In_ ULONG minimum_threads,
	_In_ ULONG maximum_threads,
	_In_ ULONG no_work_timeout,
	_In_opt_ PR_ENVIRONMENT environment,
	_In_opt_ LPCWSTR thread_name
)
{
	InitializeListHead (&work_queue->queue_list_head);

	_r_queuedlock_initialize (&work_queue->queue_lock);
	_r_queuedlock_initialize (&work_queue->state_lock);

	_r_protection_initialize (&work_queue->rundown_protect);
	_r_condition_initialize (&work_queue->queue_empty_condition);

	work_queue->minimum_threads = minimum_threads;
	work_queue->maximum_threads = maximum_threads;
	work_queue->no_work_timeout = no_work_timeout;

	if (environment)
	{
		work_queue->environment = *environment;
	}
	else
	{
		_r_sys_setenvironment (
			&work_queue->environment,
			THREAD_PRIORITY_NORMAL,
			IoPriorityNormal,
			MEMORY_PRIORITY_NORMAL
		);
	}

	work_queue->thread_name = thread_name ? _r_obj_createstring (thread_name) : NULL;

	NtCreateSemaphore (&work_queue->semaphore_handle, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);

	work_queue->current_threads = 0;
	work_queue->busy_count = 0;

	work_queue->is_terminating = FALSE;
}

VOID _r_workqueue_destroy (
	_Inout_ PR_WORKQUEUE work_queue
)
{
	PR_WORKQUEUE_ITEM work_queue_item;
	PLIST_ENTRY list_entry;

	// Wait for all worker threads to exit.
	work_queue->is_terminating = TRUE;
	MemoryBarrier ();

	NtReleaseSemaphore (work_queue->semaphore_handle, work_queue->current_threads, NULL);

	_r_protection_waitfor (&work_queue->rundown_protect);

	// Free all un-executed work items.
	list_entry = work_queue->queue_list_head.Flink;

	while (list_entry != &work_queue->queue_list_head)
	{
		work_queue_item = CONTAINING_RECORD (list_entry, R_WORKQUEUE_ITEM, list_entry);
		list_entry = list_entry->Flink;

		_r_workqueue_destroyitem (work_queue_item);
	}

	NtClose (work_queue->semaphore_handle);
	work_queue->semaphore_handle = NULL;
}

PR_WORKQUEUE_ITEM _r_workqueue_createitem (
	_In_ PR_WORKQUEUE_FUNCTION base_address,
	_In_opt_ PVOID context
)
{
	PR_WORKQUEUE_ITEM work_queue_item;
	PR_FREE_LIST free_list;

	free_list = _r_workqueue_getfreelist ();

	work_queue_item = _r_freelist_allocateitem (free_list);

	work_queue_item->base_address = base_address;
	work_queue_item->context = context;

	return work_queue_item;
}

VOID _r_workqueue_destroyitem (
	_In_ PR_WORKQUEUE_ITEM work_queue_item
)
{
	PR_FREE_LIST free_list;

	free_list = _r_workqueue_getfreelist ();

	_r_freelist_deleteitem (free_list, work_queue_item);
}

NTSTATUS _r_workqueue_threadproc (
	_In_ PVOID arglist
)
{
	PR_WORKQUEUE work_queue;
	PR_WORKQUEUE_ITEM work_queue_item;
	PLIST_ENTRY list_entry;
	LARGE_INTEGER timeout;
	BOOLEAN is_terminate;
	NTSTATUS status;

	work_queue = (PR_WORKQUEUE)arglist;

	while (TRUE)
	{
		// Check if we have more threads than the limit.
		if (work_queue->current_threads > work_queue->maximum_threads)
		{
			is_terminate = FALSE;

			// Lock and re-check.
			_r_queuedlock_acquireexclusive (&work_queue->state_lock);

			// Check the minimum as well.
			if (work_queue->current_threads > work_queue->minimum_threads)
			{
				work_queue->current_threads -= 1;
				is_terminate = TRUE;
			}

			_r_queuedlock_releaseexclusive (&work_queue->state_lock);

			if (is_terminate)
				break;
		}

		if (!work_queue->is_terminating)
		{
			// Wait for work.
			_r_calc_millisecondstolargeinteger (&timeout, work_queue->no_work_timeout);

			status = NtWaitForSingleObject (work_queue->semaphore_handle, FALSE, &timeout);
		}
		else
		{
			status = STATUS_UNSUCCESSFUL;
		}

		if (status == STATUS_WAIT_0 && !work_queue->is_terminating)
		{
			// Dequeue the work item.
			_r_queuedlock_acquireexclusive (&work_queue->queue_lock);

			list_entry = RemoveHeadList (&work_queue->queue_list_head);

			if (IsListEmpty (&work_queue->queue_list_head))
				_r_condition_pulse (&work_queue->queue_empty_condition);

			_r_queuedlock_releaseexclusive (&work_queue->queue_lock);

			// Make sure we got work.
			if (list_entry != &work_queue->queue_list_head)
			{
				work_queue_item = CONTAINING_RECORD (list_entry, R_WORKQUEUE_ITEM, list_entry);

				work_queue_item->base_address (work_queue_item->context, work_queue->busy_count);

				InterlockedDecrement (&work_queue->busy_count);

				_r_workqueue_destroyitem (work_queue_item);
			}
		}
		else
		{
			is_terminate = FALSE;

			// No work arrived before the timeout passed, or we are terminating, or some error
			// occurred. Terminate the thread.
			_r_queuedlock_acquireexclusive (&work_queue->state_lock);

			if (work_queue->is_terminating || work_queue->current_threads > work_queue->minimum_threads)
			{
				work_queue->current_threads -= 1;
				is_terminate = TRUE;
			}

			_r_queuedlock_releaseexclusive (&work_queue->state_lock);

			if (is_terminate)
				break;
		}
	}

	_r_protection_release (&work_queue->rundown_protect);

	return STATUS_SUCCESS;
}

VOID _r_workqueue_queueitem (
	_Inout_ PR_WORKQUEUE work_queue,
	_In_ PR_WORKQUEUE_FUNCTION function_address,
	_In_opt_ PVOID context
)
{
	PR_WORKQUEUE_ITEM work_queue_item;
	NTSTATUS status;

	work_queue_item = _r_workqueue_createitem (function_address, context);

	// Enqueue the work item.
	_r_queuedlock_acquireexclusive (&work_queue->queue_lock);

	InsertTailList (&work_queue->queue_list_head, &work_queue_item->list_entry);
	InterlockedIncrement (&work_queue->busy_count);

	_r_queuedlock_releaseexclusive (&work_queue->queue_lock);

	// Signal the semaphore once to let a worker thread continue.
	NtReleaseSemaphore (work_queue->semaphore_handle, 1, NULL);

	// Check if all worker threads are currently busy, and if we can create more threads.
	if (work_queue->busy_count >= work_queue->current_threads && work_queue->current_threads < work_queue->maximum_threads)
	{
		// Lock and re-check.
		_r_queuedlock_acquireexclusive (&work_queue->state_lock);

		// Make sure the structure doesn't get deleted while the thread is running.
		if (_r_protection_acquire (&work_queue->rundown_protect))
		{
			status = _r_sys_createthread (
				&_r_workqueue_threadproc,
				work_queue,
				NULL,
				&work_queue->environment,
				_r_obj_getstring (work_queue->thread_name)
			);

			if (NT_SUCCESS (status))
			{
				work_queue->current_threads += 1;
			}
			else
			{
				_r_protection_release (&work_queue->rundown_protect);
			}
		}

		_r_queuedlock_releaseexclusive (&work_queue->state_lock);
	}
}

VOID _r_workqueue_waitforfinish (
	_Inout_ PR_WORKQUEUE work_queue
)
{
	_r_queuedlock_acquireexclusive (&work_queue->queue_lock);

	while (!IsListEmpty (&work_queue->queue_list_head))
	{
		_r_condition_waitfor (&work_queue->queue_empty_condition, &work_queue->queue_lock);
	}

	_r_queuedlock_releaseexclusive (&work_queue->queue_lock);
}

//
// Synchronization: Mutex
//

BOOLEAN _r_mutex_isexists (
	_In_ LPCWSTR name
)
{
	HANDLE hmutex;

	hmutex = OpenMutex (MUTANT_QUERY_STATE, FALSE, name);

	if (hmutex)
	{
		NtClose (hmutex);

		return TRUE;
	}

	return FALSE;
}

_When_ (return != FALSE, _Acquires_lock_ (*hmutex))
BOOLEAN _r_mutex_create (
	_In_ LPCWSTR name,
	_Out_ PHANDLE hmutex
)
{
	HANDLE original_mutex;

	original_mutex = CreateMutex (NULL, FALSE, name);

	*hmutex = original_mutex;

	if (original_mutex)
		return TRUE;

	return FALSE;
}

_When_ (return != FALSE, _Releases_lock_ (*hmutex))
BOOLEAN _r_mutex_destroy (
	_Inout_ PHANDLE hmutex
)
{
	HANDLE original_mutex;

	original_mutex = *hmutex;
	*hmutex = NULL;

	if (original_mutex)
	{
		ReleaseMutex (original_mutex);
		NtClose (original_mutex);

		return TRUE;
	}

	return FALSE;
}

//
// Memory allocation
//

HANDLE NTAPI _r_mem_getheap ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE heap_handle = NULL;

	if (_r_initonce_begin (&init_once))
	{
		// win10rs1 preview (14295)+
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			heap_handle = RtlCreateHeap (
				HEAP_GROWABLE | HEAP_CLASS_1 | HEAP_CREATE_SEGMENT_HEAP,
				NULL,
				0,
				0,
				NULL,
				NULL
			);
		}

		if (!heap_handle)
		{
			heap_handle = RtlCreateHeap (
				HEAP_GROWABLE | HEAP_CLASS_1,
				NULL,
				_r_calc_megabytes2bytes (2),
				_r_calc_megabytes2bytes (1),
				NULL,
				NULL
			);
		}

		if (heap_handle)
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
			{
				RtlSetHeapInformation (
					heap_handle,
					HeapCompatibilityInformation,
					&(ULONG){HEAP_COMPATIBILITY_LFH},
					sizeof (ULONG)
				);
			}
		}

		_r_initonce_end (&init_once);
	}

	return heap_handle;
}

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocate (
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID base_address;

	heap_handle = _r_mem_getheap ();

	base_address = RtlAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS, bytes_count);

	return base_address;
}

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocatezero (
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID base_address;

	heap_handle = _r_mem_getheap ();

	base_address = RtlAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);

	return base_address;
}

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocatezerosafe (
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID base_address;

	heap_handle = _r_mem_getheap ();

	base_address = RtlAllocateHeap (heap_handle, HEAP_ZERO_MEMORY, bytes_count);

	return base_address;
}

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocateandcopy (
	_In_ LPCVOID src,
	_In_ SIZE_T bytes_count
)
{
	PVOID base_address;

	base_address = _r_mem_allocatezero (bytes_count);

	RtlCopyMemory (base_address, src, bytes_count);

	return base_address;
}

// If RtlReAllocateHeap fails, the original memory is not freed, and the original handle and pointer are still valid.
// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocate (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID new_address;

	heap_handle = _r_mem_getheap ();

	new_address = RtlReAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS, base_address, bytes_count);

	return new_address;
}

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocatezero (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID new_address;

	heap_handle = _r_mem_getheap ();

	new_address = RtlReAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, base_address, bytes_count);

	return new_address;
}

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocatezerosafe (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
)
{
	HANDLE heap_handle;
	PVOID new_address;

	heap_handle = _r_mem_getheap ();

	new_address = RtlReAllocateHeap (heap_handle, HEAP_ZERO_MEMORY, base_address, bytes_count);

	return new_address;
}

VOID NTAPI _r_mem_free (
	_Frees_ptr_opt_ PVOID base_address
)
{
	HANDLE heap_handle;

	heap_handle = _r_mem_getheap ();

	RtlFreeHeap (heap_handle, 0, base_address);
}

BOOLEAN _r_mem_frobnicate (
	_Inout_ PR_BYTEREF bytes
)
{
	LPSTR buffer;
	SIZE_T length;
	BYTE chr;

	if (!bytes->length)
		return FALSE;

	buffer = bytes->buffer;
	length = _r_str_getbytelength3 (bytes);

	do
	{
		chr = *buffer;

		if (chr != 0 && chr != 42)
			*buffer = chr ^ 42;

		buffer += 1;
	}
	while (--length);

	return TRUE;
}

//
// Objects reference
//

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_obj_allocate (
	_In_ SIZE_T bytes_count,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_OBJECT_HEADER object_header;

	object_header = _r_mem_allocatezero (UFIELD_OFFSET (R_OBJECT_HEADER, body) + bytes_count);

	InterlockedIncrement (&object_header->ref_count);

	object_header->cleanup_callback = cleanup_callback;

	return PR_OBJECT_HEADER_TO_OBJECT (object_header);
}

VOID NTAPI _r_obj_dereference (
	_In_ PVOID object_body
)
{
	_r_obj_dereference_ex (object_body, 1);
}

VOID NTAPI _r_obj_dereferencelist (
	_In_reads_ (objects_count) PVOID_PTR objects_list,
	_In_ SIZE_T objects_count
)
{
	for (SIZE_T i = 0; i < objects_count; i++)
	{
		_r_obj_dereference (objects_list[i]);
	}
}

VOID NTAPI _r_obj_dereference_ex (
	_In_ PVOID object_body,
	_In_ LONG ref_count
)
{
	PR_OBJECT_HEADER object_header;

	LONG old_count;
	LONG new_count;

	assert (!(ref_count < 0));

	object_header = PR_OBJECT_TO_OBJECT_HEADER (object_body);

	old_count = InterlockedExchangeAdd (&object_header->ref_count, -ref_count);

	new_count = old_count - ref_count;

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

PVOID NTAPI _r_obj_reference (
	_In_ PVOID object_body
)
{
	PR_OBJECT_HEADER object_header;

	object_header = PR_OBJECT_TO_OBJECT_HEADER (object_body);

	InterlockedIncrement (&object_header->ref_count);

	return object_body;
}

_Ret_maybenull_
PVOID NTAPI _r_obj_referencesafe (
	_In_opt_ PVOID object_body
)
{
	if (!object_body)
		return NULL;

	return _r_obj_reference (object_body);
}

//
// 8-bit string object
//

PR_BYTE _r_obj_createbyte (
	_In_ LPSTR string
)
{
	return _r_obj_createbyte_ex (string, _r_str_getbytelength (string));
}

PR_BYTE _r_obj_createbyte2 (
	_In_ PR_BYTE string
)
{
	return _r_obj_createbyte_ex (string->buffer, string->length);
}

PR_BYTE _r_obj_createbyte3 (
	_In_ PR_BYTEREF string
)
{
	return _r_obj_createbyte_ex (string->buffer, string->length);
}

PR_BYTE _r_obj_createbyte_ex (
	_In_opt_ LPCSTR buffer,
	_In_ SIZE_T length
)
{
	PR_BYTE bytes;

	if (!length)
		length = sizeof (ANSI_NULL);

	bytes = _r_obj_allocate (UFIELD_OFFSET (R_BYTE, data) + length + sizeof (ANSI_NULL), NULL);

	bytes->length = length;
	bytes->buffer = bytes->data;

	if (buffer)
	{
		RtlCopyMemory (bytes->buffer, buffer, length);
		_r_obj_writebytenullterminator (bytes);
	}
	else
	{
		bytes->buffer[0] = ANSI_NULL;
	}

	return bytes;
}

BOOLEAN _r_obj_isbytenullterminated (
	_In_ PR_BYTEREF string
)
{
	return (string->buffer[string->length] == ANSI_NULL);
}

VOID _r_obj_setbytelength (
	_Inout_ PR_BYTE string,
	_In_ SIZE_T new_length
)
{
	_r_obj_setbytelength_ex (string, new_length, string->length);
}

VOID _r_obj_setbytelength_ex (
	_Inout_ PR_BYTE string,
	_In_ SIZE_T new_length,
	_In_ SIZE_T allocated_length
)
{
	if (allocated_length < new_length)
		new_length = allocated_length;

	string->length = new_length;

	_r_obj_writebytenullterminator (string); // terminate
}

VOID _r_obj_skipbytelength (
	_Inout_ PR_BYTEREF string,
	_In_ SIZE_T length
)
{
	string->buffer = (LPSTR)PTR_ADD_OFFSET (string->buffer, length);
	string->length -= length;
}

VOID _r_obj_trimbytetonullterminator (
	_In_ PR_BYTE string
)
{
	string->length = _r_str_getbytelength_ex (string->buffer, string->length + 1);

	_r_obj_writebytenullterminator (string); // terminate
}

VOID _r_obj_writebytenullterminator (
	_In_ PR_BYTE string
)
{
	*(LPSTR)PTR_ADD_OFFSET (string->buffer, string->length) = ANSI_NULL;
}

//
// 16-bit string object
//

PR_STRING _r_obj_createstring (
	_In_ LPCWSTR string
)
{
	return _r_obj_createstring_ex (string, _r_str_getlength (string) * sizeof (WCHAR));
}

PR_STRING _r_obj_createstring2 (
	_In_ PR_STRING string
)
{
	return _r_obj_createstring_ex (string->buffer, string->length);
}

PR_STRING _r_obj_createstring3 (
	_In_ PR_STRINGREF string
)
{
	return _r_obj_createstring_ex (string->buffer, string->length);
}

PR_STRING _r_obj_createstring4 (
	_In_ PUNICODE_STRING string
)
{
	return _r_obj_createstring_ex (string->Buffer, string->Length);
}

PR_STRING _r_obj_createstring_ex (
	_In_opt_ LPCWSTR buffer,
	_In_ SIZE_T length
)
{
	PR_STRING string;

	assert (!(length & 0x01));

	if (!length)
		length = sizeof (UNICODE_NULL);

	string = _r_obj_allocate (UFIELD_OFFSET (R_STRING, data) + length + sizeof (UNICODE_NULL), NULL);

	string->length = length;
	string->buffer = string->data;

	if (buffer)
	{
		RtlCopyMemory (string->buffer, buffer, length);
		_r_obj_writestringnullterminator (string);
	}
	else
	{
		string->buffer[0] = UNICODE_NULL;
	}

	return string;
}

PR_STRING _r_obj_concatstrings (
	_In_ SIZE_T count,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, count);
	string = _r_obj_concatstrings_v (count, arg_ptr);
	va_end (arg_ptr);

	return string;
}

PR_STRING _r_obj_concatstrings_v (
	_In_ SIZE_T count,
	_In_ va_list arg_ptr
)
{
	va_list argptr;
	PR_STRING string;
	LPCWSTR arg;
	SIZE_T cached_length[PR_SIZE_CONCAT_LENGTH_CACHE] = {0};
	SIZE_T total_length;
	SIZE_T string_length;
	SIZE_T i;

	argptr = arg_ptr;
	total_length = 0;

	for (i = 0; i < count; i++)
	{
		arg = va_arg (argptr, LPCWSTR);

		if (!arg)
			continue;

		string_length = _r_str_getlength (arg) * sizeof (WCHAR);
		total_length += string_length;

		if (i < PR_SIZE_CONCAT_LENGTH_CACHE)
			cached_length[i] = string_length;
	}

	string = _r_obj_createstring_ex (NULL, total_length);

	argptr = arg_ptr;
	total_length = 0;

	for (i = 0; i < count; i++)
	{
		arg = va_arg (argptr, LPCWSTR);

		if (!arg)
			continue;

		if (i < PR_SIZE_CONCAT_LENGTH_CACHE)
		{
			string_length = cached_length[i];
		}
		else
		{
			string_length = _r_str_getlength (arg) * sizeof (WCHAR);
		}

		RtlCopyMemory (
			PTR_ADD_OFFSET (string->buffer, total_length),
			arg,
			string_length
		);

		total_length += string_length;
	}

	return string;
}

PR_STRING _r_obj_concatstringrefs (
	_In_ SIZE_T count,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, count);
	string = _r_obj_concatstringrefs_v (count, arg_ptr);
	va_end (arg_ptr);

	return string;
}

PR_STRING _r_obj_concatstringrefs_v (
	_In_ SIZE_T count,
	_In_ va_list arg_ptr
)
{
	va_list argptr;
	PR_STRINGREF arg;
	PR_STRING string;
	SIZE_T total_length;
	SIZE_T i;

	argptr = arg_ptr;
	total_length = 0;

	for (i = 0; i < count; i++)
	{
		arg = va_arg (argptr, PR_STRINGREF);

		if (!arg || !arg->length)
			continue;

		total_length += arg->length;
	}

	string = _r_obj_createstring_ex (NULL, total_length);

	argptr = arg_ptr;
	total_length = 0;

	for (i = 0; i < count; i++)
	{
		arg = va_arg (argptr, PR_STRINGREF);

		if (!arg || !arg->length)
			continue;

		RtlCopyMemory (
			PTR_ADD_OFFSET (string->buffer, total_length),
			arg->buffer,
			arg->length
		);

		total_length += arg->length;
	}

	return string;
}

BOOLEAN _r_obj_isstringnullterminated (
	_In_ PR_STRINGREF string
)
{
	SIZE_T length;

	length = _r_str_getlength3 (string);

	return (string->buffer[length] == UNICODE_NULL);
}

PR_STRING _r_obj_referenceemptystring ()
{
	static PR_STRING cached_string = NULL;

	PR_STRING current_string;
	PR_STRING new_string;

	current_string = InterlockedCompareExchangePointer (&cached_string, NULL, NULL);

	if (!current_string)
	{
		new_string = _r_obj_createstring_ex (NULL, 0);

		current_string = InterlockedCompareExchangePointer (&cached_string, new_string, NULL);

		if (!current_string)
		{
			current_string = new_string;
		}
		else
		{
			_r_obj_dereference (new_string);
		}
	}

	return _r_obj_reference (current_string);
}

VOID _r_obj_removestring (
	_In_ PR_STRING string,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T length
)
{
	RtlMoveMemory (
		&string->buffer[start_pos],
		&string->buffer[start_pos + length],
		string->length - (length + start_pos) * sizeof (WCHAR)
	);

	string->length -= (length * sizeof (WCHAR));

	_r_obj_writestringnullterminator (string);
}

VOID _r_obj_setstringlength (
	_Inout_ PR_STRING string,
	_In_ SIZE_T new_length
)
{
	_r_obj_setstringlength_ex (string, new_length, string->length);
}

VOID _r_obj_setstringlength_ex (
	_Inout_ PR_STRING string,
	_In_ SIZE_T new_length,
	_In_ SIZE_T allocated_length
)
{
	if (allocated_length < new_length)
		new_length = allocated_length;

	if (new_length & 0x01)
		new_length += 1;

	string->length = new_length;

	_r_obj_writestringnullterminator (string); // terminate
}

VOID _r_obj_skipstringlength (
	_Inout_ PR_STRINGREF string,
	_In_ SIZE_T length
)
{
	assert (!(length & 0x01));

	string->buffer = (LPWSTR)PTR_ADD_OFFSET (string->buffer, length);
	string->length -= length;
}

VOID _r_obj_trimstringtonullterminator (
	_In_ PR_STRING string
)
{
	string->length = _r_str_getlength_ex (string->buffer, _r_str_getlength2 (string) + 1) * sizeof (WCHAR);

	_r_obj_writestringnullterminator (string); // terminate
}

VOID _r_obj_writestringnullterminator (
	_In_ PR_STRING string
)
{
	assert (!(string->length & 0x01));

	*(LPWSTR)PTR_ADD_OFFSET (string->buffer, string->length) = UNICODE_NULL;
}

//
// 8-bit string reference object
//

VOID _r_obj_initializebyterefempty (
	_Out_ PR_BYTEREF string
)
{
	_r_obj_initializebyteref_ex (string, NULL, 0);
}

VOID _r_obj_initializebyterefconst (
	_Out_ PR_BYTEREF string,
	_In_ LPCSTR buffer
)
{
	_r_obj_initializebyteref_ex (string, (LPSTR)buffer, _r_str_getbytelength (buffer));
}

VOID _r_obj_initializebyteref (
	_Out_ PR_BYTEREF string,
	_In_ LPSTR buffer
)
{
	_r_obj_initializebyteref_ex (string, buffer, _r_str_getbytelength (buffer));
}

VOID _r_obj_initializebyteref2 (
	_Out_ PR_BYTEREF string,
	_In_ PR_BYTE buffer
)
{
	_r_obj_initializebyteref_ex (string, buffer->buffer, buffer->length);
}

VOID _r_obj_initializebyteref3 (
	_Out_ PR_BYTEREF string,
	_In_ PR_BYTEREF buffer
)
{
	_r_obj_initializebyteref_ex (string, buffer->buffer, buffer->length);
}

VOID _r_obj_initializebyteref_ex (
	_Out_ PR_BYTEREF string,
	_In_opt_ LPSTR buffer,
	_In_opt_ SIZE_T length
)
{
	string->buffer = buffer;
	string->length = length;
}

//
// 16-bit string reference object
//

VOID _r_obj_initializestringrefempty (
	_Out_ PR_STRINGREF string
)
{
	_r_obj_initializestringref_ex (string, NULL, 0);
}

VOID _r_obj_initializestringrefconst (
	_Out_ PR_STRINGREF string,
	_In_ LPCWSTR buffer
)
{
	_r_obj_initializestringref_ex (string, (LPWSTR)buffer, _r_str_getlength (buffer) * sizeof (WCHAR));
}

VOID _r_obj_initializestringref (
	_Out_ PR_STRINGREF string,
	_In_ LPWSTR buffer
)
{
	_r_obj_initializestringref_ex (string, buffer, _r_str_getlength (buffer) * sizeof (WCHAR));
}

VOID _r_obj_initializestringref2 (
	_Out_ PR_STRINGREF string,
	_In_ PR_STRING buffer
)
{
	_r_obj_initializestringref_ex (string, buffer->buffer, buffer->length);
}

VOID _r_obj_initializestringref3 (
	_Out_ PR_STRINGREF string,
	_In_ PR_STRINGREF buffer
)
{
	_r_obj_initializestringref_ex (string, buffer->buffer, buffer->length);
}

VOID _r_obj_initializestringref4 (
	_Out_ PR_STRINGREF string,
	_In_ PUNICODE_STRING buffer
)
{
	_r_obj_initializestringref_ex (string, buffer->Buffer, buffer->Length);
}

VOID _r_obj_initializestringref_ex (
	_Out_ PR_STRINGREF string,
	_In_opt_ LPWSTR buffer,
	_In_opt_ SIZE_T length
)
{
	assert (!(length & 0x01));

	string->buffer = buffer;
	string->length = length;
}

//
// Unicode string object
//

BOOLEAN _r_obj_initializeunicodestring_ex (
	_Out_ PUNICODE_STRING string,
	_In_opt_ LPWSTR buffer,
	_In_opt_ USHORT length,
	_In_opt_ USHORT max_length
)
{
	string->Buffer = buffer;
	string->Length = length;
	string->MaximumLength = max_length;

	return string->Length <= UNICODE_STRING_MAX_BYTES;
}

BOOLEAN _r_obj_initializeunicodestring2 (
	_Out_ PUNICODE_STRING string,
	_In_ PR_STRING buffer
)
{
	return _r_obj_initializeunicodestring_ex (
		string,
		buffer->buffer,
		(USHORT)buffer->length,
		(USHORT)buffer->length + sizeof (UNICODE_NULL)
	);
}

BOOLEAN _r_obj_initializeunicodestring3 (
	_Out_ PUNICODE_STRING string,
	_In_ PR_STRINGREF buffer
)
{
	return _r_obj_initializeunicodestring_ex (
		string,
		buffer->buffer,
		(USHORT)buffer->length,
		(USHORT)buffer->length + sizeof (UNICODE_NULL)
	);
}

//
// String builder
//

VOID _r_obj_initializestringbuilder (
	_Out_ PR_STRINGBUILDER builder
)
{
	_r_obj_initializestringbuilder_ex (builder, 256 * sizeof (WCHAR));
}

VOID _r_obj_initializestringbuilder_ex (
	_Out_ PR_STRINGBUILDER builder,
	_In_ SIZE_T initial_capacity
)
{
	// Make sure the initial capacity is even, as required for all string objects.
	if (initial_capacity & 0x01)
		initial_capacity += 1;

	builder->allocated_length = initial_capacity;

	// Allocate a new string object for the string builder.
	// We will dereference it and allocate a new one when we need to resize the string.
	builder->string = _r_obj_createstring_ex (NULL, initial_capacity);

	// We will keep modifying the length field of the string so that:
	// 1. We know how much of the string is used, and
	// 2. The user can simply get a reference to the string and use it as-is.
	builder->string->length = 0;

	// Write the null terminator.
	builder->string->buffer[0] = UNICODE_NULL;
}

VOID _r_obj_deletestringbuilder (
	_Inout_ PR_STRINGBUILDER builder
)
{
	builder->allocated_length = 0;

	SAFE_DELETE_REFERENCE (builder->string);
}

PR_STRING _r_obj_finalstringbuilder (
	_In_ PR_STRINGBUILDER builder
)
{
	return builder->string;
}

VOID _r_obj_appendstringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ LPCWSTR string
)
{
	_r_obj_appendstringbuilder_ex (builder, string, _r_str_getlength (string) * sizeof (WCHAR));
}

VOID _r_obj_appendstringbuilder2 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PR_STRING string
)
{
	_r_obj_appendstringbuilder_ex (builder, string->buffer, string->length);
}

VOID _r_obj_appendstringbuilder3 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PR_STRINGREF string
)
{
	_r_obj_appendstringbuilder_ex (builder, string->buffer, string->length);
}

VOID _r_obj_appendstringbuilder4 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PUNICODE_STRING string
)
{
	_r_obj_appendstringbuilder_ex (builder, string->Buffer, string->Length);
}

VOID _r_obj_appendstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ LPCWSTR string,
	_In_ SIZE_T length
)
{
	// See if we need to re-allocate the string.
	if (builder->allocated_length < builder->string->length + length)
		_r_obj_resizestringbuilder (builder, builder->string->length + length);

	// Copy the string, add the length, then write the null terminator.
	RtlCopyMemory (
		PTR_ADD_OFFSET (builder->string->buffer, builder->string->length),
		string,
		length
	);

	builder->string->length += length;

	_r_obj_writestringnullterminator (builder->string); // terminate
}

VOID _r_obj_appendstringbuilderformat (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_appendstringbuilderformat_v (builder, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_obj_appendstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	LPWSTR buffer;
	SIZE_T length_in_bytes;
	LONG length;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (builder->allocated_length < builder->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (builder, builder->string->length + length_in_bytes);

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)

	buffer = PTR_ADD_OFFSET (builder->string->buffer, builder->string->length);
	_vsnwprintf (buffer, length, format, arg_ptr);

#pragma warning(pop)

	builder->string->length += length_in_bytes;

	_r_obj_writestringnullterminator (builder->string); // terminate
}

VOID _r_obj_insertstringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ LPCWSTR string
)
{
	_r_obj_insertstringbuilder_ex (builder, index, string, _r_str_getlength (string) * sizeof (WCHAR));
}

VOID _r_obj_insertstringbuilder2 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PR_STRING string
)
{
	_r_obj_insertstringbuilder_ex (builder, index, string->buffer, string->length);
}

VOID _r_obj_insertstringbuilder3 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PR_STRINGREF string
)
{
	_r_obj_insertstringbuilder_ex (builder, index, string->buffer, string->length);
}

VOID _r_obj_insertstringbuilder4 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PUNICODE_STRING string
)
{
	_r_obj_insertstringbuilder_ex (builder, index, string->Buffer, string->Length);
}

VOID _r_obj_insertstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ LPCWSTR string,
	_In_ SIZE_T length
)
{
	// See if we need to re-allocate the string.
	if (builder->allocated_length < builder->string->length + length)
		_r_obj_resizestringbuilder (builder, builder->string->length + length);

	if ((index * sizeof (WCHAR)) < builder->string->length)
	{
		// Create some space for the string.
		RtlMoveMemory (
			&builder->string->buffer[index + (length / sizeof (WCHAR))],
			&builder->string->buffer[index],
			builder->string->length - (index * sizeof (WCHAR))
		);
	}

	// Copy the new string.
	RtlCopyMemory (
		&builder->string->buffer[index],
		string,
		length
	);

	builder->string->length += length;

	_r_obj_writestringnullterminator (builder->string); // terminate
}

VOID _r_obj_insertstringbuilderformat (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_insertstringbuilderformat_v (builder, index, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_obj_insertstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	SIZE_T length_in_bytes;
	LONG length;

	length = _vscwprintf (format, arg_ptr);

	if (length == 0 || length == -1)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (builder->allocated_length < builder->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (builder, builder->string->length + length_in_bytes);

	if ((index * sizeof (WCHAR)) < builder->string->length)
	{
		RtlMoveMemory (
			&builder->string->buffer[index + length],
			&builder->string->buffer[index],
			builder->string->length - (index * sizeof (WCHAR))
		);
	}

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf (&builder->string->buffer[index], length, format, arg_ptr);
#pragma warning(pop)

	builder->string->length += length_in_bytes;

	_r_obj_writestringnullterminator (builder->string); // terminate
}

VOID _r_obj_resizestringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T new_capacity
)
{
	PR_STRING new_string;
	SIZE_T new_size;

	// Double the string size. If that still isn't enough room, just use the new length.
	new_size = builder->allocated_length * 2;

	if (new_capacity & 0x01)
		new_capacity += 1;

	if (new_size < new_capacity)
		new_size = new_capacity;

	// Allocate a new string.
	new_string = _r_obj_createstring_ex (NULL, new_size);

	// Copy the old string to the new string.
	RtlCopyMemory (
		new_string->buffer,
		builder->string->buffer,
		builder->string->length + sizeof (UNICODE_NULL)
	);

	// Copy the old string length.
	new_string->length = builder->string->length;

	// Dereference the old string and replace it with the new string.
	_r_obj_movereference (&builder->string, new_string);
}

//
// Array object
//

PR_ARRAY _r_obj_createarray_ex (
	_In_ SIZE_T item_size,
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_ARRAY array_node;

	if (!initial_capacity)
		initial_capacity = 1;

	array_node = _r_obj_allocate (sizeof (R_ARRAY), &_r_util_cleanarray_callback);

	array_node->allocated_count = initial_capacity;
	array_node->cleanup_callback = cleanup_callback;
	array_node->count = 0;

	array_node->item_size = item_size;
	array_node->items = _r_mem_allocatezero (initial_capacity * item_size);

	return array_node;
}

PR_ARRAY _r_obj_createarray (
	_In_ SIZE_T item_size,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	return _r_obj_createarray_ex (item_size, 2, cleanup_callback);
}

PVOID _r_obj_addarrayitem_ex (
	_Inout_ PR_ARRAY array_node,
	_In_opt_ LPCVOID array_item,
	_Out_opt_ PSIZE_T new_index_ptr
)
{
	PVOID dst;
	SIZE_T new_index;

	if (array_node->count == array_node->allocated_count)
		_r_obj_resizearray (array_node, array_node->allocated_count * 2);

	new_index = array_node->count;

	dst = _r_obj_getarrayitem (array_node, new_index);

	if (array_item)
	{
		RtlCopyMemory (dst, array_item, array_node->item_size);
	}
	else
	{
		RtlSecureZeroMemory (dst, array_node->item_size);
	}

	array_node->count += 1;

	if (new_index_ptr)
		*new_index_ptr = new_index;

	return dst;
}

PVOID _r_obj_addarrayitem (
	_Inout_ PR_ARRAY array_node,
	_In_opt_ LPCVOID array_item
)
{
	return _r_obj_addarrayitem_ex (array_node, array_item, NULL);
}

VOID _r_obj_addarrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ LPCVOID array_items,
	_In_ SIZE_T count
)
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

	RtlCopyMemory (dst, array_items, count * array_node->item_size);

	array_node->count += count;
}

VOID _r_obj_cleararray (
	_Inout_ PR_ARRAY array_node
)
{
	PVOID array_item;
	SIZE_T count;
	SIZE_T i;

	if (!array_node->count)
		return;

	count = array_node->count;
	array_node->count = 0;

	if (array_node->cleanup_callback)
	{
		for (i = 0; i < count; i++)
		{
			array_item = PTR_ADD_OFFSET (array_node->items, i * array_node->item_size);

			array_node->cleanup_callback (array_item);
		}
	}

	RtlSecureZeroMemory (array_node->items, count * array_node->item_size);
}

PVOID _r_obj_getarrayitem (
	_In_ PR_ARRAY array_node,
	_In_ SIZE_T index
)
{
	return PTR_ADD_OFFSET (array_node->items, index * array_node->item_size);
}

SIZE_T _r_obj_getarraysize (
	_In_ PR_ARRAY array_node
)
{
	return array_node->count;
}

VOID _r_obj_removearrayitem (
	_In_ PR_ARRAY array_node,
	_In_ SIZE_T index
)
{
	_r_obj_removearrayitems (array_node, index, 1);
}

VOID _r_obj_removearrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T count
)
{
	PVOID dst;
	PVOID src;
	PVOID array_item;

	dst = _r_obj_getarrayitem (array_node, start_pos);
	src = _r_obj_getarrayitem (array_node, start_pos + count);

	if (array_node->cleanup_callback)
	{
		for (SIZE_T i = start_pos; i < (start_pos + count); i++)
		{
			array_item = PTR_ADD_OFFSET (array_node->items, i * array_node->item_size);

			array_node->cleanup_callback (array_item);
		}
	}

	RtlMoveMemory (
		dst,
		src,
		(array_node->count - start_pos - count) * array_node->item_size
	);

	array_node->count -= count;
}

VOID _r_obj_resizearray (
	_Inout_ PR_ARRAY array_node,
	_In_ SIZE_T new_capacity
)
{
	if (array_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	array_node->allocated_count = new_capacity;

	array_node->items = _r_mem_reallocatezero (
		array_node->items,
		array_node->allocated_count * array_node->item_size
	);
}

//
// List object
//

PR_LIST _r_obj_createlist_ex (
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_LIST list_node;

	if (!initial_capacity)
		initial_capacity = 1;

	list_node = _r_obj_allocate (sizeof (R_LIST), &_r_util_cleanlist_callback);

	list_node->allocated_count = initial_capacity;
	list_node->cleanup_callback = cleanup_callback;
	list_node->count = 0;

	list_node->items = _r_mem_allocatezero (list_node->allocated_count * sizeof (PVOID));

	return list_node;
}

PR_LIST _r_obj_createlist (
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	return _r_obj_createlist_ex (2, cleanup_callback);
}

VOID _r_obj_addlistitem_ex (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item,
	_Out_opt_ PSIZE_T new_index_ptr
)
{
	SIZE_T new_index;

	if (list_node->count == list_node->allocated_count)
		_r_obj_resizelist (list_node, list_node->allocated_count * 2);

	new_index = list_node->count;

	list_node->items[new_index] = list_item;

	if (new_index_ptr)
		*new_index_ptr = new_index;

	list_node->count += 1;
}

VOID _r_obj_addlistitem (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item
)
{
	_r_obj_addlistitem_ex (list_node, list_item, NULL);
}

VOID _r_obj_clearlist (
	_Inout_ PR_LIST list_node
)
{
	PVOID list_item;
	SIZE_T count;
	SIZE_T i;

	if (!list_node->count)
		return;

	count = list_node->count;
	list_node->count = 0;

	if (list_node->cleanup_callback)
	{
		for (i = 0; i < count; i++)
		{
			list_item = list_node->items[i];

			if (list_item)
				list_node->cleanup_callback (list_item);
		}
	}

	RtlSecureZeroMemory (list_node->items, count * sizeof (PVOID));
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_obj_findlistitem (
	_In_ PR_LIST list_node,
	_In_opt_ LPCVOID list_item
)
{
	for (SIZE_T i = 0; i < list_node->count; i++)
	{
		if (list_node->items[i] == list_item)
			return i;
	}

	return SIZE_MAX;
}

_Ret_maybenull_
PVOID _r_obj_getlistitem (
	_In_ PR_LIST list_node,
	_In_ SIZE_T index
)
{
	return list_node->items[index];
}

SIZE_T _r_obj_getlistsize (
	_In_ PR_LIST list_node
)
{
	return list_node->count;
}

VOID _r_obj_insertlistitems (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T start_pos,
	_In_ PVOID_PTR list_items,
	_In_ SIZE_T count
)
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
		RtlMoveMemory (
			&list_node->items[start_pos + count],
			&list_node->items[start_pos],
			(list_node->count - start_pos) * sizeof (PVOID)
		);
	}

	RtlCopyMemory (&list_node->items[start_pos], list_items, count * sizeof (PVOID));

	list_node->count += count;
}

VOID _r_obj_removelistitem (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T index
)
{
	_r_obj_removelistitems (list_node, index, 1);
}

VOID _r_obj_removelistitems (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T count
)
{
	PVOID list_item;

	for (SIZE_T i = start_pos; i < (start_pos + count); i++)
	{
		list_item = list_node->items[i];
		list_node->items[i] = NULL;

		if (list_node->cleanup_callback)
		{
			if (list_item)
				list_node->cleanup_callback (list_item);
		}
	}

	RtlMoveMemory (
		&list_node->items[start_pos],
		&list_node->items[start_pos + count],
		(list_node->count - start_pos - count) * sizeof (PVOID)
	);

	list_node->count -= count;
}

VOID _r_obj_resizelist (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T new_capacity
)
{
	if (list_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	list_node->allocated_count = new_capacity;

	list_node->items = _r_mem_reallocatezero (list_node->items, list_node->allocated_count * sizeof (PVOID));
}

VOID _r_obj_setlistitem (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T index,
	_In_opt_ PVOID list_item
)
{
	PVOID prev_list_item;

	prev_list_item = list_node->items[index];
	list_node->items[index] = list_item;

	if (list_node->cleanup_callback)
	{
		if (prev_list_item)
			list_node->cleanup_callback (prev_list_item);
	}
}

//
// Hashtable object
//

// A hashtable with power-of-two bucket sizes and with all entries stored in a single
// array. This improves locality but may be inefficient when resizing the hashtable. It is a good
// idea to store pointers to objects as entries, as opposed to the objects themselves.

PR_HASHTABLE _r_obj_createhashtable_ex (
	_In_ SIZE_T entry_size,
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_HASHTABLE hashtable;

	hashtable = _r_obj_allocate (sizeof (R_HASHTABLE), &_r_util_cleanhashtable_callback);

	// Initial capacity of 0 is not allowed.
	if (!initial_capacity)
		initial_capacity = 1;

	hashtable->entry_size = entry_size;
	hashtable->cleanup_callback = cleanup_callback;

	// Allocate the buckets.
	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (initial_capacity);
	hashtable->buckets = _r_mem_allocatezero (hashtable->allocated_buckets * sizeof (SIZE_T));

	// Set all bucket values to -1.
	memset (
		hashtable->buckets,
		PR_HASHTABLE_INIT_VALUE,
		hashtable->allocated_buckets * sizeof (SIZE_T)
	);

	// Allocate the entries.
	hashtable->allocated_entries = hashtable->allocated_buckets;
	hashtable->entries = _r_mem_allocatezero (hashtable->allocated_entries * PR_HASHTABLE_ENTRY_SIZE (entry_size));

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

	return hashtable;
}

PR_HASHTABLE _r_obj_createhashtable (
	_In_ SIZE_T entry_size,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	return _r_obj_createhashtable_ex (entry_size, 2, cleanup_callback);
}

FORCEINLINE PVOID _r_obj_addhashtableitem_ex (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry,
	_In_ BOOLEAN is_checkduplicates,
	_Out_opt_ PBOOLEAN is_added_ptr
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;
	SIZE_T free_entry;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);

	if (is_checkduplicates)
	{
		for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
		{
			hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, i);

			if (_r_obj_validatehash (hashtable_entry->hash_code) == valid_hash)
			{
				if (is_added_ptr)
					*is_added_ptr = FALSE;

				return &hashtable_entry->body;
			}
		}
	}

	// Use a free entry if possible.
	if (hashtable->free_entry != SIZE_MAX)
	{
		free_entry = hashtable->free_entry;
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, free_entry);
		hashtable->free_entry = hashtable_entry->next;
	}
	else
	{
		// Use the next entry in the entry array.
		if (hashtable->next_entry == hashtable->allocated_entries)
		{
			// Resize the hashtable.
			_r_obj_resizehashtable (hashtable, hashtable->allocated_buckets * 2);

			index = _r_obj_indexfromhash (hashtable, valid_hash);
		}

		free_entry = hashtable->next_entry++;
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, free_entry);
	}

	if (hashtable_entry->hash_code && hashtable_entry->hash_code != SIZE_MAX)
	{
		if (hashtable->cleanup_callback)
			hashtable->cleanup_callback (&hashtable_entry->body);
	}

	// Initialize the entry.
	hashtable_entry->hash_code = hash_code;
	hashtable_entry->next = hashtable->buckets[index];

	hashtable->buckets[index] = free_entry;

	// Copy the user-supplied data to the entry.
	if (entry)
	{
		RtlCopyMemory (&hashtable_entry->body, entry, hashtable->entry_size);
	}
	else
	{
		RtlSecureZeroMemory (&hashtable_entry->body, hashtable->entry_size);
	}

	hashtable->count += 1;

	if (is_added_ptr)
		*is_added_ptr = TRUE;

	return &hashtable_entry->body;
}

_Ret_maybenull_
PVOID _r_obj_addhashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry
)
{
	PVOID hashtable_entry;
	BOOLEAN is_added;

	hashtable_entry = _r_obj_addhashtableitem_ex (hashtable, hash_code, entry, TRUE, &is_added);

	if (is_added)
		return hashtable_entry;

	return NULL;
}

PVOID _r_obj_replacehashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry
)
{
	PVOID hashtable_entry;

	hashtable_entry = _r_obj_addhashtableitem_ex (hashtable, hash_code, entry, FALSE, NULL);

	return hashtable_entry;
}

VOID _r_obj_clearhashtable (
	_Inout_ PR_HASHTABLE hashtable
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T next_entry;
	SIZE_T index;

	if (!hashtable->count)
		return;

	next_entry = hashtable->next_entry;

	memset (
		hashtable->buckets,
		PR_HASHTABLE_INIT_VALUE,
		hashtable->allocated_buckets * sizeof (SIZE_T)
	);

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

	index = 0;

	while (index < next_entry)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, index);

		if (hashtable_entry->hash_code != SIZE_MAX)
		{
			hashtable_entry->hash_code = SIZE_MAX;

			if (hashtable->cleanup_callback)
				hashtable->cleanup_callback (&hashtable_entry->body);

			RtlSecureZeroMemory (&hashtable_entry->body, hashtable->entry_size);
		}

		index += 1;
	}
}

_Success_ (return)
BOOLEAN _r_obj_enumhashtable (
	_In_ PR_HASHTABLE hashtable,
	_Outptr_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PSIZE_T enum_key
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;

	while (*enum_key < hashtable->next_entry)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, *enum_key);

		(*enum_key) += 1;

		if (hashtable_entry->hash_code != SIZE_MAX)
		{
			if (entry_ptr)
				*entry_ptr = &hashtable_entry->body;

			if (hash_code_ptr)
				*hash_code_ptr = hashtable_entry->hash_code;

			return TRUE;
		}
	}

	return FALSE;
}

_Ret_maybenull_
PVOID _r_obj_findhashtable (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);

	for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, i);

		if (_r_obj_validatehash (hashtable_entry->hash_code) == valid_hash)
			return &hashtable_entry->body;
	}

	return NULL;
}

SIZE_T _r_obj_gethashtablesize (
	_In_ PR_HASHTABLE hashtable
)
{
	return hashtable->count;
}

BOOLEAN _r_obj_removehashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;
	SIZE_T previous_index;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);
	previous_index = SIZE_MAX;

	for (SIZE_T i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, i);

		if (_r_obj_validatehash (hashtable_entry->hash_code) == valid_hash)
		{
			// Unlink the entry from the bucket.
			if (previous_index == SIZE_MAX)
			{
				hashtable->buckets[index] = hashtable_entry->next;
			}
			else
			{
				PR_HASHTABLE_GET_ENTRY (hashtable, previous_index)->next = hashtable_entry->next;
			}

			hashtable_entry->hash_code = SIZE_MAX; // indicates the entry is not being used
			hashtable_entry->next = hashtable->free_entry;

			hashtable->free_entry = i;
			hashtable->count -= 1;

			if (hashtable->cleanup_callback)
				hashtable->cleanup_callback (&hashtable_entry->body);

			RtlSecureZeroMemory (&hashtable_entry->body, hashtable->entry_size);

			return TRUE;
		}

		previous_index = i;
	}

	return FALSE;
}

VOID _r_obj_resizehashtable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ SIZE_T new_capacity
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	SIZE_T index;

	// Re-allocate the buckets. Note that we don't need to keep the contents.
	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (new_capacity);

	hashtable->buckets = _r_mem_reallocatezero (
		hashtable->buckets,
		hashtable->allocated_buckets * sizeof (SIZE_T)
	);

	// Set all bucket values to -1.
	memset (
		hashtable->buckets,
		PR_HASHTABLE_INIT_VALUE,
		hashtable->allocated_buckets * sizeof (SIZE_T)
	);

	// Re-allocate the entries.
	hashtable->allocated_entries = hashtable->allocated_buckets;

	hashtable->entries = _r_mem_reallocatezero (
		hashtable->entries,
		PR_HASHTABLE_ENTRY_SIZE (hashtable->entry_size) * hashtable->allocated_entries
	);

	// Re-distribute the entries among the buckets.

	// PR_HASHTABLE_GET_ENTRY is quite slow (it involves a multiply), so we use a pointer here.
	hashtable_entry = hashtable->entries;

	for (SIZE_T i = 0; i < hashtable->next_entry; i++)
	{
		if (hashtable_entry->hash_code != SIZE_MAX)
		{
			index = _r_obj_indexfromhash (hashtable, _r_obj_validatehash (hashtable_entry->hash_code));

			hashtable_entry->next = hashtable->buckets[index];
			hashtable->buckets[index] = i;
		}

		hashtable_entry = PTR_ADD_OFFSET (hashtable_entry, PR_HASHTABLE_ENTRY_SIZE (hashtable->entry_size));
	}
}

//
// Hashtable pointer object
//

PR_HASHTABLE _r_obj_createhashtablepointer (
	_In_ SIZE_T initial_capacity
)
{
	return _r_obj_createhashtable_ex (
		sizeof (R_OBJECT_POINTER),
		initial_capacity,
		&_r_util_cleanhashtablepointer_callback
	);
}

_Ret_maybenull_
PR_OBJECT_POINTER _r_obj_addhashtablepointer (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ PVOID value
)
{
	PR_OBJECT_POINTER hashtable_entry;
	R_OBJECT_POINTER object_ptr;

	object_ptr.object_body = value;

	hashtable_entry = _r_obj_addhashtableitem (hashtable, hash_code, &object_ptr);

	return hashtable_entry;
}

_Success_ (return)
BOOLEAN _r_obj_enumhashtablepointer (
	_In_ PR_HASHTABLE hashtable,
	_Outptr_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PSIZE_T enum_key
)
{
	PR_OBJECT_POINTER object_ptr;
	BOOLEAN is_success;

	is_success = _r_obj_enumhashtable (hashtable, &object_ptr, hash_code_ptr, enum_key);

	if (is_success)
	{
		if (entry_ptr)
			*entry_ptr = object_ptr->object_body;
	}

	return is_success;
}

_Ret_maybenull_
PVOID _r_obj_findhashtablepointer (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
)
{
	PR_OBJECT_POINTER object_ptr;

	object_ptr = _r_obj_findhashtable (hashtable, hash_code);

	if (object_ptr)
		return _r_obj_referencesafe (object_ptr->object_body);

	return NULL;
}

BOOLEAN _r_obj_removehashtablepointer (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
)
{
	return _r_obj_removehashtableitem (hashtable, hash_code);
}

//
// System messages
//

_Success_ (return)
BOOLEAN _r_msg_taskdialog (
	_In_ const TASKDIALOGCONFIG * task_dialog,
	_Out_opt_ PINT button_ptr,
	_Out_opt_ PINT radio_button_ptr,
	_Out_opt_ LPBOOL is_flagchecked_ptr
)
{
	HRESULT hr;

#if !defined(APP_NO_DEPRECATIONS)
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static TDI _TaskDialogIndirect = NULL;

	HINSTANCE hcomctl32;

	if (_r_initonce_begin (&init_once))
	{
		hcomctl32 = _r_sys_loadlibrary (L"comctl32.dll");

		if (hcomctl32)
		{
			// vista+
			_TaskDialogIndirect = (TDI)GetProcAddress (hcomctl32, "TaskDialogIndirect");

			FreeLibrary (hcomctl32);
		}

		_r_initonce_end (&init_once);
	}

	if (_TaskDialogIndirect)
	{
		hr = _TaskDialogIndirect (task_dialog, button_ptr, radio_button_ptr, is_flagchecked_ptr);
	}
	else
	{
		return FALSE;
	}

#else

	hr = TaskDialogIndirect (task_dialog, button_ptr, radio_button_ptr, is_flagchecked_ptr);

#endif // APP_NO_DEPRECATIONS

	return (hr == S_OK);
}

HRESULT CALLBACK _r_msg_callback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam,
	_In_opt_ LONG_PTR lpdata
)
{
	UNREFERENCED_PARAMETER (wparam);

	switch (msg)
	{
		case TDN_CREATED:
		{
			HWND hparent;
			BOOL is_topmost;

			// center window
			hparent = GetParent (hwnd);

			_r_wnd_center (hwnd, hparent);

			// set on top
			is_topmost = HIWORD (lpdata);

			if (is_topmost)
				_r_wnd_top (hwnd, TRUE);

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
			LPCWSTR link;

			link = (LPCWSTR)lparam;

			if (link)
				_r_shell_opendefault (link);

			break;
		}
	}

	return S_OK;
}

//
// Clipboard operations
//

_Ret_maybenull_
PR_STRING _r_clipboard_get (
	_In_opt_ HWND hwnd
)
{
	PR_STRING string;
	HANDLE hdata;
	PVOID base_address;
	SIZE_T length;

	if (!OpenClipboard (hwnd))
		return NULL;

	hdata = GetClipboardData (CF_UNICODETEXT);
	string = NULL;

	if (hdata)
	{
		base_address = GlobalLock (hdata);

		if (base_address)
		{
			length = GlobalSize (base_address);

			string = _r_obj_createstring_ex (base_address, length);
		}

		GlobalUnlock (hdata);
	}

	CloseClipboard ();

	return string;
}

BOOLEAN _r_clipboard_set (
	_In_opt_ HWND hwnd,
	_In_ PR_STRINGREF string
)
{
	HANDLE hdata;
	PVOID base_address;
	BOOLEAN is_success;

	if (!OpenClipboard (hwnd))
		return FALSE;

	hdata = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, string->length + sizeof (UNICODE_NULL));

	is_success = FALSE;

	if (hdata)
	{
		base_address = GlobalLock (hdata);

		if (base_address)
		{
			RtlCopyMemory (base_address, string->buffer, string->length);

			*(LPWSTR)PTR_ADD_OFFSET (base_address, string->length) = UNICODE_NULL; // terminate

			GlobalUnlock (base_address);

			if (EmptyClipboard ())
				is_success = !!(SetClipboardData (CF_UNICODETEXT, hdata));
		}
	}

	if (!is_success)
		GlobalFree (hdata);

	CloseClipboard ();

	return is_success;
}

//
// Filesystem
//

BOOLEAN _r_fs_deletefile (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_forced
)
{
	ULONG attributes;

	attributes = GetFileAttributes (path);

	if (attributes == INVALID_FILE_ATTRIBUTES)
		return FALSE;

	if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;

	if (is_forced)
		SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL);

	return !!DeleteFile (path);
}

BOOLEAN _r_fs_deletedirectory (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_recurse
)
{
	SHFILEOPSTRUCT shfop = {0};
	PR_STRING string;
	SIZE_T length;
	ULONG attributes;
	LONG status;

	attributes = GetFileAttributes (path);

	if (attributes == INVALID_FILE_ATTRIBUTES)
		return FALSE;

	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
		return FALSE;

	length = _r_str_getlength (path) + 1;

	// required to set 2 nulls at end
	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	_r_str_copy (string->buffer, length, path);

	shfop.wFunc = FO_DELETE;
	shfop.fFlags = FOF_NO_UI;
	shfop.pFrom = string->buffer;

	if (!is_recurse)
		shfop.fFlags |= FOF_NORECURSION;

	status = SHFileOperation (&shfop);

	_r_obj_dereference (string);

	return (status == ERROR_SUCCESS);
}

LONG64 _r_fs_getsize (
	_In_ HANDLE hfile
)
{
	LARGE_INTEGER file_size;

	if (GetFileSizeEx (hfile, &file_size))
		return file_size.QuadPart;

	return 0;
}

LONG64 _r_fs_getfilesize (
	_In_ LPCWSTR path
)
{
	HANDLE hfile;
	LONG64 file_size;

	hfile = CreateFile (
		path,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (!_r_fs_isvalidhandle (hfile))
		return 0;

	file_size = _r_fs_getsize (hfile);

	NtClose (hfile);

	return file_size;
}

BOOLEAN _r_fs_makebackup (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_removesourcefile
)
{
	WCHAR timestamp_string[64];
	PR_STRING new_path;
	LONG64 current_timestamp;

	R_STRINGREF directory_part;
	R_STRINGREF basename_part;
	PR_STRING directory;

	BOOLEAN is_success;

	current_timestamp = _r_unixtime_now ();

	_r_obj_initializestringrefconst (&directory_part, path);

	_r_path_getpathinfo (&directory_part, &directory_part, &basename_part);

	directory = _r_obj_createstring3 (&directory_part);

	_r_str_fromlong64 (timestamp_string, RTL_NUMBER_OF (timestamp_string), current_timestamp);

	new_path = _r_obj_concatstrings (
		6,
		directory->buffer,
		L"\\",
		basename_part.buffer,
		L"-",
		timestamp_string,
		L".bak"
	);

	if (_r_fs_exists (new_path->buffer))
		_r_fs_deletefile (new_path->buffer, TRUE);

	if (is_removesourcefile)
	{
		is_success = _r_fs_movefile (path, new_path->buffer, 0);

		if (!is_success)
			is_success = _r_fs_movefile (path, new_path->buffer, MOVEFILE_COPY_ALLOWED);
	}
	else
	{
		is_success = _r_fs_copyfile (path, new_path->buffer, 0);
	}

	_r_obj_dereference (directory);
	_r_obj_dereference (new_path);

	return is_success;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_fs_mkdir (
	_In_ LPCWSTR path
)
{
	ULONG attributes;
	ULONG status;

	attributes = GetFileAttributes (path);

	if (attributes != INVALID_FILE_ATTRIBUTES)
	{
		if (attributes & FILE_ATTRIBUTE_DIRECTORY) // already exists
			return TRUE;

		_r_fs_deletefile (path, TRUE);
	}

	status = SHCreateDirectoryEx (NULL, path, NULL);

	return status;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_fs_mapfile (
	_In_ LPCWSTR path,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	LARGE_INTEGER file_size;
	PVOID file_bytes;
	HANDLE hfile;
	HANDLE hmap;
	NTSTATUS status;

	*out_buffer = NULL;

	hfile = CreateFile (
		path,
		FILE_GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL
	);

	if (!_r_fs_isvalidhandle (hfile))
		return RtlGetLastNtStatus ();

	if (!GetFileSizeEx (hfile, &file_size))
	{
		status = GetLastError ();

		NtClose (hfile);

		return status;
	}

	hmap = CreateFileMapping (hfile, NULL, PAGE_READONLY, file_size.HighPart, file_size.LowPart, NULL);

	if (!hmap)
	{
		status = GetLastError ();
	}
	else
	{
		file_bytes = MapViewOfFile (hmap, FILE_MAP_READ, 0, 0, 0);

		if (!file_bytes)
		{
			status = GetLastError ();
		}
		else
		{
#ifdef _WIN64
			*out_buffer = _r_obj_createbyte_ex (file_bytes, file_size.QuadPart);
#else
			*out_buffer = _r_obj_createbyte_ex (file_bytes, file_size.LowPart);
#endif

			UnmapViewOfFile (file_bytes);

			status = STATUS_SUCCESS;
		}

		NtClose (hmap);
	}

	NtClose (hfile);

	return status;
}

LONG64 _r_fs_getpos (
	_In_ HANDLE hfile
)
{
	LARGE_INTEGER li = {0};
	LARGE_INTEGER pos;

	if (!SetFilePointerEx (hfile, li, &pos, FILE_CURRENT))
		return 0;

	return pos.QuadPart;
}

BOOLEAN _r_fs_setpos (
	_In_ HANDLE hfile,
	_In_ LONG64 pos,
	_In_ ULONG method
)
{
	LARGE_INTEGER li = {0};

	li.QuadPart = pos;

	return !!SetFilePointerEx (hfile, li, NULL, method);
}

//
// Paths
//

PR_STRING _r_path_compact (
	_In_ PR_STRING path,
	_In_ ULONG length
)
{
	PR_STRING string;

	if (_r_str_getlength2 (path) <= length)
		return _r_obj_reference (path);

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	if (PathCompactPathEx (string->buffer, path->buffer, length, 0))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;
	}

	_r_obj_dereference (string);

	return _r_obj_reference (path);
}

BOOLEAN _r_path_getpathinfo (
	_In_ PR_STRINGREF path,
	_Out_opt_ PR_STRINGREF directory,
	_Out_opt_ PR_STRINGREF basename
)
{
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;
	BOOLEAN is_success;

	if (!directory && !basename)
		return FALSE;

	is_success = _r_str_splitatlastchar (
		path,
		OBJ_NAME_PATH_SEPARATOR,
		&directory_part,
		&basename_part
	);

	if (directory)
		_r_obj_initializestringref3 (directory, &directory_part);

	if (basename)
		_r_obj_initializestringref3 (basename, &basename_part);

	return is_success;
}

_Ret_maybenull_
PR_STRING _r_path_getbasedirectory (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;

	if (!_r_path_getpathinfo (path, &directory_part, &basename_part))
		return NULL;

	return _r_obj_createstring3 (&directory_part);
}

LPCWSTR _r_path_getbasename (
	_In_ LPCWSTR path
)
{
	R_STRINGREF fullpath_part;
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;

	_r_obj_initializestringrefconst (&fullpath_part, path);

	if (!_r_path_getpathinfo (&fullpath_part, &directory_part, &basename_part))
		return path;

	return basename_part.buffer;
}

PR_STRING _r_path_getbasenamestring (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;

	if (!_r_path_getpathinfo (path, &directory_part, &basename_part))
		return _r_obj_createstring3 (path);

	return _r_obj_createstring3 (&basename_part);
}

_Ret_maybenull_
LPCWSTR _r_path_getextension (
	_In_ LPCWSTR path
)
{
	R_STRINGREF fullpath_part;
	R_STRINGREF directory_part;
	R_STRINGREF extension_part;

	_r_obj_initializestringrefconst (&fullpath_part, path);

	if (!_r_str_splitatlastchar (&fullpath_part, L'.', &directory_part, &extension_part))
		return NULL;

	return extension_part.buffer;
}

_Ret_maybenull_
PR_STRING _r_path_getextensionstring (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF directory_part;
	R_STRINGREF extension_part;

	if (!_r_str_splitatlastchar (path, L'.', &directory_part, &extension_part))
		return NULL;

	return _r_obj_createstring3 (&extension_part);
}

_Ret_maybenull_
PR_STRING _r_path_getfullpath (
	_In_ LPCWSTR path
)
{
	PR_STRING full_path;
	ULONG buffer_length;
	ULONG return_length;

	buffer_length = 256;
	full_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = RtlGetFullPathName_U (path, buffer_length, full_path->buffer, NULL);

	if (return_length > buffer_length)
	{
		buffer_length = return_length;

		_r_obj_dereference (full_path);
		full_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

		return_length = RtlGetFullPathName_U (path, buffer_length, full_path->buffer, NULL);
	}

	if (return_length)
	{
		_r_obj_setstringlength (full_path, return_length);

		return full_path;
	}

	_r_obj_dereference (full_path);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_getknownfolder (
	_In_ ULONG folder,
	_In_opt_ LPCWSTR append
)
{
	PR_STRING string;
	SIZE_T append_length;
	HRESULT hr;

	append_length = append ? _r_str_getlength (append) * sizeof (WCHAR) : 0;

	string = _r_obj_createstring_ex (NULL, (256 * sizeof (WCHAR)) + append_length);

	hr = SHGetFolderPath (NULL, folder, NULL, SHGFP_TYPE_CURRENT, string->buffer);

	if (hr == S_OK)
	{
		_r_obj_trimstringtonullterminator (string);

		if (append)
		{
			RtlCopyMemory (
				&string->buffer[_r_str_getlength2 (string)],
				append,
				append_length + sizeof (UNICODE_NULL)
			);

			string->length += append_length;
		}

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (
	_In_opt_ HINSTANCE hinstance
)
{
	PR_STRING string;
	ULONG allocated_length;
	ULONG return_length;
	ULONG attempts;

	attempts = 6;
	allocated_length = 256;

	do
	{
		string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

		return_length = GetModuleFileName (hinstance, string->buffer, allocated_length);

		if (!return_length)
		{
			_r_obj_dereference (string);
			break;
		}

		if (return_length < allocated_length)
		{
			_r_obj_setstringlength (string, return_length * sizeof (WCHAR));
			return string;
		}

		allocated_length *= 2;

		_r_obj_dereference (string);

		if (allocated_length > PR_SIZE_BUFFER_OVERFLOW)
			break;
	}
	while (--attempts);

	return NULL;
}

BOOLEAN _r_path_issecurelocation (
	_In_ LPCWSTR file_path
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PACL dacl;
	PSID current_user_sid;
	PACCESS_ALLOWED_ACE ace;
	ULONG status;
	BOOLEAN is_writeable;

	status = GetNamedSecurityInfo (
		file_path,
		SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION,
		NULL,
		NULL,
		&dacl,
		NULL,
		&security_descriptor
	);

	if (status != ERROR_SUCCESS)
		return FALSE;

	is_writeable = FALSE;

	if (!dacl)
	{
		is_writeable = TRUE;
	}
	else
	{
		current_user_sid = _r_sys_getcurrenttoken ().token_sid;

		for (WORD ace_index = 0; ace_index < dacl->AceCount; ace_index++)
		{
			if (!GetAce (dacl, ace_index, &ace))
				continue;

			if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
				continue;

			if (RtlEqualSid (&ace->SidStart, &SeAuthenticatedUserSid) ||
				RtlEqualSid (&ace->SidStart, current_user_sid))
			{
				if (ace->Mask & (DELETE | ACTRL_FILE_WRITE_ATTRIB | SYNCHRONIZE | READ_CONTROL))
				{
					is_writeable = TRUE;
					break;
				}
			}
		}
	}

	LocalFree (security_descriptor);

	return !is_writeable;
}

// Parses a command line string. If the string does not contain quotation marks
// around the file name part, the function determines the file name to use.
//
// Source: https://github.com/processhacker2/processhacker

BOOLEAN _r_path_parsecommandlinefuzzy (
	_In_ PR_STRINGREF args,
	_Out_ PR_STRINGREF path,
	_Out_ PR_STRINGREF command_line,
	_Out_opt_ PR_STRING_PTR full_file_name
)
{
	static R_STRINGREF whitespace = PR_STRINGREF_INIT (L" \t");

	R_STRINGREF args_sr;
	R_STRINGREF current_part;
	R_STRINGREF arguments;
	R_STRINGREF remaining_part;
	R_STRINGREF temp;
	PR_STRING file_path_sr;
	PR_STRING buffer;
	WCHAR original_char;
	BOOLEAN is_found;

	args_sr = *args;

	_r_str_trimstringref (&args_sr, &whitespace, 0);

	if (args_sr.length == 0)
	{
		_r_obj_initializestringrefempty (path);
		_r_obj_initializestringrefempty (command_line);

		if (full_file_name)
			*full_file_name = NULL;

		return FALSE;
	}

	if (*args_sr.buffer == L'"')
	{
		_r_obj_skipstringlength (&args_sr, sizeof (WCHAR));

		// Find the matching quote character and we have our file name.
		if (!_r_str_splitatchar (&args_sr, L'"', &args_sr, &arguments))
		{
			// Unskip the initial quote character
			_r_obj_skipstringlength (&args_sr, -(LONG_PTR)sizeof (WCHAR));

			*path = args_sr;

			_r_obj_initializestringrefempty (command_line);

			if (full_file_name)
				*full_file_name = NULL;

			return FALSE;
		}

		_r_str_trimstringref (&arguments, &whitespace, PR_TRIM_START_ONLY);

		*path = args_sr;
		*command_line = arguments;

		if (full_file_name)
		{
			buffer = _r_obj_createstring3 (&args_sr);

			file_path_sr = _r_path_search (buffer->buffer, L".exe", FALSE);

			if (file_path_sr)
			{
				*full_file_name = file_path_sr;
			}
			else
			{
				*full_file_name = NULL;
			}

			_r_obj_dereference (buffer);
		}

		return TRUE;
	}

	// Try to find an existing executable file, starting with the first part of the command line and
	// successively restoring the rest of the command line.
	//
	// For example, in "C:\Program Files\Internet Explorer\iexplore", we try to match:
	// * "C:\Program"
	// * "C:\Program Files\Internet"
	// * "C:\Program Files\Internet "
	// * "C:\Program Files\Internet  "
	// * "C:\Program Files\Internet   Explorer\iexplore"
	//
	// Note that we do not trim whitespace in each part because filenames can contain trailing
	// whitespace before the extension (e.g. "Internet  .exe").

	temp.buffer = _r_mem_allocatezero (args_sr.length + sizeof (UNICODE_NULL));

	RtlCopyMemory (temp.buffer, args_sr.buffer, args_sr.length);

	temp.buffer[_r_str_getlength3 (&args_sr)] = UNICODE_NULL;
	temp.length = args_sr.length;

	remaining_part = temp;

	while (remaining_part.length != 0)
	{
		original_char = UNICODE_NULL;

		is_found = _r_str_splitatchar (&remaining_part, L' ', &current_part, &remaining_part);

		if (is_found)
		{
			original_char = *(remaining_part.buffer - 1);
			*(remaining_part.buffer - 1) = UNICODE_NULL;
		}

		file_path_sr = _r_path_search (temp.buffer, L".exe", FALSE);

		if (is_found)
			*(remaining_part.buffer - 1) = original_char;

		if (file_path_sr)
		{
			path->buffer = args_sr.buffer;
			path->length = (SIZE_T)PTR_SUB_OFFSET (current_part.buffer, temp.buffer) + current_part.length;

			_r_str_trimstringref (&remaining_part, &whitespace, PR_TRIM_START_ONLY);

			*command_line = remaining_part;

			if (full_file_name)
			{
				*full_file_name = file_path_sr;
			}
			else
			{
				_r_obj_dereference (file_path_sr);
			}

			_r_mem_free (temp.buffer);

			return TRUE;
		}
	}

	_r_mem_free (temp.buffer);

	*path = *args;

	_r_obj_initializestringrefempty (command_line);

	if (full_file_name)
		*full_file_name = NULL;

	return FALSE;
}

_Ret_maybenull_
PR_STRING _r_path_resolvedeviceprefix (
	_In_ PR_STRING path
)
{
	// device name prefixes
	OBJECT_ATTRIBUTES object_attributes;
	UNICODE_STRING device_name;
	UNICODE_STRING device_prefix;
	R_STRINGREF device_prefix_sr;
	WCHAR device_name_buffer[7] = L"\\??\\?:";
	WCHAR device_prefix_buffer[PR_DEVICE_PREFIX_LENGTH] = {0};
	PR_STRING string;
	HANDLE link_handle;
	SIZE_T prefix_length;
	NTSTATUS status;

#ifndef _WIN64
	PROCESS_DEVICEMAP_INFORMATION device_map = {0};
#else
	PROCESS_DEVICEMAP_INFORMATION_EX device_map = {0};
#endif

	status = NtQueryInformationProcess (
		NtCurrentProcess (),
		ProcessDeviceMap,
		&device_map,
		sizeof (device_map),
		NULL
	);

	if (NT_SUCCESS (status))
	{
		for (ULONG i = 0; i < PR_DEVICE_COUNT; i++)
		{
			if (device_map.Query.DriveMap)
			{
				if (!(device_map.Query.DriveMap & (0x01 << i)))
					continue;
			}

			device_name_buffer[4] = L'A' + (WCHAR)i;

			_r_obj_initializeunicodestring_ex (
				&device_name,
				device_name_buffer,
				(RTL_NUMBER_OF (device_name_buffer) - 1) * sizeof (WCHAR),
				RTL_NUMBER_OF (device_name_buffer) * sizeof (WCHAR)
			);

			InitializeObjectAttributes (
				&object_attributes,
				&device_name,
				OBJ_CASE_INSENSITIVE | (_r_sys_isosversiongreaterorequal (WINDOWS_10) ? OBJ_DONT_REPARSE : 0),
				NULL,
				NULL
			);

			status = NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &object_attributes);

			if (NT_SUCCESS (status))
			{
				_r_obj_initializeunicodestring_ex (
					&device_prefix,
					device_prefix_buffer,
					0,
					RTL_NUMBER_OF (device_prefix_buffer) * sizeof (WCHAR)
				);

				status = NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL);

				if (NT_SUCCESS (status))
				{
					_r_obj_initializestringref4 (&device_prefix_sr, &device_prefix);

					if (_r_str_isstartswith (&path->sr, &device_prefix_sr, TRUE))
					{
						prefix_length = _r_str_getlength4 (&device_prefix);

						// To ensure we match the longest prefix, make sure the next character is a
						// backslash or the path is equal to the prefix.
						if (path->length == device_prefix.Length || path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
						{
							// <letter>:path
							string = _r_obj_createstring_ex (
								NULL,
								(2 * sizeof (WCHAR)) + path->length - device_prefix.Length
							);

							string->buffer[0] = L'A' + (WCHAR)i;
							string->buffer[1] = L':';

							RtlCopyMemory (
								&string->buffer[2],
								&path->buffer[prefix_length],
								path->length - device_prefix.Length
							);

							_r_obj_trimstringtonullterminator (string);

							NtClose (link_handle);

							return string;
						}
					}
				}

				NtClose (link_handle);
			}
		}
	}

	return NULL;
}

// Device map link resolution does not resolve custom FS.
// Workaround this thing using \\GLOBAL?? objects enumeration. (HACK!!!)
//
// https://github.com/henrypp/simplewall/issues/817
// https://github.com/maharmstone/btrfs/issues/324

_Ret_maybenull_
PR_STRING _r_path_resolvedeviceprefix_workaround (
	_In_ PR_STRING path
)
{
	OBJECT_ATTRIBUTES object_attributes;
	UNICODE_STRING device_prefix;
	R_STRINGREF device_prefix_sr;
	WCHAR device_prefix_buffer[PR_DEVICE_PREFIX_LENGTH] = {0};
	PR_STRING string;
	HANDLE link_handle;
	HANDLE directory_handle;
	SIZE_T prefix_length;
	ULONG query_context;
	POBJECT_DIRECTORY_INFORMATION directory_entry;
	POBJECT_DIRECTORY_INFORMATION directory_info;
	ULONG i;
	ULONG buffer_size;
	BOOLEAN is_firsttime;
	NTSTATUS status;

	RtlInitUnicodeString (&device_prefix, L"\\GLOBAL??");

	InitializeObjectAttributes (&object_attributes, &device_prefix, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtOpenDirectoryObject (&directory_handle, DIRECTORY_QUERY, &object_attributes);

	if (NT_SUCCESS (status))
	{
		buffer_size = sizeof (OBJECT_DIRECTORY_INFORMATION) + (MAX_PATH * sizeof (WCHAR));
		directory_entry = _r_mem_allocatezero (buffer_size);

		is_firsttime = TRUE;
		query_context = 0;

		while (TRUE)
		{
			while (TRUE)
			{
				status = NtQueryDirectoryObject (
					directory_handle,
					directory_entry,
					buffer_size,
					FALSE,
					is_firsttime,
					&query_context,
					NULL
				);

				if (status != STATUS_MORE_ENTRIES)
					break;

				// Check if we have at least one entry. If not, we'll double the buffer size and try again.
				if (directory_entry[0].Name.Buffer)
					break;

				// Make sure we don't use too much memory.
				if (buffer_size > PR_SIZE_BUFFER_OVERFLOW)
				{
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				buffer_size *= 2;

				directory_entry = _r_mem_reallocatezero (directory_entry, buffer_size);
			}

			if (!NT_SUCCESS (status))
				break;

			i = 0;

			while (TRUE)
			{
				directory_info = &directory_entry[i];

				if (!directory_info->Name.Buffer)
					break;

				if (directory_info->Name.Length == (2 * sizeof (WCHAR)) && directory_info->Name.Buffer[1] == L':')
				{
					InitializeObjectAttributes (
						&object_attributes,
						&directory_info->Name,
						OBJ_CASE_INSENSITIVE | (_r_sys_isosversiongreaterorequal (WINDOWS_10) ? OBJ_DONT_REPARSE : 0),
						directory_handle,
						NULL
					);

					status = NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &object_attributes);

					if (NT_SUCCESS (status))
					{
						_r_obj_initializeunicodestring_ex (
							&device_prefix,
							device_prefix_buffer,
							0,
							RTL_NUMBER_OF (device_prefix_buffer) * sizeof (WCHAR)
						);

						status = NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL);

						if (NT_SUCCESS (status))
						{
							_r_obj_initializestringref4 (&device_prefix_sr, &device_prefix);

							if (_r_str_isstartswith (&path->sr, &device_prefix_sr, TRUE))
							{
								prefix_length = _r_str_getlength4 (&device_prefix);

								// To ensure we match the longest prefix, make sure the next character is a
								// backslash or the path is equal to the prefix.
								if (path->length == device_prefix.Length ||
									path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
								{
									// <letter>:path
									string = _r_obj_createstring_ex (
										NULL,
										(2 * sizeof (WCHAR)) + path->length - device_prefix.Length
									);

									string->buffer[0] = directory_info->Name.Buffer[0];
									string->buffer[1] = L':';

									RtlCopyMemory (
										&string->buffer[2],
										&path->buffer[prefix_length],
										path->length - device_prefix.Length
									);

									_r_obj_trimstringtonullterminator (string);

									_r_mem_free (directory_entry);

									NtClose (link_handle);
									NtClose (directory_handle);

									return string;
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

	return NULL;
}

// network share prefixes
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/support-for-unc-naming-and-mup

_Ret_maybenull_
PR_STRING _r_path_resolvenetworkprefix (
	_In_ PR_STRING path
)
{
	static R_STRINGREF services_part_sr = PR_STRINGREF_INIT (L"System\\CurrentControlSet\\Services\\");
	static R_STRINGREF provider_part_sr = PR_STRINGREF_INIT (L"\\NetworkProvider");

	HKEY hkey;
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING provider_order;
	PR_STRING service_key;
	PR_STRING device_name_string;
	PR_STRING string;
	SIZE_T prefix_length;
	LSTATUS status;

	status = RegOpenKeyEx (
		HKEY_LOCAL_MACHINE,
		L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order",
		0,
		KEY_READ,
		&hkey
	);

	if (status != ERROR_SUCCESS)
		return NULL;

	provider_order = _r_reg_querystring (hkey, NULL, L"ProviderOrder");

	if (provider_order)
	{
		_r_obj_initializestringref2 (&remaining_part, provider_order);

		while (remaining_part.length != 0)
		{
			_r_str_splitatchar (&remaining_part, L',', &first_part, &remaining_part);

			if (first_part.length != 0)
			{
				service_key = _r_obj_concatstringrefs (
					3,
					&services_part_sr,
					&first_part,
					&provider_part_sr
				);

				if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, service_key->buffer, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					device_name_string = _r_reg_querystring (hkey, NULL, L"DeviceName");

					if (device_name_string)
					{
						if (_r_str_isstartswith (&path->sr, &device_name_string->sr, TRUE))
						{
							prefix_length = _r_str_getlength2 (device_name_string);

							// To ensure we match the longest prefix, make sure the next character is a
							// backslash. Don't resolve if the name *is* the prefix. Otherwise, we will end
							// up with a useless string like "\".
							if (path->length != device_name_string->length &&
								path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
							{
								// \path
								string = _r_obj_createstring_ex (
									NULL,
									sizeof (WCHAR) + path->length - device_name_string->length
								);

								string->buffer[0] = OBJ_NAME_PATH_SEPARATOR;

								RtlCopyMemory (
									&string->buffer[1],
									&path->buffer[prefix_length],
									path->length - device_name_string->length
								);

								_r_obj_trimstringtonullterminator (string);

								_r_obj_dereference (provider_order);
								_r_obj_dereference (service_key);
								_r_obj_dereference (device_name_string);

								RegCloseKey (hkey);

								return string;
							}
						}

						_r_obj_dereference (device_name_string);
					}

					RegCloseKey (hkey);
				}

				_r_obj_dereference (service_key);
			}
		}

		_r_obj_dereference (provider_order);
	}

	RegCloseKey (hkey);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_path_search (
	_In_ LPCWSTR path,
	_In_opt_ LPCWSTR extension,
	_In_ BOOLEAN is_dontcheckattributes
)
{
	PR_STRING full_path;
	ULONG buffer_length;
	ULONG return_length;
	ULONG attributes;

	buffer_length = 256;
	full_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = SearchPath (
		NULL,
		path,
		extension,
		buffer_length,
		full_path->buffer,
		NULL
	);

	if (return_length == 0 && return_length <= buffer_length)
		goto CleanupExit;

	if (return_length > buffer_length)
	{
		buffer_length = return_length;

		_r_obj_dereference (full_path);
		full_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

		return_length = SearchPath (
			NULL,
			path,
			extension,
			buffer_length,
			full_path->buffer,
			NULL
		);
	}

	if (return_length == 0 && return_length <= buffer_length)
		goto CleanupExit;

	_r_obj_setstringlength (full_path, return_length * sizeof (WCHAR));

	if (!is_dontcheckattributes)
	{
		attributes = GetFileAttributes (full_path->buffer);

		// Make sure this file is exists.
		if (attributes == INVALID_FILE_ATTRIBUTES)
			goto CleanupExit;

		// Make sure this is not a directory.
		if (attributes & FILE_ATTRIBUTE_DIRECTORY)
			goto CleanupExit;
	}

	return full_path;

CleanupExit:

	_r_obj_dereference (full_path);

	return NULL;
}

PR_STRING _r_path_dospathfromnt (
	_In_ PR_STRING path
)
{
	R_STRINGREF system_root;
	PR_STRING string;
	SIZE_T path_length;

	path_length = _r_str_getlength2 (path);

	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_r_str_isstartswith2 (&path->sr, L"\\??\\", TRUE))
	{
		string = _r_obj_createstring_ex (NULL, path->length - 4 * sizeof (WCHAR));

		RtlCopyMemory (
			string->buffer,
			&path->buffer[4],
			path->length - 4 * sizeof (WCHAR)
		);

		return string;
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_r_str_isstartswith2 (&path->sr, L"\\systemroot", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + path->length - 11 * sizeof (WCHAR));

		RtlCopyMemory (
			string->buffer,
			system_root.buffer,
			system_root.length
		);

		RtlCopyMemory (
			PTR_ADD_OFFSET (string->buffer, system_root.length),
			&path->buffer[11], path->length - 11 * sizeof (WCHAR)
		);

		return string;
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_r_str_isstartswith2 (&path->sr, L"system32\\", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + sizeof (UNICODE_NULL) + path->length);

		RtlCopyMemory (
			string->buffer,
			system_root.buffer,
			system_root.length
		);

		string->buffer[_r_str_getlength3 (&system_root)] = OBJ_NAME_PATH_SEPARATOR;

		RtlCopyMemory (
			PTR_ADD_OFFSET (string->buffer, system_root.length + sizeof (WCHAR)),
			path->buffer,
			path->length
		);

		return string;

	}
#ifdef _WIN64
	// "SysWOW64\" means "C:\Windows\SysWOW64\".
	else if (_r_str_isstartswith2 (&path->sr, L"SysWOW64\\", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + sizeof (UNICODE_NULL) + path->length);

		RtlCopyMemory (
			string->buffer,
			system_root.buffer,
			system_root.length
		);

		string->buffer[_r_str_getlength3 (&system_root)] = OBJ_NAME_PATH_SEPARATOR;

		RtlCopyMemory (
			PTR_ADD_OFFSET (string->buffer, system_root.length + sizeof (UNICODE_NULL)),
			path->buffer, path->length
		);

		return string;
	}
#endif
	else if (_r_str_isstartswith2 (&path->sr, L"\\device\\", TRUE))
	{
		if (_r_str_isstartswith2 (&path->sr, L"\\device\\mup", TRUE)) // network share (win7+)
		{
			if (path_length != 11 && path->buffer[11] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				string = _r_obj_concatstrings (
					2,
					L"\\",
					path->buffer + 11
				);

				return string;
			}
		}
		else if (_r_str_isstartswith2 (&path->sr, L"\\device\\lanmanredirector", TRUE)) // network share (winxp+)
		{
			if (path_length != 24 && path->buffer[24] == OBJ_NAME_PATH_SEPARATOR)
			{
				// \\path
				string = _r_obj_concatstrings (
					2,
					L"\\",
					path->buffer + 24
				);

				return string;
			}
		}
		else
		{
			string = _r_path_resolvedeviceprefix (path);

			if (string)
				return string;

			string = _r_path_resolvedeviceprefix_workaround (path);

			if (string)
				return string;

			string = _r_path_resolvenetworkprefix (path);

			if (string)
				return string;
		}
	}

	return _r_obj_reference (path);
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_path_ntpathfromdos (
	_In_ PR_STRING path,
	_Out_ PR_STRING_PTR out_buffer
)
{
	POBJECT_NAME_INFORMATION obj_name_info;
	HANDLE hfile;
	PR_STRING string;
	ULONG buffer_size;
	ULONG attempts;
	NTSTATUS status;

	hfile = CreateFile (
		path->buffer,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
		NULL
	);

	if (!_r_fs_isvalidhandle (hfile))
	{
		status = RtlGetLastNtStatus ();

		*out_buffer = NULL;

		return status;
	}

	buffer_size = sizeof (OBJECT_NAME_INFORMATION) + (MAX_PATH * sizeof (WCHAR));
	obj_name_info = _r_mem_allocatezero (buffer_size);

	attempts = 6;

	do
	{
		status = NtQueryObject (hfile, ObjectNameInformation, obj_name_info, buffer_size, &buffer_size);

		if (status == STATUS_BUFFER_OVERFLOW ||
			status == STATUS_INFO_LENGTH_MISMATCH ||
			status == STATUS_BUFFER_TOO_SMALL)
		{
			if (buffer_size > PR_SIZE_BUFFER_OVERFLOW)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			obj_name_info = _r_mem_reallocatezero (obj_name_info, buffer_size);
		}
		else
		{
			break;
		}
	}
	while (--attempts);

	if (status == STATUS_SUCCESS)
	{
		string = _r_obj_createstring4 (&obj_name_info->Name);

		_r_str_tolower (&string->sr); // lower is important!

		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (obj_name_info);

	NtClose (hfile);

	return status;
}

//
// Shell
//

VOID _r_shell_showfile (
	_In_ LPCWSTR path
)
{
	LPITEMIDLIST item;
	PR_STRING string;
	SFGAOF attributes;
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
		string = _r_obj_concatstrings (
			3,
			L"\"explorer.exe\" /select,\"",
			path,
			L"\""
		);

		_r_sys_createprocess (NULL, string->buffer, NULL);

		_r_obj_dereference (string);
	}
}

//
// Strings
//

VOID _r_str_append (
	_Inout_updates_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ LPCWSTR string
)
{
	SIZE_T dest_length;

	if (buffer_size <= PR_SIZE_MAX_STRING_LENGTH)
	{
		dest_length = _r_str_getlength_ex (buffer, buffer_size + 1);

		_r_str_copy (buffer + dest_length, buffer_size - dest_length, string);
	}
}

VOID _r_str_appendformat (
	_Inout_updates_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	SIZE_T dest_length;

	if (buffer_size > PR_SIZE_MAX_STRING_LENGTH)
		return;

	dest_length = _r_str_getlength_ex (buffer, buffer_size + 1);

	va_start (arg_ptr, format);

	_r_str_printf_v (
		buffer + dest_length,
		buffer_size - dest_length,
		format,
		arg_ptr
	);

	va_end (arg_ptr);
}

_Check_return_
INT _r_str_compare (
	_In_ LPCWSTR string1,
	_In_ LPCWSTR string2
)
{
	return _wcsicmp (string1, string2);
}

_Check_return_
INT _r_str_compare_length (
	_In_ LPCWSTR string1,
	_In_ LPCWSTR string2,
	_In_ SIZE_T length
)
{
	return _wcsnicmp (string1, string2, length);
}

_Check_return_
INT _r_str_compare_logical (
	_In_ PR_STRING string1,
	_In_ PR_STRING string2
)
{
	return StrCmpLogicalW (string1->buffer, string2->buffer);
}

VOID _r_str_copy (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ LPCWSTR string
)
{
	if (buffer_size && buffer_size <= PR_SIZE_MAX_STRING_LENGTH)
	{
		while (buffer_size && (*string != UNICODE_NULL))
		{
			*buffer++ = *string++;
			buffer_size -= 1;
		}

		if (!buffer_size)
			buffer -= 1; // truncate buffer
	}

	*buffer = UNICODE_NULL;
}

VOID _r_str_copystring (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ PR_STRINGREF string
)
{
	SIZE_T length;

	length = _r_str_getlength3 (string) + 1;

	if (length > buffer_size)
		length = buffer_size;

	_r_str_copy (buffer, length, string->buffer);
}

_Ret_maybenull_
PR_STRING _r_str_environmentexpandstring (
	_In_ PR_STRINGREF string
)
{
	UNICODE_STRING input_string;
	UNICODE_STRING output_string;
	PR_STRING buffer_string;
	ULONG buffer_size;
	NTSTATUS status;

	if (!_r_obj_initializeunicodestring3 (&input_string, string))
		return NULL;

	buffer_size = 512 * sizeof (WCHAR);

	buffer_string = _r_obj_createstring_ex (NULL, buffer_size);

	_r_obj_initializeunicodestring_ex (
		&output_string,
		buffer_string->buffer,
		0,
		(USHORT)buffer_size
	);

	status = RtlExpandEnvironmentStrings_U (NULL, &input_string, &output_string, &buffer_size);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		_r_obj_dereference (buffer_string);
		buffer_string = _r_obj_createstring_ex (NULL, buffer_size);

		_r_obj_initializeunicodestring_ex (
			&output_string,
			buffer_string->buffer,
			0,
			(USHORT)buffer_size
		);

		status = RtlExpandEnvironmentStrings_U (NULL, &input_string, &output_string, &buffer_size);
	}

	if (NT_SUCCESS (status))
	{
		_r_obj_setstringlength (buffer_string, output_string.Length); // terminate

		return buffer_string;
	}

	_r_obj_dereference (buffer_string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_str_environmentunexpandstring (
	_In_ LPCWSTR string
)
{
	PR_STRING buffer;
	ULONG buffer_length;

	buffer_length = 512;
	buffer = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	if (PathUnExpandEnvStrings (string, buffer->buffer, buffer_length))
	{
		_r_obj_trimstringtonullterminator (buffer);

		return buffer;
	}

	_r_obj_dereference (buffer);

	return _r_obj_createstring (string);
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	SIZE_T length;
	WCHAR chr;

	if (!string->length)
		return SIZE_MAX;

	buffer = string->buffer;
	length = _r_str_getlength3 (string);

	if (is_ignorecase)
		character = _r_str_upper (character);

	do
	{
		if (is_ignorecase)
		{
			chr = _r_str_upper (*buffer);
		}
		else
		{
			chr = *buffer;
		}

		if (chr == character)
			return _r_str_getlength3 (string) - length;

		buffer += 1;
	}
	while (--length);

	return SIZE_MAX;
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findlastchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	SIZE_T length;
	WCHAR chr;

	if (!string->length)
		return SIZE_MAX;

	buffer = PTR_ADD_OFFSET (string->buffer, string->length);
	length = _r_str_getlength3 (string);

	if (is_ignorecase)
		character = _r_str_upper (character);

	buffer -= 1;

	do
	{
		if (is_ignorecase)
		{
			chr = _r_str_upper (*buffer);
		}
		else
		{
			chr = *buffer;
		}

		if (chr == character)
			return length - 1;

		buffer -= 1;
	}
	while (--length);

	return SIZE_MAX;
}

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findstring (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF sub_string,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	SIZE_T length1;
	SIZE_T length2;

	WCHAR chr1;
	WCHAR chr2;

	length1 = _r_str_getlength3 (string);
	length2 = _r_str_getlength3 (sub_string);

	// Can't be a substring if it's bigger than the first string.
	if (length2 > length1)
		return SIZE_MAX;

	// We always get a match if the substring is zero-length.
	if (length2 == 0)
		return 0;

	_r_obj_initializestringref_ex (
		&sr1,
		string->buffer,
		sub_string->length - sizeof (WCHAR)
	);

	_r_obj_initializestringref_ex (
		&sr2,
		sub_string->buffer,
		sub_string->length - sizeof (WCHAR)
	);

	if (is_ignorecase)
	{
		chr2 = _r_str_upper (*sr2.buffer++);
	}
	else
	{
		chr2 = *sr2.buffer++;
	}

	for (SIZE_T i = length1 - length2 + 1; i != 0; i--)
	{
		if (is_ignorecase)
		{
			chr1 = _r_str_upper (*sr1.buffer++);
		}
		else
		{
			chr1 = *sr1.buffer++;
		}

		if (chr2 == chr1)
		{
			if (_r_str_isequal (&sr1, &sr2, is_ignorecase))
				return (SIZE_T)(sr1.buffer - string->buffer - 1);
		}
	}

	return SIZE_MAX;
}

PR_STRING _r_str_formatversion (
	_In_ PR_STRING string
)
{
	SYSTEMTIME st;

	if (_r_str_isnumeric (&string->sr))
	{
		_r_unixtime_to_systemtime (_r_str_tolong64 (&string->sr), &st);

		return _r_format_string (
			L"%02d-%02d-%04d %02d:%02d:%02d",
			st.wDay,
			st.wMonth,
			st.wYear,
			st.wHour,
			st.wMinute,
			st.wSecond
		);
	}

	return _r_obj_reference (string);
}

VOID _r_str_fromlong (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ LONG value
)
{
	_r_str_printf (buffer, buffer_size, L"%" TEXT (PR_LONG), value);
}

VOID _r_str_fromlong64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ LONG64 value
)
{
	_r_str_printf (buffer, buffer_size, L"%" TEXT (PR_LONG64), value);
}

VOID _r_str_fromulong (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ ULONG value
)
{
	_r_str_printf (buffer, buffer_size, L"%" TEXT (PR_ULONG), value);
}

VOID _r_str_fromulong64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ ULONG64 value
)
{
	_r_str_printf (buffer, buffer_size, L"%" TEXT (PR_ULONG64), value);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_fromguid (
	_In_ LPCGUID guid,
	_In_ BOOLEAN is_uppercase,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING us;
	PR_STRING string;
	NTSTATUS status;

	status = RtlStringFromGUID ((LPGUID)guid, &us);

	if (NT_SUCCESS (status))
	{
		string = _r_obj_createstring4 (&us);

		if (is_uppercase)
			_r_str_toupper (&string->sr);

		*out_buffer = string;

		RtlFreeUnicodeString (&us);
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

PR_STRING _r_str_fromhex (
	_In_reads_bytes_ (length) PUCHAR buffer,
	_In_ SIZE_T length,
	_In_ BOOLEAN is_uppercase
)
{
	static CHAR integer_char_upper_table[69] =
		"0123456789" // 0 - 9
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 10 - 35
		" !\"#$%&'()*+,-./" // 36 - 51
		":;<=>?@" // 52 - 58
		"[\\]^_`" // 59 - 64
		"{|}~" // 65 - 68
		;

	static CHAR integer_char_table[69] =
		"0123456789" // 0 - 9
		"abcdefghijklmnopqrstuvwxyz" // 10 - 35
		" !\"#$%&'()*+,-./" // 36 - 51
		":;<=>?@" // 52 - 58
		"[\\]^_`" // 59 - 64
		"{|}~" // 65 - 68
		;

	PR_STRING string;
	PCHAR table;

	table = is_uppercase ? integer_char_upper_table : integer_char_table;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR) * 2);

	for (SIZE_T i = 0; i < length; i++)
	{
		string->buffer[i * sizeof (WCHAR)] = table[buffer[i] >> 4];
		string->buffer[i * sizeof (WCHAR) + 1] = table[buffer[i] & 0xf];
	}

	return string;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_str_fromsecuritydescriptor (
	_In_ PSECURITY_DESCRIPTOR security_descriptor,
	_In_ SECURITY_INFORMATION security_information,
	_Out_ PR_STRING_PTR out_buffer
)
{
	LPWSTR security_string;
	PR_STRING string;
	ULONG length;
	BOOL result;

	result = ConvertSecurityDescriptorToStringSecurityDescriptor (
		security_descriptor,
		SDDL_REVISION,
		security_information,
		&security_string,
		&length
	);

	if (!result)
	{
		*out_buffer = NULL;

		return GetLastError ();
	}

	string = _r_obj_createstring_ex (security_string, length * sizeof (WCHAR));

	*out_buffer = string;

	LocalFree (security_string);

	return ERROR_SUCCESS;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_fromsid (
	_In_ PSID sid,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING us;
	PR_STRING string;
	NTSTATUS status;

	string = _r_obj_createstring_ex (NULL, SECURITY_MAX_SID_STRING_CHARACTERS * sizeof (WCHAR));

	_r_obj_initializeunicodestring2 (&us, string);

	status = RtlConvertSidToUnicodeString (&us, sid, FALSE);

	if (status == STATUS_SUCCESS)
	{
		_r_obj_setstringlength (string, us.Length); // terminate

		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (string);
	}

	return status;
}

VOID _r_str_generaterandom (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ BOOLEAN is_uppercase
)
{
	WCHAR chr;

	chr = is_uppercase ? L'A' : L'a';

	for (SIZE_T i = 0; i < buffer_size - 1; i++)
	{
		buffer[i] = chr + (_r_math_getrandom () % 26);
	}

	buffer[buffer_size - 1] = UNICODE_NULL;
}

ULONG _r_str_gethash (
	_In_ LPCWSTR string,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, string);

	return _r_str_gethash3 (&sr, is_ignorecase);
}

ULONG _r_str_gethash2 (
	_In_ PR_STRING string,
	_In_ BOOLEAN is_ignorecase
)
{
	return _r_str_gethash3 (&string->sr, is_ignorecase);
}

ULONG _r_str_gethash3 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	return _r_str_x65599 (string, is_ignorecase);
}

SIZE_T _r_str_getlength (
	_In_ LPCWSTR string
)
{
	return _r_str_getlength_ex (string, PR_SIZE_MAX_STRING_LENGTH);
}

SIZE_T _r_str_getlength2 (
	_In_ PR_STRING string
)
{
	assert (!(string->length & 0x01));

	return string->length / sizeof (WCHAR);
}

SIZE_T _r_str_getlength3 (
	_In_ PR_STRINGREF string
)
{
	assert (!(string->length & 0x01));

	return string->length / sizeof (WCHAR);
}

SIZE_T _r_str_getlength4 (
	_In_ PUNICODE_STRING string
)
{
	assert (!(string->Length & 0x01));

	return string->Length / sizeof (WCHAR);
}

SIZE_T _r_str_getlength_ex (
	_In_reads_or_z_ (max_count) LPCWSTR string,
	_In_ SIZE_T max_count
)
{
	return wcsnlen_s (string, max_count);
}

SIZE_T _r_str_getbytelength (
	_In_ LPCSTR string
)
{
	return _r_str_getbytelength_ex (string, PR_SIZE_MAX_STRING_LENGTH);
}

SIZE_T _r_str_getbytelength2 (
	_In_ PR_BYTE string
)
{
	return string->length;
}

SIZE_T _r_str_getbytelength3 (
	_In_ PR_BYTEREF string
)
{
	return string->length;
}

SIZE_T _r_str_getbytelength_ex (
	_In_reads_or_z_ (max_count) LPCSTR string,
	_In_ SIZE_T max_count
)
{
	return strnlen_s (string, max_count);
}

BOOLEAN _r_str_isdigit (
	_In_ WCHAR chr
)
{
	return (USHORT)(chr - L'0') < 10;
}

BOOLEAN _r_str_isequal (
	_In_ PR_STRINGREF string1,
	_In_ PR_STRINGREF string2,
	_In_ BOOLEAN is_ignorecase
)
{
	LPCWSTR buffer1;
	LPCWSTR buffer2;
	SIZE_T length1;
	SIZE_T length2;
	SIZE_T length;
	WCHAR chr1;
	WCHAR chr2;

	length1 = string1->length;
	length2 = string2->length;

	assert (!(length1 & 0x01));
	assert (!(length2 & 0x01));

	if (length1 != length2)
		return FALSE;

	buffer1 = string1->buffer;
	buffer2 = string2->buffer;

#if !defined(_ARM64_)
	if (USER_SHARED_DATA->ProcessorFeatures[PF_XMMI64_INSTRUCTIONS_AVAILABLE])
	{
		length = length1 / 16;

		if (length != 0)
		{
			__m128i b1;
			__m128i b2;

			do
			{
				b1 = _mm_loadu_si128 ((__m128i *)buffer1);
				b2 = _mm_loadu_si128 ((__m128i *)buffer2);
				b1 = _mm_cmpeq_epi32 (b1, b2);

				if (_mm_movemask_epi8 (b1) != 0xffff)
				{
					if (!is_ignorecase)
					{
						return FALSE;
					}
					else
					{
						// Compare character-by-character to ignore case.
						length1 = length * 16 + (length1 & 15);
						length1 /= sizeof (WCHAR);

						goto CompareCharacters;
					}
				}

				buffer1 += 16 / sizeof (WCHAR);
				buffer2 += 16 / sizeof (WCHAR);
			}
			while (--length != 0);
		}

		// Compare character-by-character because we have no more 16-byte blocks to compare.
		length1 = (length1 & 15) / sizeof (WCHAR);
	}
	else
#endif // !_ARM64_
	{
		length = length1 / sizeof (ULONG_PTR);

		if (length != 0)
		{
			do
			{
				if (*(PULONG_PTR)buffer1 != *(PULONG_PTR)buffer2)
				{
					if (!is_ignorecase)
					{
						return FALSE;
					}
					else
					{
						// Compare character-by-character to ignore case.
						length1 = length * sizeof (ULONG_PTR) + (length1 & (sizeof (ULONG_PTR) - 1));
						length1 /= sizeof (WCHAR);

						goto CompareCharacters;
					}
				}

				buffer1 += sizeof (ULONG_PTR) / sizeof (WCHAR);
				buffer2 += sizeof (ULONG_PTR) / sizeof (WCHAR);
			}
			while (--length != 0);
		}

		// Compare character-by-character because we have no more ULONG_PTR blocks to compare.
		length1 = (length1 & (sizeof (ULONG_PTR) - 1)) / sizeof (WCHAR);
	}

CompareCharacters:
	if (length1 != 0)
	{
		do
		{
			if (!is_ignorecase)
			{
				chr1 = *buffer1;
				chr2 = *buffer2;
			}
			else
			{
				chr1 = _r_str_upper (*buffer1);
				chr2 = _r_str_upper (*buffer2);
			}

			if (chr1 != chr2)
				return FALSE;

			buffer1 += 1;
			buffer2 += 1;
		}
		while (--length1 != 0);
	}

	return TRUE;
}

BOOLEAN _r_str_isequal2 (
	_In_ PR_STRINGREF string1,
	_In_ LPCWSTR string2,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, string2);

	return _r_str_isequal (string1, &sr, is_ignorecase);
}

BOOLEAN _r_str_isnumeric (
	_In_ PR_STRINGREF string
)
{
	SIZE_T length;

	if (!string->length)
		return FALSE;

	length = _r_str_getlength3 (string);

	for (SIZE_T i = 0; i < length; i++)
	{
		if (!_r_str_isdigit (string->buffer[i]))
			return FALSE;
	}

	return TRUE;
}

BOOLEAN _r_str_isstartswith (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF prefix,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	if (string->length < prefix->length)
		return FALSE;

	_r_obj_initializestringref_ex (&sr, string->buffer, prefix->length);

	return _r_str_isequal (&sr, prefix, is_ignorecase);
}

BOOLEAN _r_str_isstartswith2 (
	_In_ PR_STRINGREF string,
	_In_ LPCWSTR prefix,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, prefix);

	return _r_str_isstartswith (string, &sr, is_ignorecase);
}

BOOLEAN _r_str_isendsswith (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF suffix,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	if (suffix->length > string->length)
		return FALSE;

	_r_obj_initializestringref_ex (
		&sr,
		(LPWSTR)PTR_ADD_OFFSET (string->buffer, string->length - suffix->length),
		suffix->length
	);

	return _r_str_isequal (&sr, suffix, is_ignorecase);
}

BOOLEAN _r_str_isendsswith2 (
	_In_ PR_STRINGREF string,
	_In_ LPCWSTR suffix,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, suffix);

	return _r_str_isendsswith (string, &sr, is_ignorecase);
}

_Success_ (return)
BOOLEAN _r_str_match (
	_In_ LPCWSTR string,
	_In_ LPCWSTR pattern,
	_In_ BOOLEAN is_ignorecase
)
{
	LPCWSTR s, p;
	BOOLEAN is_star = FALSE;

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
				is_star = TRUE;

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
	{
		p += 1;
	}

	return (!*p);

StarCheck:
	if (!is_star)
		return FALSE;

	string += 1;
	goto LoopStart;
}

VOID _r_str_printf (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	if (buffer_size > PR_SIZE_MAX_STRING_LENGTH)
	{
		*buffer = UNICODE_NULL;
		return;
	}

	va_start (arg_ptr, format);
	_r_str_printf_v (buffer, buffer_size, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_str_printf_v (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	SIZE_T max_length;
	LONG format_size;

	if (buffer_size > PR_SIZE_MAX_STRING_LENGTH)
	{
		*buffer = UNICODE_NULL;
		return;
	}

	// leave the last space for the null terminator
	max_length = buffer_size - 1;

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	format_size = _vsnwprintf (buffer, max_length, format, arg_ptr);
#pragma warning(pop)

	if (format_size == -1 || (SIZE_T)format_size >= max_length)
	{
		// need to null terminate the string
		buffer += max_length;
		*buffer = UNICODE_NULL;
	}
}

VOID _r_str_replacechar (
	_Inout_ PR_STRINGREF string,
	_In_ WCHAR char_from,
	_In_ WCHAR char_to
)
{
	SIZE_T length;

	if (!string->length)
		return;

	length = _r_str_getlength3 (string);

	for (SIZE_T i = 0; i < length; i++)
	{
		if (string->buffer[i] == char_from)
			string->buffer[i] = char_to;
	}
}

BOOLEAN _r_str_splitatchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR separator,
	_Out_ PR_STRINGREF first_part,
	_Out_ PR_STRINGREF second_part
)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findchar (string, separator, FALSE);

	if (index == SIZE_MAX)
	{
		_r_obj_initializestringref3 (first_part, string);
		_r_obj_initializestringrefempty (second_part);

		return FALSE;
	}

	_r_obj_initializestringref_ex (
		first_part,
		input.buffer,
		index * sizeof (WCHAR)
	);

	_r_obj_initializestringref_ex (
		second_part,
		PTR_ADD_OFFSET (input.buffer, index * sizeof (WCHAR) + sizeof (UNICODE_NULL)),
		input.length - index * sizeof (WCHAR) - sizeof (UNICODE_NULL)
	);

	return TRUE;
}

BOOLEAN _r_str_splitatlastchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR separator,
	_Out_ PR_STRINGREF first_part,
	_Out_ PR_STRINGREF second_part
)
{
	R_STRINGREF input;
	SIZE_T index;

	input = *string;
	index = _r_str_findlastchar (string, separator, FALSE);

	if (index == SIZE_MAX)
	{
		_r_obj_initializestringref3 (first_part, string);
		_r_obj_initializestringrefempty (second_part);

		return FALSE;
	}

	_r_obj_initializestringref_ex (
		first_part,
		input.buffer,
		index * sizeof (WCHAR)
	);

	_r_obj_initializestringref_ex (
		second_part,
		PTR_ADD_OFFSET (input.buffer, index * sizeof (WCHAR) + sizeof (UNICODE_NULL)),
		input.length - index * sizeof (WCHAR) - sizeof (UNICODE_NULL)
	);

	return TRUE;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_toguid (
	_In_ PR_STRINGREF string,
	_Out_ LPGUID guid
)
{
	UNICODE_STRING us;
	NTSTATUS status;

	if (!_r_obj_initializeunicodestring3 (&us, string))
		return STATUS_BUFFER_OVERFLOW;

	status = RtlGUIDFromString (&us, guid);

	return status;
}

_Ret_maybenull_
PR_BYTE _r_str_tosid (
	_In_ PR_STRING sid_string
)
{
	PR_BYTE bytes;
	PSID sid;
	ULONG length;

	if (!ConvertStringSidToSid (sid_string->buffer, &sid))
		return NULL;

	length = RtlLengthSid (sid);
	bytes = _r_obj_createbyte_ex (NULL, length);

	RtlCopyMemory (bytes->buffer, sid, length);

	LocalFree (sid);

	return bytes;
}

BOOLEAN _r_str_toboolean (
	_In_ PR_STRINGREF string
)
{
	static R_STRINGREF value = PR_STRINGREF_INIT (L"true");

	if (_r_str_tolong (string) != 0)
		return TRUE;

	return _r_str_isequal (string, &value, TRUE);
}

LONG _r_str_tolong (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	LONG number;

	if (_r_str_tointeger64 (string, 10, NULL, &value))
	{
		if (LongLongToLong (value, &number) == S_OK)
			return number;
	}

	return 0;
}

LONG64 _r_str_tolong64 (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;

	if (_r_str_tointeger64 (string, 10, NULL, &value))
		return value;

	return 0;
}

ULONG _r_str_toulong (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	ULONG number;

	if (_r_str_tointeger64 (string, 10, NULL, &value))
	{
		if (LongLongToULong (value, &number) == S_OK)
			return number;
	}

	return 0;
}

ULONG64 _r_str_toulong64 (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	ULONG64 number;

	if (_r_str_tointeger64 (string, 10, NULL, &value))
	{
		if (LongLongToULongLong (value, &number) == S_OK)
			return number;
	}

	return 0;
}

BOOLEAN _r_str_tointeger64 (
	_In_ PR_STRINGREF string,
	_In_opt_ ULONG base,
	_Out_opt_ PULONG new_base_ptr,
	_Out_ PLONG64 integer_ptr
)
{
	R_STRINGREF input;
	ULONG64 result;
	ULONG base_used;
	BOOLEAN is_negative;
	BOOLEAN is_valid;

	if (new_base_ptr)
		*new_base_ptr = 0;

	*integer_ptr = 0;

	if (!string->length)
		return FALSE;

	if (base > 69)
		return FALSE;

	input = *string;
	is_negative = FALSE;

	if ((input.buffer[0] == L'-' || input.buffer[0] == L'+'))
	{
		if (input.buffer[0] == L'-')
			is_negative = TRUE;

		_r_obj_skipstringlength (&input, sizeof (WCHAR));
	}

	if (base)
	{
		base_used = base;
	}
	else
	{
		base_used = 10;

		if (input.length >= 2 * sizeof (WCHAR) && input.buffer[0] == L'0')
		{
			switch (_r_str_lower (input.buffer[1]))
			{
				case L'x':
				{
					base_used = 16;
					break;
				}

				case L'o':
				{
					base_used = 8;
					break;
				}

				case L'b':
				{
					base_used = 2;
					break;
				}

				case L't': // ternary
				{
					base_used = 3;
					break;
				}

				case L'q': // quaternary
				{
					base_used = 4;
					break;
				}

				case L'w': // base 12
				{
					base_used = 12;
					break;
				}

				case L'r': // base 32
				{
					base_used = 32;
					break;
				}
			}

			if (base_used != 10)
				_r_obj_skipstringlength (&input, 2 * sizeof (WCHAR));
		}
	}

	if (new_base_ptr)
		*new_base_ptr = base_used;

	is_valid = _r_str_touinteger64 (&input, base_used, &result);

	*integer_ptr = is_negative ? -(LONG64)result : result;

	return is_valid;
}

BOOLEAN _r_str_touinteger64 (
	_In_ PR_STRINGREF string,
	_In_ ULONG base,
	_Out_ PULONG64 integer_ptr
)
{
	static const ULONG char_to_integer[256] =
	{
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0 - 15
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 16 - 31
		36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // ' ' - '/'
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, // '0' - '9'
		52, 53, 54, 55, 56, 57, 58, // ':' - '@'
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, // 'A' - 'Z
		59, 60, 61, 62, 63, 64, // '[' - '`'
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, // 'a' - 'z
		65, 66, 67, 68, -1, // '{' - 127
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 128 - 143
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 144 - 159
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 160 - 175
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 176 - 191
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 192 - 207
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 208 - 223
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 224 - 239
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 // 240 - 255
	};

	LPWSTR buffer;
	ULONG64 result;
	SIZE_T length;
	ULONG value;
	BOOLEAN is_valid;

	buffer = string->buffer;
	length = _r_str_getlength3 (string);

	result = 0;
	is_valid = TRUE;

	if (length)
	{
		do
		{
			value = char_to_integer[(UCHAR)*buffer];

			if (value < base)
			{
				result = result * base + value;
			}
			else
			{
				is_valid = FALSE;
			}

			buffer += 1;
		}
		while (--length);
	}

	*integer_ptr = result;

	return is_valid;
}

VOID _r_str_tolower (
	_Inout_ PR_STRINGREF string
)
{
	SIZE_T length;

	if (!string->length)
		return;

	length = _r_str_getlength3 (string);

	for (SIZE_T i = 0; i < length; i++)
	{
		string->buffer[i] = _r_str_lower (string->buffer[i]);
	}
}

VOID _r_str_toupper (
	_Inout_ PR_STRINGREF string
)
{
	SIZE_T length;

	if (!string->length)
		return;

	length = _r_str_getlength3 (string);

	for (SIZE_T i = 0; i < length; i++)
	{
		string->buffer[i] = _r_str_upper (string->buffer[i]);
	}
}

VOID _r_str_trimstring (
	_Inout_ PR_STRING string,
	_In_ PR_STRINGREF charset,
	_In_ ULONG flags
)
{
	_r_str_trimstringref (&string->sr, charset, flags);

	_r_obj_writestringnullterminator (string);
}

VOID _r_str_trimstring2 (
	_Inout_ PR_STRING string,
	_In_ LPCWSTR charset,
	_In_ ULONG flags
)
{
	_r_str_trimstringref2 (&string->sr, charset, flags);

	_r_obj_writestringnullterminator (string);
}

VOID _r_str_trimstringref (
	_Inout_ PR_STRINGREF string,
	_In_ PR_STRINGREF charset,
	_In_ ULONG flags
)
{
	LPCWSTR charset_buff;
	LPCWSTR buffer;

	BOOLEAN charset_table[256];
	BOOLEAN charset_table_complete;
	SIZE_T charset_count;
	SIZE_T trim_count;
	SIZE_T count;

	SIZE_T i;
	USHORT chr;

	if (!string->length || !charset->length)
		return;

	if (charset->length == sizeof (WCHAR))
	{
		chr = charset->buffer[0];

		if (!(flags & PR_TRIM_END_ONLY))
		{
			trim_count = 0;
			count = _r_str_getlength3 (string);
			buffer = string->buffer;

			while (count-- != 0)
			{
				if (*buffer++ != chr)
					break;

				trim_count += 1;
			}

			if (trim_count)
				_r_obj_skipstringlength (string, trim_count * sizeof (WCHAR));
		}

		if (!(flags & PR_TRIM_START_ONLY))
		{
			trim_count = 0;
			count = _r_str_getlength3 (string);
			buffer = PTR_ADD_OFFSET (string->buffer, string->length - sizeof (WCHAR));

			while (count-- != 0)
			{
				if (*buffer-- != chr)
					break;

				trim_count += 1;
			}

			if (trim_count)
				string->length -= trim_count * sizeof (WCHAR);
		}

		return;
	}

	charset_buff = charset->buffer;
	charset_count = _r_str_getlength3 (charset);

	RtlZeroMemory (charset_table, sizeof (charset_table));

	charset_table_complete = TRUE;

	for (i = 0; i < charset_count; i++)
	{
		chr = charset_buff[i];
		charset_table[chr & 0xFF] = TRUE;

		if (chr >= RTL_NUMBER_OF (charset_table))
			charset_table_complete = FALSE;
	}

	if (!(flags & PR_TRIM_END_ONLY))
	{
		trim_count = 0;
		count = _r_str_getlength3 (string);
		buffer = string->buffer;

		while (count-- != 0)
		{
			chr = *buffer++;

			if (!charset_table[chr & 0xFF])
				break;

			if (!charset_table_complete)
			{
				for (i = 0; i < charset_count; i++)
				{
					if (charset_buff[i] == chr)
						goto CharFound;
				}

				break;
			}

CharFound:
			trim_count += 1;
		}

		_r_obj_skipstringlength (string, trim_count * sizeof (WCHAR));
	}

	if (!(flags & PR_TRIM_START_ONLY))
	{
		trim_count = 0;
		count = _r_str_getlength3 (string);
		buffer = PTR_ADD_OFFSET (string->buffer, string->length - sizeof (WCHAR));

		while (count-- != 0)
		{
			chr = *buffer--;

			if (!charset_table[chr & 0xFF])
				break;

			if (!charset_table_complete)
			{
				for (i = 0; i < charset_count; i++)
				{
					if (charset_buff[i] == chr)
						goto CharFound2;
				}

				break;
			}

CharFound2:
			trim_count += 1;
		}

		string->length -= trim_count * sizeof (WCHAR);
	}
}

VOID _r_str_trimstringref2 (
	_Inout_ PR_STRINGREF string,
	_In_ LPCWSTR charset,
	_In_ ULONG flags
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, charset);

	_r_str_trimstringref (string, &sr, flags);
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_multibyte2unicode (
	_In_ PR_BYTEREF string,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING out_string;
	ULONG output_size;
	NTSTATUS status;

	status = RtlMultiByteToUnicodeSize (&output_size, string->buffer, (ULONG)string->length);

	if (status != STATUS_SUCCESS)
		return status;

	out_string = _r_obj_createstring_ex (NULL, output_size);

	status = RtlMultiByteToUnicodeN (
		out_string->buffer,
		(ULONG)out_string->length,
		NULL,
		string->buffer,
		(ULONG)string->length
	);

	if (status == STATUS_SUCCESS)
	{
		*out_buffer = out_string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (out_string);
	}

	return status;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_unicode2multibyte (
	_In_ PR_STRINGREF string,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE out_string;
	ULONG output_size;
	NTSTATUS status;

	status = RtlUnicodeToMultiByteSize (&output_size, string->buffer, (ULONG)string->length);

	if (status != STATUS_SUCCESS)
		return status;

	out_string = _r_obj_createbyte_ex (NULL, output_size);

	status = RtlUnicodeToMultiByteN (
		out_string->buffer,
		(ULONG)out_string->length,
		NULL,
		string->buffer,
		(ULONG)string->length
	);

	if (status == STATUS_SUCCESS)
	{
		*out_buffer = out_string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (out_string);
	}

	return status;
}

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (
	_In_ PR_STRINGREF string,
	_In_ WCHAR key_delimeter,
	_In_ WCHAR value_delimeter
)
{
	static R_STRINGREF whitespace = PR_STRINGREF_INIT (L"\r\n ");

	R_STRINGREF remaining_part;
	R_STRINGREF values_part;
	R_STRINGREF key_part;
	R_STRINGREF value_part;
	PR_HASHTABLE hashtable;
	ULONG_PTR hash_code;

	_r_obj_initializestringref3 (&remaining_part, string);

	hashtable = _r_obj_createhashtablepointer (4);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, key_delimeter, &values_part, &remaining_part);

		_r_str_trimstringref (&values_part, &whitespace, 0);

		if (_r_str_splitatchar (&values_part, value_delimeter, &key_part, &value_part))
		{
			// trim key string whitespaces
			_r_str_trimstringref (&key_part, &whitespace, 0);

			hash_code = _r_str_gethash3 (&key_part, TRUE);

			if (hash_code)
			{
				if (!_r_obj_findhashtable (hashtable, hash_code))
				{
					// trim value string whitespaces
					_r_str_trimstringref (&value_part, &whitespace, 0);

					_r_obj_addhashtablepointer (
						hashtable,
						hash_code,
						value_part.length ? _r_obj_createstring3 (&value_part) : NULL
					);
				}
			}
		}
	}

	if (!_r_obj_gethashtablesize (hashtable))
	{
		_r_obj_dereference (hashtable);

		return NULL;
	}

	return hashtable;
}

ULONG64 _r_str_versiontoulong64 (
	_In_ PR_STRINGREF version
)
{
	R_STRINGREF first_part;
	R_STRINGREF remaining_part;
	ULONG major_number;
	ULONG minor_number;
	ULONG build_number;
	ULONG revision_number;

	_r_str_splitatchar (version, L'.', &first_part, &remaining_part);
	major_number = _r_str_toulong (&first_part);

	_r_str_splitatchar (&remaining_part, L'.', &first_part, &remaining_part);
	minor_number = _r_str_toulong (&first_part);

	_r_str_splitatchar (&remaining_part, L'.', &first_part, &remaining_part);
	build_number = _r_str_toulong (&first_part);

	_r_str_splitatchar (&remaining_part, L'.', &first_part, &remaining_part);
	revision_number = _r_str_toulong (&first_part);

	return PR_MAKE_VERSION_ULONG64 (major_number, minor_number, build_number, revision_number);
}

// return 1 if v1 > v2
// return 0 if v1 = v2
// return -1 if v1 < v2

INT _r_str_versioncompare (
	_In_ PR_STRINGREF v1,
	_In_ PR_STRINGREF v2
)
{
	ULONG64 value_v1;
	ULONG64 value_v2;

	if (_r_str_isnumeric (v1) && _r_str_isnumeric (v2))
	{
		value_v1 = _r_str_toulong64 (v1);
		value_v2 = _r_str_toulong64 (v2);
	}
	else
	{
		value_v1 = _r_str_versiontoulong64 (v1);
		value_v2 = _r_str_versiontoulong64 (v2);
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

ULONG _r_str_crc32 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	static const ULONG crc32_table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
		0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
		0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
		0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
		0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
		0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
		0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
		0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
		0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
		0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
		0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
		0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
	};

	LPWSTR buffer;
	LPWSTR end_buffer;
	ULONG hash_code;
	WCHAR chr;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength3 (string);

	hash_code = ULONG_MAX;

	if (is_ignorecase)
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = _r_str_upper (*buffer);

			hash_code = (hash_code >> 8) ^ (crc32_table[(hash_code ^ (ULONG)chr) & 0xFF]);
		}
	}
	else
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = *buffer;

			hash_code = (hash_code >> 8) ^ (crc32_table[(hash_code ^ (ULONG)chr) & 0xFF]);
		}
	}

	return hash_code ^ ULONG_MAX;
}

ULONG64 _r_str_crc64 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	static const ULONG64 crc64_table[256] = {
		0x0000000000000000ULL, 0x7ad870c830358979ULL, 0xf5b0e190606b12f2ULL, 0x8f689158505e9b8bULL,
		0xc038e5739841b68fULL, 0xbae095bba8743ff6ULL, 0x358804e3f82aa47dULL, 0x4f50742bc81f2d04ULL,
		0xab28ecb46814fe75ULL, 0xd1f09c7c5821770cULL, 0x5e980d24087fec87ULL, 0x24407dec384a65feULL,
		0x6b1009c7f05548faULL, 0x11c8790fc060c183ULL, 0x9ea0e857903e5a08ULL, 0xe478989fa00bd371ULL,
		0x7d08ff3b88be6f81ULL, 0x07d08ff3b88be6f8ULL, 0x88b81eabe8d57d73ULL, 0xf2606e63d8e0f40aULL,
		0xbd301a4810ffd90eULL, 0xc7e86a8020ca5077ULL, 0x4880fbd87094cbfcULL, 0x32588b1040a14285ULL,
		0xd620138fe0aa91f4ULL, 0xacf86347d09f188dULL, 0x2390f21f80c18306ULL, 0x594882d7b0f40a7fULL,
		0x1618f6fc78eb277bULL, 0x6cc0863448deae02ULL, 0xe3a8176c18803589ULL, 0x997067a428b5bcf0ULL,
		0xfa11fe77117cdf02ULL, 0x80c98ebf2149567bULL, 0x0fa11fe77117cdf0ULL, 0x75796f2f41224489ULL,
		0x3a291b04893d698dULL, 0x40f16bccb908e0f4ULL, 0xcf99fa94e9567b7fULL, 0xb5418a5cd963f206ULL,
		0x513912c379682177ULL, 0x2be1620b495da80eULL, 0xa489f35319033385ULL, 0xde51839b2936bafcULL,
		0x9101f7b0e12997f8ULL, 0xebd98778d11c1e81ULL, 0x64b116208142850aULL, 0x1e6966e8b1770c73ULL,
		0x8719014c99c2b083ULL, 0xfdc17184a9f739faULL, 0x72a9e0dcf9a9a271ULL, 0x08719014c99c2b08ULL,
		0x4721e43f0183060cULL, 0x3df994f731b68f75ULL, 0xb29105af61e814feULL, 0xc849756751dd9d87ULL,
		0x2c31edf8f1d64ef6ULL, 0x56e99d30c1e3c78fULL, 0xd9810c6891bd5c04ULL, 0xa3597ca0a188d57dULL,
		0xec09088b6997f879ULL, 0x96d1784359a27100ULL, 0x19b9e91b09fcea8bULL, 0x636199d339c963f2ULL,
		0xdf7adabd7a6e2d6fULL, 0xa5a2aa754a5ba416ULL, 0x2aca3b2d1a053f9dULL, 0x50124be52a30b6e4ULL,
		0x1f423fcee22f9be0ULL, 0x659a4f06d21a1299ULL, 0xeaf2de5e82448912ULL, 0x902aae96b271006bULL,
		0x74523609127ad31aULL, 0x0e8a46c1224f5a63ULL, 0x81e2d7997211c1e8ULL, 0xfb3aa75142244891ULL,
		0xb46ad37a8a3b6595ULL, 0xceb2a3b2ba0eececULL, 0x41da32eaea507767ULL, 0x3b024222da65fe1eULL,
		0xa2722586f2d042eeULL, 0xd8aa554ec2e5cb97ULL, 0x57c2c41692bb501cULL, 0x2d1ab4dea28ed965ULL,
		0x624ac0f56a91f461ULL, 0x1892b03d5aa47d18ULL, 0x97fa21650afae693ULL, 0xed2251ad3acf6feaULL,
		0x095ac9329ac4bc9bULL, 0x7382b9faaaf135e2ULL, 0xfcea28a2faafae69ULL, 0x8632586aca9a2710ULL,
		0xc9622c4102850a14ULL, 0xb3ba5c8932b0836dULL, 0x3cd2cdd162ee18e6ULL, 0x460abd1952db919fULL,
		0x256b24ca6b12f26dULL, 0x5fb354025b277b14ULL, 0xd0dbc55a0b79e09fULL, 0xaa03b5923b4c69e6ULL,
		0xe553c1b9f35344e2ULL, 0x9f8bb171c366cd9bULL, 0x10e3202993385610ULL, 0x6a3b50e1a30ddf69ULL,
		0x8e43c87e03060c18ULL, 0xf49bb8b633338561ULL, 0x7bf329ee636d1eeaULL, 0x012b592653589793ULL,
		0x4e7b2d0d9b47ba97ULL, 0x34a35dc5ab7233eeULL, 0xbbcbcc9dfb2ca865ULL, 0xc113bc55cb19211cULL,
		0x5863dbf1e3ac9decULL, 0x22bbab39d3991495ULL, 0xadd33a6183c78f1eULL, 0xd70b4aa9b3f20667ULL,
		0x985b3e827bed2b63ULL, 0xe2834e4a4bd8a21aULL, 0x6debdf121b863991ULL, 0x1733afda2bb3b0e8ULL,
		0xf34b37458bb86399ULL, 0x8993478dbb8deae0ULL, 0x06fbd6d5ebd3716bULL, 0x7c23a61ddbe6f812ULL,
		0x3373d23613f9d516ULL, 0x49aba2fe23cc5c6fULL, 0xc6c333a67392c7e4ULL, 0xbc1b436e43a74e9dULL,
		0x95ac9329ac4bc9b5ULL, 0xef74e3e19c7e40ccULL, 0x601c72b9cc20db47ULL, 0x1ac40271fc15523eULL,
		0x5594765a340a7f3aULL, 0x2f4c0692043ff643ULL, 0xa02497ca54616dc8ULL, 0xdafce7026454e4b1ULL,
		0x3e847f9dc45f37c0ULL, 0x445c0f55f46abeb9ULL, 0xcb349e0da4342532ULL, 0xb1eceec59401ac4bULL,
		0xfebc9aee5c1e814fULL, 0x8464ea266c2b0836ULL, 0x0b0c7b7e3c7593bdULL, 0x71d40bb60c401ac4ULL,
		0xe8a46c1224f5a634ULL, 0x927c1cda14c02f4dULL, 0x1d148d82449eb4c6ULL, 0x67ccfd4a74ab3dbfULL,
		0x289c8961bcb410bbULL, 0x5244f9a98c8199c2ULL, 0xdd2c68f1dcdf0249ULL, 0xa7f41839ecea8b30ULL,
		0x438c80a64ce15841ULL, 0x3954f06e7cd4d138ULL, 0xb63c61362c8a4ab3ULL, 0xcce411fe1cbfc3caULL,
		0x83b465d5d4a0eeceULL, 0xf96c151de49567b7ULL, 0x76048445b4cbfc3cULL, 0x0cdcf48d84fe7545ULL,
		0x6fbd6d5ebd3716b7ULL, 0x15651d968d029fceULL, 0x9a0d8ccedd5c0445ULL, 0xe0d5fc06ed698d3cULL,
		0xaf85882d2576a038ULL, 0xd55df8e515432941ULL, 0x5a3569bd451db2caULL, 0x20ed197575283bb3ULL,
		0xc49581ead523e8c2ULL, 0xbe4df122e51661bbULL, 0x3125607ab548fa30ULL, 0x4bfd10b2857d7349ULL,
		0x04ad64994d625e4dULL, 0x7e7514517d57d734ULL, 0xf11d85092d094cbfULL, 0x8bc5f5c11d3cc5c6ULL,
		0x12b5926535897936ULL, 0x686de2ad05bcf04fULL, 0xe70573f555e26bc4ULL, 0x9ddd033d65d7e2bdULL,
		0xd28d7716adc8cfb9ULL, 0xa85507de9dfd46c0ULL, 0x273d9686cda3dd4bULL, 0x5de5e64efd965432ULL,
		0xb99d7ed15d9d8743ULL, 0xc3450e196da80e3aULL, 0x4c2d9f413df695b1ULL, 0x36f5ef890dc31cc8ULL,
		0x79a59ba2c5dc31ccULL, 0x037deb6af5e9b8b5ULL, 0x8c157a32a5b7233eULL, 0xf6cd0afa9582aa47ULL,
		0x4ad64994d625e4daULL, 0x300e395ce6106da3ULL, 0xbf66a804b64ef628ULL, 0xc5bed8cc867b7f51ULL,
		0x8aeeace74e645255ULL, 0xf036dc2f7e51db2cULL, 0x7f5e4d772e0f40a7ULL, 0x05863dbf1e3ac9deULL,
		0xe1fea520be311aafULL, 0x9b26d5e88e0493d6ULL, 0x144e44b0de5a085dULL, 0x6e963478ee6f8124ULL,
		0x21c640532670ac20ULL, 0x5b1e309b16452559ULL, 0xd476a1c3461bbed2ULL, 0xaeaed10b762e37abULL,
		0x37deb6af5e9b8b5bULL, 0x4d06c6676eae0222ULL, 0xc26e573f3ef099a9ULL, 0xb8b627f70ec510d0ULL,
		0xf7e653dcc6da3dd4ULL, 0x8d3e2314f6efb4adULL, 0x0256b24ca6b12f26ULL, 0x788ec2849684a65fULL,
		0x9cf65a1b368f752eULL, 0xe62e2ad306bafc57ULL, 0x6946bb8b56e467dcULL, 0x139ecb4366d1eea5ULL,
		0x5ccebf68aecec3a1ULL, 0x2616cfa09efb4ad8ULL, 0xa97e5ef8cea5d153ULL, 0xd3a62e30fe90582aULL,
		0xb0c7b7e3c7593bd8ULL, 0xca1fc72bf76cb2a1ULL, 0x45775673a732292aULL, 0x3faf26bb9707a053ULL,
		0x70ff52905f188d57ULL, 0x0a2722586f2d042eULL, 0x854fb3003f739fa5ULL, 0xff97c3c80f4616dcULL,
		0x1bef5b57af4dc5adULL, 0x61372b9f9f784cd4ULL, 0xee5fbac7cf26d75fULL, 0x9487ca0fff135e26ULL,
		0xdbd7be24370c7322ULL, 0xa10fceec0739fa5bULL, 0x2e675fb4576761d0ULL, 0x54bf2f7c6752e8a9ULL,
		0xcdcf48d84fe75459ULL, 0xb71738107fd2dd20ULL, 0x387fa9482f8c46abULL, 0x42a7d9801fb9cfd2ULL,
		0x0df7adabd7a6e2d6ULL, 0x772fdd63e7936bafULL, 0xf8474c3bb7cdf024ULL, 0x829f3cf387f8795dULL,
		0x66e7a46c27f3aa2cULL, 0x1c3fd4a417c62355ULL, 0x935745fc4798b8deULL, 0xe98f353477ad31a7ULL,
		0xa6df411fbfb21ca3ULL, 0xdc0731d78f8795daULL, 0x536fa08fdfd90e51ULL, 0x29b7d047efec8728ULL,
	};

	LPWSTR buffer;
	LPWSTR end_buffer;
	ULONG64 hash_code;
	WCHAR chr;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength3 (string);

	hash_code = 0;

	if (is_ignorecase)
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = _r_str_upper (*buffer);

			hash_code = (hash_code >> 8) ^ (crc64_table[(hash_code ^ (ULONG)chr) & 0xFF]);
		}
	}
	else
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = *buffer;

			hash_code = (hash_code >> 8) ^ (crc64_table[(hash_code ^ (ULONG)chr) & 0xFF]);
		}
	}

	return ~hash_code;
}

ULONG _r_str_fnv32a (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	LPWSTR end_buffer;
	ULONG hash_code;
	WCHAR chr;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength3 (string);

	hash_code = 2166136261UL;

	if (is_ignorecase)
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = _r_str_upper (*buffer);

			hash_code = (hash_code ^ (ULONG)chr) * 16777619U;
		}
	}
	else
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = *buffer;

			hash_code = (hash_code ^ (ULONG)chr) * 16777619U;
		}
	}

	return hash_code;
}

ULONG64 _r_str_fnv64a (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	LPWSTR end_buffer;
	ULONG64 hash_code;
	WCHAR chr;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength3 (string);

	hash_code = 14695981039346656037ULL;

	if (is_ignorecase)
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = _r_str_upper (*buffer);

			hash_code = (hash_code ^ (ULONG)chr) * 1099511628211LL;
		}
	}
	else
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			chr = *buffer;

			hash_code = (hash_code ^ (ULONG)chr) * 1099511628211LL;
		}
	}

	return hash_code;
}

ULONG _r_str_x65599 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	LPWSTR end_buffer;
	ULONG hash_code;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength3 (string);

	hash_code = 0;

	if (is_ignorecase)
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			hash_code = ((65599 * (hash_code)) + (ULONG)(((*buffer) >= L'a' &&
						 (*buffer) <= L'z') ? (*buffer) - L'a' + L'A' : (*buffer)));
		}
	}
	else
	{
		for (buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			hash_code = ((65599 * (hash_code)) + (ULONG)(*buffer));
		}
	}

	return hash_code;
}

//
// Performance
//

LONG64 _r_perf_getexecutionstart ()
{
	LONG64 current_time;

	current_time = _r_perf_querycounter ();

	return current_time;
}

DOUBLE _r_perf_getexecutionfinal (
	_In_ LONG64 start_time
)
{
	LONG64 current_time;
	LONG64 frequency;

	current_time = _r_perf_querycounter ();
	frequency = _r_perf_queryfrequency ();

	return ((current_time - start_time) * 1000.0) / frequency / 1000.0;
}

//
// System information
//

BOOLEAN _r_sys_iselevated ()
{
#if defined(APP_NO_DEPRECATIONS)
	return !!_r_sys_getcurrenttoken ().is_elevated;
#else
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_elevated = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		// winxp compatibility
		if (_r_sys_isosversionlower (WINDOWS_VISTA))
		{
			is_elevated = !!IsUserAnAdmin ();

			_r_initonce_end (&init_once);

			return is_elevated;
		}

		is_elevated = !!_r_sys_getcurrenttoken ().is_elevated;

		_r_initonce_end (&init_once);
	}

	return is_elevated;
#endif // !APP_NO_DEPRECATIONS
}

BOOLEAN _r_sys_isosversionequal (
	_In_ ULONG version
)
{
	ULONG windows_version;

	windows_version = _r_sys_getwindowsversion ();

	return windows_version == version;
}

BOOLEAN _r_sys_isosversiongreaterorequal (
	_In_ ULONG version
)
{
	ULONG windows_version;

	windows_version = _r_sys_getwindowsversion ();

	return windows_version >= version;
}

BOOLEAN _r_sys_isosversionlower (
	_In_ ULONG version
)
{
	ULONG windows_version;

	windows_version = _r_sys_getwindowsversion ();

	return windows_version < version;
}

BOOLEAN _r_sys_isosversionlowerorequal (
	_In_ ULONG version
)
{
	ULONG windows_version;

	windows_version = _r_sys_getwindowsversion ();

	return windows_version <= version;
}

BOOLEAN _r_sys_isprocessimmersive (
	_In_ HANDLE hprocess
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static IIP _IsImmersiveProcess = NULL;

	HINSTANCE huser32;

	if (_r_initonce_begin (&init_once))
	{
		huser32 = _r_sys_loadlibrary (L"user32.dll");

		if (huser32)
		{
			// win8+
			_IsImmersiveProcess = (IIP)GetProcAddress (huser32, "IsImmersiveProcess");

			FreeLibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	if (!_IsImmersiveProcess)
		return FALSE;

	return !!_IsImmersiveProcess (hprocess);
}

BOOLEAN _r_sys_iswine ()
{
	HINSTANCE hntdll;
	BOOLEAN is_wine;

	hntdll = _r_sys_loadlibrary (L"ntdll.dll");

	if (!hntdll)
		return FALSE;

	is_wine = (GetProcAddress (hntdll, "wine_get_version") != NULL);

	FreeLibrary (hntdll);

	return is_wine;
}

BOOLEAN _r_sys_iswow64 ()
{
#if !defined(_WIN64)
	ULONG_PTR iswow64;
	NTSTATUS status;

	status = NtQueryInformationProcess (
		NtCurrentProcess (),
		ProcessWow64Information,
		&iswow64,
		sizeof (ULONG_PTR),
		NULL
	);

	if (NT_SUCCESS (status))
		return !!iswow64;

#endif // _WIN64

	return FALSE;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_formatmessage (
	_In_ ULONG error_code,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ ULONG lang_id,
	_Out_ PR_STRING_PTR out_buffer
)
{
	static R_STRINGREF whitespace_sr = PR_STRINGREF_INIT (L"\r\n ");

	PR_STRING string;
	ULONG allocated_length;
	ULONG return_length;
	ULONG attempts;
	ULONG status;

	attempts = 6;
	allocated_length = 256;
	string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

	do
	{
		return_length = FormatMessage (
			FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
			hinstance,
			error_code,
			lang_id,
			string->buffer,
			allocated_length,
			NULL
		);

		if (return_length)
		{
			_r_obj_setstringlength (string, return_length * sizeof (WCHAR));

			_r_str_trimstring (string, &whitespace_sr, 0);

			*out_buffer = string;

			return ERROR_SUCCESS;
		}

		status = GetLastError ();

		_r_obj_dereference (string);

		if (status == ERROR_INSUFFICIENT_BUFFER)
		{
			allocated_length *= 2;

			if (allocated_length > PR_SIZE_BUFFER_OVERFLOW)
				break;

			string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));
		}
		else
		{
			if (lang_id)
				return _r_sys_formatmessage (error_code, hinstance, 0, out_buffer);

			break;
		}
	}
	while (--attempts);

	*out_buffer = NULL;

	return status;
}

R_TOKEN_ATTRIBUTES _r_sys_getcurrenttoken ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_TOKEN_ATTRIBUTES attributes = {0};

	HANDLE token_handle;
	TOKEN_ELEVATION elevation;
	TOKEN_ELEVATION_TYPE elevation_type;
	PTOKEN_USER token_user;
	ULONG return_length;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		attributes.elevation_type = TokenElevationTypeDefault;

		if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
		{
			attributes.token_handle = NtCurrentProcessToken ();
		}
		else
		{
			status = NtOpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &token_handle);

			if (NT_SUCCESS (status))
				attributes.token_handle = token_handle;
		}

		if (attributes.token_handle)
		{
			status = NtQueryInformationToken (
				attributes.token_handle,
				TokenElevation,
				&elevation,
				sizeof (elevation),
				&return_length
			);

			if (NT_SUCCESS (status))
				attributes.is_elevated = !!elevation.TokenIsElevated;

			status = NtQueryInformationToken (
				attributes.token_handle,
				TokenElevationType,
				&elevation_type,
				sizeof (elevation_type),
				&return_length
			);

			if (NT_SUCCESS (status))
				attributes.elevation_type = elevation_type;

			status = _r_sys_querytokeninformation (attributes.token_handle, TokenUser, &token_user);

			if (NT_SUCCESS (status))
			{
				attributes.token_sid = _r_mem_allocateandcopy (token_user->User.Sid, RtlLengthSid (token_user->User.Sid));

				_r_mem_free (token_user);
			}
		}

		_r_initonce_end (&init_once);
	}

	return attributes;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_getlocaleinfo (
	_In_ LCID locale_id,
	_In_ LCTYPE locale_type,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING string;
	ULONG return_length;
	ULONG status;

	return_length = GetLocaleInfo (locale_id, locale_type, NULL, 0);

	if (!return_length)
	{
		*out_buffer = NULL;

		return GetLastError ();
	}

	string = _r_obj_createstring_ex (NULL, return_length * sizeof (WCHAR) - sizeof (UNICODE_NULL));

	return_length = GetLocaleInfo (locale_id, locale_type, string->buffer, return_length);

	if (!return_length)
	{
		*out_buffer = NULL;

		status = GetLastError ();

		_r_obj_dereference (string);
	}
	else
	{
		*out_buffer = string;

		status = ERROR_SUCCESS;
	}

	return status;
}

VOID _r_sys_getsystemroot (
	_Out_ PR_STRINGREF path
)
{
	static R_STRINGREF system_root = {0};

	R_STRINGREF local_system_root;

	if (system_root.buffer)
	{
		*path = system_root;
		return;
	}

	_r_obj_initializestringref (&local_system_root, USER_SHARED_DATA->NtSystemRoot);

	// Make sure the system root string doesn't have a trailing backslash.
	if (local_system_root.buffer[_r_str_getlength3 (&local_system_root) - 1] == OBJ_NAME_PATH_SEPARATOR)
		local_system_root.length -= sizeof (WCHAR);

	*path = local_system_root;

	system_root.length = local_system_root.length;
	MemoryBarrier ();
	system_root.buffer = local_system_root.buffer;
}

PR_STRING _r_sys_getsystemdirectory ()
{
	static PR_STRING cached_path = NULL;

	R_STRINGREF system_root;
	R_STRINGREF system_path;

	PR_STRING current_path;
	PR_STRING new_path;

	current_path = InterlockedCompareExchangePointer (&cached_path, NULL, NULL);

	if (!current_path)
	{
		_r_sys_getsystemroot (&system_root);

		_r_obj_initializestringref (&system_path, L"\\system32");

		new_path = _r_obj_concatstringrefs (
			2,
			&system_root,
			&system_path
		);

		current_path = InterlockedCompareExchangePointer (&cached_path, new_path, NULL);

		if (!current_path)
		{
			current_path = new_path;
		}
		else
		{
			_r_obj_dereference (new_path);
		}
	}

	return current_path;
}

PR_STRING _r_sys_gettempdirectory ()
{
	static PR_STRING cached_path = NULL;

	PR_STRING current_path;
	PR_STRING new_path;

	ULONG buffer_length;
	ULONG return_length;

	current_path = InterlockedCompareExchangePointer (&cached_path, NULL, NULL);

	if (!current_path)
	{
		buffer_length = 256;
		new_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

		return_length = GetTempPath (buffer_length, new_path->buffer);

		if (return_length > buffer_length)
		{
			buffer_length = return_length;

			_r_obj_dereference (new_path);
			new_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

			return_length = GetTempPath (buffer_length, new_path->buffer);
		}

		if (return_length)
		{
			// Remove trailing backslash
			if (new_path->buffer[return_length - 1] == OBJ_NAME_PATH_SEPARATOR)
				return_length -= 1;

			_r_obj_setstringlength (new_path, return_length * sizeof (WCHAR));
		}
		else
		{
			_r_obj_clearreference (&new_path);
		}

		current_path = InterlockedCompareExchangePointer (&cached_path, new_path, NULL);

		if (!current_path)
		{
			current_path = new_path;
		}
		else
		{
			_r_obj_dereference (new_path);
		}
	}

	return current_path;
}

BOOLEAN _r_sys_getopt (
	_In_ LPCWSTR args,
	_In_ LPCWSTR name,
	_Outptr_opt_result_maybenull_ PR_STRING_PTR out_value
)
{
	R_STRINGREF key_name;
	R_STRINGREF key_value;
	R_STRINGREF name_sr;
	LPWSTR *arga;
	LPWSTR buffer;
	SIZE_T option_length;
	INT numargs;
	BOOLEAN is_namefound;

	arga = CommandLineToArgvW (args, &numargs);

	if (out_value)
		*out_value = NULL;

	if (!arga)
		return FALSE;

	is_namefound = FALSE;

	_r_obj_initializestringrefconst (&name_sr, name);

	for (INT i = 0; i < numargs; i++)
	{
		_r_obj_initializestringref (&key_name, arga[i]);

		if (key_name.length < (2 * sizeof (WCHAR)))
			continue;

		if (*key_name.buffer != L'/' && *key_name.buffer != L'-')
			continue;

		// -option
		_r_obj_skipstringlength (&key_name, sizeof (WCHAR));

		// --long-option
		if (*key_name.buffer == L'-')
			_r_obj_skipstringlength (&key_name, sizeof (WCHAR));

		// parse key name
		if (_r_str_isstartswith (&key_name, &name_sr, TRUE))
		{
			option_length = _r_str_getlength3 (&name_sr);
			is_namefound = TRUE;
		}
		else
		{
			continue;
		}

		// parse key value
		_r_obj_initializestringrefempty (&key_value);

		if (key_name.buffer[option_length] != UNICODE_NULL)
		{
			if (key_name.buffer[option_length] == L':' ||
				key_name.buffer[option_length] == L'=' ||
				key_name.buffer[option_length] == L' ')
			{
				if (out_value)
				{
					_r_obj_initializestringref3 (&key_value, &key_name);
					_r_obj_skipstringlength (&key_value, (option_length + 1) * sizeof (WCHAR));
				}
			}
			else
			{
				continue;
			}
		}
		else
		{
			if (out_value)
			{
				if (numargs > (i + 1))
				{
					buffer = arga[i + 1];

					if (*buffer == L'/' || *buffer == L'-' || *buffer == L' ')
						continue;

					_r_obj_initializestringref (&key_value, buffer);
				}
			}
		}

		if (out_value)
		{
			if (!_r_obj_isstringempty2 (&key_value))
			{
				*out_value = _r_obj_createstring3 (&key_value);
			}
			else
			{
				continue;
			}

			break;
		}
		else
		{
			//is_namefound = TRUE;
			break;
		}
	}

	LocalFree (arga);

	return is_namefound;
}

_Success_ (return == ERROR_SUCCESS)
LONG _r_sys_getpackagepath (
	_In_ PR_STRING package_full_name,
	_Out_ PR_STRING_PTR out_buffer
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GSPPBF _GetStagedPackagePathByFullName = NULL;

	HINSTANCE hkernel32;
	PR_STRING string;
	UINT32 length;
	LONG status;

	if (_r_initonce_begin (&init_once))
	{
		hkernel32 = _r_sys_loadlibrary (L"kernel32.dll");

		if (hkernel32)
		{
			// win81+
			_GetStagedPackagePathByFullName = (GSPPBF)GetProcAddress (hkernel32, "GetStagedPackagePathByFullName");

			FreeLibrary (hkernel32);
		}

		_r_initonce_end (&init_once);
	}

	if (!_GetStagedPackagePathByFullName)
	{
		*out_buffer = NULL;

		return ERROR_NOT_SUPPORTED;
	}

	length = 0;
	status = _GetStagedPackagePathByFullName (package_full_name->buffer, &length, NULL);

	if (status != ERROR_INSUFFICIENT_BUFFER)
	{
		*out_buffer = NULL;

		return status;
	}

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));
	status = _GetStagedPackagePathByFullName (package_full_name->buffer, &length, string->buffer);

	if (status == ERROR_SUCCESS)
	{
		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (string);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getservicesid (
	_In_ PR_STRINGREF name,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	UNICODE_STRING service_name;
	ULONG sid_length;
	PR_BYTE sid;
	NTSTATUS status;

	_r_obj_initializeunicodestring3 (&service_name, name);

	sid = NULL;
	sid_length = 0;

	status = RtlCreateServiceSid (&service_name, sid, &sid_length);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		sid = _r_obj_createbyte_ex (NULL, sid_length);

		status = RtlCreateServiceSid (&service_name, sid->buffer, &sid_length);

		if (NT_SUCCESS (status))
		{
			*out_buffer = sid;
		}
		else
		{
			*out_buffer = NULL;

			_r_obj_dereference (sid);
		}
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_getsessioninfo (
	_In_ WTS_INFO_CLASS info_class,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING string;
	LPWSTR buffer;
	ULONG length;

	if (!WTSQuerySessionInformation (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, info_class, &buffer, &length))
	{
		*out_buffer = NULL;

		return GetLastError ();
	}

	string = _r_obj_createstring_ex (buffer, length * sizeof (WCHAR));

	*out_buffer = string;

	WTSFreeMemory (buffer);

	return ERROR_SUCCESS;
}

ULONG _r_sys_gettickcount ()
{
#ifdef _WIN64

	return (ULONG)((USER_SHARED_DATA->TickCountQuad * USER_SHARED_DATA->TickCountMultiplier) >> 24);

#else

	ULARGE_INTEGER tick_count;

	while (TRUE)
	{
		tick_count.HighPart = (ULONG)USER_SHARED_DATA->TickCount.High1Time;
		tick_count.LowPart = USER_SHARED_DATA->TickCount.LowPart;

		if (tick_count.HighPart == (ULONG)USER_SHARED_DATA->TickCount.High2Time)
			break;

		YieldProcessor ();
	}

	return (ULONG)((UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) +
				   UInt32x32To64 ((tick_count.HighPart << 8) & 0xffffffff, USER_SHARED_DATA->TickCountMultiplier));

#endif
}

ULONG64 _r_sys_gettickcount64 ()
{
	ULARGE_INTEGER tick_count;

#ifdef _WIN64

	tick_count.QuadPart = USER_SHARED_DATA->TickCountQuad;

#else

	while (TRUE)
	{
		tick_count.HighPart = (ULONG)USER_SHARED_DATA->TickCount.High1Time;
		tick_count.LowPart = USER_SHARED_DATA->TickCount.LowPart;

		if (tick_count.HighPart == (ULONG)USER_SHARED_DATA->TickCount.High2Time)
			break;

		YieldProcessor ();
	}

#endif

	return (UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) +
		(UInt32x32To64 (tick_count.HighPart, USER_SHARED_DATA->TickCountMultiplier) << 8);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getusernamefromsid (
	_In_ PSID sid,
	_Out_ PR_STRING_PTR out_buffer
)
{
	LSA_OBJECT_ATTRIBUTES object_attributes;
	LSA_HANDLE policy_handle;
	PLSA_REFERENCED_DOMAIN_LIST referenced_domains;
	PLSA_TRANSLATED_NAME names;
	R_STRINGBUILDER sb = {0};
	PLSA_TRUST_INFORMATION trust_info;
	BOOLEAN is_hasdomain;
	BOOLEAN is_hasname;
	NTSTATUS status;

	InitializeObjectAttributes (&object_attributes, NULL, 0, NULL, NULL);

	referenced_domains = NULL;
	names = NULL;

	status = LsaOpenPolicy (NULL, &object_attributes, POLICY_LOOKUP_NAMES, &policy_handle);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = LsaLookupSids (policy_handle, 1, &sid, &referenced_domains, &names);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	if (names[0].Use == SidTypeInvalid || names[0].Use == SidTypeUnknown)
	{
		status = STATUS_SOME_NOT_MAPPED;
		goto CleanupExit;
	}

	_r_obj_initializestringbuilder (&sb);

	is_hasdomain = (referenced_domains && names[0].DomainIndex >= 0);
	is_hasname = (names[0].Name.Buffer != NULL);

	if (is_hasdomain)
	{
		trust_info = &referenced_domains->Domains[names[0].DomainIndex];

		if (trust_info->Name.Buffer)
		{
			_r_obj_appendstringbuilder4 (&sb, &trust_info->Name);

			if (is_hasname)
				_r_obj_appendstringbuilder (&sb, L"\\");
		}
	}

	if (is_hasname)
		_r_obj_appendstringbuilder4 (&sb, &names[0].Name);

CleanupExit:

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_finalstringbuilder (&sb);
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_deletestringbuilder (&sb);
	}

	if (referenced_domains)
		LsaFreeMemory (referenced_domains);

	if (names)
		LsaFreeMemory (names);

	if (policy_handle)
		LsaClose (policy_handle);

	return status;
}

ULONG _r_sys_getwindowsversion ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ULONG windows_version = 0;

	RTL_OSVERSIONINFOEXW version_info;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		RtlZeroMemory (&version_info, sizeof (version_info));

		version_info.dwOSVersionInfoSize = sizeof (version_info);

		status = RtlGetVersion (&version_info);

		if (NT_SUCCESS (status))
		{
			if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 0)
			{
				windows_version = WINDOWS_2000;
			}
			else if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 1)
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
				if (version_info.dwBuildNumber >= 22567)
				{
					windows_version = WINDOWS_11_22H2;
				}
				else if (version_info.dwBuildNumber >= 22000)
				{
					windows_version = WINDOWS_11_21H2;
				}
				else if (version_info.dwBuildNumber >= 20348)
				{
					windows_version = WINDOWS_10_21H2_SERVER;
				}
				else if (version_info.dwBuildNumber >= 19044)
				{
					windows_version = WINDOWS_10_21H2;
				}
				else if (version_info.dwBuildNumber >= 19043)
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
					windows_version = WINDOWS_10_1507;
				}
				else
				{
					windows_version = WINDOWS_10;
				}
			}
			else
			{
				windows_version = ULONG_MAX;
			}
		}

		_r_initonce_end (&init_once);
	}

	return windows_version;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_compressbuffer (
	_In_ USHORT format,
	_In_ PR_BYTEREF buffer,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE tmp_buffer;
	PVOID ws_buffer;
	ULONG ws_buffer_length;
	ULONG ws_fragment_length;
	ULONG allocation_length;
	ULONG return_length;
	NTSTATUS status;

	status = RtlGetCompressionWorkSpaceSize (format, &ws_buffer_length, &ws_fragment_length);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	ws_buffer = _r_mem_allocatezero (ws_buffer_length);

	allocation_length = (ULONG)(buffer->length);

	tmp_buffer = _r_obj_createbyte_ex (NULL, allocation_length);

	status = RtlCompressBuffer (
		format,
		buffer->buffer,
		(ULONG)buffer->length,
		tmp_buffer->buffer,
		allocation_length,
		4096,
		&return_length,
		ws_buffer
	);

	if (NT_SUCCESS (status))
	{
		tmp_buffer->length = return_length;

		*out_buffer = tmp_buffer;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (tmp_buffer);
	}

	_r_mem_free (ws_buffer);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_decompressbuffer (
	_In_ USHORT format,
	_In_ PR_BYTEREF buffer,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE tmp_buffer;
	ULONG allocation_length;
	ULONG return_length;
	ULONG attempts;
	NTSTATUS status;

	allocation_length = (ULONG)(buffer->length) * 2;

	if (allocation_length > PR_SIZE_BUFFER_OVERFLOW)
	{
		*out_buffer = NULL;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	tmp_buffer = _r_obj_createbyte_ex (NULL, allocation_length);

	status = RtlDecompressBuffer (
		format,
		tmp_buffer->buffer,
		allocation_length,
		buffer->buffer,
		(ULONG)buffer->length,
		&return_length
	);

	if (status == STATUS_BAD_COMPRESSION_BUFFER)
	{
		attempts = 6;

		do
		{
			allocation_length *= 2;

			if (allocation_length > PR_SIZE_BUFFER_OVERFLOW)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			_r_obj_dereference (tmp_buffer);
			tmp_buffer = _r_obj_createbyte_ex (NULL, allocation_length);

			status = RtlDecompressBuffer (
				format,
				tmp_buffer->buffer,
				allocation_length,
				buffer->buffer,
				(ULONG)buffer->length,
				&return_length
			);

			if (status != STATUS_BAD_COMPRESSION_BUFFER)
				break;
		}
		while (--attempts);
	}

	if (NT_SUCCESS (status))
	{
		tmp_buffer->length = return_length;

		*out_buffer = tmp_buffer;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (tmp_buffer);
	}

	return status;
}

_Ret_maybenull_
HINSTANCE _r_sys_loadlibrary (
	_In_ LPCWSTR lib_name
)
{
	HINSTANCE hlib;
	ULONG flags;

#if defined(APP_NO_DEPRECATIONS)
	flags = LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32;
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		flags = LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32;
	}
	else
	{
		flags = 0;
	}
#endif // APP_NO_DEPRECATIONS

	hlib = LoadLibraryEx (lib_name, NULL, flags);

	return hlib;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createprocess (
	_In_opt_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory
)
{
	return _r_sys_createprocess_ex (file_name, command_line, current_directory, NULL, SW_SHOWDEFAULT, 0);
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createprocess_ex (
	_In_opt_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory,
	_In_opt_ LPSTARTUPINFO startup_info_ptr,
	_In_ WORD show_state,
	_In_ ULONG flags
)
{
	STARTUPINFO startup_info = {0};
	PROCESS_INFORMATION process_info = {0};

	PR_STRING file_name_string = NULL;
	PR_STRING command_line_string = NULL;
	PR_STRING current_directory_string = NULL;

	PR_STRING new_path;
	LPWSTR *arga;
	INT numargs;

	NTSTATUS status;

	if (!startup_info_ptr)
	{
		startup_info_ptr = &startup_info;

		startup_info_ptr->cb = sizeof (startup_info);
	}

	if (show_state != SW_SHOWDEFAULT)
	{
		startup_info_ptr->dwFlags |= STARTF_USESHOWWINDOW;
		startup_info_ptr->wShowWindow = show_state;
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
			arga = CommandLineToArgvW (command_line_string->buffer, &numargs);

			if (arga)
			{
				file_name_string = _r_obj_createstring (arga[0]);

				LocalFree (arga);
			}
		}
	}

	if (file_name_string && !_r_fs_exists (file_name_string->buffer))
	{
		// The user typed a name without a path so attempt to locate the executable. (dmex)
		new_path = _r_path_search (file_name_string->buffer, L".exe", FALSE);

		_r_obj_movereference (&file_name_string, new_path); // clear or set
	}

	if (current_directory)
	{
		current_directory_string = _r_obj_createstring (current_directory);
	}
	else
	{
		if (!_r_obj_isstringempty (file_name_string))
			current_directory_string = _r_path_getbasedirectory (&file_name_string->sr);
	}

	if (CreateProcess (
		_r_obj_getstring (file_name_string),
		command_line_string ? command_line_string->buffer : NULL,
		NULL,
		NULL,
		FALSE,
		flags,
		NULL,
		_r_obj_getstring (current_directory_string),
		startup_info_ptr,
		&process_info))
	{
		status = STATUS_SUCCESS;
	}
	else
	{
		status = RtlGetLastNtStatus ();
	}

	if (process_info.hProcess)
		NtClose (process_info.hProcess);

	if (process_info.hThread)
		NtClose (process_info.hThread);

	if (file_name_string)
		_r_obj_dereference (file_name_string);

	if (command_line_string)
		_r_obj_dereference (command_line_string);

	if (current_directory_string)
		_r_obj_dereference (current_directory_string);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_openprocess (
	_In_opt_ HANDLE process_id,
	_In_ ACCESS_MASK desired_access,
	_Out_ PHANDLE process_handle
)
{
	OBJECT_ATTRIBUTES object_attributes;
	CLIENT_ID client_id;
	NTSTATUS status;

	InitializeObjectAttributes (&object_attributes, NULL, 0, NULL, NULL);

	client_id.UniqueProcess = process_id;
	client_id.UniqueThread = NULL;

	status = NtOpenProcess (process_handle, desired_access, &object_attributes, &client_id);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_queryprocessstring (
	_In_ HANDLE process_handle,
	_In_ PROCESSINFOCLASS info_class,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PVOID buffer;
	ULONG return_length;
	NTSTATUS status;

	return_length = 0;

	status = NtQueryInformationProcess (
		process_handle,
		info_class,
		NULL,
		0,
		&return_length
	);

	if (status != STATUS_BUFFER_OVERFLOW &&
		status != STATUS_BUFFER_TOO_SMALL &&
		status != STATUS_INFO_LENGTH_MISMATCH)
	{
		*out_buffer = NULL;

		return status;
	}

	buffer = _r_mem_allocatezero (return_length);

	status = NtQueryInformationProcess (
		process_handle,
		info_class,
		buffer,
		return_length,
		&return_length
	);

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_createstring4 (buffer);
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (buffer);

	return status;
}

BOOLEAN _r_sys_runasadmin (
	_In_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory
)
{
	SHELLEXECUTEINFO shex = {0};

	shex.cbSize = sizeof (shex);
	shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	shex.lpVerb = L"runas";
	shex.nShow = SW_SHOW;
	shex.lpFile = file_name;
	shex.lpParameters = command_line;
	shex.lpDirectory = current_directory;

	return !!ShellExecuteEx (&shex);
}

VOID _r_sys_sleep (
	_In_ ULONG milliseconds
)
{
	LARGE_INTEGER timeout;

	if (milliseconds == 0 || milliseconds == INFINITE)
		return;

	_r_calc_millisecondstolargeinteger (&timeout, milliseconds);

	NtDelayExecution (FALSE, &timeout);
}

NTSTATUS NTAPI _r_sys_basethreadstart (
	_In_ PVOID arglist
)
{
	R_THREAD_CONTEXT context;
	PR_FREE_LIST free_list;
	NTSTATUS status;
	HRESULT hr;

	free_list = _r_sys_getthreadfreelist ();

	context = *(PR_THREAD_CONTEXT)arglist;

	_r_freelist_deleteitem (free_list, arglist);

	hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	status = context.base_address (context.arglist);

	if (hr == S_OK || hr == S_FALSE)
		CoUninitialize ();

	RtlExitUserThread (status);

	return status;
}

PR_FREE_LIST _r_sys_getthreadfreelist ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_FREE_LIST free_list = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_freelist_initialize (&free_list, sizeof (R_THREAD_CONTEXT), 32);

		_r_initonce_end (&init_once);
	}

	return &free_list;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_createthread (
	_In_ PUSER_THREAD_START_ROUTINE base_address,
	_In_opt_ PVOID arglist,
	_Out_opt_ PHANDLE thread_handle,
	_In_opt_ PR_ENVIRONMENT environment,
	_In_opt_ LPCWSTR thread_name
)
{
	PR_THREAD_CONTEXT context;
	PR_FREE_LIST free_list;
	HANDLE hthread;
	NTSTATUS status;

	free_list = _r_sys_getthreadfreelist ();

	context = _r_freelist_allocateitem (free_list);

	context->base_address = base_address;
	context->arglist = arglist;

	status = RtlCreateUserThread (
		NtCurrentProcess (),
		NULL,
		TRUE,
		0,
		0,
		0,
		&_r_sys_basethreadstart,
		context,
		&hthread,
		NULL
	);

	if (NT_SUCCESS (status))
	{
		if (environment)
			_r_sys_setthreadenvironment (hthread, environment);

		// win10rs1+
		if (thread_name)
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
				_r_sys_setthreadname (hthread, thread_name);
		}

		if (thread_handle)
		{
			*thread_handle = hthread;
		}
		else
		{
			NtResumeThread (hthread, NULL);
			NtClose (hthread);
		}
	}
	else
	{
		if (thread_handle)
			*thread_handle = NULL;

		_r_freelist_deleteitem (free_list, context);
	}

	return status;
}

_Ret_maybenull_
HICON _r_sys_loadicon (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR icon_name,
	_In_ LONG icon_size
)
{
	HICON hicon;
	HRESULT hr;

#if defined(APP_NO_DEPRECATIONS)

	hr = LoadIconWithScaleDown (hinstance, icon_name, icon_size, icon_size, &hicon);

	if (hr == S_OK)
		return hicon;

#else

	static R_INITONCE init_once = PR_INITONCE_INIT;
	static LIWSD _LoadIconWithScaleDown = NULL;

	HINSTANCE hcomctl32;

	if (_r_initonce_begin (&init_once))
	{
		hcomctl32 = _r_sys_loadlibrary (L"comctl32.dll");

		if (hcomctl32)
		{
			// vista+
			_LoadIconWithScaleDown = (LIWSD)GetProcAddress (hcomctl32, "LoadIconWithScaleDown");

			FreeLibrary (hcomctl32);
		}

		_r_initonce_end (&init_once);
	}

	if (_LoadIconWithScaleDown)
	{
		hr = _LoadIconWithScaleDown (hinstance, icon_name, icon_size, icon_size, &hicon);

		if (hr == S_OK)
			return hicon;
	}

#endif // APP_NO_DEPRECATIONS

	hicon = (HICON)LoadImage (hinstance, icon_name, IMAGE_ICON, icon_size, icon_size, 0);

	return hicon;
}

_Ret_maybenull_
HICON _r_sys_loadsharedicon (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR icon_name,
	_In_ LONG icon_size
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_QUEUED_LOCK queued_lock = PR_QUEUED_LOCK_INIT;
	static PR_HASHTABLE shared_icons = NULL;

	PR_OBJECT_POINTER object_ptr;
	R_OBJECT_POINTER object_data;

	HICON hicon;

	ULONG hash_code;
	ULONG name_hash;

	if (_r_initonce_begin (&init_once))
	{
		shared_icons = _r_obj_createhashtable_ex (sizeof (R_OBJECT_POINTER), 8, NULL);

		_r_initonce_end (&init_once);
	}

	if (IS_INTRESOURCE (icon_name))
	{
		name_hash = PtrToUlong (icon_name);
	}
	else
	{
		name_hash = _r_str_gethash (icon_name, TRUE);
	}

	hash_code = (name_hash ^ (PtrToUlong (hinstance) >> 5) ^ (icon_size << 3) ^ icon_size);

	_r_queuedlock_acquireshared (&queued_lock);
	object_ptr = _r_obj_findhashtable (shared_icons, hash_code);
	_r_queuedlock_releaseshared (&queued_lock);

	// found shared icon
	if (object_ptr)
	{
		hicon = (HICON)object_ptr->object_body;
	}
	else
	{
		// add new shared icon entry
		hicon = _r_sys_loadicon (hinstance, icon_name, icon_size);

		if (hicon)
		{
			object_data.object_body = hicon;

			_r_queuedlock_acquireexclusive (&queued_lock);
			_r_obj_addhashtableitem (shared_icons, hash_code, &object_data);
			_r_queuedlock_releaseexclusive (&queued_lock);
		}
	}

	return hicon;
}

_Ret_maybenull_
PR_STRING _r_sys_querytaginformation (
	_In_ HANDLE hprocess,
	_In_ LPCVOID tag
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static IQTI _I_QueryTagInformation = NULL;

	TAG_INFO_NAME_FROM_TAG tag_query = {0};
	PR_STRING service_name_string;

	HINSTANCE hadvapi32;

	if (_r_initonce_begin (&init_once))
	{
		hadvapi32 = _r_sys_loadlibrary (L"advapi32.dll");

		if (hadvapi32)
		{
			_I_QueryTagInformation = (IQTI)GetProcAddress (hadvapi32, "I_QueryTagInformation");

			FreeLibrary (hadvapi32);
		}

		_r_initonce_end (&init_once);
	}

	if (!_I_QueryTagInformation)
		return NULL;

	tag_query.InParams.dwPid = HandleToUlong (hprocess);
	tag_query.InParams.dwTag = PtrToUlong (tag);

	_I_QueryTagInformation (NULL, TagInfoLevelNameFromTag, &tag_query);

	if (tag_query.OutParams.pszName)
	{
		service_name_string = _r_obj_createstring (tag_query.OutParams.pszName);

		LocalFree (tag_query.OutParams.pszName);

		return service_name_string;
	}

	return NULL;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_querytokeninformation (
	_In_ HANDLE token_handle,
	_In_ TOKEN_INFORMATION_CLASS token_class,
	_Out_ PVOID_PTR token_info
)
{
	PVOID buffer;
	ULONG buffer_length;
	ULONG return_length;
	NTSTATUS status;

	return_length = 0;
	buffer_length = 128;
	buffer = _r_mem_allocatezero (buffer_length);

	status = NtQueryInformationToken (
		token_handle,
		token_class,
		buffer,
		buffer_length,
		&return_length
	);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		buffer_length = return_length;
		buffer = _r_mem_reallocatezero (buffer, buffer_length);

		status = NtQueryInformationToken (
			token_handle,
			token_class,
			buffer,
			buffer_length,
			&return_length
		);
	}

	if (NT_SUCCESS (status))
	{
		*token_info = buffer;
	}
	else
	{
		*token_info = NULL;

		_r_mem_free (buffer);
	}

	return status;
}

VOID _r_sys_queryprocessenvironment (
	_In_ HANDLE process_handle,
	_Out_ PR_ENVIRONMENT environment
)
{
	IO_PRIORITY_HINT io_priority;
	PROCESS_PRIORITY_CLASS priority_class;
	PAGE_PRIORITY_INFORMATION page_priority;
	NTSTATUS status;

	// query base priority
	status = NtQueryInformationProcess (process_handle, ProcessPriorityClass, &priority_class, sizeof (priority_class), NULL);

	if (NT_SUCCESS (status))
	{
		environment->base_priority = priority_class.PriorityClass;
	}
	else
	{
		environment->base_priority = PROCESS_PRIORITY_CLASS_UNKNOWN;
	}

	// query i/o priority
	status = NtQueryInformationProcess (process_handle, ProcessIoPriority, &io_priority, sizeof (io_priority), NULL);

	if (NT_SUCCESS (status))
	{
		environment->io_priority = io_priority;
	}
	else
	{
		environment->io_priority = IoPriorityVeryLow;
	}

	// query memory priority
	status = NtQueryInformationProcess (process_handle, ProcessPagePriority, &page_priority, sizeof (page_priority), NULL);

	if (NT_SUCCESS (status))
	{
		environment->page_priority = page_priority.PagePriority;
	}
	else
	{
		environment->page_priority = MEMORY_PRIORITY_LOWEST;
	}
}

VOID _r_sys_querythreadenvironment (
	_In_ HANDLE thread_handle,
	_Out_ PR_ENVIRONMENT environment
)
{
	KPRIORITY base_priority;
	IO_PRIORITY_HINT io_priority;
	PAGE_PRIORITY_INFORMATION page_priority;
	NTSTATUS status;

	// query base priority
	status = NtQueryInformationThread (thread_handle, ThreadBasePriority, &base_priority, sizeof (base_priority), NULL);

	if (NT_SUCCESS (status))
	{
		environment->base_priority = base_priority;
	}
	else
	{
		environment->base_priority = THREAD_PRIORITY_NORMAL;
	}

	// query i/o priority
	status = NtQueryInformationThread (thread_handle, ThreadIoPriority, &io_priority, sizeof (io_priority), NULL);

	if (NT_SUCCESS (status))
	{
		environment->io_priority = io_priority;
	}
	else
	{
		environment->io_priority = IoPriorityVeryLow;
	}

	// query memory priority
	status = NtQueryInformationThread (thread_handle, ThreadPagePriority, &page_priority, sizeof (page_priority), NULL);

	if (NT_SUCCESS (status))
	{
		environment->page_priority = page_priority.PagePriority;
	}
	else
	{
		environment->page_priority = MEMORY_PRIORITY_LOWEST;
	}
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setprocessprivilege (
	_In_ HANDLE process_handle,
	_In_reads_ (count) PULONG privileges,
	_In_ ULONG count,
	_In_ BOOLEAN is_enable
)
{
	HANDLE token_handle;
	PVOID privileges_buffer;
	PTOKEN_PRIVILEGES token_privileges;
	NTSTATUS status;

	status = NtOpenProcessToken (process_handle, TOKEN_ADJUST_PRIVILEGES, &token_handle);

	if (NT_SUCCESS (status))
	{
		privileges_buffer = _r_mem_allocatezero (
			FIELD_OFFSET (TOKEN_PRIVILEGES, Privileges) + (sizeof (LUID_AND_ATTRIBUTES) * count)
		);

		token_privileges = privileges_buffer;
		token_privileges->PrivilegeCount = count;

		for (ULONG i = 0; i < count; i++)
		{
			//token_privileges->Privileges[i].Luid.HighPart = 0;
			token_privileges->Privileges[i].Luid.LowPart = privileges[i];
			token_privileges->Privileges[i].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;
		}

		status = NtAdjustPrivilegesToken (
			token_handle,
			FALSE,
			token_privileges,
			0,
			NULL,
			NULL
		);

		_r_mem_free (privileges_buffer);

		NtClose (token_handle);
	}

	return status;
}

VOID _r_sys_setenvironment (
	_Out_ PR_ENVIRONMENT environment,
	_In_ KPRIORITY base_priority,
	_In_ ULONG io_priority,
	_In_ ULONG page_priority
)
{
	environment->base_priority = base_priority;
	environment->io_priority = io_priority;
	environment->page_priority = page_priority;
}

VOID _r_sys_setprocessenvironment (
	_In_ HANDLE process_handle,
	_In_ PR_ENVIRONMENT new_environment
)
{
	R_ENVIRONMENT current_environment;
	IO_PRIORITY_HINT io_priority;
	PROCESS_PRIORITY_CLASS priority_class;
	PAGE_PRIORITY_INFORMATION page_priority;

	_r_sys_queryprocessenvironment (process_handle, &current_environment);

	// set base priority
	if (current_environment.base_priority != new_environment->base_priority)
	{
		priority_class.Foreground = FALSE;
		priority_class.PriorityClass = (UCHAR)(new_environment->base_priority);

		NtSetInformationProcess (process_handle, ProcessPriorityClass, &priority_class, sizeof (priority_class));
	}

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return;
#endif // !APP_NO_DEPRECATIONS

	// set i/o priority
	if (current_environment.io_priority != new_environment->io_priority)
	{
		io_priority = new_environment->io_priority;

		NtSetInformationProcess (process_handle, ProcessIoPriority, &io_priority, sizeof (io_priority));
	}

	// set memory priority
	if (current_environment.page_priority != new_environment->page_priority)
	{
		page_priority.PagePriority = new_environment->page_priority;

		NtSetInformationProcess (process_handle, ProcessPagePriority, &page_priority, sizeof (page_priority));
	}
}

VOID _r_sys_setthreadenvironment (
	_In_ HANDLE thread_handle,
	_In_ PR_ENVIRONMENT new_environment
)
{
	R_ENVIRONMENT current_environment;
	KPRIORITY base_priority;
	IO_PRIORITY_HINT io_priority;
	PAGE_PRIORITY_INFORMATION page_priority;

	_r_sys_querythreadenvironment (thread_handle, &current_environment);

	// set base priority
	if (current_environment.base_priority != new_environment->base_priority)
	{
		base_priority = new_environment->base_priority;

		NtSetInformationThread (thread_handle, ThreadBasePriority, &base_priority, sizeof (base_priority));
	}

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return;
#endif // !APP_NO_DEPRECATIONS

	// set i/o priority
	if (current_environment.io_priority != new_environment->io_priority)
	{
		io_priority = new_environment->io_priority;

		NtSetInformationThread (thread_handle, ThreadIoPriority, &io_priority, sizeof (io_priority));
	}

	// set memory priority
	if (current_environment.page_priority != new_environment->page_priority)
	{
		page_priority.PagePriority = new_environment->page_priority;

		NtSetInformationThread (thread_handle, ThreadPagePriority, &page_priority, sizeof (page_priority));
	}
}

_Success_ (return != 0)
EXECUTION_STATE _r_sys_setthreadexecutionstate (
	_In_ EXECUTION_STATE new_state
)
{
	EXECUTION_STATE old_state;
	NTSTATUS status;

	status = NtSetThreadExecutionState (new_state, &old_state);

	if (NT_SUCCESS (status))
		return old_state;

	return 0;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setthreadname (
	_In_ HANDLE thread_handle,
	_In_ LPCWSTR thread_name
)
{
	THREAD_NAME_INFORMATION tni;
	NTSTATUS status;

	RtlZeroMemory (&tni, sizeof (tni));
	RtlInitUnicodeString (&tni.ThreadName, thread_name);

	status = NtSetInformationThread (thread_handle, ThreadNameInformation, &tni, sizeof (tni));

	return status;
}

//
// Unixtime
//

LONG64 _r_unixtime_now ()
{
	FILETIME file_time;

	do
	{
		file_time.dwHighDateTime = USER_SHARED_DATA->SystemTime.High1Time;
		file_time.dwLowDateTime = USER_SHARED_DATA->SystemTime.LowPart;
	}
	while (file_time.dwHighDateTime != USER_SHARED_DATA->SystemTime.High2Time);

	return _r_unixtime_from_filetime (&file_time);
}

LONG64 _r_unixtime_from_filetime (
	_In_ const FILETIME * file_time
)
{
	LARGE_INTEGER time_value;

	time_value.LowPart = file_time->dwLowDateTime;
	time_value.HighPart = file_time->dwHighDateTime;

	return (time_value.QuadPart - 116444736000000000LL) / 10000000LL;
}

LONG64 _r_unixtime_from_systemtime (
	_In_ const SYSTEMTIME * system_time
)
{
	FILETIME file_time;

	if (SystemTimeToFileTime (system_time, &file_time))
		return _r_unixtime_from_filetime (&file_time);

	return 0;
}

VOID _r_unixtime_to_filetime (
	_In_ LONG64 unixtime,
	_Out_ PFILETIME file_time
)
{
	LARGE_INTEGER time_value;

	time_value.QuadPart = (unixtime * 10000000LL) + 116444736000000000LL;

	file_time->dwHighDateTime = time_value.HighPart;
	file_time->dwLowDateTime = time_value.LowPart;
}

_Success_ (return)
BOOLEAN _r_unixtime_to_systemtime (
	_In_ LONG64 unixtime,
	_Out_ PSYSTEMTIME system_time
)
{
	FILETIME file_time;

	_r_unixtime_to_filetime (unixtime, &file_time);

	return !!FileTimeToSystemTime (&file_time, system_time);
}

//
// Device context
//

_Success_ (return)
BOOLEAN _r_dc_adjustwindowrect (
	_Inout_ LPRECT rect,
	_In_ ULONG style,
	_In_ ULONG ex_style,
	_In_ LONG dpi_value,
	_In_ BOOL is_menu
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static AWRFD _AdjustWindowRectExForDpi = NULL;

	HINSTANCE huser32;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			huser32 = _r_sys_loadlibrary (L"user32.dll");

			if (huser32)
			{
				// win10rs1+
				_AdjustWindowRectExForDpi = (AWRFD)GetProcAddress (huser32, "AdjustWindowRectExForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_AdjustWindowRectExForDpi)
		return !!_AdjustWindowRectExForDpi (rect, style, is_menu, ex_style, dpi_value);

	return !!AdjustWindowRectEx (rect, style, is_menu, ex_style);
}

_Ret_maybenull_
HBITMAP _r_dc_bitmapfromicon (
	_In_ HICON hicon,
	_In_ LONG x,
	_In_ LONG y
)
{
	BITMAPINFO bitmap_info = {0};
	BLENDFUNCTION blend_func = {0};
	BP_PAINTPARAMS paint_params = {0};
	RECT icon_rect;
	HPAINTBUFFER hpaint_buffer;
	HGDIOBJ old_bitmap;
	PVOID memory_bits;
	HBITMAP hbitmap;
	HDC buffer_hdc;
	HDC screen_hdc;
	HDC hdc;

	hbitmap = NULL;

	hdc = GetDC (NULL);

	if (!hdc)
		return NULL;

	screen_hdc = CreateCompatibleDC (hdc);

	if (!screen_hdc)
		goto CleanupExit;

	bitmap_info.bmiHeader.biSize = sizeof (bitmap_info);
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	bitmap_info.bmiHeader.biWidth = x;
	bitmap_info.bmiHeader.biHeight = y;
	bitmap_info.bmiHeader.biBitCount = 32;

	hbitmap = CreateDIBSection (screen_hdc, &bitmap_info, DIB_RGB_COLORS, &memory_bits, NULL, 0);

	if (!hbitmap)
		goto CleanupExit;

	old_bitmap = SelectObject (screen_hdc, hbitmap);

	//blend_func.BlendOp = AC_SRC_OVER;
	blend_func.AlphaFormat = AC_SRC_ALPHA;
	blend_func.SourceConstantAlpha = 255;

	paint_params.cbSize = sizeof (paint_params);
	paint_params.dwFlags = BPPF_ERASE;
	paint_params.pBlendFunction = &blend_func;

	SetRect (&icon_rect, 0, 0, x, y);

	hpaint_buffer = BeginBufferedPaint (screen_hdc, &icon_rect, BPBF_DIB, &paint_params, &buffer_hdc);

	if (hpaint_buffer)
	{
		DrawIconEx (buffer_hdc, 0, 0, hicon, x, y, 0, NULL, DI_NORMAL);

		EndBufferedPaint (hpaint_buffer, TRUE);
	}
	else
	{
		_r_dc_fillrect (screen_hdc, &icon_rect, RGB (255, 255, 255));

		DrawIconEx (screen_hdc, 0, 0, hicon, x, y, 0, NULL, DI_NORMAL);
	}

	SelectObject (screen_hdc, old_bitmap);

CleanupExit:

	if (screen_hdc)
		DeleteDC (screen_hdc);

	ReleaseDC (NULL, hdc);

	return hbitmap;
}

_Ret_maybenull_
HICON _r_dc_bitmaptoicon (
	_In_ HBITMAP hbitmap,
	_In_ LONG x,
	_In_ LONG y
)
{
	ICONINFO icon_info = {0};
	HBITMAP hmask;
	HICON hicon;
	HDC hdc;

	hdc = GetDC (NULL);

	if (!hdc)
		return NULL;

	hmask = CreateCompatibleBitmap (hdc, x, y);

	if (!hmask)
	{
		ReleaseDC (NULL, hdc);
		return NULL;
	}

	icon_info.fIcon = TRUE;
	icon_info.hbmColor = hbitmap;
	icon_info.hbmMask = hmask;

	hicon = CreateIconIndirect (&icon_info);

	DeleteObject (hmask);
	ReleaseDC (NULL, hdc);

	return hicon;
}

BOOLEAN _r_dc_drawwindow (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_ BOOLEAN is_drawfooter
)
{
	RECT rect;
	LONG dpi_value;
	LONG footer_height;
	LONG wnd_width;
	LONG wnd_height;
	COLORREF clr;

	if (!GetClientRect (hwnd, &rect))
		return FALSE;

	if (is_drawfooter)
	{
		clr = GetSysColor (COLOR_WINDOW);
	}
	else
	{
		clr = GetSysColor (COLOR_BTNFACE);
	}

	// fill background color
	_r_dc_fillrect (hdc, &rect, clr);

	if (!is_drawfooter)
		return TRUE;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	wnd_width = rect.right;
	wnd_height = rect.bottom;

	footer_height = _r_dc_getdpi (PR_SIZE_FOOTERHEIGHT, dpi_value);

	SetRect (&rect, 0, wnd_height - footer_height, wnd_width, wnd_height);

	// fill footer color
	clr = _r_dc_getcolorshade (clr, 94);

	_r_dc_fillrect (hdc, &rect, clr);

	// draw footer line
	clr = _r_dc_getcolorshade (clr, 86);

	for (LONG i = 0; i < rect.right; i++)
	{
		SetPixelV (hdc, i, rect.top, clr);
	}

	return TRUE;
}

BOOLEAN _r_dc_drawimagelisticon (
	_In_ HDC hdc,
	_In_ HIMAGELIST himglist,
	_In_ INT index,
	_In_ LONG x,
	_In_ LONG y,
	_In_opt_ ULONG state,
	_In_opt_ UINT style
)
{
	IMAGELISTDRAWPARAMS ildp = {0};

	ildp.cbSize = sizeof (ildp);
	ildp.himl = himglist;
	ildp.hdcDst = hdc;
	ildp.i = index;
	ildp.x = x;
	ildp.y = y;
	ildp.fState = state;
	ildp.fStyle = style;

	return !!ImageList_DrawIndirect (&ildp);
}

// Optimized version of WinAPI function "FillRect"
VOID _r_dc_fillrect (
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ COLORREF clr
)
{
	COLORREF clr_prev;

	clr_prev = SetBkColor (hdc, clr);

	ExtTextOut (hdc, 0, 0, ETO_OPAQUE, rect, NULL, 0, NULL);

	SetBkColor (hdc, clr_prev);
}

VOID _r_dc_fixcontrolfont (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	HFONT hfont;

	hfont = (HFONT)SendDlgItemMessage (hwnd, ctrl_id, WM_GETFONT, 0, 0);

	SelectObject (hdc, hfont);
}

VOID _r_dc_fixwindowfont (
	_In_ HDC hdc,
	_In_ HWND hwnd
)
{
	HFONT hfont;

	hfont = (HFONT)SendMessage (hwnd, WM_GETFONT, 0, 0);

	SelectObject (hdc, hfont);
}

LONG _r_dc_fontsizetoheight (
	_In_ LONG size,
	_In_ LONG dpi_value
)
{
	return -_r_calc_multipledividesigned (size, dpi_value, 72);
}

LONG _r_dc_fontheighttosize (
	_In_ LONG height,
	_In_ LONG dpi_value
)
{
	return _r_calc_multipledividesigned (-height, 72, dpi_value);
}

COLORREF _r_dc_getcoloraccent ()
{
	COLORREF clr;
	BOOL is_opaque;
	HRESULT hr;

	hr = DwmGetColorizationColor (&clr, &is_opaque);

	if (hr != S_OK)
		return 0;

	return RGB (GetBValue (clr), GetGValue (clr), GetRValue (clr));
}

COLORREF _r_dc_getcolorbrightness (
	_In_ COLORREF clr
)
{
	COLORREF r;
	COLORREF g;
	COLORREF b;

	COLORREF min;
	COLORREF max;

	r = clr & 0xff;
	g = (clr >> 8) & 0xff;
	b = (clr >> 16) & 0xff;

	min = r;
	max = r;

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

COLORREF _r_dc_getcolorinverse (
	_In_ COLORREF clr
)
{
	COLORREF r;
	COLORREF g;
	COLORREF b;

	r = 255 - GetRValue (clr);
	g = 255 - GetGValue (clr);
	b = 255 - GetBValue (clr);

	return RGB (r, g, b);
}

COLORREF _r_dc_getcolorshade (
	_In_ COLORREF clr,
	_In_ ULONG percent
)
{
	COLORREF r;
	COLORREF g;
	COLORREF b;

	r = ((GetRValue (clr) * percent) / 100);
	g = ((GetGValue (clr) * percent) / 100);
	b = ((GetBValue (clr) * percent) / 100);

	return RGB (r, g, b);
}

VOID _r_dc_getdefaultfont (
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_forced
)
{
	NONCLIENTMETRICS ncm;
	PLOGFONT system_font;

	RtlZeroMemory (&ncm, sizeof (ncm));

#if defined(APP_NO_DEPRECATIONS)
	ncm.cbSize = sizeof (ncm);
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
		ncm.cbSize = sizeof (ncm);
	}
	else
	{
		ncm.cbSize = RTL_SIZEOF_THROUGH_FIELD (NONCLIENTMETRICS, lfMessageFont); // xp support
	}
#endif // APP_NO_DEPRECATIONS

	if (!_r_dc_getsystemparametersinfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, dpi_value))
		return;

	system_font = &ncm.lfMessageFont;

	if (is_forced || _r_str_isempty2 (logfont->lfFaceName))
		_r_str_copy (logfont->lfFaceName, LF_FACESIZE, system_font->lfFaceName);

	if (is_forced || !logfont->lfHeight)
		logfont->lfHeight = system_font->lfHeight;

	if (is_forced || !logfont->lfWeight)
		logfont->lfWeight = system_font->lfWeight;
}

LONG _r_dc_getdpi (
	_In_ LONG number,
	_In_ LONG dpi_value
)
{
	return _r_calc_multipledividesigned (number, dpi_value, USER_DEFAULT_SCREEN_DPI);
}

LONG _r_dc_getdpivalue (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCRECT rect
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GDFM _GetDpiForMonitor = NULL; // win81+
	static GDFW _GetDpiForWindow = NULL; // win10rs1+
	static GDFS _GetDpiForSystem = NULL; // win10rs1+

	HMONITOR hmonitor;
	HINSTANCE hshcore;
	HINSTANCE huser32;
	HDC hdc;
	LONG dpi_value;
	UINT dpi_x;
	UINT dpi_y;
	HRESULT hr;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
		{
			hshcore = _r_sys_loadlibrary (L"shcore.dll");
			huser32 = _r_sys_loadlibrary (L"user32.dll");

			if (hshcore)
			{
				// win81+
				_GetDpiForMonitor = (GDFM)GetProcAddress (hshcore, "GetDpiForMonitor");

				FreeLibrary (hshcore);
			}

			if (huser32)
			{
				// win10rs1+
				_GetDpiForWindow = (GDFW)GetProcAddress (huser32, "GetDpiForWindow");

				// win10rs1+
				_GetDpiForSystem = (GDFS)GetProcAddress (huser32, "GetDpiForSystem");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		if (rect || hwnd)
		{
			// win10rs1+
			if (_GetDpiForWindow)
			{
				if (hwnd)
					return _GetDpiForWindow (hwnd);
			}

			// win81+
			if (_GetDpiForMonitor)
			{
				if (rect)
				{
					hmonitor = MonitorFromRect (rect, MONITOR_DEFAULTTONEAREST);
				}
				else
				{
					hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
				}

				hr = _GetDpiForMonitor (hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

				if (hr == S_OK)
					return dpi_x;
			}
		}

		// win10rs1+
		if (_GetDpiForSystem)
			return _GetDpiForSystem ();
	}

	// win8 and lower fallback
	hdc = GetDC (NULL);

	if (hdc)
	{
		dpi_value = GetDeviceCaps (hdc, LOGPIXELSX);

		ReleaseDC (NULL, hdc);

		return dpi_value;
	}

	return USER_DEFAULT_SCREEN_DPI;
}

_Success_ (return != 0)
LONG _r_dc_getfontwidth (
	_In_ HDC hdc,
	_In_ PR_STRINGREF string
)
{
	SIZE size;

	if (!GetTextExtentPoint32 (hdc, string->buffer, (ULONG)_r_str_getlength3 (string), &size))
		return 0;

	return size.cx;
}

LONG _r_dc_getmonitordpi (
	_In_ LPCRECT rect
)
{
	return _r_dc_getdpivalue (NULL, rect);
}

VOID _r_dc_getsizedpivalue (
	_Inout_ PR_SIZE size,
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_unpack
)
{
	LONG numerator;
	LONG denominator;

	if (dpi_value == USER_DEFAULT_SCREEN_DPI)
		return;

	if (is_unpack)
	{
		numerator = dpi_value;
		denominator = USER_DEFAULT_SCREEN_DPI;
	}
	else
	{
		numerator = USER_DEFAULT_SCREEN_DPI;
		denominator = dpi_value;
	}

	size->cx = _r_calc_multipledividesigned (size->cx, numerator, denominator);
	size->cy = _r_calc_multipledividesigned (size->cy, numerator, denominator);
}

LONG _r_dc_getsystemmetrics (
	_In_ INT index,
	_In_ LONG dpi_value
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GSMFD _GetSystemMetricsForDpi = NULL;

	HINSTANCE huser32;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			huser32 = _r_sys_loadlibrary (L"user32.dll");

			if (huser32)
			{
				// win10rs1+
				_GetSystemMetricsForDpi = (GSMFD)GetProcAddress (huser32, "GetSystemMetricsForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_GetSystemMetricsForDpi)
		return _GetSystemMetricsForDpi (index, dpi_value);

	return GetSystemMetrics (index);
}

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (
	_In_ UINT action,
	_In_ UINT param1,
	_Pre_maybenull_ _Post_valid_ PVOID param2,
	_In_ LONG dpi_value
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SPIFP _SystemParametersInfoForDpi = NULL;

	HINSTANCE huser32;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
		{
			huser32 = _r_sys_loadlibrary (L"user32.dll");

			if (huser32)
			{
				// win10rs1+
				_SystemParametersInfoForDpi = (SPIFP)GetProcAddress (huser32, "SystemParametersInfoForDpi");

				FreeLibrary (huser32);
			}
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (_SystemParametersInfoForDpi)
		return !!_SystemParametersInfoForDpi (action, param1, param2, 0, dpi_value);

	return !!SystemParametersInfo (action, param1, param2, 0);
}

LONG _r_dc_gettaskbardpi ()
{
	APPBARDATA taskbar_rect = {0};

	taskbar_rect.cbSize = sizeof (APPBARDATA);

	if (SHAppBarMessage (ABM_GETTASKBARPOS, &taskbar_rect))
		return _r_dc_getmonitordpi (&taskbar_rect.rc);

	return _r_dc_getdpivalue (NULL, NULL);
}

LONG _r_dc_getwindowdpi (
	_In_ HWND hwnd
)
{
	return _r_dc_getdpivalue (hwnd, NULL);
}

_Ret_maybenull_
HBITMAP _r_dc_imagetobitmap (
	_In_ LPCGUID format,
	_In_ WICInProcPointer buffer,
	_In_ ULONG buffer_length,
	_In_ LONG x,
	_In_ LONG y
)
{
	BITMAPINFO bmi = {0};

	HDC hdc = NULL;
	HDC hdc_buffer = NULL;
	HBITMAP hbitmap = NULL;
	PVOID bitmap_buffer = NULL;

	IWICStream *wicStream = NULL;
	IWICBitmapSource *wicBitmapSource = NULL;
	IWICBitmapDecoder *wicDecoder = NULL;
	IWICBitmapFrameDecode *wicFrame = NULL;
	IWICImagingFactory2 *wicFactory = NULL;
	IWICFormatConverter *wicFormatConverter = NULL;
	IWICBitmapScaler *wicScaler = NULL;
	WICPixelFormatGUID pixelFormat;
	WICRect rect = {0};
	UINT frame_count;

	HRESULT hr;

	// Create the ImagingFactory (win8+)
	hr = CoCreateInstance (
		&CLSID_WICImagingFactory2,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IWICImagingFactory2,
		&wicFactory
	);

	if (FAILED (hr))
	{
		// winxp+
		hr = CoCreateInstance (
			&CLSID_WICImagingFactory1,
			NULL,
			CLSCTX_INPROC_SERVER,
			&IID_IWICImagingFactory,
			&wicFactory
		);

		if (FAILED (hr))
			goto CleanupExit;
	}

	// Create the Stream
	hr = IWICImagingFactory_CreateStream (wicFactory, &wicStream);

	if (FAILED (hr))
		goto CleanupExit;

	// Initialize the Stream from Memory
	hr = IWICStream_InitializeFromMemory (wicStream, buffer, buffer_length);

	if (FAILED (hr))
		goto CleanupExit;

	hr = IWICImagingFactory_CreateDecoder (wicFactory, format, NULL, &wicDecoder);

	if (FAILED (hr))
		goto CleanupExit;

	hr = IWICBitmapDecoder_Initialize (wicDecoder, (IStream *)wicStream, WICDecodeMetadataCacheOnLoad);

	if (FAILED (hr))
		goto CleanupExit;

	// Get the Frame count
	hr = IWICBitmapDecoder_GetFrameCount (wicDecoder, &frame_count);

	if (FAILED (hr) || frame_count < 1)
		goto CleanupExit;

	// Get the Frame
	hr = IWICBitmapDecoder_GetFrame (wicDecoder, 0, &wicFrame);

	if (FAILED (hr))
		goto CleanupExit;

	// Get the WicFrame image format
	hr = IWICBitmapFrameDecode_GetPixelFormat (wicFrame, &pixelFormat);

	if (FAILED (hr))
		goto CleanupExit;

	// Check if the image format is supported:
	if (IsEqualGUID (&pixelFormat, &GUID_WICPixelFormat32bppPRGBA))
	{
		wicBitmapSource = (IWICBitmapSource *)wicFrame;
	}
	else
	{
		hr = IWICImagingFactory_CreateFormatConverter (wicFactory, &wicFormatConverter);

		if (FAILED (hr))
			goto CleanupExit;

		hr = IWICFormatConverter_Initialize (
			wicFormatConverter,
			(IWICBitmapSource *)wicFrame,
			&GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			NULL,
			0.0,
			WICBitmapPaletteTypeCustom
		);

		if (FAILED (hr))
			goto CleanupExit;

		// Convert the image to the correct format.
		IWICFormatConverter_QueryInterface (
			wicFormatConverter,
			&IID_IWICBitmapSource,
			&wicBitmapSource
		);

		// Cleanup the converter.
		IWICFormatConverter_Release (wicFormatConverter);

		// Dispose the old frame now that the converted frame is in wicBitmapSource.
		IWICBitmapFrameDecode_Release (wicFrame);
	}

	bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = x;
	bmi.bmiHeader.biHeight = -(y);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hdc = GetDC (NULL);

	if (!hdc)
	{
		hr = E_FAIL;
		goto CleanupExit;
	}

	hdc_buffer = CreateCompatibleDC (hdc);

	if (!hdc_buffer)
	{
		hr = E_FAIL;
		goto CleanupExit;
	}

	hbitmap = CreateDIBSection (hdc, &bmi, DIB_RGB_COLORS, &bitmap_buffer, NULL, 0);

	if (!hbitmap)
	{
		hr = E_FAIL;
		goto CleanupExit;
	}

	hr = IWICImagingFactory_CreateBitmapScaler (wicFactory, &wicScaler);

	if (FAILED (hr))
		goto CleanupExit;

	hr = IWICBitmapScaler_Initialize (wicScaler, wicBitmapSource, x, y, WICBitmapInterpolationModeFant);

	if (FAILED (hr))
		goto CleanupExit;

	rect.Width = x;
	rect.Height = y;

	hr = IWICBitmapScaler_CopyPixels (
		wicScaler,
		&rect,
		x * 4,
		x * y * 4,
		(PBYTE)bitmap_buffer
	);

	if (FAILED (hr))
		goto CleanupExit;

CleanupExit:

	if (hdc_buffer)
		DeleteDC (hdc_buffer);

	if (hdc)
		ReleaseDC (NULL, hdc);

	if (wicScaler)
		IWICBitmapScaler_Release (wicScaler);

	if (wicBitmapSource)
		IWICBitmapSource_Release (wicBitmapSource);

	if (wicStream)
		IWICStream_Release (wicStream);

	if (wicDecoder)
		IWICBitmapDecoder_Release (wicDecoder);

	if (wicFactory)
		IWICImagingFactory_Release (wicFactory);

	if (FAILED (hr))
	{
		if (hbitmap)
			DeleteObject (hbitmap);

		return NULL;
	}

	return hbitmap;
}

BOOLEAN _r_dc_isfontexists (
	_In_ PLOGFONT logfont
)
{
	HDC hdc;
	INT result;

	hdc = GetDC (NULL);

	result = EnumFontFamiliesEx (hdc, logfont, &_r_util_enum_font_callback, (LPARAM)logfont, 0);

	if (hdc)
		ReleaseDC (NULL, hdc);

	return !(result != 0);
}

//
// File dialog
//

_Success_ (return)
BOOLEAN _r_filedialog_initialize (
	_Out_ PR_FILE_DIALOG file_dialog,
	_In_ ULONG flags
)
{
	IFileDialog *ifd;
	HRESULT hr;

	hr = CoCreateInstance (
		(flags & (PR_FILEDIALOG_OPENFILE | PR_FILEDIALOG_OPENDIR)) ? &CLSID_FileOpenDialog : &CLSID_FileSaveDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IFileDialog,
		&ifd
	);

	if (SUCCEEDED (hr))
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
			ofn->Flags = OFN_DONTADDTORECENT | OFN_ENABLESIZING | OFN_EXPLORER | OFN_HIDEREADONLY |
				OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		}
		else
		{
			ofn->Flags = OFN_DONTADDTORECENT | OFN_ENABLESIZING | OFN_EXPLORER | OFN_HIDEREADONLY |
				OFN_LONGNAMES | OFN_OVERWRITEPROMPT;
		}

		file_dialog->u.ofn = ofn;

		return TRUE;
	}
#endif // !APP_NO_DEPRECATIONS

	return FALSE;
}

_Success_ (return)
BOOLEAN _r_filedialog_show (
	_In_opt_ HWND hwnd,
	_In_ PR_FILE_DIALOG file_dialog
)
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

PR_STRING _r_filedialog_getpath (
	_In_ PR_FILE_DIALOG file_dialog
)
{
#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // APP_NO_DEPRECATIONS
	{
		IShellItem *result;
		PR_STRING file_name = NULL;
		LPWSTR name;
		HRESULT hr;

		hr = IFileDialog_GetResult (file_dialog->u.ifd, &result);

		if (SUCCEEDED (hr))
		{
			hr = IShellItem_GetDisplayName (result, SIGDN_FILESYSPATH, &name);

			if (SUCCEEDED (hr))
			{
				file_name = _r_obj_createstring (name);

				CoTaskMemFree (name);
			}

			IShellItem_Release (result);
		}

		if (!file_name)
		{
			hr = IFileDialog_GetFileName (file_dialog->u.ifd, &name);

			if (SUCCEEDED (hr))
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

VOID _r_filedialog_setpath (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ LPCWSTR path
)
{
	R_STRINGREF sr;

	_r_obj_initializestringrefconst (&sr, path);

#if !defined(APP_NO_DEPRECATIONS)
	if ((file_dialog->flags & PR_FILEDIALOG_ISIFILEDIALOG))
#endif // !APP_NO_DEPRECATIONS
	{
		IShellItem *shell_item = NULL;
		R_STRINGREF directory_part;
		R_STRINGREF basename_part;
		LPITEMIDLIST item;
		SFGAOF attributes;
		PR_STRING directory;
		HRESULT hr;

		if (_r_path_getpathinfo (&sr, &directory_part, &basename_part))
		{
			directory = _r_obj_createstring3 (&directory_part);

			hr = SHParseDisplayName (directory->buffer, NULL, &item, 0, &attributes);

			if (SUCCEEDED (hr))
			{
				SHCreateShellItem (NULL, NULL, item, &shell_item);

				CoTaskMemFree (item);
			}

			_r_obj_dereference (directory);
		}

		if (shell_item)
		{
			IFileDialog_SetFolder (file_dialog->u.ifd, shell_item);
			IFileDialog_SetFileName (file_dialog->u.ifd, basename_part.buffer);

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

		if (_r_str_findchar (&sr, L'/', FALSE) != SIZE_MAX || _r_str_findchar (&sr, L'\"', FALSE) != SIZE_MAX)
			return;

		ofn->nMaxFile = (ULONG)max (_r_str_getlength3 (&sr) + 1, 1024);
		ofn->lpstrFile = _r_mem_reallocatezero (ofn->lpstrFile, ofn->nMaxFile * sizeof (WCHAR));

		RtlCopyMemory (ofn->lpstrFile, sr.buffer, sr.length + sizeof (UNICODE_NULL));
	}
#endif // !APP_NO_DEPRECATIONS
}

VOID _r_filedialog_setfilter (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ COMDLG_FILTERSPEC * filters,
	_In_ ULONG count
)
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
		LPOPENFILENAME ofn;
		PR_STRING filter_string;
		R_STRINGBUILDER sb;

		ofn = file_dialog->u.ofn;

		_r_obj_initializestringbuilder (&sb);

		for (ULONG i = 0; i < count; i++)
		{
			_r_obj_appendstringbuilderformat (
				&sb,
				L"%s%c%s%c",
				filters[i].pszName,
				UNICODE_NULL,
				filters[i].pszSpec,
				UNICODE_NULL
			);
		}

		filter_string = _r_obj_finalstringbuilder (&sb);

		if (ofn->lpstrFilter)
			_r_mem_free ((PVOID)ofn->lpstrFilter);

		ofn->lpstrFilter = _r_mem_allocateandcopy (
			filter_string->buffer,
			filter_string->length + sizeof (UNICODE_NULL)
		);

		_r_obj_dereference (filter_string);
	}
#endif // !APP_NO_DEPRECATIONS
}

VOID _r_filedialog_destroy (
	_In_ PR_FILE_DIALOG file_dialog
)
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
		LPOPENFILENAME ofn;

		ofn = file_dialog->u.ofn;

		if (ofn->lpstrFilter)
			_r_mem_free ((PVOID)ofn->lpstrFilter);

		if (ofn->lpstrFile)
			_r_mem_free (ofn->lpstrFile);

		_r_mem_free (ofn);
	}
#endif // !APP_NO_DEPRECATIONS
}

//
// Window layout
//

_Success_ (return)
BOOLEAN _r_layout_initializemanager (
	_Out_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd
)
{
	R_RECTANGLE client_rect;
	R_RECTANGLE rect;
	LONG dpi_value;

	if (!_r_wnd_getposition (hwnd, &rect))
		return FALSE;

	if (!_r_wnd_getclientsize (hwnd, &client_rect))
		return FALSE;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_dc_getsizedpivalue (&rect.size, dpi_value, FALSE);
	_r_dc_getsizedpivalue (&client_rect.size, dpi_value, FALSE);

	_r_wnd_copyrectangle (&layout_manager->root_item.rect, &client_rect);

	layout_manager->root_item.hwnd = hwnd;
	layout_manager->root_item.defer_handle = NULL;
	layout_manager->root_item.parent_item = NULL;
	layout_manager->root_item.number_of_children = 0;
	layout_manager->root_item.flags = 0;

	layout_manager->dpi_value = dpi_value;

	layout_manager->original_size = rect.size;

	layout_manager->list = _r_obj_createlist_ex (1, &_r_mem_free);

	// Add sizing border for windows
	_r_wnd_addstyle (
		hwnd,
		0,
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
		GWL_STYLE
	);

	// Enumerate child control and windows
	_r_layout_enumcontrols (layout_manager, &layout_manager->root_item, hwnd);

	return TRUE;
}

VOID _r_layout_destroymanager (
	_Inout_ PR_LAYOUT_MANAGER layout_manager
)
{
	SAFE_DELETE_REFERENCE (layout_manager->list);
}

_Ret_maybenull_
PR_LAYOUT_ITEM _r_layout_additem (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ PR_LAYOUT_ITEM parent_item,
	_In_ HWND hwnd,
	_In_ ULONG flags
)
{
	PR_LAYOUT_ITEM layout_item;
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return NULL;

	flags |= _r_layout_getcontrolflags (hwnd);

	layout_item = _r_mem_allocatezero (sizeof (R_LAYOUT_ITEM));

	layout_item->parent_item = parent_item;
	layout_item->hwnd = hwnd;
	layout_item->flags = flags;

	MapWindowPoints (HWND_DESKTOP, parent_item->hwnd, (PPOINT)&rect, 2);

	_r_wnd_recttorectangle (&layout_item->rect, &rect);

	_r_dc_getsizedpivalue (&layout_item->rect.position, layout_manager->dpi_value, FALSE);
	_r_dc_getsizedpivalue (&layout_item->rect.size, layout_manager->dpi_value, FALSE);

	_r_wnd_copyrectangle (&layout_item->prev_rect, &layout_item->rect);

	// set control anchor points
	_r_layout_setitemanchor (layout_item);

	layout_item->parent_item->number_of_children += 1;

	_r_obj_addlistitem (layout_manager->list, layout_item);

	return layout_item;
}

BOOL CALLBACK _r_layout_enumcontrolscallback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	PR_LAYOUT_ENUM layout_enum;
	PR_LAYOUT_ITEM layout_item;

	layout_enum = (PR_LAYOUT_ENUM)lparam;

	if (!IsWindow (hwnd) || _r_wnd_ismenu (hwnd))
		return TRUE;

	// Ensure child is a direct descendent of parent (mandatory for DeferWindowPos).
	if (GetParent (hwnd) != layout_enum->root_hwnd)
		return TRUE;

	layout_item = _r_layout_additem (layout_enum->layout_manager, layout_enum->layout_item, hwnd, 0);

	if (layout_item)
	{
		if (_r_wnd_isdialog (hwnd))
			_r_layout_enumcontrols (layout_enum->layout_manager, layout_item, hwnd);
	}

	return TRUE;
}

VOID _r_layout_enumcontrols (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ PR_LAYOUT_ITEM layout_item,
	_In_ HWND hwnd
)
{
	R_LAYOUT_ENUM layout_enum = {0};

	layout_enum.layout_manager = layout_manager;
	layout_enum.layout_item = layout_item;
	layout_enum.root_hwnd = hwnd;

	EnumChildWindows (hwnd, &_r_layout_enumcontrolscallback, (LPARAM)&layout_enum);
}

ULONG _r_layout_getcontrolflags (
	_In_ HWND hwnd
)
{
	WCHAR class_name[128];
	R_STRINGREF sr;
	ULONG length;

	length = GetClassName (hwnd, class_name, RTL_NUMBER_OF (class_name));

	if (!length)
		return 0;

	_r_obj_initializestringref_ex (&sr, class_name, length * sizeof (WCHAR));

	if (_r_str_isequal2 (&sr, WC_STATIC, TRUE))
	{
		return PR_LAYOUT_FORCE_INVALIDATE;
	}
	else if (_r_str_isequal2 (&sr, STATUSCLASSNAME, TRUE))
	{
		return PR_LAYOUT_SEND_NOTIFY | PR_LAYOUT_NO_ANCHOR;
	}
	else if (_r_str_isequal2 (&sr, REBARCLASSNAME, TRUE))
	{
		return PR_LAYOUT_SEND_NOTIFY | PR_LAYOUT_NO_ANCHOR;
	}

	return 0;
}

BOOLEAN _r_layout_resize (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ WPARAM wparam
)
{
	PR_LAYOUT_ITEM layout_item;
	R_RECTANGLE rect;
	LONG dpi_value;

	if (wparam != SIZE_RESTORED && wparam != SIZE_MAXIMIZED)
		return FALSE;

	if (!_r_wnd_getclientsize (layout_manager->root_item.hwnd, &rect))
		return FALSE;

	dpi_value = _r_dc_getwindowdpi (layout_manager->root_item.hwnd);

	layout_manager->dpi_value = dpi_value;

	_r_dc_getsizedpivalue (&rect.size, dpi_value, FALSE);

	_r_wnd_copyrectangle (&layout_manager->root_item.prev_rect, &layout_manager->root_item.rect);
	_r_wnd_copyrectangle (&layout_manager->root_item.rect, &rect);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		if (layout_item->flags & PR_LAYOUT_SEND_NOTIFY)
			SendMessage (layout_item->hwnd, WM_SIZE, 0, 0);

		if (!(layout_item->flags & PR_LAYOUT_NO_ANCHOR))
			_r_layout_resizeitem (layout_manager, layout_item);
	}

	for (SIZE_T i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		if (layout_item->defer_handle)
		{
			EndDeferWindowPos (layout_item->defer_handle);

			layout_item->defer_handle = NULL;
		}

		if (layout_item->flags & PR_LAYOUT_FORCE_INVALIDATE)
			InvalidateRect (layout_item->hwnd, NULL, FALSE);
	}

	if (layout_manager->root_item.defer_handle)
	{
		EndDeferWindowPos (layout_manager->root_item.defer_handle);

		layout_manager->root_item.defer_handle = NULL;
	}

	return TRUE;
}

VOID _r_layout_resizeitem (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_Inout_ PR_LAYOUT_ITEM layout_item
)
{
	R_RECTANGLE rectangle;
	RECT parent_rect;
	RECT prev_rect;
	RECT rect;
	LONG width;
	LONG height;
	LONG sx;
	LONG sy;

	if (layout_item->number_of_children > 0 && !layout_item->defer_handle)
		layout_item->defer_handle = BeginDeferWindowPos (layout_item->number_of_children);

	if (!layout_item->parent_item)
		return;

	_r_layout_resizeitem (layout_manager, layout_item->parent_item);

	// save previous value
	_r_wnd_rectangletorect (&rect, &layout_item->rect);
	_r_wnd_rectangletorect (&prev_rect, &layout_item->parent_item->prev_rect);
	_r_wnd_rectangletorect (&parent_rect, &layout_item->parent_item->rect);

	_r_wnd_copyrectangle (&layout_item->prev_rect, &layout_item->rect);

	width = layout_item->parent_item->rect.width;
	height = layout_item->parent_item->rect.height;

	// horizontal
	if ((layout_item->flags & PR_LAYOUT_ANCHOR_LEFT) && (layout_item->flags & PR_LAYOUT_ANCHOR_RIGHT))
	{
		rect.right += parent_rect.right - prev_rect.right;
	}
	else if (layout_item->flags & PR_LAYOUT_ANCHOR_LEFT)
	{
		// left is default
	}
	else if (layout_item->flags & PR_LAYOUT_ANCHOR_RIGHT)
	{
		rect.right += parent_rect.right - prev_rect.right;
		rect.left += parent_rect.right - prev_rect.right;
	}
	else
	{
		// relative move
		sx = ((parent_rect.right - parent_rect.left) / 2) - ((prev_rect.right - prev_rect.left) / 2);

		rect.right += sx;
		rect.left += sx;
	}

	// vertical
	if ((layout_item->flags & PR_LAYOUT_ANCHOR_TOP) && (layout_item->flags & PR_LAYOUT_ANCHOR_BOTTOM))
	{
		rect.bottom += parent_rect.bottom - prev_rect.bottom;
	}
	else if (layout_item->flags & PR_LAYOUT_ANCHOR_TOP)
	{
		// top is default
	}
	else if (layout_item->flags & PR_LAYOUT_ANCHOR_BOTTOM)
	{
		rect.bottom += parent_rect.bottom - prev_rect.bottom;
		rect.top += parent_rect.bottom - prev_rect.bottom;
	}
	else
	{
		// relative move
		sy = ((parent_rect.bottom - parent_rect.top) / 2) - ((prev_rect.bottom - prev_rect.top) / 2);

		rect.bottom += sy;
		rect.top += sy;
	}

	if (layout_item->flags & PR_LAYOUT_DOCK_LEFT)
		rect.left = parent_rect.left;

	if (layout_item->flags & PR_LAYOUT_DOCK_TOP)
		rect.top = parent_rect.top;

	if (layout_item->flags & PR_LAYOUT_DOCK_RIGHT)
		rect.right = parent_rect.right;

	if (layout_item->flags & PR_LAYOUT_DOCK_BOTTOM)
		rect.bottom = parent_rect.bottom;

	// copy before conversion
	_r_wnd_recttorectangle (&rectangle, &rect);
	_r_wnd_copyrectangle (&layout_item->rect, &rectangle);

	_r_dc_getsizedpivalue (&rectangle.position, layout_manager->dpi_value, TRUE);
	_r_dc_getsizedpivalue (&rectangle.size, layout_manager->dpi_value, TRUE);

	layout_item->parent_item->defer_handle = DeferWindowPos (
		layout_item->parent_item->defer_handle,
		layout_item->hwnd,
		NULL,
		rectangle.left,
		rectangle.top,
		rectangle.width,
		rectangle.height,
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER
	);
}

VOID _r_layout_resizeminimumsize (
	_In_ PR_LAYOUT_MANAGER layout_manager,
	_Inout_ LPARAM lparam
)
{
	PMINMAXINFO minmax;
	R_SIZE point;

	minmax = (PMINMAXINFO)lparam;

	point = layout_manager->original_size;

	_r_dc_getsizedpivalue (&point, layout_manager->dpi_value, TRUE);

	minmax->ptMinTrackSize.x = point.cx;
	minmax->ptMinTrackSize.y = point.cy;
}

VOID _r_layout_setoriginalsize (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ LONG width,
	_In_ LONG height
)
{
	layout_manager->original_size.cx = width;
	layout_manager->original_size.cy = height;
}

VOID _r_layout_setitemanchor (
	_Inout_ PR_LAYOUT_ITEM layout_item
)
{
	LONG width;
	LONG height;
	LONG horz_break;
	LONG vert_break;

	// Compute control edge points if it did not set
	if (layout_item->flags & (PR_LAYOUT_NO_ANCHOR | PR_LAYOUT_ANCHOR_ALL))
		return;

	width = layout_item->parent_item->rect.width;
	height = layout_item->parent_item->rect.height;

	horz_break = _r_calc_percentval (48, width);
	vert_break = _r_calc_percentval (78, height);

	// If the left-edge of the control is within the left-half of the client area, set a left-anchor.
	if (layout_item->rect.left < horz_break)
		layout_item->flags |= PR_LAYOUT_ANCHOR_LEFT;

	// If the top-edge of the control is within the upper-half of the client area, set a top-anchor.
	if (layout_item->rect.top < vert_break)
		layout_item->flags |= PR_LAYOUT_ANCHOR_TOP;

	// If the right-edge of the control is within the right-half of the client area, set a right-anchor.
	if ((layout_item->rect.left + layout_item->rect.width) > horz_break)
		layout_item->flags |= PR_LAYOUT_ANCHOR_RIGHT;

	// If the bottom-edge of the control is within the lower-half of the client area, set a bottom-anchor.
	if ((layout_item->rect.top + layout_item->rect.height) > vert_break)
		layout_item->flags |= PR_LAYOUT_ANCHOR_BOTTOM;
}

BOOLEAN _r_layout_setwindowanchor (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd,
	_In_ ULONG anchor
)
{
	PR_LAYOUT_ITEM layout_item;

	for (SIZE_T i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item || layout_item->hwnd != hwnd)
			continue;

		layout_item->flags &= ~(PR_LAYOUT_ANCHOR_ALL);
		layout_item->flags |= anchor; // set new anchor flags

		return TRUE;
	}

	return FALSE;
}

//
// Window management
//

VOID _r_wnd_addstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG_PTR mask,
	_In_ LONG_PTR state_mask,
	_In_ INT index
)
{
	HWND htarget;
	LONG_PTR style;

	if (ctrl_id)
	{
		htarget = GetDlgItem (hwnd, ctrl_id);

		if (!htarget)
			return;
	}
	else
	{
		htarget = hwnd;
	}

	style = (GetWindowLongPtr (htarget, index) & ~state_mask) | mask;

	SetWindowLongPtr (htarget, index, style);

	SetWindowPos (
		htarget,
		NULL,
		0,
		0,
		0,
		0,
		SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER
	);
}

VOID _r_wnd_adjustrectangletobounds (
	_Inout_ PR_RECTANGLE rectangle,
	_In_ PR_RECTANGLE bounds
)
{
	if (rectangle->left + rectangle->width > bounds->left + bounds->width)
		rectangle->left = bounds->left + bounds->width - rectangle->width;

	if (rectangle->top + rectangle->height > bounds->top + bounds->height)
		rectangle->top = bounds->top + bounds->height - rectangle->height;

	if (rectangle->left < bounds->left)
		rectangle->left = bounds->left;

	if (rectangle->top < bounds->top)
		rectangle->top = bounds->top;
}

VOID _r_wnd_adjustrectangletoworkingarea (
	_Inout_ PR_RECTANGLE rectangle,
	_In_opt_ HWND hwnd
)
{
	MONITORINFO monitor_info;
	HMONITOR hmonitor;
	R_RECTANGLE bounds;
	RECT rect;

	if (hwnd)
	{
		hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
	}
	else
	{
		_r_wnd_rectangletorect (&rect, rectangle);

		hmonitor = MonitorFromRect (&rect, MONITOR_DEFAULTTONEAREST);
	}

	monitor_info.cbSize = sizeof (monitor_info);

	if (GetMonitorInfo (hmonitor, &monitor_info))
	{
		_r_wnd_recttorectangle (&bounds, &monitor_info.rcWork);
		_r_wnd_adjustrectangletobounds (rectangle, &bounds);
	}
}

VOID _r_wnd_calculateoverlappedrect (
	_In_ HWND hwnd,
	_Inout_ PRECT window_rect
)
{
	RECT rect_current;
	RECT rect_intersection;
	HWND hwnd_current;

	hwnd_current = hwnd;

	while ((hwnd_current = GetWindow (hwnd_current, GW_HWNDPREV)) && hwnd_current != hwnd)
	{
		if (!(_r_wnd_getstyle (hwnd_current) & WS_VISIBLE))
			continue;

		if (!GetWindowRect (hwnd_current, &rect_current))
			continue;

		if ((_r_wnd_ismenu (hwnd_current) || !(_r_wnd_getstyle_ex (hwnd_current) & WS_EX_TOPMOST)) &&
			IntersectRect (&rect_intersection, window_rect, &rect_current))
		{
			if (rect_current.left < window_rect->left)
				window_rect->left -= (window_rect->left - rect_current.left);

			if (rect_current.top < window_rect->top)
				window_rect->top -= (window_rect->top - rect_current.top);

			if (rect_current.bottom > window_rect->bottom)
				window_rect->bottom += (rect_current.bottom - window_rect->bottom);

			if (rect_current.right > window_rect->right)
				window_rect->right += (rect_current.right - window_rect->right);
		}
	}
}

VOID _r_wnd_center (
	_In_ HWND hwnd,
	_In_opt_ HWND hparent
)
{
	MONITORINFO monitor_info;
	R_RECTANGLE rectangle;
	R_RECTANGLE parent_rect;
	HMONITOR hmonitor;

	if (hparent)
	{
		if (_r_wnd_isvisible_ex (hparent) &&
			_r_wnd_getposition (hwnd, &rectangle) &&
			_r_wnd_getposition (hparent, &parent_rect))
		{
			_r_wnd_centerwindowrect (&rectangle, &parent_rect);
			_r_wnd_adjustrectangletoworkingarea (&rectangle, hwnd);

			_r_wnd_setposition (hwnd, &rectangle.position, NULL);

			return;
		}
	}

	monitor_info.cbSize = sizeof (monitor_info);

	hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

	if (!GetMonitorInfo (hmonitor, &monitor_info))
		return;

	if (!_r_wnd_getposition (hwnd, &rectangle))
		return;

	_r_wnd_recttorectangle (&parent_rect, &monitor_info.rcWork);
	_r_wnd_centerwindowrect (&rectangle, &parent_rect);

	_r_wnd_setposition (hwnd, &rectangle.position, NULL);
}

VOID _r_wnd_centerwindowrect (
	_In_ PR_RECTANGLE rectangle,
	_In_ PR_RECTANGLE bounds
)
{
	rectangle->left = bounds->left + (bounds->width - rectangle->width) / 2;
	rectangle->top = bounds->top + (bounds->height - rectangle->height) / 2;
}

VOID _r_wnd_changemessagefilter (
	_In_ HWND hwnd,
	_In_reads_ (count) PUINT messages,
	_In_ SIZE_T count,
	_In_ ULONG action
)
{
#if defined(APP_NO_DEPRECATIONS)

	for (SIZE_T i = 0; i < count; i++)
	{
		ChangeWindowMessageFilterEx (hwnd, messages[i], action, NULL); // win7+
	}

#else

	HINSTANCE huser32;
	CWMFEX _ChangeWindowMessageFilterEx;

	huser32 = _r_sys_loadlibrary (L"user32.dll");

	if (huser32)
	{
		// win7+
		_ChangeWindowMessageFilterEx = (CWMFEX)GetProcAddress (huser32, "ChangeWindowMessageFilterEx");

		if (_ChangeWindowMessageFilterEx)
		{
			for (SIZE_T i = 0; i < count; i++)
			{
				_ChangeWindowMessageFilterEx (hwnd, messages[i], action, NULL);
			}
		}

		FreeLibrary (huser32);
	}
#endif // APP_NO_DEPRECATIONS
}

VOID _r_wnd_copyrectangle (
	_Out_ PR_RECTANGLE rectangle_dst,
	_In_ PR_RECTANGLE rectangle_src
)
{
	*rectangle_dst = *rectangle_src;
}

_Ret_maybenull_
HWND _r_wnd_createwindow (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
)
{
	R_BYTEREF buffer;
	HWND hwnd;

	if (!_r_res_loadresource (hinstance, name, RT_DIALOG, &buffer))
		return NULL;

	hwnd = CreateDialogIndirectParam (
		hinstance,
		(LPDLGTEMPLATE)buffer.buffer,
		hparent,
		dlg_proc,
		(LPARAM)lparam
	);

	return hwnd;
}

INT_PTR _r_wnd_createmodalwindow (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
)
{
	R_BYTEREF buffer;
	INT_PTR result;

	if (!_r_res_loadresource (hinstance, name, RT_DIALOG, &buffer))
		return 0;

	result = DialogBoxIndirectParam (
		hinstance,
		(LPDLGTEMPLATE)buffer.buffer,
		hparent,
		dlg_proc,
		(LPARAM)lparam
	);

	return result;
}

_Success_ (return)
BOOLEAN _r_wnd_getclientsize (
	_In_ HWND hwnd,
	_Out_ PR_RECTANGLE rectangle
)
{
	RECT rect;

	if (!GetClientRect (hwnd, &rect))
		return FALSE;

	_r_wnd_setrectangle (rectangle, 0, 0, rect.right, rect.bottom);

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_wnd_getposition (
	_In_ HWND hwnd,
	_Out_ PR_RECTANGLE rectangle
)
{
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return FALSE;

	_r_wnd_recttorectangle (rectangle, &rect);

	return TRUE;
}

LONG_PTR _r_wnd_getstyle (
	_In_ HWND hwnd
)
{
	return GetWindowLongPtr (hwnd, GWL_STYLE);
}

LONG_PTR _r_wnd_getstyle_ex (
	_In_ HWND hwnd
)
{
	return GetWindowLongPtr (hwnd, GWL_EXSTYLE);
}

BOOLEAN _r_wnd_isdesktop (
	_In_ HWND hwnd
)
{
	ULONG_PTR atom;

	atom = GetClassLongPtr (hwnd, GCW_ATOM);

	// #32769
	if (atom == 0x8001)
		return TRUE;

	return FALSE;
}

BOOLEAN _r_wnd_isdialog (
	_In_ HWND hwnd
)
{
	ULONG_PTR atom;

	atom = GetClassLongPtr (hwnd, GCW_ATOM);

	// #32770
	if (atom == 0x8002)
		return TRUE;

	return FALSE;
}

BOOLEAN _r_wnd_isfullscreenconsolemode (
	_In_ HWND hwnd
)
{
	ULONG pid = 0;
	ULONG modes = 0;

	GetWindowThreadProcessId (hwnd, &pid);

	if (!pid)
		return FALSE;

	if (!AttachConsole (pid))
		return FALSE;

	GetConsoleDisplayMode (&modes);

	FreeConsole ();

	return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

BOOLEAN _r_wnd_isfullscreenusermode ()
{
	QUERY_USER_NOTIFICATION_STATE state = 0;

	// SHQueryUserNotificationState is only available for Vista+
#if defined(APP_NO_DEPRECATIONS)
	if (SHQueryUserNotificationState (&state) != S_OK)
		return FALSE;
#else
	HINSTANCE hshell32;
	SHQUNS _SHQueryUserNotificationState;

	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return FALSE;

	hshell32 = _r_sys_loadlibrary (L"shell32.dll");

	if (hshell32)
	{
		// vista+
		_SHQueryUserNotificationState = (SHQUNS)GetProcAddress (hshell32, "SHQueryUserNotificationState");

		FreeLibrary (hshell32);

		if (_SHQueryUserNotificationState)
		{
			if (_SHQueryUserNotificationState (&state) != S_OK)
				return FALSE;
		}
	}
#endif // APP_NO_DEPRECATIONS

	return (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE);
}

BOOLEAN _r_wnd_isfullscreenwindowmode (
	_In_ HWND hwnd
)
{
	MONITORINFO monitor_info;
	RECT wnd_rect;
	HMONITOR hmonitor;
	LONG_PTR style;
	LONG_PTR ex_style;

	// Get the monitor where the window is located.
	if (!GetWindowRect (hwnd, &wnd_rect))
		return FALSE;

	hmonitor = MonitorFromRect (&wnd_rect, MONITOR_DEFAULTTONULL);

	if (!hmonitor)
		return FALSE;

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
	style = _r_wnd_getstyle (hwnd);
	ex_style = _r_wnd_getstyle_ex (hwnd);

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
			 (ex_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

BOOLEAN _r_wnd_isfullscreenmode ()
{
	HWND hwnd;

	if (_r_wnd_isfullscreenusermode ())
		return TRUE;

	// Get the foreground window which the user is currently working on.
	hwnd = GetForegroundWindow ();

	if (hwnd)
		return (_r_wnd_isfullscreenwindowmode (hwnd) || _r_wnd_isfullscreenconsolemode (hwnd));

	return FALSE;
}

BOOLEAN _r_wnd_ismaximized (
	_In_ HWND hwnd
)
{
	LONG_PTR style;

	style = _r_wnd_getstyle (hwnd);

	if (style & WS_MAXIMIZE)
		return TRUE;

	return FALSE;
}

BOOLEAN _r_wnd_ismenu (
	_In_ HWND hwnd
)
{
	ULONG_PTR atom;

	atom = GetClassLongPtr (hwnd, GCW_ATOM);

	// #32768
	if (atom == 0x8000)
		return TRUE;

	return FALSE;
}

BOOLEAN _r_wnd_isminimized (
	_In_ HWND hwnd
)
{
	LONG_PTR style;

	style = _r_wnd_getstyle (hwnd);

	if (style & WS_MINIMIZE)
		return TRUE;

	return FALSE;
}

BOOLEAN _r_wnd_isoverlapped (
	_In_ HWND hwnd
)
{
	RECT rect_original;
	RECT rect_current;
	RECT rect_intersection;
	HWND hwnd_current;

	if (!GetWindowRect (hwnd, &rect_original))
		return FALSE;

	hwnd_current = hwnd;

	while ((hwnd_current = GetWindow (hwnd_current, GW_HWNDPREV)) && hwnd_current != hwnd)
	{
		if (!(_r_wnd_getstyle (hwnd_current) & WS_VISIBLE))
			continue;

		if (!GetWindowRect (hwnd_current, &rect_current))
			continue;

		if (!(_r_wnd_getstyle_ex (hwnd_current) & WS_EX_TOPMOST) &&
			IntersectRect (&rect_intersection, &rect_original, &rect_current))
		{
			return TRUE;
		}
	}

	return FALSE;
}

// Author: Mikhail
// https://stackoverflow.com/a/9126096

BOOLEAN _r_wnd_isundercursor (
	_In_ HWND hwnd
)
{
	RECT rect;
	POINT point;

	if (!_r_wnd_isvisible_ex (hwnd))
		return FALSE;

	if (!GetCursorPos (&point) || !GetWindowRect (hwnd, &rect))
		return FALSE;

	return !!PtInRect (&rect, point);
}

BOOLEAN _r_wnd_isvisible (
	_In_ HWND hwnd
)
{
	return !!IsWindowVisible (hwnd);
}

BOOLEAN _r_wnd_isvisible_ex (
	_In_ HWND hwnd
)
{
	LONG_PTR style;

	style = _r_wnd_getstyle (hwnd);

	if (style & WS_MINIMIZE)
		return FALSE;

	return !!IsWindowVisible (hwnd);
}

ULONG CALLBACK _r_wnd_message_callback (
	_In_ HWND main_wnd,
	_In_ LPCWSTR accelerator_table
)
{
	MSG msg;
	HWND hactive_wnd;
	HACCEL haccelerator;
	INT result;
	BOOLEAN is_processed;

	haccelerator = LoadAccelerators (NULL, accelerator_table);

	if (!haccelerator)
		return GetLastError ();

	while (TRUE)
	{
		result = GetMessage (&msg, NULL, 0, 0);

		if (result <= 0)
			break;

		is_processed = FALSE;

		hactive_wnd = GetActiveWindow ();

		if (!hactive_wnd)
		{
			if (msg.hwnd && _r_wnd_isdialog (msg.hwnd))
			{
				hactive_wnd = msg.hwnd;
			}
			else
			{
				hactive_wnd = main_wnd;
			}
		}

		if (TranslateAccelerator (hactive_wnd, haccelerator, &msg))
			is_processed = TRUE;

		if (IsDialogMessage (main_wnd, &msg))
			is_processed = TRUE;

		if (!is_processed)
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}

	DestroyAcceleratorTable (haccelerator);

	return ERROR_SUCCESS;
}

VOID CALLBACK _r_wnd_message_dpichanged (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
)
{
	R_RECTANGLE rectangle;
	LPCRECT rect;

	UNREFERENCED_PARAMETER (wparam);

	rect = (LPCRECT)lparam;

	if (!rect)
		return;

	_r_wnd_recttorectangle (&rectangle, rect);

	_r_wnd_setposition (hwnd, &rectangle.position, &rectangle.size);
}

VOID CALLBACK _r_wnd_message_settingchange (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
)
{
	R_STRINGREF sr;
	LPWSTR type;

	UNREFERENCED_PARAMETER (wparam);

	type = (LPWSTR)lparam;

	if (!type)
		return;

	_r_obj_initializestringref (&sr, type);

	if (_r_str_isequal2 (&sr, L"WindowMetrics", TRUE))
	{
		SendMessage (hwnd, RM_LOCALIZE, 0, 0);
	}
}

VOID _r_wnd_rectangletorect (
	_Out_ PRECT rect,
	_In_ PR_RECTANGLE rectangle
)
{
	SetRect (
		rect,
		rectangle->left,
		rectangle->top,
		rectangle->left + rectangle->width,
		rectangle->top + rectangle->height
	);
}

VOID _r_wnd_recttorectangle (
	_Out_ PR_RECTANGLE rectangle,
	_In_ LPCRECT rect
)
{
	rectangle->left = rect->left;
	rectangle->top = rect->top;
	rectangle->width = rect->right - rect->left;
	rectangle->height = rect->bottom - rect->top;
}

VOID _r_wnd_seticon (
	_In_ HWND hwnd,
	_In_opt_ HICON hicon_small,
	_In_opt_ HICON hicon_big
)
{
	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon_small);
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon_big);
}

VOID _r_wnd_setrectangle (
	_Out_ PR_RECTANGLE rectangle,
	_In_ LONG left,
	_In_ LONG top,
	_In_ LONG width,
	_In_ LONG height
)
{
	rectangle->left = left;
	rectangle->top = top;
	rectangle->width = width;
	rectangle->height = height;
}

VOID _r_wnd_setposition (
	_In_ HWND hwnd,
	_In_opt_ PR_SIZE position,
	_In_opt_ PR_SIZE size
)
{
	R_RECTANGLE rectangle = {0};
	UINT swp_flags;

	swp_flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	if (position)
	{
		rectangle.position = *position;
	}
	else
	{
		swp_flags |= SWP_NOMOVE;
	}

	if (size)
	{
		rectangle.size = *size;
	}
	else
	{
		swp_flags |= SWP_NOSIZE;
	}

	SetWindowPos (
		hwnd,
		NULL,
		rectangle.left,
		rectangle.top,
		rectangle.width,
		rectangle.height,
		swp_flags
	);
}

VOID _r_wnd_setstyle (
	_In_ HWND hwnd,
	_In_ LONG_PTR style
)
{
	SetWindowLongPtr (hwnd, GWL_STYLE, style);
}

VOID _r_wnd_setstyle_ex (
	_In_ HWND hwnd,
	_In_ LONG_PTR ex_style
)
{
	SetWindowLongPtr (hwnd, GWL_EXSTYLE, ex_style);
}

VOID _r_wnd_toggle (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_show
)
{
	BOOLEAN is_success;
	BOOLEAN is_minimized;

	is_minimized = _r_wnd_isminimized (hwnd);

	if (is_show || !_r_wnd_isvisible (hwnd) || is_minimized || _r_wnd_isoverlapped (hwnd))
	{
		is_success = !!ShowWindow (hwnd, is_minimized ? SW_RESTORE : SW_SHOW);

		if (!is_success)
		{
			// uipi fix
			if (GetLastError () == ERROR_ACCESS_DENIED)
				SendMessage (hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
		}

		SetForegroundWindow (hwnd);
	}
	else
	{
		ShowWindow (hwnd, SW_HIDE);
	}
}

VOID _r_wnd_top (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	SetWindowPos (
		hwnd,
		is_enable ? HWND_TOPMOST : HWND_NOTOPMOST,
		0,
		0,
		0,
		0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER
	);
}

PR_HASHTABLE _r_wnd_getcontext_table ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_HASHTABLE hashtable = NULL;

	if (_r_initonce_begin (&init_once))
	{
		hashtable = _r_obj_createhashtable_ex (sizeof (R_OBJECT_POINTER), 8, NULL);

		_r_initonce_end (&init_once);
	}

	return hashtable;
}

ULONG _r_wnd_getcontext_hash (
	_In_ HWND hwnd,
	_In_ ULONG property_id
)
{
	ULONG hash_code;

	hash_code = _r_math_hashinteger_ptr ((ULONG_PTR)hwnd) ^ _r_math_hashinteger32 (property_id);

	return hash_code;
}

_Ret_maybenull_
PVOID _r_wnd_getcontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id
)
{
	PR_HASHTABLE hashtable;
	PR_OBJECT_POINTER object_pointer;
	ULONG hash_code;

	hashtable = _r_wnd_getcontext_table ();
	hash_code = _r_wnd_getcontext_hash (hwnd, property_id);

	_r_queuedlock_acquireshared (&_r_context_lock);
	object_pointer = _r_obj_findhashtable (hashtable, hash_code);
	_r_queuedlock_releaseshared (&_r_context_lock);

	if (object_pointer)
		return object_pointer->object_body;

	return NULL;
}

VOID _r_wnd_setcontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id,
	_In_ PVOID context
)
{
	PR_HASHTABLE hashtable;
	R_OBJECT_POINTER object_pointer;
	ULONG hash_code;

	hashtable = _r_wnd_getcontext_table ();
	hash_code = _r_wnd_getcontext_hash (hwnd, property_id);

	object_pointer.object_body = context;

	_r_queuedlock_acquireshared (&_r_context_lock);
	_r_obj_addhashtableitem (hashtable, hash_code, &object_pointer);
	_r_queuedlock_releaseshared (&_r_context_lock);
}

VOID _r_wnd_removecontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id
)
{
	PR_HASHTABLE hashtable;
	ULONG hash_code;

	hashtable = _r_wnd_getcontext_table ();
	hash_code = _r_wnd_getcontext_hash (hwnd, property_id);

	_r_queuedlock_acquireexclusive (&_r_context_lock);
	_r_obj_removehashtableitem (hashtable, hash_code);
	_r_queuedlock_releaseexclusive (&_r_context_lock);
}

//
// Inernet access (WinHTTP)
//

_Ret_maybenull_
HINTERNET _r_inet_createsession (
	_In_opt_ PR_STRING useragent
)
{
	HINTERNET hsession;
	ULONG access_type;

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		access_type = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
	}
	else
	{
		access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
	}

	hsession = WinHttpOpen (_r_obj_getstring (useragent), access_type, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (!hsession)
		return NULL;

	if (_r_sys_isosversionlowerorequal (WINDOWS_8))
	{
		// enable secure protocols
		WinHttpSetOption (
			hsession,
			WINHTTP_OPTION_SECURE_PROTOCOLS,
			&(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
			WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2},
			sizeof (ULONG)
		);
	}
	else
	{
		// enable secure protocols
		WinHttpSetOption (
			hsession,
			WINHTTP_OPTION_SECURE_PROTOCOLS,
			&(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
			WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3},
			sizeof (ULONG)
		);

		// disable redirect from https to http
		WinHttpSetOption (
			hsession,
			WINHTTP_OPTION_REDIRECT_POLICY,
			&(ULONG){WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP},
			sizeof (ULONG)
		);

		// enable compression feature
		WinHttpSetOption (
			hsession,
			WINHTTP_OPTION_DECOMPRESSION,
			&(ULONG){WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE},
			sizeof (ULONG)
		);

		// enable http2 protocol (win10+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		{
			WinHttpSetOption (
				hsession,
				WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
				&(ULONG){WINHTTP_PROTOCOL_FLAG_HTTP2},
				sizeof (ULONG)
			);
		}
	}

	return hsession;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (
	_In_ HINTERNET hsession,
	_In_ PR_STRING url,
	_Out_ LPHINTERNET hconnect_ptr,
	_Out_ LPHINTERNET hrequest_ptr,
	_Out_opt_ PULONG total_length_ptr
)
{
	R_URLPARTS url_parts;
	HINTERNET hconnect;
	HINTERNET hrequest;
	ULONG attempts;
	ULONG flags;
	ULONG status;
	BOOL result;

	*hconnect_ptr = NULL;
	*hrequest_ptr = NULL;

	if (total_length_ptr)
		*total_length_ptr = 0;

	status = _r_inet_queryurlparts (
		url,
		PR_URLPARTS_SCHEME | PR_URLPARTS_HOST | PR_URLPARTS_PORT | PR_URLPARTS_PATH,
		&url_parts
	);

	if (status != ERROR_SUCCESS)
		goto CleanupExit;

	hconnect = WinHttpConnect (hsession, url_parts.host->buffer, url_parts.port, 0);

	if (!hconnect)
	{
		status = GetLastError ();

		goto CleanupExit;
	}

	flags = WINHTTP_FLAG_REFRESH;

	if (url_parts.scheme == INTERNET_SCHEME_HTTPS)
		flags |= WINHTTP_FLAG_SECURE;

	hrequest = WinHttpOpenRequest (
		hconnect,
		NULL,
		url_parts.path->buffer,
		NULL,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		flags
	);

	if (!hrequest)
	{
		status = GetLastError ();

		_r_inet_close (hconnect);

		goto CleanupExit;
	}

	// disable "keep-alive" feature (win7+)
#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
#endif // !APP_NO_DEPRECATIONS
	{
		WinHttpSetOption (
			hrequest,
			WINHTTP_OPTION_DISABLE_FEATURE,
			&(ULONG){WINHTTP_DISABLE_KEEP_ALIVE},
			sizeof (ULONG)
		);
	}

	attempts = 6;

	do
	{
		result = WinHttpSendRequest (
			hrequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0,
			WINHTTP_NO_REQUEST_DATA,
			0,
			WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH,
			0
		);

		if (!result)
		{
			status = GetLastError ();

			if (status == ERROR_WINHTTP_RESEND_REQUEST)
			{
				continue;
			}
			else if (status == ERROR_WINHTTP_CONNECTION_ERROR)
			{
				result = WinHttpSetOption (
					hsession,
					WINHTTP_OPTION_SECURE_PROTOCOLS,
					&(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
					WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2},
					sizeof (ULONG)
				);

				if (!result)
					break;
			}
			else if (status == ERROR_WINHTTP_SECURE_FAILURE)
			{
				result = WinHttpSetOption (
					hrequest,
					WINHTTP_OPTION_SECURITY_FLAGS,
					&(ULONG){SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE},
					sizeof (ULONG)
				);

				if (!result)
					break;
			}
			else
			{
				// ERROR_WINHTTP_NAME_NOT_RESOLVED
				// ERROR_NOT_ENOUGH_MEMORY
				// ERROR_WINHTTP_CANNOT_CONNECT etc.

				break;
			}
		}
		else
		{
			if (!WinHttpReceiveResponse (hrequest, NULL))
			{
				status = GetLastError ();
			}
			else
			{
				if (_r_inet_querystatuscode (hrequest) != HTTP_STATUS_OK)
				{
					status = ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
					break;
				}

				if (total_length_ptr)
					*total_length_ptr = _r_inet_querycontentlength (hrequest);

				*hconnect_ptr = hconnect;
				*hrequest_ptr = hrequest;

				status = ERROR_SUCCESS;

				goto CleanupExit;
			}
		}
	}
	while (--attempts);

	_r_inet_close (hrequest);
	_r_inet_close (hconnect);

CleanupExit:

	_r_inet_destroyurlparts (&url_parts);

	return status;
}

_Success_ (return)
BOOLEAN _r_inet_readrequest (
	_In_ HINTERNET hrequest,
	_Out_writes_bytes_ (buffer_size) PVOID buffer,
	_In_ ULONG buffer_size,
	_Out_ PULONG readed_ptr,
	_Inout_opt_ PULONG total_readed_ptr
)
{
	ULONG readed;

	if (!WinHttpReadData (hrequest, buffer, buffer_size, &readed))
	{
		*readed_ptr = 0;

		return FALSE;
	}

	if (!readed)
	{
		*readed_ptr = 0;

		return FALSE;
	}

	*readed_ptr = readed;

	if (total_readed_ptr)
		*total_readed_ptr += readed;

	return TRUE;
}

ULONG _r_inet_querycontentlength (
	_In_ HINTERNET hrequest
)
{
	ULONG content_length;
	ULONG size;

	size = sizeof (ULONG);

	if (WinHttpQueryHeaders (
		hrequest,
		WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&content_length,
		&size,
		WINHTTP_NO_HEADER_INDEX))
	{
		return content_length;
	}

	return 0;
}

LONG64 _r_inet_querylastmodified (
	_In_ HINTERNET hrequest
)
{
	SYSTEMTIME lastmod;
	ULONG size;

	size = sizeof (SYSTEMTIME);

	if (WinHttpQueryHeaders (
		hrequest,
		WINHTTP_QUERY_LAST_MODIFIED | WINHTTP_QUERY_FLAG_SYSTEMTIME,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&lastmod,
		&size,
		WINHTTP_NO_HEADER_INDEX))
	{
		return _r_unixtime_from_systemtime (&lastmod);
	}

	return 0;
}

ULONG _r_inet_querystatuscode (
	_In_ HINTERNET hrequest
)
{
	ULONG status;
	ULONG size;

	size = sizeof (ULONG);

	if (WinHttpQueryHeaders (
		hrequest,
		WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&status,
		&size,
		WINHTTP_NO_HEADER_INDEX))
	{
		return status;
	}

	return 0;
}

VOID _r_inet_initializedownload (
	_Out_ PR_DOWNLOAD_INFO download_info
)
{
	_r_inet_initializedownload_ex (download_info, NULL, NULL, NULL);
}

VOID _r_inet_initializedownload_ex (
	_Out_ PR_DOWNLOAD_INFO download_info,
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_INET_DOWNLOAD_CALLBACK download_callback,
	_In_opt_ PVOID lparam
)
{
	download_info->is_savetofile = (hfile != NULL);

	if (download_info->is_savetofile)
	{
		download_info->u.hfile = hfile;
	}
	else
	{
		download_info->u.string = NULL;
	}

	download_info->download_callback = download_callback;
	download_info->lparam = lparam;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_begindownload (
	_In_ HINTERNET hsession,
	_In_ PR_STRING url,
	_Inout_ PR_DOWNLOAD_INFO download_info
)
{
	R_STRINGBUILDER sb;
	HINTERNET hconnect;
	HINTERNET hrequest;
	PR_STRING string;
	PR_BYTE content_bytes;
	ULONG allocated_length;
	ULONG total_readed;
	ULONG total_length;
	ULONG readed_length;
	ULONG unused;
	LONG status;

	status = _r_inet_openurl (hsession, url, &hconnect, &hrequest, &total_length);

	if (status != ERROR_SUCCESS)
		return status;

	if (!download_info->is_savetofile)
		_r_obj_initializestringbuilder (&sb);

	allocated_length = PR_SIZE_INET_READ_BUFFER;
	content_bytes = _r_obj_createbyte_ex (NULL, allocated_length);

	total_readed = 0;

	while (_r_inet_readrequest (hrequest, content_bytes->buffer, allocated_length, &readed_length, &total_readed))
	{
		_r_sys_setthreadexecutionstate (ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);

		_r_obj_setbytelength_ex (content_bytes, readed_length, allocated_length);

		if (download_info->is_savetofile)
		{
			if (!WriteFile (download_info->u.hfile, content_bytes->buffer, readed_length, &unused, NULL))
			{
				status = GetLastError ();
				break;
			}
		}
		else
		{
			status = _r_str_multibyte2unicode (&content_bytes->sr, &string);

			if (status == STATUS_SUCCESS)
			{
				_r_obj_appendstringbuilder2 (&sb, string);
				_r_obj_dereference (string);
			}
			else
			{
				status = RtlNtStatusToDosError (status);
				break;
			}
		}

		if (download_info->download_callback)
		{
			if (!download_info->download_callback (total_readed, max (total_readed, total_length), download_info->lparam))
			{
				status = ERROR_CANCELLED;
				break;
			}
		}
	}

	if (!download_info->is_savetofile)
	{
		if (status == ERROR_SUCCESS)
		{
			string = _r_obj_finalstringbuilder (&sb);

			download_info->u.string = string;
		}
		else
		{
			_r_obj_deletestringbuilder (&sb);

			download_info->u.string = NULL;
		}
	}

	_r_sys_setthreadexecutionstate (ES_CONTINUOUS);

	_r_obj_dereference (content_bytes);

	_r_inet_close (hrequest);
	_r_inet_close (hconnect);

	return status;
}

VOID _r_inet_destroydownload (
	_Inout_ PR_DOWNLOAD_INFO download_info
)
{
	if (download_info->is_savetofile)
	{
		SAFE_DELETE_HANDLE (download_info->u.hfile);
	}
	else
	{
		SAFE_DELETE_REFERENCE (download_info->u.string);
	}
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_queryurlparts (
	_In_ PR_STRING url,
	_In_ ULONG flags,
	_Out_ PR_URLPARTS url_parts
)
{
	URL_COMPONENTS url_comp = {0};
	ULONG length;
	ULONG status;

	url_comp.dwStructSize = sizeof (url_comp);

	RtlZeroMemory (url_parts, sizeof (R_URLPARTS));

	length = 256;

	if (flags & PR_URLPARTS_HOST)
	{
		url_parts->host = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

		url_comp.lpszHostName = url_parts->host->buffer;
		url_comp.dwHostNameLength = length;
	}

	if (flags & PR_URLPARTS_PATH)
	{
		url_parts->path = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

		url_comp.lpszUrlPath = url_parts->path->buffer;
		url_comp.dwUrlPathLength = length;
	}

	if (flags & PR_URLPARTS_USER)
	{
		url_parts->user = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

		url_comp.lpszUserName = url_parts->user->buffer;
		url_comp.dwUserNameLength = length;
	}

	if (flags & PR_URLPARTS_PASS)
	{
		url_parts->pass = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

		url_comp.lpszPassword = url_parts->pass->buffer;
		url_comp.dwPasswordLength = length;
	}

	if (!WinHttpCrackUrl (url->buffer, (ULONG)_r_str_getlength2 (url), ICU_DECODE, &url_comp))
	{
		status = GetLastError ();

		_r_inet_destroyurlparts (url_parts);

		return status;
	}

	if (flags & PR_URLPARTS_SCHEME)
		url_parts->scheme = url_comp.nScheme;

	if (flags & PR_URLPARTS_PORT)
		url_parts->port = url_comp.nPort;

	if (flags & PR_URLPARTS_HOST)
		_r_obj_trimstringtonullterminator (url_parts->host);

	if (flags & PR_URLPARTS_PATH)
		_r_obj_trimstringtonullterminator (url_parts->path);

	if (flags & PR_URLPARTS_USER)
		_r_obj_trimstringtonullterminator (url_parts->user);

	if (flags & PR_URLPARTS_PASS)
		_r_obj_trimstringtonullterminator (url_parts->pass);

	return ERROR_SUCCESS;
}

VOID _r_inet_destroyurlparts (
	_Inout_ PR_URLPARTS url_parts
)
{
	SAFE_DELETE_REFERENCE (url_parts->host);
	SAFE_DELETE_REFERENCE (url_parts->path);
	SAFE_DELETE_REFERENCE (url_parts->user);
	SAFE_DELETE_REFERENCE (url_parts->pass);
}

//
// Registry
//

_Ret_maybenull_
PR_BYTE _r_reg_querybinary (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
)
{
	PR_BYTE buffer;
	ULONG type;
	ULONG size;
	LSTATUS status;

	status = _r_reg_queryvalue (
		hkey,
		subkey,
		value_name,
		&type,
		NULL,
		&size
	);

	if (status != ERROR_SUCCESS)
		return NULL;

	if (type != REG_BINARY)
		return NULL;

	buffer = _r_obj_createbyte_ex (NULL, size);

	status = RegQueryValueEx (
		hkey,
		value_name,
		NULL,
		NULL,
		(PBYTE)buffer->buffer,
		&size
	);

	if (status != ERROR_SUCCESS)
	{
		_r_obj_dereference (buffer); // cleanup

		return NULL;
	}

	return buffer;
}

_Ret_maybenull_
PR_STRING _r_reg_querystring (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name
)
{
	PR_STRING buffer;
	PR_STRING expanded_string;
	ULONG type;
	ULONG size;
	LSTATUS status;

	status = _r_reg_queryvalue (hkey, subkey, value_name, &type, NULL, &size);

	if (status != ERROR_SUCCESS)
		return NULL;

	if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ)
		return NULL;

	buffer = _r_obj_createstring_ex (NULL, size * sizeof (WCHAR));

	size = (ULONG)buffer->length;
	status = _r_reg_queryvalue (hkey, subkey, value_name, &type, (PBYTE)buffer->buffer, &size);

	if (status == ERROR_MORE_DATA)
	{
		_r_obj_dereference (buffer);
		buffer = _r_obj_createstring_ex (NULL, size * sizeof (WCHAR));

		size = (ULONG)buffer->length;
		status = _r_reg_queryvalue (hkey, subkey, value_name, &type, (PBYTE)buffer->buffer, &size);
	}

	if (status != ERROR_SUCCESS)
	{
		_r_obj_dereference (buffer); // cleanup

		return NULL;
	}

	_r_obj_trimstringtonullterminator (buffer);

	if (type == REG_EXPAND_SZ)
	{
		expanded_string = _r_str_environmentexpandstring (&buffer->sr);

		if (expanded_string)
			_r_obj_movereference (&buffer, expanded_string);
	}

	return buffer;
}

ULONG _r_reg_queryulong (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
)
{
	ULONG buffer;
	ULONG type;
	ULONG buffer_size;
	LSTATUS status;

	buffer_size = sizeof (ULONG);
	buffer = 0;

	status = _r_reg_queryvalue (hkey, subkey, value_name, &type, (PBYTE)&buffer, &buffer_size);

	if (status != ERROR_SUCCESS)
		return 0;

	if (type != REG_DWORD)
		return 0;

	return buffer;
}

ULONG64 _r_reg_queryulong64 (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
)
{
	ULONG64 buffer;
	ULONG type;
	ULONG buffer_size;
	LSTATUS status;

	buffer_size = sizeof (ULONG64);
	buffer = 0;

	status = _r_reg_queryvalue (hkey, subkey, value_name, &type, (PBYTE)&buffer, &buffer_size);

	if (status != ERROR_SUCCESS)
		return 0;

	if (type != REG_QWORD)
		return 0;

	return buffer;
}

ULONG _r_reg_querysubkeylength (
	_In_ HKEY hkey
)
{
	ULONG max_length;
	LSTATUS status;

	status = RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, &max_length, NULL, NULL, NULL, NULL, NULL, NULL);

	if (status != ERROR_SUCCESS)
		return 0;

	return max_length;
}

LONG64 _r_reg_querytimestamp (
	_In_ HKEY hkey
)
{
	FILETIME file_time;
	LSTATUS status;

	status = RegQueryInfoKey (hkey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &file_time);

	if (status != ERROR_SUCCESS)
		return 0;

	return _r_unixtime_from_filetime (&file_time);
}

LSTATUS _r_reg_queryvalue (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name,
	_Out_opt_ PULONG type,
	_Out_writes_bytes_to_opt_ (*buffer_length, *buffer_length) PBYTE buffer,
	_When_ (buffer == NULL, _Out_opt_) _When_ (buffer != NULL, _Inout_opt_) PULONG buffer_length
)
{
	HKEY hsubkey;
	LSTATUS status;

	if (subkey)
	{
		status = RegOpenKeyEx (hkey, subkey, 0, KEY_READ, &hsubkey);

		if (status != ERROR_SUCCESS)
			return status;

		status = RegQueryValueEx (hsubkey, value_name, NULL, type, buffer, buffer_length);

		RegCloseKey (hsubkey);
	}
	else
	{
		status = RegQueryValueEx (hkey, value_name, NULL, type, buffer, buffer_length);
	}

	return status;
}

//
// Cryptography
//

VOID _r_crypt_initialize (
	_Out_ PR_CRYPT_CONTEXT crypt_context,
	_In_ BOOLEAN is_hashing
)
{
	crypt_context->is_hashing = is_hashing;

	crypt_context->alg_handle = NULL;
	crypt_context->u.hash_handle = NULL;

	crypt_context->object_data = NULL;
	crypt_context->block_data = NULL;
}

VOID _r_crypt_destroycryptcontext (
	_In_ PR_CRYPT_CONTEXT crypt_context
)
{
	if (crypt_context->is_hashing)
	{
		if (crypt_context->u.hash_handle)
		{
			BCryptDestroyHash (crypt_context->u.hash_handle);
			crypt_context->u.hash_handle = NULL;
		}
	}
	else
	{
		if (crypt_context->u.key_handle)
		{
			BCryptDestroyKey (crypt_context->u.key_handle);
			crypt_context->u.key_handle = NULL;
		}
	}

	if (crypt_context->alg_handle)
	{
		BCryptCloseAlgorithmProvider (crypt_context->alg_handle, 0);
		crypt_context->alg_handle = NULL;
	}

	SAFE_DELETE_REFERENCE (crypt_context->object_data);
	SAFE_DELETE_REFERENCE (crypt_context->block_data);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_createcryptcontext (
	_Out_ PR_CRYPT_CONTEXT crypt_context,
	_In_ LPCWSTR algorithm_id
)
{
	ULONG query_size;
	ULONG data_length;
	NTSTATUS status;

	_r_crypt_initialize (crypt_context, FALSE);

	status = BCryptOpenAlgorithmProvider (
		&crypt_context->alg_handle,
		algorithm_id,
		NULL,
		0
	);

	if (status != STATUS_SUCCESS)
		goto CleanupExit;

	status = BCryptSetProperty (
		crypt_context->alg_handle,
		BCRYPT_CHAINING_MODE,
		(PBYTE)BCRYPT_CHAIN_MODE_CBC,
		sizeof (BCRYPT_CHAIN_MODE_CBC),
		0
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// Calculate the size of the buffer to hold the key object
	status = BCryptGetProperty (
		crypt_context->alg_handle,
		BCRYPT_OBJECT_LENGTH,
		(PBYTE)&data_length,
		sizeof (ULONG),
		&query_size,
		0
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	crypt_context->object_data = _r_obj_createbyte_ex (NULL, data_length);

	// Calculate the block length for the IV
	status = BCryptGetProperty (
		crypt_context->alg_handle,
		BCRYPT_BLOCK_LENGTH,
		(PBYTE)&data_length,
		sizeof (ULONG),
		&query_size,
		0
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	crypt_context->block_data = _r_obj_createbyte_ex (NULL, data_length);

CleanupExit:

	if (!NT_SUCCESS (status))
		_r_crypt_destroycryptcontext (crypt_context);

	return status;
}

PR_BYTE _r_crypt_getkeyblock (
	_Inout_ PR_CRYPT_CONTEXT crypt_context
)
{
	return crypt_context->object_data;
}

PR_BYTE _r_crypt_getivblock (
	_Inout_ PR_CRYPT_CONTEXT crypt_context
)
{
	return crypt_context->block_data;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_generatekey (
	_Inout_ PR_CRYPT_CONTEXT crypt_context,
	_In_ PR_BYTEREF key
)
{
	NTSTATUS status;

	if (crypt_context->u.key_handle)
	{
		BCryptDestroyKey (crypt_context->u.key_handle);
		crypt_context->u.key_handle = NULL;
	}

	RtlSecureZeroMemory (crypt_context->object_data->buffer, crypt_context->object_data->length);

	status = BCryptGenerateSymmetricKey (
		crypt_context->alg_handle,
		&crypt_context->u.key_handle,
		crypt_context->object_data->buffer,
		(ULONG)crypt_context->object_data->length,
		key->buffer,
		(ULONG)key->length,
		0
	);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_encryptbuffer (
	_In_ PR_CRYPT_CONTEXT crypt_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE tmp_buffer;
	ULONG return_length;
	ULONG written;
	NTSTATUS status;

	status = BCryptEncrypt (
		crypt_context->u.key_handle,
		buffer,
		buffer_length,
		NULL,
		crypt_context->block_data->buffer,
		(ULONG)crypt_context->block_data->length,
		NULL,
		0,
		&return_length,
		BCRYPT_BLOCK_PADDING
	);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	tmp_buffer = _r_obj_createbyte_ex (NULL, return_length);

	status = BCryptEncrypt (
		crypt_context->u.key_handle,
		buffer,
		buffer_length,
		NULL,
		crypt_context->block_data->buffer,
		(ULONG)crypt_context->block_data->length,
		tmp_buffer->buffer,
		return_length,
		&written,
		BCRYPT_BLOCK_PADDING
	);

	if (NT_SUCCESS (status))
	{
		tmp_buffer->length = written;

		*out_buffer = tmp_buffer;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (tmp_buffer);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_decryptbuffer (
	_In_ PR_CRYPT_CONTEXT crypt_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE tmp_buffer;
	ULONG return_length;
	ULONG written;
	NTSTATUS status;

	status = BCryptDecrypt (
		crypt_context->u.key_handle,
		buffer,
		buffer_length,
		NULL,
		crypt_context->block_data->buffer,
		(ULONG)crypt_context->block_data->length,
		NULL,
		0,
		&return_length,
		BCRYPT_BLOCK_PADDING
	);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	tmp_buffer = _r_obj_createbyte_ex (NULL, return_length);

	status = BCryptDecrypt (
		crypt_context->u.key_handle,
		buffer,
		buffer_length,
		NULL,
		crypt_context->block_data->buffer,
		(ULONG)crypt_context->block_data->length,
		tmp_buffer->buffer,
		return_length,
		&written,
		BCRYPT_BLOCK_PADDING
	);

	if (NT_SUCCESS (status))
	{
		tmp_buffer->length = written;

		*out_buffer = tmp_buffer;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (tmp_buffer);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_createhashcontext (
	_Out_ PR_CRYPT_CONTEXT hash_context,
	_In_ LPCWSTR algorithm_id
)
{
	ULONG data_length;
	ULONG query_size;
	NTSTATUS status;

	_r_crypt_initialize (hash_context, TRUE);

	status = BCryptOpenAlgorithmProvider (
		&hash_context->alg_handle,
		algorithm_id,
		NULL,
		0
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = BCryptGetProperty (
		hash_context->alg_handle,
		BCRYPT_OBJECT_LENGTH,
		(PBYTE)&data_length,
		sizeof (ULONG),
		&query_size,
		0
	);

	if (status != STATUS_SUCCESS)
		goto CleanupExit;

	hash_context->object_data = _r_obj_createbyte_ex (NULL, data_length);

	status = BCryptGetProperty (
		hash_context->alg_handle,
		BCRYPT_HASH_LENGTH,
		(PBYTE)&data_length,
		sizeof (ULONG),
		&query_size,
		0
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	hash_context->block_data = _r_obj_createbyte_ex (NULL, data_length);

	status = BCryptCreateHash (
		hash_context->alg_handle,
		&hash_context->u.hash_handle,
		hash_context->object_data->buffer,
		(ULONG)hash_context->object_data->length,
		NULL,
		0,
		0
	);

CleanupExit:

	if (!NT_SUCCESS (status))
		_r_crypt_destroycryptcontext (hash_context);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_hashbuffer (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length
)
{
	NTSTATUS status;

	status = BCryptHashData (hash_context->u.hash_handle, buffer, buffer_length, 0);

	return status;
}

_Ret_maybenull_
PR_STRING _r_crypt_finalhashcontext (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_In_ BOOLEAN is_uppercase
)
{
	PR_BYTE bytes;
	PR_STRING string;
	NTSTATUS status;

	status = _r_crypt_finalhashcontext_ex (hash_context, &bytes);

	if (!NT_SUCCESS (status))
		return NULL;

	string = _r_str_fromhex (bytes->buffer, bytes->length, is_uppercase);

	_r_obj_dereference (bytes);

	return string;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_finalhashcontext_ex (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	NTSTATUS status;

	status = BCryptFinishHash (
		hash_context->u.hash_handle,
		hash_context->block_data->buffer,
		(ULONG)hash_context->block_data->length,
		0
	);

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_reference (hash_context->block_data);
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

//
// Math
//

ULONG _r_math_exponentiate (
	_In_ ULONG base,
	_In_ ULONG exponent
)
{
	ULONG result;

	result = 1;

	while (exponent)
	{
		if (exponent & 1)
			result *= base;

		exponent >>= 1;
		base *= base;
	}

	return result;
}

ULONG64 _r_math_exponentiate64 (
	_In_ ULONG64 base,
	_In_ ULONG exponent
)
{
	ULONG64 result;

	result = 1;

	while (exponent)
	{
		if (exponent & 1)
			result *= base;

		exponent >>= 1;
		base *= base;
	}

	return result;
}

ULONG _r_math_getrandom ()
{
	static ULONG seed = 0; // save seed

	return RtlRandomEx (&seed);
}

ULONG _r_math_getrandomrange (
	_In_ ULONG min_number,
	_In_ ULONG max_number
)
{
	return min_number + (_r_math_getrandom () % (max_number - min_number + 1));
}

ULONG _r_math_hashinteger32 (
	_In_ ULONG value
)
{
	// Java style.
	value ^= (value >> 20) ^ (value >> 12);

	return value ^ (value >> 7) ^ (value >> 4);
}

ULONG _r_math_hashinteger64 (
	_In_ ULONG64 value
)
{
	// http://www.concentric.net/~Ttwang/tech/inthash.htm
	value = ~value + (value << 18);
	value ^= value >> 31;
	value *= 21;
	value ^= value >> 11;
	value += value << 6;
	value ^= value >> 22;

	return (ULONG)value;
}

SIZE_T _r_math_rounduptopoweroftwo (
	_In_ SIZE_T number
)
{
	number -= 1;

	number |= number >> 1;
	number |= number >> 2;
	number |= number >> 4;
	number |= number >> 8;
	number |= number >> 16;

	return number + 1;
}

//
// Resources
//

BOOLEAN _r_res_loadresource (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_ LPCWSTR type,
	_Out_ PR_BYTEREF out_buffer
)
{
	HRSRC hres;
	HGLOBAL hloaded;
	PVOID hlock;

	hres = FindResource (hinstance, name, type);

	if (hres)
	{
		hloaded = LoadResource (hinstance, hres);

		if (hloaded)
		{
			hlock = LockResource (hloaded);

			if (hlock)
			{
				_r_obj_initializebyteref_ex (
					out_buffer,
					hlock,
					SizeofResource (hinstance, hres)
				);

				return TRUE;
			}
		}
	}

	_r_obj_initializebyterefempty (out_buffer);

	return FALSE;
}

_Ret_maybenull_
PR_STRING _r_res_loadstring (
	_In_opt_ HINSTANCE hinstance,
	_In_ UINT string_id
)
{
	PR_STRING string;
	LPWSTR buffer;
	ULONG return_length;

	buffer = NULL;

	return_length = LoadString (hinstance, string_id, (LPWSTR)&buffer, 0);

	if (!return_length)
		return NULL;

	string = _r_obj_createstring_ex (buffer, return_length * sizeof (WCHAR));

	return string;
}

_Ret_maybenull_
PR_STRING _r_res_querystring (
	_In_ LPCVOID ver_block,
	_In_ LPCWSTR entry_name,
	_In_ ULONG lcid
)
{
	const ULONG lcid_arr[] = {
		lcid,
		(lcid & 0xFFFF0000) + 1252,
		PR_LANG_TO_LCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), 1200),
		PR_LANG_TO_LCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), 1252),
		PR_LANG_TO_LCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), 0),
	};

	PR_STRING string;

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (lcid_arr); i++)
	{
		string = _r_res_querystring_ex (ver_block, entry_name, lcid_arr[i]);

		if (string)
			return string;
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_res_querystring_ex (
	_In_ LPCVOID ver_block,
	_In_ LPCWSTR entry_name,
	_In_ ULONG lcid
)
{
	WCHAR entry[128];
	PVOID buffer;
	PR_STRING string;
	UINT length;

	_r_str_printf (
		entry,
		RTL_NUMBER_OF (entry),
		L"\\StringFileInfo\\%08X\\%s",
		lcid,
		entry_name
	);

	if (VerQueryValue (ver_block, entry, &buffer, &length) && buffer)
	{
		if (length <= sizeof (UNICODE_NULL))
			return NULL;

		string = _r_obj_createstring_ex (buffer, (length - 1) * sizeof (WCHAR));

		return string;
	}

	return NULL;
}

ULONG _r_res_querytranslation (
	_In_ LPCVOID ver_block
)
{
	PR_VERSION_TRANSLATION buffer;
	UINT length;

	buffer = NULL;

	if (VerQueryValue (ver_block, L"\\VarFileInfo\\Translation", &buffer, &length) && buffer)
		return PR_LANG_TO_LCID (buffer->lang_id, buffer->code_page);

	return PR_LANG_TO_LCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), 1252);
}

_Success_ (return)
BOOLEAN _r_res_queryversion (
	_In_ LPCVOID ver_block,
	_Out_ PVOID_PTR file_info
)
{
	UINT length;

	return !!VerQueryValue (ver_block, L"\\", file_info, &length);
}

_Ret_maybenull_
PR_STRING _r_res_queryversionstring (
	_In_ LPCWSTR path
)
{
	VS_FIXEDFILEINFO *file_info;
	PR_STRING string;
	PVOID ver_block;
	ULONG ver_size;
	BOOL result;

	ver_size = GetFileVersionInfoSize (path, NULL);

	if (!ver_size)
		return NULL;

	ver_block = _r_mem_allocatezero (ver_size);

	result = GetFileVersionInfo (path, 0, ver_size, ver_block);

	if (result)
	{
		result = _r_res_queryversion (ver_block, &file_info);

		if (result)
		{
			if (file_info->dwSignature == VS_FFI_SIGNATURE)
			{
				string = _r_format_string (
					L"%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG),
					HIWORD (file_info->dwFileVersionMS),
					LOWORD (file_info->dwFileVersionMS),
					HIWORD (file_info->dwFileVersionLS),
					LOWORD (file_info->dwFileVersionLS)
				);

				_r_mem_free (ver_block);

				return string;
			}
		}
	}

	_r_mem_free (ver_block);

	return NULL;
}

//
// Other
//

PR_HASHTABLE _r_parseini (
	_In_ PR_STRING path,
	_Inout_opt_ PR_LIST section_list
)
{
	static R_STRINGREF delimeter = PR_STRINGREF_INIT (L"\\");

	R_STRINGREF sections_iterator;
	R_STRINGREF values_iterator;
	R_STRINGREF key_string;
	R_STRINGREF value_string;
	PR_HASHTABLE hashtable;
	PR_STRING sections_string;
	PR_STRING values_string;
	PR_STRING hash_string;
	ULONG_PTR hash_code;
	ULONG allocated_length;
	ULONG return_length;

	hashtable = _r_obj_createhashtablepointer (16);

	// read section names
	allocated_length = 0x0800; // maximum length for GetPrivateProfileSectionNames
	sections_string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

	return_length = GetPrivateProfileSectionNames (sections_string->buffer, allocated_length, path->buffer);

	if (!return_length)
	{
		_r_obj_dereference (sections_string);

		return hashtable;
	}

	_r_obj_setstringlength (sections_string, return_length * sizeof (WCHAR));

	allocated_length = 0x7fff; // maximum length for GetPrivateProfileSection
	values_string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

	// initialize section iterator
	_r_obj_initializestringref (&sections_iterator, sections_string->buffer);

	while (!_r_str_isempty (sections_iterator.buffer))
	{
		return_length = GetPrivateProfileSection (sections_iterator.buffer, values_string->buffer, allocated_length, path->buffer);

		if (return_length)
		{
			_r_obj_setstringlength_ex (values_string, return_length * sizeof (WCHAR), allocated_length * sizeof (WCHAR));

			if (section_list)
				_r_obj_addlistitem (section_list, _r_obj_createstring3 (&sections_iterator));

			// initialize values iterator
			_r_obj_initializestringref (&values_iterator, values_string->buffer);

			while (!_r_str_isempty (values_iterator.buffer))
			{
				// skip comments
				if (*values_iterator.buffer != L'#')
				{
					if (_r_str_splitatchar (&values_iterator, '=', &key_string, &value_string))
					{
						// set hash code in table to "section\key" string
						hash_string = _r_obj_concatstringrefs (
							3,
							&sections_iterator,
							&delimeter,
							&key_string
						);

						hash_code = _r_str_gethash2 (hash_string, TRUE);

						if (hash_code)
						{
							if (!_r_obj_findhashtable (hashtable, hash_code))
							{
								_r_obj_addhashtablepointer (
									hashtable,
									hash_code,
									value_string.length ? _r_obj_createstring3 (&value_string) : NULL
								);
							}
						}

						_r_obj_dereference (hash_string);
					}
				}

				// go next value
				values_iterator.buffer = PTR_ADD_OFFSET (values_iterator.buffer, values_iterator.length + sizeof (UNICODE_NULL));
				values_iterator.length = _r_str_getlength (values_iterator.buffer) * sizeof (WCHAR);
			}
		}

		// go next section
		sections_iterator.buffer = PTR_ADD_OFFSET (sections_iterator.buffer, sections_iterator.length + sizeof (UNICODE_NULL));
		sections_iterator.length = _r_str_getlength (sections_iterator.buffer) * sizeof (WCHAR);
	}

	_r_obj_dereference (sections_string);
	_r_obj_dereference (values_string);

	return hashtable;
}

//
// Xml library
//

_Success_ (return == S_OK)
HRESULT _r_xml_initializelibrary (
	_Out_ PR_XML_LIBRARY xml_library,
	_In_ BOOLEAN is_reader
)
{
	HRESULT hr;

	xml_library->hstream = NULL;
	xml_library->is_reader = is_reader;

	if (is_reader)
	{
		hr = CreateXmlReader (&IID_IXmlReader, (PVOID_PTR)&xml_library->reader, NULL);

		if (hr != S_OK)
		{
			xml_library->reader = NULL;
			return hr;
		}

		IXmlReader_SetProperty (xml_library->reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
	}
	else
	{
		hr = CreateXmlWriter (&IID_IXmlWriter, (PVOID_PTR)&xml_library->writer, NULL);

		if (hr != S_OK)
		{
			xml_library->writer = NULL;
			return hr;
		}

		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_Indent, TRUE);
		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_CompactEmptyElement, TRUE);
	}

	return S_OK;
}

VOID _r_xml_destroylibrary (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
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

	SAFE_DELETE_STREAM (xml_library->hstream);
}

_Success_ (return == S_OK)
HRESULT _r_xml_createfilestream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path,
	_In_ ULONG mode,
	_In_ BOOL is_create
)
{
	PR_XML_STREAM hstream;
	PR_XML_STREAM hstream_prev;
	HRESULT hr;

	hr = SHCreateStreamOnFileEx (file_path, mode, FILE_ATTRIBUTE_NORMAL, is_create, NULL, &hstream);

	if (hr != S_OK)
		return hr;

	hstream_prev = xml_library->hstream;

	hr = _r_xml_setlibrarystream (xml_library, hstream);

	if (hstream_prev)
		IStream_Release (hstream_prev);

	return hr;
}

_Success_ (return == S_OK)
HRESULT _r_xml_createstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_opt_ LPCVOID buffer,
	_In_ ULONG buffer_length
)
{
	PR_XML_STREAM hstream;
	PR_XML_STREAM hstream_prev;
	HRESULT hr;

	hstream = SHCreateMemStream (buffer, buffer_length);

	if (!hstream)
		return S_FALSE;

	hstream_prev = xml_library->hstream;

	hr = _r_xml_setlibrarystream (xml_library, hstream);

	if (hstream_prev)
		IStream_Release (hstream_prev);

	return hr;
}

_Success_ (return == S_OK)
HRESULT _r_xml_parsefile (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path
)
{
	HRESULT hr;

	hr = _r_xml_createfilestream (xml_library, file_path, STGM_READ, FALSE);

	return hr;
}

_Success_ (return == S_OK)
HRESULT _r_xml_parsestring (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCVOID buffer,
	_In_ ULONG buffer_length
)
{
	HRESULT hr;

	hr = _r_xml_createstream (xml_library, buffer, buffer_length);

	return hr;
}

_Success_ (return == S_OK)
HRESULT _r_xml_readstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	ULARGE_INTEGER size;
	PR_BYTE bytes;
	ULONG readed;
	HRESULT hr;

	hr = IStream_Size (xml_library->hstream, &size);

	if (hr != S_OK)
	{
		*out_buffer = NULL;

		return hr;
	}

	// reset stream position to the beginning
	IStream_Reset (xml_library->hstream);

	bytes = _r_obj_createbyte_ex (NULL, size.LowPart);

	hr = ISequentialStream_Read (xml_library->hstream, bytes->buffer, (ULONG)bytes->length, &readed);

	if (hr == S_OK)
	{
		_r_obj_setbytelength (bytes, readed);

		*out_buffer = bytes;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (bytes);
	}

	return hr;
}

_Success_ (return)
BOOLEAN _r_xml_getattribute (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_Out_ PR_STRINGREF value
)
{
	LPCWSTR value_string;
	UINT value_length;
	HRESULT hr;

	hr = IXmlReader_MoveToAttributeByName (xml_library->reader, attrib_name, NULL);

	if (hr != S_OK)
		return FALSE;

	hr = IXmlReader_GetValue (xml_library->reader, &value_string, &value_length);

	// restore position before return from the function!
	IXmlReader_MoveToElement (xml_library->reader);

	if (hr != S_OK || _r_str_isempty (value_string) || !value_length)
		return FALSE;

	_r_obj_initializestringref_ex (
		value,
		(LPWSTR)value_string,
		value_length * sizeof (WCHAR)
	);

	return TRUE;
}

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value))
		return NULL;

	return _r_obj_createstring3 (&text_value);
}

BOOLEAN _r_xml_getattribute_boolean (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value))
		return FALSE;

	return _r_str_toboolean (&text_value);
}

LONG _r_xml_getattribute_long (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value))
		return 0;

	return _r_str_tolong (&text_value);
}

LONG64 _r_xml_getattribute_long64 (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (!_r_xml_getattribute (xml_library, attrib_name, &text_value))
		return 0;

	return _r_str_tolong64 (&text_value);
}

VOID _r_xml_setattribute_long (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_ LONG value
)
{
	WCHAR value_text[64];
	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), value);

	_r_xml_setattribute (xml_library, attrib_name, value_text);
}

VOID _r_xml_setattribute_long64 (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_ LONG64 value
)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_xml_setattribute (xml_library, attrib_name, value_text);
}

_Success_ (return)
BOOLEAN _r_xml_enumchilditemsbytagname (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR tag_name
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	LPWSTR buffer;
	UINT buffer_length;
	XmlNodeType node_type;
	HRESULT hr;

	_r_obj_initializestringrefconst (&sr2, tag_name);

	while (TRUE)
	{
		if (IXmlReader_IsEOF (xml_library->reader))
			return FALSE;

		hr = IXmlReader_Read (xml_library->reader, &node_type);

		if (hr != S_OK)
			return FALSE;

		if (node_type == XmlNodeType_Element)
		{
			buffer = NULL;

			hr = IXmlReader_GetLocalName (xml_library->reader, &buffer, &buffer_length);

			if (hr != S_OK)
				return FALSE;

			if (_r_str_isempty (buffer))
				return FALSE;

			_r_obj_initializestringref_ex (
				&sr1,
				buffer,
				buffer_length * sizeof (WCHAR)
			);

			if (_r_str_isequal (&sr1, &sr2, TRUE))
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
BOOLEAN _r_xml_findchildbytagname (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR tag_name
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	LPWSTR buffer;
	UINT buffer_length;
	XmlNodeType node_type;
	HRESULT hr;

	_r_xml_resetlibrarystream (xml_library);

	_r_obj_initializestringrefconst (&sr2, tag_name);

	while (TRUE)
	{
		if (IXmlReader_IsEOF (xml_library->reader))
			return FALSE;

		hr = IXmlReader_Read (xml_library->reader, &node_type);

		if (hr != S_OK)
			return FALSE;

		if (node_type == XmlNodeType_Element)
		{
			// do not return empty elements
			if (IXmlReader_IsEmptyElement (xml_library->reader))
				continue;

			buffer = NULL;

			hr = IXmlReader_GetLocalName (xml_library->reader, &buffer, &buffer_length);

			if (hr != S_OK)
				break;

			if (_r_str_isempty (buffer))
				break;

			_r_obj_initializestringref_ex (
				&sr1,
				buffer,
				buffer_length * sizeof (WCHAR)
			);

			if (_r_str_isequal (&sr1, &sr2, TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

_Success_ (return == S_OK)
HRESULT _r_xml_resetlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	HRESULT hr;

	if (!xml_library->hstream)
		return S_FALSE;

	// reset stream position to the beginning
	IStream_Reset (xml_library->hstream);

	hr = _r_xml_setlibrarystream (xml_library, xml_library->hstream);

	return hr;
}

_Success_ (return == S_OK)
HRESULT _r_xml_setlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ PR_XML_STREAM hstream
)
{
	HRESULT hr;

	xml_library->hstream = hstream;

	if (xml_library->is_reader)
	{
		hr = IXmlReader_SetInput (xml_library->reader, (IUnknown *)hstream);
	}
	else
	{
		hr = IXmlWriter_SetOutput (xml_library->writer, (IUnknown *)hstream);
	}

	return hr;
}

//
// System tray
//

VOID _r_tray_initialize (
	_Inout_ PNOTIFYICONDATA nid,
	_In_ HWND hwnd,
	_In_ LPCGUID guid
)
{
	ULONG hash_code;

	hash_code = _r_str_gethash (_r_sys_getimagepath (), TRUE);

#if defined(APP_NO_DEPRECATIONS)

	nid->cbSize = sizeof (NOTIFYICONDATA);

	nid->uFlags |= NIF_GUID;
	nid->hWnd = hwnd;
	nid->uID = guid->Data2;

	RtlCopyMemory (&nid->guidItem, guid, sizeof (GUID));

	// The path of the binary file is included in the registration of the icon's GUID and cannot be changed.
	// Settings associated with the icon are preserved through an upgrade only if the file path and GUID are
	// unchanged. If the path must be changed, the application should remove any GUID information that was added
	// when the existing icon was registered. Once that information is removed, you can move the binary file to
	// a new location and reregister it with a new GUID.

	nid->guidItem.Data1 ^= hash_code; // HACK!!!

#else

	BOOLEAN is_vistaorlater;

	is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid->cbSize = (is_vistaorlater ? sizeof (NOTIFYICONDATA) : NOTIFYICONDATA_V3_SIZE);
	nid->hWnd = hwnd;
	nid->uID = guid->Data2;

	if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
	{
		nid->uFlags |= NIF_GUID;

		RtlCopyMemory (&nid->guidItem, guid, sizeof (GUID));

		// The path of the binary file is included in the registration of the icon's GUID and cannot be changed.
		// Settings associated with the icon are preserved through an upgrade only if the file path and GUID are
		// unchanged. If the path must be changed, the application should remove any GUID information that was added
		// when the existing icon was registered. Once that information is removed, you can move the binary file to
		// a new location and reregister it with a new GUID.

		nid->guidItem.Data1 ^= hash_code; // HACK!!!
	}

#endif // APP_NO_DEPRECATIONS
}

VOID _r_tray_setversion (
	_Inout_ PNOTIFYICONDATA nid
)
{
#if defined(APP_NO_DEPRECATIONS)

	nid->uVersion = NOTIFYICON_VERSION_4;

#else

	BOOLEAN is_vistaorlater;

	is_vistaorlater = _r_sys_isosversiongreaterorequal (WINDOWS_VISTA);

	nid->uVersion = (is_vistaorlater ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);

#endif // APP_NO_DEPRECATIONS

	Shell_NotifyIcon (NIM_SETVERSION, nid);
}

BOOLEAN _r_tray_create (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ UINT code,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip,
	_In_ BOOLEAN is_hidden
)
{
	NOTIFYICONDATA nid = {0};

	_r_tray_initialize (&nid, hwnd, guid);

	Shell_NotifyIcon (NIM_DELETE, &nid); // HACK!!!

	nid.uFlags |= NIF_MESSAGE;
	nid.uCallbackMessage = code;

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

		if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
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
		_r_tray_setversion (&nid);

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_tray_destroy (
	_In_ HWND hwnd,
	_In_ LPCGUID guid
)
{
	NOTIFYICONDATA nid = {0};

	_r_tray_initialize (&nid, hwnd, guid);

	return !!Shell_NotifyIcon (NIM_DELETE, &nid);
}

BOOLEAN _r_tray_popup (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG icon_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR text
)
{
	NOTIFYICONDATA nid = {0};

	_r_tray_initialize (&nid, hwnd, guid);

#if defined(APP_NO_DEPRECATIONS)
	nid.uFlags |= NIF_REALTIME;
#else
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		nid.uFlags |= NIF_REALTIME;
#endif // APP_NO_DEPRECATIONS

	if (icon_id)
	{
		nid.uFlags |= NIF_INFO;
		nid.dwInfoFlags = icon_id;
	}

	if (title)
		_r_str_copy (nid.szInfoTitle, RTL_NUMBER_OF (nid.szInfoTitle), title);

	if (text)
		_r_str_copy (nid.szInfo, RTL_NUMBER_OF (nid.szInfo), text);

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_popupformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG icon_id,
	_In_opt_ LPCWSTR title,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;
	BOOLEAN status;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	status = _r_tray_popup (hwnd, guid, icon_id, title, string->buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_setinfo (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip
)
{
	NOTIFYICONDATA nid = {0};

	_r_tray_initialize (&nid, hwnd, guid);

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

		if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
			nid.uFlags |= NIF_SHOWTIP;
#endif // APP_NO_DEPRECATIONS

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOLEAN _r_tray_setinfoformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;
	BOOLEAN status;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	status = _r_tray_setinfo (hwnd, guid, hicon, string->buffer);

	_r_obj_dereference (string);

	return status;
}

BOOLEAN _r_tray_toggle (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ BOOLEAN is_show
)
{
	NOTIFYICONDATA nid = {0};

	_r_tray_initialize (&nid, hwnd, guid);

	nid.uFlags |= NIF_STATE;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	return !!Shell_NotifyIcon (NIM_MODIFY, &nid);
}

//
// Control: common
//

BOOLEAN _r_ctrl_isenabled (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	HWND hctrl;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (hctrl)
		return !!IsWindowEnabled (hctrl);

	return FALSE;
}

INT _r_ctrl_isradiobuttonchecked (
	_In_ HWND hwnd,
	_In_ INT start_id,
	_In_ INT end_id
)
{
	for (INT i = start_id; i <= end_id; i++)
	{
		if (IsDlgButtonChecked (hwnd, i) == BST_CHECKED)
			return i;
	}

	return 0;
}

VOID _r_ctrl_enable (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ BOOLEAN is_enable
)
{
	HWND htarget;

	if (ctrl_id)
	{
		htarget = GetDlgItem (hwnd, ctrl_id);

		if (!htarget)
			return;
	}
	else
	{
		htarget = hwnd;
	}

	EnableWindow (htarget, is_enable);
}

LONG64 _r_ctrl_getinteger (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_Out_opt_ PULONG base_ptr
)
{
	PR_STRING string;
	LONG64 value;

	string = _r_ctrl_getstring (hwnd, ctrl_id);

	if (!string)
	{
		if (base_ptr)
			*base_ptr = 0;

		return 0;
	}

	_r_str_tointeger64 (&string->sr, 0, base_ptr, &value);

	_r_obj_dereference (string);

	return value;
}

_Ret_maybenull_
PR_STRING _r_ctrl_getstring (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PR_STRING string;
	ULONG length;

	length = _r_ctrl_getstringlength (hwnd, ctrl_id);

	if (!length)
		return NULL;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	if ((ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXT, (WPARAM)length + 1, (LPARAM)string->buffer))
	{
		_r_obj_trimstringtonullterminator (string);

		return string;

	}

	_r_obj_dereference (string);

	return NULL;
}

LONG _r_ctrl_getwidth (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	RECT rect;
	HWND htarget;

	if (ctrl_id)
	{
		htarget = GetDlgItem (hwnd, ctrl_id);

		if (!htarget)
			return 0;
	}
	else
	{
		htarget = hwnd;
	}

	if (GetClientRect (htarget, &rect))
		return rect.right;

	return 0;
}

VOID _r_ctrl_setbuttonmargins (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ LONG dpi_value
)
{
	BUTTON_SPLITINFO bsi = {0};
	RECT padding_rect;
	HWND hctrl;
	LONG padding;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return;

	// set button text margin
	padding = _r_dc_getdpi (4, dpi_value);

	SetRect (&padding_rect, padding, 0, padding, 0);

	SendMessage (hctrl, BCM_SETTEXTMARGIN, 0, (LPARAM)&padding_rect);

	// set button split margin
	if (_r_wnd_getstyle (hctrl) & BS_SPLITBUTTON)
	{
		bsi.mask = BCSIF_SIZE;

		bsi.size.cx = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value) + (padding / 2);
		//bsi.size.cy = 0;

		SendMessage (hctrl, BCM_SETSPLITINFO, 0, (LPARAM)&bsi);
	}
}

VOID _r_ctrl_settablestring (
	_In_ HWND hwnd,
	_In_ INT ctrl_id1,
	_In_ PR_STRINGREF text1,
	_In_ INT ctrl_id2,
	_In_ PR_STRINGREF text2
)
{
	RECT control_rect;
	HDWP hdefer;
	HWND hctrl1;
	HWND hctrl2;
	HDC hdc;
	LONG wnd_width;
	LONG wnd_spacing;
	LONG ctrl1_width;
	LONG ctrl2_width;

	hdc = GetDC (hwnd);

	if (!hdc)
		return;

	hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	if (!hctrl1 || !hctrl2)
		goto CleanupExit;

	wnd_width = _r_ctrl_getwidth (hwnd, 0);

	if (!wnd_width)
		goto CleanupExit;

	if (!GetWindowRect (hctrl1, &control_rect))
		goto CleanupExit;

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&control_rect, 2);

	wnd_spacing = control_rect.left;
	wnd_width -= (wnd_spacing * 2);

	_r_dc_fixwindowfont (hdc, hctrl1); // fix

	ctrl1_width = _r_dc_getfontwidth (hdc, text1) + wnd_spacing;
	ctrl2_width = _r_dc_getfontwidth (hdc, text2) + wnd_spacing;

	ctrl2_width = min (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);
	ctrl1_width = min (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing);

	_r_ctrl_setstringlength (hwnd, ctrl_id1, text1);
	_r_ctrl_setstringlength (hwnd, ctrl_id2, text2);

	hdefer = BeginDeferWindowPos (2);

	if (hdefer)
	{
		// resize control #1
		hdefer = DeferWindowPos (
			hdefer,
			hctrl1,
			NULL,
			control_rect.left,
			control_rect.top,
			ctrl1_width,
			_r_calc_rectheight (&control_rect),
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		// resize control #2
		hdefer = DeferWindowPos (
			hdefer,
			hctrl2,
			NULL,
			wnd_width - ctrl2_width,
			control_rect.top,
			ctrl2_width + wnd_spacing,
			_r_calc_rectheight (&control_rect),
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER
		);

		EndDeferWindowPos (hdefer);
	}

CleanupExit:

	ReleaseDC (hwnd, hdc);
}

VOID _r_ctrl_setstringformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_ctrl_setstring (hwnd, ctrl_id, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_setstringlength (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ PR_STRINGREF string
)
{
	PR_STRING tmp_string;

	// Note: PR_STRINGREF can be not null terminated.
	if (!_r_obj_isstringnullterminated (string))
	{
		tmp_string = _r_obj_createstring3 (string);

		_r_ctrl_setstring (hwnd, ctrl_id, tmp_string->buffer);

		_r_obj_dereference (tmp_string);
	}
	else
	{
		_r_ctrl_setstring (hwnd, ctrl_id, string->buffer);
	}
}

_Ret_maybenull_
HWND _r_ctrl_createtip (
	_In_opt_ HWND hparent
)
{
	HWND htip;

	htip = CreateWindowEx (
		WS_EX_TOPMOST,
		TOOLTIPS_CLASS,
		NULL,
		WS_CHILD | WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		hparent,
		NULL,
		_r_sys_getimagebase (),
		NULL
	);

	if (htip)
	{
		_r_ctrl_settipstyle (htip);

		return htip;
	}

	return NULL;
}

VOID _r_ctrl_settiptext (
	_In_ HWND htip,
	_In_ HWND hparent,
	_In_ INT ctrl_id,
	_In_ LPCWSTR text
)
{
	TOOLINFO tool_info = {0};

	tool_info.cbSize = sizeof (tool_info);
	tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	tool_info.hwnd = hparent;
	tool_info.hinst = _r_sys_getimagebase ();
	tool_info.uId = (UINT_PTR)GetDlgItem (hparent, ctrl_id);
	tool_info.lpszText = (LPWSTR)text;

	GetClientRect (hparent, &tool_info.rect);

	SendMessage (htip, TTM_ADDTOOL, 0, (LPARAM)&tool_info);
}

VOID _r_ctrl_settiptextformat (
	_In_ HWND htip,
	_In_ HWND hparent,
	_In_ INT ctrl_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_ctrl_settiptext (htip, hparent, ctrl_id, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_settipstyle (
	_In_ HWND htip
)
{
	SendMessage (htip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);
	SendMessage (htip, TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

	_r_wnd_top (htip, TRUE); // HACK!!!
}

VOID _r_ctrl_showballoontip (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT icon_id,
	_In_opt_ LPCWSTR title,
	_In_ LPCWSTR text
)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.pszTitle = title;
	ebt.pszText = text;
	ebt.ttiIcon = icon_id;

	SendDlgItemMessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

VOID _r_ctrl_showballoontipformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT icon_id,
	_In_opt_ LPCWSTR title,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_ctrl_showballoontip (hwnd, ctrl_id, icon_id, title, string->buffer);

	_r_obj_dereference (string);
}

//
// Control: menu
//

VOID _r_menu_checkitem (
	_In_ HMENU hmenu,
	_In_ UINT item_id_start,
	_In_opt_ UINT item_id_end,
	_In_ UINT position_flag,
	_In_ UINT check_id
)
{
	if (item_id_end)
	{
		CheckMenuRadioItem (
			hmenu,
			min (item_id_start, item_id_end),
			max (item_id_start, item_id_end),
			check_id,
			position_flag
		);
	}
	else
	{
		CheckMenuItem (
			hmenu,
			item_id_start,
			position_flag | (check_id ? MF_CHECKED : MF_UNCHECKED)
		);
	}
}

VOID _r_menu_clearitems (
	_In_ HMENU hmenu
)
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

VOID _r_menu_setitembitmap (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ HBITMAP hbitmap
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbitmap;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtext (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ LPCWSTR text
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = (LPWSTR)text;

	SetMenuItemInfo (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtextformat (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_menu_setitemtext (hmenu, item_id, is_byposition, string->buffer);

	_r_obj_dereference (string);
}

INT _r_menu_popup (
	_In_ HMENU hmenu,
	_In_ HWND hwnd,
	_In_opt_ PPOINT point,
	_In_ BOOLEAN is_sendmessage
)
{
	POINT pt;
	INT command_id;

	if (!point)
	{
		GetCursorPos (&pt);
		point = &pt;
	}

	command_id = TrackPopupMenuEx (
		hmenu,
		TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
		point->x,
		point->y,
		hwnd,
		NULL
	);

	if (is_sendmessage && command_id && hwnd)
		PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

	return command_id;
}

//
// Control: tab
//

VOID _r_tab_adjustchild (
	_In_ HWND hwnd,
	_In_ INT tab_id,
	_In_ HWND hchild
)
{
	RECT tab_rect;
	RECT new_rect;
	HWND htab;

	htab = GetDlgItem (hwnd, tab_id);

	if (!htab || !GetClientRect (htab, &new_rect) || !GetWindowRect (htab, &tab_rect))
		return;

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&tab_rect, 2);

	OffsetRect (&new_rect, tab_rect.left, tab_rect.top);

	SendDlgItemMessage (hwnd, tab_id, TCM_ADJUSTRECT, FALSE, (LPARAM)&new_rect);

	SetWindowPos (
		hchild,
		NULL,
		new_rect.left,
		new_rect.top,
		_r_calc_rectwidth (&new_rect),
		_r_calc_rectheight (&new_rect),
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER
	);
}

INT _r_tab_additem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = (LPWSTR)text;
	}

	if (image_id != I_IMAGENONE)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image_id;
	}

	if (lparam)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_INSERTITEM, (WPARAM)item_id, (LPARAM)&tci);
}

LPARAM _r_tab_getitemlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	if ((INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEM, (WPARAM)item_id, (LPARAM)&tci))
		return tci.lParam;

	return 0;
}

INT _r_tab_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TCITEM tci = {0};

	if (text)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = (LPWSTR)text;
	}

	if (image_id != I_IMAGENONE)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image_id;
	}

	if (lparam)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_SETITEM, (WPARAM)item_id, (LPARAM)&tci);
}

VOID _r_tab_selectitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	NMHDR hdr = {0};

	hdr.hwndFrom = GetDlgItem (hwnd, ctrl_id);
	hdr.idFrom = (UINT_PTR)(UINT)ctrl_id;

#pragma warning(push)
#pragma warning(disable: 26454)
	hdr.code = TCN_SELCHANGING;
	SendMessage (hwnd, WM_NOTIFY, (WPARAM)ctrl_id, (LPARAM)&hdr);

	SendDlgItemMessage (hwnd, ctrl_id, TCM_SETCURSEL, (WPARAM)item_id, 0);

	hdr.code = TCN_SELCHANGE;
	SendMessage (hwnd, WM_NOTIFY, (WPARAM)ctrl_id, (LPARAM)&hdr);
#pragma warning(pop)
}

//
// Control: listview
//

INT _r_listview_addcolumn (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ INT width,
	_In_opt_ INT fmt
)
{
	LVCOLUMN lvc = {0};
	LONG client_width;

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
			client_width = _r_ctrl_getwidth (hwnd, ctrl_id);

			if (client_width)
			{
				width = (INT)_r_calc_percentval (-width, client_width);
			}
			else
			{
				width = -width;
			}
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

INT _r_listview_addgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ UINT align,
	_In_opt_ UINT state,
	_In_opt_ UINT state_mask
)
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

INT _r_listview_additem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ LPCWSTR text
)
{
	return _r_listview_additem_ex (hwnd, ctrl_id, item_id, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

INT _r_listview_additem_ex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_opt_ LPARAM lparam
)
{
	LVITEM lvi = {0};

	lvi.iItem = item_id;
	lvi.iSubItem = 0;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = (LPWSTR)text;
	}

	if (image_id != I_IMAGENONE)
	{
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = image_id;
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

	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

_Success_ (return != -1)
INT _r_listview_finditem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT start_pos,
	_In_ LPARAM lparam
)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	return (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)start_pos, (LPARAM)&lvfi);
}

VOID _r_listview_deleteallcolumns (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	INT column_count;

	column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	for (INT i = column_count; i >= 0; i--)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
}

INT _r_listview_getcolumncount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	HWND header;

	header = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (!header)
		return 0;

	return (INT)SendMessage (header, HDM_GETITEMCOUNT, 0, 0);
}

_Ret_maybenull_
PR_STRING _r_listview_getcolumntext (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id
)
{
	LVCOLUMN lvc = {0};
	PR_STRING string;
	ULONG allocated_count;

	allocated_count = 256;
	string = _r_obj_createstring_ex (NULL, allocated_count * sizeof (WCHAR));

	lvc.mask = LVCF_TEXT;
	lvc.pszText = string->buffer;
	lvc.cchTextMax = (INT)allocated_count + 1;

	if (!(INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc))
	{
		_r_obj_dereference (string);

		return NULL;
	}

	_r_obj_trimstringtonullterminator (string);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_dereference (string);

	return NULL;
}

INT _r_listview_getcolumnwidth (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id
)
{
	LONG total_width;
	LONG column_width;

	total_width = _r_ctrl_getwidth (hwnd, ctrl_id);

	if (!total_width)
		return 0;

	column_width = (LONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0);

	return (INT)_r_calc_percentof (column_width, total_width);
}

INT _r_listview_getitemcheckedcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	INT total_count;
	INT checks_count;

	checks_count = 0;
	total_count = _r_listview_getitemcount (hwnd, ctrl_id);

	for (INT i = 0; i < total_count; i++)
	{
		if (_r_listview_isitemchecked (hwnd, ctrl_id, i))
			checks_count += 1;
	}

	return checks_count;
}

INT _r_listview_getitemgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_GROUPID;
	lvi.iItem = item_id;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.iGroupId;
}

LPARAM _r_listview_getitemlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	LVITEM lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item_id;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

_Ret_maybenull_
PR_STRING _r_listview_getitemtext (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id
)
{
	LVITEM lvi = {0};
	PR_STRING string = NULL;
	ULONG allocated_count;
	ULONG count;

	allocated_count = 256;
	count = allocated_count;

	while (count >= allocated_count)
	{
		allocated_count *= 2;

		_r_obj_movereference (&string, _r_obj_createstring_ex (NULL, allocated_count * sizeof (WCHAR)));

		lvi.iSubItem = subitem_id;
		lvi.pszText = string->buffer;
		lvi.cchTextMax = (INT)allocated_count + 1;

		count = (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMTEXT, (WPARAM)item_id, (LPARAM)&lvi);
	}

	_r_obj_trimstringtonullterminator (string);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_dereference (string);

	return NULL;
}

VOID _r_listview_redraw (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	if (item_id != -1)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, (WPARAM)item_id, (LPARAM)item_id);
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_REDRAWITEMS, 0, (LPARAM)INT_MAX);
	}
}

VOID _r_listview_setcolumn (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id,
	_In_opt_ LPCWSTR text,
	_In_opt_ INT width
)
{
	LVCOLUMN lvc = {0};
	LONG client_width;

	if (text)
	{
		lvc.mask |= LVCF_TEXT;
		lvc.pszText = (LPWSTR)text;
	}

	if (width)
	{
		if (width < LVSCW_AUTOSIZE_USEHEADER)
		{
			client_width = _r_ctrl_getwidth (hwnd, ctrl_id);

			if (client_width)
			{
				width = (INT)_r_calc_percentval (-width, client_width);
			}
			else
			{
				width = -width;
			}
		}

		lvc.mask |= LVCF_WIDTH;
		lvc.cx = width;
	}

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

VOID _r_listview_setcolumnsortindex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id,
	_In_ INT arrow
)
{
	HDITEM hitem = {0};
	HWND header;

	header = (HWND)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETHEADER, 0, 0);

	if (!header)
		return;

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

VOID _r_listview_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id,
	_In_opt_ LPCWSTR text
)
{
	_r_listview_setitem_ex (hwnd, ctrl_id, item_id, subitem_id, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

VOID _r_listview_setitem_ex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_opt_ LPARAM lparam
)
{
	LVITEM lvi = {0};

	lvi.iItem = item_id;
	lvi.iSubItem = subitem_id;

	if (text)
	{
		lvi.mask |= LVIF_TEXT;
		lvi.pszText = (LPWSTR)text;
	}

	if (!subitem_id)
	{
		if (image_id != I_IMAGENONE)
		{
			lvi.mask |= LVIF_IMAGE;
			lvi.iImage = image_id;
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

VOID _r_listview_setitemcheck (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ BOOLEAN is_check
)
{
	_r_listview_setitemstate (hwnd, ctrl_id, item_id, INDEXTOSTATEIMAGEMASK (is_check ? 2 : 1), LVIS_STATEIMAGEMASK);
}

VOID _r_listview_setitemstate (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ UINT state,
	_In_opt_ UINT state_mask
)
{
	LVITEM lvi = {0};

	lvi.state = state;
	lvi.stateMask = state_mask;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item_id, (LPARAM)&lvi);
}

VOID _r_listview_setitemvisible (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	_r_listview_setitemstate (hwnd, ctrl_id, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	_r_listview_setitemstate (hwnd, ctrl_id, item_id, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	_r_listview_ensurevisible (hwnd, ctrl_id, item_id);
}

VOID _r_listview_setgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ UINT state,
	_In_opt_ UINT state_mask
)
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

VOID _r_listview_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_ BOOL is_groupview
)
{
	HWND hctrl;
	HWND htip;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return;

	SetWindowTheme (hctrl, L"Explorer", NULL);

	htip = (HWND)SendMessage (hctrl, LVM_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	if (ex_style)
		SendMessage (hctrl, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)ex_style);

	SendMessage (hctrl, LVM_ENABLEGROUPVIEW, (WPARAM)is_groupview, 0);
}

//
// Control: treeview
//

HTREEITEM _r_treeview_additem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ LPCWSTR text,
	_In_opt_ HTREEITEM hparent,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
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

	if (image_id != I_IMAGENONE)
	{
		tvi.itemex.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.itemex.iImage = image_id;
		tvi.itemex.iSelectedImage = image_id;
	}

	if (lparam)
	{
		tvi.itemex.mask |= TVIF_PARAM;
		tvi.itemex.lParam = lparam;
	}

	return (HTREEITEM)SendDlgItemMessage (hwnd, ctrl_id, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HTREEITEM hitem
)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = hitem;

	SendDlgItemMessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

VOID _r_treeview_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HTREEITEM hitem,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TVITEMEX tvi = {0};

	tvi.hItem = hitem;

	if (text)
	{
		tvi.mask |= TVIF_TEXT;
		tvi.pszText = (LPWSTR)text;
	}

	if (image_id != I_IMAGENONE)
	{
		tvi.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.iImage = image_id;
		tvi.iSelectedImage = image_id;
	}

	if (lparam)
	{
		tvi.mask |= TVIF_PARAM;
		tvi.lParam = lparam;
	}

	SendDlgItemMessage (hwnd, ctrl_id, TVM_SETITEM, 0, (LPARAM)&tvi);
}

VOID _r_treeview_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_opt_ INT height,
	_In_opt_ INT indent
)
{
	HWND hctrl;
	HWND htip;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return;

	SetWindowTheme (hctrl, L"Explorer", NULL);

	htip = (HWND)SendMessage (hctrl, TVM_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	if (ex_style)
		SendMessage (hctrl, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)ex_style);

	if (indent)
		SendMessage (hctrl, TVM_SETINDENT, (WPARAM)indent, 0);

	if (height)
		SendMessage (hctrl, TVM_SETITEMHEIGHT, (WPARAM)height, 0);
}

//
// Control: statusbar
//

LONG _r_status_getheight (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	RECT rect;
	HWND hctrl;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return 0;

	if (GetClientRect (hctrl, &rect))
		return rect.bottom;

	return 0;
}

VOID _r_status_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ INT height
)
{
	if (height)
		SendDlgItemMessage (hwnd, ctrl_id, SB_SETMINHEIGHT, (WPARAM)height, 0);

	SendDlgItemMessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

VOID _r_status_settextformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT part_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_status_settext (hwnd, ctrl_id, part_id, string->buffer);

	_r_obj_dereference (string);
}

//
// Control: rebar
//

VOID _r_rebar_insertband (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT band_id,
	_In_ HWND hchild,
	_In_opt_ UINT style,
	_In_ UINT width,
	_In_ UINT height
)
{
	REBARBANDINFO rbi = {0};

	rbi.cbSize = sizeof (rbi);
	rbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_ID;
	rbi.wID = band_id;
	rbi.hwndChild = hchild;
	rbi.cxMinChild = width;
	rbi.cyMinChild = height;

	if (style)
	{
		rbi.fMask |= RBBIM_STYLE;
		rbi.fStyle = style;
	}

	SendDlgItemMessage (hwnd, ctrl_id, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

VOID _r_rebar_deleteband (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT band_id
)
{
	UINT index;

	index = (UINT)SendDlgItemMessage (hwnd, ctrl_id, RB_IDTOINDEX, (WPARAM)band_id, 0);

	if (index == UINT_MAX)
		return;

	SendDlgItemMessage (hwnd, ctrl_id, RB_DELETEBAND, (WPARAM)index, 0);
}

//
// Control: toolbar
//

VOID _r_toolbar_addbutton (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT command_id,
	_In_ INT style,
	_In_opt_ INT_PTR text,
	_In_ INT state,
	_In_ INT image_id
)
{
	TBBUTTON tbi = {0};
	INT button_id;

	tbi.idCommand = command_id;
	tbi.fsStyle = (BYTE)style;
	tbi.iString = text;
	tbi.fsState = (BYTE)state;
	tbi.iBitmap = image_id;

	button_id = _r_toolbar_getbuttoncount (hwnd, ctrl_id);

	SendDlgItemMessage (hwnd, ctrl_id, TB_INSERTBUTTON, (WPARAM)button_id, (LPARAM)&tbi);
}

VOID _r_toolbar_addseparator (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	_r_toolbar_addbutton (hwnd, ctrl_id, 0, BTNS_SEP, 0, 0, I_IMAGENONE);
}

INT _r_toolbar_getwidth (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	RECT rect;
	INT total_width;

	total_width = 0;

	SetRectEmpty (&rect);

	for (INT i = 0; i < _r_toolbar_getbuttoncount (hwnd, ctrl_id); i++)
	{
		if (SendDlgItemMessage (hwnd, ctrl_id, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&rect) != 0)
			total_width += _r_calc_rectwidth (&rect);
	}

	return total_width;
}

VOID _r_toolbar_setbutton (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT command_id,
	_In_opt_ LPCWSTR text,
	_In_opt_ INT style,
	_In_opt_ INT state,
	_In_ INT image_id
)
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

	if (image_id != I_IMAGENONE)
	{
		tbi.dwMask |= TBIF_IMAGE;
		tbi.iImage = image_id;
	}

	SendDlgItemMessage (hwnd, ctrl_id, TB_SETBUTTONINFO, (WPARAM)command_id, (LPARAM)&tbi);
}

VOID _r_toolbar_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style
)
{
	HWND hctrl;
	HWND htip;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return;

	SetWindowTheme (hctrl, L"Explorer", NULL);

	htip = (HWND)SendMessage (hctrl, TB_GETTOOLTIPS, 0, 0);

	if (htip)
		_r_ctrl_settipstyle (htip);

	SendMessage (hctrl, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof (TBBUTTON), 0);

	if (ex_style)
		SendMessage (hctrl, TB_SETEXTENDEDSTYLE, 0, (LPARAM)ex_style);
}

//
// Control: progress bar
//

VOID _r_progress_setmarquee (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ BOOL is_enable
)
{
	SendDlgItemMessage (hwnd, ctrl_id, PBM_SETMARQUEE, (WPARAM)is_enable, (LPARAM)10);

	_r_wnd_addstyle (hwnd, ctrl_id, is_enable ? PBS_MARQUEE : 0, PBS_MARQUEE, GWL_STYLE);
}

//
// Util
//

BOOL CALLBACK _r_util_activate_window_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	WCHAR window_title[128];
	LPCWSTR app_name;
	ULONG pid;

	if (!_r_wnd_isdialog (hwnd))
		return TRUE;

	GetWindowThreadProcessId (hwnd, &pid);

	if (HandleToUlong (NtCurrentProcessId ()) == pid)
		return TRUE;

	app_name = (LPCWSTR)lparam;

	// check window title
	if (!GetWindowText (hwnd, window_title, RTL_NUMBER_OF (window_title)))
		return TRUE;

	if (!(_r_wnd_getstyle (hwnd) & WS_DLGFRAME))
		return TRUE;

	if (_r_str_compare_length (window_title, app_name, _r_str_getlength (app_name)) == 0)
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

INT CALLBACK _r_util_enum_font_callback (
	_In_ const LOGFONT * logfont,
	_In_ const TEXTMETRIC * textmetric,
	_In_ DWORD font_type,
	_In_ LPARAM lparam
)
{
	PLOGFONT logfont_req;

	logfont_req = (PLOGFONT)lparam;

	if (_r_str_compare (logfont_req->lfFaceName, logfont->lfFaceName) == 0)
	{
		if (logfont_req->lfWeight)
		{
			if (logfont_req->lfWeight != logfont->lfWeight)
				return TRUE;
		}

		return FALSE;
	}

	return TRUE;
}

VOID _r_util_templatewritecontrol (
	_Inout_ PBYTE_PTR ptr,
	_In_ ULONG ctrl_id,
	_In_ ULONG style,
	_In_ SHORT x,
	_In_ SHORT y,
	_In_ SHORT cx,
	_In_ SHORT cy,
	_In_ LPCWSTR class_name
)
{
	*ptr = (PBYTE)ALIGN_UP (*ptr, ULONG); // align as ULONG

	//
	// fill DLGITEMTEMPLATEEX
	//

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

VOID _r_util_templatewriteshort (
	_Inout_ PBYTE_PTR ptr,
	_In_ WORD data
)
{
	_r_util_templatewrite_ex (ptr, &data, sizeof (data));
}

VOID _r_util_templatewritestring (
	_Inout_ PBYTE_PTR ptr,
	_In_ LPCWSTR string
)
{
	SIZE_T length;

	length = _r_str_getlength (string) * sizeof (WCHAR);

	*(LPWSTR)PTR_ADD_OFFSET (*ptr, length) = UNICODE_NULL; // terminate

	_r_util_templatewrite_ex (ptr, string, length + sizeof (UNICODE_NULL));
}

VOID _r_util_templatewriteulong (
	_Inout_ PBYTE_PTR ptr,
	_In_ ULONG data
)
{
	_r_util_templatewrite_ex (ptr, &data, sizeof (data));
}

VOID _r_util_templatewrite_ex (
	_Inout_ PBYTE_PTR ptr,
	_In_bytecount_ (size) LPCVOID data,
	_In_ SIZE_T size
)
{
	RtlCopyMemory (*ptr, data, size);

	*ptr = PTR_ADD_OFFSET (*ptr, size);
}

VOID NTAPI _r_util_cleanarray_callback (
	_In_ PVOID entry
)
{
	PR_ARRAY array_node;

	array_node = entry;

	_r_obj_cleararray (array_node);

	array_node->allocated_count = 0;

	if (array_node->items)
		_r_mem_free (array_node->items);
}

VOID NTAPI _r_util_cleanlist_callback (
	_In_ PVOID entry
)
{
	PR_LIST list_node;

	list_node = entry;

	_r_obj_clearlist (list_node);

	list_node->allocated_count = 0;

	if (list_node->items)
		_r_mem_free (list_node->items);
}

VOID NTAPI _r_util_cleanhashtable_callback (
	_In_ PVOID entry
)
{
	PR_HASHTABLE hashtable;

	hashtable = entry;

	_r_obj_clearhashtable (hashtable);

	hashtable->allocated_buckets = 0;
	hashtable->allocated_entries = 0;

	if (hashtable->buckets)
		_r_mem_free (hashtable->buckets);

	if (hashtable->entries)
		_r_mem_free (hashtable->entries);
}

VOID NTAPI _r_util_cleanhashtablepointer_callback (
	_In_ PVOID entry
)
{
	PR_OBJECT_POINTER object_ptr;

	object_ptr = entry;

	if (object_ptr->object_body)
		_r_obj_dereference (object_ptr->object_body);
}
