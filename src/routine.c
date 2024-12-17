// routine.c
// project sdk library
//
// Copyright (c) 2012-2025 Henry++

#include "routine.h"

//
// Vars
//

static R_QUEUED_LOCK _r_context_lock = PR_QUEUED_LOCK_INIT;

//
// Debugging
//

VOID _r_debug (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	WCHAR string[512];
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	OutputDebugStringW (string);
}

//
// Console
//

WORD _r_console_getcolor ()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hconsole;

	hconsole = NtCurrentPeb ()->ProcessParameters->StandardOutput;

	if (!hconsole || hconsole == INVALID_HANDLE_VALUE)
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

	hconsole = NtCurrentPeb ()->ProcessParameters->StandardOutput;

	if (!hconsole || hconsole == INVALID_HANDLE_VALUE)
		return;

	SetConsoleTextAttribute (hconsole, clr);
}

VOID _r_console_writestring_ex (
	_In_reads_ (length) LPCWSTR string,
	_In_ ULONG length
)
{
	HANDLE hconsole;

	hconsole = NtCurrentPeb ()->ProcessParameters->StandardOutput;

	if (!hconsole || hconsole == INVALID_HANDLE_VALUE)
		return;

	WriteConsoleW (hconsole, string, length, NULL, NULL);
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

	_r_console_writestring2 (&string->sr);

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

	length = _vscwprintf_l (format, NULL, arg_ptr);

	if (length == 0 || length == -1)
		return _r_obj_referenceemptystring ();

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf_l (string->buffer, length, format, NULL, arg_ptr);
#pragma warning(pop)

	return string;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_format_bytesize64 (
	_Out_writes_ (buffer_length) LPWSTR buffer,
	_In_ UINT buffer_length,
	_In_ ULONG64 bytes
)
{
	HRESULT status;

	// vista (sp1)+
	status = StrFormatByteSizeEx (bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, buffer_length);

	if (FAILED (status))
		*buffer = UNICODE_NULL;

	return status;
}

_Ret_maybenull_
PR_STRING _r_format_filetime (
	_In_ PFILETIME file_time,
	_In_ ULONG flags
)
{
	PR_STRING string;
	ULONG buffer_length = 128;
	ULONG return_length;

	string = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = SHFormatDateTimeW (file_time, &flags, string->buffer, buffer_length);

	if (return_length)
	{
		_r_str_setlength (&string->sr, return_length * sizeof (WCHAR));

		_r_str_trimstring2 (&string->sr, L" ", 0);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_format_interval (
	_In_ LONG64 seconds,
	_In_ BOOLEAN is_precise
)
{
	PR_STRING string;
	ULONG_PTR return_length;
	ULONG buffer_length = 128;
	ULONG seconds32;
	HRESULT status;

	seconds = _r_calc_seconds2milliseconds64 (seconds);

	status = LongLongToULong (seconds, &seconds32);

	if (FAILED (status))
		return NULL;

	string = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	return_length = StrFromTimeIntervalW (string->buffer, buffer_length, seconds32, is_precise ? 3 : 1);

	if (return_length)
	{
		_r_str_setlength (&string->sr, return_length * sizeof (WCHAR));

		_r_str_trimstring2 (&string->sr, L" ", 0);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

VOID _r_format_number (
	_Out_writes_ (buffer_length) LPWSTR buffer,
	_In_ ULONG buffer_length,
	_In_ LONG64 number
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR decimal_separator[4] = {0};
	static WCHAR thousand_separator[4] = {0};

	NUMBERFMT nf = {0};
	WCHAR string[64];

	if (_r_initonce_begin (&init_once))
	{
		if (!GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal_separator, RTL_NUMBER_OF (decimal_separator)))
		{
			decimal_separator[0] = L'.';
			decimal_separator[1] = UNICODE_NULL;
		}

		if (!GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousand_separator, RTL_NUMBER_OF (thousand_separator)))
		{
			thousand_separator[0] = L',';
			thousand_separator[1] = UNICODE_NULL;
		}

		_r_initonce_end (&init_once);
	}

	nf.lpDecimalSep = decimal_separator;
	nf.lpThousandSep = thousand_separator;
	nf.NegativeOrder = 1;
	nf.Grouping = 3;

	_r_str_fromlong64 (string, RTL_NUMBER_OF (string), number);

	if (!GetNumberFormatEx (LOCALE_NAME_USER_DEFAULT, 0, string, &nf, buffer, buffer_length))
		_r_str_copy (buffer, buffer_length, string);
}

_Ret_maybenull_
PR_STRING _r_format_unixtime (
	_In_ LONG64 unixtime,
	_In_opt_ ULONG flags
)
{
	FILETIME file_time;
	PR_STRING string;

	_r_unixtime_to_filetime (unixtime, &file_time);

	if (!flags)
		flags = FDTF_DEFAULT;

	string = _r_format_filetime (&file_time, flags);

	return string;
}

//
// Synchronization: A fast event object
//

VOID FASTCALL _r_event_intialize (
	_Out_ PR_EVENT event_object
)
{
	event_object->value = PR_EVENT_REFCOUNT_INC;
	event_object->event_handle = NULL;
}

VOID _r_event_dereference (
	_Inout_ PR_EVENT event_object,
	_In_opt_ HANDLE event_handle
)
{
	ULONG_PTR value;

	value = InterlockedExchangeAddPointer ((LONG_PTR volatile*)(&event_object->value), -PR_EVENT_REFCOUNT_INC);

	// See if the reference count has become 0.
	if (((value >> PR_EVENT_REFCOUNT_SHIFT) & PR_EVENT_REFCOUNT_MASK) - 1 == 0)
	{
		if (event_handle)
		{
			NtClose (event_handle);

			event_object->event_handle = NULL;
		}
	}
}

VOID _r_event_reference (
	_Inout_ PR_EVENT event_object
)
{
	InterlockedExchangeAddPointer ((LONG_PTR volatile*)(&event_object->value), PR_EVENT_REFCOUNT_INC);
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
	HANDLE event_handle;
	ULONG_PTR value;
	BOOLEAN result;

	if (_r_event_test (event_object))
		return TRUE;

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
		if (_InterlockedCompareExchangePointer (&event_object->event_handle, event_handle, NULL) != NULL)
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

BOOLEAN _r_initonce_begin (
	_Inout_ PR_INITONCE init_once
)
{
	NTSTATUS status;

	// vista+
	status = RtlRunOnceBeginInitialize (init_once, RTL_RUN_ONCE_CHECK_ONLY, NULL);

	if (NT_SUCCESS (status))
		return FALSE;

	status = RtlRunOnceBeginInitialize (init_once, 0, NULL);

	return (status == STATUS_PENDING);
}

//
// Synchronization: Free list
//

VOID _r_freelist_initialize (
	_Out_ PR_FREE_LIST free_list,
	_In_ ULONG_PTR size,
	_In_ ULONG maximum_count
)
{
	RtlInitializeSListHead (&free_list->list_head);

	free_list->count = 0;
	free_list->size = size;
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
		_InterlockedDecrement (&free_list->count);

		entry = CONTAINING_RECORD (list_entry, R_FREE_LIST_ENTRY, list_entry);

		RtlSecureZeroMemory (&entry->body, free_list->size);
	}
	else
	{
		entry = _r_mem_allocate (UFIELD_OFFSET (R_FREE_LIST_ENTRY, body) + free_list->size);
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

		_InterlockedIncrement (&free_list->count);
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

FORCEINLINE ULONG _r_queuedlock_getspincount ()
{
	if (NtCurrentPeb ()->NumberOfProcessors > 1)
		return 4000;

	return 0;
}

FORCEINLINE NTSTATUS _r_queuedlock_blockwaitblock (
	_Inout_ PR_QUEUED_WAIT_BLOCK wait_block,
	_In_ BOOLEAN is_spin,
	_In_opt_ PLARGE_INTEGER timeout
)
{
	NTSTATUS status;

	if (is_spin)
	{
		for (ULONG i = _r_queuedlock_getspincount (); i != 0; i--)
		{
			if (!(*(volatile ULONG*)&wait_block->flags & PR_QUEUED_WAITER_SPINNING))
				return STATUS_SUCCESS;

			YieldProcessor ();
		}
	}

	if (_interlockedbittestandreset ((PLONG)&wait_block->flags, PR_QUEUED_WAITER_SPINNING_SHIFT))
	{
		status = NtWaitForKeyedEvent (_r_queuedlock_getevent (), wait_block, FALSE, timeout);

		// If an error occurred (timeout is not an error), raise an exception as it is nearly impossible to recover from this situation.
		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);
	}
	else
	{
		status = STATUS_SUCCESS;
	}

	return status;
}

FORCEINLINE PR_QUEUED_WAIT_BLOCK _r_queuedlock_findlastwaitblock (
	_In_ ULONG_PTR value
)
{
	PR_QUEUED_WAIT_BLOCK last_wait_block;
	PR_QUEUED_WAIT_BLOCK wait_block;

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

_Ret_maybenull_
FORCEINLINE PR_QUEUED_WAIT_BLOCK _r_queuedlock_preparetowake (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR current_value,
	_In_ BOOLEAN is_ignoreowned,
	_In_ BOOLEAN is_wakeall
)
{
	PR_QUEUED_WAIT_BLOCK previous_wait_block;
	PR_QUEUED_WAIT_BLOCK first_wait_block;
	PR_QUEUED_WAIT_BLOCK last_wait_block;
	PR_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR new_value;
	ULONG_PTR value;

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

			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

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
		if (!is_wakeall && (wait_block->flags & PR_QUEUED_WAITER_EXCLUSIVE) && (previous_wait_block = wait_block->previous_block))
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
				InterlockedExchangeAddPointer ((PLONG_PTR)&queued_lock->value, -(LONG_PTR)PR_QUEUED_LOCK_TRAVERSING); // Clear the traversing bit.

			break;
		}
		else
		{
			// We're waking an exclusive waiter and there is only one waiter, or we are waking a
			// shared waiter and possibly others.
			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, NULL, (PVOID)value);

			if (new_value == value)
				break;

			// Someone changed the lock (acquired it or pushed a wait block).
			value = new_value;
		}
	}

	return wait_block;
}

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
	BOOLEAN is_optimize = FALSE;

	wait_block->previous_block = NULL; // set later by optimization

	wait_block->flags = is_exclusive ? PR_QUEUED_WAITER_EXCLUSIVE | PR_QUEUED_WAITER_SPINNING : PR_QUEUED_WAITER_SPINNING;

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

	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

	*new_value_ptr = new_value;
	*is_optimize_ptr = is_optimize;
	*current_value_ptr = new_value;

	return (new_value == value);
}

FORCEINLINE VOID _r_queuedlock_optimizelist_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_ignoreowned
)
{
	PR_QUEUED_WAIT_BLOCK previous_wait_block;
	PR_QUEUED_WAIT_BLOCK first_wait_block;
	PR_QUEUED_WAIT_BLOCK last_wait_block;
	PR_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR current_value;
	ULONG_PTR new_value;

	current_value = value;

	while (TRUE)
	{
		assert (value & PR_QUEUED_LOCK_TRAVERSING);

		if (!is_ignoreowned && !(current_value & PR_QUEUED_LOCK_OWNED))
		{
			// Someone has requested that we wake waiters.
			_r_queuedlock_wake (queued_lock, current_value, FALSE, FALSE);

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
		new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)current_value);

		if (new_value == current_value)
			break;

		// Either someone pushed a wait block onto the list or someone released ownership. In either
		// case we need to go back.
		current_value = new_value;
	}
}

FORCEINLINE VOID _r_queuedlock_unblockwaitblock (
	_Inout_ PR_QUEUED_WAIT_BLOCK wait_block
)
{
	NTSTATUS status;

	if (!_interlockedbittestandreset ((PLONG)&wait_block->flags, PR_QUEUED_WAITER_SPINNING_SHIFT))
	{
		status = NtReleaseKeyedEvent (_r_queuedlock_getevent (), wait_block, FALSE, NULL);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);
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

VOID FASTCALL _r_queuedlock_acquireexclusive_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	R_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR current_value;
	ULONG_PTR new_value;
	ULONG_PTR value;
	BOOLEAN is_optimize;

	value = queued_lock->value;

	while (TRUE)
	{
		if (!(value & PR_QUEUED_LOCK_OWNED))
		{
			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)(value + PR_QUEUED_LOCK_OWNED), (PVOID)value);

			if (new_value == value)
				break;
		}
		else
		{
			if (_r_queuedlock_pushwaitblock (queued_lock, value, TRUE, &wait_block, &is_optimize, &new_value, &current_value))
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
	ULONG_PTR current_value;
	ULONG_PTR new_value;
	ULONG_PTR value;
	BOOLEAN is_optimize;

	value = queued_lock->value;

	while (TRUE)
	{
		// We can't acquire if there are waiters for two reasons:
		//
		// We want to prioritize exclusive acquires over shared acquires. There's currently no fast,
		// safe way of finding the last wait block and incrementing the shared owners count here.
		if (!(value & PR_QUEUED_LOCK_WAITERS) && (!(value & PR_QUEUED_LOCK_OWNED) || (_r_queuedlock_getsharedowners (value) > 0)))
		{
			new_value = (value + PR_QUEUED_LOCK_SHARED_INC) | PR_QUEUED_LOCK_OWNED;

			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

			if (new_value == value)
				break;
		}
		else
		{
			if (_r_queuedlock_pushwaitblock (queued_lock, value, FALSE, &wait_block, &is_optimize, &new_value, &current_value))
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
	ULONG_PTR current_value;
	ULONG_PTR new_value;
	ULONG_PTR value;

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
			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

			if (new_value == value)
				break;
		}
		else
		{
			// We need to wake waiters and no one is traversing the list.
			// Try to set the traversing bit and wake waiters.

			new_value = (value - PR_QUEUED_LOCK_OWNED + PR_QUEUED_LOCK_TRAVERSING);
			current_value = new_value;

			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

			if (new_value == value)
			{
				_r_queuedlock_wake (queued_lock, current_value, FALSE, FALSE);

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
	PR_QUEUED_WAIT_BLOCK wait_lock;
	ULONG_PTR current_value;
	ULONG_PTR new_value;
	ULONG_PTR value;

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

		new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

		if (new_value == value)
			return;

		value = new_value;
	}

	if (value & PR_QUEUED_LOCK_MULTIPLE_SHARED)
	{
		// Unfortunately we have to find the last wait block and decrement the shared owners count.
		wait_lock = _r_queuedlock_findlastwaitblock (value);

		if ((ULONG)_InterlockedDecrement ((PLONG)&wait_lock->shared_owners) > 0)
			return;
	}

	while (TRUE)
	{
		if (value & PR_QUEUED_LOCK_TRAVERSING)
		{
			new_value = value & ~(PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_MULTIPLE_SHARED);

			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

			if (new_value == value)
				break;
		}
		else
		{
			new_value = (value & ~(PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_MULTIPLE_SHARED)) | PR_QUEUED_LOCK_TRAVERSING;

			current_value = new_value;

			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

			if (new_value == value)
			{
				_r_queuedlock_wake (queued_lock, current_value, FALSE, FALSE);

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
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_ignoreowned,
	_In_ BOOLEAN is_wakeall
)
{
	PR_QUEUED_WAIT_BLOCK previous_wait_block;
	PR_QUEUED_WAIT_BLOCK wait_block;

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
	ULONG_PTR current_value;
	ULONG_PTR new_value;

	new_value = value + PR_QUEUED_LOCK_TRAVERSING;

	current_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, (PVOID)new_value, (PVOID)value);

	if (current_value == value)
		_r_queuedlock_wake (queued_lock, new_value, FALSE, FALSE);
}

//
// Synchronization: Condition
//

VOID FASTCALL _r_condition_pulse (
	_Inout_ PR_CONDITION condition
)
{
	if (condition->value & PR_QUEUED_LOCK_WAITERS)
		_r_queuedlock_wake (condition, condition->value, TRUE, FALSE);
}

VOID FASTCALL _r_condition_waitfor (
	_Inout_ PR_CONDITION condition,
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	R_QUEUED_WAIT_BLOCK wait_block;
	ULONG_PTR current_value;
	ULONG_PTR value;
	BOOLEAN is_optimize = FALSE;

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
	ULONG_PTR new_value;
	ULONG_PTR value;

	// Increment the reference count only if rundown has not started.
	while (TRUE)
	{
		value = protection->value;

		if (value & PR_RUNDOWN_ACTIVE)
			return FALSE;

		new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, (PVOID)(value + PR_RUNDOWN_REF_INC), (PVOID)value);

		if (new_value == value)
			return TRUE;
	}
}

VOID FASTCALL _r_protection_release_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	PR_RUNDOWN_WAIT_BLOCK wait_block;
	ULONG_PTR new_value;
	ULONG_PTR value;

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
			new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, (PVOID)(value - PR_RUNDOWN_REF_INC), (PVOID)value);

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
	ULONG_PTR new_value;
	ULONG_PTR value;
	ULONG_PTR count;
	BOOLEAN is_initialized = FALSE;

	// Fast path. If the reference count is 0 or rundown has already been completed, return.
	value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, IntToPtr (PR_RUNDOWN_ACTIVE), NULL);

	if (value == 0 || value == PR_RUNDOWN_ACTIVE)
		return;

	while (TRUE)
	{
		value = protection->value;
		count = value >> PR_RUNDOWN_REF_SHIFT;

		// Initialize the wait block if necessary.
		if (count != 0 && !is_initialized)
		{
			_r_event_intialize (&wait_block.wake_event);

			is_initialized = TRUE;
		}

		// Save the existing reference count.
		wait_block.count = count;

		new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, (PVOID)((ULONG_PTR)&wait_block | PR_RUNDOWN_ACTIVE), (PVOID)value);

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
		_r_freelist_initialize (&free_list, sizeof (R_WORKQUEUE_ITEM), 128);

		_r_initonce_end (&init_once);
	}

	return &free_list;
}

HANDLE _r_workqueue_getsemaphore (
	_Inout_ PR_WORKQUEUE work_queue
)
{
	HANDLE hsemaphore;

	hsemaphore = work_queue->hsemaphore;

	if (!hsemaphore)
	{
		NtCreateSemaphore (&hsemaphore, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);

		assert (hsemaphore);

		if (_InterlockedCompareExchangePointer (&work_queue->hsemaphore, hsemaphore, NULL) != NULL)
		{
			// Someone else created the semaphore before we did.
			NtClose (hsemaphore);

			hsemaphore = work_queue->hsemaphore;
		}
	}

	return hsemaphore;
}

VOID _r_workqueue_initialize (
	_Out_ PR_WORKQUEUE work_queue,
	_In_ ULONG maximum_threads,
	_In_opt_ PR_ENVIRONMENT environment,
	_In_opt_ LPCWSTR thread_name
)
{
	RtlSecureZeroMemory (work_queue, sizeof (R_WORKQUEUE));

	InitializeListHead (&work_queue->queue_list_head);

	_r_queuedlock_initialize (&work_queue->queue_lock);
	_r_queuedlock_initialize (&work_queue->state_lock);

	_r_protection_initialize (&work_queue->rundown_protect);

	_r_condition_initialize (&work_queue->queue_empty_condition);

	if (environment)
	{
		RtlCopyMemory (&work_queue->environment, environment, sizeof (R_ENVIRONMENT));
	}
	else
	{
		_r_sys_setenvironment (&work_queue->environment, THREAD_PRIORITY_NORMAL, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);
	}

	work_queue->thread_name = thread_name ? _r_obj_createstring (thread_name) : NULL;

	work_queue->maximum_threads = maximum_threads;
	work_queue->minimum_threads = 0;

	work_queue->hsemaphore = NULL;

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

	if (work_queue->hsemaphore)
		NtReleaseSemaphore (work_queue->hsemaphore, work_queue->current_threads, NULL);

	_r_protection_waitfor (&work_queue->rundown_protect);

	// Free all un-executed work items.
	list_entry = work_queue->queue_list_head.Flink;

	while (list_entry != &work_queue->queue_list_head)
	{
		work_queue_item = CONTAINING_RECORD (list_entry, R_WORKQUEUE_ITEM, list_entry);

		list_entry = list_entry->Flink;

		_r_workqueue_destroyitem (work_queue_item);
	}

	SAFE_DELETE_HANDLE (work_queue->hsemaphore);
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

NTSTATUS NTAPI _r_workqueue_threadproc (
	_In_ PVOID arglist
)
{
	PR_WORKQUEUE_ITEM work_queue_item;
	PR_WORKQUEUE work_queue;
	PLIST_ENTRY list_entry;
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
			if (work_queue->current_threads > work_queue->maximum_threads && work_queue->current_threads > work_queue->minimum_threads)
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
			status = _r_sys_waitforsingleobject (_r_workqueue_getsemaphore (work_queue), 7500);
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

				work_queue_item->base_address (work_queue_item->context);

				_InterlockedDecrement (&work_queue->busy_count);

				_r_workqueue_destroyitem (work_queue_item);
			}
		}
		else
		{
			is_terminate = FALSE;

			// No work arrived before the timeout passed, or we are terminating, or some error occurred. Terminate the thread.
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

VOID NTAPI _r_workqueue_queueitem (
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

	_InterlockedIncrement (&work_queue->busy_count);

	_r_queuedlock_releaseexclusive (&work_queue->queue_lock);

	// Signal the semaphore once to let a worker thread continue.
	NtReleaseSemaphore (_r_workqueue_getsemaphore (work_queue), 1, NULL);

	// Check if all worker threads are currently busy, and if we can create more threads.
	if (work_queue->busy_count >= work_queue->current_threads && work_queue->current_threads < work_queue->maximum_threads)
	{
		// Lock and re-check.
		_r_queuedlock_acquireexclusive (&work_queue->state_lock);

		if (work_queue->current_threads < work_queue->maximum_threads)
		{
			// Make sure the structure doesn't get deleted while the thread is running.
			if (_r_protection_acquire (&work_queue->rundown_protect))
			{
				status = _r_sys_createthread (NULL, NtCurrentProcess (), &_r_workqueue_threadproc, work_queue, &work_queue->environment, _r_obj_getstring (work_queue->thread_name));

				if (NT_SUCCESS (status))
				{
					work_queue->current_threads += 1;
				}
				else
				{
					_r_protection_release (&work_queue->rundown_protect);
				}
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
// Synchronization: Mutant (mutex)
//

_When_ (NT_SUCCESS (return), _Acquires_lock_ (*out_buffer))
NTSTATUS _r_mutant_create (
	_In_ LPWSTR name,
	_In_ ULONG desired_access,
	_In_ BOOLEAN is_initowner,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING us;
	WCHAR buffer[128];
	HANDLE hmutex;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\Sessions\\%u\\BaseNamedObjects\\%s", NtCurrentPeb ()->SessionId, name);

	_r_obj_initializeunicodestring (&us, buffer);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE | OBJ_OPENIF, NULL, NULL);

	status = NtCreateMutant (&hmutex, desired_access, &oa, is_initowner);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hmutex;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_mutant_open (
	_In_ LPWSTR name,
	_In_ ULONG desired_access,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING us;
	WCHAR buffer[128];
	HANDLE hmutex;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\Sessions\\%u\\BaseNamedObjects\\%s", NtCurrentPeb ()->SessionId, name);

	_r_obj_initializeunicodestring (&us, buffer);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE | OBJ_OPENIF, NULL, NULL);

	status = NtOpenMutant (&hmutex, desired_access, &oa);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hmutex;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_When_ (NT_SUCCESS (return), _Releases_lock_ (*hmutex))
NTSTATUS _r_mutant_destroy (
	_Inout_ PHANDLE hmutex
)
{
	HANDLE original_mutex;
	NTSTATUS status;

	original_mutex = *hmutex;

	status = NtReleaseMutant (original_mutex, NULL);

	if (NT_SUCCESS (status))
	{
		*hmutex = NULL;

		NtClose (original_mutex);
	}

	return status;
}

_Success_ (return)
BOOLEAN _r_mutant_isexists (
	_In_ LPWSTR name
)
{
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING us;
	WCHAR buffer[128];
	HANDLE hmutex;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\Sessions\\%u\\BaseNamedObjects\\%s", NtCurrentPeb ()->SessionId, name);

	_r_obj_initializeunicodestring (&us, buffer);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtOpenMutant (&hmutex, MUTANT_QUERY_STATE, &oa);

	if (NT_SUCCESS (status))
	{
		NtClose (hmutex);

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
		// win8+
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
			heap_handle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1 | HEAP_CREATE_SEGMENT_HEAP, NULL, 0, 0, NULL, NULL);

		if (!heap_handle)
			heap_handle = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1, NULL, _r_calc_megabytes2bytes (2), _r_calc_megabytes2bytes (1), NULL, NULL);

		if (heap_handle)
			RtlSetHeapInformation (heap_handle, HeapCompatibilityInformation, &(ULONG){HEAP_COMPATIBILITY_LFH}, sizeof (ULONG));

		_r_initonce_end (&init_once);
	}

	return heap_handle;
}

DECLSPEC_RESTRICT
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocate (
	_In_ ULONG_PTR bytes_count
)
{
	HANDLE heap_handle;
	PVOID base_address;

	assert (bytes_count);

	heap_handle = _r_mem_getheap ();

	base_address = RtlAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);

	return base_address;
}

_Ret_maybenull_
DECLSPEC_RESTRICT
_Must_inspect_result_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocatesafe (
	_In_ ULONG_PTR bytes_count
)
{
	HANDLE heap_handle;
	PVOID base_address;

	assert (bytes_count);

	heap_handle = _r_mem_getheap ();

	base_address = RtlAllocateHeap (heap_handle, HEAP_ZERO_MEMORY, bytes_count);

	return base_address;
}

DECLSPEC_RESTRICT
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocateandcopy (
	_In_ LPCVOID source,
	_In_ ULONG_PTR bytes_count
)
{
	PVOID base_address;

	assert (bytes_count);

	base_address = _r_mem_allocate (bytes_count);

	RtlCopyMemory (base_address, source, bytes_count);

	return base_address;
}

DECLSPEC_RESTRICT
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocate (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ ULONG_PTR bytes_count
)
{
	HANDLE heap_handle;
	PVOID new_address;

	assert (bytes_count);

	heap_handle = _r_mem_getheap ();

	// RtlReAllocateHeap does not behave the same as realloc when Memory is NULL.
	// For consistency with realloc above and easier drop-in replacements for
	// realloc, produce the same behavior as realloc. If Memory is NULL, then
	// allocate a new block.

	if (base_address)
	{
		new_address = RtlReAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, base_address, bytes_count);
	}
	else
	{
		new_address = RtlAllocateHeap (heap_handle, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);
	}

	return new_address;
}

_Ret_maybenull_
DECLSPEC_RESTRICT
_Must_inspect_result_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocatesafe (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ ULONG_PTR bytes_count
)
{
	HANDLE heap_handle;
	PVOID new_address;

	assert (bytes_count);

	heap_handle = _r_mem_getheap ();

	// RtlReAllocateHeap does not behave the same as realloc when Memory is NULL.
	// For consistency with realloc above and easier drop-in replacements for
	// realloc, produce the same behavior as realloc. If Memory is NULL, then
	// allocate a new block.

	if (base_address)
	{
		new_address = RtlReAllocateHeap (heap_handle, HEAP_ZERO_MEMORY, base_address, bytes_count);
	}
	else
	{
		new_address = RtlAllocateHeap (heap_handle, HEAP_ZERO_MEMORY, bytes_count);
	}

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

_Success_ (return)
BOOLEAN _r_mem_frobnicate (
	_Inout_ PR_BYTEREF bytes
)
{
	LPSTR buffer;
	ULONG_PTR length;
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
	_In_ ULONG_PTR bytes_count,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_OBJECT_HEADER object_header;

	object_header = _r_mem_allocate (UFIELD_OFFSET (R_OBJECT_HEADER, body) + bytes_count);

	_InterlockedIncrement (&object_header->ref_count);

	object_header->cleanup_callback = cleanup_callback;

	return PR_OBJECT_HEADER_TO_OBJECT (object_header);
}

VOID NTAPI _r_obj_dereferencelist (
	_In_reads_ (objects_count) PVOID_PTR objects_list,
	_In_ ULONG_PTR objects_count
)
{
	for (ULONG_PTR i = 0; i < objects_count; i++)
	{
		_r_obj_dereference_ex (objects_list[i], 1);
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

	old_count = _InterlockedExchangeAdd (&object_header->ref_count, -ref_count);

	new_count = old_count - ref_count;

	if (new_count == 0)
	{
		if (object_header->cleanup_callback)
			object_header->cleanup_callback (object_body);

		_r_mem_free (object_header);
	}
	else if (new_count < 0)
	{
		RtlRaiseStatus (STATUS_IN_PAGE_ERROR);
	}
}

VOID NTAPI _r_obj_movereference (
	_Inout_ PVOID_PTR object_body,
	_In_opt_ PVOID new_object
)
{
	PVOID old_object;

	old_object = *object_body;
	*object_body = new_object;

	if (old_object)
		_r_obj_dereference (old_object);
}

PVOID NTAPI _r_obj_reference (
	_In_ PVOID object_body
)
{
	PR_OBJECT_HEADER object_header;

	object_header = PR_OBJECT_TO_OBJECT_HEADER (object_body);

	_InterlockedIncrement (&object_header->ref_count);

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


VOID NTAPI _r_obj_swapreference (
	_Inout_ PVOID_PTR object_body,
	_In_opt_ PVOID new_object
)
{
	PVOID old_object;

	old_object = *object_body;
	*object_body = new_object;

	if (old_object)
		_r_obj_dereference (old_object);

	if (new_object)
		new_object = _r_obj_reference (new_object);
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

PR_BYTE _r_obj_createbyte4 (
	_In_ PR_STORAGE string
)
{
	return _r_obj_createbyte_ex (string->buffer, string->length);
}

PR_BYTE _r_obj_createbyte_ex (
	_In_opt_ LPCSTR buffer,
	_In_ ULONG_PTR length
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

VOID _r_obj_setbytelength_ex (
	_Inout_ PR_BYTE string,
	_In_ ULONG_PTR new_length,
	_In_ ULONG_PTR allocated_length
)
{
	if (allocated_length < new_length)
		new_length = allocated_length;

	string->length = new_length;

	_r_obj_writebytenullterminator (string); // terminate
}

VOID _r_obj_skipbytelength (
	_Inout_ PR_BYTEREF string,
	_In_ ULONG_PTR length
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

//
// 16-bit string object
//

PR_STRING _r_obj_createstring_ex (
	_In_opt_ LPCWSTR string,
	_In_ ULONG_PTR length
)
{
	PR_STRING buffer;

	assert (!(length & 0x01));

	if (!length)
		length = sizeof (UNICODE_NULL);

	buffer = _r_obj_allocate (UFIELD_OFFSET (R_STRING, data) + length + sizeof (UNICODE_NULL), NULL);

	buffer->length = length;
	buffer->buffer = buffer->data;

	if (string)
	{
		RtlCopyMemory (buffer->buffer, string, length);

		_r_str_writenullterminator (&buffer->sr);
	}
	else
	{
		buffer->buffer[0] = UNICODE_NULL;
	}

	return buffer;
}

PR_STRING _r_obj_concatstrings (
	_In_ ULONG_PTR count,
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
	_In_ ULONG_PTR count,
	_In_ va_list arg_ptr
)
{
	va_list argptr;
	PR_STRING string;
	LPCWSTR arg;
	LPWSTR ptr;
	ULONG_PTR cached_length[PR_SIZE_CONCAT_LENGTH_CACHE] = {0};
	ULONG_PTR total_length = 0;
	ULONG_PTR string_length;

	argptr = arg_ptr;

	for (ULONG_PTR i = 0; i < count; i++)
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

	for (ULONG_PTR i = 0; i < count; i++)
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

		ptr = PTR_ADD_OFFSET (string->buffer, total_length);

		RtlCopyMemory (ptr, arg, string_length);

		total_length += string_length;
	}

	return string;
}

PR_STRING _r_obj_concatstringrefs (
	_In_ ULONG_PTR count,
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
	_In_ ULONG_PTR count,
	_In_ va_list arg_ptr
)
{
	va_list argptr;
	PR_STRINGREF arg;
	PR_STRING string;
	LPWSTR ptr;
	ULONG_PTR total_length = 0;

	argptr = arg_ptr;

	for (ULONG_PTR i = 0; i < count; i++)
	{
		arg = va_arg (argptr, PR_STRINGREF);

		if (!arg || !arg->length)
			continue;

		total_length += arg->length;
	}

	string = _r_obj_createstring_ex (NULL, total_length);

	argptr = arg_ptr;
	total_length = 0;

	for (ULONG_PTR i = 0; i < count; i++)
	{
		arg = va_arg (argptr, PR_STRINGREF);

		if (!arg || !arg->length)
			continue;

		ptr = PTR_ADD_OFFSET (string->buffer, total_length);

		RtlCopyMemory (ptr, arg->buffer, arg->length);

		total_length += arg->length;
	}

	return string;
}

PR_STRING _r_obj_referenceemptystring ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING string = NULL;

	if (_r_initonce_begin (&init_once))
	{
		string = _r_obj_createstring_ex (NULL, 0);

		_r_initonce_end (&init_once);
	}

	return _r_obj_reference (string);
}

VOID _r_obj_removestring (
	_Inout_ PR_STRINGREF string,
	_In_ ULONG_PTR start_pos,
	_In_ ULONG_PTR length
)
{
	RtlMoveMemory (&string->buffer[start_pos], &string->buffer[start_pos + length], string->length - (length + start_pos) * sizeof (WCHAR));

	string->length -= (length * sizeof (WCHAR));

	_r_str_writenullterminator (string);
}

//
// 8-bit string reference object
//

VOID _r_obj_initializebyteref_ex (
	_Out_ PR_BYTEREF string,
	_In_opt_ LPSTR buffer,
	_In_opt_ ULONG_PTR length
)
{
	string->buffer = buffer;
	string->length = length;
}

//
// 16-bit string reference object
//

VOID _r_obj_initializestringref_ex (
	_Out_ PR_STRINGREF string,
	_In_opt_ LPWSTR buffer,
	_In_opt_ ULONG_PTR length
)
{
	assert (!(length & 0x01));

	string->buffer = buffer;
	string->length = length;
}

//
// Unicode string object
//

BOOLEAN _r_obj_initializeunicodestring (
	_Out_ PUNICODE_STRING string,
	_In_opt_z_ LPWSTR buffer
)
{
	ULONG length = 0;

	if (buffer)
		length = (ULONG)_r_str_getlength (buffer) * sizeof (WCHAR);

	return _r_obj_initializeunicodestring_ex (string, buffer, length, length + sizeof (UNICODE_NULL));
}

BOOLEAN _r_obj_initializeunicodestring_ex (
	_Out_ PUNICODE_STRING string,
	_In_opt_z_ LPWSTR buffer,
	_In_opt_ ULONG length,
	_In_opt_ ULONG max_length
)
{
	string->Buffer = buffer;
	string->Length = (USHORT)min (length, UNICODE_STRING_MAX_BYTES);
	string->MaximumLength = (USHORT)min (max_length, UNICODE_STRING_MAX_BYTES);

	return string->Length <= UNICODE_STRING_MAX_BYTES;
}

//
// String builder
//

VOID _r_obj_initializestringbuilder (
	_Out_ PR_STRINGBUILDER sb,
	_In_ ULONG_PTR initial_capacity
)
{
	// Make sure the initial capacity is even, as required for all string objects.
	if (initial_capacity & 0x01)
		initial_capacity += 1;

	sb->allocated_length = initial_capacity;

	// Allocate a new string object for the string builder.
	// We will dereference it and allocate a new one when we need to resize the string.
	sb->string = _r_obj_createstring_ex (NULL, initial_capacity);

	// We will keep modifying the length field of the string so that:
	// 1. We know how much of the string is used, and
	// 2. The user can simply get a reference to the string and use it as-is.
	sb->string->length = 0;

	// Write the null terminator.
	sb->string->buffer[0] = UNICODE_NULL;
}

VOID _r_obj_appendstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ LPCWSTR string,
	_In_ ULONG_PTR length
)
{
	// See if we need to re-allocate the string.
	if (sb->allocated_length < sb->string->length + length)
		_r_obj_resizestringbuilder (sb, sb->string->length + length);

	// Copy the string, add the length, then write the null terminator.
	RtlCopyMemory (PTR_ADD_OFFSET (sb->string->buffer, sb->string->length), string, length);

	sb->string->length += length;

	_r_str_writenullterminator (&sb->string->sr); // terminate
}

VOID _r_obj_appendstringbuilderformat (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_appendstringbuilderformat_v (sb, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_obj_appendstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	ULONG_PTR length_in_bytes;
	LONG length;

	length = _vscwprintf_l (format, NULL, arg_ptr);

	if (length == -1 || length == 0)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (sb->allocated_length < sb->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (sb, sb->string->length + length_in_bytes);

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf_l (PTR_ADD_OFFSET (sb->string->buffer, sb->string->length), length, format, NULL, arg_ptr);
#pragma warning(pop)

	sb->string->length += length_in_bytes;

	_r_str_writenullterminator (&sb->string->sr); // terminate
}

VOID _r_obj_insertstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ ULONG_PTR index,
	_In_ LPCWSTR string,
	_In_ ULONG_PTR length
)
{
	// See if we need to re-allocate the string.
	if (sb->allocated_length < sb->string->length + length)
		_r_obj_resizestringbuilder (sb, sb->string->length + length);

	// Create some space for the string.
	if ((index * sizeof (WCHAR)) < sb->string->length)
		RtlMoveMemory (&sb->string->buffer[index + (length / sizeof (WCHAR))], &sb->string->buffer[index], sb->string->length - (index * sizeof (WCHAR)));

	// Copy the new string.
	RtlCopyMemory (&sb->string->buffer[index], string, length);

	sb->string->length += length;

	_r_str_writenullterminator (&sb->string->sr); // terminate
}

VOID _r_obj_insertstringbuilderformat (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ ULONG_PTR index,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_insertstringbuilderformat_v (sb, index, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_obj_insertstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ ULONG_PTR index,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	ULONG_PTR length_in_bytes;
	LONG length;

	length = _vscwprintf_l (format, NULL, arg_ptr);

	if (length == 0 || length == -1)
		return;

	length_in_bytes = length * sizeof (WCHAR);

	if (sb->allocated_length < sb->string->length + length_in_bytes)
		_r_obj_resizestringbuilder (sb, sb->string->length + length_in_bytes);

	if ((index * sizeof (WCHAR)) < sb->string->length)
		RtlMoveMemory (&sb->string->buffer[index + length], &sb->string->buffer[index], sb->string->length - (index * sizeof (WCHAR)));

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	_vsnwprintf_l (&sb->string->buffer[index], length, format, NULL, arg_ptr);
#pragma warning(pop)

	sb->string->length += length_in_bytes;

	_r_str_writenullterminator (&sb->string->sr); // terminate
}

VOID _r_obj_resizestringbuilder (
	_Inout_ PR_STRINGBUILDER sb,
	_In_ ULONG_PTR new_capacity
)
{
	PR_STRING new_string;
	ULONG_PTR new_size;

	// Double the string size. If that still isn't enough room, just use the new length.
	new_size = sb->allocated_length * 2;

	if (new_capacity & 0x01)
		new_capacity += 1;

	if (new_size < new_capacity)
		new_size = new_capacity;

	// Allocate a new string.
	new_string = _r_obj_createstring_ex (NULL, new_size);

	// Copy the old string to the new string include null terminator.
	RtlCopyMemory (new_string->buffer, sb->string->buffer, sb->string->length + sizeof (UNICODE_NULL));

	// Copy the old string length.
	new_string->length = sb->string->length;

	// Dereference the old string and replace it with the new string.
	_r_obj_movereference (&sb->string, new_string);
}

//
// Array object
//

PR_ARRAY _r_obj_createarray (
	_In_ ULONG_PTR item_size,
	_In_ ULONG_PTR initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_ARRAY array_node;

	// Initial capacity of 0 is not allowed.
	if (initial_capacity == 0)
		initial_capacity = 1;

	array_node = _r_obj_allocate (sizeof (R_ARRAY), &_r_util_cleanarray_callback);

	array_node->allocated_count = initial_capacity;
	array_node->cleanup_callback = cleanup_callback;
	array_node->count = 0;

	array_node->item_size = item_size;
	array_node->items = _r_mem_allocate (initial_capacity * item_size);

	return array_node;
}

PVOID _r_obj_addarrayitem (
	_Inout_ PR_ARRAY array_node,
	_In_opt_ LPCVOID array_item,
	_Out_opt_ PULONG_PTR new_index_ptr
)
{
	PVOID ptr;
	ULONG_PTR new_index;

	if (array_node->count == array_node->allocated_count)
		_r_obj_resizearray (array_node, array_node->allocated_count * 2);

	new_index = array_node->count;

	ptr = _r_obj_getarrayitem (array_node, new_index);

	if (array_item)
	{
		RtlCopyMemory (ptr, array_item, array_node->item_size);
	}
	else
	{
		RtlSecureZeroMemory (ptr, array_node->item_size);
	}

	array_node->count += 1;

	if (new_index_ptr)
		*new_index_ptr = new_index;

	return ptr;
}

VOID _r_obj_addarrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ LPCVOID array_items,
	_In_ ULONG_PTR count
)
{
	if (array_node->allocated_count < array_node->count + count)
	{
		array_node->allocated_count *= 2;

		if (array_node->allocated_count < array_node->count + count)
			array_node->allocated_count = array_node->count + count;

		_r_obj_resizearray (array_node, array_node->allocated_count);
	}

	RtlCopyMemory (
		_r_obj_getarrayitem (array_node, array_node->count),
		array_items,
		count * array_node->item_size
	);

	array_node->count += count;
}

VOID _r_obj_cleararray (
	_Inout_ PR_ARRAY array_node
)
{
	PVOID array_item;
	ULONG_PTR count;

	if (!array_node->count)
		return;

	count = array_node->count;
	array_node->count = 0;

	if (array_node->cleanup_callback)
	{
		for (ULONG_PTR i = 0; i < count; i++)
		{
			array_item = PTR_ADD_OFFSET (array_node->items, i * array_node->item_size);

			array_node->cleanup_callback (array_item);
		}
	}

	RtlSecureZeroMemory (array_node->items, count * array_node->item_size);
}

PVOID _r_obj_getarrayitem (
	_In_ PR_ARRAY array_node,
	_In_ ULONG_PTR index
)
{
	return PTR_ADD_OFFSET (array_node->items, index * array_node->item_size);
}

ULONG_PTR _r_obj_getarraysize (
	_In_ PR_ARRAY array_node
)
{
	return array_node->count;
}

VOID _r_obj_removearrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ ULONG_PTR start_pos,
	_In_ ULONG_PTR count
)
{
	PVOID dst;
	PVOID src;
	PVOID ptr;

	dst = _r_obj_getarrayitem (array_node, start_pos);
	src = _r_obj_getarrayitem (array_node, start_pos + count);

	if (array_node->cleanup_callback)
	{
		for (ULONG_PTR i = start_pos; i < (start_pos + count); i++)
		{
			ptr = PTR_ADD_OFFSET (array_node->items, i * array_node->item_size);

			array_node->cleanup_callback (ptr);
		}
	}

	RtlMoveMemory (dst, src, (array_node->count - start_pos - count) * array_node->item_size);

	array_node->count -= count;
}

VOID _r_obj_resizearray (
	_Inout_ PR_ARRAY array_node,
	_In_ ULONG_PTR new_capacity
)
{
	if (array_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	array_node->allocated_count = new_capacity;

	array_node->items = _r_mem_reallocate (array_node->items, array_node->allocated_count * array_node->item_size);
}

//
// List object
//

PR_LIST _r_obj_createlist (
	_In_ ULONG_PTR initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_LIST list_node;

	// Initial capacity of 0 is not allowed.
	if (initial_capacity == 0)
		initial_capacity = 1;

	list_node = _r_obj_allocate (sizeof (R_LIST), &_r_util_cleanlist_callback);

	list_node->allocated_count = initial_capacity;
	list_node->cleanup_callback = cleanup_callback;
	list_node->count = 0;

	list_node->items = _r_mem_allocate (list_node->allocated_count * sizeof (PVOID));

	return list_node;
}

VOID _r_obj_addlistitem (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item
)
{
	_r_obj_addlistitem_ex (list_node, list_item, NULL);
}

VOID _r_obj_addlistitem_ex (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item,
	_Out_opt_ PULONG_PTR new_index_ptr
)
{
	ULONG_PTR new_index;

	if (list_node->count == list_node->allocated_count)
		_r_obj_resizelist (list_node, list_node->allocated_count * 2);

	new_index = list_node->count;

	list_node->items[new_index] = list_item;

	if (new_index_ptr)
		*new_index_ptr = new_index;

	list_node->count += 1;
}

VOID _r_obj_clearlist (
	_Inout_ PR_LIST list_node
)
{
	PVOID list_item;
	ULONG_PTR count;

	if (!list_node->count)
		return;

	count = list_node->count;
	list_node->count = 0;

	if (list_node->cleanup_callback)
	{
		for (ULONG_PTR i = 0; i < count; i++)
		{
			list_item = list_node->items[i];

			if (list_item)
				list_node->cleanup_callback (list_item);
		}
	}

	RtlSecureZeroMemory (list_node->items, count * sizeof (PVOID));
}

_Success_ (return != SIZE_MAX)
ULONG_PTR _r_obj_findlistitem (
	_In_ PR_LIST list_node,
	_In_opt_ LPCVOID list_item
)
{
	for (ULONG_PTR i = 0; i < list_node->count; i++)
	{
		if (list_node->items[i] == list_item)
			return i;
	}

	return SIZE_MAX;
}

_Ret_maybenull_
PVOID _r_obj_getlistitem (
	_In_ PR_LIST list_node,
	_In_ ULONG_PTR index
)
{
	return list_node->items[index];
}

ULONG_PTR _r_obj_getlistsize (
	_In_ PR_LIST list_node
)
{
	return list_node->count;
}

VOID _r_obj_insertlistitems (
	_Inout_ PR_LIST list_node,
	_In_ ULONG_PTR start_pos,
	_In_ PVOID_PTR list_items,
	_In_ ULONG_PTR count
)
{
	ULONG_PTR new_count;

	if (list_node->allocated_count < list_node->count + count)
	{
		new_count = list_node->allocated_count * 2;

		if (new_count < list_node->count + count)
			new_count = list_node->count + count;

		_r_obj_resizelist (list_node, new_count);
	}

	if (start_pos < list_node->count)
		RtlMoveMemory (&list_node->items[start_pos + count], &list_node->items[start_pos], (list_node->count - start_pos) * sizeof (PVOID));

	RtlCopyMemory (&list_node->items[start_pos], list_items, count * sizeof (PVOID));

	list_node->count += count;
}

VOID _r_obj_removelistitem (
	_Inout_ PR_LIST list_node,
	_In_ ULONG_PTR index
)
{
	_r_obj_removelistitems (list_node, index, 1);
}

VOID _r_obj_removelistitems (
	_Inout_ PR_LIST list_node,
	_In_ ULONG_PTR start_pos,
	_In_ ULONG_PTR count
)
{
	PVOID list_item;

	for (ULONG_PTR i = start_pos; i < (start_pos + count); i++)
	{
		list_item = list_node->items[i];

		list_node->items[i] = NULL;

		if (list_node->cleanup_callback)
		{
			if (list_item)
				list_node->cleanup_callback (list_item);
		}
	}

	RtlMoveMemory (&list_node->items[start_pos], &list_node->items[start_pos + count], (list_node->count - start_pos - count) * sizeof (PVOID));

	list_node->count -= count;
}

VOID _r_obj_resizelist (
	_Inout_ PR_LIST list_node,
	_In_ ULONG_PTR new_capacity
)
{
	if (list_node->count > new_capacity)
		RtlRaiseStatus (STATUS_INVALID_PARAMETER_2);

	list_node->allocated_count = new_capacity;

	list_node->items = _r_mem_reallocate (list_node->items, list_node->allocated_count * sizeof (PVOID));
}

VOID _r_obj_setlistitem (
	_Inout_ PR_LIST list_node,
	_In_ ULONG_PTR index,
	_In_opt_ PVOID list_item
)
{
	PVOID prev_list_item;

	prev_list_item = list_node->items[index];
	list_node->items[index] = list_item;

	if (!list_node->cleanup_callback)
		return;

	if (prev_list_item)
		list_node->cleanup_callback (prev_list_item);
}

//
// Hashtable object
//

// A hashtable with power-of-two bucket sizes and with all entries stored in a single
// array. This improves locality but may be inefficient when resizing the hashtable. It is a good
// idea to store pointers to objects as entries, as opposed to the objects themselves.

PR_HASHTABLE _r_obj_createhashtable (
	_In_ ULONG_PTR entry_size,
	_In_ ULONG_PTR initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
)
{
	PR_HASHTABLE hashtable;

	hashtable = _r_obj_allocate (sizeof (R_HASHTABLE), &_r_util_cleanhashtable_callback);

	// Initial capacity of 0 is not allowed.
	if (initial_capacity == 0)
		initial_capacity = 1;

	hashtable->entry_size = entry_size;
	hashtable->cleanup_callback = cleanup_callback;

	// Allocate the buckets.
	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (initial_capacity);
	hashtable->buckets = _r_mem_allocate (hashtable->allocated_buckets * sizeof (ULONG_PTR));

	// Set all bucket values to -1.
	RtlFillMemory (hashtable->buckets, hashtable->allocated_buckets * sizeof (ULONG_PTR), PR_HASHTABLE_INIT_VALUE);

	// Allocate the entries.
	hashtable->allocated_entries = hashtable->allocated_buckets;
	hashtable->entries = _r_mem_allocate (hashtable->allocated_entries * PR_HASHTABLE_ENTRY_SIZE (entry_size));

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

	return hashtable;
}

FORCEINLINE PVOID _r_obj_addhashtableitem_ex (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry,
	_In_ BOOLEAN is_checkduplicates,
	_Out_opt_ PBOOLEAN is_added_ptr
)
{
	PR_HASHTABLE_ENTRY hashtable_entry = NULL;
	ULONG_PTR free_entry;
	ULONG_PTR index;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);

	if (is_checkduplicates)
	{
		for (ULONG_PTR i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
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

VOID _r_obj_clearhashtable (
	_Inout_ PR_HASHTABLE hashtable
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	ULONG_PTR next_entry;
	ULONG_PTR index = 0;

	if (!hashtable->count)
		return;

	next_entry = hashtable->next_entry;

	RtlFillMemory (hashtable->buckets, hashtable->allocated_buckets * sizeof (ULONG_PTR), PR_HASHTABLE_INIT_VALUE);

	hashtable->count = 0;
	hashtable->free_entry = SIZE_MAX;
	hashtable->next_entry = 0;

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
	_Out_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PULONG_PTR enum_key
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
	PR_HASHTABLE_ENTRY hashtable_entry = NULL;
	ULONG_PTR index;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);

	for (ULONG_PTR i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, i);

		if (_r_obj_validatehash (hashtable_entry->hash_code) == valid_hash)
			return &hashtable_entry->body;
	}

	return NULL;
}

ULONG_PTR _r_obj_gethashtablesize (
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
	PR_HASHTABLE_ENTRY hashtable_entry = NULL;
	ULONG_PTR prev_index = SIZE_MAX;
	ULONG_PTR index;
	ULONG valid_hash;

	valid_hash = _r_obj_validatehash (hash_code);
	index = _r_obj_indexfromhash (hashtable, valid_hash);

	for (ULONG_PTR i = hashtable->buckets[index]; i != SIZE_MAX; i = hashtable_entry->next)
	{
		hashtable_entry = PR_HASHTABLE_GET_ENTRY (hashtable, i);

		if (_r_obj_validatehash (hashtable_entry->hash_code) == valid_hash)
		{
			// Unlink the entry from the bucket.
			if (prev_index == SIZE_MAX)
			{
				hashtable->buckets[index] = hashtable_entry->next;
			}
			else
			{
				PR_HASHTABLE_GET_ENTRY (hashtable, prev_index)->next = hashtable_entry->next;
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

		prev_index = i;
	}

	return FALSE;
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

VOID _r_obj_resizehashtable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR new_capacity
)
{
	PR_HASHTABLE_ENTRY hashtable_entry;
	ULONG_PTR index;

	// Re-allocate the buckets. Note that we don't need to keep the contents.
	hashtable->allocated_buckets = _r_math_rounduptopoweroftwo (new_capacity);

	hashtable->buckets = _r_mem_reallocate (hashtable->buckets, hashtable->allocated_buckets * sizeof (ULONG_PTR));

	// Set all bucket values to -1.
	RtlFillMemory (hashtable->buckets, hashtable->allocated_buckets * sizeof (ULONG_PTR), PR_HASHTABLE_INIT_VALUE);

	// Re-allocate the entries.
	hashtable->allocated_entries = hashtable->allocated_buckets;

	hashtable->entries = _r_mem_reallocate (hashtable->entries, PR_HASHTABLE_ENTRY_SIZE (hashtable->entry_size) * hashtable->allocated_entries);

	// Re-distribute the entries among the buckets.

	// PR_HASHTABLE_GET_ENTRY is quite slow (it involves a multiply), so we use a pointer here.
	hashtable_entry = hashtable->entries;

	for (ULONG_PTR i = 0; i < hashtable->next_entry; i++)
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
	_In_ ULONG_PTR initial_capacity
)
{
	PR_HASHTABLE hashtable;

	hashtable = _r_obj_createhashtable (sizeof (R_OBJECT_POINTER), initial_capacity, &_r_util_cleanhashtablepointer_callback);

	return hashtable;
}

_Ret_maybenull_
PR_OBJECT_POINTER _r_obj_addhashtablepointer (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ PVOID value
)
{
	PR_OBJECT_POINTER hashtable_entry;
	R_OBJECT_POINTER object_ptr = {0};

	object_ptr.object_body = value;

	hashtable_entry = _r_obj_addhashtableitem (hashtable, hash_code, &object_ptr);

	return hashtable_entry;
}

_Success_ (return)
BOOLEAN _r_obj_enumhashtablepointer (
	_In_ PR_HASHTABLE hashtable,
	_Out_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PULONG_PTR enum_key
)
{
	PR_OBJECT_POINTER object_ptr = NULL;
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

//
// System messages
//

HRESULT CALLBACK _r_msg_callback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam,
	_In_opt_ LONG_PTR lpdata
)
{
	UNREFERENCED_PARAMETER (wparam);
	UNREFERENCED_PARAMETER (lparam);

	switch (msg)
	{
		case TDN_CREATED:
		{
			HWND hparent;

			// center window
			hparent = GetParent (hwnd);

			_r_wnd_center (hwnd, hparent);

			// set on top (always)
			_r_wnd_top (hwnd, TRUE);

			// don't round corners
			if (_r_sys_isosversiongreaterorequal (WINDOWS_11))
				DwmSetWindowAttribute (hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &(DWM_WINDOW_CORNER_PREFERENCE){ DWMWCP_DONOTROUND }, sizeof (DWM_WINDOW_CORNER_PREFERENCE));

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			// remove window icon
			_r_wnd_seticon (hwnd, NULL, NULL);

			//if (_r_theme_isenabled ())
			//	_r_theme_initializetaskdialogtheme (hwnd, 0);

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
// Clipboard
//

_Ret_maybenull_
PR_STRING _r_clipboard_get (
	_In_opt_ HWND hwnd
)
{
	PR_STRING string = NULL;
	HANDLE hdata;
	PVOID base_address;
	ULONG_PTR length;

	if (!IsClipboardFormatAvailable (CF_UNICODETEXT))
		return NULL;

	if (!OpenClipboard (hwnd))
		return NULL;

	hdata = GetClipboardData (CF_UNICODETEXT);

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
	BOOLEAN is_success = FALSE;

	if (!OpenClipboard (hwnd))
		return FALSE;

	hdata = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, string->length + sizeof (UNICODE_NULL));

	if (hdata)
	{
		base_address = GlobalLock (hdata);

		if (base_address)
		{
			RtlCopyMemory (base_address, string->buffer, string->length);

			*(LPWSTR)PTR_ADD_OFFSET (base_address, string->length) = UNICODE_NULL; // terminate

			GlobalUnlock (base_address);

			is_success = !!EmptyClipboard ();

			if (!is_success)
				goto CleanupExit;

			is_success = !!SetClipboardData (CF_UNICODETEXT, hdata);
		}
	}

CleanupExit:

	if (!is_success)
		GlobalFree (hdata);

	CloseClipboard ();

	return is_success;
}

//
// Filesystem
//

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_clearfile (
	_In_ HANDLE hfile
)
{
	LARGE_INTEGER size = {0};

	_r_fs_setsize (hfile, &size);
	_r_fs_setpos (hfile, &size);

	return _r_fs_flushfile (hfile);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_createdirectory (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF separator = PR_STRINGREF_INIT (L"\\");
	R_STRINGREF directory_part;
	R_STRINGREF remaining_part;
	R_STRINGREF basename_part;
	PR_STRING directory_path = NULL;
	PR_STRING directory_name;
	HANDLE hfile;
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	if (_r_fs_isdirectory (path))
		return STATUS_SUCCESS;

	_r_obj_initializestringref2 (&remaining_part, path);

	while (remaining_part.length != 0)
	{
		_r_str_splitatlastchar (&remaining_part, OBJ_NAME_PATH_SEPARATOR, &directory_part, &basename_part);

		if (directory_part.length != 0)
		{
			if (_r_fs_isdirectory (&directory_part))
			{
				directory_path = _r_obj_createstring2 (&directory_part);

				break;
			}
		}

		remaining_part = directory_part;
	}

	if (_r_obj_isstringempty (directory_path))
		return STATUS_UNSUCCESSFUL;

	_r_obj_initializestringref_ex (
		&remaining_part,
		PTR_ADD_OFFSET (path->buffer, directory_path->length + sizeof (OBJ_NAME_PATH_SEPARATOR)),
		path->length - directory_path->length - sizeof (OBJ_NAME_PATH_SEPARATOR)
	);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, OBJ_NAME_PATH_SEPARATOR, &directory_part, &remaining_part);

		if (directory_part.length != 0)
		{
			if (_r_obj_isstringempty (directory_path))
			{
				_r_obj_movereference (&directory_path, _r_obj_createstring2 (&directory_part));
			}
			else
			{
				directory_name = _r_obj_concatstringrefs (
					3,
					&directory_path->sr,
					&separator,
					&directory_part
				);

				if (!_r_fs_isdirectory (&directory_name->sr))
				{
					status = _r_fs_createfile (
						&directory_name->sr,
						FILE_CREATE,
						FILE_LIST_DIRECTORY,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						FILE_ATTRIBUTE_NORMAL,
						FILE_OPEN_FOR_BACKUP_INTENT,
						TRUE,
						NULL,
						&hfile
					);

					if (NT_SUCCESS (status))
						NtClose (hfile);
				}

				_r_obj_movereference (&directory_path, directory_name);
			}
		}
	}

	_r_obj_dereference (directory_path);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_createfile (
	_In_ PR_STRINGREF path,
	_In_ ULONG create_disposition,
	_In_ ACCESS_MASK desired_access,
	_In_opt_ ULONG share_access,
	_In_opt_ ULONG file_attributes,
	_In_opt_ ULONG create_option,
	_In_ BOOLEAN is_directory,
	_In_opt_ PLARGE_INTEGER allocation_size,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	IO_STATUS_BLOCK isb;
	UNICODE_STRING nt_path;
	HANDLE hfile;
	NTSTATUS status;

	status = _r_fs_dospathnametontpathname (path, &nt_path);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	InitializeObjectAttributes (&oa, &nt_path, OBJ_CASE_INSENSITIVE, NULL, NULL);

	create_option |= is_directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;

	status = NtCreateFile (
		&hfile,
		desired_access | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&oa,
		&isb,
		allocation_size,
		file_attributes ? file_attributes : FILE_ATTRIBUTE_NORMAL,
		share_access,
		create_disposition,
		FILE_SYNCHRONOUS_IO_NONALERT | create_option,
		NULL,
		0
	);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hfile;
	}
	else
	{
		*out_buffer = NULL;
	}

	RtlFreeUnicodeString (&nt_path);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_copyfile (
	_In_ PR_STRINGREF path_from,
	_In_ PR_STRINGREF path_to,
	_In_ BOOLEAN is_failifexists
)
{
	FILETIME creation_time;
	FILETIME access_time;
	FILETIME write_time;
	HANDLE hfile_src;
	HANDLE hfile_dst;
	PBYTE buffer;
	ULONG_PTR readed;
	ULONG attributes;
	ULONG length;
	NTSTATUS status;

	status = _r_fs_getattributes (path_from, &attributes);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_openfile (path_from, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, FALSE, &hfile_src);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_gettimestamp (hfile_src, &creation_time, &access_time, &write_time);

	if (!NT_SUCCESS (status))
	{
		NtClose (hfile_src);

		return status;
	}

	status = _r_fs_createfile (
		path_to,
		is_failifexists ? FILE_CREATE : FILE_OVERWRITE_IF,
		FILE_GENERIC_WRITE,
		FILE_SHARE_READ,
		attributes,
		FILE_SEQUENTIAL_ONLY,
		FALSE,
		NULL,
		&hfile_dst
	);

	if (!NT_SUCCESS (status))
	{
		NtClose (hfile_src);

		return status;
	}

	length = PR_SIZE_BUFFER;
	buffer = _r_mem_allocate (length);

	while (TRUE)
	{
		status = _r_fs_readfile (hfile_src, buffer, length, &readed);

		if (!NT_SUCCESS (status) || readed == 0)
			break;

		status = _r_fs_writefile (hfile_dst, buffer, (ULONG)readed);

		if (!NT_SUCCESS (status))
			break;
	}

	if (status == STATUS_END_OF_FILE)
		status = STATUS_SUCCESS;

	if (NT_SUCCESS (status))
	{
		_r_fs_settimestamp (hfile_dst, &creation_time, &access_time, &write_time);
	}
	else
	{
		_r_fs_deletefile (NULL, hfile_dst);
	}

	_r_mem_free (buffer);

	NtClose (hfile_src);
	NtClose (hfile_dst);

	return status;
}

BOOLEAN NTAPI _r_fs_recursivedirectorydelete_callback (
	_In_ PR_STRING path,
	_In_ ULONG attributes,
	_In_ PLARGE_INTEGER creation_time,
	_In_ PLARGE_INTEGER lastwrite_time,
	_In_opt_ PVOID context
)
{
	UNREFERENCED_PARAMETER (creation_time);
	UNREFERENCED_PARAMETER (lastwrite_time);
	UNREFERENCED_PARAMETER (context);

	if (attributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		_r_fs_deletedirectory (&path->sr, TRUE);
	}
	else
	{
		if (attributes & FILE_ATTRIBUTE_READONLY && _r_sys_isosversionlower (WINDOWS_10_RS5))
			_r_fs_setattributes (NULL, &path->sr, attributes & ~FILE_ATTRIBUTE_READONLY);

		_r_fs_deletefile (&path->sr, NULL);
	}

	return TRUE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_deletedirectory (
	_In_ PR_STRINGREF path,
	_In_ BOOLEAN is_recurse
)
{
	FILE_DISPOSITION_INFORMATION fdi = {0};
	FILE_DISPOSITION_INFO_EX fdi_ex = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hdirectory;
	NTSTATUS status;

	status = _r_fs_createfile (
		path,
		FILE_OPEN,
		FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY | DELETE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_ATTRIBUTE_NORMAL,
		FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT,
		TRUE,
		NULL,
		&hdirectory
	);

	if (!NT_SUCCESS (status))
	{
		if (status == STATUS_OBJECT_NAME_NOT_FOUND)
			status = STATUS_SUCCESS;

		return status;
	}

	_r_fs_setattributes (hdirectory, NULL, FILE_ATTRIBUTE_NORMAL);

	if (is_recurse)
		_r_fs_enumfiles (path, hdirectory, NULL, &_r_fs_recursivedirectorydelete_callback, NULL);

	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
	{
		fdi_ex.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;

		status = NtSetInformationFile (hdirectory, &isb, &fdi_ex, sizeof (fdi_ex), FileDispositionInformationEx);
	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
	}

	if (!NT_SUCCESS (status))
	{
		fdi.FileDelete = TRUE;

		status = NtSetInformationFile (hdirectory, &isb, &fdi, sizeof (fdi), FileDispositionInformation);
	}

	NtClose (hdirectory);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_deletefile (
	_In_opt_ PR_STRINGREF path,
	_In_opt_ HANDLE hfile
)
{
	FILE_DISPOSITION_INFORMATION fdi = {0};
	FILE_DISPOSITION_INFO_EX fdi_ex = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hfile_new = NULL;
	NTSTATUS status;

	if (!path && !hfile)
		return STATUS_INVALID_PARAMETER;

	if (path && !hfile)
	{
		status = _r_fs_openfile (path, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, FALSE, &hfile_new);

		if (!NT_SUCCESS (status))
		{
			if (status == STATUS_OBJECT_NAME_NOT_FOUND)
				status = STATUS_SUCCESS;

			return status;
		}

		hfile = hfile_new;
	}

	_r_fs_setattributes (hfile, NULL, FILE_ATTRIBUTE_NORMAL);

	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
	{
		fdi_ex.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;

		status = NtSetInformationFile (hfile, &isb, &fdi_ex, sizeof (fdi_ex), FileDispositionInformationEx);
	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
	}

	if (!NT_SUCCESS (status))
	{
		fdi.FileDelete = TRUE;

		status = NtSetInformationFile (hfile, &isb, &fdi, sizeof (fdi), FileDispositionInformation);
	}

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_fs_deleterecycle (
	_In_ PR_STRINGREF path
)
{
	IFileOperation* file_ops = NULL;
	IShellItem* shell_item = NULL;
	PR_STRING string = NULL;
	PR_STRINGREF sr;
	HRESULT status;

	if (!_r_fs_exists (path))
		return E_NOT_SET;

	// vista+
	status = CoCreateInstance (&CLSID_FileOperation, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOperation, &file_ops);

	if (FAILED (status))
		return status;

	if (_r_str_isnullterminated (path))
	{
		sr = path;
	}
	else
	{
		string = _r_obj_createstring2 (path);

		sr = &string->sr;
	}

	status = SHCreateItemFromParsingName (sr->buffer, NULL, &IID_IShellItem, &shell_item);

	if (FAILED (status))
		goto CleanupExit;

	IFileOperation_SetOperationFlags (file_ops, FOF_ALLOWUNDO | FOF_NO_UI);

	IFileOperation_DeleteItem (file_ops, shell_item, NULL);

	status = IFileOperation_PerformOperations (file_ops);

CleanupExit:

	if (file_ops)
		IFileOperation_Release (file_ops);

	if (shell_item)
		IShellItem_Release (shell_item);

	if (string)
		_r_obj_dereference (string);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_deviceiocontrol (
	_In_ HANDLE hdevice,
	_In_ ULONG ioctrl,
	_In_reads_bytes_opt_ (in_length) PVOID in_buffer,
	_In_ ULONG in_length,
	_Out_writes_bytes_opt_ (out_length) PVOID out_buffer,
	_In_ ULONG out_length,
	_Out_opt_ PULONG_PTR return_length
)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	if (DEVICE_TYPE_FROM_CTL_CODE (ioctrl) == FILE_DEVICE_FILE_SYSTEM)
	{
		status = NtFsControlFile (hdevice, NULL, NULL, NULL, &isb, ioctrl, in_buffer, in_length, out_buffer, out_length);
	}
	else
	{
		status = NtDeviceIoControlFile (hdevice, NULL, NULL, NULL, &isb, ioctrl, in_buffer, in_length, out_buffer, out_length);
	}

	if (status == STATUS_PENDING)
	{
		status = _r_sys_waitforsingleobject (hdevice, INFINITE);

		if (NT_SUCCESS (status))
			status = isb.Status;
	}

	if (return_length)
		*return_length = isb.Information;

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_dospathnametontpathname (
	_In_ PR_STRINGREF path,
	_Out_ PUNICODE_STRING out_buffer
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static RDLPN2NPN _RtlDosLongPathNameToNtPathName_I_WithStatus = NULL;

	PR_STRING string = NULL;
	PR_STRINGREF sr;
	PVOID hntdll;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		// win10rs1+
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS1) && NtCurrentPeb ()->IsLongPathAwareProcess) // RtlAreLongPathsEnabled ()
		{
			status = _r_sys_loadlibrary2 (L"ntdll.dll", 0, &hntdll);

			if (NT_SUCCESS (status))
			{
				_RtlDosLongPathNameToNtPathName_I_WithStatus = (RDLPN2NPN)_r_sys_getprocaddress (hntdll, "RtlDosLongPathNameToNtPathName_U_WithStatus", 0);

				_r_sys_freelibrary (hntdll);
			}
		}

		_r_initonce_end (&init_once);
	}

	if (_r_str_isnullterminated (path))
	{
		sr = path;
	}
	else
	{
		string = _r_obj_createstring2 (path);

		sr = &string->sr;
	}

	if (_RtlDosLongPathNameToNtPathName_I_WithStatus)
	{
		status = _RtlDosLongPathNameToNtPathName_I_WithStatus (sr->buffer, out_buffer, NULL, NULL);
	}
	else
	{
		status = RtlDosPathNameToNtPathName_U_WithStatus (sr->buffer, out_buffer, NULL, NULL);
	}

	if (string)
		_r_obj_dereference (string);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_enumfiles (
	_In_ PR_STRINGREF path,
	_In_opt_ HANDLE hdirectory,
	_In_opt_ LPWSTR search_pattern,
	_In_ PR_FILE_ENUM_CALLBACK enum_callback,
	_In_opt_ PVOID context
)
{
	R_STRINGREF separator = PR_STRINGREF_INIT (L"\\");
	PFILE_DIRECTORY_INFORMATION directory_info;
	UNICODE_STRING pattern = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hdirectory_new = NULL;
	PR_STRING file_name;
	PR_STRING path_full;
	PVOID buffer;
	ULONG buffer_length = 0x400;
	ULONG i;
	BOOLEAN is_firsttime = TRUE;
	BOOLEAN is_break = FALSE;
	NTSTATUS status;

	if (!hdirectory)
	{
		status = _r_fs_openfile (
			path,
			GENERIC_READ | FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			0,
			TRUE,
			&hdirectory_new
		);

		if (!NT_SUCCESS (status))
			return status;

		hdirectory = hdirectory_new;
	}

	if (search_pattern)
		_r_obj_initializeunicodestring (&pattern, search_pattern);

	buffer = _r_mem_allocate (buffer_length);

	while (TRUE)
	{
		// Query the directory, doubling the buffer each time NtQueryDirectoryFile fails.
		while (TRUE)
		{
			status = NtQueryDirectoryFile (hdirectory, NULL, NULL, NULL, &isb, buffer, buffer_length, FileDirectoryInformation, FALSE, &pattern, is_firsttime);

			// Our ISB is on the stack, so we have to wait for the operation to complete before continuing.
			if (status == STATUS_PENDING)
			{
				status = _r_sys_waitforsingleobject (hdirectory, INFINITE);

				if (NT_SUCCESS (status))
					status = isb.Status;
			}

			if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH)
			{
				buffer_length *= 2;

				buffer = _r_mem_reallocate (buffer, buffer_length);
			}
			else
			{
				break;
			}
		}

		// If we don't have any entries to read, exit.
		if (status == STATUS_NO_MORE_FILES)
		{
			status = STATUS_SUCCESS;

			break;
		}

		if (!NT_SUCCESS (status))
			break;

		i = 0;

		// Read the batch and execute the callback function for each file.
		while (TRUE)
		{
			directory_info = PTR_ADD_OFFSET (buffer, i);

			if (directory_info->FileNameLength && !_r_str_isempty2 (directory_info->FileName))
			{
				if (_r_str_compare (directory_info->FileName, L".", TRUE) != 0 && _r_str_compare (directory_info->FileName, L"..", TRUE) != 0)
				{
					file_name = _r_obj_createstring_ex (directory_info->FileName, directory_info->FileNameLength);

					path_full = _r_obj_concatstringrefs (
						3,
						path,
						&separator,
						&file_name->sr
					);

					if (!enum_callback (path_full, directory_info->FileAttributes, &directory_info->CreationTime, &directory_info->LastWriteTime, context))
						is_break = TRUE;

					_r_obj_dereference (path_full);
					_r_obj_dereference (file_name);
				}
			}

			if (is_break)
				break;

			if (directory_info->NextEntryOffset != 0)
			{
				i += directory_info->NextEntryOffset;
			}
			else
			{
				break;
			}
		}

		is_firsttime = FALSE;
	}

	if (hdirectory_new)
		NtClose (hdirectory_new);

	_r_mem_free (buffer);

	return status;
}

_Success_ (return)
BOOLEAN _r_fs_exists (
	_In_ PR_STRINGREF path
)
{
	ULONG attributes;
	NTSTATUS status;

	status = _r_fs_getattributes (path, &attributes);

	if (NT_SUCCESS (status) || status == STATUS_SHARING_VIOLATION || status == STATUS_ACCESS_DENIED)
		return TRUE;

	return FALSE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_flushfile (
	_In_ HANDLE hfile
)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	// win8+
	//if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	//{
	//	status = NtFlushBuffersFileEx (hfile, 0, 0, 0, &isb);
	//}
	//else
	{
		status = NtFlushBuffersFile (hfile, &isb);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getattributes (
	_In_ PR_STRINGREF path,
	_Out_ PULONG out_buffer
)
{
	FILE_BASIC_INFORMATION info = {0};
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING nt_path;
	NTSTATUS status;

	status = _r_fs_dospathnametontpathname (path, &nt_path);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = 0;

		return status;
	}

	InitializeObjectAttributes (&oa, &nt_path, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtQueryAttributesFile (&oa, &info);

	if (NT_SUCCESS (status))
	{
		*out_buffer = info.FileAttributes;
	}
	else
	{
		*out_buffer = 0;
	}

	RtlFreeUnicodeString (&nt_path);

	return status;
}

PR_STRING _r_fs_getcurrentdirectory ()
{
	PR_STRING string;
	ULONG_PTR length;

	// Lock the PEB to get the current directory
	RtlAcquirePebLock ();

	string = _r_obj_createstring3 (&NtCurrentPeb ()->ProcessParameters->CurrentDirectory.DosPath);

	RtlReleasePebLock ();

	length = string->length / sizeof (WCHAR);

	// Again check for our two cases (drive root vs path)
	if ((length <= 1) || (string->buffer[length - 2] != L':'))
	{
		// Replace the trailing slash with a null
		string->buffer[length - 1] = UNICODE_NULL;
		string->length -= sizeof (UNICODE_NULL);
	}
	else
	{
		// Append the null char since there's no trailing slash
		string->buffer[length] = UNICODE_NULL;
	}

	return string;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getdiskinformation (
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_STRINGREF path,
	_Out_opt_ PR_STRING_PTR label_ptr,
	_Out_opt_ PR_STRING_PTR filesystem_ptr,
	_Out_opt_ PULONG flags_ptr,
	_Out_opt_ PULONG serialnumber_ptr
)
{
	PFILE_FS_ATTRIBUTE_INFORMATION attribute_info;
	PFILE_FS_VOLUME_INFORMATION volume_info;
	IO_STATUS_BLOCK isb;
	HANDLE hfile_new = NULL;
	ULONG length;
	NTSTATUS status;

	if (!hfile && !path)
		return STATUS_INVALID_PARAMETER;

	if (!hfile && path)
	{
		status = _r_fs_openfile (path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, TRUE, &hfile_new);

		if (!NT_SUCCESS (status))
			return status;

		hfile = hfile_new;
	}

	length = sizeof (FILE_FS_VOLUME_INFORMATION) + 512 * sizeof (WCHAR);
	volume_info = _r_mem_allocate (length);

	status = NtQueryVolumeInformationFile (hfile, &isb, volume_info, length, FileFsVolumeInformation);

	if (NT_SUCCESS (status))
	{
		if (label_ptr)
			*label_ptr = _r_obj_createstring_ex (volume_info->VolumeLabel, volume_info->VolumeLabelLength * sizeof (WCHAR));

		if (serialnumber_ptr)
			*serialnumber_ptr = volume_info->VolumeSerialNumber;
	}
	else
	{
		if (label_ptr)
			*label_ptr = NULL;

		if (serialnumber_ptr)
			*serialnumber_ptr = 0;
	}

	length = sizeof (FILE_FS_ATTRIBUTE_INFORMATION) + 512 * sizeof (WCHAR);
	attribute_info = _r_mem_reallocate (volume_info, length);

	status = NtQueryVolumeInformationFile (hfile, &isb, attribute_info, length, FileFsAttributeInformation);

	if (NT_SUCCESS (status))
	{
		if (filesystem_ptr)
			*filesystem_ptr = _r_obj_createstring_ex (attribute_info->FileSystemName, attribute_info->FileSystemNameLength * sizeof (WCHAR));

		if (flags_ptr)
			*flags_ptr = attribute_info->FileSystemAttributes;
	}
	else
	{
		if (filesystem_ptr)
			*filesystem_ptr = NULL;

		if (flags_ptr)
			*flags_ptr = 0;
	}

	_r_mem_free (attribute_info);

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getdisklist (
	_Out_ PULONG out_buffer
)
{
#if defined(_WIN64)
	PROCESS_DEVICEMAP_INFORMATION_EX device_map = {0};
#else
	PROCESS_DEVICEMAP_INFORMATION device_map = {0};
#endif // _WIN64

	NTSTATUS status;

	// Get the Device Map for this Process
	status = NtQueryInformationProcess (NtCurrentProcess (), ProcessDeviceMap, &device_map, sizeof (device_map), NULL);

	if (NT_SUCCESS (status))
	{
		*out_buffer = device_map.Query.DriveMap;
	}
	else
	{
		*out_buffer = 0;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getdiskspace (
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_STRINGREF path,
	_Out_ PLARGE_INTEGER freespace_ptr,
	_Out_ PLARGE_INTEGER totalspace_ptr
)
{
	FILE_FS_SIZE_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hfile_new = NULL;
	ULONG units;
	NTSTATUS status;

	if (!hfile && !path)
		return STATUS_INVALID_PARAMETER;

	if (!hfile && path)
	{
		status = _r_fs_openfile (path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, TRUE, &hfile_new);

		if (!NT_SUCCESS (status))
			return status;

		hfile = hfile_new;
	}

	status = NtQueryVolumeInformationFile (hfile, &isb, &info, sizeof (info), FileFsSizeInformation);

	if (NT_SUCCESS (status))
	{
		RtlCopyMemory (freespace_ptr, &info.AvailableAllocationUnits, sizeof (LARGE_INTEGER));
		RtlCopyMemory (totalspace_ptr, &info.TotalAllocationUnits, sizeof (LARGE_INTEGER));

		units = info.SectorsPerAllocationUnit * info.BytesPerSector;

		freespace_ptr->QuadPart = info.AvailableAllocationUnits.QuadPart * units;
		totalspace_ptr->QuadPart = info.TotalAllocationUnits.QuadPart * units;
	}
	else
	{
		RtlZeroMemory (freespace_ptr, sizeof (LARGE_INTEGER));
		RtlZeroMemory (totalspace_ptr, sizeof (LARGE_INTEGER));
	}

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getobjectname (
	_In_ HANDLE hfile,
	_In_ BOOLEAN is_ntpathtodos,
	_Out_ PR_STRING_PTR out_buffer
)
{
	POBJECT_NAME_INFORMATION buffer;
	PR_STRING string;
	ULONG buffer_length;
	ULONG attempts = 6;
	NTSTATUS status;

	buffer_length = sizeof (OBJECT_NAME_INFORMATION) + (256 * sizeof (WCHAR));
	buffer = _r_mem_allocate (buffer_length);

	do
	{
		status = NtQueryObject (hfile, ObjectNameInformation, buffer, buffer_length, &buffer_length);

		if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL)
		{
			buffer = _r_mem_reallocate (buffer, buffer_length);
		}
		else
		{
			break;
		}
	}
	while (--attempts);

	if (NT_SUCCESS (status))
	{
		string = _r_obj_createstring3 (&buffer->Name);

		if (is_ntpathtodos)
		{
			*out_buffer = _r_path_dospathfromnt (&string->sr);

			_r_obj_dereference (string);
		}
		else
		{
			*out_buffer = string;
		}
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (buffer);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getpos (
	_In_ HANDLE hfile,
	_Out_ PLONG64 out_buffer
)
{
	FILE_POSITION_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	status = NtQueryInformationFile (hfile, &isb, &info, sizeof (info), FilePositionInformation);

	if (NT_SUCCESS (status))
	{
		*out_buffer = info.CurrentByteOffset.QuadPart;
	}
	else
	{
		*out_buffer = 0;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getsecurityinfo (
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_STRINGREF path,
	_Out_ PSECURITY_DESCRIPTOR_PTR out_sd,
	_Out_opt_ PACL_PTR out_dacl
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PACL dacl = NULL;
	HANDLE hfile_new = NULL;
	ULONG buffer_length = 0x100;
	ULONG required_length;
	BOOLEAN is_defaulted;
	BOOLEAN is_present = FALSE;
	NTSTATUS status;

	if (!hfile && !path)
		return STATUS_INVALID_PARAMETER;

	if (!hfile && path)
	{
		status = _r_fs_openfile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, FALSE, &hfile_new);

		if (!NT_SUCCESS (status))
			return status;

		hfile = hfile_new;
	}

	security_descriptor = _r_mem_allocate (buffer_length);

	status = NtQuerySecurityObject (hfile, DACL_SECURITY_INFORMATION, security_descriptor, buffer_length, &required_length);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		buffer_length = required_length;
		security_descriptor = _r_mem_reallocate (security_descriptor, buffer_length);

		status = NtQuerySecurityObject (hfile, DACL_SECURITY_INFORMATION, security_descriptor, buffer_length, &buffer_length);
	}

	if (NT_SUCCESS (status) && out_dacl)
		status = RtlGetDaclSecurityDescriptor (security_descriptor, &is_present, &dacl, &is_defaulted);

	if (NT_SUCCESS (status))
	{
		*out_sd = security_descriptor;

		if (out_dacl)
			*out_dacl = dacl;
	}
	else
	{
		*out_sd = NULL;

		if (out_dacl)
			*out_dacl = NULL;

		_r_mem_free (security_descriptor);
	}

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getsize (
	_In_opt_ PR_STRINGREF path,
	_In_opt_ HANDLE hfile,
	_Out_ PLARGE_INTEGER out_buffer
)
{
	FILE_STANDARD_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hfile_new = NULL;
	NTSTATUS status;

	RtlZeroMemory (out_buffer, sizeof (LARGE_INTEGER));

	if (!hfile && !path)
		return STATUS_INVALID_PARAMETER_MIX;

	if (!hfile && path)
	{
		status = _r_fs_openfile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, FALSE, &hfile_new);

		if (!NT_SUCCESS (status))
			return status;

		hfile = hfile_new;
	}

	status = NtQueryInformationFile (hfile, &isb, &info, sizeof (info), FileStandardInformation);

	if (NT_SUCCESS (status))
		RtlCopyMemory (out_buffer, &info.EndOfFile, sizeof (LARGE_INTEGER));

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_getsize2 (
	_In_opt_ PR_STRINGREF path,
	_In_opt_ HANDLE hfile,
	_Out_ PLONG64 out_buffer
)
{
	LARGE_INTEGER li;
	NTSTATUS status;

	status = _r_fs_getsize (path, hfile, &li);

	if (NT_SUCCESS (status))
	{
		*out_buffer = li.QuadPart;
	}
	else
	{
		*out_buffer = 0;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_gettimestamp (
	_In_ HANDLE hfile,
	_Out_opt_ PFILETIME creation_time,
	_Out_opt_ PFILETIME access_time,
	_Out_opt_ PFILETIME write_time
)
{
	FILE_BASIC_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	status = NtQueryInformationFile (hfile, &isb, &info, sizeof (info), FileBasicInformation);

	if (NT_SUCCESS (status))
	{
		if (creation_time)
		{
			creation_time->dwHighDateTime = info.CreationTime.HighPart;
			creation_time->dwLowDateTime = info.CreationTime.LowPart;
		}

		if (access_time)
		{
			access_time->dwHighDateTime = info.LastAccessTime.HighPart;
			access_time->dwLowDateTime = info.LastAccessTime.LowPart;
		}

		if (write_time)
		{
			write_time->dwHighDateTime = info.LastWriteTime.HighPart;
			write_time->dwLowDateTime = info.LastWriteTime.LowPart;
		}
	}
	else
	{
		if (creation_time)
			RtlZeroMemory (creation_time, sizeof (FILETIME));

		if (access_time)
			RtlZeroMemory (access_time, sizeof (FILETIME));

		if (write_time)
			RtlZeroMemory (write_time, sizeof (FILETIME));
	}

	return status;
}

_Success_ (return)
BOOLEAN _r_fs_isfileused (
	_In_ PR_STRINGREF path
)
{
	HANDLE hfile;
	NTSTATUS status;

	status = _r_fs_openfile (
		path,
		GENERIC_WRITE | GENERIC_READ,
		0,
		FILE_SEQUENTIAL_ONLY,
		FALSE,
		&hfile
	);

	if (NT_SUCCESS (status))
		NtClose (hfile);

	return (status == STATUS_SHARING_VIOLATION) ? TRUE : FALSE;
}

_Success_ (return)
BOOLEAN _r_fs_isdirectory (
	_In_ PR_STRINGREF path
)
{
	ULONG attributes;
	NTSTATUS status;

	status = _r_fs_getattributes (path, &attributes);

	if (NT_SUCCESS (status) && attributes & FILE_ATTRIBUTE_DIRECTORY)
		return TRUE;

	return FALSE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_movefile (
	_In_ PR_STRINGREF path_from,
	_In_ PR_STRINGREF path_to,
	_In_ BOOLEAN is_failifexists
)
{
	PFILE_RENAME_INFORMATION rename_info;
	UNICODE_STRING nt_path;
	IO_STATUS_BLOCK isb;
	HANDLE hfile = NULL;
	ULONG attributes;
	ULONG length;
	NTSTATUS status;

	status = _r_fs_dospathnametontpathname (path_to, &nt_path);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_getattributes (path_from, &attributes);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_fs_createfile (
		path_from,
		FILE_OPEN,
		FILE_GENERIC_READ | DELETE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		0,
		(attributes & FILE_ATTRIBUTE_DIRECTORY) ? 0 : FILE_SEQUENTIAL_ONLY,
		(attributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	length = sizeof (FILE_RENAME_INFORMATION) + nt_path.Length + sizeof (UNICODE_NULL);

	rename_info = _r_mem_allocate (length);

	rename_info->ReplaceIfExists = is_failifexists ? FALSE : TRUE;
	rename_info->RootDirectory = NULL;
	rename_info->FileNameLength = nt_path.Length;

	RtlCopyMemory (rename_info->FileName, nt_path.Buffer, nt_path.Length);

	status = NtSetInformationFile (hfile, &isb, rename_info, length, FileRenameInformation);

	if (status == STATUS_NOT_SAME_DEVICE)
	{
		status = _r_fs_copyfile (path_from, path_to, is_failifexists);

		if (NT_SUCCESS (status))
			_r_fs_deletefile (NULL, hfile);
	}

CleanupExit:

	RtlFreeUnicodeString (&nt_path);

	if (hfile)
		NtClose (hfile);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_openfile (
	_In_ PR_STRINGREF path,
	_In_ ACCESS_MASK desired_access,
	_In_opt_ ULONG share_access,
	_In_opt_ ULONG open_options,
	_In_ BOOLEAN is_directory,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	IO_STATUS_BLOCK isb;
	UNICODE_STRING nt_path;
	HANDLE hfile;
	ULONG create_option = 0;
	NTSTATUS status;

	status = _r_fs_dospathnametontpathname (path, &nt_path);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	InitializeObjectAttributes (&oa, &nt_path, OBJ_CASE_INSENSITIVE, NULL, NULL);

	create_option |= is_directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;

	if (open_options)
		create_option |= open_options;

	status = NtOpenFile (
		&hfile,
		desired_access | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		&oa,
		&isb,
		share_access,
		FILE_SYNCHRONOUS_IO_NONALERT | create_option
	);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hfile;
	}
	else
	{
		*out_buffer = NULL;
	}

	RtlFreeUnicodeString (&nt_path);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_readfile (
	_In_ HANDLE hfile,
	_Out_writes_bytes_ (length) PVOID buffer,
	_In_ ULONG length,
	_Out_opt_ PULONG_PTR out_readed
)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	if (out_readed)
		*out_readed = 0;

	status = NtReadFile (hfile, NULL, NULL, NULL, &isb, buffer, length, NULL, NULL);

	if (status == STATUS_PENDING)
	{
		// Wait for the operation to finish. This probably means we got called on an asynchronous file object
		_r_sys_waitforsingleobject (hfile, INFINITE);

		status = isb.Status;
	}

	if (out_readed)
		*out_readed = isb.Information;

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_readbytes (
	_In_ HANDLE hfile,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	LARGE_INTEGER file_size;
	PR_BYTE buffer;
	PVOID buffer_ptr;
	ULONG_PTR length = 0;
	ULONG_PTR readed;
	ULONG allocated_length;
	NTSTATUS status;

	status = _r_fs_getsize (NULL, hfile, &file_size);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	if (!file_size.QuadPart || file_size.QuadPart > PR_SIZE_BUFFER_OVERFLOW)
	{
		*out_buffer = NULL;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

#if defined(_WIN64)
	buffer = _r_obj_createbyte_ex (NULL, file_size.QuadPart);
#else
	buffer = _r_obj_createbyte_ex (NULL, (ULONG_PTR)file_size.LowPart);
#endif // _WIN64

	allocated_length = min ((ULONG)file_size.QuadPart, PR_SIZE_BUFFER);
	buffer_ptr = _r_mem_allocate (allocated_length);

	while (TRUE)
	{
		status = _r_fs_readfile (hfile, buffer_ptr, allocated_length, &readed);

		if (status == STATUS_END_OF_FILE)
		{
			status = STATUS_SUCCESS;

			break;
		}

		if (!NT_SUCCESS (status) || !readed)
			break;

		RtlCopyMemory (PTR_ADD_OFFSET (buffer->buffer, length), buffer_ptr, readed);

		length += readed;
	}

	if (NT_SUCCESS (status))
	{
		*out_buffer = buffer;

		_r_obj_setbytelength (buffer, length);
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (buffer);
	}

	_r_mem_free (buffer_ptr);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_setattributes (
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_STRINGREF path,
	_In_ ULONG attributes
)
{
	FILE_BASIC_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	HANDLE hfile_new = NULL;
	NTSTATUS status;

	if (!path && !hfile)
		return STATUS_INVALID_PARAMETER;

	if (path && !hfile)
	{
		status = _r_fs_openfile (
			path,
			FILE_WRITE_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT,
			FALSE,
			&hfile_new
		);

		if (!NT_SUCCESS (status))
			return status;

		hfile = hfile_new;
	}

	info.FileAttributes = attributes | FILE_ATTRIBUTE_NORMAL;

	status = NtSetInformationFile (hfile, &isb, &info, sizeof (info), FileBasicInformation);

	if (hfile_new)
		NtClose (hfile_new);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_setcurrentdirectory (
	_In_ PR_STRINGREF path
)
{
	UNICODE_STRING us;
	NTSTATUS status;

	_r_obj_initializeunicodestring2 (&us, path);

	status = RtlSetCurrentDirectory_U (&us);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_setpos (
	_In_ HANDLE hfile,
	_In_ PLARGE_INTEGER new_pos
)
{
	FILE_POSITION_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	RtlCopyMemory (&info.CurrentByteOffset, new_pos, sizeof (LARGE_INTEGER));

	status = NtSetInformationFile (hfile, &isb, &info, sizeof (info), FilePositionInformation);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_setsize (
	_In_ HANDLE hfile,
	_In_ PLARGE_INTEGER new_size
)
{
	FILE_END_OF_FILE_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	RtlCopyMemory (&info.EndOfFile, new_size, sizeof (LARGE_INTEGER));

	status = NtSetInformationFile (hfile, &isb, &info, sizeof (info), FileEndOfFileInformation);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_settimestamp (
	_In_ HANDLE hfile,
	_In_opt_ PFILETIME creation_time,
	_In_opt_ PFILETIME access_time,
	_In_opt_ PFILETIME write_time
)
{
	FILE_BASIC_INFORMATION info = {0};
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	if (creation_time)
	{
		info.CreationTime.HighPart = creation_time->dwHighDateTime;
		info.CreationTime.LowPart = creation_time->dwLowDateTime;
	}

	if (access_time)
	{
		info.LastAccessTime.HighPart = access_time->dwHighDateTime;
		info.LastAccessTime.LowPart = access_time->dwLowDateTime;
	}

	if (write_time)
	{
		info.LastWriteTime.HighPart = write_time->dwHighDateTime;
		info.LastWriteTime.LowPart = write_time->dwLowDateTime;
	}

	status = NtSetInformationFile (hfile, &isb, &info, sizeof (info), FileBasicInformation);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_fs_writefile (
	_In_ HANDLE hfile,
	_In_reads_bytes_ (length) PVOID buffer,
	_In_ ULONG length
)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;

	status = NtWriteFile (hfile, NULL, NULL, NULL, &isb, buffer, length, NULL, NULL);

	if (status == STATUS_PENDING)
	{
		// Wait for the operation to finish. This probably means we got called on an asynchronous file object
		_r_sys_waitforsingleobject (hfile, INFINITE);

		status = isb.Status;
	}

	return status;
}

//
// Paths
//

PR_STRING _r_path_compact (
	_In_ PR_STRINGREF path,
	_In_ ULONG length
)
{
	PR_STRINGREF sr;
	PR_STRING string = NULL;
	PR_STRING result;

	if (_r_str_getlength2 (path) <= length)
		return _r_obj_createstring2 (path);

	result = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	if (_r_str_isnullterminated (path))
	{
		sr = path;
	}
	else
	{
		string = _r_obj_createstring2 (path);

		sr = &string->sr;
	}

	if (PathCompactPathExW (result->buffer, sr->buffer, length, 0))
	{
		_r_str_trimtonullterminator (&result->sr);

		if (string)
			_r_obj_dereference (string);

		return result;
	}

	if (string)
		_r_obj_dereference (string);

	_r_obj_dereference (result);

	return _r_obj_createstring2 (path);
}

_Ret_maybenull_
PR_STRING _r_path_find (
	_In_ PR_STRINGREF path
)
{
	PR_STRING string;
	NTSTATUS status;

	if (_r_fs_exists (path))
		return _r_obj_createstring2 (path);

	if (path->buffer[0] == L'%')
	{
		status = _r_str_environmentexpandstring (NULL, path, &string);

		if (!NT_SUCCESS (status))
			return NULL;
	}
	else if (_r_path_getnametype (path) == RtlPathTypeRelative)
	{
		status = _r_path_search (NULL, path, (_r_str_findchar (path, L'.', FALSE) == SIZE_MAX) ? L".exe" : NULL, &string);

		if (!NT_SUCCESS (status))
			return NULL;
	}
	else
	{
		string = _r_obj_createstring2 (path);
	}

	return string;
}

BOOLEAN _r_path_geticon (
	_In_ PR_STRINGREF path,
	_Out_opt_ PLONG out_icon_id,
	_Out_opt_ HICON_PTR out_hicon
)
{
	SHFILEINFO shfi = {0};
	PR_STRINGREF sr;
	PR_STRING string = NULL;
	UINT flags = SHGFI_LARGEICON;
	BOOLEAN is_success = FALSE;

	if (!out_icon_id && !out_hicon)
		return FALSE;

	if (out_icon_id)
	{
		flags |= SHGFI_SYSICONINDEX;

		*out_icon_id = 0;
	}

	if (out_hicon)
	{
		flags |= SHGFI_ICON;

		*out_hicon = NULL;
	}

	if (_r_str_isnullterminated (path))
	{
		sr = path;
	}
	else
	{
		string = _r_obj_createstring2 (path);

		sr = &string->sr;
	}

	if (SHGetFileInfoW (sr->buffer, 0, &shfi, sizeof (SHFILEINFO), flags))
	{
		if (out_icon_id)
			*out_icon_id = shfi.iIcon;

		if (out_hicon)
			*out_hicon = shfi.hIcon; // cleanup required

		is_success = TRUE;
	}

	if (string)
		_r_obj_dereference (string);

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

	return _r_obj_createstring2 (&directory_part);
}

LPWSTR _r_path_getbasename (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;

	if (!_r_path_getpathinfo (path, &directory_part, &basename_part))
		return path->buffer;

	return basename_part.buffer;
}

PR_STRING _r_path_getbasenamestring (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;

	if (!_r_path_getpathinfo (path, &directory_part, &basename_part))
		return _r_obj_createstring2 (path);

	return _r_obj_createstring2 (&basename_part);
}

_Ret_maybenull_
LPCWSTR _r_path_getextension (
	_In_ LPWSTR path
)
{
	R_STRINGREF fullpath_part;
	R_STRINGREF directory_part;
	R_STRINGREF extension_part;

	_r_obj_initializestringref (&fullpath_part, path);

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

	return _r_obj_createstring2 (&extension_part);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_path_getfullpath (
	_In_ LPCWSTR filename,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING full_path;
	ULONG buffer_length = 512;
	ULONG return_length;
	NTSTATUS status;

	full_path = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

	status = RtlGetFullPathName_UEx (filename, buffer_length, full_path->buffer, NULL, &return_length);

	if (!NT_SUCCESS (status))
	{
		_r_obj_dereference (full_path);

		*out_buffer = NULL;

		return status;
	}

	if (return_length > buffer_length)
	{
		buffer_length = return_length;

		_r_obj_movereference (&full_path, _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR)));

		status = RtlGetFullPathName_UEx (filename, buffer_length, full_path->buffer, NULL, &return_length);
	}

	if (NT_SUCCESS (status))
	{
		_r_str_trimtonullterminator (&full_path->sr);

		*out_buffer = full_path;
	}
	else
	{
		_r_obj_dereference (full_path);

		*out_buffer = NULL;
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_path_getknownfolder (
	_In_ LPCGUID rfid,
	_In_ ULONG flags,
	_In_opt_ LPCWSTR append,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING string;
	LPWSTR buffer;
	ULONG_PTR append_length;
	HRESULT status;

	// vista+
	status = SHGetKnownFolderPath (rfid, flags | KF_FLAG_DONT_VERIFY, NULL, &buffer);

	if (SUCCEEDED (status))
	{
		append_length = append ? _r_str_getlength (append) * sizeof (WCHAR) : 0;

		string = _r_obj_createstring_ex (buffer, (_r_str_getlength (buffer) * sizeof (WCHAR)) + append_length);

		_r_str_trimtonullterminator (&string->sr);

		if (append)
		{
			RtlCopyMemory (&string->buffer[_r_str_getlength2 (&string->sr)], append, append_length + sizeof (UNICODE_NULL));

			string->length += append_length;
		}

		*out_buffer = string;

		CoTaskMemFree (buffer);
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (
	_In_ PVOID hinst
)
{
	PLDR_DATA_TABLE_ENTRY result = NULL;
	PLDR_DATA_TABLE_ENTRY entry;
	PLIST_ENTRY list_entry;
	PLIST_ENTRY list_head;
	PR_STRING path;

	RtlEnterCriticalSection (NtCurrentPeb ()->LoaderLock);

	list_head = &NtCurrentPeb ()->Ldr->InLoadOrderModuleList;
	list_entry = list_head->Flink;

	while (list_entry != list_head)
	{
		entry = CONTAINING_RECORD (list_entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		if (entry->DllBase == hinst)
		{
			result = entry;

			break;
		}

		list_entry = list_entry->Flink;
	}

	path = result ? _r_obj_createstring3 (&result->FullDllName) : NULL;

	RtlLeaveCriticalSection (NtCurrentPeb ()->LoaderLock);

	return path;
}

_Success_ (return)
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

	is_success = _r_str_splitatlastchar (path, OBJ_NAME_PATH_SEPARATOR, &directory_part, &basename_part);

	if (directory)
		_r_obj_initializestringref2 (directory, &directory_part);

	if (basename)
		_r_obj_initializestringref2 (basename, &basename_part);

	return is_success;
}

RTL_PATH_TYPE _r_path_getnametype (
	_In_ PR_STRINGREF path
)
{
	// RtlDetermineDosPathNameType_U
	if (path->buffer[0] == OBJ_NAME_PATH_SEPARATOR || path->buffer[0] == OBJ_NAME_ALTPATH_SEPARATOR)
	{
		if (path->buffer[1] == OBJ_NAME_PATH_SEPARATOR || path->buffer[1] == OBJ_NAME_ALTPATH_SEPARATOR)
		{
			if (path->buffer[2] == L'?' || path->buffer[2] == L'.')
			{
				if (path->buffer[3] == OBJ_NAME_PATH_SEPARATOR || path->buffer[3] == OBJ_NAME_ALTPATH_SEPARATOR)
					return RtlPathTypeLocalDevice;

				if (path->buffer[3] != UNICODE_NULL)
					return RtlPathTypeUncAbsolute;

				return RtlPathTypeRootLocalDevice;
			}

			return RtlPathTypeUncAbsolute;
		}

		return RtlPathTypeRooted;
	}
	else if (path->buffer[0] != UNICODE_NULL && path->buffer[1] == L':')
	{
		if (path->buffer[2] == OBJ_NAME_PATH_SEPARATOR || path->buffer[2] == OBJ_NAME_ALTPATH_SEPARATOR)
			return RtlPathTypeDriveAbsolute;

		return RtlPathTypeDriveRelative;
	}

	return RtlPathTypeRelative;
}

BOOLEAN _r_path_issecurelocation (
	_In_ LPWSTR path
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PACCESS_ALLOWED_ACE ace = {0};
	R_STRINGREF sr;
	PSID current_token;
	PACL dacl;
	BOOLEAN is_writeable = FALSE;
	NTSTATUS status;

	_r_obj_initializestringref (&sr, path);

	status = _r_fs_getsecurityinfo (NULL, &sr, &security_descriptor, &dacl);

	if (!NT_SUCCESS (status))
		return FALSE;

	if (!dacl)
	{
		is_writeable = TRUE;
	}
	else
	{
		current_token = _r_sys_getcurrenttoken ()->token_sid;

		for (WORD ace_index = 0; ace_index < dacl->AceCount; ace_index++)
		{
			status = RtlGetAce (dacl, ace_index, &ace);

			if (!NT_SUCCESS (status))
				continue;

			if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
				continue;

			if (RtlEqualSid (&ace->SidStart, &SeAuthenticatedUserSid) || RtlEqualSid (&ace->SidStart, current_token))
			{
				if (ace->Mask & (DELETE | ACTRL_FILE_WRITE_ATTRIB | SYNCHRONIZE | READ_CONTROL))
				{
					is_writeable = TRUE;

					break;
				}
			}
		}
	}

	_r_mem_free (security_descriptor);

	return !is_writeable;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_path_makebackup (
	_In_ PR_STRING path,
	_In_ BOOLEAN is_removesourcefile
)
{
	WCHAR timestamp_string[64];
	PR_STRING new_path;
	LONG64 current_timestamp;
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;
	PR_STRING directory;
	NTSTATUS status;

	_r_obj_initializestringref2 (&directory_part, &path->sr);

	_r_path_getpathinfo (&directory_part, &directory_part, &basename_part);

	directory = _r_obj_createstring2 (&directory_part);

	current_timestamp = _r_unixtime_now ();

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

	if (_r_fs_exists (&new_path->sr))
		_r_fs_deletefile (&new_path->sr, NULL);

	if (is_removesourcefile)
	{
		status = _r_fs_movefile (&path->sr, &new_path->sr, FALSE);

		if (!NT_SUCCESS (status))
		{
			status = _r_fs_copyfile (&path->sr, &new_path->sr, FALSE);

			if (NT_SUCCESS (status))
				_r_fs_deletefile (&path->sr, NULL);
		}
	}
	else
	{
		status = _r_fs_copyfile (&path->sr, &new_path->sr, FALSE);
	}

	_r_obj_dereference (directory);
	_r_obj_dereference (new_path);

	return status;
}

// Parses a command line string. If the string does not contain quotation marks
// around the file name part, the function determines the file name to use.
//
// Source: https://github.com/processhacker2/processhacker

_Success_ (return)
BOOLEAN _r_path_parsecommandlinefuzzy (
	_In_ PR_STRINGREF args,
	_Out_ PR_STRINGREF out_path,
	_Out_opt_ PR_STRINGREF out_command_line,
	_Out_opt_ PR_STRING_PTR out_full_file_name
)
{
	R_STRINGREF whitespace = PR_STRINGREF_INIT (L" \t");
	R_STRINGREF remaining_part;
	R_STRINGREF current_part;
	R_STRINGREF arguments;
	R_STRINGREF temp = {0};
	R_STRINGREF args_sr;
	PR_STRING file_path;
	WCHAR original_char;
	LPWSTR extension;
	BOOLEAN is_found;
	NTSTATUS status;

	_r_obj_initializestringref2 (&args_sr, args);

	_r_str_trimstring (&args_sr, &whitespace, 0);

	if (_r_obj_isstringempty2 (&args_sr))
	{
		_r_obj_initializestringrefempty (out_path);

		if (out_command_line)
			_r_obj_initializestringrefempty (out_command_line);

		if (out_full_file_name)
			*out_full_file_name = NULL;

		return FALSE;
	}

	extension = (_r_str_findchar (&args_sr, L'.', FALSE) == SIZE_MAX) ? L".exe" : NULL;

	if (*args_sr.buffer == L'"')
	{
		_r_str_skiplength (&args_sr, sizeof (WCHAR));

		// Find the matching quote character and we have our file name.
		if (!_r_str_splitatchar (&args_sr, L'"', &args_sr, &arguments))
		{
			// Unskip the initial quote character
			_r_str_skiplength (&args_sr, -(LONG_PTR)sizeof (WCHAR));

			if (out_command_line)
				_r_obj_initializestringrefempty (out_command_line);

			if (out_full_file_name)
				*out_full_file_name = NULL;

			return FALSE;
		}

		_r_str_trimstring (&arguments, &whitespace, PR_TRIM_START_ONLY);

		_r_obj_initializestringref2 (out_path, &args_sr);

		if (out_command_line)
			_r_obj_initializestringref2 (out_command_line, &arguments);

		if (out_full_file_name)
		{
			status = _r_path_search (NULL, &args_sr, extension, &file_path);

			if (NT_SUCCESS (status))
			{
				*out_full_file_name = file_path;
			}
			else
			{
				*out_full_file_name = NULL;
			}
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

	temp.buffer = _r_mem_allocate (args_sr.length + sizeof (UNICODE_NULL));

	RtlCopyMemory (temp.buffer, args_sr.buffer, args_sr.length);

	temp.buffer[_r_str_getlength2 (&args_sr)] = UNICODE_NULL;
	temp.length = args_sr.length;

	_r_obj_initializestringref2 (&remaining_part, &temp);

	while (remaining_part.length != 0)
	{
		original_char = UNICODE_NULL;

		is_found = _r_str_splitatchar (&remaining_part, L' ', &current_part, &remaining_part);

		if (is_found)
		{
			original_char = *(remaining_part.buffer - 1);

			*(remaining_part.buffer - 1) = UNICODE_NULL;
		}

		_r_str_trimtonullterminator (&temp); // terminate

		// HACK!
		if (_r_fs_isdirectory (&temp))
			continue;

		status = _r_path_search (NULL, &temp, extension, &file_path);

		if (is_found)
			*(remaining_part.buffer - 1) = original_char;

		if (NT_SUCCESS (status))
		{
			out_path->buffer = args_sr.buffer;
			out_path->length = (ULONG_PTR)PTR_SUB_OFFSET (current_part.buffer, temp.buffer) + current_part.length;

			_r_str_trimstring (&remaining_part, &whitespace, PR_TRIM_START_ONLY);

			if (out_command_line)
			{
				if (remaining_part.length)
				{
					out_command_line->buffer = PTR_ADD_OFFSET (args_sr.buffer, PTR_SUB_OFFSET (remaining_part.buffer, temp.buffer));
					out_command_line->length = args_sr.length - (ULONG_PTR)PTR_SUB_OFFSET (remaining_part.buffer, temp.buffer);
				}
				else
				{
					_r_obj_initializestringrefempty (out_command_line);
				}
			}

			if (out_full_file_name)
			{
				*out_full_file_name = file_path;
			}
			else
			{
				if (file_path)
					_r_obj_dereference (file_path);
			}

			_r_mem_free (temp.buffer);

			return TRUE;
		}
	}

	_r_mem_free (temp.buffer);

	_r_obj_initializestringref2 (out_path, args);

	if (out_command_line)
		_r_obj_initializestringrefempty (out_command_line);

	if (out_full_file_name)
		*out_full_file_name = NULL;

	return FALSE;
}

_Ret_maybenull_
PR_STRING _r_path_resolvedeviceprefix (
	_In_ PR_STRINGREF path
)
{
#if defined(_WIN64)
	PROCESS_DEVICEMAP_INFORMATION_EX device_map = {0};
#else
	PROCESS_DEVICEMAP_INFORMATION device_map = {0};
#endif // !_WIN64

	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING device_name;
	UNICODE_STRING device_prefix;
	R_STRINGREF device_prefix_sr;
	WCHAR device_prefix_buffer[PR_DEVICE_PREFIX_LENGTH] = {0};
	WCHAR device_name_buffer[7] = L"\\??\\?:";
	PR_STRING string;
	HANDLE link_handle;
	ULONG_PTR prefix_length;
	ULONG flags;
	NTSTATUS status;

	status = NtQueryInformationProcess (NtCurrentProcess (), ProcessDeviceMap, &device_map, sizeof (device_map), NULL);

	if (!NT_SUCCESS (status))
		return NULL;

	flags = OBJ_CASE_INSENSITIVE | (_r_sys_isosversiongreaterorequal (WINDOWS_10) ? OBJ_DONT_REPARSE : 0);

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

		InitializeObjectAttributes (&oa, &device_name, flags, NULL, NULL);

		status = NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &oa);

		if (NT_SUCCESS (status))
		{
			_r_obj_initializeunicodestring_ex (&device_prefix, device_prefix_buffer, 0, RTL_NUMBER_OF (device_prefix_buffer) * sizeof (WCHAR));

			status = NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL);

			if (NT_SUCCESS (status))
			{
				_r_obj_initializestringref3 (&device_prefix_sr, &device_prefix);

				if (_r_str_isstartswith (path, &device_prefix_sr, TRUE))
				{
					prefix_length = _r_str_getlength3 (&device_prefix);

					// To ensure we match the longest prefix, make sure the next character is a
					// backslash or the path is equal to the prefix.
					if (path->length == device_prefix.Length || path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
					{
						// <letter>:path
						string = _r_obj_createstring_ex (NULL, (2 * sizeof (WCHAR)) + path->length - device_prefix.Length);

						string->buffer[0] = L'A' + (WCHAR)i;
						string->buffer[1] = L':';

						RtlCopyMemory (&string->buffer[2], &path->buffer[prefix_length], path->length - device_prefix.Length);

						_r_str_trimtonullterminator (&string->sr);

						NtClose (link_handle);

						return string;
					}
				}
			}

			NtClose (link_handle);
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
	_In_ PR_STRINGREF path
)
{
	WCHAR device_prefix_buffer[PR_DEVICE_PREFIX_LENGTH] = {0};
	POBJECT_DIRECTORY_INFORMATION directory_entry;
	POBJECT_DIRECTORY_INFORMATION directory_info;
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING device_prefix;
	R_STRINGREF device_prefix_sr;
	PR_STRING string;
	HANDLE link_handle;
	HANDLE directory_handle;
	ULONG_PTR prefix_length;
	ULONG query_context = 0;
	ULONG buffer_length;
	ULONG i;
	ULONG flags;
	BOOLEAN is_firsttime = TRUE;
	NTSTATUS status;

	RtlInitUnicodeString (&device_prefix, L"\\GLOBAL??");

	InitializeObjectAttributes (&oa, &device_prefix, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtOpenDirectoryObject (&directory_handle, DIRECTORY_QUERY, &oa);

	if (!NT_SUCCESS (status))
		return NULL;

	flags = OBJ_CASE_INSENSITIVE | (_r_sys_isosversiongreaterorequal (WINDOWS_10) ? OBJ_DONT_REPARSE : 0);

	buffer_length = sizeof (OBJECT_DIRECTORY_INFORMATION) + (512 * sizeof (WCHAR));
	directory_entry = _r_mem_allocate (buffer_length);

	while (TRUE)
	{
		while (TRUE)
		{
			status = NtQueryDirectoryObject (directory_handle, directory_entry, buffer_length, FALSE, is_firsttime, &query_context, NULL);

			if (status != STATUS_MORE_ENTRIES)
				break;

			// Check if we have at least one entry. If not, we'll double the buffer size and try again.
			if (directory_entry[0].Name.Buffer)
				break;

			// Make sure we don't use too much memory.
			if (buffer_length > PR_SIZE_BUFFER_OVERFLOW)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;

				break;
			}

			buffer_length *= 2;

			directory_entry = _r_mem_reallocate (directory_entry, buffer_length);
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
				InitializeObjectAttributes (&oa, &directory_info->Name, flags, directory_handle, NULL);

				status = NtOpenSymbolicLinkObject (&link_handle, SYMBOLIC_LINK_QUERY, &oa);

				if (NT_SUCCESS (status))
				{
					_r_obj_initializeunicodestring_ex (&device_prefix, device_prefix_buffer, 0, RTL_NUMBER_OF (device_prefix_buffer) * sizeof (WCHAR));

					status = NtQuerySymbolicLinkObject (link_handle, &device_prefix, NULL);

					if (NT_SUCCESS (status))
					{
						_r_obj_initializestringref3 (&device_prefix_sr, &device_prefix);

						if (_r_str_isstartswith (path, &device_prefix_sr, TRUE))
						{
							prefix_length = _r_str_getlength3 (&device_prefix);

							// To ensure we match the longest prefix, make sure the next character is a
							// backslash or the path is equal to the prefix.
							if (path->length == device_prefix.Length || path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
							{
								// <letter>:path
								string = _r_obj_createstring_ex (NULL, (2 * sizeof (WCHAR)) + path->length - device_prefix.Length);

								string->buffer[0] = directory_info->Name.Buffer[0];
								string->buffer[1] = L':';

								RtlCopyMemory (&string->buffer[2], &path->buffer[prefix_length], path->length - device_prefix.Length);

								_r_str_trimtonullterminator (&string->sr);

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

	return NULL;
}

// network share prefixes
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/support-for-unc-naming-and-mup

_Ret_maybenull_
PR_STRING _r_path_resolvenetworkprefix (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF services_part_sr = PR_STRINGREF_INIT (L"System\\CurrentControlSet\\Services\\");
	R_STRINGREF provider_part_sr = PR_STRINGREF_INIT (L"\\NetworkProvider");
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING device_name_string;
	PR_STRING provider_order;
	PR_STRING service_key;
	PR_STRING string;
	HANDLE hsvckey;
	HANDLE hkey;
	ULONG_PTR prefix_length;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order", 0, KEY_READ, &hkey);

	if (!NT_SUCCESS (status))
		return NULL;

	status = _r_reg_querystring (hkey, L"ProviderOrder", &provider_order, NULL);

	if (NT_SUCCESS (status))
	{
		_r_obj_initializestringref2 (&remaining_part, &provider_order->sr);

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

				status = _r_reg_openkey (HKEY_LOCAL_MACHINE, service_key->buffer, 0, KEY_READ, &hsvckey);

				if (NT_SUCCESS (status))
				{
					status = _r_reg_querystring (hsvckey, L"DeviceName", &device_name_string, NULL);

					if (NT_SUCCESS (status))
					{
						if (_r_str_isstartswith (path, &device_name_string->sr, TRUE))
						{
							prefix_length = _r_str_getlength2 (&device_name_string->sr);

							// To ensure we match the longest prefix, make sure the next character is a
							// backslash. Don't resolve if the name *is* the prefix. Otherwise, we will end
							// up with a useless string like "\".
							if (path->length != device_name_string->length && path->buffer[prefix_length] == OBJ_NAME_PATH_SEPARATOR)
							{
								// \path
								string = _r_obj_createstring_ex (NULL, sizeof (WCHAR) + path->length - device_name_string->length);

								string->buffer[0] = OBJ_NAME_PATH_SEPARATOR;

								RtlCopyMemory (&string->buffer[1], &path->buffer[prefix_length], path->length - device_name_string->length);

								_r_str_trimtonullterminator (&string->sr);

								_r_obj_dereference (device_name_string);
								_r_obj_dereference (provider_order);
								_r_obj_dereference (service_key);

								NtClose (hsvckey);
								NtClose (hkey);

								return string;
							}
						}

						_r_obj_dereference (device_name_string);
					}

					NtClose (hsvckey);
				}

				_r_obj_dereference (service_key);
			}
		}

		_r_obj_dereference (provider_order);
	}

	NtClose (hkey);

	return NULL;
}

PR_STRING _r_path_dospathfromnt (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF system_root;
	PR_STRING string;
	LPWSTR ptr;
	ULONG_PTR path_length;

	path_length = _r_str_getlength2 (path);

	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_r_str_isstartswith2 (path, L"\\??\\", TRUE))
	{
		string = _r_obj_createstring_ex (NULL, path->length - 4 * sizeof (WCHAR));

		RtlCopyMemory (string->buffer, &path->buffer[4], path->length - 4 * sizeof (WCHAR));

		return string;
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_r_str_isstartswith2 (path, L"\\systemroot", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + path->length - 11 * sizeof (WCHAR));

		RtlCopyMemory (string->buffer, system_root.buffer, system_root.length);

		ptr = PTR_ADD_OFFSET (string->buffer, system_root.length);

		RtlCopyMemory (ptr, &path->buffer[11], path->length - 11 * sizeof (WCHAR));

		return string;
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_r_str_isstartswith2 (path, L"system32\\", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + sizeof (UNICODE_NULL) + path->length);

		RtlCopyMemory (string->buffer, system_root.buffer, system_root.length);

		string->buffer[_r_str_getlength2 (&system_root)] = OBJ_NAME_PATH_SEPARATOR;

		ptr = PTR_ADD_OFFSET (string->buffer, system_root.length + sizeof (WCHAR));

		RtlCopyMemory (ptr, path->buffer, path->length);

		return string;
	}
	// "SysWOW64\" means "C:\Windows\SysWOW64\".
	else if (_r_str_isstartswith2 (path, L"SysWOW64\\", TRUE))
	{
		_r_sys_getsystemroot (&system_root);

		string = _r_obj_createstring_ex (NULL, system_root.length + sizeof (UNICODE_NULL) + path->length);

		RtlCopyMemory (string->buffer, system_root.buffer, system_root.length);

		string->buffer[_r_str_getlength2 (&system_root)] = OBJ_NAME_PATH_SEPARATOR;

		ptr = PTR_ADD_OFFSET (string->buffer, system_root.length + sizeof (UNICODE_NULL));

		RtlCopyMemory (ptr, path->buffer, path->length);

		return string;
	}
	else if (_r_str_isstartswith2 (path, L"\\device\\", TRUE))
	{
		// network share (win7+)
		if (_r_str_isstartswith2 (path, L"\\device\\mup", TRUE))
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
		// network share (winxp+)
		else if (_r_str_isstartswith2 (path, L"\\device\\lanmanredirector", TRUE))
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

	return _r_obj_createstring2 (path);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_path_ntpathfromdos (
	_In_ PR_STRINGREF path,
	_In_ BOOLEAN is_lowercase,
	_Out_ PR_STRING_PTR out_buffer
)
{
	POBJECT_NAME_INFORMATION obj_name_info;
	HANDLE hfile;
	PR_STRING string;
	ULONG buffer_length;
	ULONG attributes;
	ULONG attempts = 6;
	NTSTATUS status;

	_r_fs_getattributes (path, &attributes);

	status = _r_fs_openfile (
		path,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT,
		attributes & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE,
		&hfile
	);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	buffer_length = sizeof (OBJECT_NAME_INFORMATION) + (512 * sizeof (WCHAR));
	obj_name_info = _r_mem_allocate (buffer_length);

	do
	{
		status = NtQueryObject (hfile, ObjectNameInformation, obj_name_info, buffer_length, &buffer_length);

		if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL)
		{
			if (buffer_length > PR_SIZE_BUFFER_OVERFLOW)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;

				break;
			}

			obj_name_info = _r_mem_reallocate (obj_name_info, buffer_length);
		}
		else
		{
			break;
		}
	}
	while (--attempts);

	if (NT_SUCCESS (status))
	{
		string = _r_obj_createstring3 (&obj_name_info->Name);

		if (is_lowercase)
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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_path_search (
	_In_opt_ PR_STRINGREF path,
	_In_ PR_STRINGREF filename,
	_In_opt_ LPWSTR extension,
	_Out_ PR_STRING_PTR out_buffer
)
{
	R_STRINGREF env = PR_STRINGREF_INIT (L"%PATH%");
	UNICODE_STRING extension_us;
	UNICODE_STRING filename_us;
	UNICODE_STRING buffer = {0};
	UNICODE_STRING path_us;
	PR_STRING full_path = NULL;
	PR_STRING string = NULL;
	ULONG_PTR return_length;
	ULONG flags = RTL_DOS_SEARCH_PATH_FLAG_DISALLOW_DOT_RELATIVE_PATH_SEARCH | RTL_DOS_SEARCH_PATH_FLAG_APPLY_DEFAULT_EXTENSION_WHEN_NOT_RELATIVE_PATH_EVEN_IF_FILE_HAS_EXTENSION;
	NTSTATUS status;

	if (path)
	{
		_r_obj_initializeunicodestring2 (&path_us, path);
	}
	else
	{
		// works as RtlGetSearchPath (win8+)
		status = _r_str_environmentexpandstring (NULL, &env, &string);

		if (!NT_SUCCESS (status))
		{
			*out_buffer = NULL;

			return status;
		}

		_r_obj_initializeunicodestring2 (&path_us, &string->sr);

		// request SxS isolation
		flags |= RTL_DOS_SEARCH_PATH_FLAG_APPLY_ISOLATION_REDIRECTION;
	}

	_r_obj_initializeunicodestring2 (&filename_us, filename);
	_r_obj_initializeunicodestring (&extension_us, extension);

	status = RtlDosSearchPath_Ustr (flags, &path_us, &filename_us, &extension_us, NULL, NULL, NULL, NULL, &return_length);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		full_path = _r_obj_createstring_ex (NULL, return_length * sizeof (WCHAR));

		_r_obj_initializeunicodestring_ex (&buffer, full_path->buffer, 0, (ULONG)full_path->length);

		// return can be STATUS_NO_SUCH_FILE
		status = RtlDosSearchPath_Ustr (flags, &path_us, &filename_us, &extension_us, &buffer, NULL, NULL, NULL, NULL);
	}

	if (NT_SUCCESS (status))
	{
		if (full_path)
			_r_str_setlength (&full_path->sr, full_path->length / sizeof (WCHAR)); // terminate

		*out_buffer = full_path;
	}
	else
	{
		if (full_path)
			_r_obj_dereference (full_path); // cleanup

		*out_buffer = NULL;
	}

	if (string)
		_r_obj_dereference (string);

	return status;
}

//
// Locale
//

_Ret_maybenull_
PR_STRING _r_locale_getinfo (
	_In_ LCID locale,
	_In_ LCTYPE locale_type
)
{
	PR_STRING string;
	ULONG return_length;

	return_length = GetLocaleInfoW (locale, locale_type, NULL, 0);

	if (!return_length)
		return NULL;

	string = _r_obj_createstring_ex (NULL, return_length * sizeof (WCHAR) - sizeof (UNICODE_NULL));

	return_length = GetLocaleInfoW (locale, locale_type, string->buffer, return_length);

	if (return_length)
		return string;

	_r_obj_dereference (string);

	return NULL;
}

NTSTATUS _r_locale_getlcid (
	_In_ BOOLEAN is_userprofile,
	_Out_ PLCID out_buffer
)
{
	LCID locale_id = is_userprofile ? LOCALE_USER_DEFAULT : LOCALE_SYSTEM_DEFAULT;
	NTSTATUS status;

	status = NtQueryDefaultLocale (is_userprofile, &locale_id);

	if (NT_SUCCESS (status))
	{
		*out_buffer = locale_id;
	}
	else
	{
		*out_buffer = is_userprofile ? LOCALE_USER_DEFAULT : LOCALE_SYSTEM_DEFAULT;
	}

	return status;
}

NTSTATUS _r_locale_lcidtoname (
	_In_ LCID lcid,
	_Out_ PR_STRING_PTR out_buffer
)
{
	WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {0};
	UNICODE_STRING us = {0};
	NTSTATUS status;

	_r_obj_initializeunicodestring_ex (&us, locale_name, 0, sizeof (locale_name));

	status = RtlLcidToLocaleName (lcid, &us, 0, FALSE);

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_createstring3 (&us);
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

//
// Shell
//

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_shell_openkey (
	_In_opt_ HWND hwnd,
	_In_ HANDLE hkey
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_STRINGREF hkcr_prefix_sr = PR_STRINGREF_INIT (L"\\Registry\\Machine\\Software\\Classes");
	static R_STRINGREF hklm_prefix_sr = PR_STRINGREF_INIT (L"\\Registry\\Machine");
	static R_STRINGREF hku_prefix_sr = PR_STRINGREF_INIT (L"\\Registry\\User");
	static R_STRINGREF hkcucr_sr = PR_STRINGREF_INIT (L"HKCU\\Software\\Classes");
	static R_STRINGREF hklm_sr = PR_STRINGREF_INIT (L"HKLM");
	static R_STRINGREF hkcr_sr = PR_STRINGREF_INIT (L"HKCR");
	static R_STRINGREF hkcu_sr = PR_STRINGREF_INIT (L"HKCU");
	static R_STRINGREF hku_sr = PR_STRINGREF_INIT (L"HKU");
	static PR_STRING hkcucr_prefix;
	static PR_STRING hkcu_prefix;

	R_STRINGREF registry_user_sr = PR_STRINGREF_INIT (L"\\Registry\\User\\");
	R_STRINGREF classes_sr = PR_STRINGREF_INIT (L"_Classes");
	R_STRINGREF name;
	PR_STRING string = NULL;
	PR_STRING value = NULL;
	HANDLE hkey_handle = NULL;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_str_fromsid (_r_sys_getcurrenttoken ()->token_sid, &string);

		if (NT_SUCCESS (status))
		{
			hkcu_prefix = _r_obj_concatstringrefs (2, &registry_user_sr, &string->sr);
			hkcucr_prefix = _r_obj_concatstringrefs (2, &hkcu_prefix->sr, &classes_sr);

			_r_obj_dereference (string);
		}
		else
		{
			// some random string that won't ever get matched
			hkcu_prefix = _r_obj_createstring (L"...");
			hkcucr_prefix = _r_obj_createstring (L"...");
		}

		_r_initonce_end (&init_once);
	}

	status = _r_fs_getobjectname (hkey, FALSE, &string);

	if (!NT_SUCCESS (status))
	{
		if (hwnd)
			_r_show_errormessage (hwnd, L"Could not get object name!", status, NULL, ET_NATIVE);

		return status;
	}

	status = _r_reg_createkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, KEY_WRITE, NULL, &hkey_handle);

	if (!NT_SUCCESS (status))
	{
		if (hwnd)
			_r_show_errormessage (hwnd, L"Could not open key!", status, NULL, ET_NATIVE);

		goto CleanupExit;
	}

	name = string->sr;

	if (_r_str_isstartswith (&name, &hkcu_prefix->sr, TRUE))
	{
		_r_str_skiplength (&name, hkcu_prefix->length);

		value = _r_obj_concatstringrefs (2, &hkcu_sr, &name);
	}
	else if (_r_str_isstartswith (&name, &hklm_prefix_sr, TRUE))
	{
		_r_str_skiplength (&name, hklm_prefix_sr.length);

		value = _r_obj_concatstringrefs (2, &hklm_sr, &name);
	}
	else if (_r_str_isstartswith (&name, &hkcr_prefix_sr, TRUE))
	{
		_r_str_skiplength (&name, hkcr_sr.length);

		value = _r_obj_concatstringrefs (2, &hkcr_prefix_sr, &name);
	}
	else if (_r_str_isstartswith (&name, &hkcucr_prefix->sr, TRUE))
	{
		_r_str_skiplength (&name, hkcucr_prefix->length);

		value = _r_obj_concatstringrefs (2, &hkcucr_sr, &name);
	}
	else if (_r_str_isstartswith (&name, &hku_prefix_sr, TRUE))
	{
		_r_str_skiplength (&name, hku_prefix_sr.length);

		value = _r_obj_concatstringrefs (2, &hku_sr, &name);
	}
	else
	{
		value = _r_obj_reference (string);
	}

	status = _r_reg_setvalue (hkey_handle, L"LastKey", REG_SZ, value->buffer, (ULONG)value->length);

	if (NT_SUCCESS (status))
	{
		if (_r_sys_iselevated ())
		{
			status = _r_sys_createprocess (L"regedit.exe", NULL, NULL, FALSE);
		}
		else
		{
			status = _r_sys_runasadmin (L"regedit.exe", NULL, NULL);
		}

		if (hwnd && !NT_SUCCESS (status) && status != STATUS_CANCELLED)
			_r_show_errormessage (hwnd, NULL, status, L"Could not create process", ET_NATIVE);
	}
	else
	{
		if (hwnd)
			_r_show_errormessage (hwnd, NULL, status, L"Could not set value", ET_NATIVE);
	}

CleanupExit:

	if (string)
		_r_obj_dereference (string);

	if (value)
		_r_obj_dereference (value);

	if (hkey_handle)
		NtClose (hkey_handle);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_shell_resolveshortcut (
	_In_ LPWSTR lnk_path,
	_Out_ PR_STRING_PTR out_path,
	_Out_opt_ PR_STRING_PTR out_arguments
)
{
	IPersistFile* persist_file = NULL;
	IShellLinkW* shell_link = NULL;
	WCHAR buffer[512] = {0};
	HRESULT status;

	*out_path = NULL;

	if (out_arguments)
		*out_arguments = NULL;

	status = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, &shell_link);

	if (SUCCEEDED (status))
	{
		status = IShellLinkW_QueryInterface (shell_link, &IID_IPersistFile, &persist_file);

		if (SUCCEEDED (status))
		{
			status = IPersistFile_Load (persist_file, lnk_path, STGM_READ);

			if (SUCCEEDED (status) && SUCCEEDED (IShellLinkW_Resolve (shell_link, NULL, SLR_NO_UI)))
			{
				status = IShellLinkW_GetPath (shell_link, buffer, RTL_NUMBER_OF (buffer), NULL, 0);

				if (SUCCEEDED (status))
					*out_path = _r_obj_createstring (buffer);

				if (out_arguments)
				{
					status = IShellLinkW_GetArguments (shell_link, buffer, RTL_NUMBER_OF (buffer));

					if (SUCCEEDED (status))
						*out_arguments = _r_format_string (L"\"%s\" %s", _r_obj_getstring (*out_path), buffer);
				}
			}

			IPersistFile_Release (persist_file);
		}

		IShellLinkW_Release (shell_link);
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_shell_showfile (
	_In_ PR_STRINGREF path
)
{
	SHELLEXECUTEINFO info = {0};
	LPITEMIDLIST item;
	PR_STRINGREF sr;
	PR_STRING string = NULL;
	HRESULT status;

	if (!_r_fs_exists (path))
		return STG_E_PATHNOTFOUND;

	if (_r_str_isnullterminated (path))
	{
		sr = path;
	}
	else
	{
		string = _r_obj_createstring2 (path);

		sr = &string->sr;
	}

	if (_r_fs_isdirectory (path))
	{
		info.cbSize = sizeof (SHELLEXECUTEINFO);
		info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
		info.nShow = SW_SHOW;
		info.lpFile = L"explorer.exe";
		info.lpParameters = sr->buffer;

		if (ShellExecuteExW (&info))
		{
			status = S_OK;
		}
		else
		{
			status = HRESULT_FROM_WIN32 (NtLastError ());
		}
	}
	else
	{
		status = SHParseDisplayName (sr->buffer, NULL, &item, 0, NULL);

		if (SUCCEEDED (status))
		{
			status = SHOpenFolderAndSelectItems (item, 0, NULL, 0);

			CoTaskMemFree (item);
		}
	}

	if (string)
		_r_obj_dereference (string);

	return status;
}

//
// Strings
//

VOID _r_str_append (
	_Inout_updates_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ LPCWSTR string
)
{
	ULONG_PTR dest_length;

	if (buffer_length <= PR_SIZE_MAX_STRING_LENGTH)
	{
		dest_length = _r_str_getlength_ex (buffer, buffer_length + 1);

		_r_str_copy (buffer + dest_length, buffer_length - dest_length, string);
	}
}

VOID _r_str_appendformat (
	_Inout_updates_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	ULONG_PTR dest_length;

	if (buffer_length > PR_SIZE_MAX_STRING_LENGTH)
		return;

	dest_length = _r_str_getlength_ex (buffer, buffer_length + 1);

	va_start (arg_ptr, format);
	_r_str_printf_v (buffer + dest_length, buffer_length - dest_length, format, arg_ptr);
	va_end (arg_ptr);
}

FORCEINLINE LONG _r_str_compareleft (
	_In_ PCWSTR A,
	_In_ PCWSTR B
)
{
	for (; ; A++, B++)
	{
		if (!_r_str_isdigit (*A) && !_r_str_isdigit (*B))
		{
			return 0;
		}
		else if (!_r_str_isdigit (*A))
		{
			return -1;
		}
		else if (!_r_str_isdigit (*B))
		{
			return 1;
		}
		else if (*A < *B)
		{
			return -1;
		}
		else if (*A > *B)
		{
			return 1;
		}
	}

	return 0;
}

FORCEINLINE LONG _r_str_compareright (
	_In_ PCWSTR A,
	_In_ PCWSTR B
)
{
	LONG bias = 0;

	for (; ; A++, B++)
	{
		if (!_r_str_isdigit (*A) && !_r_str_isdigit (*B))
		{
			return bias;
		}
		else if (!_r_str_isdigit (*A))
		{
			return -1;
		}
		else if (!_r_str_isdigit (*B))
		{
			return 1;
		}
		else if (*A < *B)
		{
			if (bias == 0)
				bias = -1;
		}
		else if (*A > *B)
		{
			if (bias == 0)
				bias = 1;
		}
		else if (!*A && !*B)
		{
			return bias;
		}
	}

	return 0;
}

LONG _r_str_compare (
	_In_ LPCWSTR string1,
	_In_ LPCWSTR string2,
	_In_ BOOLEAN is_ignorecase
)
{
	WCHAR ca;
	WCHAR cb;
	ULONG_PTR ai = 0;
	ULONG_PTR bi = 0;
	LONG result;

	while (TRUE)
	{
		ca = string1[ai];
		cb = string2[bi];

		// Skip over leading spaces or zeros
		while (ca == L' ')
			ca = string1[++ai];

		while (cb == L' ')
			cb = string2[++bi];

		// Process run of digits
		if (_r_str_isdigit (ca) && _r_str_isdigit (cb))
		{
			if (ca == L'0' || cb == L'0')
			{
				result = _r_str_compareleft (string1 + ai, string2 + bi);

				if (result != 0)
					return result;
			}
			else
			{
				result = _r_str_compareright (string1 + ai, string2 + bi);

				if (result != 0)
					return result;
			}
		}

		if (!ca && !cb)
		{
			// Strings are considered the same
			return 0;
		}

		if (is_ignorecase)
		{
			ca = _r_str_upper (ca);
			cb = _r_str_upper (cb);
		}

		if (ca < cb)
		{
			return -1;
		}
		else if (ca > cb)
		{
			return 1;
		}

		ai += 1;
		bi += 1;
	}
}

VOID _r_str_copy (
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ LPCWSTR string
)
{
	if (buffer_length && buffer_length <= PR_SIZE_MAX_STRING_LENGTH)
	{
		while (buffer_length && (*string != UNICODE_NULL))
		{
			*buffer++ = *string++;
			buffer_length -= 1;
		}

		if (!buffer_length)
			buffer -= 1; // truncate buffer
	}

	*buffer = UNICODE_NULL;
}

VOID _r_str_copystring (
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ PR_STRINGREF string
)
{
	ULONG_PTR length;

	length = _r_str_getlength2 (string) + 1;

	if (length > buffer_length)
		length = buffer_length;

	_r_str_copy (buffer, length, string->buffer);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_environmentexpandstring (
	_In_opt_ PVOID environment,
	_In_ PR_STRINGREF name,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING output_string;
	UNICODE_STRING input_string;
	PR_STRING buffer_string;
	ULONG buffer_length;
	NTSTATUS status;

	_r_obj_initializeunicodestring2 (&input_string, name);

	buffer_length = 256 * sizeof (WCHAR);
	buffer_string = _r_obj_createstring_ex (NULL, buffer_length);

	_r_obj_initializeunicodestring_ex (&output_string, buffer_string->buffer, 0, buffer_length);

	status = RtlExpandEnvironmentStrings_U (environment, &input_string, &output_string, &buffer_length);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		_r_obj_movereference (&buffer_string, _r_obj_createstring_ex (NULL, buffer_length));

		_r_obj_initializeunicodestring_ex (&output_string, buffer_string->buffer, 0, buffer_length);

		status = RtlExpandEnvironmentStrings_U (environment, &input_string, &output_string, &buffer_length);
	}

	if (NT_SUCCESS (status))
	{
		_r_str_setlength (&buffer_string->sr, output_string.Length); // terminate

		*out_buffer = buffer_string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (buffer_string);
	}

	return status;
}

PR_STRING _r_str_environmentunexpandstring (
	_In_ LPCWSTR string
)
{
	PR_STRING buffer;
	ULONG length = 512;

	buffer = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	if (PathUnExpandEnvStringsW (string, buffer->buffer, length))
	{
		_r_str_trimtonullterminator (&buffer->sr);
	}
	else
	{
		_r_obj_movereference (&buffer, _r_obj_createstring (string));
	}

	return buffer;
}

_Success_ (return != SIZE_MAX)
ULONG_PTR _r_str_findchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	ULONG_PTR length;
	WCHAR chr;

	if (!string->length)
		return SIZE_MAX;

	buffer = string->buffer;
	length = _r_str_getlength2 (string);

	if (is_ignorecase)
		character = _r_str_lower (character);

	do
	{
		if (is_ignorecase)
		{
			chr = _r_str_lower (*buffer);
		}
		else
		{
			chr = *buffer;
		}

		if (chr == character)
			return _r_str_getlength2 (string) - length;

		buffer += 1;
	}
	while (--length);

	return SIZE_MAX;
}

_Success_ (return != SIZE_MAX)
ULONG_PTR _r_str_findlastchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer;
	ULONG_PTR length;
	WCHAR chr;

	if (!string->length)
		return SIZE_MAX;

	buffer = PTR_ADD_OFFSET (string->buffer, string->length);
	length = _r_str_getlength2 (string);

	if (is_ignorecase)
		character = _r_str_lower (character);

	buffer -= 1;

	do
	{
		if (is_ignorecase)
		{
			chr = _r_str_lower (*buffer);
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
ULONG_PTR _r_str_findstring (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF sub_string,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	ULONG_PTR length1;
	ULONG_PTR length2;
	WCHAR chr1;
	WCHAR chr2;

	length1 = _r_str_getlength2 (string);
	length2 = _r_str_getlength2 (sub_string);

	// Can't be a substring if it's bigger than the first string.
	if (length2 > length1)
		return SIZE_MAX;

	// We always get a match if the substring is zero-length.
	if (length2 == 0)
		return 0;

	_r_obj_initializestringref_ex (&sr1, string->buffer, sub_string->length - sizeof (WCHAR));
	_r_obj_initializestringref_ex (&sr2, sub_string->buffer, sub_string->length - sizeof (WCHAR));

	if (is_ignorecase)
	{
		chr2 = _r_str_lower (*sr2.buffer++);
	}
	else
	{
		chr2 = *sr2.buffer++;
	}

	for (ULONG_PTR i = length1 - length2 + 1; i != 0; i--)
	{
		if (is_ignorecase)
		{
			chr1 = _r_str_lower (*sr1.buffer++);
		}
		else
		{
			chr1 = *sr1.buffer++;
		}

		if (chr2 == chr1)
		{
			if (_r_str_isequal (&sr1, &sr2, is_ignorecase))
				return (ULONG_PTR)(sr1.buffer - string->buffer - 1);
		}
	}

	return SIZE_MAX;
}

_Success_ (return != SIZE_MAX)
ULONG_PTR _r_str_findstring2 (
	_In_ PR_STRINGREF string,
	_In_ LPWSTR sub_string,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringref (&sr, sub_string);

	return _r_str_findstring (string, &sr, is_ignorecase);
}

PR_STRING _r_str_formatversion (
	_In_ PR_STRING string
)
{
	SYSTEMTIME st;
	LONG64 unixtime;

	if (_r_str_isnumeric (&string->sr))
	{
		unixtime = _r_str_tolong64 (&string->sr);

		_r_unixtime_to_systemtime (unixtime, &st);

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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_fromguid (
	_In_ LPGUID guid,
	_In_ BOOLEAN is_uppercase,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING us;
	PR_STRING string;
	NTSTATUS status;

	status = RtlStringFromGUID (guid, &us);

	if (NT_SUCCESS (status))
	{
		string = _r_obj_createstring3 (&us);

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
	_In_ ULONG_PTR length,
	_In_ BOOLEAN is_uppercase
)
{
	static CHAR integer_char_table[] =
		"0123456789" // 0 - 9
		"abcdefghijklmnopqrstuvwxyz" // 10 - 35
		" !\"#$%&'()*+,-./" // 36 - 51
		":;<=>?@" // 52 - 58
		"[\\]^_`" // 59 - 64
		"{|}~"; // 65 - 68

	static CHAR integer_char_upper_table[] =
		"0123456789" // 0 - 9
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 10 - 35
		" !\"#$%&'()*+,-./" // 36 - 51
		":;<=>?@" // 52 - 58
		"[\\]^_`" // 59 - 64
		"{|}~"; // 65 - 68

	C_ASSERT (RTL_NUMBER_OF (integer_char_table) == RTL_NUMBER_OF (integer_char_upper_table));

	PR_STRING string;
	PCHAR table;

	table = is_uppercase ? integer_char_upper_table : integer_char_table;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR) * 2);

	for (ULONG_PTR i = 0; i < length; i++)
	{
		string->buffer[i * sizeof (WCHAR)] = table[buffer[i] >> 4];
		string->buffer[i * sizeof (WCHAR) + 1] = table[buffer[i] & 0xF];
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
	BOOL is_success;
	ULONG status;

	is_success = ConvertSecurityDescriptorToStringSecurityDescriptorW (security_descriptor, SDDL_REVISION, security_information, &security_string, &length);

	if (is_success)
	{
		status = ERROR_SUCCESS;

		string = _r_obj_createstring_ex (security_string, length * sizeof (WCHAR));

		*out_buffer = string;

		LocalFree (security_string);
	}
	else
	{
		status = NtLastError ();

		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_fromsid (
	_In_ PSID sid,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING us;
	PR_STRING string;
	NTSTATUS status;

	string = _r_obj_createstring_ex (NULL, SECURITY_MAX_SID_STRING_CHARACTERS * sizeof (WCHAR));

	_r_obj_initializeunicodestring2 (&us, &string->sr);

	status = RtlConvertSidToUnicodeString (&us, sid, FALSE);

	if (NT_SUCCESS (status))
	{
		*out_buffer = string;

		_r_str_setlength (&string->sr, us.Length); // terminate
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (string);
	}

	return status;
}

VOID _r_str_generaterandom (
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ ULONG_PTR buffer_length,
	_In_ BOOLEAN is_uppercase
)
{
	WCHAR chr;

	chr = is_uppercase ? L'A' : L'a';

	for (ULONG_PTR i = 0; i < buffer_length - 1; i++)
	{
		buffer[i] = chr + (_r_math_getrandom () % 26);
	}

	buffer[buffer_length - 1] = UNICODE_NULL;
}

ULONG _r_str_gethash (
	_In_ LPWSTR string,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringref (&sr, string);

	return _r_str_gethash2 (&sr, is_ignorecase);
}

ULONG _r_str_gethash2 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	return _r_str_x65599 (string, is_ignorecase);
}

ULONG_PTR _r_str_getbytelength (
	_In_ LPCSTR string
)
{
	return _r_str_getbytelength_ex (string, PR_SIZE_MAX_STRING_LENGTH);
}

ULONG_PTR _r_str_getbytelength2 (
	_In_ PR_BYTE string
)
{
	return string->length;
}

ULONG_PTR _r_str_getbytelength3 (
	_In_ PR_BYTEREF string
)
{
	return string->length;
}

ULONG_PTR _r_str_getbytelength_ex (
	_In_reads_or_z_ (max_length) LPCSTR string,
	_In_ _In_range_ (<= , PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR max_length
)
{
	ULONG_PTR original_length;

	original_length = max_length;

	while (max_length && (*string != ANSI_NULL))
	{
		string += 1;

		max_length -= 1;
	}

	if (original_length == 0)
		return 0;

	return original_length - max_length;
}

ULONG_PTR _r_str_getlength_ex (
	_In_reads_or_z_ (max_length) LPCWSTR string,
	_In_ _In_range_ (<= , PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR max_length
)
{
	LPCWSTR p;
	ULONG_PTR i;
	ULONG unaligned;
	ULONG index;
	ULONG mask;
	R_INT128 b;
	R_INT128 z;

	// SSE2
	if (USER_SHARED_DATA->ProcessorFeatures[PF_XMMI64_INSTRUCTIONS_AVAILABLE])
	{
		p = (LPCWSTR)((ULONG_PTR)string & ~0xE); // string should be 2 byte aligned
		unaligned = PtrToUlong (string) & 0xF;

		z = _r_intrinsics_setzeroint128 ();

		if (unaligned != 0)
		{
			b = _r_intrinsics_loadint128 ((PLONG)p);
			b = _r_intrinsics_compareint128by16 (b, z);
			mask = _r_intrinsics_movemaskint128by8 (b) >> unaligned;

			if (_BitScanForward (&index, mask))
				return index / sizeof (WCHAR);

			p += 16 / sizeof (WCHAR);
		}

		while (TRUE)
		{
			b = _r_intrinsics_loadint128u ((PLONG)p);
			b = _r_intrinsics_compareint128by16 (b, z);
			mask = _r_intrinsics_movemaskint128by8 (b);

			if (_BitScanForward (&index, mask))
				return (ULONG_PTR)(p - string) + index / sizeof (WCHAR);

			p += 16 / sizeof (WCHAR);
		}
	}
	else
	{
		for (i = 0; i < max_length; i++)
		{
			if (string[i] == UNICODE_NULL)
				break;
		}

		return i;
	}
}

BOOLEAN _r_str_isequal (
	_In_ PR_STRINGREF string1,
	_In_ PR_STRINGREF string2,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR buffer1;
	LPWSTR buffer2;
	ULONG_PTR length1;
	ULONG_PTR length2;
	ULONG_PTR length;
	R_INT128 b1;
	R_INT128 b2;
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

	// SSE2
	if (USER_SHARED_DATA->ProcessorFeatures[PF_XMMI64_INSTRUCTIONS_AVAILABLE])
	{
		length = length1 / 16;

		if (length != 0)
		{
			do
			{
				b1 = _r_intrinsics_loadint128u ((PLONG)buffer1);
				b2 = _r_intrinsics_loadint128u ((PLONG)buffer2);
				b1 = _r_intrinsics_compareint128by32 (b1, b2);

				if (_r_intrinsics_movemaskint128by8 (b1) != 0xFFFF)
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
			if (is_ignorecase)
			{
				chr1 = _r_str_lower (*buffer1);
				chr2 = _r_str_lower (*buffer2);
			}
			else
			{
				chr1 = *buffer1;
				chr2 = *buffer2;
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
	_In_ LPWSTR string2,
	_In_ BOOLEAN is_ignorecase
)
{
	R_STRINGREF sr;

	_r_obj_initializestringref (&sr, string2);

	return _r_str_isequal (string1, &sr, is_ignorecase);
}

BOOLEAN _r_str_isnumeric (
	_In_ PR_STRINGREF string
)
{
	ULONG_PTR length;

	if (!string->length)
		return FALSE;

	length = _r_str_getlength2 (string);

	for (ULONG_PTR i = 0; i < length; i++)
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
		PTR_ADD_OFFSET (string->buffer, string->length - suffix->length),
		suffix->length
	);

	return _r_str_isequal (&sr, suffix, is_ignorecase);
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
				if (is_ignorecase)
				{
					if (_r_str_lower (*s) != _r_str_lower (*p))
						goto StarCheck;
				}
				else
				{
					if (*s != *p)
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
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;

	if (buffer_length > PR_SIZE_MAX_STRING_LENGTH)
	{
		*buffer = UNICODE_NULL;

		return;
	}

	va_start (arg_ptr, format);
	_r_str_printf_v (buffer, buffer_length, format, arg_ptr);
	va_end (arg_ptr);
}

VOID _r_str_printf_v (
	_Out_writes_z_ (buffer_length) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) ULONG_PTR buffer_length,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
)
{
	ULONG_PTR max_length;
	ULONG_PTR format_size;

	if (buffer_length > PR_SIZE_MAX_STRING_LENGTH)
	{
		*buffer = UNICODE_NULL;

		return;
	}

	// leave the last space for the null terminator
	max_length = buffer_length - 1;

#pragma warning(push)
#pragma warning(disable: _UCRT_DISABLED_WARNINGS)
	format_size = _vsnwprintf_l (buffer, max_length, format, NULL, arg_ptr);
#pragma warning(pop)

	if (format_size == -1 || format_size >= max_length)
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
	ULONG_PTR length;

	if (!string->length)
		return;

	length = _r_str_getlength2 (string);

	for (ULONG_PTR i = 0; i < length; i++)
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
	ULONG_PTR index;

	index = _r_str_findchar (string, separator, FALSE);

	if (index == SIZE_MAX)
	{
		_r_obj_initializestringref2 (first_part, string);
		_r_obj_initializestringrefempty (second_part);

		return FALSE;
	}

	// get a copy of the input because first_part/second_part may alias string
	_r_obj_initializestringref2 (&input, string);

	_r_obj_initializestringref_ex (first_part, input.buffer, index * sizeof (WCHAR));

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
	ULONG_PTR index;

	index = _r_str_findlastchar (string, separator, FALSE);

	if (index == SIZE_MAX)
	{
		_r_obj_initializestringref2 (first_part, string);
		_r_obj_initializestringrefempty (second_part);

		return FALSE;
	}

	// get a copy of the input because first_part/second_part may alias string
	_r_obj_initializestringref2 (&input, string);

	_r_obj_initializestringref_ex (first_part, input.buffer, index * sizeof (WCHAR));

	_r_obj_initializestringref_ex (
		second_part,
		PTR_ADD_OFFSET (input.buffer, index * sizeof (WCHAR) + sizeof (UNICODE_NULL)),
		input.length - index * sizeof (WCHAR) - sizeof (UNICODE_NULL)
	);

	return TRUE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_toguid (
	_In_ PR_STRINGREF string,
	_Out_ LPGUID guid
)
{
	UNICODE_STRING us;
	NTSTATUS status;

	if (!_r_obj_initializeunicodestring2 (&us, string))
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

	if (!ConvertStringSidToSidW (sid_string->buffer, &sid))
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
	R_STRINGREF value = PR_STRINGREF_INIT (L"true");

	if (_r_str_tolong (string) != 0)
		return TRUE;

	return _r_str_isequal (string, &value, TRUE);
}

_Success_ (return != 0)
LONG _r_str_tolong (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	LONG number;

	if (_r_str_tointeger64 (string, 0, NULL, &value))
	{
		if (SUCCEEDED (LongLongToLong (value, &number)))
			return number;
	}

	return 0;
}

_Success_ (return != 0)
LONG64 _r_str_tolong64 (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;

	if (_r_str_tointeger64 (string, 0, NULL, &value))
		return value;

	return 0;
}

_Success_ (return != 0)
ULONG _r_str_toulong (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	ULONG number;

	if (_r_str_tointeger64 (string, 0, NULL, &value))
	{
		if (SUCCEEDED (LongLongToULong (value, &number)))
			return number;
	}

	return 0;
}

_Success_ (return != 0)
ULONG64 _r_str_toulong64 (
	_In_ PR_STRINGREF string
)
{
	LONG64 value;
	ULONG64 number;

	if (_r_str_tointeger64 (string, 0, NULL, &value))
	{
		if (SUCCEEDED (LongLongToULongLong (value, &number)))
			return number;
	}

	return 0;
}

_Success_ (return)
BOOLEAN _r_str_tointeger64 (
	_In_ PR_STRINGREF string,
	_In_opt_ ULONG base,
	_Out_opt_ PULONG new_base_ptr,
	_Out_ PLONG64 integer_ptr
)
{
	R_STRINGREF sr;
	ULONG64 result;
	ULONG base_used;
	BOOLEAN is_negative = FALSE;
	BOOLEAN is_valid;

	*integer_ptr = 0;

	if (new_base_ptr)
		*new_base_ptr = 0;

	_r_obj_initializestringref2 (&sr, string);

	if (sr.length != 0 && (sr.buffer[0] == L'-' || sr.buffer[0] == L'+'))
	{
		if (sr.buffer[0] == L'-')
			is_negative = TRUE;

		_r_str_skiplength (&sr, sizeof (WCHAR));
	}

	// If the caller specified a base, don't perform any additional processing.
	if (base)
	{
		base_used = base;
	}
	else
	{
		base_used = 10;

		if (sr.length >= 2 * sizeof (WCHAR) && sr.buffer[0] == L'0')
		{
			switch (_r_str_lower (sr.buffer[1]))
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
				_r_str_skiplength (&sr, 2 * sizeof (WCHAR));
		}
	}

	is_valid = _r_str_touinteger64 (&sr, base_used, &result);

	*integer_ptr = is_negative ? -(LONG64)result : result;

	if (new_base_ptr)
		*new_base_ptr = base_used;

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
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, // 'A' - 'Z'
		59, 60, 61, 62, 63, 64, // '[' - '`'
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, // 'a' - 'z'
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

	ULONG64 result = 0;
	ULONG_PTR length;
	ULONG value;
	BOOLEAN is_valid = TRUE;

	length = _r_str_getlength2 (string);

	for (ULONG_PTR i = 0; i < length; i++)
	{
		value = char_to_integer[(UCHAR)string->buffer[i]];

		if (value < base)
		{
			result = result * base + value;
		}
		else
		{
			is_valid = FALSE;
		}
	}

	*integer_ptr = result;

	return is_valid;
}

VOID _r_str_tolower (
	_Inout_ PR_STRINGREF string
)
{
	ULONG_PTR length;

	if (!string->length)
		return;

	length = _r_str_getlength2 (string);

	for (ULONG_PTR i = 0; i < length; i++)
	{
		string->buffer[i] = _r_str_lower (string->buffer[i]);
	}
}

VOID _r_str_toupper (
	_Inout_ PR_STRINGREF string
)
{
	ULONG_PTR length;

	if (!string->length)
		return;

	length = _r_str_getlength2 (string);

	for (ULONG_PTR i = 0; i < length; i++)
	{
		string->buffer[i] = _r_str_upper (string->buffer[i]);
	}
}

VOID _r_str_trimstring (
	_Inout_ PR_STRINGREF string,
	_In_ PR_STRINGREF charset,
	_In_opt_ ULONG flags
)
{
	LPCWSTR buffer;
	WCHAR chr;
	ULONG_PTR trim_count;
	ULONG_PTR length;
	BOOLEAN chr_table_complete = TRUE;
	BOOLEAN chr_table[256] = {0};

	if (_r_obj_isstringempty2 (string) || _r_obj_isstringempty2 (charset))
		return;

	if (charset->length == sizeof (WCHAR))
	{
		chr = charset->buffer[0];

		if (!(flags & PR_TRIM_END_ONLY))
		{
			trim_count = 0;

			buffer = string->buffer;
			length = _r_str_getlength2 (string);

			while (length-- != 0)
			{
				if (*buffer++ != chr)
					break;

				trim_count += 1;
			}

			if (trim_count)
				_r_str_skiplength (string, trim_count * sizeof (WCHAR));
		}

		if (!(flags & PR_TRIM_START_ONLY))
		{
			trim_count = 0;

			buffer = PTR_ADD_OFFSET (string->buffer, string->length - sizeof (WCHAR));
			length = _r_str_getlength2 (string);

			while (length-- != 0)
			{
				if (*buffer-- != chr)
					break;

				trim_count += 1;
			}

			if (trim_count)
				string->length -= trim_count * sizeof (WCHAR);
		}

		_r_str_writenullterminator (string);

		return;
	}

	// Build the character set lookup table.
	for (ULONG_PTR i = 0; i < _r_str_getlength2 (charset); i++)
	{
		chr = charset->buffer[i];
		chr_table[chr & 0xFF] = TRUE;

		if (chr >= RTL_NUMBER_OF (chr_table))
			chr_table_complete = FALSE;
	}

	// Trim the string.
	if (!(flags & PR_TRIM_END_ONLY))
	{
		trim_count = 0;

		buffer = string->buffer;
		length = _r_str_getlength2 (string);

		while (length-- != 0)
		{
			chr = *buffer++;

			if (!chr_table[chr & 0xFF])
				break;

			if (!chr_table_complete)
			{
				for (ULONG_PTR i = 0; i < _r_str_getlength2 (charset); i++)
				{
					if (charset->buffer[i] == chr)
						goto CharFound;
				}

				break;
			}

CharFound:

			trim_count += 1;
		}

		if (trim_count)
			_r_str_skiplength (string, trim_count * sizeof (WCHAR));
	}

	if (!(flags & PR_TRIM_START_ONLY))
	{
		trim_count = 0;

		buffer = PTR_ADD_OFFSET (string->buffer, string->length - sizeof (WCHAR));
		length = _r_str_getlength2 (string);

		while (length-- != 0)
		{
			chr = *buffer--;

			if (!chr_table[chr & 0xFF])
				break;

			if (!chr_table_complete)
			{
				for (ULONG_PTR i = 0; i < _r_str_getlength2 (charset); i++)
				{
					if (charset->buffer[i] == chr)
						goto CharFound2;
				}

				break;
			}

CharFound2:
			trim_count += 1;
		}

		if (trim_count)
			string->length -= trim_count * sizeof (WCHAR);
	}

	_r_str_writenullterminator (string);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_multibyte2unicode (
	_In_ PR_BYTEREF string,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING out_string;
	ULONG out_length;
	NTSTATUS status;

	status = RtlMultiByteToUnicodeSize (&out_length, string->buffer, (ULONG)string->length);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	out_string = _r_obj_createstring_ex (NULL, out_length);

	status = RtlMultiByteToUnicodeN (out_string->buffer, (ULONG)out_string->length, NULL, string->buffer, (ULONG)string->length);

	if (NT_SUCCESS (status))
	{
		// RtlMultiByteToUnicodeN doesn't null terminate the string.
		*(LPWSTR)PTR_ADD_OFFSET (out_string->buffer, out_length) = UNICODE_NULL;

		*out_buffer = out_string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (out_string);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_unicode2multibyte (
	_In_ PR_STRINGREF string,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE out_string;
	ULONG out_length;
	NTSTATUS status;

	status = RtlUnicodeToMultiByteSize (&out_length, string->buffer, (ULONG)string->length);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	out_string = _r_obj_createbyte_ex (NULL, out_length);

	status = RtlUnicodeToMultiByteN (out_string->buffer, (ULONG)out_string->length, NULL, string->buffer, (ULONG)string->length);

	if (NT_SUCCESS (status))
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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_utf8toutf16 (
	_In_ PR_BYTEREF string,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING out_string;
	ULONG out_length;
	NTSTATUS status;

	status = RtlUTF8ToUnicodeN (NULL, 0, &out_length, string->buffer, (ULONG)string->length);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	out_string = _r_obj_createstring_ex (NULL, out_length);

	status = RtlUTF8ToUnicodeN (out_string->buffer, (ULONG)out_string->length, NULL, string->buffer, (ULONG)string->length);

	if (NT_SUCCESS (status))
	{
		// RtlUTF8ToUnicodeN doesn't null terminate the string.
		*(LPWSTR)PTR_ADD_OFFSET (out_string->buffer, out_length) = UNICODE_NULL;

		*out_buffer = out_string;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (out_string);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_str_utf16toutf8 (
	_In_ PR_STRINGREF string,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE out_string;
	ULONG out_length;
	NTSTATUS status;

	status = RtlUnicodeToUTF8N (NULL, 0, &out_length, string->buffer, (ULONG)string->length);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	out_string = _r_obj_createbyte_ex (NULL, out_length);

	status = RtlUnicodeToUTF8N (out_string->buffer, (ULONG)out_string->length, NULL, string->buffer, (ULONG)string->length);

	if (NT_SUCCESS (status))
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

VOID _r_str_reversestring (
	_Inout_ PR_STRINGREF string
)
{
	WCHAR chr;

	for (ULONG_PTR i = 0, j = _r_str_getlength2 (string) - 1; i <= j; i++, j--)
	{
		chr = string->buffer[i];

		string->buffer[i] = string->buffer[j];
		string->buffer[j] = chr;
	}
}

VOID _r_str_setlength_ex (
	_Inout_ PR_STRINGREF string,
	_In_ ULONG_PTR new_length,
	_In_ ULONG_PTR allocated_length
)
{
	if (allocated_length < new_length)
		new_length = allocated_length;

	if (new_length & 0x01)
		new_length += 1;

	string->length = new_length;

	_r_str_writenullterminator (string); // terminate
}

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (
	_In_ PR_STRINGREF string,
	_In_ WCHAR key_delimeter,
	_In_ WCHAR value_delimeter
)
{
	R_STRINGREF whitespace = PR_STRINGREF_INIT (L"\r\n ");
	R_STRINGREF remaining_part;
	R_STRINGREF values_part;
	R_STRINGREF key_part;
	R_STRINGREF value_part;
	PR_HASHTABLE hashtable;
	PR_STRING value;
	ULONG_PTR hash_code;

	_r_obj_initializestringref2 (&remaining_part, string);

	hashtable = _r_obj_createhashtablepointer (4);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, key_delimeter, &values_part, &remaining_part);

		_r_str_trimstring (&values_part, &whitespace, 0);

		if (_r_str_splitatchar (&values_part, value_delimeter, &key_part, &value_part))
		{
			// trim key string whitespaces
			_r_str_trimstring (&key_part, &whitespace, 0);

			hash_code = _r_str_gethash2 (&key_part, TRUE);

			if (hash_code)
			{
				if (!_r_obj_findhashtable (hashtable, hash_code))
				{
					// trim value string whitespaces
					_r_str_trimstring (&value_part, &whitespace, 0);

					value = value_part.length ? _r_obj_createstring2 (&value_part) : NULL;

					_r_obj_addhashtablepointer (hashtable, hash_code, value);
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
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
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

LONG _r_str_versioncompare (
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

ULONG _r_str_x65599 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
)
{
	LPWSTR end_buffer;
	ULONG hash_code = 0;

	if (!string->length)
		return 0;

	end_buffer = string->buffer + _r_str_getlength2 (string);

	// This is the fastest implementation (copied from ReactOS) (dmex)
	if (is_ignorecase)
	{
		for (LPWSTR buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			hash_code = ((65599 * (hash_code)) + (ULONG)(((*buffer) >= L'a' && (*buffer) <= L'z') ? (*buffer) - L'a' + L'A' : (*buffer)));
		}
	}
	else
	{
		for (LPWSTR buffer = string->buffer; buffer != end_buffer; buffer++)
		{
			hash_code = ((65599 * (hash_code)) + (ULONG)(*buffer));
		}
	}

	return hash_code;
}

//
// Performance
//

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

LONG64 _r_perf_querycounter ()
{
	LARGE_INTEGER counter = {0};

	// win7+
	if (!RtlQueryPerformanceCounter (&counter))
		return 0;

	return counter.QuadPart;
}

LONG64 _r_perf_queryfrequency ()
{
	LARGE_INTEGER frequency = {0};

	// win7+
	if (!RtlQueryPerformanceFrequency (&frequency))
		return 0;

	return frequency.QuadPart;
};

//
// System information
//

BOOLEAN _r_sys_iswine ()
{
	PVOID procedure;
	PVOID hntdll;
	NTSTATUS status;

	status = _r_sys_loadlibrary2 (L"ntdll.dll", 0, &hntdll);

	if (!NT_SUCCESS (status))
		return FALSE;

	procedure = _r_sys_getprocaddress (hntdll, "wine_get_version", 0);

	_r_sys_freelibrary (hntdll);

	return (procedure != NULL);
}

BOOLEAN _r_sys_iswow64 ()
{
	ULONG_PTR iswow64 = 0;
	NTSTATUS status;

	status = NtQueryInformationProcess (NtCurrentProcess (), ProcessWow64Information, &iswow64, sizeof (ULONG_PTR), NULL);

	if (NT_SUCCESS (status))
		return !!iswow64;

	return FALSE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_formatmessage (
	_In_ ULONG error_code,
	_In_ PVOID hinst,
	_In_opt_ ULONG lang_id,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PMESSAGE_RESOURCE_ENTRY entry;
	PR_STRING string;
	R_BYTEREF bytes;
	LCID locale_id;
	NTSTATUS status;

	// 11 means RT_MESSAGETABLE
	status = RtlFindMessage (hinst, 0xB, lang_id, error_code, &entry);

	// Try using the system LANGID.
	if (!NT_SUCCESS (status))
	{
		_r_locale_getlcid (FALSE, &locale_id);

		status = RtlFindMessage (hinst, 0xB, locale_id, error_code, &entry);
	}

	// Try using U.S. English.
	if (!NT_SUCCESS (status))
		status = RtlFindMessage (hinst, 0xB, MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), error_code, &entry);

	if (NT_SUCCESS (status))
	{
		if (entry->Flags & MESSAGE_RESOURCE_UNICODE)
		{
			string = _r_obj_createstring ((LPCWSTR)entry->Text);
		}
		else if (entry->Flags & MESSAGE_RESOURCE_UTF8)
		{
			_r_obj_initializebyteref_ex (&bytes, entry->Text, entry->Length);

			_r_str_utf8toutf16 (&bytes, &string);
		}
		else
		{
			_r_obj_initializebyteref_ex (&bytes, entry->Text, entry->Length);

			_r_str_multibyte2unicode (&bytes, &string);
		}

		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getbinarytype (
	_In_ PR_STRINGREF path,
	_Inout_ PULONG out_buffer
)
{
	SECTION_IMAGE_INFORMATION image_info = {0};
	HANDLE hsection;
	HANDLE hfile;
	NTSTATUS status;

	status = _r_fs_openfile (path, FILE_READ_DATA | FILE_EXECUTE, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, FALSE, &hfile);

	if (!NT_SUCCESS (status))
		return status;

	status = NtCreateSection (&hsection, SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_EXECUTE, NULL, NULL, PAGE_EXECUTE, SEC_IMAGE, hfile);

	NtClose (hfile);

	if (!NT_SUCCESS (status))
		return status;

	status = NtQuerySection (hsection, SectionImageInformation, &image_info, sizeof (image_info), NULL);

	NtClose (hsection);

	if (NT_SUCCESS (status))
	{
		switch (image_info.Machine)
		{
			case IMAGE_FILE_MACHINE_I386:
			{
				*out_buffer = SCS_32BIT_BINARY;

				break;
			}

			case IMAGE_FILE_MACHINE_AMD64:
			case IMAGE_FILE_MACHINE_IA64:
			{
				*out_buffer = SCS_64BIT_BINARY;

				break;
			}
		}
	}
	else
	{
		switch (status)
		{
			case STATUS_INVALID_IMAGE_PROTECT:
			{
				*out_buffer = SCS_DOS_BINARY;

				return STATUS_SUCCESS;
			}

			case STATUS_INVALID_IMAGE_NOT_MZ:
			{
				*out_buffer = SCS_DOS_BINARY;

				return STATUS_SUCCESS;
			}

			case STATUS_INVALID_IMAGE_WIN_16:
			{
				*out_buffer = SCS_WOW_BINARY;

				return STATUS_SUCCESS;
			}

			case STATUS_INVALID_IMAGE_WIN_32:
			{
				*out_buffer = SCS_32BIT_BINARY;

				return STATUS_SUCCESS;
			}

			case STATUS_INVALID_IMAGE_WIN_64:
			{
				*out_buffer = SCS_64BIT_BINARY;

				return STATUS_SUCCESS;
			}
		}
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getcomputername (
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING string = NULL;
	HANDLE hkey;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", 0, KEY_READ, &hkey);

	if (NT_SUCCESS (status))
	{
		status = _r_reg_querystring (hkey, L"ComputerName", &string, NULL);

		NtClose (hkey);
	}

	if (NT_SUCCESS (status))
	{
		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

PR_TOKEN_ATTRIBUTES _r_sys_getcurrenttoken ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_TOKEN_ATTRIBUTES attributes = {0};

	TOKEN_ELEVATION_TYPE elevation_type = 0;
	TOKEN_ELEVATION elevation = {0};
	PTOKEN_USER token_user = NULL;
	HANDLE token_handle = NULL;
	ULONG return_length;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
		{
			token_handle = NtCurrentProcessToken ();
		}
		else
		{
			NtOpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &token_handle);
		}

		status = NtQueryInformationToken (token_handle, TokenElevation, &elevation, sizeof (TOKEN_ELEVATION), &return_length);

		if (NT_SUCCESS (status))
			attributes.is_elevated = !!elevation.TokenIsElevated;

		status = NtQueryInformationToken (token_handle, TokenElevationType, &elevation_type, sizeof (TOKEN_ELEVATION_TYPE), &return_length);

		attributes.elevation_type = NT_SUCCESS (status) ? elevation_type : TokenElevationTypeDefault;

		status = _r_sys_querytokeninformation (token_handle, TokenUser, &token_user);

		if (NT_SUCCESS (status))
		{
			attributes.token_sid = _r_mem_allocateandcopy (token_user->User.Sid, RtlLengthSid (token_user->User.Sid));

			_r_mem_free (token_user);
		}

		_r_initonce_end (&init_once);
	}

	return &attributes;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getenvironmentvariable (
	_In_opt_ PVOID environment,
	_In_ PR_STRINGREF name_sr,
	_Out_ PR_STRING_PTR out_buffer
)
{
	UNICODE_STRING value_us = {0};
	UNICODE_STRING name_us;
	NTSTATUS status;

	_r_obj_initializeunicodestring2 (&name_us, name_sr);

	value_us.Length = 0x100 * sizeof (WCHAR);
	value_us.MaximumLength = value_us.Length + sizeof (UNICODE_NULL);
	value_us.Buffer = _r_mem_allocate (value_us.MaximumLength);

	status = RtlQueryEnvironmentVariable_U (environment, &name_us, &value_us);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		value_us.MaximumLength = value_us.Length + sizeof (UNICODE_NULL) > UNICODE_STRING_MAX_BYTES ? value_us.Length : value_us.Length + sizeof (UNICODE_NULL);
		value_us.Buffer = _r_mem_reallocate (value_us.Buffer, value_us.MaximumLength);

		status = RtlQueryEnvironmentVariable_U (environment, &name_us, &value_us);
	}

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_createstring3 (&value_us);
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (value_us.Buffer);

	return status;
}

_Ret_maybenull_
PR_STRING _r_sys_getkernelfilename (
	_In_ BOOLEAN is_ntpathtodos
)
{
	UCHAR buffer[FIELD_OFFSET (RTL_PROCESS_MODULES, Modules) + sizeof (RTL_PROCESS_MODULE_INFORMATION)] = {0};
	PRTL_PROCESS_MODULES modules;
	R_BYTEREF br;
	PR_STRING string;
	ULONG length;
	NTSTATUS status;

	modules = (PRTL_PROCESS_MODULES)buffer;
	length = sizeof (buffer);

	status = NtQuerySystemInformation (SystemModuleInformation, modules, length, &length);

	if (status != STATUS_SUCCESS && status != STATUS_INFO_LENGTH_MISMATCH)
		return NULL;

	if (status == STATUS_SUCCESS && modules->NumberOfModules < 1)
		return NULL;

	_r_obj_initializebyteref (&br, modules->Modules[0].FullPathName);

	_r_str_utf8toutf16 (&br, &string);

	if (is_ntpathtodos)
		string = _r_path_dospathfromnt (&string->sr);

	return string;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getmemoryinfo (
	_Out_ PR_MEMORY_INFO out_buffer
)
{
	SYSTEM_PERFORMANCE_INFORMATION perf_info = {0};
	SYSTEM_BASIC_INFORMATION basic_info = {0};
	SYSTEM_FILECACHE_INFORMATION sfci = {0};
	PSYSTEM_PAGEFILE_INFORMATION page_file;
	PSYSTEM_PAGEFILE_INFORMATION pagefile;
	ULONG buffer_length = 0x200;
	ULONG attempts = 6;
	NTSTATUS status;

	RtlSecureZeroMemory (out_buffer, sizeof (R_MEMORY_INFO));

	// physical memory information
	status = NtQuerySystemInformation (SystemBasicInformation, &basic_info, sizeof (SYSTEM_BASIC_INFORMATION), NULL);

	if (NT_SUCCESS (status))
	{
		out_buffer->physical_memory.total_bytes = basic_info.NumberOfPhysicalPages;
		out_buffer->physical_memory.total_bytes *= basic_info.PageSize;
	}

	status = NtQuerySystemInformation (SystemPerformanceInformation, &perf_info, sizeof (SYSTEM_PERFORMANCE_INFORMATION), NULL);

	if (NT_SUCCESS (status))
	{
		out_buffer->physical_memory.free_bytes = perf_info.AvailablePages;
		out_buffer->physical_memory.free_bytes *= basic_info.PageSize;

		out_buffer->physical_memory.used_bytes = out_buffer->physical_memory.total_bytes - out_buffer->physical_memory.free_bytes;

		out_buffer->physical_memory.percent = _r_calc_percentof64 (out_buffer->physical_memory.used_bytes, out_buffer->physical_memory.total_bytes);
	}

	// file cache information
	status = NtQuerySystemInformation (SystemFileCacheInformation, &sfci, sizeof (SYSTEM_FILECACHE_INFORMATION), NULL);

	if (NT_SUCCESS (status))
	{
		out_buffer->system_cache.total_bytes = sfci.PeakSize;
		out_buffer->system_cache.free_bytes = (ULONG64)sfci.PeakSize - (ULONG64)sfci.CurrentSize;
		out_buffer->system_cache.used_bytes = sfci.CurrentSize;

		out_buffer->system_cache.percent = _r_calc_percentof64 (out_buffer->system_cache.used_bytes, out_buffer->system_cache.total_bytes);
	}

	// page file information
	page_file = _r_mem_allocate (buffer_length);

	do
	{
		status = NtQuerySystemInformation (SystemPageFileInformation, page_file, buffer_length, NULL);

		if (status != STATUS_INFO_LENGTH_MISMATCH)
			break;

		buffer_length *= 2;
		page_file = _r_mem_reallocate (page_file, buffer_length);
	}
	while (--attempts);

	if (NT_SUCCESS (status))
	{
		// caclulate all pagefile size
		pagefile = page_file->TotalSize ? page_file : NULL;

		while (pagefile)
		{
			out_buffer->page_file.total_bytes += UInt32x32To64 (pagefile->TotalSize, PAGE_SIZE);
			out_buffer->page_file.free_bytes += UInt32x32To64 (pagefile->TotalSize - pagefile->PeakUsage, PAGE_SIZE);
			out_buffer->page_file.used_bytes += UInt32x32To64 (pagefile->TotalInUse, PAGE_SIZE);

			pagefile = page_file->NextEntryOffset ? PTR_ADD_OFFSET ((page_file), page_file->NextEntryOffset) : NULL;
		}

		out_buffer->page_file.percent = _r_calc_percentof64 (out_buffer->page_file.used_bytes, out_buffer->page_file.total_bytes);
	}

	_r_mem_free (page_file);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getmodulehandle (
	_In_ LPWSTR lib_name,
	_Out_ PVOID_PTR out_buffer
)
{
	UNICODE_STRING us;
	PVOID dll_handle;
	NTSTATUS status;

	_r_obj_initializeunicodestring (&us, lib_name);

	status = LdrGetDllHandleEx (LDR_GET_DLL_HANDLE_EX_UNCHANGED_REFCOUNT, NULL, NULL, &us, &dll_handle);

	if (NT_SUCCESS (status))
	{
		*out_buffer = dll_handle;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getprocessorinformation (
	_Out_opt_ PUSHORT out_architecture,
	_Out_opt_ PUSHORT out_revision,
	_Out_opt_ PULONG out_features
)
{
	SYSTEM_PROCESSOR_INFORMATION cpu_info = {0};
	NTSTATUS status;

	status = NtQuerySystemInformation (SystemProcessorInformation, &cpu_info, sizeof (cpu_info), NULL);

	if (NT_SUCCESS (status))
	{
		if (out_architecture)
			*out_architecture = cpu_info.ProcessorArchitecture;

		if (out_revision)
			*out_revision = cpu_info.ProcessorRevision;

		if (out_features)
			*out_features = cpu_info.ProcessorFeatureBits;
	}
	else
	{
		if (out_architecture)
			*out_architecture = 0;

		if (out_revision)
			*out_revision = 0;

		if (out_features)
			*out_features = 0;
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
		_r_obj_initializestringref2 (path, &system_root);

		return;
	}

	_r_obj_initializestringref (&local_system_root, USER_SHARED_DATA->NtSystemRoot);

	// Make sure the system root string doesn't have a trailing backslash.
	if (local_system_root.buffer[_r_str_getlength2 (&local_system_root) - 1] == OBJ_NAME_PATH_SEPARATOR)
		local_system_root.length -= sizeof (WCHAR);

	_r_obj_initializestringref2 (path, &local_system_root);

	system_root.length = local_system_root.length;
	MemoryBarrier ();
	system_root.buffer = local_system_root.buffer;
}

PR_STRING _r_sys_getsystemdirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	R_STRINGREF system_root;
	R_STRINGREF system_path;

	if (_r_initonce_begin (&init_once))
	{
		_r_sys_getsystemroot (&system_root);

		_r_obj_initializestringref (&system_path, L"\\system32");

		cached_path = _r_obj_concatstringrefs (
			2,
			&system_root,
			&system_path
		);

		_r_initonce_end (&init_once);
	}

	return cached_path;
}

_Ret_maybenull_
PR_STRING _r_sys_gettempdirectory ()
{
	static R_STRINGREF userprofile_sr = PR_STRINGREF_INIT (L"USERPROFILE");
	static R_STRINGREF temp_sr = PR_STRINGREF_INIT (L"TEMP");
	static R_STRINGREF tmp_sr = PR_STRINGREF_INIT (L"TMP");

	PR_STRING string;
	NTSTATUS status;

	status = _r_sys_getenvironmentvariable (NULL, &tmp_sr, &string);

	if (NT_SUCCESS (status))
		return string;

	status = _r_sys_getenvironmentvariable (NULL, &temp_sr, &string);

	if (NT_SUCCESS (status))
		return string;

	status = _r_sys_getenvironmentvariable (NULL, &userprofile_sr, &string);

	if (NT_SUCCESS (status))
		return string;

	return NULL;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_gettimezoneinfo (
	_Out_ PRTL_TIME_ZONE_INFORMATION out_buffer
)
{
	NTSTATUS status;

	status = NtQuerySystemInformation (SystemCurrentTimeZoneInformation, out_buffer, sizeof (RTL_TIME_ZONE_INFORMATION), NULL);

	if (!NT_SUCCESS (status))
		RtlZeroMemory (out_buffer, sizeof (RTL_TIME_ZONE_INFORMATION));

	return status;
}

BOOLEAN _r_sys_getopt (
	_In_ LPCWSTR args,
	_In_ LPWSTR name,
	_Outptr_opt_result_maybenull_ PR_STRING_PTR out_value
)
{
	R_STRINGREF key_name;
	R_STRINGREF key_value;
	R_STRINGREF name_sr;
	LPWSTR* arga;
	LPWSTR buffer;
	ULONG_PTR option_length;
	INT numargs;
	BOOLEAN is_namefound = FALSE;

	if (out_value)
		*out_value = NULL;

	arga = CommandLineToArgvW (args, &numargs);

	if (!arga)
		return FALSE;

	_r_obj_initializestringref (&name_sr, name);

	for (INT i = 0; i < numargs; i++)
	{
		_r_obj_initializestringref (&key_name, arga[i]);

		if (key_name.length < (2 * sizeof (WCHAR)))
			continue;

		if (*key_name.buffer != L'/' && *key_name.buffer != L'-')
			continue;

		// -option
		_r_str_skiplength (&key_name, sizeof (WCHAR));

		// --long-option
		if (*key_name.buffer == L'-')
			_r_str_skiplength (&key_name, sizeof (WCHAR));

		// parse key name
		if (_r_str_isstartswith (&key_name, &name_sr, TRUE))
		{
			option_length = _r_str_getlength2 (&name_sr);

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
			if (key_name.buffer[option_length] == L':' || key_name.buffer[option_length] == L'=' || key_name.buffer[option_length] == L' ')
			{
				if (out_value)
				{
					_r_obj_initializestringref2 (&key_value, &key_name);

					_r_str_skiplength (&key_value, (option_length + 1) * sizeof (WCHAR));
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
				*out_value = _r_obj_createstring2 (&key_value);
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

	PVOID hkernel32;
	PR_STRING string;
	UINT32 length = 0;
	LONG status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"kernel32.dll", 0, &hkernel32);

		if (NT_SUCCESS (status))
		{
			// win81+
			_GetStagedPackagePathByFullName = (GSPPBF)_r_sys_getprocaddress (hkernel32, "GetStagedPackagePathByFullName", 0);

			_r_sys_freelibrary (hkernel32);
		}

		_r_initonce_end (&init_once);
	}

	if (!_GetStagedPackagePathByFullName)
		return ERROR_NOT_SUPPORTED;

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
	_In_ LPWSTR name,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	UNICODE_STRING service_name;
	ULONG sid_length = 0;
	PR_BYTE sid;
	NTSTATUS status;

	_r_obj_initializeunicodestring (&service_name, name);

	status = RtlCreateServiceSid (&service_name, NULL, &sid_length);

	if (status != STATUS_BUFFER_TOO_SMALL)
	{
		*out_buffer = NULL;

		return status;
	}

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

	return status;
}

_Success_ (return)
BOOLEAN _r_sys_getsessioninfo (
	_In_ WTS_INFO_CLASS info_class,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING string;
	LPWSTR buffer;
	ULONG length;

	if (!WTSQuerySessionInformationW (WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, info_class, &buffer, &length))
	{
		*out_buffer = NULL;

		return FALSE;
	}

	string = _r_obj_createstring_ex (buffer, length * sizeof (WCHAR));

	*out_buffer = string;

	WTSFreeMemory (buffer);

	return TRUE;
}

ULONG _r_sys_gettickcount ()
{
#if !defined(_WIN64)
	ULARGE_INTEGER tick_count = {0};
#endif // _WIN64

#if defined(_WIN64)

	return (ULONG)((USER_SHARED_DATA->TickCountQuad * USER_SHARED_DATA->TickCountMultiplier) >> 24);

#else

	while (TRUE)
	{
		tick_count.HighPart = (ULONG)USER_SHARED_DATA->TickCount.High1Time;
		tick_count.LowPart = USER_SHARED_DATA->TickCount.LowPart;

		if (tick_count.HighPart == (ULONG)USER_SHARED_DATA->TickCount.High2Time)
			break;

		YieldProcessor ();
	}

	return (ULONG)((UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) +
				   UInt32x32To64 ((tick_count.HighPart << 8) & 0xFFFFFFFF, USER_SHARED_DATA->TickCountMultiplier));

#endif // _WIN64
}

ULONG64 _r_sys_gettickcount64 ()
{
	ULARGE_INTEGER tick_count = {0};

#if defined(_WIN64)

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

#endif // _WIN64

	return (UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) + (UInt32x32To64 (tick_count.HighPart, USER_SHARED_DATA->TickCountMultiplier) << 8);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getusername (
	_In_ PSID sid,
	_In_ BOOLEAN is_withdomain,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PLSA_REFERENCED_DOMAIN_LIST domains = NULL;
	PLSA_TRANSLATED_NAME names = NULL;
	PLSA_TRUST_INFORMATION trust_info;
	LSA_OBJECT_ATTRIBUTES oa = {0};
	LSA_HANDLE policy_handle;
	R_STRINGBUILDER sb = {0};
	BOOLEAN is_hasdomain;
	BOOLEAN is_hasname;
	NTSTATUS status;

	InitializeObjectAttributes (&oa, NULL, 0, NULL, NULL);

	status = LsaOpenPolicy (NULL, &oa, POLICY_LOOKUP_NAMES, &policy_handle);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = LsaLookupSids (policy_handle, 1, &sid, &domains, &names);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	if (names[0].Use != SidTypeInvalid && names[0].Use != SidTypeUnknown)
	{
		_r_obj_initializestringbuilder (&sb, 256);

		is_hasdomain = is_withdomain && (domains && names[0].DomainIndex >= 0);
		is_hasname = (names[0].Name.Buffer != NULL);

		if (is_hasdomain)
		{
			trust_info = &domains->Domains[names[0].DomainIndex];

			if (trust_info->Name.Buffer)
			{
				_r_obj_appendstringbuilder3 (&sb, &trust_info->Name);

				if (is_hasname)
					_r_obj_appendstringbuilder (&sb, L"\\");
			}
		}

		if (is_hasname)
			_r_obj_appendstringbuilder3 (&sb, &names[0].Name);
	}
	else
	{
		status = STATUS_NONE_MAPPED;
	}

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

	if (domains)
		LsaFreeMemory (domains);

	if (names)
		LsaFreeMemory (names);

	if (policy_handle)
		LsaClose (policy_handle);

	return status;
}

ULONG _r_sys_getwindowsversion ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ULONG windows_version = WINDOWS_ANCIENT;

	RTL_OSVERSIONINFOEXW version_info = {0};
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		version_info.dwOSVersionInfoSize = sizeof (version_info);

		status = RtlGetVersion (&version_info);

		if (NT_SUCCESS (status))
		{
			if (version_info.dwMajorVersion == 6 && version_info.dwMinorVersion == 1)
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
				windows_version = WINDOWS_10_TH1; // earlier versions of windows 10 have 6.4 version number
			}
			else if (version_info.dwMajorVersion == 10 && version_info.dwMinorVersion == 0)
			{
				if (version_info.dwBuildNumber > 26100)
				{
					windows_version = WINDOWS_NEW;
				}
				else if (version_info.dwBuildNumber >= 26100)
				{
					windows_version = WINDOWS_11_24H2;
				}
				else if (version_info.dwBuildNumber >= 22631)
				{
					windows_version = WINDOWS_11_23H2;
				}
				else if (version_info.dwBuildNumber >= 22621)
				{
					windows_version = WINDOWS_11_22H2;
				}
				else if (version_info.dwBuildNumber >= 22000)
				{
					windows_version = WINDOWS_11_21H2;
				}
				else if (version_info.dwBuildNumber >= 19045)
				{
					windows_version = WINDOWS_10_22H2;
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
					windows_version = WINDOWS_10_20H1;
				}
				else if (version_info.dwBuildNumber >= 18363)
				{
					windows_version = WINDOWS_10_19H2;
				}
				else if (version_info.dwBuildNumber >= 18362)
				{
					windows_version = WINDOWS_10_19H1;
				}
				else if (version_info.dwBuildNumber >= 17763)
				{
					windows_version = WINDOWS_10_RS5;
				}
				else if (version_info.dwBuildNumber >= 17134)
				{
					windows_version = WINDOWS_10_RS4;
				}
				else if (version_info.dwBuildNumber >= 16299)
				{
					windows_version = WINDOWS_10_RS3;
				}
				else if (version_info.dwBuildNumber >= 15063)
				{
					windows_version = WINDOWS_10_RS2;
				}
				else if (version_info.dwBuildNumber >= 14393)
				{
					windows_version = WINDOWS_10_RS1;
				}
				else if (version_info.dwBuildNumber >= 10586)
				{
					windows_version = WINDOWS_10_TH2;
				}
				else // 10240+
				{
					windows_version = WINDOWS_10_TH1;
				}
			}
			else
			{
				windows_version = WINDOWS_NEW;
			}
		}

		_r_initonce_end (&init_once);
	}

	return windows_version;
}

BOOLEAN _r_sys_isprocessimmersive (
	_In_ HANDLE hprocess
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static IIP _IsImmersiveProcess = NULL;

	PVOID huser32;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"user32.dll", 0, &huser32);

		if (NT_SUCCESS (status))
		{
			// win8+
			_IsImmersiveProcess = (IIP)_r_sys_getprocaddress (huser32, "IsImmersiveProcess", 0);

			_r_sys_freelibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	if (!_IsImmersiveProcess)
		return FALSE;

	return !!_IsImmersiveProcess (hprocess);
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createprocess (
	_In_opt_ LPWSTR file_name,
	_In_opt_ LPWSTR command_line,
	_In_opt_ LPWSTR directory,
	_In_ BOOLEAN is_wait
)
{
	JOBOBJECT_BASIC_ACCOUNTING_INFORMATION basic_info = {0};
	PROCESS_INFORMATION process_info = {0};
	STARTUPINFO startup_info = {0};
	OBJECT_ATTRIBUTES oa = {0};
	PR_STRING command_line_string = NULL;
	PR_STRING directory_string = NULL;
	PR_STRING file_name_string = NULL;
	PR_STRING new_path;
	HANDLE hjob;
	NTSTATUS status;

	if (file_name)
	{
		file_name_string = _r_obj_createstring (file_name);

		// The user typed a name without a path so attempt to locate the executable. (dmex)
		if (!_r_fs_exists (&file_name_string->sr))
		{
			status = _r_path_search (NULL, &file_name_string->sr, L".exe", &new_path);

			if (!NT_SUCCESS (status))
				return status;

			_r_obj_movereference (&file_name_string, new_path);
		}
	}

	if (directory)
	{
		directory_string = _r_obj_createstring (directory);
	}
	else
	{
		if (!_r_obj_isstringempty (file_name_string))
			directory_string = _r_path_getbasedirectory (&file_name_string->sr);
	}

	if (command_line)
		command_line_string = _r_obj_createstring (command_line); // duplicate because CreateProcess modifies the string (wj32)

	startup_info.cb = sizeof (startup_info);

	if (CreateProcessW (
		_r_obj_getstring (file_name_string),
		_r_obj_getstring (command_line_string),
		NULL,
		NULL,
		FALSE,
		CREATE_UNICODE_ENVIRONMENT,
		NULL,
		_r_obj_getstring (directory_string),
		&startup_info,
		&process_info))
	{
		status = STATUS_SUCCESS;
	}
	else
	{
		status = _r_sys_doserrortontstatus (NtLastError ());
	}

	if (status == STATUS_SUCCESS && is_wait)
	{
		InitializeObjectAttributes (&oa, NULL, OBJ_OPENIF | OBJ_CASE_INSENSITIVE, NULL, NULL);

		if (NT_SUCCESS (NtCreateJobObject (&hjob, JOB_OBJECT_ALL_ACCESS, &oa)))
		{
			NtAssignProcessToJobObject (hjob, process_info.hProcess);

			do
			{
				if (!NT_SUCCESS (NtQueryInformationJobObject (hjob, JobObjectBasicAccountingInformation, &basic_info, sizeof (basic_info), NULL)))
					break;

				_r_sys_sleep (250);
			}
			while (basic_info.ActiveProcesses);

			NtClose (hjob);
		}
	}

	if (process_info.hProcess)
		NtClose (process_info.hProcess);

	if (process_info.hThread)
		NtClose (process_info.hThread);

	if (command_line_string)
		_r_obj_dereference (command_line_string);

	if (file_name_string)
		_r_obj_dereference (file_name_string);

	if (directory_string)
		_r_obj_dereference (directory_string);

	return status;
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

	ws_buffer = _r_mem_allocate (ws_buffer_length);

	allocation_length = (ULONG)(buffer->length);

	tmp_buffer = _r_obj_createbyte_ex (NULL, allocation_length);

	status = RtlCompressBuffer (format, buffer->buffer, (ULONG)buffer->length, tmp_buffer->buffer, allocation_length, 4096, &return_length, ws_buffer);

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
	ULONG attempts = 6;
	NTSTATUS status;

	allocation_length = (ULONG)(buffer->length) * 2;

	if (allocation_length > PR_SIZE_BUFFER_OVERFLOW)
	{
		*out_buffer = NULL;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	tmp_buffer = _r_obj_createbyte_ex (NULL, allocation_length);

	status = RtlDecompressBuffer (format, tmp_buffer->buffer, allocation_length, buffer->buffer, (ULONG)buffer->length, &return_length);

	if (status == STATUS_BAD_COMPRESSION_BUFFER)
	{
		do
		{
			allocation_length *= 2;

			if (allocation_length > PR_SIZE_BUFFER_OVERFLOW)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;

				break;
			}

			_r_obj_movereference (&tmp_buffer, _r_obj_createbyte_ex (NULL, allocation_length));

			status = RtlDecompressBuffer (format, tmp_buffer->buffer, allocation_length, buffer->buffer, (ULONG)buffer->length, &return_length);

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

NTSTATUS _r_sys_doserrortontstatus (
	_In_ ULONG error_code
)
{
	if (NT_CUSTOMER (error_code))
		return error_code;

	switch (error_code)
	{
		case ERROR_SUCCESS:
		{
			return STATUS_SUCCESS;
		}

		case ERROR_INVALID_FUNCTION:
		{
			return STATUS_ILLEGAL_FUNCTION;
		}

		case ERROR_FILE_NOT_FOUND:
		{
			return STATUS_NO_SUCH_FILE;
		}

		case ERROR_PATH_NOT_FOUND:
		{
			return STATUS_OBJECT_PATH_NOT_FOUND;
		}

		case ERROR_ACCESS_DENIED:
		{
			return STATUS_ACCESS_DENIED;
		}

		case ERROR_INVALID_HANDLE:
		{
			return STATUS_INVALID_HANDLE;
		}

		case ERROR_INVALID_DATA:
		{
			return STATUS_DATA_ERROR;
		}

		case ERROR_NO_MORE_FILES:
		{
			return STATUS_NO_MORE_FILES;
		}

		case ERROR_BAD_LENGTH:
		{
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		case ERROR_SHARING_VIOLATION:
		{
			return STATUS_SHARING_VIOLATION;
		}

		case ERROR_HANDLE_EOF:
		{
			return STATUS_END_OF_FILE;
		}

		case ERROR_NOT_SUPPORTED:
		{
			return STATUS_NOT_SUPPORTED;
		}

		case ERROR_INVALID_PARAMETER:
		{
			return STATUS_INVALID_PARAMETER;
		}

		case ERROR_INSUFFICIENT_BUFFER:
		{
			return STATUS_BUFFER_TOO_SMALL;
		}

		case ERROR_INVALID_NAME:
		{
			return STATUS_OBJECT_NAME_INVALID;
		}

		case ERROR_MOD_NOT_FOUND:
		{
			return STATUS_DLL_NOT_FOUND;
		}

		case ERROR_PROC_NOT_FOUND:
		{
			return STATUS_PROCEDURE_NOT_FOUND;
		}

		case ERROR_NOT_LOCKED:
		{
			return STATUS_NOT_LOCKED;
		}

		case ERROR_BAD_PATHNAME:
		{
			return STATUS_OBJECT_PATH_INVALID;
		}

		case ERROR_ALREADY_EXISTS:
		{
			return STATUS_OBJECT_NAME_COLLISION;
		}

		case ERROR_DELETE_PENDING:
		{
			return STATUS_DELETE_PENDING;
		}

		case ERROR_MORE_DATA:
		{
			return STATUS_MORE_ENTRIES;
		}

		case ERROR_NO_MORE_ITEMS:
		{
			return STATUS_NO_MORE_ENTRIES;
		}

		case ERROR_PARTIAL_COPY:
		{
			return STATUS_PARTIAL_COPY;
		}

		case ERROR_INVALID_IMAGE_HASH:
		{
			return STATUS_INVALID_IMAGE_HASH;
		}

		case ERROR_NOINTERFACE:
		{
			return STATUS_NOINTERFACE;
		}

		case ERROR_SERVICE_NOTIFICATION:
		{
			return STATUS_SERVICE_NOTIFICATION;
		}

		case ERROR_ALERTED:
		{
			return STATUS_ALERTED;
		}

		case ERROR_ELEVATION_REQUIRED:
		{
			return STATUS_ELEVATION_REQUIRED;
		}

		case ERROR_REPARSE:
		{
			return STATUS_REPARSE;
		}

		case ERROR_REPARSE_OBJECT:
		{
			return STATUS_REPARSE_OBJECT;
		}

		case ERROR_NOACCESS:
		{
			return STATUS_ACCESS_VIOLATION;
		}

		case ERROR_STACK_OVERFLOW:
		{
			return STATUS_STACK_OVERFLOW;
		}

		case ERROR_DEPENDENT_SERVICES_RUNNING:
		{
			return STATUS_UNSATISFIED_DEPENDENCIES;
		}

		case ERROR_SERVICE_ALREADY_RUNNING:
		{
			return STATUS_IMAGE_ALREADY_LOADED;
		}

		case ERROR_SERVICE_DISABLED:
		{
			return STATUS_ACCOUNT_DISABLED;
		}

		case ERROR_SERVICE_DOES_NOT_EXIST:
		{
			return STATUS_OBJECT_NAME_NOT_FOUND;
		}

		case ERROR_SERVICE_EXISTS:
		{
			return STATUS_OBJECT_NAME_COLLISION;
		}

		case ERROR_DUPLICATE_SERVICE_NAME:
		{
			return STATUS_OBJECT_NAME_EXISTS;
		}

		case ERROR_DLL_INIT_FAILED:
		{
			return STATUS_DLL_INIT_FAILED;
		}

		case ERROR_NOT_FOUND:
		{
			return STATUS_NOT_FOUND;
		}

		case ERROR_CANCELLED:
		{
			return STATUS_CANCELLED;
		}

		case ERROR_SERVICE_NOT_FOUND:
		{
			return STATUS_OBJECT_PATH_INVALID;
		}

		case ERROR_NOT_ALL_ASSIGNED:
		{
			return STATUS_NOT_ALL_ASSIGNED;
		}

		case ERROR_SOME_NOT_MAPPED:
		{
			return STATUS_SOME_NOT_MAPPED;
		}

		case ERROR_PRIVILEGE_NOT_HELD:
		{
			return STATUS_PRIVILEGE_NOT_HELD;
		}

		case ERROR_LOGON_FAILURE:
		{
			return STATUS_LOGON_FAILURE;
		}

		case ERROR_NONE_MAPPED:
		{
			return STATUS_NONE_MAPPED;
		}

		case ERROR_NO_SUCH_DOMAIN:
		{
			return STATUS_NO_SUCH_DOMAIN;
		}

		case ERROR_INTERNAL_ERROR:
		{
			return STATUS_INTERNAL_ERROR;
		}

		case ERROR_INVALID_WINDOW_HANDLE:
		{
			return STATUS_INVALID_HANDLE;
		}

		case ERROR_NO_SYSTEM_RESOURCES:
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		case ERROR_TIMEOUT:
		{
			return STATUS_TIMEOUT;
		}

		case ERROR_RESOURCE_TYPE_NOT_FOUND:
		{
			return STATUS_RESOURCE_TYPE_NOT_FOUND;
		}

		case ERROR_RESOURCE_NAME_NOT_FOUND:
		{
			return STATUS_RESOURCE_NAME_NOT_FOUND;
		}

		case ERROR_RESOURCE_LANG_NOT_FOUND:
		{
			return STATUS_RESOURCE_LANG_NOT_FOUND;
		}

		case ERROR_NOT_ENOUGH_QUOTA:
		{
			return STATUS_QUOTA_EXCEEDED;
		}

		case ERROR_INVALID_TIME:
		{
			return STATUS_INVALID_PARAMETER;
		}

		case ERROR_WMI_GUID_NOT_FOUND:
		{
			return STATUS_WMI_GUID_NOT_FOUND;
		}

		case ERROR_WMI_INSTANCE_NOT_FOUND:
		{
			return STATUS_WMI_INSTANCE_NOT_FOUND;
		}

		case ERROR_ACTIVE_CONNECTIONS:
		{
			return STATUS_ALREADY_DISCONNECTED;
		}

		case ERROR_CTX_CLOSE_PENDING:
		{
			return STATUS_CTX_CLOSE_PENDING;
		}

		case ERROR_SERVICES_FAILED_AUTOSTART:
		{
			return STATUS_SERVICES_FAILED_AUTOSTART;
		}

		case ERROR_INVALID_SERVICE_CONTROL:
		{
			return STATUS_INVALID_DEVICE_REQUEST;
		}

		case ERROR_MUI_FILE_NOT_FOUND:
		{
			return STATUS_MUI_FILE_NOT_FOUND;
		}

		case ERROR_MUI_INVALID_FILE:
		{
			return STATUS_MUI_INVALID_FILE;
		}

		case ERROR_MUI_INVALID_RC_CONFIG:
		{
			return STATUS_MUI_INVALID_RC_CONFIG;
		}

		case ERROR_MUI_INVALID_LOCALE_NAME:
		{
			return STATUS_MUI_INVALID_LOCALE_NAME;
		}

		case ERROR_MUI_INVALID_ULTIMATEFALLBACK_NAME:
		{
			return STATUS_MUI_INVALID_ULTIMATEFALLBACK_NAME;
		}

		case ERROR_MUI_FILE_NOT_LOADED:
		{
			return STATUS_MUI_FILE_NOT_LOADED;
		}

		case ERROR_RESOURCE_ENUM_USER_STOP:
		{
			return STATUS_RESOURCE_ENUM_USER_STOP;
		}

		case NTE_INVALID_HANDLE:
		{
			return STATUS_INVALID_HANDLE;
		}

		case NTE_INVALID_PARAMETER:
		{
			return STATUS_INVALID_PARAMETER;
		}

		case NTE_BUFFER_TOO_SMALL:
		{
			return STATUS_BUFFER_TOO_SMALL;
		}

		default:
		{
			return NTSTATUS_FROM_WIN32 (error_code);
		}
	}
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_enumprocesses (
	_Out_ PSYSTEM_PROCESS_INFORMATION_PTR out_buffer
)
{
	PVOID buffer;
	ULONG length;
	NTSTATUS status;

	length = PR_SIZE_BUFFER;
	buffer = _r_mem_allocate (length);

	while (TRUE)
	{
		status = NtQuerySystemInformation (SystemProcessInformation, buffer, length, &length);

		if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
		{
			buffer = _r_mem_reallocate (buffer, length);
		}
		else
		{
			break;
		}
	}

	if (NT_SUCCESS (status))
	{
		*out_buffer = buffer;
	}
	else
	{
		*out_buffer = NULL;

		_r_mem_free (buffer);
	}

	return status;
}

_Ret_maybenull_
PVOID _r_sys_getprocaddress (
	_In_ PVOID hinst,
	_In_opt_ LPSTR name,
	_In_opt_ ULONG ordinal
)
{
	ANSI_STRING procedure_name = {0};
	PANSI_STRING ptr = NULL;
	PVOID proc_address;
	NTSTATUS status;

	if (!name && !ordinal)
		return NULL;

	if (name)
	{
		RtlInitAnsiString (&procedure_name, name);

		ptr = &procedure_name;
	}

	status = LdrGetProcedureAddressEx (hinst, ptr, ordinal, &proc_address, 0);

	if (NT_SUCCESS (status))
		return proc_address;

	return NULL;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getprocessimagepath (
	_In_ HANDLE hprocess,
	_In_ BOOLEAN is_ntpathtodos,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PUNICODE_STRING filename_nt;
	ULONG return_length;
	ULONG buffer_length;
	NTSTATUS status;

	buffer_length = sizeof (UNICODE_STRING) + 0x100;
	filename_nt = _r_mem_allocate (buffer_length);

	status = NtQueryInformationProcess (hprocess, is_ntpathtodos ? ProcessImageFileNameWin32 : ProcessImageFileName, filename_nt, buffer_length, &return_length);

	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		buffer_length = return_length;
		filename_nt = _r_mem_reallocate (filename_nt, buffer_length);

		status = NtQueryInformationProcess (hprocess, is_ntpathtodos ? ProcessImageFileNameWin32 : ProcessImageFileName, filename_nt, buffer_length, &return_length);
	}

	if (NT_SUCCESS (status))
	{
		// Note: Some minimal/pico processes have UNICODE_NULL as their filename. (dmex)
		if (filename_nt->Length == 0)
		{
			_r_mem_free (filename_nt);

			*out_buffer = NULL;

			return STATUS_UNSUCCESSFUL;
		}

		*out_buffer = _r_obj_createstring3 (filename_nt);
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (filename_nt);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_getprocessimagepathbyid (
	_In_ HANDLE hprocess_id,
	_In_ BOOLEAN is_ntpathtodos,
	_Out_ PR_STRING_PTR out_buffer
)
{
	SYSTEM_PROCESS_ID_INFORMATION data = {0};
	PR_STRING path;
	NTSTATUS status;

	// On input, specify the PID and a buffer to hold the string.
	data.ProcessId = hprocess_id;

	data.ImageName.MaximumLength = 0x100;
	data.ImageName.Buffer = _r_mem_allocate (data.ImageName.MaximumLength);

	status = NtQuerySystemInformation (SystemProcessIdInformation, &data, sizeof (SYSTEM_PROCESS_ID_INFORMATION), NULL);

	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		// Required length is stored in MaximumLength.
		data.ImageName.Buffer = _r_mem_reallocate (data.ImageName.Buffer, data.ImageName.MaximumLength);

		status = NtQuerySystemInformation (SystemProcessIdInformation, &data, sizeof (SYSTEM_PROCESS_ID_INFORMATION), NULL);
	}

	// Note: Some minimal/pico processes have UNICODE_NULL as their filename. (dmex)
	if (NT_SUCCESS (status))
	{
		if (data.ImageName.Length == 0)
			status = STATUS_UNSUCCESSFUL;
	}

	if (NT_SUCCESS (status))
	{
		path = _r_obj_createstring3 (&data.ImageName);

		if (is_ntpathtodos)
		{
			*out_buffer = _r_path_dospathfromnt (&path->sr);

			_r_obj_dereference (path);
		}
		else
		{
			*out_buffer = path;
		}
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (data.ImageName.Buffer);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_sys_loadicon (
	_In_opt_ PVOID hinst,
	_In_ LPCWSTR icon_name,
	_In_ LONG icon_size,
	_Out_ HICON_PTR out_buffer
)
{
	HICON hicon;
	HRESULT status;

	// vista+
	status = LoadIconWithScaleDown (hinst, icon_name, icon_size, icon_size, &hicon);

	if (SUCCEEDED (status))
	{
		*out_buffer = hicon;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Ret_maybenull_
HICON _r_sys_loadsharedicon (
	_In_opt_ PVOID hinst,
	_In_ LPWSTR icon_name,
	_In_ LONG icon_size
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_QUEUED_LOCK queued_lock = PR_QUEUED_LOCK_INIT;
	static PR_HASHTABLE shared_icons = NULL;

	R_OBJECT_POINTER object_data = {0};
	PR_OBJECT_POINTER object_ptr;
	HICON hicon;
	ULONG hash_code;
	ULONG name_hash;
	HRESULT status;

	if (_r_initonce_begin (&init_once))
	{
		shared_icons = _r_obj_createhashtable (sizeof (R_OBJECT_POINTER), 8, NULL);

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

	hash_code = (name_hash ^ (PtrToUlong (hinst) >> 5) ^ (icon_size << 3) ^ icon_size);

	_r_queuedlock_acquireshared (&queued_lock);
	object_ptr = _r_obj_findhashtable (shared_icons, hash_code);
	_r_queuedlock_releaseshared (&queued_lock);

	if (object_ptr)
	{
		// found shared icon
		hicon = (HICON)object_ptr->object_body;
	}
	else
	{
		// add new shared icon entry
		status = _r_sys_loadicon (hinst, icon_name, icon_size, &hicon);

		if (SUCCEEDED (status))
		{
			object_data.object_body = hicon;

			_r_queuedlock_acquireexclusive (&queued_lock);
			_r_obj_addhashtableitem (shared_icons, hash_code, &object_data);
			_r_queuedlock_releaseexclusive (&queued_lock);
		}
	}

	return hicon;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_loadlibraryasresource (
	_In_ PR_STRINGREF lib_name,
	_Out_ PVOID_PTR out_buffer
)
{
	LARGE_INTEGER offset = {0};
	PVOID base_address = NULL;
	PR_STRING path;
	HANDLE hsection = NULL;
	HANDLE hfile;
	ULONG_PTR base_size = 0;
	NTSTATUS status;

	*out_buffer = NULL;

	if (_r_fs_exists (lib_name))
	{
		path = _r_obj_createstring2 (lib_name);
	}
	else
	{
		status = _r_path_search (NULL, lib_name, NULL, &path);

		if (!NT_SUCCESS (status))
			return status;
	}

	status = _r_fs_openfile (&path->sr, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, FALSE, &hfile);

	if (!NT_SUCCESS (status))
		return status;

	status = NtCreateSection (
		&hsection,
		SECTION_QUERY | SECTION_MAP_READ,
		NULL,
		NULL,
		PAGE_READONLY,
		_r_sys_isosversiongreaterorequal (WINDOWS_8) ? SEC_IMAGE_NO_EXECUTE : SEC_IMAGE, // win8+
		hfile
	);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = NtMapViewOfSection (
		hsection,
		NtCurrentProcess (),
		&base_address,
		0,
		0,
		&offset,
		&base_size,
		ViewUnmap,
		_r_sys_isosversiongreaterorequal (WINDOWS_10_RS2) ? MEM_MAPPED : 0,
		PAGE_READONLY
	);

	if (status == STATUS_IMAGE_NOT_AT_BASE)
		status = STATUS_SUCCESS;

	// Windows returns the address with bitwise OR|2 for use with LDR_IS_IMAGEMAPPING (dmex)
	if (NT_SUCCESS (status))
		*out_buffer = LDR_MAPPEDVIEW_TO_IMAGEMAPPING (base_address);

CleanupExit:

	_r_obj_dereference (path);

	if (hsection)
		NtClose (hsection);

	NtClose (hfile);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_loadlibrary (
	_In_ PR_STRINGREF lib_name,
	_In_opt_ ULONG lib_flags,
	_Out_ PVOID_PTR out_buffer
)
{
	UNICODE_STRING us;
	PVOID hmodule;
	ULONG flags;
	NTSTATUS status;

	if (lib_flags)
	{
		flags = lib_flags;
	}
	else
	{
		flags = LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32;
	}

	if (flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE))
	{
		status = _r_sys_loadlibraryasresource (lib_name, &hmodule);
	}
	else
	{
		_r_obj_initializeunicodestring2 (&us, lib_name);

		status = LdrLoadDll (NULL, &flags, &us, &hmodule);
	}

	if (NT_SUCCESS (status))
	{
		*out_buffer = hmodule;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_loadlibrarytype (
	_In_ R_ERROR_TYPE type,
	_Out_ PVOID_PTR out_buffer
)
{
	PVOID hmodule;
	R_STRINGREF name;
	NTSTATUS status;

	switch (type)
	{
		case ET_NONE:
		{
			*out_buffer = NULL;

			return STATUS_SUCCESS;
		}

		case ET_WINDOWS:
		{
			_r_obj_initializestringref (&name, L"kernel32.dll");
			break;
		}

		case ET_NATIVE:
		{
			_r_obj_initializestringref (&name, L"ntdll.dll");
			break;
		}

		case ET_WINHTTP:
		{
			_r_obj_initializestringref (&name, L"winhttp.dll");
			break;
		}

		default:
		{
			*out_buffer = NULL;

			return STATUS_INVALID_INFO_CLASS;
		}
	}

	status = _r_sys_loadlibraryasresource (&name, &hmodule);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hmodule;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_openprocess (
	_In_opt_ HANDLE process_id,
	_In_ ACCESS_MASK desired_access,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	CLIENT_ID client_id = {0};
	HANDLE hprocess;
	NTSTATUS status;

	InitializeObjectAttributes (&oa, NULL, 0, NULL, NULL);

	client_id.UniqueProcess = process_id;

	status = NtOpenProcess (&hprocess, desired_access, &oa, &client_id);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hprocess;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_openthread (
	_In_opt_ HANDLE thread_id,
	_In_ ACCESS_MASK desired_access,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	CLIENT_ID client_id = {0};
	HANDLE hprocess;
	NTSTATUS status;

	InitializeObjectAttributes (&oa, NULL, 0, NULL, NULL);

	client_id.UniqueThread = thread_id;

	status = NtOpenThread (&hprocess, desired_access, &oa, &client_id);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hprocess;
	}
	else
	{
		*out_buffer = NULL;
	}

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

	status = NtQueryInformationProcess (process_handle, info_class, NULL, 0, &return_length);

	if (status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL && status != STATUS_INFO_LENGTH_MISMATCH)
	{
		*out_buffer = NULL;

		return status;
	}

	buffer = _r_mem_allocate (return_length);

	status = NtQueryInformationProcess (process_handle, info_class, buffer, return_length, &return_length);

	if (NT_SUCCESS (status))
	{
		*out_buffer = _r_obj_createstring3 (buffer);
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_mem_free (buffer);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_sys_registerrestart (
	_In_ BOOLEAN is_register
)
{
	HRESULT status;

	// vista+
	if (is_register)
	{
		status = RegisterApplicationRestart (_r_sys_getcommandline (), RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
	}
	else
	{
		status = UnregisterApplicationRestart ();
	}

	return status;
}

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_runasadmin (
	_In_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR directory
)
{
	SHELLEXECUTEINFO shex = {0};
	NTSTATUS status;

	shex.cbSize = sizeof (shex);
	shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_NO_UI;
	shex.lpVerb = L"runas";
	shex.nShow = SW_SHOW;
	shex.lpFile = file_name;
	shex.lpParameters = command_line;
	shex.lpDirectory = directory;

	if (ShellExecuteExW (&shex))
	{
		status = STATUS_SUCCESS;
	}
	else
	{
		status = _r_sys_doserrortontstatus (NtLastError ());
	}

	return status;
}

NTSTATUS NTAPI _r_sys_basethreadstart (
	_In_ PVOID arglist
)
{
	R_THREAD_CONTEXT context;
	PR_FREE_LIST free_list;
	HRESULT status;

	free_list = _r_sys_getthreadfreelist ();

	context = *(PR_THREAD_CONTEXT)arglist;

	_r_freelist_deleteitem (free_list, arglist);

	status = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	context.base_address (context.arglist);

	if (status == S_OK || status == S_FALSE)
		CoUninitialize ();

	RtlExitUserThread (status);

	return STATUS_SUCCESS;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_freelibrary (
	_In_ PVOID dll_handle
)
{
	NTSTATUS status;

	if (LDR_IS_IMAGEMAPPING (dll_handle))
	{
		status = NtUnmapViewOfSection (NtCurrentProcess (), LDR_IMAGEMAPPING_TO_MAPPEDVIEW (dll_handle));
	}
	else
	{
		status = LdrUnloadDll (dll_handle);
	}

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
	_Out_opt_ PHANDLE thread_handle,
	_In_ HANDLE hprocess,
	_In_ PUSER_THREAD_START_ROUTINE base_address,
	_In_opt_ PVOID arglist,
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
		hprocess,
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
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
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
PR_STRING _r_sys_querytaginformation (
	_In_ HANDLE hprocess,
	_In_ LPCVOID tag
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static IQTI _I_QueryTagInformation = NULL;

	TAG_INFO_NAME_FROM_TAG tag_query = {0};
	PR_STRING service_name_string;
	PVOID hsechost;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"sechost.dll", 0, &hsechost); // win81+

		if (NT_SUCCESS (status))
		{
			_I_QueryTagInformation = (IQTI)_r_sys_getprocaddress (hsechost, "I_QueryTagInformation", 0);

			_r_sys_freelibrary (hsechost);
		}

		_r_initonce_end (&init_once);
	}

	if (!_I_QueryTagInformation)
		return NULL;

	tag_query.InParams.dwPid = HandleToULong (hprocess);
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
	ULONG buffer_length = 128;
	ULONG return_length;
	NTSTATUS status;

	buffer = _r_mem_allocate (buffer_length);

	status = NtQueryInformationToken (token_handle, token_class, buffer, buffer_length, &return_length);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		buffer_length = return_length;
		buffer = _r_mem_reallocate (buffer, buffer_length);

		status = NtQueryInformationToken (token_handle, token_class, buffer, buffer_length, &return_length);
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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_queryprocessenvironment (
	_In_ HANDLE process_handle,
	_Out_ PR_ENVIRONMENT environment
)
{
	PAGE_PRIORITY_INFORMATION page_priority = {0};
	PROCESS_PRIORITY_CLASS priority_class = {0};
	IO_PRIORITY_HINT io_priority = {0};
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

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_querythreadenvironment (
	_In_ HANDLE thread_handle,
	_Out_ PR_ENVIRONMENT environment
)
{
	PAGE_PRIORITY_INFORMATION page_priority = {0};
	IO_PRIORITY_HINT io_priority = {0};
	KPRIORITY base_priority = 0;
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

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setprocessprivilege (
	_In_ HANDLE process_handle,
	_In_reads_ (count) PULONG privileges,
	_In_ ULONG count,
	_In_ BOOLEAN is_enable
)
{
	PTOKEN_PRIVILEGES token_privileges;
	LARGE_INTEGER li = {0};
	PVOID privileges_buffer;
	HANDLE token_handle;
	NTSTATUS status;

	status = NtOpenProcessToken (process_handle, TOKEN_ADJUST_PRIVILEGES, &token_handle);

	if (!NT_SUCCESS (status))
		return status;

	privileges_buffer = _r_mem_allocate (FIELD_OFFSET (TOKEN_PRIVILEGES, Privileges) + (sizeof (LUID_AND_ATTRIBUTES) * count));

	token_privileges = privileges_buffer;
	token_privileges->PrivilegeCount = count;

	for (ULONG_PTR i = 0; i < count; i++)
	{
		li.QuadPart = privileges[i];

		token_privileges->Privileges[i].Luid.LowPart = li.LowPart;
		token_privileges->Privileges[i].Luid.HighPart = li.HighPart;

		token_privileges->Privileges[i].Attributes = is_enable ? SE_PRIVILEGE_ENABLED : SE_PRIVILEGE_REMOVED;
	}

	// can be STATUS_NOT_ALL_ASSIGNED
	status = NtAdjustPrivilegesToken (token_handle, FALSE, token_privileges, 0, NULL, NULL);

	_r_mem_free (privileges_buffer);

	NtClose (token_handle);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setenvironmentvariable (
	_Inout_opt_ PVOID_PTR environment,
	_In_ PR_STRINGREF name_sr,
	_In_opt_ PR_STRINGREF value_sr
)
{
	UNICODE_STRING value_us;
	UNICODE_STRING name_us;
	NTSTATUS status;

	_r_obj_initializeunicodestring2 (&name_us, name_sr);

	if (value_sr)
	{
		_r_obj_initializeunicodestring2 (&value_us, value_sr);
	}
	else
	{
		_r_obj_initializeunicodestring (&value_us, NULL);
	}

	status = RtlSetEnvironmentVariable (environment, &name_us, &value_us);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setprocessenvironment (
	_In_ HANDLE process_handle,
	_In_ PR_ENVIRONMENT new_environment
)
{
	PROCESS_PRIORITY_CLASS_EX priority_class_ex = {0};
	PAGE_PRIORITY_INFORMATION page_priority = {0};
	PROCESS_PRIORITY_CLASS priority_class = {0};
	R_ENVIRONMENT current_environment;
	IO_PRIORITY_HINT io_priority;
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	_r_sys_queryprocessenvironment (process_handle, &current_environment);

	// set base priority
	if (current_environment.base_priority != new_environment->base_priority)
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_11_22H2))
		{
			priority_class_ex.PriorityClass = (UCHAR)(new_environment->base_priority);
			priority_class_ex.PriorityClassValid = TRUE;

			status = NtSetInformationProcess (process_handle, ProcessPriorityClassEx, &priority_class_ex, sizeof (priority_class_ex));
		}
		else
		{
			priority_class.PriorityClass = (UCHAR)(new_environment->base_priority);
			priority_class.Foreground = FALSE;

			status = NtSetInformationProcess (process_handle, ProcessPriorityClass, &priority_class, sizeof (priority_class));
		}
	}

	// set i/o priority
	if (current_environment.io_priority != new_environment->io_priority)
	{
		io_priority = new_environment->io_priority;

		status = NtSetInformationProcess (process_handle, ProcessIoPriority, &io_priority, sizeof (io_priority));
	}

	// set memory priority
	if (current_environment.page_priority != new_environment->page_priority)
	{
		page_priority.PagePriority = new_environment->page_priority;

		status = NtSetInformationProcess (process_handle, ProcessPagePriority, &page_priority, sizeof (page_priority));
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setthreadenvironment (
	_In_ HANDLE thread_handle,
	_In_ PR_ENVIRONMENT new_environment
)
{
	PAGE_PRIORITY_INFORMATION page_priority = {0};
	R_ENVIRONMENT current_environment;
	IO_PRIORITY_HINT io_priority;
	KPRIORITY base_priority;
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	_r_sys_querythreadenvironment (thread_handle, &current_environment);

	// set base priority
	if (current_environment.base_priority != new_environment->base_priority)
	{
		base_priority = new_environment->base_priority;

		status = NtSetInformationThread (thread_handle, ThreadBasePriority, &base_priority, sizeof (base_priority));
	}

	// set i/o priority
	if (current_environment.io_priority != new_environment->io_priority)
	{
		io_priority = new_environment->io_priority;

		status = NtSetInformationThread (thread_handle, ThreadIoPriority, &io_priority, sizeof (io_priority));
	}

	// set memory priority
	if (current_environment.page_priority != new_environment->page_priority)
	{
		page_priority.PagePriority = new_environment->page_priority;

		status = NtSetInformationThread (thread_handle, ThreadPagePriority, &page_priority, sizeof (page_priority));
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setthreadname (
	_In_ HANDLE thread_handle,
	_In_ LPCWSTR thread_name
)
{
	THREAD_NAME_INFORMATION tni = {0};
	NTSTATUS status;

	if (_r_sys_isosversionlower (WINDOWS_10))
		return STATUS_NOT_SUPPORTED;

	status = RtlInitUnicodeStringEx (&tni.ThreadName, thread_name);

	if (!NT_SUCCESS (status))
		return status;

	// win10+
	status = NtSetInformationThread (thread_handle, ThreadNameInformation, &tni, sizeof (tni));

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_sleep (
	_In_ ULONG milliseconds
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static RDE _RtlDelayExecution = NULL;

	LARGE_INTEGER timeout;
	PVOID ntdll;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"ntdll.dll", 0, &ntdll);

		if (NT_SUCCESS (status))
		{
			// win10+
			_RtlDelayExecution = (RDE)_r_sys_getprocaddress (ntdll, "RtlDelayExecution", 0);

			_r_sys_freelibrary (ntdll);
		}

		_r_initonce_end (&init_once);
	}

	if (_RtlDelayExecution)
		return _RtlDelayExecution (FALSE, _r_calc_millisecondstolargeinteger (&timeout, milliseconds));

	return NtDelayExecution (FALSE, _r_calc_millisecondstolargeinteger (&timeout, milliseconds));
}

NTSTATUS _r_sys_waitformultipleobjects (
	_In_ ULONG count,
	_In_reads_ (count) PVOID_PTR hevents,
	_In_ ULONG milliseconds,
	_In_ BOOLEAN is_waitall
)
{
	LARGE_INTEGER timeout;
	NTSTATUS status;

	status = NtWaitForMultipleObjects (count, hevents, is_waitall ? WaitAll : WaitAny, FALSE, _r_calc_millisecondstolargeinteger (&timeout, milliseconds));

	//if (!NT_SUCCESS (status))
	//	status = WAIT_FAILED;

	return status;
}

NTSTATUS _r_sys_waitforsingleobject (
	_In_ HANDLE hevent,
	_In_ ULONG milliseconds
)
{
	LARGE_INTEGER timeout;
	NTSTATUS status;

	status = NtWaitForSingleObject (hevent, FALSE, _r_calc_millisecondstolargeinteger (&timeout, milliseconds));

	//if (!NT_SUCCESS (status))
	//	status = WAIT_FAILED;

	return status;
}

//
// Unixtime
//

LONG64 _r_unixtime_now ()
{
	LARGE_INTEGER time_value = {0};

#if defined(_WIN64)
	time_value.QuadPart = *(volatile ULONG64*)&USER_SHARED_DATA->SystemTime;
#else
	do
	{
		time_value.HighPart = USER_SHARED_DATA->SystemTime.High1Time;
		time_value.LowPart = USER_SHARED_DATA->SystemTime.LowPart;
	}
	while (time_value.HighPart != USER_SHARED_DATA->SystemTime.High2Time);
#endif // _WIN64

	return _r_unixtime_from_largeinteger (&time_value);
}

LONG64 _r_unixtime_from_filetime (
	_In_ const PFILETIME file_time
)
{
	LARGE_INTEGER time_value = {0};

	time_value.HighPart = file_time->dwHighDateTime;
	time_value.LowPart = file_time->dwLowDateTime;

	return _r_unixtime_from_largeinteger (&time_value);
}

LONG64 _r_unixtime_from_systemtime (
	_In_ const LPSYSTEMTIME system_time
)
{
	FILETIME file_time = {0};
	TIME_FIELDS tf = {0};
	LONG64 timestamp = 0;

	tf.Year = system_time->wYear;
	tf.Month = system_time->wMonth;
	tf.Day = system_time->wDay;
	tf.Hour = system_time->wHour;
	tf.Minute = system_time->wMinute;
	tf.Second = system_time->wSecond;
	tf.Milliseconds = system_time->wMilliseconds;

	if (RtlTimeFieldsToTime (&tf, (PLARGE_INTEGER)&file_time))
		timestamp = _r_unixtime_from_filetime (&file_time);

	return timestamp;
}

VOID _r_unixtime_to_filetime (
	_In_ LONG64 unixtime,
	_Out_ PFILETIME file_time
)
{
	LARGE_INTEGER time_value = {0};

	time_value.QuadPart = (unixtime * 10000000LL) + 116444736000000000LL;

	file_time->dwHighDateTime = time_value.HighPart;
	file_time->dwLowDateTime = time_value.LowPart;
}

VOID _r_unixtime_to_systemtime (
	_In_ LONG64 unixtime,
	_Out_ PSYSTEMTIME system_time
)
{
	TIME_FIELDS tf = {0};
	FILETIME file_time;
	PLARGE_INTEGER li;

	_r_unixtime_to_filetime (unixtime, &file_time);

	li = (PLARGE_INTEGER)&file_time;

	RtlTimeToTimeFields (li, &tf);

	system_time->wYear = tf.Year;
	system_time->wMonth = tf.Month;
	system_time->wDay = tf.Day;
	system_time->wHour = tf.Hour;
	system_time->wMinute = tf.Minute;
	system_time->wSecond = tf.Second;
	system_time->wMilliseconds = tf.Milliseconds;
	system_time->wDayOfWeek = tf.Weekday;
}

//
// Device context
//

_Success_ (return)
BOOLEAN _r_dc_adjustwindowrect (
	_Inout_ LPRECT rect,
	_In_ ULONG style,
	_In_ ULONG ex_style,
	_In_opt_ LONG dpi_value,
	_In_ BOOL is_menu
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static AWRFD _AdjustWindowRectExForDpi = NULL;

	PVOID huser32;
	NTSTATUS status;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"user32.dll", 0, &huser32);

		if (NT_SUCCESS (status))
		{
			// win10rs1+
			_AdjustWindowRectExForDpi = (AWRFD)_r_sys_getprocaddress (huser32, "AdjustWindowRectExForDpi", 0);

			_r_sys_freelibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (dpi_value && _AdjustWindowRectExForDpi)
		return !!_AdjustWindowRectExForDpi (rect, style, is_menu, ex_style, dpi_value);

	return !!AdjustWindowRectEx (rect, style, is_menu, ex_style);
}

_Ret_maybenull_
HBITMAP _r_dc_bitmapfromicon (
	_In_ HICON hicon,
	_In_ LONG width,
	_In_ LONG height
)
{
	BP_PAINTPARAMS paint_params = {0};
	BLENDFUNCTION blend_func = {0};
	BITMAPINFO bitmap_info = {0};
	HPAINTBUFFER hpaint_buffer;
	HBITMAP hbitmap = NULL;
	HGDIOBJ old_bitmap;
	PVOID memory_bits;
	HDC buffer_hdc;
	HDC hdc;
	RECT rect;

	hdc = CreateCompatibleDC (NULL);

	if (!hdc)
		goto CleanupExit;

	bitmap_info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap_info.bmiHeader.biWidth = width;
	bitmap_info.bmiHeader.biHeight = height;
	bitmap_info.bmiHeader.biSizeImage = width * height;

	hbitmap = CreateDIBSection (hdc, &bitmap_info, DIB_RGB_COLORS, &memory_bits, NULL, 0);

	if (!hbitmap)
		goto CleanupExit;

	old_bitmap = SelectObject (hdc, hbitmap);

	blend_func.AlphaFormat = AC_SRC_ALPHA;
	blend_func.BlendOp = AC_SRC_OVER;
	blend_func.BlendFlags = 0;
	blend_func.SourceConstantAlpha = 255;

	paint_params.cbSize = sizeof (paint_params);
	paint_params.dwFlags = BPPF_ERASE;
	paint_params.pBlendFunction = &blend_func;

	SetRect (&rect, 0, 0, width, height);

	// vista+
	hpaint_buffer = BeginBufferedPaint (hdc, &rect, BPBF_DIB, &paint_params, &buffer_hdc);

	if (hpaint_buffer)
	{
		DrawIconEx (buffer_hdc, 0, 0, hicon, width, height, 0, NULL, DI_NORMAL);

		// This will write the buffer contents to the destination bitmap.
		EndBufferedPaint (hpaint_buffer, TRUE);
	}
	else
	{
		_r_dc_fillrect (hdc, &rect, RGB (255, 255, 255));

		DrawIconEx (hdc, 0, 0, hicon, width, height, 0, NULL, DI_NORMAL);
	}

	SelectObject (hdc, old_bitmap);

CleanupExit:

	if (hdc)
		DeleteDC (hdc);

	return hbitmap;
}

_Ret_maybenull_
HICON _r_dc_bitmaptoicon (
	_In_ HBITMAP hbitmap,
	_In_ LONG width,
	_In_ LONG height
)
{
	ICONINFO icon_info = {0};
	HBITMAP hmask;
	HICON hicon;
	HDC hdc;

	hdc = GetDC (NULL);

	if (!hdc)
		return NULL;

	hmask = CreateCompatibleBitmap (hdc, width, height);

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

_Ret_maybenull_
HBITMAP _r_dc_createbitmap (
	_In_opt_ HDC hdc,
	_In_ LONG width,
	_In_ LONG height,
	_Out_ _When_ (return != NULL, _Notnull_) PVOID_PTR bits
)
{
	BITMAPINFO bmi = {0};
	HBITMAP hbitmap;
	HDC new_hdc = NULL;

	if (!hdc)
	{
		new_hdc = GetDC (NULL);

		if (!new_hdc)
		{
			*bits = NULL;

			return NULL;
		}

		hdc = new_hdc;
	}

	bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbitmap = CreateDIBSection (hdc, &bmi, DIB_RGB_COLORS, bits, NULL, 0);

	if (new_hdc)
		ReleaseDC (NULL, new_hdc);

	return hbitmap;
}

VOID _r_dc_drawtext (
	_In_opt_ HTHEME htheme,
	_In_ HDC hdc,
	_In_ PR_STRINGREF string,
	_Inout_ LPRECT rect,
	_In_ INT part_id,
	_In_ INT state_id,
	_In_ UINT flags,
	_In_opt_ COLORREF clr_text
)
{
	DTTOPTS dtto = {0};
	COLORREF clr_old = 0;

	SetBkMode (hdc, TRANSPARENT);

	if (htheme)
	{
		dtto.dwSize = sizeof (DTTOPTS);

		if (clr_text)
		{
			dtto.dwFlags |= DTT_TEXTCOLOR;

			dtto.crText = clr_text;
		}

		if (state_id)
		{
			dtto.dwFlags |= DTT_STATEID;

			dtto.iStateId = state_id;
		}

		// vista+
		DrawThemeTextEx (htheme, hdc, part_id, state_id, string->buffer, (INT)_r_str_getlength2 (string), flags, rect, &dtto);
	}
	else
	{
		if (clr_text)
			clr_old = SetTextColor (hdc, clr_text);

		DrawTextExW (hdc, string->buffer, (INT)_r_str_getlength2 (string), rect, flags, NULL);

		if (clr_text)
			SetTextColor (hdc, clr_old);
	}
}

BOOLEAN _r_dc_drawwindow (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_ BOOLEAN is_drawfooter
)
{
	RECT rect;
	COLORREF clr;
	LONG footer_height;
	LONG wnd_height;
	LONG dpi_value;

	if (!GetClientRect (hwnd, &rect))
		return FALSE;

	if (_r_theme_isenabled ())
	{
		clr = WND_BACKGROUND_CLR;
	}
	else
	{
		clr = GetSysColor (is_drawfooter ? COLOR_WINDOW : COLOR_BTNFACE);
	}

	// fill background color
	_r_dc_fillrect (hdc, &rect, clr);

	if (!is_drawfooter)
		return TRUE;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	wnd_height = rect.bottom;

	footer_height = _r_dc_getdpi (PR_SIZE_FOOTERHEIGHT, dpi_value);

	SetRect (&rect, 0, wnd_height - footer_height, rect.right, wnd_height);

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

// Optimized version of WinAPI function "FillRect"
BOOLEAN _r_dc_fillrect (
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ COLORREF clr
)
{
	COLORREF clr_prev;
	BOOLEAN is_success;

	clr_prev = SetBkColor (hdc, clr);

	is_success = !!ExtTextOutW (hdc, 0, 0, ETO_OPAQUE, rect, NULL, 0, NULL);

	SetBkColor (hdc, clr_prev);

	return is_success;
}

_Ret_maybenull_
HGDIOBJ _r_dc_fixfont (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	HFONT hfont;

	hfont = _r_ctrl_getfont (hwnd, ctrl_id);

	if (!hfont)
		return NULL;

	return SelectObject (hdc, hfont);
}

BOOLEAN _r_dc_framerect (
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ COLORREF clr
)
{
	SetDCBrushColor (hdc, clr);

	return FrameRect (hdc, rect, GetStockObject (DC_BRUSH)) != 0;
}

_Success_ (return != 0)
COLORREF _r_dc_getcoloraccent ()
{
	COLORREF clr;
	BOOL is_opaque;
	HRESULT status;

	if (_r_sys_isosversionequal (WINDOWS_7))
		return WND_BORDER_CLR;

	status = DwmGetColorizationColor (&clr, &is_opaque);

	if (FAILED (status))
		return 0;

	return RGB (GetBValue (clr), GetGValue (clr), GetRValue (clr));
}

COLORREF _r_dc_getcolorbrightness (
	_In_ COLORREF clr
)
{
	DOUBLE brightness;
	LONG r;
	LONG g;
	LONG b;

	r = GetRValue (clr);
	g = GetGValue (clr);
	b = GetBValue (clr);

	brightness = (r * 0.299) + (g * 0.587) + (b * 0.114);

	return (255.0 - brightness < 105.0) ? RGB (0x00, 0x00, 0x00) : RGB (0xFF, 0xFF, 0xFF);
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

BOOLEAN _r_dc_getdefaultfont (
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_forced
)
{
	NONCLIENTMETRICS ncm = {0};
	PLOGFONT system_font;

	ncm.cbSize = sizeof (ncm);

	if (!_r_dc_getsystemparametersinfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, dpi_value))
		return FALSE;

	system_font = &ncm.lfMessageFont;

	if (is_forced || _r_str_isempty2 (logfont->lfFaceName))
		_r_str_copy (logfont->lfFaceName, LF_FACESIZE, system_font->lfFaceName);

	if (is_forced || !logfont->lfHeight)
		logfont->lfHeight = system_font->lfHeight;

	if (is_forced || !logfont->lfWeight)
		logfont->lfWeight = system_font->lfWeight;

	return TRUE;
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
	PVOID huser32;
	PVOID hshcore;
	HDC hdc;
	LONG dpi_value;
	UINT dpi_x;
	UINT dpi_y;
	NTSTATUS status;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"shcore.dll", 0, &hshcore);

		if (NT_SUCCESS (status))
		{
			// win81+
			_GetDpiForMonitor = (GDFM)_r_sys_getprocaddress (hshcore, "GetDpiForMonitor", 0);

			_r_sys_freelibrary (hshcore);
		}

		status = _r_sys_loadlibrary2 (L"user32.dll", 0, &huser32);

		if (NT_SUCCESS (status))
		{
			// win10rs1+
			_GetDpiForWindow = (GDFW)_r_sys_getprocaddress (huser32, "GetDpiForWindow", 0);

			// win10rs1+
			_GetDpiForSystem = (GDFS)_r_sys_getprocaddress (huser32, "GetDpiForSystem", 0);

			_r_sys_freelibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	if (rect || hwnd)
	{
		// win10rs1+
		if (_GetDpiForWindow)
		{
			if (hwnd)
			{
				dpi_x = _GetDpiForWindow (hwnd);

				if (dpi_x)
					return dpi_x;
			}
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

			status = _GetDpiForMonitor (hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

			if (SUCCEEDED (status))
				return dpi_x;
		}
	}

	// win10rs1+
	if (_GetDpiForSystem)
		return _GetDpiForSystem ();

	// win8 and lower fallback
	hdc = GetDC (NULL);

	if (hdc)
	{
		dpi_value = GetDeviceCaps (hdc, LOGPIXELSX);

		ReleaseDC (NULL, hdc);

		return dpi_value;
	}

	return USER_DEFAULT_SCREEN_DPI; // fallback
}

_Success_ (return != 0)
LONG _r_dc_getfontwidth (
	_In_ HDC hdc,
	_In_ PR_STRINGREF string,
	_Out_opt_ PSIZE out_buffer
)
{
	SIZE size = {0};

	if (GetTextExtentExPointW (hdc, string->buffer, (ULONG)_r_str_getlength2 (string), 0, NULL, NULL, &size))
	{
		if (out_buffer)
			RtlCopyMemory (out_buffer, &size, sizeof (SIZE));

		return size.cx;
	}

	if (out_buffer)
		RtlSecureZeroMemory (out_buffer, sizeof (SIZE));

	return 0;
}

VOID _r_dc_getsizedpivalue (
	_Inout_ PR_SIZE size,
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_unpack
)
{
	LONG denominator;
	LONG numerator;

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

	size->cx = _r_calc_multipledivide (size->cx, numerator, denominator);
	size->cy = _r_calc_multipledivide (size->cy, numerator, denominator);
}

LONG _r_dc_getsystemmetrics (
	_In_ LONG index,
	_In_opt_ LONG dpi_value
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GSMFD _GetSystemMetricsForDpi = NULL;

	PVOID huser32;
	NTSTATUS status;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"user32.dll", 0, &huser32);

		if (NT_SUCCESS (status))
		{
			// win10rs1+
			_GetSystemMetricsForDpi = (GSMFD)_r_sys_getprocaddress (huser32, "GetSystemMetricsForDpi", 0);

			_r_sys_freelibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (dpi_value && _GetSystemMetricsForDpi)
		return _GetSystemMetricsForDpi (index, dpi_value);

	return GetSystemMetrics (index);
}

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (
	_In_ UINT action,
	_In_ UINT param1,
	_Pre_maybenull_ _Post_valid_ PVOID param2,
	_In_opt_ LONG dpi_value
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SPIFP _SystemParametersInfoForDpi = NULL;

	PVOID huser32;
	NTSTATUS status;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"user32.dll", 0, &huser32);

		if (NT_SUCCESS (status))
		{
			// win10rs1+
			_SystemParametersInfoForDpi = (SPIFP)_r_sys_getprocaddress (huser32, "SystemParametersInfoForDpi", 0);

			_r_sys_freelibrary (huser32);
		}

		_r_initonce_end (&init_once);
	}

	// win10rs1+
	if (dpi_value && _SystemParametersInfoForDpi)
		return !!_SystemParametersInfoForDpi (action, param1, param2, 0, dpi_value);

	return !!SystemParametersInfoW (action, param1, param2, 0);
}

LONG _r_dc_gettaskbardpi ()
{
	APPBARDATA taskbar_rect = {0};

	taskbar_rect.cbSize = sizeof (taskbar_rect);

	if (SHAppBarMessage (ABM_GETTASKBARPOS, &taskbar_rect))
		return _r_dc_getmonitordpi (&taskbar_rect.rc);

	return _r_dc_getdpivalue (NULL, NULL);
}

_Ret_maybenull_
HBITMAP _r_dc_getuacshield (
	_In_opt_ LONG dpi_value,
	_In_opt_ LONG width,
	_In_opt_ LONG height
)
{
	HBITMAP hbitmap = NULL;
	HICON hicon;
	LONG icon_size_x;
	LONG icon_size_y;
	HRESULT status;

	if (width && height)
	{
		icon_size_x = width;
		icon_size_y = height;
	}
	else
	{
		icon_size_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
		icon_size_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);
	}

	status = _r_sys_loadicon (NULL, IDI_SHIELD, icon_size_x, &hicon);

	if (SUCCEEDED (status))
	{
		hbitmap = _r_dc_bitmapfromicon (hicon, icon_size_x, icon_size_y);

		DestroyIcon (hicon);
	}

	return hbitmap;
}

LONG _r_dc_getwindowdpi (
	_In_ HWND hwnd
)
{
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return _r_dc_getdpivalue (hwnd, NULL);

	return _r_dc_getdpivalue (NULL, &rect);
}

_Ret_maybenull_
HTHEME _r_dc_openthemedata (
	_In_opt_ HWND hwnd,
	_In_ PCWSTR class_list,
	_In_opt_ LONG dpi_value
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static OTDFD _OpenThemeDataForDpi = NULL;

	PVOID huxtheme;
	NTSTATUS status;

	// initialize library calls
	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"uxtheme.dll", 0, &huxtheme);

		if (NT_SUCCESS (status))
		{
			// win10rs2+
			_OpenThemeDataForDpi = (OTDFD)_r_sys_getprocaddress (huxtheme, "OpenThemeDataForDpi", 0);

			_r_sys_freelibrary (huxtheme);
		}

		_r_initonce_end (&init_once);
	}

	// win10rs2+
	if (dpi_value && _OpenThemeDataForDpi)
		return _OpenThemeDataForDpi (hwnd, class_list, dpi_value);

	// vista+
	return OpenThemeData (hwnd, class_list);
}

//
// File dialog
//

_Success_ (SUCCEEDED (return))
HRESULT _r_filedialog_initialize (
	_Out_ PR_FILE_DIALOG file_dialog,
	_In_ ULONG flags
)
{
	IFileDialog* ifd = NULL;
	HRESULT status;

	status = CoCreateInstance (
		(flags & (PR_FILEDIALOG_OPENFILE | PR_FILEDIALOG_OPENDIR)) ? &CLSID_FileOpenDialog : &CLSID_FileSaveDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IFileDialog,
		&ifd
	);

	if (SUCCEEDED (status))
	{
		file_dialog->flags = flags;
		file_dialog->ifd = ifd;
	}

	return status;
}

VOID _r_filedialog_destroy (
	_In_ PR_FILE_DIALOG file_dialog
)
{
	IFileDialog_Release (file_dialog->ifd);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_filedialog_getpath (
	_In_ PR_FILE_DIALOG file_dialog,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PR_STRING file_name = NULL;
	IShellItem* isi;
	LPWSTR name;
	HRESULT status;

	status = IFileDialog_GetResult (file_dialog->ifd, &isi);

	if (SUCCEEDED (status))
	{
		status = IShellItem_GetDisplayName (isi, SIGDN_FILESYSPATH, &name);

		if (SUCCEEDED (status))
		{
			file_name = _r_obj_createstring (name);

			CoTaskMemFree (name);
		}

		IShellItem_Release (isi);
	}

	if (!file_name)
	{
		status = IFileDialog_GetFileName (file_dialog->ifd, &name);

		if (SUCCEEDED (status))
		{
			file_name = _r_obj_createstring (name);

			CoTaskMemFree (name);
		}
	}

	*out_buffer = file_name;

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_filedialog_show (
	_In_opt_ HWND hwnd,
	_In_ PR_FILE_DIALOG file_dialog
)
{
	FILEOPENDIALOGOPTIONS options = 0;
	HRESULT status;

	// Set a blank default extension. This will have an effect when the user selects a different file type.
	IFileDialog_SetDefaultExtension (file_dialog->ifd, L"");

	IFileDialog_GetOptions (file_dialog->ifd, &options);

	if ((file_dialog->flags & PR_FILEDIALOG_OPENDIR))
		options |= FOS_PICKFOLDERS;

	IFileDialog_SetOptions (file_dialog->ifd, options | FOS_DONTADDTORECENT | FOS_FORCESHOWHIDDEN);

	status = IFileDialog_Show (file_dialog->ifd, hwnd);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_filedialog_setfilter (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ LPCOMDLG_FILTERSPEC filters,
	_In_ ULONG count
)
{
	return IFileDialog_SetFileTypes (file_dialog->ifd, count, filters);
}

VOID _r_filedialog_setpath (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ LPWSTR path
)
{
	IShellItem* isi = NULL;
	R_STRINGREF directory_part;
	R_STRINGREF basename_part;
	LPITEMIDLIST item;
	SFGAOF attributes;
	PR_STRING directory;
	R_STRINGREF sr;
	HRESULT status;

	_r_obj_initializestringref (&sr, path);

	if (_r_path_getpathinfo (&sr, &directory_part, &basename_part))
	{
		directory = _r_obj_createstring2 (&directory_part);

		status = SHParseDisplayName (directory->buffer, NULL, &item, 0, &attributes);

		if (SUCCEEDED (status))
		{
			SHCreateShellItem (NULL, NULL, item, &isi);

			CoTaskMemFree (item);
		}

		_r_obj_dereference (directory);
	}

	if (isi)
	{
		IFileDialog_SetFolder (file_dialog->ifd, isi);
		IFileDialog_SetFileName (file_dialog->ifd, basename_part.buffer);

		IShellItem_Release (isi);
	}
	else
	{
		IFileDialog_SetFileName (file_dialog->ifd, path);
	}
}

//
// Window layout
//

VOID _r_layout_initializemanager (
	_Out_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd
)
{
	R_RECTANGLE client_rect;
	R_RECTANGLE rect;
	LONG dpi_value;

	RtlZeroMemory (layout_manager, sizeof (R_LAYOUT_MANAGER));

	if (!_r_wnd_getposition (hwnd, &rect))
		return;

	if (!_r_wnd_getclientsize (hwnd, &client_rect))
		return;

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

	layout_manager->list = _r_obj_createlist (1, &_r_mem_free);

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

	layout_item = _r_mem_allocate (sizeof (R_LAYOUT_ITEM));

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
	PR_STRING class_name;
	ULONG flags = 0;

	class_name = _r_wnd_getclassname (hwnd);

	if (!class_name)
		return 0;

	if (_r_str_isequal2 (&class_name->sr, WC_STATICW, TRUE))
	{
		flags = PR_LAYOUT_FORCE_INVALIDATE;
	}
	else if (_r_str_isequal2 (&class_name->sr, STATUSCLASSNAMEW, TRUE))
	{
		flags = PR_LAYOUT_SEND_NOTIFY | PR_LAYOUT_NO_ANCHOR;
	}
	else if (_r_str_isequal2 (&class_name->sr, REBARCLASSNAMEW, TRUE))
	{
		flags = PR_LAYOUT_SEND_NOTIFY | PR_LAYOUT_NO_ANCHOR;
	}

	return flags;
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

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
	{
		layout_item = _r_obj_getlistitem (layout_manager->list, i);

		if (!layout_item)
			continue;

		if (layout_item->flags & PR_LAYOUT_SEND_NOTIFY)
		{
			PostMessageW (layout_item->hwnd, WM_SIZE, 0, 0);

			InvalidateRect (layout_item->hwnd, NULL, FALSE);
		}

		if (!(layout_item->flags & PR_LAYOUT_NO_ANCHOR))
			_r_layout_resizeitem (layout_manager, layout_item);
	}

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
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
	LONG height;
	LONG width;
	LONG sx;
	LONG sy;

	if (layout_item->number_of_children > 0 && !layout_item->defer_handle)
		layout_item->defer_handle = BeginDeferWindowPos (layout_item->number_of_children);

	// If this is the root item we must stop here.
	if (!layout_item->parent_item)
		return;

	_r_layout_resizeitem (layout_manager, layout_item->parent_item);

	// save previous value
	_r_wnd_rectangletorect (&prev_rect, &layout_item->parent_item->prev_rect);
	_r_wnd_rectangletorect (&parent_rect, &layout_item->parent_item->rect);
	_r_wnd_rectangletorect (&rect, &layout_item->rect);

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

VOID _r_layout_setitemanchor (
	_Inout_ PR_LAYOUT_ITEM layout_item
)
{
	LONG horz_break;
	LONG vert_break;
	LONG height;
	LONG width;

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

VOID _r_layout_setoriginalsize (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ LONG width,
	_In_ LONG height
)
{
	layout_manager->original_size.cx = width;
	layout_manager->original_size.cy = height;
}

BOOLEAN _r_layout_setwindowanchor (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd,
	_In_ ULONG anchor
)
{
	PR_LAYOUT_ITEM layout_item;

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (layout_manager->list); i++)
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

BOOLEAN _r_wnd_addstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG_PTR mask,
	_In_ LONG_PTR state_mask,
	_In_ INT index
)
{
	HWND htarget;
	LONG_PTR style;

	htarget = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!htarget)
		return FALSE;

	style = (GetWindowLongPtrW (htarget, index) & ~state_mask) | mask;

	SetWindowLongPtrW (htarget, index, style);

	return !!SetWindowPos (
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

	if (rectangle->width > bounds->width)
		rectangle->width = bounds->width;

	if (rectangle->height > bounds->height)
		rectangle->height = bounds->height;
}

VOID _r_wnd_adjustrectangletoworkingarea (
	_Inout_ PR_RECTANGLE rectangle,
	_In_opt_ HWND hwnd
)
{
	MONITORINFO monitor_info = {0};
	R_RECTANGLE bounds;
	HMONITOR hmonitor;
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

	if (!GetMonitorInfoW (hmonitor, &monitor_info))
		return;

	_r_wnd_recttorectangle (&bounds, &monitor_info.rcWork);
	_r_wnd_adjustrectangletobounds (rectangle, &bounds);
}

VOID _r_wnd_calculateoverlappedrect (
	_In_ HWND hwnd,
	_Inout_ PRECT window_rect
)
{
	RECT rect_intersection;
	RECT rect_current;
	HWND hwnd_current;

	hwnd_current = hwnd;

	while ((hwnd_current = GetWindow (hwnd_current, GW_HWNDPREV)) && hwnd_current != hwnd)
	{
		if (!(_r_wnd_getstyle (hwnd_current, GWL_STYLE) & WS_VISIBLE))
			continue;

		if (!GetWindowRect (hwnd_current, &rect_current))
			continue;

		if ((_r_wnd_ismenu (hwnd_current) || !(_r_wnd_getstyle (hwnd_current, GWL_EXSTYLE) & WS_EX_TOPMOST)) && IntersectRect (&rect_intersection, window_rect, &rect_current))
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

_Success_ (return)
BOOLEAN _r_wnd_center (
	_In_ HWND hwnd,
	_In_opt_ HWND hparent
)
{
	MONITORINFO monitor_info = {0};
	R_RECTANGLE parent_rect;
	R_RECTANGLE rectangle;
	HMONITOR hmonitor;

	if (hparent)
	{
		if (!_r_wnd_isvisible (hparent, TRUE))
			return FALSE;

		if (IsIconic (hparent))
			return FALSE;

		if (!_r_wnd_getposition (hwnd, &rectangle) || !_r_wnd_getposition (hparent, &parent_rect))
			return FALSE;

		_r_wnd_centerwindowrect (&rectangle, &parent_rect);
		_r_wnd_adjustrectangletoworkingarea (&rectangle, hwnd);

		_r_wnd_setposition (hwnd, &rectangle.position, &rectangle.size);

		return TRUE;
	}

	monitor_info.cbSize = sizeof (monitor_info);

	hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

	if (!GetMonitorInfoW (hmonitor, &monitor_info))
		return FALSE;

	if (!_r_wnd_getposition (hwnd, &rectangle))
		return FALSE;

	_r_wnd_recttorectangle (&parent_rect, &monitor_info.rcWork);
	_r_wnd_centerwindowrect (&rectangle, &parent_rect);

	_r_wnd_setposition (hwnd, &rectangle.position, &rectangle.size);

	return TRUE;
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
	_In_ ULONG_PTR count,
	_In_ ULONG action
)
{
	for (ULONG_PTR i = 0; i < count; i++)
	{
		ChangeWindowMessageFilterEx (hwnd, messages[i], action, NULL); // win7+
	}
}

INT_PTR _r_wnd_createmodalwindow (
	_In_ PVOID hinst,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
)
{
	R_STORAGE buffer;
	INT_PTR result;
	NTSTATUS status;

	status = _r_res_loadresource (hinst, RT_DIALOG, name, 0, &buffer);

	if (!NT_SUCCESS (status))
		return 0;

	result = DialogBoxIndirectParamW (hinst, (LPCDLGTEMPLATE)buffer.buffer, hparent, dlg_proc, (LPARAM)lparam);

	return result;
}

_Ret_maybenull_
HWND _r_wnd_createwindow (
	_In_ PVOID hinst,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
)
{
	R_STORAGE buffer;
	HWND hwnd;
	NTSTATUS status;

	status = _r_res_loadresource (hinst, RT_DIALOG, name, 0, &buffer);

	if (!NT_SUCCESS (status))
		return NULL;

	hwnd = CreateDialogIndirectParamW (hinst, (LPCDLGTEMPLATE)buffer.buffer, hparent, dlg_proc, (LPARAM)lparam);

	return hwnd;
}

_Ret_maybenull_
PR_STRING _r_wnd_getclassname (
	_In_ HWND hwnd
)
{
	PR_STRING string;
	ULONG length = 128;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	length = GetClassNameW (hwnd, string->buffer, length);

	if (!length)
	{
		_r_obj_dereference (string);

		return NULL;
	}

	_r_str_setlength (&string->sr, length * sizeof (WCHAR));

	return string;
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

BOOLEAN _r_wnd_isdarkmodeenabled ()
{
	HANDLE hkey;
	ULONG value;
	BOOLEAN is_enabled = FALSE;
	NTSTATUS status;

	// "ShouldAppsUseDarkMode" is incorrect started from 1903+, use this instead!
	// https://github.com/ysc3839/win32-darkmode/issues/3
	status = _r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hkey);

	if (NT_SUCCESS (status))
	{
		status = _r_reg_queryulong (hkey, L"AppsUseLightTheme", &value);

		if (NT_SUCCESS (status))
			is_enabled = (value == 0);

		NtClose (hkey);
	}

	return is_enabled;
}

BOOLEAN _r_wnd_isfocusassist ()
{
	static R_INITONCE init_once = {0};
	static NTQWNFSD _NtQueryWnfStateData = NULL;

	WNF_CHANGE_STAMP change_stamp;
	WNF_STATE_NAME state_name = {0};
	PVOID hntdll;
	ULONG buffer_length;
	ULONG buffer = 0;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"ntdll.dll", 0, &hntdll);

		if (NT_SUCCESS (status))
		{
			// win10rs3+
			_NtQueryWnfStateData = (NTQWNFSD)_r_sys_getprocaddress (hntdll, "NtQueryWnfStateData", 0);

			_r_sys_freelibrary (hntdll);
		}

		_r_initonce_end (&init_once);
	}

	if (!_NtQueryWnfStateData)
		return FALSE;

	// WNF_SHEL_QUIETHOURS_ACTIVE_PROFILE_CHANGED
	// This wnf is signaled whenever active quiet hours profile / mode changes.
	// The value is the restrictive level of an active profile and
	// the restrictive level is unique per profile

	state_name.Data[0] = 0xA3BF1C75;
	state_name.Data[1] = 0xD83063E;

	buffer_length = sizeof (ULONG);

	status = _NtQueryWnfStateData (&state_name, NULL, NULL, &change_stamp, &buffer, &buffer_length);

	if (NT_SUCCESS (status))
	{
		if (buffer == FOCUS_ASSIST_PRIORITY_ONLY || buffer == FOCUS_ASSIST_ALARMS_ONLY)
			return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_wnd_isfullscreenconsolemode (
	_In_ HWND hwnd
)
{
	ULONG modes = 0;
	ULONG pid = 0;

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
	QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
	HRESULT status;

	// vista+
	status = SHQueryUserNotificationState (&state);

	if (FAILED (status))
		return FALSE;

	return (state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE);
}

BOOLEAN _r_wnd_isfullscreenwindowmode (
	_In_ HWND hwnd
)
{
	MONITORINFO monitor_info = {0};
	HMONITOR hmonitor;
	RECT rect;
	LONG_PTR style;
	LONG_PTR ex_style;

	// Get the monitor where the window is located.
	if (!GetWindowRect (hwnd, &rect))
		return FALSE;

	hmonitor = MonitorFromRect (&rect, MONITOR_DEFAULTTONULL);

	if (!hmonitor)
		return FALSE;

	monitor_info.cbSize = sizeof (monitor_info);

	if (!GetMonitorInfoW (hmonitor, &monitor_info))
		return FALSE;

	// It should be the main monitor.
	if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
		return FALSE;

	// The window should be at least as large as the monitor.
	if (!IntersectRect (&rect, &rect, &monitor_info.rcMonitor))
		return FALSE;

	if (!EqualRect (&rect, &monitor_info.rcMonitor))
		return FALSE;

	// At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
	// its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
	style = _r_wnd_getstyle (hwnd, GWL_STYLE);
	ex_style = _r_wnd_getstyle (hwnd, GWL_EXSTYLE);

	return !((style & (WS_DLGFRAME | WS_THICKFRAME)) || (ex_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

BOOLEAN _r_wnd_isfullscreenmode ()
{
	HWND hwnd;

	// win10rs3+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS3))
	{
		if (_r_wnd_isfocusassist ())
			return TRUE;
	}

	// vista+
	if (_r_wnd_isfullscreenusermode ())
		return TRUE;

	// Get the foreground window which the user is currently working on.
	hwnd = GetForegroundWindow ();

	if (!hwnd)
		return FALSE;

	return _r_wnd_isfullscreenwindowmode (hwnd) || _r_wnd_isfullscreenconsolemode (hwnd);
}

BOOLEAN _r_wnd_isoverlapped (
	_In_ HWND hwnd
)
{
	RECT rect_intersection;
	RECT rect_original;
	RECT rect_current;
	HWND hwnd_current;

	if (!GetWindowRect (hwnd, &rect_original))
		return FALSE;

	hwnd_current = hwnd;

	while ((hwnd_current = GetWindow (hwnd_current, GW_HWNDPREV)) && hwnd_current != hwnd)
	{
		if (!(_r_wnd_getstyle (hwnd_current, GWL_STYLE) & WS_VISIBLE))
			continue;

		if (!GetWindowRect (hwnd_current, &rect_current))
			continue;

		if (!(_r_wnd_getstyle (hwnd_current, GWL_EXSTYLE) & WS_EX_TOPMOST) && IntersectRect (&rect_intersection, &rect_original, &rect_current))
			return TRUE;
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

	if (!_r_wnd_isvisible (hwnd, TRUE))
		return FALSE;

	if (!GetCursorPos (&point) || !GetWindowRect (hwnd, &rect))
		return FALSE;

	return !!PtInRect (&rect, point);
}

BOOLEAN _r_wnd_isvisible (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_checkminimize
)
{
	if (is_checkminimize && _r_wnd_isminimized (hwnd))
		return FALSE;

	return !!IsWindowVisible (hwnd);
}

ULONG CALLBACK _r_wnd_message_callback (
	_In_ HWND hwnd,
	_In_opt_ LPCWSTR accelerator_table
)
{
	MSG msg;
	HACCEL haccelerator = NULL;
	HWND hactive_wnd;
	INT result;
	BOOLEAN is_processed;
	ULONG status = ERROR_SUCCESS;

	if (accelerator_table)
		haccelerator = LoadAcceleratorsW (NULL, accelerator_table);

	while (TRUE)
	{
		result = GetMessageW (&msg, NULL, 0, 0);

		if (msg.message == WM_QUIT)
			status = (ULONG)msg.wParam;

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
				hactive_wnd = hwnd;
			}
		}

		if (haccelerator)
		{
			if (TranslateAcceleratorW (hactive_wnd, haccelerator, &msg))
				is_processed = TRUE;
		}

		if (IsDialogMessageW (hwnd, &msg))
			is_processed = TRUE;

		if (!is_processed)
		{
			TranslateMessage (&msg);
			DispatchMessageW (&msg);
		}
	}

	if (haccelerator)
		DestroyAcceleratorTable (haccelerator);

	return status;
}

VOID CALLBACK _r_wnd_message_dpichanged (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
)
{
	//R_RECTANGLE rectangle;
	//LPCRECT rect;

	UNREFERENCED_PARAMETER (hwnd);
	UNREFERENCED_PARAMETER (wparam);
	UNREFERENCED_PARAMETER (lparam);

	//rect = (LPCRECT)lparam;

	//if (!rect)
	//	return;

	//_r_wnd_recttorectangle (&rectangle, rect);

	//_r_wnd_setposition (hwnd, &rectangle.position, &rectangle.size);
}

VOID CALLBACK _r_wnd_message_settingchange (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static RICPS _RefreshImmersiveColorPolicyState = NULL;
	static GIICUHC _GetIsImmersiveColorUsingHighContrast = NULL;

	R_STRINGREF sr;
	PVOID huxtheme;
	LPWSTR type;
	NTSTATUS status;

	UNREFERENCED_PARAMETER (wparam);

	type = (LPWSTR)lparam;

	if (!type)
		return;

	_r_obj_initializestringref (&sr, type);

	if (_r_str_isequal2 (&sr, L"WindowMetrics", TRUE))
	{
		_r_wnd_sendmessage (hwnd, 0, RM_LOCALIZE, 0, 0);
	}
	else if (_r_str_isequal2 (&sr, L"ImmersiveColorSet", TRUE))
	{
		if (_r_initonce_begin (&init_once))
		{
			status = _r_sys_getmodulehandle (L"uxtheme.dll", &huxtheme);

			if (NT_SUCCESS (status))
			{
				_RefreshImmersiveColorPolicyState = (RICPS)_r_sys_getprocaddress (huxtheme, NULL, 104);
				_GetIsImmersiveColorUsingHighContrast = (GIICUHC)_r_sys_getprocaddress (huxtheme, NULL, 106);
			}

			_r_initonce_end (&init_once);
		}

		if (_RefreshImmersiveColorPolicyState)
			_RefreshImmersiveColorPolicyState ();

		if (_GetIsImmersiveColorUsingHighContrast)
			_GetIsImmersiveColorUsingHighContrast (IHCM_REFRESH);
	}
}

LRESULT _r_wnd_sendmessage (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ UINT msg,
	_Pre_maybenull_ _Post_valid_ WPARAM wparam,
	_Pre_maybenull_ _Post_valid_ LPARAM lparam
)
{
	LRESULT val;

	if (ctrl_id)
	{
		val = SendDlgItemMessageW (hwnd, ctrl_id, msg, wparam, lparam);
	}
	else
	{
		val = SendMessageW (hwnd, msg, wparam, lparam);
	}

	return val;
}

VOID _r_wnd_setposition (
	_In_ HWND hwnd,
	_In_opt_ PR_SIZE position,
	_In_opt_ PR_SIZE size
)
{
	R_RECTANGLE rectangle = {0};
	UINT swp_flags;

	if (position && size)
	{
		MoveWindow (hwnd, position->cx, position->cy, size->cx, size->cy, TRUE);

		return;
	}

	swp_flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	if (position)
	{
		RtlCopyMemory (&rectangle.position, position, sizeof (R_SIZE));
	}
	else
	{
		swp_flags |= SWP_NOMOVE;
	}

	if (size)
	{
		RtlCopyMemory (&rectangle.size, size, sizeof (R_SIZE));
	}
	else
	{
		swp_flags |= SWP_NOSIZE;
	}

	SetWindowPos (hwnd, NULL, rectangle.left, rectangle.top, rectangle.width, rectangle.height, swp_flags);
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

VOID _r_wnd_setstyle (
	_In_ HWND hwnd,
	_In_ LONG_PTR mask,
	_In_ LONG_PTR value,
	_In_ INT index
)
{
	LONG_PTR style;

	style = GetWindowLongPtrW (hwnd, index);

	style = (style & ~mask) | (value & mask);

	SetWindowLongPtrW (hwnd, index, style);
}

_Ret_maybenull_
WNDPROC _r_wnd_getsubclass (
	_In_ HWND hwnd
)
{
	WNDPROC wnd_proc;

	wnd_proc = (WNDPROC)_r_wnd_getcontext (hwnd, LONG_MAX);

	if (wnd_proc)
		return wnd_proc;

	return NULL;
}

BOOLEAN _r_wnd_removesubclass (
	_In_ HWND hwnd
)
{
	WNDPROC wnd_proc;

	wnd_proc = (WNDPROC)_r_wnd_getcontext (hwnd, LONG_MAX);

	if (wnd_proc)
	{
		_r_wnd_removecontext (hwnd, LONG_MAX);

		SetWindowLongPtrW (hwnd, GWLP_WNDPROC, (LONG_PTR)wnd_proc);

		return TRUE;
	}

	return FALSE;
}

WNDPROC _r_wnd_setsubclass (
	_In_ HWND hwnd,
	_In_ LONG index,
	_In_ WNDPROC subclass_proc
)
{
	WNDPROC wnd_proc;

	wnd_proc = (WNDPROC)GetWindowLongPtrW (hwnd, index);

	if (wnd_proc != subclass_proc)
	{
		_r_wnd_setcontext (hwnd, LONG_MAX, wnd_proc);

		SetWindowLongPtrW (hwnd, index, (LONG_PTR)subclass_proc);
	}

	return wnd_proc;
}

VOID _r_wnd_toggle (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_show
)
{
	BOOLEAN is_minimized;
	BOOLEAN is_success;

	is_minimized = _r_wnd_isminimized (hwnd);

	if (is_show || !_r_wnd_isvisible (hwnd, FALSE) || is_minimized || _r_wnd_isoverlapped (hwnd))
	{
		is_success = !!ShowWindow (hwnd, is_minimized ? SW_RESTORE : SW_SHOW);

		if (!is_success)
		{
			// uipi fix
			if (NtLastError () == ERROR_ACCESS_DENIED)
				_r_wnd_sendmessage (hwnd, 0, WM_SYSCOMMAND, SC_RESTORE, 0);
		}

		SetForegroundWindow (hwnd);
	}
	else
	{
		ShowWindow (hwnd, SW_HIDE);
	}
}

PR_HASHTABLE _r_wnd_getcontext_table ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_HASHTABLE hashtable = NULL;

	if (_r_initonce_begin (&init_once))
	{
		hashtable = _r_obj_createhashtable (sizeof (R_OBJECT_POINTER), 8, NULL);

		_r_initonce_end (&init_once);
	}

	return hashtable;
}

FORCEINLINE ULONG _r_wnd_getcontext_hash (
	_In_ HWND hwnd,
	_In_ ULONG property_id
)
{
	return _r_math_hashinteger_ptr ((ULONG_PTR)hwnd) ^ property_id;
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

	if (!object_pointer)
		return NULL;

	return object_pointer->object_body;
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

VOID _r_wnd_setcontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id,
	_In_ PVOID context
)
{
	R_OBJECT_POINTER object_pointer = {0};
	PR_HASHTABLE hashtable;
	ULONG hash_code;

	hashtable = _r_wnd_getcontext_table ();
	hash_code = _r_wnd_getcontext_hash (hwnd, property_id);

	object_pointer.object_body = context;

	_r_queuedlock_acquireexclusive (&_r_context_lock);
	_r_obj_addhashtableitem (hashtable, hash_code, &object_pointer);
	_r_queuedlock_releaseexclusive (&_r_context_lock);
}

//
// Inernet access (WinHTTP)
//

_Ret_maybenull_
HINTERNET _r_inet_createsession (
	_In_opt_ PR_STRING useragent,
	_In_opt_ PR_STRING proxy
)
{
	R_URLPARTS proxy_parts;
	WCHAR proxy_buffer[256];
	HINTERNET hsession = NULL;
	ULONG protocols;

	// proxy configuration
	if (!_r_obj_isstringempty (proxy))
	{
		if (_r_inet_queryurlparts (&proxy->sr, PR_URLPARTS_HOST | PR_URLPARTS_PORT | PR_URLPARTS_USER | PR_URLPARTS_PASS, &proxy_parts))
		{
			_r_str_printf (proxy_buffer, RTL_NUMBER_OF (proxy_buffer), L"%s:%" TEXT (PRIu16), proxy_parts.host->buffer, proxy_parts.port);

			hsession = WinHttpOpen (_r_obj_getstring (useragent), WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxy_buffer, WINHTTP_NO_PROXY_BYPASS, 0);

			_r_inet_destroyurlparts (&proxy_parts);
		}
	}
	else
	{
		hsession = WinHttpOpen (
			_r_obj_getstring (useragent),
			_r_sys_isosversiongreaterorequal (WINDOWS_8_1) ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0
		);
	}

	if (!hsession)
		return NULL;

	// enable secure protocols
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
	}
	else
	{
		protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	}

	WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){protocols}, sizeof (ULONG));

	// disable redirect from https to http
	WinHttpSetOption (hsession, WINHTTP_OPTION_REDIRECT_POLICY, &(ULONG){WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP}, sizeof (ULONG));

	// enable compression feature
	WinHttpSetOption (hsession, WINHTTP_OPTION_DECOMPRESSION, &(ULONG){WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE}, sizeof (ULONG));

	// enable http2 protocol (win10+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		WinHttpSetOption (hsession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &(ULONG){WINHTTP_PROTOCOL_FLAG_HTTP2}, sizeof (ULONG));

	// disable global, cross-session pooling (win11+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_11))
		WinHttpSetOption (hsession, WINHTTP_OPTION_DISABLE_GLOBAL_POOLING, &(ULONG){TRUE}, sizeof (ULONG));

	return hsession;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (
	_In_ HINTERNET hsession,
	_In_ PR_STRINGREF url,
	_Out_ LPHINTERNET hconnect_ptr,
	_Out_ LPHINTERNET hrequest_ptr,
	_Out_opt_ PULONG total_length_ptr
)
{
	R_URLPARTS url_parts;
	HINTERNET hrequest = NULL;
	HINTERNET hconnect;
	ULONG flags = WINHTTP_FLAG_REFRESH;
	ULONG attempts = 6;
	BOOL is_valid;
	ULONG status;

	*hconnect_ptr = NULL;
	*hrequest_ptr = NULL;

	if (total_length_ptr)
		*total_length_ptr = 0;

	if (!_r_inet_queryurlparts (url, PR_URLPARTS_SCHEME | PR_URLPARTS_HOST | PR_URLPARTS_PORT | PR_URLPARTS_PATH, &url_parts))
		return NtLastError ();

	hconnect = WinHttpConnect (hsession, url_parts.host->buffer, url_parts.port, 0);

	if (!hconnect)
	{
		status = NtLastError ();

		goto CleanupExit;
	}

	if (url_parts.scheme == INTERNET_SCHEME_HTTPS)
		flags |= WINHTTP_FLAG_SECURE;

	hrequest = WinHttpOpenRequest (hconnect, NULL, url_parts.path->buffer, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

	if (!hrequest)
	{
		status = NtLastError ();

		_r_inet_close (hconnect);

		goto CleanupExit;
	}

	// disable "keep-alive" feature (win7+)
	WinHttpSetOption (hrequest, WINHTTP_OPTION_DISABLE_FEATURE, &(ULONG){WINHTTP_DISABLE_KEEP_ALIVE}, sizeof (ULONG));

	do
	{
		is_valid = WinHttpSendRequest (hrequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH, 0);

		if (!is_valid)
		{
			status = NtLastError ();

			if (status == ERROR_WINHTTP_RESEND_REQUEST)
			{
				continue;
			}
			else if (status == ERROR_WINHTTP_CONNECTION_ERROR)
			{
				is_valid = WinHttpSetOption (hsession, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2}, sizeof (ULONG));

				if (!is_valid)
					break;
			}
			else if (status == ERROR_WINHTTP_SECURE_FAILURE)
			{
				is_valid = WinHttpSetOption (hrequest, WINHTTP_OPTION_SECURITY_FLAGS, &(ULONG){SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE}, sizeof (ULONG));

				if (!is_valid)
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
			if (WinHttpReceiveResponse (hrequest, NULL))
			{
				if (_r_inet_querystatuscode (hrequest) != HTTP_STATUS_OK)
				{
					status = ERROR_WINHTTP_INVALID_SERVER_RESPONSE;

					break;
				}

				*hconnect_ptr = hconnect;
				*hrequest_ptr = hrequest;

				if (total_length_ptr)
					*total_length_ptr = _r_inet_querycontentlength (hrequest);

				status = ERROR_SUCCESS;

				goto CleanupExit;
			}
		}
	}
	while (--attempts);

CleanupExit:

	if (status != ERROR_SUCCESS)
	{
		if (hrequest)
			_r_inet_close (hrequest);

		if (hconnect)
			_r_inet_close (hconnect);
	}

	_r_inet_destroyurlparts (&url_parts);

	return status;
}

_Success_ (return)
BOOLEAN _r_inet_readrequest (
	_In_ HINTERNET hrequest,
	_Out_writes_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length,
	_Out_opt_ PULONG readed_ptr,
	_Inout_opt_ PULONG total_readed_ptr
)
{
	ULONG readed = 0;

	if (!WinHttpReadData (hrequest, buffer, buffer_length, &readed) || !readed)
		return FALSE;

	if (readed_ptr)
		*readed_ptr = readed;

	if (total_readed_ptr)
		*total_readed_ptr += readed;

	return TRUE;
}

_Success_ (return != 0)
ULONG _r_inet_querycontentlength (
	_In_ HINTERNET hrequest
)
{
	ULONG content_length = 0;
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

_Success_ (return != 0)
LONG64 _r_inet_querylastmodified (
	_In_ HINTERNET hrequest
)
{
	SYSTEMTIME lastmod = {0};
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

_Success_ (return != 0)
ULONG _r_inet_querystatuscode (
	_In_ HINTERNET hrequest
)
{
	ULONG status = 0;
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
	_Out_ PR_DOWNLOAD_INFO download_info,
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_INET_DOWNLOAD_CALLBACK download_callback,
	_In_opt_ PVOID lparam
)
{
	download_info->is_savetofile = (hfile != NULL);

	if (download_info->is_savetofile)
	{
		download_info->hfile = hfile;
	}
	else
	{
		download_info->string = NULL;
	}

	download_info->download_callback = download_callback;
	download_info->lparam = lparam;
}

NTSTATUS _r_inet_begindownload (
	_In_ HINTERNET hsession,
	_In_ PR_STRINGREF url,
	_Inout_ PR_DOWNLOAD_INFO download_info
)
{
	R_STRINGBUILDER sb;
	HINTERNET hconnect;
	HINTERNET hrequest;
	PR_BYTE content_bytes;
	PR_STRING string;
	ULONG allocated_length;
	ULONG total_readed = 0;
	ULONG total_length;
	ULONG readed_length;
	NTSTATUS status;

	status = _r_inet_openurl (hsession, url, &hconnect, &hrequest, &total_length);

	if (status != ERROR_SUCCESS)
		return status;

	if (!download_info->is_savetofile)
		_r_obj_initializestringbuilder (&sb, PR_SIZE_BUFFER);

	allocated_length = PR_SIZE_BUFFER;
	content_bytes = _r_obj_createbyte_ex (NULL, allocated_length);

	while (_r_inet_readrequest (hrequest, content_bytes->buffer, allocated_length, &readed_length, &total_readed))
	{
		_r_sys_setthreadexecutionstate (ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);

		_r_obj_setbytelength_ex (content_bytes, readed_length, allocated_length);

		if (download_info->is_savetofile)
		{
			status = _r_fs_writefile (download_info->hfile, content_bytes->buffer, readed_length);

			if (!NT_SUCCESS (status))
				break;
		}
		else
		{
			status = _r_str_multibyte2unicode (&content_bytes->sr, &string);

			if (NT_SUCCESS (status))
			{
				_r_obj_appendstringbuilder2 (&sb, &string->sr);

				_r_obj_dereference (string);
			}
			else
			{
				break;
			}
		}

		if (download_info->download_callback)
		{
			if (!download_info->download_callback (total_readed, max (total_readed, total_length), download_info->lparam))
			{
				status = STATUS_CANCELLED;

				break;
			}
		}
	}

	if (!download_info->is_savetofile)
	{
		if (status == STATUS_SUCCESS)
		{
			download_info->string = _r_obj_finalstringbuilder (&sb);
		}
		else
		{
			_r_obj_deletestringbuilder (&sb);

			download_info->string = NULL;
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
		SAFE_DELETE_HANDLE (download_info->hfile);
	}
	else
	{
		SAFE_DELETE_REFERENCE (download_info->string);
	}
}

_Success_ (return)
BOOLEAN _r_inet_queryurlparts (
	_In_ PR_STRINGREF url,
	_In_ ULONG flags,
	_Out_ PR_URLPARTS url_parts
)
{
	URL_COMPONENTS url_comp = {0};
	PR_STRING url_fixed;
	ULONG length = 256;

	url_comp.dwStructSize = sizeof (url_comp);

	RtlZeroMemory (url_parts, sizeof (R_URLPARTS));

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
		if (NtLastError () != ERROR_WINHTTP_UNRECOGNIZED_SCHEME)
		{
			_r_inet_destroyurlparts (url_parts);

			return FALSE;
		}

		url_fixed = _r_format_string (L"https://%s", url->buffer);

		if (!WinHttpCrackUrl (url_fixed->buffer, (ULONG)_r_str_getlength2 (&url_fixed->sr), ICU_DECODE, &url_comp))
		{
			_r_inet_destroyurlparts (url_parts);
			_r_obj_dereference (url_fixed);

			return FALSE;
		}

		_r_obj_dereference (url_fixed);
	}

	if (flags & PR_URLPARTS_SCHEME)
		url_parts->scheme = url_comp.nScheme;

	if (flags & PR_URLPARTS_PORT)
		url_parts->port = url_comp.nPort;

	if (flags & PR_URLPARTS_HOST)
		_r_str_trimtonullterminator (&url_parts->host->sr);

	if (flags & PR_URLPARTS_PATH)
		_r_str_trimtonullterminator (&url_parts->path->sr);

	if (flags & PR_URLPARTS_USER)
		_r_str_trimtonullterminator (&url_parts->user->sr);

	if (flags & PR_URLPARTS_PASS)
		_r_str_trimtonullterminator (&url_parts->pass->sr);

	return TRUE;
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

HANDLE _r_reg_getroothandle (
	_In_ HANDLE hroot
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE hkeyhkcr = NULL;
	static HANDLE hkeyhkcu = NULL;
	static HANDLE hkeyhklm = NULL;
	static HANDLE hkeyhku = NULL;
	static HANDLE hkeyhcc = NULL;

	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		// HKEY_CLASSES_ROOT
		status = _r_reg_openkey (NULL, L"\\Registry\\Machine\\Software\\CLASSES", 0, MAXIMUM_ALLOWED, &hkeyhkcr);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		// HKEY_CURRENT_USER
		status = RtlOpenCurrentUser (MAXIMUM_ALLOWED, &hkeyhkcu);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		// HKEY_LOCAL_MACHINE
		status = _r_reg_openkey (NULL, L"\\Registry\\Machine", 0, MAXIMUM_ALLOWED, &hkeyhklm);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		// HKEY_USERS
		status = _r_reg_openkey (NULL, L"\\Registry\\User", 0, MAXIMUM_ALLOWED, &hkeyhku);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		// HKEY_CURRENT_CONFIG
		status = _r_reg_openkey (NULL, L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current", 0, MAXIMUM_ALLOWED, &hkeyhcc);

		if (!NT_SUCCESS (status))
			RtlRaiseStatus (status);

		// HKEY_DYN_DATA
		//status = _r_reg_openkey (NULL, L"\\Registry\\DynData", 0, MAXIMUM_ALLOWED, &hkeydd);
		//
		//if (!NT_SUCCESS (status))
		//	RtlRaiseStatus (status); // have STATUS_OBJECT_NAME_NOT_FOUND

		_r_initonce_end (&init_once);
	}

	switch ((ULONG_PTR)hroot)
	{
		case HKEY_CLASSES_ROOT:
		{
			return hkeyhkcr;
		}

		case HKEY_CURRENT_USER:
		{
			return hkeyhkcu;
		}

		case HKEY_LOCAL_MACHINE:
		{
			return hkeyhklm;
		}

		case HKEY_USERS:
		{
			return hkeyhku;
		}

		case HKEY_CURRENT_CONFIG:
		{
			return hkeyhcc;
		}

		//case HKEY_DYN_DATA:
		//{
		//	return hkeydd;
		//}
	}

	return hroot;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_createkey (
	_In_ HANDLE hroot,
	_In_opt_ LPWSTR path,
	_In_ ULONG flags,
	_In_ ACCESS_MASK desired_access,
	_Out_opt_ PULONG disposition,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING us;
	HANDLE hroot_handle;
	HANDLE hkey;
	NTSTATUS status;

	hroot_handle = _r_reg_getroothandle (hroot);

	_r_obj_initializeunicodestring (&us, path);

	InitializeObjectAttributes (&oa, &us, OBJ_OPENIF | OBJ_CASE_INSENSITIVE, hroot_handle, NULL);

	if (flags & REG_OPTION_OPEN_LINK)
		oa.Attributes |= OBJ_OPENLINK;

	status = NtCreateKey (&hkey, desired_access, &oa, 0, NULL, flags, disposition);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hkey;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_openkey (
	_In_opt_ HANDLE hroot,
	_In_opt_ LPWSTR path,
	_In_ ULONG flags,
	_In_ ACCESS_MASK desired_access,
	_Out_ PHANDLE out_buffer
)
{
	OBJECT_ATTRIBUTES oa = {0};
	UNICODE_STRING us;
	HANDLE hroot_handle = NULL;
	HANDLE hkey;
	NTSTATUS status;

	if (hroot)
		hroot_handle = _r_reg_getroothandle (hroot);

	_r_obj_initializeunicodestring (&us, path);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE, hroot_handle, NULL);

	if (flags & REG_OPTION_OPEN_LINK)
		oa.Attributes |= OBJ_OPENLINK;

	// win7+
	status = NtOpenKeyEx (&hkey, desired_access, &oa, flags);

	if (NT_SUCCESS (status))
	{
		*out_buffer = hkey;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_deletevalue (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name
)
{
	UNICODE_STRING us;
	NTSTATUS status;

	_r_obj_initializeunicodestring (&us, value_name);

	status = NtDeleteValueKey (hkey, &us);

	return status;
}

// returns STATUS_NO_MORE_ENTRIES when no more items
_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_enumkey (
	_In_ HANDLE hkey,
	_In_ ULONG index,
	_Out_ PR_STRING_PTR out_name,
	_Out_opt_ PLONG64 out_timestamp
)
{
	PKEY_NODE_INFORMATION buffer;
	ULONG buffer_length = 0x100;
	NTSTATUS status;

	buffer = _r_mem_allocate (buffer_length);

	status = NtEnumerateKey (hkey, index, KeyNodeInformation, buffer, buffer_length, &buffer_length);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		buffer = _r_mem_reallocate (buffer, buffer_length);

		status = NtEnumerateKey (hkey, index, KeyNodeInformation, buffer, buffer_length, &buffer_length);
	}

	if (NT_SUCCESS (status))
	{
		*out_name = _r_obj_createstring_ex (buffer->Name, buffer->NameLength);

		if (out_timestamp)
			*out_timestamp = _r_unixtime_from_largeinteger (&buffer->LastWriteTime);
	}
	else
	{
		*out_name = NULL;

		if (out_timestamp)
			*out_timestamp = 0;
	}

	_r_mem_free (buffer);

	return status;
}

// returns STATUS_NO_MORE_ENTRIES when no more items
_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_enumvalues (
	_In_ HANDLE hkey,
	_In_ ULONG index,
	_Out_ PR_STRING_PTR out_name,
	_Out_opt_ PR_STRING_PTR out_value,
	_Out_opt_ PULONG out_type
)
{
	PKEY_VALUE_FULL_INFORMATION buffer;
	R_STRINGREF sr;
	PR_STRING string;
	ULONG buffer_length = 0x100;
	NTSTATUS status;

	buffer = _r_mem_allocate (buffer_length);

	status = NtEnumerateValueKey (hkey, index, KeyValueFullInformation, buffer, buffer_length, &buffer_length);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		buffer = _r_mem_reallocate (buffer, buffer_length);

		status = NtEnumerateValueKey (hkey, index, KeyValueFullInformation, buffer, buffer_length, &buffer_length);
	}

	if (NT_SUCCESS (status))
	{
		*out_name = _r_obj_createstring_ex (buffer->Name, buffer->NameLength);

		if (out_value)
		{
			*out_value = NULL;

			_r_obj_initializestringref_ex (&sr, PTR_ADD_OFFSET (buffer, buffer->DataOffset), buffer->DataLength);

			if (buffer->Type == REG_BINARY)
			{
				*out_value = _r_str_fromhex (PTR_ADD_OFFSET (buffer, buffer->DataOffset), buffer->DataLength, TRUE);
			}
			else if (buffer->Type == REG_SZ || buffer->Type == REG_EXPAND_SZ || buffer->Type == REG_MULTI_SZ)
			{
				if (buffer->Type == REG_EXPAND_SZ)
				{
					if (NT_SUCCESS (_r_str_environmentexpandstring (NULL, &sr, &string)))
					{
						*out_value = string;
					}
					else
					{
						*out_value = NULL;
					}
				}
				else
				{
					*out_value = _r_obj_createstring2 (&sr);
				}
			}
			else if (buffer->Type == REG_DWORD)
			{
				*out_value = _r_format_string (L"%" TEXT (PR_ULONG), _r_str_toulong (&sr));
			}
			else if (buffer->Type == REG_QWORD)
			{
				*out_value = _r_format_string (L"%" TEXT (PR_ULONG64), _r_str_toulong64 (&sr));
			}
		}

		if (out_type)
			*out_type = buffer->Type;
	}
	else
	{
		*out_name = NULL;

		if (out_value)
			*out_value = NULL;

		if (out_type)
			*out_type = REG_NONE;
	}

	_r_mem_free (buffer);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_queryinfo (
	_In_ HANDLE hkey,
	_Out_opt_ PULONG out_length,
	_Out_opt_ PLONG64 out_timestamp
)
{
	KEY_FULL_INFORMATION full_info = {0};
	ULONG length = 0;
	NTSTATUS status;

	status = NtQueryKey (hkey, KeyFullInformation, &full_info, sizeof (KEY_FULL_INFORMATION), &length);

	if (NT_SUCCESS (status))
	{
		if (out_length)
			*out_length = full_info.MaxValueNameLength / sizeof (WCHAR);

		if (out_timestamp)
			*out_timestamp = _r_unixtime_from_largeinteger (&full_info.LastWriteTime);
	}
	else
	{
		if (out_length)
			*out_length = 0;

		if (out_timestamp)
			*out_timestamp = 0;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_querybinary (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE bytes;
	ULONG buffer_length;
	ULONG type;
	NTSTATUS status;

	status = _r_reg_queryvalue (hkey, value_name, NULL, &buffer_length, &type);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	if (type != REG_BINARY)
	{
		*out_buffer = NULL;

		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	bytes = _r_obj_createbyte_ex (NULL, buffer_length);

	status = _r_reg_queryvalue (hkey, value_name, bytes->buffer, &buffer_length, NULL);

	if (NT_SUCCESS (status))
	{
		*out_buffer = bytes;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (bytes); // cleanup
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_querystring (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_Out_ PR_STRING_PTR out_string,
	_Out_opt_ PULONG out_type
)
{
	PR_STRING expanded_string;
	PR_STRING string;
	ULONG buffer_length;
	ULONG type;
	NTSTATUS status;

	status = _r_reg_queryvalue (hkey, value_name, NULL, &buffer_length, &type);

	if (!NT_SUCCESS (status))
	{
		*out_string = NULL;

		if (out_type)
			*out_type = type;

		return status;
	}

	if (out_type)
		*out_type = type;

	if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ)
	{
		*out_string = NULL;

		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	string = _r_obj_createstring_ex (NULL, buffer_length);

	status = _r_reg_queryvalue (hkey, value_name, string->buffer, &buffer_length, &type);

	if (status == STATUS_BUFFER_TOO_SMALL)
	{
		_r_obj_movereference (&string, _r_obj_createstring_ex (NULL, buffer_length));

		status = _r_reg_queryvalue (hkey, value_name, string->buffer, &buffer_length, &type);
	}

	if (NT_SUCCESS (status))
	{
		if (type != REG_MULTI_SZ)
			_r_str_trimtonullterminator (&string->sr);

		if (type == REG_EXPAND_SZ)
		{
			status = _r_str_environmentexpandstring (NULL, &string->sr, &expanded_string);

			if (NT_SUCCESS (status))
				_r_obj_movereference (&string, expanded_string);
		}

		*out_string = string;
	}
	else
	{
		*out_string = NULL;

		_r_obj_dereference (string); // cleanup
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_queryulong (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_Out_ PULONG out_buffer
)
{
	ULONG buffer_length;
	ULONG buffer = 0;
	ULONG type;
	NTSTATUS status;

	status = _r_reg_queryvalue (hkey, value_name, &buffer, &buffer_length, &type);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = 0;

		return status;
	}

	if (type != REG_DWORD)
	{
		*out_buffer = 0;

		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	RtlCopyMemory (out_buffer, &buffer, buffer_length);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_queryulong64 (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_Out_ PULONG64 out_buffer
)
{
	ULONG64 buffer = 0;
	ULONG buffer_length;
	ULONG type;
	NTSTATUS status;

	status = _r_reg_queryvalue (hkey, value_name, &buffer, &buffer_length, &type);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = 0;

		return status;
	}

	if (type != REG_QWORD)
	{
		*out_buffer = 0;

		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	RtlCopyMemory (out_buffer, &buffer, buffer_length);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_queryvalue (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_Out_opt_ PVOID out_buffer,
	_Out_opt_ PULONG out_buffer_length,
	_Out_opt_ PULONG out_type
)
{
	PKEY_VALUE_PARTIAL_INFORMATION value_info;
	UNICODE_STRING us;
	ULONG length;
	NTSTATUS status;

	_r_obj_initializeunicodestring (&us, value_name);

	status = NtQueryValueKey (hkey, &us, KeyValuePartialInformation, NULL, 0, &length);

	if (status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL)
	{
		if (out_buffer)
			RtlZeroMemory (out_buffer, 2);

		if (out_buffer_length)
			*out_buffer_length = 0;

		if (out_type)
			*out_type = REG_NONE;

		return status;
	}

	value_info = _r_mem_allocate (length);

	status = NtQueryValueKey (hkey, &us, KeyValuePartialInformation, value_info, length, &length);

	if (NT_SUCCESS (status))
	{
		if (out_buffer)
			RtlCopyMemory (out_buffer, value_info->Data, value_info->DataLength);

		if (out_buffer_length)
			*out_buffer_length = value_info->DataLength;

		if (out_type)
			*out_type = value_info->Type;
	}
	else
	{
		if (out_buffer)
			RtlZeroMemory (out_buffer, 2);

		if (out_buffer_length)
			*out_buffer_length = 0;

		if (out_type)
			*out_type = REG_NONE;
	}

	_r_mem_free (value_info);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_reg_setvalue (
	_In_ HANDLE hkey,
	_In_opt_ LPWSTR value_name,
	_In_ ULONG type,
	_In_reads_bytes_opt_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length
)
{
	UNICODE_STRING us;
	NTSTATUS status;

	_r_obj_initializeunicodestring (&us, value_name);

	status = NtSetValueKey (hkey, &us, 0, type, buffer, buffer_length);

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
	RtlZeroMemory (crypt_context, sizeof (R_CRYPT_CONTEXT));

	crypt_context->is_hashing = is_hashing;
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
	ULONG data_length = 0;
	ULONG query_length;
	NTSTATUS status;

	_r_crypt_initialize (crypt_context, FALSE);

	status = BCryptOpenAlgorithmProvider (&crypt_context->alg_handle, algorithm_id, NULL, 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = BCryptSetProperty (crypt_context->alg_handle, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof (BCRYPT_CHAIN_MODE_CBC), 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// Calculate the size of the buffer to hold the key object
	status = BCryptGetProperty (crypt_context->alg_handle, BCRYPT_OBJECT_LENGTH, (PUCHAR)&data_length, sizeof (ULONG), &query_length, 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	crypt_context->object_data = _r_obj_createbyte_ex (NULL, data_length);

	// Calculate the block length for the IV
	status = BCryptGetProperty (crypt_context->alg_handle, BCRYPT_BLOCK_LENGTH, (PUCHAR)&data_length, sizeof (ULONG), &query_length, 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	crypt_context->block_data = _r_obj_createbyte_ex (NULL, data_length);

CleanupExit:

	if (!NT_SUCCESS (status))
		_r_crypt_destroycryptcontext (crypt_context);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_createhashcontext (
	_Out_ PR_CRYPT_CONTEXT hash_context,
	_In_ LPCWSTR algorithm_id
)
{
	ULONG data_length = 0;
	ULONG query_length;
	NTSTATUS status;

	_r_crypt_initialize (hash_context, TRUE);

	status = BCryptOpenAlgorithmProvider (&hash_context->alg_handle, algorithm_id, NULL, 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = BCryptGetProperty (hash_context->alg_handle, BCRYPT_OBJECT_LENGTH, (PBYTE)&data_length, sizeof (ULONG), &query_length, 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	hash_context->object_data = _r_obj_createbyte_ex (NULL, data_length);

	status = BCryptGetProperty (hash_context->alg_handle, BCRYPT_HASH_LENGTH, (PBYTE)&data_length, sizeof (ULONG), &query_length, 0);

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

	RtlZeroMemory (crypt_context->object_data->buffer, crypt_context->object_data->length);

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
		_r_obj_dereference (tmp_buffer);

		*out_buffer = NULL;
	}

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
		_r_obj_dereference (tmp_buffer);

		*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_finalhashcontext (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_Out_opt_ PR_STRING_PTR out_string,
	_Out_opt_ PR_BYTE_PTR out_buffer
)
{
	PR_STRING string;
	NTSTATUS status;

	status = BCryptFinishHash (hash_context->u.hash_handle, hash_context->block_data->buffer, (ULONG)hash_context->block_data->length, 0);

	if (NT_SUCCESS (status))
	{
		if (out_string)
		{
			string = _r_str_fromhex (hash_context->block_data->buffer, hash_context->block_data->length, TRUE);

			*out_string = string;
		}

		if (out_buffer)
			*out_buffer = _r_obj_reference (hash_context->block_data);
	}
	else
	{
		if (out_string)
			*out_string = NULL;

		if (out_buffer)
			*out_buffer = NULL;
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_crypt_getfilehash (
	_In_ LPCWSTR algorithm_id,
	_In_opt_ PR_STRINGREF path,
	_In_opt_ HANDLE hfile,
	_Out_ PR_STRING_PTR out_buffer
)
{
	R_CRYPT_CONTEXT hash_context;
	HANDLE hfile_new = NULL;
	PVOID buffer;
	PR_STRING string;
	ULONG_PTR readed;
	ULONG buffer_length;
	NTSTATUS status;

	if (!path && !hfile)
		return STATUS_INVALID_PARAMETER;

	if (path && !hfile)
	{
		status = _r_fs_openfile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, FALSE, &hfile_new);

		if (!NT_SUCCESS (status))
		{
			*out_buffer = NULL;

			return status;
		}

		hfile = hfile_new;
	}

	status = _r_crypt_createhashcontext (&hash_context, algorithm_id);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	buffer_length = PR_SIZE_BUFFER;
	buffer = _r_mem_allocate (buffer_length);

	while (TRUE)
	{
		status = _r_fs_readfile (hfile, buffer, buffer_length, &readed);

		if (!NT_SUCCESS (status) || readed == 0)
			break;

		status = _r_crypt_hashbuffer (&hash_context, buffer, (ULONG)readed);

		if (!NT_SUCCESS (status))
			break;
	}

	status = _r_crypt_finalhashcontext (&hash_context, &string, NULL);

	if (NT_SUCCESS (status))
	{
		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;
	}

	_r_crypt_destroycryptcontext (&hash_context);

	if (hfile_new)
		NtClose (hfile_new);

	_r_mem_free (buffer);

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

ULONG64 _r_math_exponentiate64 (
	_In_ ULONG64 base,
	_In_ ULONG exponent
)
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

VOID _r_math_generateguid (
	_Out_ LPGUID guid
)
{
	// The top/sign bit is always unusable for RtlRandomEx (the result is always unsigned), so we'll
	// take the bottom 24 bits. We need 128 bits in total, so we'll call the function 6 times.
	ULONG random[6] = {0};

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (random); i++)
		random[i] = _r_math_getrandom ();

	// random[0] is usable
	*(PUSHORT)&guid->Data1 = (USHORT)random[0];

	// top byte from random[0] is usable
	*((PUSHORT)&guid->Data1 + 1) = (USHORT)((random[0] >> 16) | (random[1] & 0xFF));

	// top 2 bytes from random[1] are usable
	guid->Data2 = (SHORT)(random[1] >> 8);

	// random[2] is usable
	guid->Data3 = (SHORT)random[2];

	// top byte from random[2] is usable
	*(PUSHORT)&guid->Data4[0] = (USHORT)((random[2] >> 16) | (random[3] & 0xFF));

	// top 2 bytes from random[3] are usable
	*(PUSHORT)&guid->Data4[2] = (USHORT)(random[3] >> 8);

	// random[4] is usable
	*(PUSHORT)&guid->Data4[4] = (USHORT)random[4];

	// top byte from random[4] is usable
	*(PUSHORT)&guid->Data4[6] = (USHORT)((random[4] >> 16) | (random[5] & 0xFF));

	((PGUID_EX)guid)->s2.Version = GUID_VERSION_RANDOM;
	((PGUID_EX)guid)->s2.Variant &= ~GUID_VARIANT_STANDARD_MASK;
	((PGUID_EX)guid)->s2.Variant |= GUID_VARIANT_STANDARD;
}

ULONG _r_math_getrandom ()
{
	static LARGE_INTEGER seed = {0}; // save seed

	ULONG value;

	RtlQueryPerformanceCounter (&seed);

	value = RtlRandomEx (&seed.LowPart);

	return value;
}

ULONG _r_math_getrandomrange (
	_In_ ULONG min_number,
	_In_ ULONG max_number
)
{
	return min_number + (_r_math_getrandom () % (max_number - min_number + 1));
}

ULONG _r_math_hashinteger32 (
	_In_ LONG value
)
{
	// Java style.
	value ^= (value >> 20) ^ (value >> 12);

	return value ^ (value >> 7) ^ (value >> 4);
}

ULONG _r_math_hashinteger64 (
	_In_ LONG64 value
)
{
	// http://www.concentric.net/~Ttwang/tech/inthash.htm
	// https://gist.github.com/badboy/6267743?permalink_comment_id=1938983#64-bit-to-32-bit-hash-functions
	value = ~value + (value << 18);
	value = value ^ (value >> 31);
	value = value * 21;
	value = value ^ (value >> 11);
	value = value + (value << 6);
	value = value ^ (value >> 22);

	return (ULONG)value;
}

ULONG_PTR _r_math_rounduptopoweroftwo (
	_In_ ULONG_PTR number
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

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_res_loadresource (
	_In_ PVOID hinst,
	_In_ LPCWSTR type,
	_In_ LPCWSTR name,
	_In_opt_ ULONG lang_id,
	_Out_ PR_STORAGE out_buffer
)
{
	PIMAGE_RESOURCE_DATA_ENTRY resource_data;
	LDR_RESOURCE_INFO resource_info = {0};
	ULONG length;
	PVOID buffer;
	NTSTATUS status;

	resource_info.Type = (ULONG_PTR)type;
	resource_info.Name = (ULONG_PTR)name;
	resource_info.Language = lang_id ? lang_id : MAKELANGID (LANG_NEUTRAL, SUBLANG_NEUTRAL);

	status = LdrFindResource_U (hinst, &resource_info, RESOURCE_DATA_LEVEL, &resource_data);

	if (!NT_SUCCESS (status))
	{
		_r_obj_initializestorage (out_buffer, NULL, 0);

		return status;
	}

	status = LdrAccessResource (hinst, resource_data, &buffer, &length);

	if (NT_SUCCESS (status))
	{
		_r_obj_initializestorage (out_buffer, buffer, length);
	}
	else
	{
		_r_obj_initializestorage (out_buffer, NULL, 0);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_res_loadimage (
	_In_ PVOID hinst,
	_In_ LPCWSTR type,
	_In_ LPCWSTR name,
	_In_ LPCGUID format,
	_In_ LONG width,
	_In_ LONG height,
	_Out_ HBITMAP_PTR out_buffer
)
{
	IWICFormatConverter* wicFormatConverter = NULL;
	IWICBitmapFrameDecode* wicBitmapFrame = NULL;
	IWICBitmapDecoder* wicBitmapDecoder = NULL;
	IWICBitmapSource* wicBitmapSource = NULL;
	IWICImagingFactory2* wicFactory = NULL;
	IWICBitmapScaler* wicScaler = NULL;
	IWICStream* wicBitmapStream = NULL;
	WICPixelFormatGUID pixelFormat;
	PVOID bitmap_buffer = NULL;
	HBITMAP hbitmap = NULL;
	R_STORAGE buffer;
	UINT height_src = 0;
	UINT width_src = 0;
	NTSTATUS status;

	status = _r_res_loadresource (hinst, type, name, 0, &buffer);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	// create the imagingfactory (win8+)
	status = CoCreateInstance (
		_r_sys_isosversiongreaterorequal (WINDOWS_8) ? &CLSID_WICImagingFactory2 : &CLSID_WICImagingFactory1,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IWICImagingFactory,
		&wicFactory
	);

	if (FAILED (status))
		goto CleanupExit;

	// create the stream
	status = IWICImagingFactory_CreateStream (wicFactory, &wicBitmapStream);

	if (FAILED (status))
		goto CleanupExit;

	// initialize the stream from memory
	status = IWICStream_InitializeFromMemory (wicBitmapStream, buffer.buffer, buffer.length);

	if (FAILED (status))
		goto CleanupExit;

	status = IWICImagingFactory_CreateDecoder (wicFactory, format, &GUID_VendorMicrosoft, &wicBitmapDecoder);

	if (FAILED (status))
		goto CleanupExit;

	status = IWICBitmapDecoder_Initialize (wicBitmapDecoder, (IStream*)wicBitmapStream, WICDecodeMetadataCacheOnDemand);

	if (FAILED (status))
		goto CleanupExit;

	// get the frame
	status = IWICBitmapDecoder_GetFrame (wicBitmapDecoder, 0, &wicBitmapFrame);

	if (FAILED (status))
		goto CleanupExit;

	// get the frame image format
	status = IWICBitmapFrameDecode_GetPixelFormat (wicBitmapFrame, &pixelFormat);

	if (FAILED (status))
		goto CleanupExit;

	// check if the image format is supported
	if (IsEqualGUID (&pixelFormat, &GUID_WICPixelFormat32bppBGRA)) // CreateDIBSection format
	{
		status = IWICBitmapFrameDecode_QueryInterface (wicBitmapFrame, &IID_IWICBitmapSource, &wicBitmapSource);

		if (FAILED (status))
			goto CleanupExit;
	}
	else
	{
		status = IWICImagingFactory_CreateFormatConverter (wicFactory, &wicFormatConverter);

		if (FAILED (status))
			goto CleanupExit;

		status = IWICFormatConverter_Initialize (
			wicFormatConverter,
			(IWICBitmapSource*)wicBitmapFrame,
			&GUID_WICPixelFormat32bppBGRA,
			WICBitmapDitherTypeNone,
			NULL,
			0.0F,
			WICBitmapPaletteTypeCustom
		);

		if (FAILED (status))
			goto CleanupExit;

		// convert the image to the correct format
		status = IWICFormatConverter_QueryInterface (wicFormatConverter, &IID_IWICBitmapSource, &wicBitmapSource);

		if (FAILED (status))
			goto CleanupExit;
	}

	hbitmap = _r_dc_createbitmap (NULL, width, height, &bitmap_buffer);

	if (!hbitmap)
	{
		status = E_FAIL;

		goto CleanupExit;
	}

	status = IWICBitmapSource_GetSize (wicBitmapSource, &width_src, &height_src);

	if (SUCCEEDED (status) && width_src == width && height_src == height)
	{
		status = IWICBitmapSource_CopyPixels (wicBitmapSource, NULL, width * sizeof (RGBQUAD), width * height * sizeof (RGBQUAD), bitmap_buffer);
	}
	else
	{
		status = IWICImagingFactory_CreateBitmapScaler (wicFactory, &wicScaler);

		if (SUCCEEDED (status))
		{
			status = IWICBitmapScaler_Initialize (
				wicScaler,
				wicBitmapSource,
				width,
				height,
				_r_sys_isosversiongreaterorequal (WINDOWS_10) ? WICBitmapInterpolationModeHighQualityCubic : WICBitmapInterpolationModeFant
			);

			if (SUCCEEDED (status))
				status = IWICBitmapScaler_CopyPixels (wicScaler, NULL, width * sizeof (RGBQUAD), width * height * sizeof (RGBQUAD), bitmap_buffer);
		}
	}

CleanupExit:

	if (wicFormatConverter)
		IWICFormatConverter_Release (wicFormatConverter);

	if (wicBitmapSource)
		IWICBitmapSource_Release (wicBitmapSource);

	if (wicScaler)
		IWICBitmapScaler_Release (wicScaler);

	if (wicBitmapDecoder)
		IWICBitmapDecoder_Release (wicBitmapDecoder);

	if (wicBitmapFrame)
		IWICBitmapFrameDecode_Release (wicBitmapFrame);

	if (wicBitmapStream)
		IWICStream_Release (wicBitmapStream);

	if (wicFactory)
		IWICImagingFactory_Release (wicFactory);

	if (SUCCEEDED (status))
	{
		*out_buffer = hbitmap;
	}
	else
	{
		*out_buffer = NULL;

		if (hbitmap)
			DeleteObject (hbitmap);
	}

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_res_loadstring (
	_In_ PVOID hinst,
	_In_ ULONG string_id,
	_Out_ PR_STRING_PTR out_buffer
)
{
	PIMAGE_RESOURCE_DIR_STRING_U string_buffer;
	PR_STRING string;
	R_STORAGE buffer;
	ULONG string_num;
	NTSTATUS status;

	status = _r_res_loadresource (hinst, RT_STRING, MAKEINTRESOURCE ((LOWORD (string_id) >> 4) + 1), 0, &buffer);

	if (!NT_SUCCESS (status))
	{
		*out_buffer = NULL;

		return status;
	}

	string_buffer = buffer.buffer;
	string_num = string_id & 0x000F;

	for (ULONG i = 0; i < string_num; i++)
	{
		string_buffer = PTR_ADD_OFFSET (string_buffer, (string_buffer->Length + sizeof (ANSI_NULL)) * sizeof (WCHAR));
	}

	if (string_buffer->Length > 0 && string_buffer->Length < UNICODE_STRING_MAX_BYTES)
	{
		string = _r_obj_createstring_ex (string_buffer->NameString, string_buffer->Length * sizeof (WCHAR));

		*out_buffer = string;
	}
	else
	{
		*out_buffer = NULL;

		status = STATUS_BUFFER_OVERFLOW;
	}

	return status;
}

_Ret_maybenull_
PR_STRING _r_res_loadindirectstring (
	_In_ PR_STRINGREF path
)
{
	R_STRINGREF path_part;
	R_STRINGREF idx_part;
	R_STRINGREF sr;
	PR_STRING indirect_string = NULL;
	PR_STRING path_string = NULL;
	PVOID hlibrary;
	LONG index;
	NTSTATUS status;

	if (path->buffer[0] == L'@')
	{
		_r_obj_initializestringref2 (&sr, path);

		_r_str_skiplength (&sr, sizeof (WCHAR)); // skip the @ character

		if (!_r_str_splitatlastchar (&sr, L',', &path_part, &idx_part))
			return NULL;

		path_string = _r_path_find (&path_part);

		if (!path_string)
			return NULL;

		status = _r_sys_loadlibraryasresource (&path_string->sr, &hlibrary);

		if (NT_SUCCESS (status))
		{
			index = _r_str_tolong (&idx_part);

			status = _r_res_loadstring (hlibrary, -index, &indirect_string);

			_r_sys_freelibrary (hlibrary);
		}

		_r_obj_dereference (path_string);
	}
	else
	{
		indirect_string = _r_obj_createstring2 (path);
	}

	return indirect_string;
}

_Ret_maybenull_
PR_STRING _r_res_querystring (
	_In_ LPCVOID ver_block,
	_In_ LPCWSTR entry_name,
	_In_ LCID lcid
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

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (lcid_arr); i++)
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
	_In_ LCID lcid
)
{
	WCHAR entry[128];
	PVOID buffer;
	PR_STRING string;
	ULONG length;

	_r_str_printf (entry, RTL_NUMBER_OF (entry), L"\\StringFileInfo\\%08X\\%s", lcid, entry_name);

	if (!_r_res_verqueryvalue (ver_block, entry, &buffer, &length))
		return NULL;

	if (length <= sizeof (UNICODE_NULL))
		return NULL;

	length -= 1;
	length *= sizeof (WCHAR);

	string = _r_obj_createstring_ex (buffer, length);

	return string;

}

LCID _r_res_querytranslation (
	_In_ LPCVOID ver_block
)
{
	PR_VERSION_TRANSLATION buffer = NULL;
	ULONG length;

	if (_r_res_verqueryvalue (ver_block, L"\\VarFileInfo\\Translation", &buffer, &length))
		return PR_LANG_TO_LCID (buffer->lang_id, buffer->code_page);

	return PR_LANG_TO_LCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US), 1252);
}

_Success_ (return)
BOOL _r_res_verqueryvalue (
	_In_ LPCVOID block,
	_In_ LPCWSTR sub_block,
	_Out_ PVOID_PTR out_buffer,
	_Out_ PULONG out_length
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static VQV _VerQueryValue = NULL;

	PVOID hversion;
	BOOL is_success;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (L"version.dll", 0, &hversion);

		if (NT_SUCCESS (status))
		{
			_VerQueryValue = (VQV)_r_sys_getprocaddress (hversion, "VerQueryValueW", 0);

			//_r_sys_freelibrary (hversion);
		}

		_r_initonce_end (&init_once);
	}

	*out_buffer = NULL;
	*out_length = 0;

	if (!_VerQueryValue)
		return FALSE;

	is_success = _VerQueryValue (block, sub_block, out_buffer, out_length);

	return is_success && *out_buffer;
}

_Success_ (return)
BOOLEAN _r_res_queryversion (
	_In_ LPCVOID ver_block,
	_Out_ PVOID_PTR out_buffer
)
{
	ULONG length;

	return !!_r_res_verqueryvalue (ver_block, L"\\", out_buffer, &length);
}

_Ret_maybenull_
PR_STRING _r_res_queryversionstring (
	_In_ LPCWSTR path
)
{
	VS_FIXEDFILEINFO* file_info = NULL;
	PR_STRING string;
	PVOID ver_block;
	ULONG ver_size;
	ULONG handle;
	BOOL ver_info;

	ver_size = GetFileVersionInfoSizeExW (FILE_VER_GET_LOCALISED | FILE_VER_GET_NEUTRAL, path, &handle);

	if (!ver_size)
		return NULL;

	ver_block = _r_mem_allocate (ver_size);

	ver_info = GetFileVersionInfoExW (FILE_VER_GET_LOCALISED, path, 0, ver_size, ver_block);

	if (!ver_info)
		goto CleanupExit;

	ver_info = _r_res_queryversion (ver_block, &file_info);

	if (!ver_info)
		goto CleanupExit;

	if (file_info->dwSignature == VS_FFI_SIGNATURE)
	{
		if (HIWORD (file_info->dwFileVersionLS) || LOWORD (file_info->dwFileVersionLS))
		{
			string = _r_format_string (
				L"%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG),
				HIWORD (file_info->dwFileVersionMS),
				LOWORD (file_info->dwFileVersionMS),
				HIWORD (file_info->dwFileVersionLS),
				LOWORD (file_info->dwFileVersionLS)
			);
		}
		else
		{
			string = _r_format_string (L"%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG), HIWORD (file_info->dwFileVersionMS), LOWORD (file_info->dwFileVersionMS));
		}

		_r_mem_free (ver_block);

		return string;
	}

CleanupExit:

	_r_mem_free (ver_block);

	return NULL;
}

//
// Imagelist
//

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_create (
	_In_ LONG width,
	_In_ LONG height,
	_In_ ULONG flags,
	_In_ LONG initial_count,
	_In_ LONG grow_count,
	_Out_ HIMAGELIST_PTR out_buffer
)
{
	IImageList2* imagelist = NULL;
	COLORREF clr_prev;
	HRESULT status;

	status = ImageList_CoCreateInstance (&CLSID_ImageList, NULL, &IID_IImageList2, &imagelist);

	if (FAILED (status))
	{
		*out_buffer = NULL;

		return status;
	}

	status = IImageList2_Initialize (imagelist, width, height, flags, initial_count, grow_count);

	if (SUCCEEDED (status))
	{
		IImageList2_SetBkColor (imagelist, CLR_NONE, &clr_prev);

		*out_buffer = IImageListToHIMAGELIST (imagelist);
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

VOID _r_imagelist_destroy (
	_In_ HIMAGELIST himg
)
{
	IImageList2_Release ((IImageList2*)himg);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_add (
	_In_ HIMAGELIST himg,
	_In_ HBITMAP hbitmap,
	_In_opt_ HBITMAP hmask,
	_Out_opt_ PLONG out_index
)
{
	LONG index = INT_ERROR;
	HRESULT status;

	status = IImageList2_Add ((IImageList2*)himg, hbitmap, hmask, &index);

	if (SUCCEEDED (status))
	{
		if (out_index)
			*out_index = index;
	}
	else
	{
		if (out_index)
			*out_index = INT_ERROR;
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_addicon (
	_In_ HIMAGELIST himg,
	_In_ HICON hicon,
	_Out_opt_ PLONG out_index
)
{
	LONG index = INT_ERROR;
	HRESULT status;

	status = IImageList2_ReplaceIcon ((IImageList2*)himg, INT_ERROR, hicon, &index);

	if (SUCCEEDED (status))
	{
		if (out_index)
			*out_index = index;
	}
	else
	{
		if (out_index)
			*out_index = INT_ERROR;
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_draw (
	_In_ HIMAGELIST himg,
	_In_ LONG index,
	_In_ HDC hdc,
	_In_ LONG x,
	_In_ LONG y,
	_In_ LONG width,
	_In_ LONG height,
	_In_opt_ COLORREF bg_clr,
	_In_opt_ COLORREF fore_clr,
	_In_ ULONG style,
	_In_ BOOLEAN is_enabled
)
{
	IMAGELISTDRAWPARAMS ildp = {0};
	HRESULT status;

	ildp.cbSize = sizeof (IMAGELISTDRAWPARAMS);
	ildp.himl = himg;
	ildp.hdcDst = hdc;
	ildp.i = index;
	ildp.x = x;
	ildp.y = y;
	ildp.cx = width;
	ildp.cy = height;
	ildp.rgbBk = bg_clr ? bg_clr : CLR_DEFAULT;
	ildp.rgbFg = fore_clr ? fore_clr : CLR_DEFAULT;
	ildp.fStyle = style;
	ildp.fState = is_enabled ? ILS_NORMAL : ILS_SATURATE;

	status = IImageList2_Draw ((IImageList2*)himg, &ildp);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_getsize (
	_In_ HIMAGELIST himg,
	_Out_ PLONG out_width,
	_Out_ PLONG out_height
)
{
	*out_width = 0;
	*out_height = 0;

	return IImageList2_GetIconSize ((IImageList2*)himg, out_width, out_height);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_getsystem (
	_In_ LONG icons_size,
	_Out_ HIMAGELIST_PTR out_buffer
)
{
	HIMAGELIST himg = NULL;
	HRESULT status;

	status = SHGetImageList (icons_size, &IID_IImageList2, &himg);

	if (SUCCEEDED (status))
	{
		*out_buffer = himg;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_imagelist_setsize (
	_In_ HIMAGELIST himg,
	_In_ LONG size
)
{
	return IImageList2_SetIconSize ((IImageList2*)himg, size, size);
}

//
// Other
//

_Success_ (return)
BOOLEAN _r_parseini (
	_In_ PR_STRING path,
	_Out_ PR_HASHTABLE_PTR out_buffer,
	_Inout_opt_ PR_LIST section_list
)
{
	R_STRINGREF delimeter = PR_STRINGREF_INIT (L"\\");
	R_STRINGREF sections_iterator;
	R_STRINGREF values_iterator;
	R_STRINGREF key_string;
	R_STRINGREF value_string;
	PR_HASHTABLE hashtable;
	PR_STRING sections_string;
	PR_STRING values_string;
	PR_STRING hash_string;
	PR_STRING value;
	ULONG_PTR hash_code;
	ULONG allocated_length;
	ULONG return_length;

	hashtable = _r_obj_createhashtablepointer (16);

	*out_buffer = hashtable;

	// read section names
	allocated_length = 0x0800; // maximum length for GetPrivateProfileSectionNames
	sections_string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

	return_length = GetPrivateProfileSectionNamesW (sections_string->buffer, allocated_length, path->buffer);

	if (!return_length)
	{
		_r_obj_dereference (sections_string);

		return FALSE;
	}

	_r_str_setlength (&sections_string->sr, return_length * sizeof (WCHAR));

	allocated_length = 0x7FFF; // maximum length for GetPrivateProfileSection
	values_string = _r_obj_createstring_ex (NULL, allocated_length * sizeof (WCHAR));

	// initialize section iterator
	_r_obj_initializestringref (&sections_iterator, sections_string->buffer);

	while (!_r_str_isempty (sections_iterator.buffer))
	{
		return_length = GetPrivateProfileSectionW (sections_iterator.buffer, values_string->buffer, allocated_length, path->buffer);

		if (return_length)
		{
			if (section_list)
				_r_obj_addlistitem (section_list, _r_obj_createstring2 (&sections_iterator));

			// initialize values iterator
			_r_str_setlength_ex (&values_string->sr, return_length * sizeof (WCHAR), allocated_length * sizeof (WCHAR));

			_r_obj_initializestringref (&values_iterator, values_string->buffer);

			while (!_r_str_isempty (values_iterator.buffer))
			{
				// skip comments
				if (*values_iterator.buffer != L'#')
				{
					if (_r_str_splitatchar (&values_iterator, L'=', &key_string, &value_string))
					{
						// set hash code in table to "section\key" string
						hash_string = _r_obj_concatstringrefs (
							3,
							&sections_iterator,
							&delimeter,
							&key_string
						);

						hash_code = _r_str_gethash2 (&hash_string->sr, TRUE);

						if (hash_code)
						{
							if (!_r_obj_findhashtable (hashtable, hash_code))
							{
								if (value_string.length)
								{
									value = _r_obj_createstring2 (&value_string);
								}
								else
								{
									value = NULL;
								}

								_r_obj_addhashtablepointer (hashtable, hash_code, value);
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

	return TRUE;
}

//
// Xml library
//

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_initializelibrary (
	_Out_ PR_XML_LIBRARY xml_library,
	_In_ BOOLEAN is_reader
)
{
	PVOID pobject;
	HRESULT status;

	RtlSecureZeroMemory (xml_library, sizeof (R_XML_LIBRARY));

	xml_library->is_reader = is_reader;

	if (is_reader)
	{
		status = CreateXmlReader (&IID_IXmlReader, &pobject, NULL);

		if (FAILED (status))
			return status;

		xml_library->reader = pobject;

		IXmlReader_SetProperty (xml_library->reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
	}
	else
	{
		status = CreateXmlWriter (&IID_IXmlWriter, &pobject, NULL);

		if (FAILED (status))
			return status;

		xml_library->writer = pobject;

		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_Indent, TRUE);
		IXmlWriter_SetProperty (xml_library->writer, XmlWriterProperty_CompactEmptyElement, TRUE);
	}

	return status;
}

VOID _r_xml_destroylibrary (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	SAFE_DELETE_STREAM (xml_library->hstream);

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
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_createfilestream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path,
	_In_ ULONG mode,
	_In_ BOOL is_create
)
{
	PR_XML_STREAM hstream;
	HRESULT status;

	status = SHCreateStreamOnFileEx (file_path, mode, FILE_ATTRIBUTE_NORMAL, is_create, NULL, &hstream);

	if (FAILED (status))
		return status;

	status = _r_xml_setlibrarystream (xml_library, hstream);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_createstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_reads_bytes_opt_ (buffer_length) LPCVOID buffer,
	_In_ ULONG buffer_length
)
{
	PR_XML_STREAM hstream;
	HRESULT status;

	hstream = SHCreateMemStream (buffer, buffer_length);

	if (!hstream)
		return E_UNEXPECTED;

	status = _r_xml_setlibrarystream (xml_library, hstream);

	return status;
}

_Success_ (return)
BOOLEAN _r_xml_enumchilditemsbytagname (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPWSTR tag_name
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	LPWSTR buffer;
	UINT buffer_length;
	XmlNodeType node_type;
	HRESULT status;

	_r_obj_initializestringref (&sr2, tag_name);

	while (TRUE)
	{
		if (IXmlReader_IsEOF (xml_library->reader))
			return FALSE;

		status = IXmlReader_Read (xml_library->reader, &node_type);

		if (FAILED (status))
			return FALSE;

		if (node_type == XmlNodeType_Element)
		{
			buffer = NULL;

			status = IXmlReader_GetLocalName (xml_library->reader, &buffer, &buffer_length);

			if (FAILED (status))
				return FALSE;

			if (_r_str_isempty (buffer))
				return FALSE;

			_r_obj_initializestringref_ex (&sr1, buffer, buffer_length * sizeof (WCHAR));

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
	_In_ LPWSTR tag_name
)
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	LPWSTR buffer;
	UINT buffer_length;
	XmlNodeType node_type;
	HRESULT status;

	_r_xml_resetlibrarystream (xml_library);

	_r_obj_initializestringref (&sr1, tag_name);

	while (TRUE)
	{
		if (IXmlReader_IsEOF (xml_library->reader))
			return FALSE;

		status = IXmlReader_Read (xml_library->reader, &node_type);

		if (FAILED (status))
			return FALSE;

		if (node_type == XmlNodeType_Element)
		{
			// do not return empty elements
			if (IXmlReader_IsEmptyElement (xml_library->reader))
				continue;

			buffer = NULL;

			status = IXmlReader_GetLocalName (xml_library->reader, &buffer, &buffer_length);

			if (FAILED (status) || _r_str_isempty (buffer))
				break;

			_r_obj_initializestringref_ex (&sr2, buffer, buffer_length * sizeof (WCHAR));

			if (_r_str_isequal (&sr1, &sr2, TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_getattribute (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_Out_ PR_STRINGREF value
)
{
	LPWSTR value_string = NULL;
	ULONG value_length = 0;
	HRESULT status;

	// initialize as empty!
	_r_obj_initializestringrefempty (value);

	status = IXmlReader_MoveToAttributeByName (xml_library->reader, attrib_name, NULL);

	if (FAILED (status))
		return status;

	status = IXmlReader_GetValue (xml_library->reader, &value_string, &value_length);

	// restore position before return from the function!
	IXmlReader_MoveToElement (xml_library->reader);

	if (FAILED (status))
		return status;

	if (value_length && !_r_str_isempty (value_string))
		_r_obj_initializestringref_ex (value, value_string, value_length * sizeof (WCHAR));

	return status;
}

BOOLEAN _r_xml_getattribute_boolean (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (FAILED (_r_xml_getattribute (xml_library, attrib_name, &text_value)))
		return FALSE;

	return _r_str_toboolean (&text_value);
}

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (FAILED (_r_xml_getattribute (xml_library, attrib_name, &text_value)))
		return NULL;

	if (_r_obj_isstringempty2 (&text_value))
		return NULL;

	return _r_obj_createstring2 (&text_value);
}

LONG _r_xml_getattribute_long (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (FAILED (_r_xml_getattribute (xml_library, attrib_name, &text_value)))
		return 0;

	return _r_str_tolong (&text_value);
}

LONG64 _r_xml_getattribute_long64 (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
)
{
	R_STRINGREF text_value;

	if (FAILED (_r_xml_getattribute (xml_library, attrib_name, &text_value)))
		return 0;

	return _r_str_tolong64 (&text_value);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_parsefile (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path
)
{
	HRESULT status;

	status = _r_xml_createfilestream (xml_library, file_path, STGM_READ, FALSE);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_parsestring (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCVOID buffer,
	_In_ ULONG buffer_length
)
{
	HRESULT status;

	status = _r_xml_createstream (xml_library, buffer, buffer_length);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_readstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	ULARGE_INTEGER size;
	PR_BYTE bytes;
	ULONG readed;
	HRESULT status;

	status = IStream_Size (xml_library->hstream, &size);

	if (FAILED (status))
	{
		*out_buffer = NULL;

		return status;
	}

	// reset stream position to the beginning
	IStream_Reset (xml_library->hstream);

#if defined(_WIN64)
	bytes = _r_obj_createbyte_ex (NULL, size.QuadPart);
#else
	bytes = _r_obj_createbyte_ex (NULL, size.LowPart);
#endif // _WIN64

	status = ISequentialStream_Read (xml_library->hstream, bytes->buffer, (ULONG)bytes->length, &readed);

	if (SUCCEEDED (status))
	{
		_r_obj_setbytelength (bytes, readed);

		*out_buffer = bytes;
	}
	else
	{
		*out_buffer = NULL;

		_r_obj_dereference (bytes);
	}

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_resetlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	HRESULT status;

	if (!xml_library->hstream)
		return E_UNEXPECTED;

	// reset stream position to the beginning
	IStream_Reset (xml_library->hstream);

	status = _r_xml_setlibrarystream (xml_library, xml_library->hstream);

	return status;
}

_Success_ (SUCCEEDED (return))
HRESULT _r_xml_setlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ PR_XML_STREAM hstream
)
{
	HRESULT status;

	xml_library->hstream = hstream;

	if (xml_library->is_reader)
	{
		status = IXmlReader_SetInput (xml_library->reader, (IUnknown*)hstream);
	}
	else
	{
		status = IXmlWriter_SetOutput (xml_library->writer, (IUnknown*)hstream);
	}

	return status;
}

VOID _r_xml_setattribute (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_opt_ LPCWSTR value
)
{
	IXmlWriter_WriteAttributeString (xml_library->writer, NULL, attrib_name, NULL, value);
}

VOID _r_xml_setattribute_boolean (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR name,
	_In_ BOOLEAN value
)
{
	_r_xml_setattribute (xml_library, name, value ? L"true" : L"false");
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

VOID _r_xml_writestartdocument (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteStartDocument (xml_library->writer, XmlStandalone_Omit);
}

VOID _r_xml_writeenddocument (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteEndDocument (xml_library->writer);

	IXmlWriter_Flush (xml_library->writer);
}

VOID _r_xml_writestartelement (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR name
)
{
	IXmlWriter_WriteStartElement (xml_library->writer, NULL, name, NULL);
}

VOID _r_xml_writeendelement (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteEndElement (xml_library->writer);
}

VOID _r_xml_writewhitespace (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR whitespace
)
{
	IXmlWriter_WriteWhitespace (xml_library->writer, whitespace);
}

//
// Taskbar (win7+)
//

_Success_ (SUCCEEDED (return))
HRESULT _r_taskbar_initialize (
	_Out_ PHANDLE out_buffer
)
{
	ITaskbarList3* taskbar_class = NULL;
	HRESULT status;

	status = CoCreateInstance (&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, &taskbar_class);

	if (SUCCEEDED (status))
	{
		status = ITaskbarList3_HrInit (taskbar_class);

		if (FAILED (status))
		{
			ITaskbarList3_Release (taskbar_class);

			taskbar_class = NULL;
		}
	}

	if (SUCCEEDED (status))
	{
		*out_buffer = taskbar_class;
	}
	else
	{
		*out_buffer = NULL;
	}

	return status;
}

VOID _r_taskbar_destroy (
	_In_ HANDLE htaskbar
)
{
	ITaskbarList3_Release ((ITaskbarList3*)htaskbar);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_taskbar_setprogressstate (
	_In_ HANDLE htaskbar,
	_In_ HWND hwnd,
	_In_ TBPFLAG state
)
{
	return ITaskbarList3_SetProgressState ((ITaskbarList3*)htaskbar, hwnd, state);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_taskbar_setprogressvalue (
	_In_ HANDLE htaskbar,
	_In_ HWND hwnd,
	_In_ ULONG64 total_written,
	_In_ ULONG64 total_length
)
{
	return ITaskbarList3_SetProgressValue ((ITaskbarList3*)htaskbar, hwnd, total_written, total_length);
}

_Success_ (SUCCEEDED (return))
HRESULT _r_taskbar_setoverlayicon (
	_In_ HANDLE htaskbar,
	_In_ HWND hwnd,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR description
)
{
	return ITaskbarList3_SetOverlayIcon ((ITaskbarList3*)htaskbar, hwnd, hicon, description);
}

//
// System tray
//

VOID _r_tray_initialize (
	_Out_ PNOTIFYICONDATA nid,
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ UINT msg
)
{
	RtlSecureZeroMemory (nid, sizeof (NOTIFYICONDATA));

	nid->cbSize = sizeof (NOTIFYICONDATA);

	nid->uFlags = NIF_GUID; // win7+
	nid->hWnd = hwnd;

	if (msg)
	{
		nid->uFlags |= NIF_MESSAGE;

		nid->uCallbackMessage = msg;
	}

	// if guidItem is specified, uID is ignored
	//nid->uID = guid->Data2;

	RtlCopyMemory (&nid->guidItem, guid, sizeof (GUID));
}

VOID _r_tray_create (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ UINT msg,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip,
	_In_ BOOLEAN is_hidden
)
{
	NOTIFYICONDATA nid;
	LONG icon_size;

	_r_tray_initialize (&nid, hwnd, guid, msg);

	Shell_NotifyIconW (NIM_DELETE, &nid); // HACK!!!

	nid.uFlags |= NIF_ICON;
	nid.hIcon = hicon;

	if (!nid.hIcon)
	{
		icon_size = _r_dc_getsystemmetrics (SM_CXSMICON, _r_dc_gettaskbardpi ());

		_r_sys_loadicon (NULL, MAKEINTRESOURCE (IDI_APPLICATION), icon_size, &nid.hIcon);
	}

	if (tooltip)
	{
		nid.uFlags |= NIF_TIP | NIF_SHOWTIP; // vista+

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	if (is_hidden)
	{
		nid.uFlags |= NIF_STATE;

		nid.dwState = NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
	}

	Shell_NotifyIconW (NIM_ADD, &nid);

	// vista+
	nid.uVersion = NOTIFYICON_VERSION_4;

	Shell_NotifyIconW (NIM_SETVERSION, &nid);
}

VOID _r_tray_destroy (
	_In_ HWND hwnd,
	_In_ LPCGUID guid
)
{
	NOTIFYICONDATA nid;

	_r_tray_initialize (&nid, hwnd, guid, 0);

	Shell_NotifyIconW (NIM_DELETE, &nid);
}

VOID _r_tray_popup (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG flags,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR string
)
{
	NOTIFYICONDATA nid;

	_r_tray_initialize (&nid, hwnd, guid, 0);

	nid.uFlags |= NIF_REALTIME; // vista+

	if (flags)
	{
		nid.uFlags |= NIF_INFO;

		nid.dwInfoFlags = flags;
	}

	if (title)
		_r_str_copy (nid.szInfoTitle, RTL_NUMBER_OF (nid.szInfoTitle), title);

	if (string)
		_r_str_copy (nid.szInfo, RTL_NUMBER_OF (nid.szInfo), string);

	Shell_NotifyIconW (NIM_MODIFY, &nid);
}

VOID _r_tray_popupformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG flags,
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

	_r_tray_popup (hwnd, guid, flags, title, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_tray_setinfo (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip
)
{
	NOTIFYICONDATA nid;

	_r_tray_initialize (&nid, hwnd, guid, 0);

	if (hicon)
	{
		nid.uFlags |= NIF_ICON;

		nid.hIcon = hicon;
	}

	if (tooltip)
	{
		nid.uFlags |= NIF_SHOWTIP | NIF_TIP;

		_r_str_copy (nid.szTip, RTL_NUMBER_OF (nid.szTip), tooltip);
	}

	Shell_NotifyIconW (NIM_MODIFY, &nid);
}

VOID _r_tray_setinfoformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_tray_setinfo (hwnd, guid, hicon, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_tray_toggle (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ BOOLEAN is_show
)
{
	NOTIFYICONDATA nid;

	_r_tray_initialize (&nid, hwnd, guid, 0);

	nid.uFlags |= NIF_STATE;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	Shell_NotifyIconW (NIM_MODIFY, &nid);
}

//
// Control: common
//

BOOLEAN _r_ctrl_isenabled (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	HWND htarget;

	htarget = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!htarget)
		return FALSE;

	return !!IsWindowEnabled (htarget);
}

INT _r_ctrl_isradiochecked (
	_In_ HWND hwnd,
	_In_ INT start_id,
	_In_ INT end_id
)
{
	for (INT i = start_id; i <= end_id; i++)
	{
		if (_r_ctrl_isbuttonchecked (hwnd, i))
			return i;
	}

	return 0;
}

_Ret_maybenull_
HWND _r_ctrl_createtip (
	_In_opt_ HWND hparent
)
{
	HWND htip;

	htip = CreateWindowExW (
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

	if (!htip)
		return NULL;

	_r_ctrl_settipstyle (htip, FALSE);

	return htip;
}

BOOLEAN _r_ctrl_enable (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ BOOLEAN is_enable
)
{
	HWND htarget;

	htarget = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!htarget)
		return FALSE;

	return !!EnableWindow (htarget, is_enable);
}

_Success_ (return != 0)
LONG _r_ctrl_getinteger (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_Out_opt_ PULONG base_ptr
)
{
	LONG64 value;

	value = _r_ctrl_getinteger64 (hwnd, ctrl_id, base_ptr);

	return (LONG)value;
}

_Success_ (return != 0)
LONG64 _r_ctrl_getinteger64 (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
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
	_In_opt_ INT ctrl_id
)
{
	PR_STRING string;
	ULONG return_length;
	ULONG length;

	length = _r_ctrl_getstringlength (hwnd, ctrl_id);

	if (!length)
		return NULL;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	return_length = (ULONG)_r_wnd_sendmessage (hwnd, ctrl_id, WM_GETTEXT, (WPARAM)length + 1, (LPARAM)string->buffer);

	if (return_length)
	{
		_r_str_trimtonullterminator (&string->sr);

		return string;
	}

	_r_obj_dereference (string);

	return NULL;
}

_Success_ (return != 0)
LONG _r_ctrl_getwidth (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	RECT rect;
	HWND htarget;

	htarget = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!htarget)
		return 0;

	if (GetClientRect (htarget, &rect))
		return rect.right;

	return 0;
}

VOID _r_ctrl_setbuttonmargins (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG dpi_value
)
{
	BUTTON_SPLITINFO bsi = {0};
	RECT rect;
	HWND hctrl;
	LONG padding;

	// set button margin
	padding = _r_dc_getdpi (4, dpi_value);

	SetRect (&rect, padding, 0, padding, 0);

	_r_wnd_sendmessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)&rect);

	// set button split margin
	hctrl = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (hctrl)
	{
		if (_r_wnd_getstyle (hctrl, GWL_STYLE) & BS_SPLITBUTTON)
		{
			bsi.mask = BCSIF_SIZE;

			bsi.size.cx = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value) + (padding / 2);
			//bsi.size.cy = 0;

			_r_wnd_sendmessage (hwnd, ctrl_id, BCM_SETSPLITINFO, 0, (LPARAM)&bsi);//
		}
	}
}

_Success_ (return)
BOOLEAN _r_ctrl_settablestring (
	_In_ HWND hwnd,
	_Inout_opt_ HDWP_PTR hdefer,
	_In_ INT ctrl_id1,
	_In_ PR_STRINGREF text1,
	_In_ INT ctrl_id2,
	_In_ PR_STRINGREF text2
)
{
	RECT rect1;
	RECT rect2;
	HWND hctrl1;
	HWND hctrl2;
	HDC hdc1;
	HDC hdc2;
	LONG ctrl1_width;
	LONG ctrl2_width;
	LONG wnd_spacing;
	LONG wnd_width;
	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	hctrl1 = _r_ctrl_getdlgitem (hwnd, ctrl_id1);
	hctrl2 = _r_ctrl_getdlgitem (hwnd, ctrl_id2);

	if (!hctrl1 || !hctrl2)
		return FALSE;

	wnd_width = _r_ctrl_getwidth (hwnd, 0);

	if (!wnd_width)
		return FALSE;

	if (!GetWindowRect (hctrl1, &rect1))
		return FALSE;

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&rect1, 2);

	wnd_spacing = rect1.left;
	wnd_width -= (wnd_spacing * 2);

	hdc1 = GetDC (hctrl1);
	hdc2 = GetDC (hctrl2);

	_r_dc_fixfont (hdc1, hwnd, ctrl_id1); // fix
	_r_dc_fixfont (hdc2, hwnd, ctrl_id2); // fix

	ctrl1_width = _r_dc_getfontwidth (hdc1, text1, NULL) + wnd_spacing;
	ctrl2_width = _r_dc_getfontwidth (hdc2, text2, NULL) + wnd_spacing;

	ctrl2_width = min (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);
	ctrl1_width = min (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing);

	// set control rectangles
	SetRect (&rect1, rect1.left, rect1.top, ctrl1_width, _r_calc_rectheight (&rect1));
	SetRect (&rect2, wnd_width - ctrl2_width, rect1.top, ctrl2_width + wnd_spacing, rect1.bottom);

	if (hdefer && *hdefer)
	{
		// resize control #1
		*hdefer = DeferWindowPos (
			*hdefer,
			hctrl1,
			NULL,
			rect1.left,
			rect1.top,
			rect1.right,
			rect1.bottom,
			flags
		);

		// resize control #2
		*hdefer = DeferWindowPos (
			*hdefer,
			hctrl2,
			NULL,
			rect2.left,
			rect2.top,
			rect2.right,
			rect2.bottom,
			flags
		);
	}
	else
	{
		// resize control #1
		SetWindowPos (
			hctrl1,
			NULL,
			rect1.left,
			rect1.top,
			rect1.right,
			rect1.bottom,
			flags
		);

		// resize control #2
		SetWindowPos (
			hctrl2,
			NULL,
			rect2.left,
			rect2.top,
			rect2.right,
			rect2.bottom,
			flags
		);
	}

	_r_ctrl_setstringlength (hwnd, ctrl_id1, text1);
	_r_ctrl_setstringlength (hwnd, ctrl_id2, text2);

	ReleaseDC (hctrl1, hdc1);
	ReleaseDC (hctrl2, hdc2);

	return TRUE;
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
	_In_opt_ INT ctrl_id,
	_In_ PR_STRINGREF string
)
{
	PR_STRING tmp_string;

	// Note: PR_STRINGREF can be not null terminated.
	if (!_r_str_isnullterminated (string))
	{
		tmp_string = _r_obj_createstring2 (string);

		_r_ctrl_setstring (hwnd, ctrl_id, tmp_string->buffer);

		_r_obj_dereference (tmp_string);
	}
	else
	{
		_r_ctrl_setstring (hwnd, ctrl_id, string->buffer);
	}
}

VOID _r_ctrl_settiptext (
	_In_ HWND htip,
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LPWSTR string
)
{
	TTTOOLINFOW tool_info = {0};

	tool_info.cbSize = sizeof (tool_info);
	tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS | TTF_PARSELINKS;
	tool_info.hwnd = hwnd;
	tool_info.hinst = _r_sys_getimagebase ();
	tool_info.uId = (UINT_PTR)_r_ctrl_getdlgitem (hwnd, ctrl_id);
	tool_info.lpszText = string;

	GetClientRect (hwnd, &tool_info.rect);

	_r_wnd_sendmessage (htip, 0, TTM_ADDTOOL, 0, (LPARAM)&tool_info);
}

VOID _r_ctrl_settiptextformat (
	_In_ HWND hwnd,
	_In_ HWND hparent,
	_In_opt_ INT ctrl_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	va_list arg_ptr;
	PR_STRING string;

	va_start (arg_ptr, format);
	string = _r_format_string_v (format, arg_ptr);
	va_end (arg_ptr);

	_r_ctrl_settiptext (hwnd, hparent, ctrl_id, string->buffer);

	_r_obj_dereference (string);
}

VOID _r_ctrl_settipstyle (
	_In_ HWND htip,
	_In_ BOOLEAN is_instant
)
{
	// set dark theme
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
	{
		if (_r_theme_isenabled ())
			_r_theme_setdarkmode (htip, TRUE);
	}

	// 0 means instant, default is -1
	_r_wnd_sendmessage (htip, 0, TTM_SETDELAYTIME, TTDT_INITIAL, is_instant ? 0 : -1);

	_r_wnd_sendmessage (htip, 0, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAXSHORT);

	// allow newlines (-1 doesn't work)
	_r_wnd_sendmessage (htip, 0, TTM_SETMAXTIPWIDTH, 0, MAXSHORT);

	_r_wnd_top (htip, TRUE); // HACK!!!
}

VOID _r_ctrl_showballoontip (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT icon_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR string
)
{
	EDITBALLOONTIP ebt = {0};

	ebt.cbStruct = sizeof (ebt);
	ebt.ttiIcon = icon_id;
	ebt.pszTitle = title;
	ebt.pszText = string;

	_r_wnd_sendmessage (hwnd, ctrl_id, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

VOID _r_ctrl_showballoontipformat (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
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
// Control: combobox
//

_Ret_maybenull_
PR_STRING _r_combobox_getitemtext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id
)
{
	PR_STRING string;
	LONG_PTR length;

	length = _r_wnd_sendmessage (hwnd, ctrl_id, CB_GETLBTEXTLEN, (WPARAM)item_id, 0);

	if (length == CB_ERR)
		return NULL;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	if (_r_wnd_sendmessage (hwnd, ctrl_id, CB_GETLBTEXT, (WPARAM)item_id, (LPARAM)string->buffer) == CB_ERR)
	{
		_r_obj_dereference (string);

		return NULL;
	}

	return string;
}

BOOLEAN _r_combobox_setcurrentitembylparam (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LPARAM lparam
)
{
	LPARAM param;
	INT count;

	count = _r_combobox_getcount (hwnd, ctrl_id);

	for (INT i = 0; i < count; i++)
	{
		param = _r_wnd_sendmessage (hwnd, ctrl_id, CB_GETITEMDATA, (WPARAM)i, 0);

		if (param == lparam)
		{
			_r_combobox_setcurrentitem (hwnd, ctrl_id, i);

			return TRUE;
		}
	}

	return FALSE;
}

//
// Control: menu
//

VOID _r_menu_additem_ex (
	_In_ HMENU hmenu,
	_In_opt_ UINT item_id,
	_In_opt_ LPWSTR string,
	_In_opt_ UINT state
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_FTYPE;

	if (item_id && string)
	{
		mii.fMask |= MIIM_ID | MIIM_STRING;

		mii.fType = MF_STRING;
		mii.dwTypeData = string;
		mii.wID = item_id;

		if (state)
		{
			mii.fMask |= MIIM_STATE;

			if (state & MF_DISABLED)
				state |= MF_GRAYED;

			mii.fState = state;
		}
	}
	else
	{
		mii.fMask |= MIIM_STATE;

		mii.fType = MF_SEPARATOR;
		mii.fState = MF_GRAYED | MF_DISABLED;
	}

	InsertMenuItemW (hmenu, MAXUINT, TRUE, &mii);
}

VOID _r_menu_addsubmenu (
	_In_ HMENU hmenu,
	_In_ UINT pos,
	_In_ HMENU hsubmenu,
	_In_opt_ LPWSTR string
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_FTYPE | MIIM_SUBMENU;
	mii.hSubMenu = hsubmenu;

	if (string)
	{
		mii.fMask |= MIIM_STRING;

		mii.dwTypeData = string;
	}

	InsertMenuItemW (hmenu, pos, TRUE, &mii);
}

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

_Ret_maybenull_
PR_STRING _r_menu_getitemtext (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOLEAN is_byposition
)
{
	MENUITEMINFO mii = {0};
	PR_STRING string;
	ULONG length = 128;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = string->buffer;
	mii.cch = length;

	if (GetMenuItemInfoW (hmenu, item_id, is_byposition, &mii))
	{
		_r_str_trimtonullterminator (&string->sr);
	}
	else
	{
		_r_obj_clearreference (&string);
	}

	return string;
}

VOID _r_menu_setitembitmap (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOLEAN is_byposition,
	_In_ HBITMAP hbitmap
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STATE | MIIM_CHECKMARKS;
	mii.hbmpChecked = hbitmap;
	mii.hbmpUnchecked = hbitmap;

	SetMenuItemInfoW (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtext (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOLEAN is_byposition,
	_In_ LPWSTR string
)
{
	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = string;

	SetMenuItemInfoW (hmenu, item_id, is_byposition, &mii);
}

VOID _r_menu_setitemtextformat (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOLEAN is_byposition,
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

	if (is_sendmessage && command_id)
		_r_ctrl_sendcommand (hwnd, command_id, 0);

	return command_id;
}

//
// Control: tab
//

VOID _r_tab_adjustchild (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HWND hchild
)
{
	RECT tab_rect;
	RECT new_rect;
	HWND htab;

	htab = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!htab)
		return;

	if (!GetClientRect (htab, &new_rect) || !GetWindowRect (htab, &tab_rect))
		return;

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&tab_rect, 2);

	OffsetRect (&new_rect, tab_rect.left, tab_rect.top);

	_r_wnd_sendmessage (hwnd, ctrl_id, TCM_ADJUSTRECT, FALSE, (LPARAM)&new_rect);

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
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TCITEMW tci = {0};

	if (string)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = string;
	}

	if (image_id != I_IMAGENONE)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image_id;
	}

	if (lparam != LONG_PTR_ERROR)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, TCM_INSERTITEM, (WPARAM)item_id, (LPARAM)&tci);
}

LPARAM _r_tab_getitemlparam (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id
)
{
	TCITEMW tci = {0};

	tci.mask = TCIF_PARAM;

	if (_r_wnd_sendmessage (hwnd, ctrl_id, TCM_GETITEM, (WPARAM)item_id, (LPARAM)&tci))
		return tci.lParam;

	return 0;
}

INT _r_tab_setitem (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TCITEMW tci = {0};

	if (string)
	{
		tci.mask |= TCIF_TEXT;
		tci.pszText = string;
	}

	if (image_id != I_IMAGENONE)
	{
		tci.mask |= TCIF_IMAGE;
		tci.iImage = image_id;
	}

	if (lparam != LONG_PTR_ERROR)
	{
		tci.mask |= TCIF_PARAM;
		tci.lParam = lparam;
	}

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, TCM_SETITEM, (WPARAM)item_id, (LPARAM)&tci);
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

	_r_wnd_sendmessage (hwnd, 0, WM_NOTIFY, (WPARAM)ctrl_id, (LPARAM)&hdr);

	_r_wnd_sendmessage (hwnd, ctrl_id, TCM_SETCURSEL, (WPARAM)item_id, 0);

	hdr.code = TCN_SELCHANGE;

	_r_wnd_sendmessage (hwnd, 0, WM_NOTIFY, (WPARAM)ctrl_id, (LPARAM)&hdr);
#pragma warning(pop)
}

//
// Control: listview
//

INT _r_listview_addcolumn (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT column_id,
	_In_opt_ LPWSTR title,
	_In_opt_ INT width,
	_In_opt_ INT fmt
)
{
	LVCOLUMNW lvc = {0};
	LONG client_width;

	lvc.mask = LVCF_SUBITEM;
	lvc.iSubItem = column_id;

	if (title)
	{
		lvc.mask |= LVCF_TEXT;

		lvc.pszText = title;
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

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_INSERTCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

INT _r_listview_addgroup (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPWSTR title,
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

		lvg.pszHeader = title;
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

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_INSERTGROUP, (WPARAM)group_id, (LPARAM)&lvg);
}

INT _r_listview_additem_ex (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_ LPARAM lparam
)
{
	LVITEMW lvi = {0};

	lvi.iItem = item_id;
	lvi.iSubItem = 0;

	if (string)
	{
		lvi.mask |= LVIF_TEXT;

		lvi.pszText = string;
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

	if (lparam != LONG_PTR_ERROR)
	{
		lvi.mask |= LVIF_PARAM;

		lvi.lParam = lparam;
	}

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_INSERTITEM, 0, (LPARAM)&lvi);
}

VOID _r_listview_deleteallcolumns (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	INT column_count;

	column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	if (!column_count)
		return;

	for (INT i = column_count; i >= 0; i--)
		_r_wnd_sendmessage (hwnd, ctrl_id, LVM_DELETECOLUMN, (WPARAM)i, 0);
}

VOID _r_listview_fillitems (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_start,
	_In_ INT item_end,
	_In_ INT subitem_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id
)
{
	if (item_start == -1 || item_end == -1)
	{
		item_start = 0;

		item_end = _r_listview_getitemcount (hwnd, ctrl_id);
	}

	for (INT i = item_start; i < item_end; i++)
	{
		if (subitem_id)
			_r_listview_setitem_ex (hwnd, ctrl_id, i, 0, NULL, image_id, I_GROUPIDNONE, LONG_PTR_ERROR);

		_r_listview_setitem_ex (hwnd, ctrl_id, i, subitem_id, string, image_id, I_GROUPIDNONE, LONG_PTR_ERROR);
	}
}

_Success_ (return != INT_ERROR)
INT _r_listview_finditem (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT start_pos,
	_In_ LPARAM lparam
)
{
	LVFINDINFOW lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	return (INT)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_FINDITEM, (WPARAM)start_pos, (LPARAM)&lvfi);
}

INT _r_listview_getcolumncount (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	HWND hhdr;

	hhdr = _r_listview_getheader (hwnd, ctrl_id);

	if (!hhdr)
		return 0;

	return (INT)_r_wnd_sendmessage (hhdr, 0, HDM_GETITEMCOUNT, 0, 0);
}

_Ret_maybenull_
PR_STRING _r_listview_getcolumntext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT column_id
)
{
	LVCOLUMNW lvc = {0};
	PR_STRING string;
	ULONG allocated_count = 256;

	string = _r_obj_createstring_ex (NULL, allocated_count * sizeof (WCHAR));

	lvc.mask = LVCF_TEXT;

	lvc.pszText = string->buffer;
	lvc.cchTextMax = (INT)allocated_count + 1;

	if (!(INT)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc))
	{
		_r_obj_dereference (string);

		return NULL;
	}

	_r_str_trimtonullterminator (&string->sr);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_dereference (string);

	return NULL;
}

INT _r_listview_getcolumnwidth (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT column_id
)
{
	LONG total_width;
	LONG column_width;

	total_width = _r_ctrl_getwidth (hwnd, ctrl_id);

	if (!total_width)
		return 0;

	column_width = (LONG)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETCOLUMNWIDTH, (WPARAM)column_id, 0);

	return (INT)_r_calc_percentof (column_width, total_width);
}

_Ret_maybenull_
PR_STRING _r_listview_getgrouptext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT group_id
)
{
	LVGROUP lvg = {0};
	PR_STRING string;
	ULONG length = 128;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	lvg.cbSize = sizeof (lvg);
	lvg.mask = LVGF_HEADER;
	lvg.pszHeader = string->buffer;
	lvg.cchHeader = length;

	if (_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETGROUPINFO, (WPARAM)group_id, (LPARAM)&lvg) == INT_ERROR)
	{
		_r_obj_dereference (string);

		return NULL;
	}

	_r_str_trimtonullterminator (&string->sr);

	return string;
}

INT _r_listview_getitemcheckedcount (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	INT checks_count = 0;
	INT total_count;

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
	_In_opt_ INT ctrl_id,
	_In_ INT item_id
)
{
	LVITEMW lvi = {0};

	lvi.mask = LVIF_GROUPID;
	lvi.iItem = item_id;

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.iGroupId;
}

LPARAM _r_listview_getitemlparam (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id
)
{
	LVITEMW lvi = {0};

	lvi.mask = LVIF_PARAM;
	lvi.iItem = item_id;

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETITEM, 0, (LPARAM)&lvi);

	return lvi.lParam;
}

_Ret_maybenull_
PR_STRING _r_listview_getitemtext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id
)
{
	LVITEMW lvi = {0};
	PR_STRING string = NULL;
	ULONG allocated_count = 0xFF;
	ULONG count = allocated_count;

	while (count >= allocated_count)
	{
		allocated_count *= 2;

		_r_obj_movereference (&string, _r_obj_createstring_ex (NULL, allocated_count * sizeof (WCHAR)));

		lvi.iSubItem = subitem_id;
		lvi.pszText = string->buffer;
		lvi.cchTextMax = (INT)allocated_count + 1;

		count = (ULONG)_r_wnd_sendmessage (hwnd, ctrl_id, LVM_GETITEMTEXT, (WPARAM)item_id, (LPARAM)&lvi);
	}

	_r_str_trimtonullterminator (&string->sr);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_dereference (string);

	return NULL;
}

VOID _r_listview_savetofile (
	_In_ PR_STRINGREF path,
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	BYTE bom[] = {0xFF, 0xFE};
	R_STRINGBUILDER sb;
	PR_STRING value;
	PR_STRING string;
	HANDLE hfile;
	INT column_count;
	INT item_count;
	NTSTATUS status;

	item_count = _r_listview_getitemcount (hwnd, ctrl_id);

	if (!item_count)
		return;

	column_count = _r_listview_getcolumncount (hwnd, ctrl_id);

	if (!column_count)
		return;

	if (_r_fs_exists (path))
		_r_fs_deleterecycle (path);

	status = _r_fs_createfile (
		path,
		FILE_CREATE,
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_ATTRIBUTE_READONLY,
		0,
		FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, L"Cannot create file!", status, path->buffer, ET_NATIVE);

		return;
	}

	_r_obj_initializestringbuilder (&sb, 512);

	// write utf-16 le byte order mask
	_r_fs_writefile (hfile, bom, sizeof (bom));

	_r_obj_appendstringbuilderformat (
		&sb, L"<html>\r\n\t<head>\r\n\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-16\" />\r\n"
		L"\t\t<title>%s</title>\r\n\t\t<style>"
		L"\r\n\t\t\thtml {background-color: #222; color: #fff; font-family: 'Segoe UI', 'Calibri', 'MS Sans Serif', 'Sans-Serif';}"
		L"\r\n\t\t\tbody {margin: 0; padding: 0; line-height: 12px;}"
		L"\r\n\t\t\t#container {margin: 0 auto; width: 90%%;}"
		L"\r\n\t\t\ttable {width: 100%%; border-collapse: collapse; margin: 1em 0;}"
		L"\r\n\t\t\ttable th {text-align: left; border: 1px solid #444; padding: 5px 10px; background-color: #111;}"
		L"\r\n\t\t\ttable td {border: 1px solid #444; padding: 5px 10px;}"
		L"\r\n\t\t\ttable tr:hover td {background-color: #111;}"
		L"\r\n\t\t</style>"
		L"\r\n\t</head>"
		L"\r\n\t<body>"
		L"\r\n\t\t<section id=\"container\">"
		L"\r\n\t\t\t<table cellspacing=\"0\" cellpadding=\"0\">"
		L"\r\n\t\t\t\t<tr>",
		_r_app_getname ()
	);

	// insert columns
	for (INT i = 0; i < column_count; i++)
	{
		value = _r_listview_getcolumntext (hwnd, ctrl_id, i);

		if (value)
		{
			_r_obj_appendstringbuilderformat (&sb, L"\r\n\t\t\t\t\t<th>%s</th>", value->buffer);

			_r_obj_dereference (value);
		}
	}

	// insert items
	_r_obj_appendstringbuilder (&sb, L"\r\n\t\t\t\t</tr>");

	for (INT i = 0; i < item_count; i++)
	{
		_r_obj_appendstringbuilder (&sb, L"\r\n\t\t\t\t<tr>");

		for (INT j = 0; j < column_count; j++)
		{
			value = _r_listview_getitemtext (hwnd, ctrl_id, i, j);

			if (value)
			{
				_r_obj_appendstringbuilderformat (&sb, L"\r\n\t\t\t\t\t<td>%s</td>", value->buffer);

				_r_obj_dereference (value);
			}
		}

		_r_obj_appendstringbuilder (&sb, L"\r\n\t\t\t\t</tr>");
	}

	_r_obj_appendstringbuilderformat (
		&sb,
		L"\r\n\t\t\t</table>"
		L"\r\n\t\t\t<section>"
		L"\r\n\t\t\t\t<p><a href=\"%s\" target=\"_blank\">%s %s</a><p>"
		L"\r\n\t\t\t\t<p>%s</p>"
		L"\r\n\t\t\t</section>"
		L"\r\n\t\t</section>"
		L"\r\n\t</body>"
		L"\r\n</html>\r\n",
		_r_app_getwebsite_url (),
		_r_app_getname (),
		_r_app_getversion (),
		_r_app_getcopyright ()
	);

	string = _r_obj_finalstringbuilder (&sb);

	_r_fs_writefile (hfile, string->buffer, (ULONG)string->length);

	_r_obj_deletestringbuilder (&sb);

	NtClose (hfile);
}

VOID _r_listview_setcolumn (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT column_id,
	_In_opt_ LPWSTR string,
	_In_opt_ INT width
)
{
	LVCOLUMNW lvc = {0};
	LONG client_width;

	if (!string && !width)
		return;

	if (string)
	{
		lvc.mask |= LVCF_TEXT;

		lvc.pszText = string;
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

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_SETCOLUMN, (WPARAM)column_id, (LPARAM)&lvc);
}

VOID _r_listview_setcolumnsortindex (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT column_id,
	_In_ INT arrow
)
{
	HDITEMW hdi = {0};
	HWND hhdr;

	hhdr = _r_listview_getheader (hwnd, ctrl_id);

	if (!hhdr)
		return;

	hdi.mask = HDI_FORMAT;

	if (!_r_wnd_sendmessage (hhdr, 0, HDM_GETITEM, (WPARAM)column_id, (LPARAM)&hdi))
		return;

	if (arrow == 1)
	{
		hdi.fmt = (hdi.fmt & ~HDF_SORTDOWN) | HDF_SORTUP;
	}
	else if (arrow == -1)
	{
		hdi.fmt = (hdi.fmt & ~HDF_SORTUP) | HDF_SORTDOWN;
	}
	else
	{
		hdi.fmt = hdi.fmt & ~(HDF_SORTDOWN | HDF_SORTUP);
	}

	_r_wnd_sendmessage (hhdr, 0, HDM_SETITEM, (WPARAM)column_id, (LPARAM)&hdi);
}

VOID _r_listview_setitem_ex (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_ LPARAM lparam
)
{
	LVITEMW lvi = {0};

	lvi.iItem = item_id;
	lvi.iSubItem = subitem_id;

	if (string)
	{
		lvi.mask |= LVIF_TEXT;

		lvi.pszText = string;
	}

	if (subitem_id == 0)
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

		if (lparam != LONG_PTR_ERROR)
		{
			lvi.mask |= LVIF_PARAM;

			lvi.lParam = lparam;
		}
	}

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_SETITEM, 0, (LPARAM)&lvi);
}

VOID _r_listview_setitemstate (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id,
	_In_ UINT state,
	_In_opt_ UINT state_mask
)
{
	LVITEMW lvi = {0};

	lvi.state = state;
	lvi.stateMask = state_mask;

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_SETITEMSTATE, (WPARAM)item_id, (LPARAM)&lvi);
}

VOID _r_listview_setitemvisible (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT item_id
)
{
	_r_listview_setitemstate (hwnd, ctrl_id, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

	_r_listview_setitemstate (hwnd, ctrl_id, item_id, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	_r_listview_ensurevisible (hwnd, ctrl_id, item_id);
}

VOID _r_listview_setgroup (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPWSTR title,
	_In_opt_ UINT state,
	_In_opt_ UINT state_mask
)
{
	LVGROUP lvg = {0};

	lvg.cbSize = sizeof (lvg);

	if (title)
	{
		lvg.mask |= LVGF_HEADER;
		lvg.pszHeader = title;
	}

	if (state || state_mask)
	{
		lvg.mask |= LVGF_STATE;
		lvg.state = state;
		lvg.stateMask = state_mask;
	}

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_SETGROUPINFO, (WPARAM)group_id, (LPARAM)&lvg);
}

VOID _r_listview_setstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_ BOOL is_groupview
)
{
	HWND hctrl;

	hctrl = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (hctrl)
		SetWindowTheme (hctrl, L"Explorer", NULL);

	hctrl = _r_listview_gettooltips (hwnd, ctrl_id);

	if (hctrl)
		_r_ctrl_settipstyle (hctrl, FALSE);

	if (ex_style)
		_r_wnd_sendmessage (hwnd, ctrl_id, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)ex_style);

	_r_wnd_sendmessage (hwnd, ctrl_id, LVM_ENABLEGROUPVIEW, (WPARAM)is_groupview, 0);
}

//
// Control: treeview
//

HTREEITEM _r_treeview_additem (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_ INT state,
	_In_opt_ HTREEITEM hparent,
	_In_opt_ LPARAM lparam
)
{
	TVINSERTSTRUCTW tvi = {0};

	tvi.itemex.mask = TVIF_STATE;
	tvi.itemex.state = state;
	tvi.itemex.stateMask = state;

	if (string)
	{
		tvi.itemex.mask |= TVIF_TEXT;

		tvi.itemex.pszText = string;
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

	return (HTREEITEM)_r_wnd_sendmessage (hwnd, ctrl_id, TVM_INSERTITEM, 0, (LPARAM)&tvi);
}

LPARAM _r_treeview_getitemlparam (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM hitem
)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_PARAM;
	tvi.hItem = hitem;

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.lParam;
}

UINT _r_treeview_getitemstate (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM item_id
)
{
	TVITEMW tvi = {0};

	tvi.mask = TVIF_STATE;
	tvi.hItem = item_id;

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	return tvi.state;
}

HTREEITEM _r_treeview_getnextitem (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ HTREEITEM item_id,
	_In_opt_ ULONG retrieve_id
)
{
	TVITEMW tvi = {0};

	tvi.mask = TVIF_HANDLE;
	tvi.hItem = item_id;

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETNEXTITEM, retrieve_id, item_id ? (LPARAM)&tvi : 0);

	return tvi.hItem;
}

_Ret_maybenull_
PR_STRING _r_treeview_getitemtext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM item_id
)
{
	TVITEMW tvi = {0};
	PR_STRING string = NULL;
	ULONG allocated_count = 0xFF;

	tvi.mask = TVIF_HANDLE | TVIF_TEXT;
	tvi.hItem = item_id;

	string = _r_obj_createstring_ex (NULL, allocated_count * sizeof (WCHAR));

	tvi.pszText = string->buffer;
	tvi.cchTextMax = (INT)allocated_count + 1;

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	_r_str_trimtonullterminator (&string->sr);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_dereference (string);

	return NULL;
}

VOID _r_treeview_selectfirstchild (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM item_id
)
{
	TVITEMEX tvi = {0};

	tvi.mask = TVIF_HANDLE;
	tvi.hItem = (HTREEITEM)_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)item_id);

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_GETITEM, 0, (LPARAM)&tvi);

	_r_treeview_selectitem (hwnd, ctrl_id, tvi.hItem);
}

VOID _r_treeview_setitemstate (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM item_id,
	_In_ UINT state,
	_In_opt_ UINT state_mask
)
{
	TVITEMW tvi = {0};

	tvi.mask = TVIF_STATE;
	tvi.hItem = item_id;
	tvi.state = state;
	tvi.stateMask = state_mask;

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_SETITEM, 0, (LPARAM)&tvi);
}

VOID _r_treeview_setitem (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ HTREEITEM hitem,
	_In_opt_ LPWSTR string,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
)
{
	TVITEMEX tvi = {0};

	tvi.hItem = hitem;

	if (string)
	{
		tvi.mask |= TVIF_TEXT;

		tvi.pszText = string;
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

	_r_wnd_sendmessage (hwnd, ctrl_id, TVM_SETITEM, 0, (LPARAM)&tvi);
}

VOID _r_treeview_setstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_opt_ ULONG height,
	_In_opt_ ULONG indent
)
{
	HWND hctrl;

	hctrl = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (hctrl)
		SetWindowTheme (hctrl, L"Explorer", NULL);

	hctrl = _r_treeview_gettooltips (hwnd, ctrl_id);

	if (hctrl)
		_r_ctrl_settipstyle (hctrl, FALSE);

	if (ex_style)
		_r_wnd_sendmessage (hwnd, ctrl_id, TVM_SETEXTENDEDSTYLE, 0, (LPARAM)ex_style);

	if (height)
		_r_wnd_sendmessage (hwnd, ctrl_id, TVM_SETITEMHEIGHT, (WPARAM)height, 0);

	if (indent)
		_r_wnd_sendmessage (hwnd, ctrl_id, TVM_SETINDENT, (WPARAM)indent, 0);
}

//
// Control: statusbar
//

LONG _r_status_getheight (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	RECT rect;
	HWND hctrl;

	hctrl = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (!hctrl)
		return 0;

	if (GetClientRect (hctrl, &rect))
		return rect.bottom;

	return 0;
}

_Ret_maybenull_
PR_STRING _r_status_gettext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG part_id,
	_Out_opt_ PLONG out_style
)
{
	PR_STRING string;
	LRESULT length;

	length = _r_wnd_sendmessage (hwnd, ctrl_id, SB_GETTEXTLENGTH, part_id, 0);

	if (!length)
	{
		if (out_style)
			*out_style = 0;

		return NULL;
	}

	if (out_style)
		*out_style = HIWORD (length);

	string = _r_obj_createstring_ex (NULL, LOWORD (length) * sizeof (WCHAR));

	_r_wnd_sendmessage (hwnd, ctrl_id, SB_GETTEXT, part_id, (LPARAM)string->buffer);

	return string;
}

VOID _r_status_setstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ ULONG height
)
{
	if (height)
		_r_wnd_sendmessage (hwnd, ctrl_id, SB_SETMINHEIGHT, height, 0);

	_r_wnd_sendmessage (hwnd, ctrl_id, WM_SIZE, 0, 0);
}

VOID _r_status_settextformat (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG part_id,
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

VOID _r_rebar_deleteband (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ ULONG band_id
)
{
	ULONG index;

	index = _r_rebar_getindex (hwnd, ctrl_id, band_id);

	if (index != UINT_MAX)
		_r_wnd_sendmessage (hwnd, ctrl_id, RB_DELETEBAND, (WPARAM)index, 0);
}

VOID _r_rebar_insertband (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ ULONG band_id,
	_In_ HWND hchild,
	_In_opt_ ULONG style,
	_In_ ULONG width,
	_In_ ULONG height
)
{
	REBARBANDINFOW rbi = {0};
	REBARINFO ri = {0};

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

	ri.cbSize = sizeof (REBARINFO);

	// Set the rebar info with no imagelist
	_r_wnd_sendmessage (hwnd, ctrl_id, RB_SETBARINFO, 0, (LPARAM)&ri);

	_r_wnd_sendmessage (hwnd, ctrl_id, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbi);
}

BOOLEAN _r_rebar_isbandexists (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ ULONG band_id
)
{
	ULONG index;

	index = _r_rebar_getindex (hwnd, ctrl_id, band_id);

	return (index != UINT_MAX) ? TRUE : FALSE;
}

//
// Control: toolbar
//

VOID _r_toolbar_addbutton (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ UINT command_id,
	_In_ INT style,
	_In_opt_ LPCWSTR string,
	_In_ INT state,
	_In_ INT image_id
)
{
	TBBUTTON tbi = {0};
	INT button_id;

	tbi.idCommand = command_id;
	tbi.fsStyle = (BYTE)style;
	tbi.iString = (INT_PTR)(PVOID)string;
	tbi.fsState = (BYTE)state;
	tbi.iBitmap = image_id;

	button_id = _r_toolbar_getbuttoncount (hwnd, ctrl_id);

	_r_wnd_sendmessage (hwnd, ctrl_id, TB_INSERTBUTTON, (WPARAM)button_id, (LPARAM)&tbi);
}

_Ret_maybenull_
PR_STRING _r_toolbar_gettext (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ UINT command_id
)
{
	PR_STRING string;
	LRESULT length;

	length = _r_wnd_sendmessage (hwnd, ctrl_id, TB_GETBUTTONTEXTW, (WPARAM)command_id, 0);

	if (length == INT_ERROR)
		return NULL;

	string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

	_r_wnd_sendmessage (hwnd, ctrl_id, TB_GETBUTTONTEXTW, (WPARAM)command_id, (LPARAM)string->buffer);

	_r_str_trimtonullterminator (&string->sr);

	return string;
}

LONG _r_toolbar_getwidth (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
)
{
	RECT rect = {0};
	LONG total_width = 0;

	for (INT i = 0; i < _r_toolbar_getbuttoncount (hwnd, ctrl_id); i++)
	{
		if (_r_wnd_sendmessage (hwnd, ctrl_id, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&rect) != 0)
			total_width += _r_calc_rectwidth (&rect);
	}

	return total_width;
}

VOID _r_toolbar_setbutton (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ UINT_PTR command_id,
	_In_opt_ LPWSTR string,
	_In_opt_ INT style,
	_In_opt_ INT state,
	_In_ INT image_id
)
{
	TBBUTTONINFOW tbi = {0};

	tbi.cbSize = sizeof (tbi);

	if (string)
	{
		tbi.dwMask |= TBIF_TEXT;

		tbi.pszText = string;
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

	_r_wnd_sendmessage (hwnd, ctrl_id, TB_SETBUTTONINFO, command_id, (LPARAM)&tbi);
}

VOID _r_toolbar_setstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_opt_ ULONG ex_style
)
{
	HWND hctrl;

	hctrl = _r_ctrl_getdlgitem (hwnd, ctrl_id);

	if (hctrl)
		SetWindowTheme (hctrl, L"Explorer", NULL);

	hctrl = _r_toolbar_gettooltips (hwnd, ctrl_id);

	if (hctrl)
		_r_ctrl_settipstyle (hctrl, FALSE);

	_r_wnd_sendmessage (hwnd, ctrl_id, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof (TBBUTTON), 0);

	if (ex_style)
		_r_wnd_sendmessage (hwnd, ctrl_id, TB_SETEXTENDEDSTYLE, 0, (LPARAM)ex_style);
}

//
// Util
//

BOOL CALLBACK _r_util_activate_window_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
)
{
	PR_STRINGREF app_name;
	PR_STRING string;
	ULONG pid;

	if (!_r_wnd_isdialog (hwnd))
		return TRUE;

	GetWindowThreadProcessId (hwnd, &pid);

	if (HandleToULong (NtCurrentProcessId ()) == pid)
		return TRUE;

	app_name = (PR_STRINGREF)lparam;

	if (!(_r_wnd_getstyle (hwnd, GWL_STYLE) & WS_DLGFRAME))
		return TRUE;

	// check window title
	string = _r_ctrl_getstring (hwnd, 0);

	if (!string)
		return TRUE;

	if (_r_str_isequal (app_name, &string->sr, FALSE))
	{
		// check window prop
		if (GetPropW (hwnd, app_name->buffer))
		{
			_r_wnd_toggle (hwnd, TRUE);

			_r_obj_dereference (string);

			return FALSE;
		}
	}

	_r_obj_dereference (string);

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

VOID _r_util_templatewritestring (
	_Inout_ PBYTE_PTR ptr,
	_In_ LPCWSTR string
)
{
	ULONG_PTR length;

	length = _r_str_getlength (string) * sizeof (WCHAR);

	*(LPWSTR)PTR_ADD_OFFSET (*ptr, length) = UNICODE_NULL; // terminate

	_r_util_templatewrite (ptr, string, length + sizeof (UNICODE_NULL));
}

VOID _r_util_templatewrite (
	_Inout_ PBYTE_PTR ptr,
	_In_bytecount_ (length) LPCVOID data,
	_In_ ULONG_PTR length
)
{
	RtlCopyMemory (*ptr, data, length);

	*ptr = PTR_ADD_OFFSET (*ptr, length);
}

VOID NTAPI _r_util_cleanarray_callback (
	_In_ PVOID object_body
)
{
	PR_ARRAY array_node;

	array_node = object_body;

	_r_obj_cleararray (array_node);

	array_node->allocated_count = 0;

	SAFE_DELETE_MEMORY (array_node->items);
}

VOID NTAPI _r_util_cleanlist_callback (
	_In_ PVOID object_body
)
{
	PR_LIST list_node;

	list_node = object_body;

	_r_obj_clearlist (list_node);

	list_node->allocated_count = 0;

	SAFE_DELETE_MEMORY (list_node->items);
}

VOID NTAPI _r_util_cleanhashtable_callback (
	_In_ PVOID object_body
)
{
	PR_HASHTABLE hashtable;

	hashtable = object_body;

	_r_obj_clearhashtable (hashtable);

	hashtable->allocated_buckets = 0;
	hashtable->allocated_entries = 0;

	SAFE_DELETE_MEMORY (hashtable->buckets);
	SAFE_DELETE_MEMORY (hashtable->entries);
}

VOID NTAPI _r_util_cleanhashtablepointer_callback (
	_In_ PVOID object_body
)
{
	PR_OBJECT_POINTER object_ptr;

	object_ptr = object_body;

	SAFE_DELETE_REFERENCE (object_ptr->object_body);
}
