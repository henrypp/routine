// routine.c
// project sdk library
//
// Copyright (c) 2012-2022 Henry++

#pragma once

// fix windot11.h errors
#if !defined(__WINDOT11_H__)
#define __WINDOT11_H__
#endif // !__WINDOT11_H__

// fix winbase.h errors
#if !defined(MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS)
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#endif // !MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#if !defined(CINTERFACE)
#define CINTERFACE
#endif // !CINTERFACE

#if !defined(COBJMACROS)
#define COBJMACROS
#endif // !COBJMACROS

#if !defined(INITGUID)
#define INITGUID
#endif // !INITGUID

#if !defined(UMDF_USING_NTSTATUS)
#define UMDF_USING_NTSTATUS
#endif // !UMDF_USING_NTSTATUS

// crt
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// winapi
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#include <ntstatus.h>
#include <aclapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dbghelp.h>
#include <dde.h>
#include <dwmapi.h>
#include <ntsecapi.h>
#include <psapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <subauth.h>
#include <taskschd.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <wincodec.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <wtsapi32.h>
#include <xmllite.h>

#if !defined(_ARM64_)
#include <smmintrin.h>
#endif

#include "ntapi.h"
#include "ntrtl.h"

#include "app.h"
#include "rconfig.h"
#include "rtypes.h"

// libs
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "xmllite.lib")

// SIDs
static SID SeNobodySid = {SID_REVISION, 1, SECURITY_NULL_SID_AUTHORITY, {SECURITY_NULL_RID}};
static SID SeEveryoneSid = {SID_REVISION, 1, SECURITY_WORLD_SID_AUTHORITY, {SECURITY_WORLD_RID}};
static SID SeLocalSid = {SID_REVISION, 1, SECURITY_LOCAL_SID_AUTHORITY, {SECURITY_LOCAL_RID}};

static SID SeCreatorOwnerSid = {SID_REVISION, 1, SECURITY_CREATOR_SID_AUTHORITY, {SECURITY_CREATOR_OWNER_RID}};
static SID SeCreatorGroupSid = {SID_REVISION, 1, SECURITY_CREATOR_SID_AUTHORITY, {SECURITY_CREATOR_GROUP_RID}};

static SID SeDialupSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_DIALUP_RID}};
static SID SeNetworkSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_NETWORK_RID}};
static SID SeBatchSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_BATCH_RID}};
static SID SeInteractiveSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_INTERACTIVE_RID}};
static SID SeServiceSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_SERVICE_RID}};
static SID SeAnonymousLogonSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_ANONYMOUS_LOGON_RID}};
static SID SeProxySid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_PROXY_RID}};
static SID SeAuthenticatedUserSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_AUTHENTICATED_USER_RID}};
static SID SeRestrictedCodeSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_RESTRICTED_CODE_RID}};
static SID SeTerminalServerUserSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_TERMINAL_SERVER_RID}};
static SID SeRemoteInteractiveLogonSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_REMOTE_LOGON_RID}};
static SID SeLocalSystemSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_LOCAL_SYSTEM_RID}};
static SID SeLocalServiceSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_LOCAL_SERVICE_RID}};
static SID SeNetworkServiceSid = {SID_REVISION, 1, SECURITY_NT_AUTHORITY, {SECURITY_NETWORK_SERVICE_RID}};

// safe clenup memory
#ifndef SAFE_DELETE_MEMORY
#define SAFE_DELETE_MEMORY(p) {if(p) {_r_mem_free (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_REFERENCE
#define SAFE_DELETE_REFERENCE(p) {if(p) {_r_obj_clearreference (&(p));}}
#endif

#ifndef SAFE_DELETE_STREAM
#define SAFE_DELETE_STREAM(p) {if(p) {IStream_Release ((p)); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_DC
#define SAFE_DELETE_DC(p) {if(p) {DeleteDC (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_OBJECT
#define SAFE_DELETE_OBJECT(p) {if(p) {DeleteObject (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_HANDLE
#define SAFE_DELETE_HANDLE(p) {if(_r_fs_isvalidhandle ((p))) {NtClose ((p)); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_LIBRARY
#define SAFE_DELETE_LIBRARY(p) {if((p)) {FreeLibrary ((p)); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_ICON
#define SAFE_DELETE_ICON(p) {if((p)) {DestroyIcon ((p)); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_LOCAL
#define SAFE_DELETE_LOCAL(p) {if((p)) {LocalFree ((p)); ((p))=NULL;}}
#endif

#ifndef SAFE_DELETE_GLOBAL
#define SAFE_DELETE_GLOBAL(p) {if((p)) {GlobalFree ((p)); (p)=NULL;}}
#endif

//
// Synchronization
//

FORCEINLINE BOOLEAN _InterlockedBitTestAndSetPointer (
	_Inout_ _Interlocked_operand_ LONG_PTR volatile *base,
	_In_ LONG_PTR bit
)
{
#ifdef _WIN64
	return _interlockedbittestandset64 ((PLONG64)base, (LONG64)bit);
#else
	return _interlockedbittestandset ((PLONG)base, (LONG)bit);
#endif
}

FORCEINLINE LONG_PTR InterlockedExchangeAddPointer (
	_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend,
	_In_ LONG_PTR value
)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedExchangeAdd64 ((PLONG64)addend, (LONG64)value);
#else
	return (LONG_PTR)_InterlockedExchangeAdd ((PLONG)addend, (LONG)value);
#endif
}

FORCEINLINE LONG_PTR InterlockedIncrementPointer (
	_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend
)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedIncrement64 ((PLONG64)addend);
#else
	return (LONG_PTR)_InterlockedIncrement ((PLONG)addend);
#endif
}

FORCEINLINE LONG_PTR InterlockedDecrementPointer (
	_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend
)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedDecrement64 ((PLONG64)addend);
#else
	return (LONG_PTR)_InterlockedDecrement ((PLONG)addend);
#endif
}

//
// Synchronization: Auto-dereference pool
//

VOID _r_autopool_initialize (
	_Out_ PR_AUTO_POOL auto_pool
);

VOID _r_autopool_destroy (
	_Inout_ PR_AUTO_POOL auto_pool
);

VOID _r_autopool_drain (
	_Inout_ PR_AUTO_POOL auto_pool
);

//
// Synchronization: A fast event object.
//

VOID FASTCALL _r_event_intialize (
	_Out_ PR_EVENT event_object
);

VOID FASTCALL _r_event_set (
	_Inout_ PR_EVENT event_object
);

VOID FASTCALL _r_event_reset (
	_Inout_ PR_EVENT event_object
);

BOOLEAN FASTCALL _r_event_wait (
	_Inout_ PR_EVENT event_object,
	_In_opt_ PLARGE_INTEGER timeout
);

BOOLEAN FASTCALL _r_event_wait_ex (
	_Inout_ PR_EVENT event_object,
	_In_opt_ PLARGE_INTEGER timeout
);

FORCEINLINE BOOLEAN _r_event_test (
	_In_ PR_EVENT event_object
)
{
	return !!event_object->is_set;
}

FORCEINLINE VOID _r_event_reference (
	_Inout_ PR_EVENT event_object
)
{
	InterlockedExchangeAddPointer ((PLONG_PTR)(&event_object->value), PR_EVENT_REFCOUNT_INC);
}

FORCEINLINE VOID _r_event_dereference (
	_Inout_ PR_EVENT event_object,
	_In_opt_ HANDLE event_handle
)
{
	ULONG_PTR value;

	value = InterlockedExchangeAddPointer ((PLONG_PTR)(&event_object->value), -PR_EVENT_REFCOUNT_INC);

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

//
// Synchronization: One-time initialization
//

#if defined(APP_NO_DEPRECATIONS)

BOOLEAN _r_initonce_begin (
	_Inout_ PR_INITONCE init_once
);

FORCEINLINE VOID _r_initonce_end (
	_Inout_ PR_INITONCE init_once
)
{
	RtlRunOnceComplete (init_once, 0, NULL);
}

#else

BOOLEAN FASTCALL _r_initonce_begin (
	_Inout_ PR_INITONCE init_once
);

BOOLEAN FASTCALL _r_initonce_begin_ex (
	_Inout_ PR_INITONCE init_once
);

VOID FASTCALL _r_initonce_end (
	_Inout_ PR_INITONCE init_once
);

#endif // APP_NO_DEPRECATIONS

//
// Synchronization: Free list
//

VOID _r_freelist_initialize (
	_Out_ PR_FREE_LIST free_list,
	_In_ SIZE_T size,
	_In_ ULONG maximum_count
);

VOID _r_freelist_destroy (
	_Inout_ PR_FREE_LIST free_list
);

PVOID _r_freelist_allocateitem (
	_Inout_ PR_FREE_LIST free_list
);

VOID _r_freelist_deleteitem (
	_Inout_ PR_FREE_LIST free_list,
	_In_ PVOID base_address
);

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

FORCEINLINE VOID _r_queuedlock_initialize (
	_Out_ PR_QUEUED_LOCK queued_lock
)
{
	queued_lock->value = 0;
}

HANDLE _r_queuedlock_getevent ();

VOID FASTCALL _r_queuedlock_acquireexclusive_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
);

VOID FASTCALL _r_queuedlock_acquireshared_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
);

VOID FASTCALL _r_queuedlock_releaseexclusive_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
);

VOID FASTCALL _r_queuedlock_releaseshared_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock
);

VOID FASTCALL _r_queuedlock_optimizelist (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
);

VOID FASTCALL _r_queuedlock_wake (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
);

VOID FASTCALL _r_queuedlock_wake_ex (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value,
	_In_ BOOLEAN is_ignoreowned,
	_In_ BOOLEAN is_wakeall
);

VOID FASTCALL _r_queuedlock_wakeforrelease (
	_Inout_ PR_QUEUED_LOCK queued_lock,
	_In_ ULONG_PTR value
);

_Acquires_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_acquireexclusive (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	if (_InterlockedBitTestAndSetPointer ((PLONG_PTR)&queued_lock->value, PR_QUEUED_LOCK_OWNED_SHIFT))
	{
		// Owned bit was already set. Slow path.
		_r_queuedlock_acquireexclusive_ex (queued_lock);
	}
}

_Acquires_shared_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_acquireshared (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	ULONG_PTR value;

	value = (ULONG_PTR)_InterlockedCompareExchangePointer (
		(PVOID_PTR)&queued_lock->value,
		IntToPtr (PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_SHARED_INC),
		IntToPtr (0)
	);

	if (value != 0)
		_r_queuedlock_acquireshared_ex (queued_lock);
}

_Releases_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_releaseexclusive (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	ULONG_PTR value;

	value = (ULONG_PTR)InterlockedExchangeAddPointer (
		(PLONG_PTR)&queued_lock->value,
		-(LONG_PTR)PR_QUEUED_LOCK_OWNED
	);

	if ((value & (PR_QUEUED_LOCK_WAITERS | PR_QUEUED_LOCK_TRAVERSING)) == PR_QUEUED_LOCK_WAITERS)
		_r_queuedlock_wakeforrelease (queued_lock, value - PR_QUEUED_LOCK_OWNED);
}

_Releases_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_releaseshared (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_SHARED_INC;

	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer (
		(PVOID_PTR)&queued_lock->value,
		IntToPtr (0),
		(PVOID)value
	);

	if (new_value != value)
		_r_queuedlock_releaseshared_ex (queued_lock);
}

_When_ (return != FALSE, _Acquires_exclusive_lock_ (*queued_lock))
FORCEINLINE BOOLEAN _r_queuedlock_tryacquireexclusive (
	_Inout_ PR_QUEUED_LOCK queued_lock
)
{
	if (!_InterlockedBitTestAndSetPointer (
		(PLONG_PTR)&queued_lock->value,
		PR_QUEUED_LOCK_OWNED_SHIFT))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

FORCEINLINE BOOLEAN _r_queuedlock_islocked (
	_In_ const PR_QUEUED_LOCK spin_lock
)
{
	BOOLEAN is_owned;

	// Need two memory barriers because we don't want the compiler re-ordering the following check
	// in either direction.
	MemoryBarrier ();

	is_owned = !!(spin_lock->value & PR_QUEUED_LOCK_OWNED);

	MemoryBarrier ();

	return is_owned;
}

//
// Synchronization: Condition
//

FORCEINLINE VOID _r_condition_initialize (
	_Out_ PR_CONDITION condition
)
{
	condition->value = 0;
}

VOID FASTCALL _r_condition_pulse (
	_Inout_ PR_CONDITION condition
);

VOID FASTCALL _r_condition_waitfor (
	_Inout_ PR_CONDITION condition,
	_Inout_ PR_QUEUED_LOCK queued_lock
);

//
// Synchronization: Rundown protection
//

FORCEINLINE VOID _r_protection_initialize (
	_Out_ PR_RUNDOWN_PROTECT protection
)
{
	protection->value = 0;
}

BOOLEAN FASTCALL _r_protection_acquire_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
);

VOID FASTCALL _r_protection_release_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
);

VOID FASTCALL _r_protection_waitfor_ex (
	_Inout_ PR_RUNDOWN_PROTECT protection
);

FORCEINLINE BOOLEAN _r_protection_acquire (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = protection->value & ~PR_RUNDOWN_ACTIVE; // fail fast path when rundown is active
	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer (
		(PVOID_PTR)&protection->value,
		(PVOID)(value + PR_RUNDOWN_REF_INC),
		(PVOID)value
	);

	if (new_value == value)
	{
		return TRUE;
	}
	else
	{
		return _r_protection_acquire_ex (protection);
	}
}

FORCEINLINE VOID _r_protection_release (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = protection->value & ~PR_RUNDOWN_ACTIVE; // Fail fast path when rundown is active
	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer (
		(PVOID_PTR)&protection->value,
		(PVOID)(value - PR_RUNDOWN_REF_INC),
		(PVOID)value
	);

	if (new_value != value)
		_r_protection_release_ex (protection);
}

FORCEINLINE VOID _r_protection_waitfor (
	_Inout_ PR_RUNDOWN_PROTECT protection
)
{
	ULONG_PTR value;

	value = (ULONG_PTR)_InterlockedCompareExchangePointer (
		(PVOID_PTR)&protection->value,
		IntToPtr (PR_RUNDOWN_ACTIVE),
		IntToPtr (0)
	);

	if (value != 0 && value != PR_RUNDOWN_ACTIVE)
		_r_protection_waitfor_ex (protection);
}

//
// Synchronization: Workqueue
//

PR_FREE_LIST _r_workqueue_getfreelist ();

VOID _r_workqueue_initialize (
	_Out_ PR_WORKQUEUE work_queue,
	_In_ ULONG minimum_threads,
	_In_ ULONG maximum_threads,
	_In_ ULONG no_work_timeout,
	_In_opt_ PR_ENVIRONMENT environment
);

VOID _r_workqueue_destroy (
	_Inout_ PR_WORKQUEUE work_queue
);

PR_WORKQUEUE_ITEM _r_workqueue_createitem (
	_In_ PR_WORKQUEUE_FUNCTION function_address,
	_In_opt_ PVOID context
);

VOID _r_workqueue_destroyitem (
	_In_ PR_WORKQUEUE_ITEM work_queue_item
);

VOID _r_workqueue_queueitem (
	_Inout_ PR_WORKQUEUE work_queue,
	_In_ PR_WORKQUEUE_FUNCTION function_address,
	_In_opt_ PVOID context
);

VOID _r_workqueue_waitforfinish (
	_Inout_ PR_WORKQUEUE work_queue
);

//
// Synchronization: Mutex
//

_When_ (return != FALSE, _Acquires_lock_ (*hmutex))
BOOLEAN _r_mutex_create (
	_In_ LPCWSTR name,
	_Out_ PHANDLE hmutex
);

_When_ (return != FALSE, _Releases_lock_ (*hmutex))
BOOLEAN _r_mutex_destroy (
	_Inout_ PHANDLE hmutex
);

BOOLEAN _r_mutex_isexists (
	_In_ LPCWSTR name
);

//
// Memory allocation
//

HANDLE NTAPI _r_mem_getheap ();

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocate (
	_In_ SIZE_T bytes_count
);

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocatezero (
	_In_ SIZE_T bytes_count
);

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocatezerosafe (
	_In_ SIZE_T bytes_count
);

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_allocateandcopy (
	_In_ LPCVOID src,
	_In_ SIZE_T bytes_count
);

// // If RtlReAllocateHeap fails, the original memory is not freed, and the original handle and pointer are still valid.
// // https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocate (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
);

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocatezero (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
);

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_mem_reallocatezerosafe (
	_Frees_ptr_opt_ PVOID base_address,
	_In_ SIZE_T bytes_count
);

VOID NTAPI _r_mem_free (
	_Frees_ptr_opt_ PVOID base_address
);

BOOLEAN _r_mem_frobnicate (
	_Inout_ PR_BYTEREF bytes
);

//
// Objects reference
//

_Post_writable_byte_size_ (bytes_count)
PVOID NTAPI _r_obj_allocate (
	_In_ SIZE_T bytes_count,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

VOID NTAPI _r_obj_dereference (
	_In_ PVOID object_body
);

VOID NTAPI _r_obj_dereferencelist (
	_In_reads_ (objects_count) PVOID_PTR objects_list,
	_In_ SIZE_T objects_count
);

VOID NTAPI _r_obj_dereference_ex (
	_In_ PVOID object_body,
	_In_ LONG ref_count
);

PVOID NTAPI _r_obj_reference (
	_In_ PVOID object_body
);

_Ret_maybenull_
PVOID NTAPI _r_obj_referencesafe (
	_In_opt_ PVOID object_body
);

FORCEINLINE VOID _r_obj_swapreference (
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
		_r_obj_reference (new_object);
}

FORCEINLINE VOID _r_obj_movereference (
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

FORCEINLINE VOID _r_obj_clearreference (
	_Inout_ PVOID_PTR object_body
)
{
	_r_obj_movereference (object_body, NULL);
}

//
// 8-bit string object
//

#define _r_obj_isbyteempty(string) \
    ((string) == NULL || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == ANSI_NULL)

PR_BYTE _r_obj_createbyte (
	_In_ LPSTR string
);

PR_BYTE _r_obj_createbyte2 (
	_In_ PR_BYTE string
);

PR_BYTE _r_obj_createbyte3 (
	_In_ PR_BYTEREF string
);

PR_BYTE _r_obj_createbyte_ex (
	_In_opt_ LPCSTR buffer,
	_In_ SIZE_T length
);

BOOLEAN _r_obj_isbytenullterminated (
	_In_ PR_BYTEREF string
);

VOID _r_obj_setbytelength (
	_Inout_ PR_BYTE string,
	_In_ SIZE_T new_length
);

VOID _r_obj_setbytelength_ex (
	_Inout_ PR_BYTE string,
	_In_ SIZE_T new_length,
	_In_ SIZE_T allocated_length
);

VOID _r_obj_skipbytelength (
	_Inout_ PR_BYTEREF string,
	_In_ SIZE_T length
);

VOID _r_obj_trimbytetonullterminator (
	_In_ PR_BYTE string
);

VOID _r_obj_writebytenullterminator (
	_In_ PR_BYTE string
);

//
// 16-bit string object
//

#define _r_obj_isstringempty(string) \
    ((string) == NULL || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == UNICODE_NULL)

#define _r_obj_isstringempty2(string) \
    ((string)->length == 0 || (string)->buffer[0] == UNICODE_NULL)

PR_STRING _r_obj_createstring (
	_In_ LPCWSTR string
);

PR_STRING _r_obj_createstring2 (
	_In_ PR_STRING string
);

PR_STRING _r_obj_createstring3 (
	_In_ PR_STRINGREF string
);

PR_STRING _r_obj_createstring4 (
	_In_ PUNICODE_STRING string
);

PR_STRING _r_obj_createstring_ex (
	_In_opt_ LPCWSTR buffer,
	_In_ SIZE_T length
);

PR_STRING _r_obj_concatstrings (
	_In_ SIZE_T count,
	...
);

PR_STRING _r_obj_concatstrings_v (
	_In_ SIZE_T count,
	_In_ va_list arg_ptr
);

PR_STRING _r_obj_concatstringrefs (
	_In_ SIZE_T count,
	...
);

PR_STRING _r_obj_concatstringrefs_v (
	_In_ SIZE_T count,
	_In_ va_list arg_ptr
);

BOOLEAN _r_obj_isstringnullterminated (
	_In_ PR_STRINGREF string
);

PR_STRING _r_obj_referenceemptystring ();

VOID _r_obj_removestring (
	_In_ PR_STRING string,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T length
);

VOID _r_obj_setstringlength (
	_Inout_ PR_STRING string,
	_In_ SIZE_T new_length
);

VOID _r_obj_setstringlength_ex (
	_Inout_ PR_STRING string,
	_In_ SIZE_T new_length,
	_In_ SIZE_T allocated_length
);

VOID _r_obj_skipstringlength (
	_Inout_ PR_STRINGREF string,
	_In_ SIZE_T length
);

VOID _r_obj_trimstringtonullterminator (
	_In_ PR_STRING string
);

VOID _r_obj_writestringnullterminator (
	_In_ PR_STRING string
);

_Ret_maybenull_
FORCEINLINE LPCWSTR _r_obj_getstring (
	_In_opt_ PR_STRING string
)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return NULL;
}

FORCEINLINE LPCWSTR _r_obj_getstringorempty (
	_In_opt_ PR_STRING string
)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return L"";
}

FORCEINLINE LPCWSTR _r_obj_getstringordefault (
	_In_opt_ PR_STRING string,
	_In_ LPCWSTR def
)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return def;
}

//
// 8-bit string reference object
//

VOID _r_obj_initializebyterefempty (
	_Out_ PR_BYTEREF string
);

VOID _r_obj_initializebyterefconst (
	_Out_ PR_BYTEREF string,
	_In_ LPCSTR buffer
);

VOID _r_obj_initializebyteref (
	_Out_ PR_BYTEREF string,
	_In_ LPSTR buffer
);

VOID _r_obj_initializebyteref2 (
	_Out_ PR_BYTEREF string,
	_In_ PR_BYTE buffer
);

VOID _r_obj_initializebyteref3 (
	_Out_ PR_BYTEREF string,
	_In_ PR_BYTEREF buffer
);

VOID _r_obj_initializebyteref_ex (
	_Out_ PR_BYTEREF string,
	_In_opt_ LPSTR buffer,
	_In_opt_ SIZE_T length
);

//
// 16-bit string reference object
//

VOID _r_obj_initializestringrefempty (
	_Out_ PR_STRINGREF string
);

VOID _r_obj_initializestringrefconst (
	_Out_ PR_STRINGREF string,
	_In_ LPCWSTR buffer
);

VOID _r_obj_initializestringref (
	_Out_ PR_STRINGREF string,
	_In_ LPWSTR buffer
);

VOID _r_obj_initializestringref2 (
	_Out_ PR_STRINGREF string,
	_In_ PR_STRING buffer
);

VOID _r_obj_initializestringref3 (
	_Out_ PR_STRINGREF string,
	_In_ PR_STRINGREF buffer
);

VOID _r_obj_initializestringref4 (
	_Out_ PR_STRINGREF string,
	_In_ PUNICODE_STRING buffer
);

VOID _r_obj_initializestringref_ex (
	_Out_ PR_STRINGREF string,
	_In_opt_ LPWSTR buffer,
	_In_opt_ SIZE_T length
);

//
// Unicode string object
//

BOOLEAN _r_obj_initializeunicodestring2 (
	_Out_ PUNICODE_STRING string,
	_In_ PR_STRING buffer
);

BOOLEAN _r_obj_initializeunicodestring3 (
	_Out_ PUNICODE_STRING string,
	_In_ PR_STRINGREF buffer
);

BOOLEAN _r_obj_initializeunicodestring_ex (
	_Out_ PUNICODE_STRING string,
	_In_opt_ LPWSTR buffer,
	_In_opt_ USHORT length,
	_In_opt_ USHORT max_length
);

//
// String builder
//

VOID _r_obj_initializestringbuilder (
	_Out_ PR_STRINGBUILDER builder
);

VOID _r_obj_initializestringbuilder_ex (
	_Out_ PR_STRINGBUILDER builder,
	_In_ SIZE_T initial_capacity
);

VOID _r_obj_deletestringbuilder (
	_Inout_ PR_STRINGBUILDER builder
);

PR_STRING _r_obj_finalstringbuilder (
	_In_ PR_STRINGBUILDER builder
);

VOID _r_obj_appendstringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ LPCWSTR string
);

VOID _r_obj_appendstringbuilder2 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PR_STRING string
);

VOID _r_obj_appendstringbuilder3 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PR_STRINGREF string
);

VOID _r_obj_appendstringbuilder4 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ PUNICODE_STRING string
);

VOID _r_obj_appendstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ LPCWSTR string,
	_In_ SIZE_T length
);

VOID _r_obj_appendstringbuilderformat (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_obj_appendstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
);

VOID _r_obj_insertstringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ LPCWSTR string
);

VOID _r_obj_insertstringbuilder2 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PR_STRING string
);

VOID _r_obj_insertstringbuilder3 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PR_STRINGREF string
);

VOID _r_obj_insertstringbuilder4 (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ PUNICODE_STRING string
);

VOID _r_obj_insertstringbuilder_ex (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index, _In_ LPCWSTR string,
	_In_ SIZE_T length
);

VOID _r_obj_insertstringbuilderformat (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_obj_insertstringbuilderformat_v (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T index,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
);

VOID _r_obj_resizestringbuilder (
	_Inout_ PR_STRINGBUILDER builder,
	_In_ SIZE_T new_capacity
);

//
// Array object
//

#define _r_obj_isarrayempty(array_node) \
    ((array_node) == NULL || (array_node)->count == 0)

PR_ARRAY _r_obj_createarray_ex (
	_In_ SIZE_T item_size,
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

PR_ARRAY _r_obj_createarray (
	_In_ SIZE_T item_size,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

PVOID _r_obj_addarrayitem_ex (
	_Inout_ PR_ARRAY array_node,
	_In_opt_ LPCVOID array_item,
	_Out_opt_ PSIZE_T new_index_ptr
);

PVOID _r_obj_addarrayitem (
	_Inout_ PR_ARRAY array_node,
	_In_opt_ LPCVOID array_item
);

VOID _r_obj_addarrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ LPCVOID array_items,
	_In_ SIZE_T count
);

VOID _r_obj_cleararray (
	_Inout_ PR_ARRAY array_node
);

PVOID _r_obj_getarrayitem (
	_In_ PR_ARRAY array_node,
	_In_ SIZE_T index
);

SIZE_T _r_obj_getarraysize (
	_In_ PR_ARRAY array_node
);

VOID _r_obj_removearrayitem (
	_In_ PR_ARRAY array_node,
	_In_ SIZE_T index
);

VOID _r_obj_removearrayitems (
	_Inout_ PR_ARRAY array_node,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T count
);

VOID _r_obj_resizearray (
	_Inout_ PR_ARRAY array_node,
	_In_ SIZE_T new_capacity
);

//
// List object
//

#define _r_obj_islistempty(list_node) \
    ((list_node) == NULL || (list_node)->count == 0)

#define _r_obj_islistempty2(list_node) \
    ((list_node)->count == 0)

PR_LIST _r_obj_createlist_ex (
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

PR_LIST _r_obj_createlist (
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

VOID _r_obj_addlistitem_ex (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item,
	_Out_opt_ PSIZE_T new_index_ptr
);

VOID _r_obj_addlistitem (
	_Inout_ PR_LIST list_node,
	_In_opt_ PVOID list_item
);

VOID _r_obj_clearlist (
	_Inout_ PR_LIST list_node
);

_Success_ (return != SIZE_MAX)
SIZE_T _r_obj_findlistitem (
	_In_ PR_LIST list_node,
	_In_opt_ LPCVOID list_item
);

_Ret_maybenull_
PVOID _r_obj_getlistitem (
	_In_ PR_LIST list_node,
	_In_ SIZE_T index
);

SIZE_T _r_obj_getlistsize (
	_In_ PR_LIST list_node
);

VOID _r_obj_insertlistitems (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T start_pos,
	_In_ PVOID_PTR list_items,
	_In_ SIZE_T count
);

VOID _r_obj_removelistitem (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T index
);

VOID _r_obj_removelistitems (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T start_pos,
	_In_ SIZE_T count
);

VOID _r_obj_resizelist (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T new_capacity
);

VOID _r_obj_setlistitem (
	_Inout_ PR_LIST list_node,
	_In_ SIZE_T index,
	_In_opt_ PVOID list_item
);

//
// Hashtable object
//

// A hashtable with power-of-two bucket sizes and with all entries stored in a single
// array. This improves locality but may be inefficient when resizing the hashtable. It is a good
// idea to store pointers to objects as entries, as opposed to the objects themselves.

#define _r_obj_ishashtableempty(hashtable) \
    ((hashtable) == NULL || (hashtable)->count == 0)

FORCEINLINE ULONG _r_obj_validatehash (
	_In_ ULONG_PTR hash_code
)
{
	return hash_code & MAXLONG;
}

FORCEINLINE SIZE_T _r_obj_indexfromhash (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG hash_code
)
{
	return hash_code & (hashtable->allocated_buckets - 1);
}

PR_HASHTABLE _r_obj_createhashtable_ex (
	_In_ SIZE_T entry_size,
	_In_ SIZE_T initial_capacity,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

PR_HASHTABLE _r_obj_createhashtable (
	_In_ SIZE_T entry_size,
	_In_opt_ PR_OBJECT_CLEANUP_CALLBACK cleanup_callback
);

_Ret_maybenull_
PVOID _r_obj_addhashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry
);

PVOID _r_obj_replacehashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ LPCVOID entry
);

VOID _r_obj_clearhashtable (
	_Inout_ PR_HASHTABLE hashtable
);

_Success_ (return)
BOOLEAN _r_obj_enumhashtable (
	_In_ PR_HASHTABLE hashtable,
	_Outptr_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PSIZE_T enum_key
);

_Ret_maybenull_
PVOID _r_obj_findhashtable (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
);

SIZE_T _r_obj_gethashtablesize (
	_In_ PR_HASHTABLE hashtable
);

BOOLEAN _r_obj_removehashtableitem (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
);

VOID _r_obj_resizehashtable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ SIZE_T new_capacity
);

//
// Hashtable pointer object
//

PR_HASHTABLE _r_obj_createhashtablepointer (
	_In_ SIZE_T initial_capacity
);

_Ret_maybenull_
PR_OBJECT_POINTER _r_obj_addhashtablepointer (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_opt_ PVOID value
);

_Success_ (return)
BOOLEAN _r_obj_enumhashtablepointer (
	_In_ PR_HASHTABLE hashtable,
	_Outptr_opt_ PVOID_PTR entry_ptr,
	_Out_opt_ PULONG_PTR hash_code_ptr,
	_Inout_ PSIZE_T enum_key
);

_Ret_maybenull_
PVOID _r_obj_findhashtablepointer (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
);

BOOLEAN _r_obj_removehashtablepointer (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code
);

//
// Debugging
//

VOID _r_debug (
	_In_ LPCWSTR string
);

VOID _r_debug_v (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_error_initialize (
	_Out_ PR_ERROR_INFO error_info,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ LPCWSTR description
);

VOID _r_error_initialize_ex (
	_Out_ PR_ERROR_INFO error_info,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ LPCWSTR description,
	_In_opt_ PEXCEPTION_POINTERS exception_ptr
);

#define RDBG(a) _r_debug ((a))
#define RDBG2(a, ...) _r_debug_v ((a), __VA_ARGS__)

//
// Console
//

_Ret_maybenull_
HANDLE _r_console_gethandle ();

WORD _r_console_getcolor ();

VOID _r_console_setcolor (
	_In_ WORD clr
);

VOID _r_console_writestring (
	_In_ LPCWSTR string
);

VOID _r_console_writestring2 (
	_In_ PR_STRING string
);

VOID _r_console_writestring3 (
	_In_ PR_STRINGREF string
);

VOID _r_console_writestring_ex (
	_In_ LPCWSTR string,
	_In_ ULONG length
);

VOID _r_console_writestringformat (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

//
// Format strings, dates, numbers
//

PR_STRING _r_format_string (
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

PR_STRING _r_format_string_v (
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
);

_Success_ (return)
BOOLEAN _r_format_bytesize64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ UINT buffer_size,
	_In_ ULONG64 bytes
);

_Ret_maybenull_
PR_STRING _r_format_filetime_ex (
	_In_ LPFILETIME file_time,
	_In_ ULONG flags
);

_Ret_maybenull_
PR_STRING _r_format_interval (
	_In_ LONG64 seconds,
	_In_ INT digits
);

_Success_ (return)
BOOLEAN _r_format_number (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ ULONG buffer_size,
	_In_ LONG64 number
);

_Ret_maybenull_
PR_STRING _r_format_unixtime (
	_In_ LONG64 unixtime
);

_Ret_maybenull_
PR_STRING _r_format_unixtime_ex (
	_In_ LONG64 unixtime,
	_In_ ULONG flags
);

//
// Calculation
//

LONG _r_calc_clamp (
	_In_ LONG value,
	_In_ LONG min_value,
	_In_ LONG max_value
);

LONG64 _r_calc_clamp64 (
	_In_ LONG64 value,
	_In_ LONG64 min_value,
	_In_ LONG64 max_value
);

ULONG _r_calc_countbits (
	_In_ ULONG value
);

VOID _r_calc_millisecondstolargeinteger (
	_Out_ PLARGE_INTEGER timeout,
	_In_ ULONG milliseconds
);

ULONG _r_calc_multipledivide (
	_In_ ULONG number,
	_In_ ULONG numerator,
	_In_ ULONG denominator
);

LONG _r_calc_multipledividesigned (
	_In_ LONG number,
	_In_ ULONG numerator,
	_In_ ULONG denominator
);

LONG _r_calc_percentof (
	_In_ LONG length,
	_In_ LONG total_length
);

LONG _r_calc_percentof64 (
	_In_ LONG64 length,
	_In_ LONG64 total_length
);

LONG _r_calc_percentval (
	_In_ LONG percent,
	_In_ LONG total_length
);

LONG64 _r_calc_percentval64 (
	_In_ LONG64 percent,
	_In_ LONG64 total_length
);

LONG _r_calc_rectheight (
	_In_ LPCRECT rect
);

LONG _r_calc_rectwidth (
	_In_ LPCRECT rect
);

ULONG64 _r_calc_roundnumber (
	_In_ ULONG64 value,
	_In_ ULONG64 granularity
);

FORCEINLINE LONG _r_calc_kilobytes2bytes (
	_In_ LONG kilobytes
)
{
	return kilobytes * 1024L;
}

FORCEINLINE LONG64 _r_calc_kilobytes2bytes64 (
	_In_ LONG64 kilobytes
)
{
	return kilobytes * 1024LL;
}

FORCEINLINE LONG _r_calc_megabytes2bytes (
	_In_ LONG megabytes
)
{
	return megabytes * 1048576L;
}

FORCEINLINE LONG64 _r_calc_megabytes2bytes64 (
	_In_ LONG64 megabytes
)
{
	return megabytes * 1048576LL;
}

FORCEINLINE LONG _r_calc_seconds2milliseconds (
	_In_ LONG seconds
)
{
	return seconds * 1000L;
}

FORCEINLINE LONG64 _r_calc_seconds2milliseconds64 (
	_In_ LONG64 seconds
)
{
	return seconds * 1000LL;
}

FORCEINLINE LONG _r_calc_minutes2seconds (
	_In_ LONG minutes
)
{
	return minutes * 60L;
}

FORCEINLINE LONG _r_calc_hours2seconds (
	_In_ LONG hours
)
{
	return hours * 3600L;
}

FORCEINLINE LONG _r_calc_days2seconds (
	_In_ LONG days
)
{
	return days * 86400L;
}

//
// Byte swap
//

#define _r_byteswap_ushort _byteswap_ushort
#define _r_byteswap_ulong _byteswap_ulong
#define _r_byteswap_ulong64 _byteswap_uint64

//
// Modal dialogs
//

HRESULT CALLBACK _r_msg_callback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam,
	_In_opt_ LONG_PTR lpdata
);

// TaskDialogIndirect (vista+)
_Success_ (return)
BOOLEAN _r_msg_taskdialog (
	_In_ const TASKDIALOGCONFIG * task_dialog,
	_Out_opt_ PINT button_ptr,
	_Out_opt_ PINT radio_button_ptr,
	_Out_opt_ LPBOOL is_flagchecked_ptr
);

//
// Clipboard operations
//

_Ret_maybenull_
PR_STRING _r_clipboard_get (
	_In_opt_ HWND hwnd
);

BOOLEAN _r_clipboard_set (
	_In_opt_ HWND hwnd,
	_In_ PR_STRINGREF string
);

//
// Filesystem
//

BOOLEAN _r_fs_deletefile (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_forced
);

BOOLEAN _r_fs_deletedirectory (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_recurse
);

LONG64 _r_fs_getsize (
	_In_ HANDLE hfile
);

LONG64 _r_fs_getfilesize (
	_In_ LPCWSTR path
);

BOOLEAN _r_fs_makebackup (
	_In_ LPCWSTR path,
	_In_ BOOLEAN is_removesourcefile
);

BOOLEAN _r_fs_mkdir (
	_In_ LPCWSTR path
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_fs_mapfile (
	_In_ LPCWSTR path,
	_Out_ PR_BYTE_PTR out_buffer
);

LONG64 _r_fs_getpos (
	_In_ HANDLE hfile
);

BOOLEAN _r_fs_setpos (
	_In_ HANDLE hfile,
	_In_ LONG64 pos,
	_In_ ULONG method
);

#define _r_fs_isvalidhandle(handle) \
    ((handle) != NULL && (handle) != INVALID_HANDLE_VALUE)

FORCEINLINE BOOLEAN _r_fs_exists (
	_In_ LPCWSTR path
)
{
	return RtlDoesFileExists_U (path);
}

FORCEINLINE BOOLEAN _r_fs_copyfile (
	_In_ LPCWSTR path_from,
	_In_ LPCWSTR path_to,
	_In_ ULONG flags
)
{
	return !!CopyFileEx (path_from, path_to, NULL, NULL, NULL, flags);
}

FORCEINLINE BOOLEAN _r_fs_movefile (
	_In_ LPCWSTR path_from,
	_In_opt_ LPCWSTR path_to,
	_In_ ULONG flags
)
{
	return !!MoveFileEx (path_from, path_to, flags);
}

//
// Paths
//

PR_STRING _r_path_compact (
	_In_ PR_STRING path,
	_In_ ULONG length
);

BOOLEAN _r_path_getpathinfo (
	_In_ PR_STRINGREF path,
	_Out_opt_ PR_STRINGREF directory,
	_Out_opt_ PR_STRINGREF basename
);

_Ret_maybenull_
PR_STRING _r_path_getbasedirectory (
	_In_ PR_STRINGREF path
);

LPCWSTR _r_path_getbasename (
	_In_ LPCWSTR path
);

PR_STRING _r_path_getbasenamestring (
	_In_ PR_STRINGREF path
);

_Ret_maybenull_
LPCWSTR _r_path_getextension (
	_In_ LPCWSTR path
);

_Ret_maybenull_
PR_STRING _r_path_getextensionstring (
	_In_ PR_STRINGREF path
);

_Ret_maybenull_
PR_STRING _r_path_getfullpath (
	_In_ LPCWSTR path
);

_Ret_maybenull_
PR_STRING _r_path_getknownfolder (
	_In_ ULONG folder,
	_In_opt_ LPCWSTR append
);

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (
	_In_opt_ HINSTANCE hinstance
);

BOOLEAN _r_path_issecurelocation (
	_In_ LPCWSTR file_path
);

BOOLEAN _r_path_parsecommandlinefuzzy (
	_In_ PR_STRINGREF args,
	_Out_ PR_STRINGREF path,
	_Out_ PR_STRINGREF command_line,
	_Out_opt_ PR_STRING_PTR full_file_name
);

_Ret_maybenull_
PR_STRING _r_path_resolvedeviceprefix (
	_In_ PR_STRING path
);

_Ret_maybenull_
PR_STRING _r_path_resolvedeviceprefix_workaround (
	_In_ PR_STRING path
);

_Ret_maybenull_
PR_STRING _r_path_resolvenetworkprefix (
	_In_ PR_STRING path
);

_Ret_maybenull_
PR_STRING _r_path_search (
	_In_ LPCWSTR path,
	_In_opt_ LPCWSTR extension,
	_In_ BOOLEAN is_dontcheckattributes
);

PR_STRING _r_path_dospathfromnt (
	_In_ PR_STRING path
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_path_ntpathfromdos (
	_In_ PR_STRING path,
	_Out_ PR_STRING_PTR out_buffer
);

//
// Shell
//

VOID _r_shell_showfile (
	_In_ LPCWSTR path
);

FORCEINLINE VOID _r_shell_opendefault (
	_In_ LPCWSTR path
)
{
	ShellExecute (NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
}

//
// Strings
//

#define _r_str_isempty(string) \
    ((string) == NULL || (string)[0] == UNICODE_NULL)

#define _r_str_isempty2(string) \
    ((string)[0] == UNICODE_NULL)

#define _r_str_isbyteempty(string) \
	((string) == NULL || (string)[0] == ANSI_NULL)

VOID _r_str_append (
	_Inout_updates_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ LPCWSTR string
);

VOID _r_str_appendformat (
	_Inout_updates_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

_Check_return_
INT _r_str_compare (
	_In_ LPCWSTR string1,
	_In_ LPCWSTR string2
);

_Check_return_
INT _r_str_compare_length (
	_In_ LPCWSTR string1,
	_In_ LPCWSTR string2,
	_In_ SIZE_T length
);

_Check_return_
INT _r_str_compare_logical (
	_In_ PR_STRING string1,
	_In_ PR_STRING string2
);

VOID _r_str_copy (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ LPCWSTR string
);

VOID _r_str_copystring (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ PR_STRINGREF string
);

_Ret_maybenull_
PR_STRING _r_str_environmentexpandstring (
	_In_ PR_STRINGREF string
);

_Ret_maybenull_
PR_STRING _r_str_environmentunexpandstring (
	_In_ LPCWSTR string
);

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
);

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findlastchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR character,
	_In_ BOOLEAN is_ignorecase
);

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findstring (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF sub_string,
	_In_ BOOLEAN is_ignorecase
);

PR_STRING _r_str_formatversion (
	_In_ PR_STRING string
);

VOID _r_str_fromlong (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ LONG value
);

VOID _r_str_fromlong64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ LONG64 value
);

VOID _r_str_fromulong (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ ULONG value
);

VOID _r_str_fromulong64 (
	_Out_writes_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ ULONG64 value
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_fromguid (
	_In_ LPCGUID guid,
	_In_ BOOLEAN is_uppercase,
	_Out_ PR_STRING_PTR out_buffer
);

PR_STRING _r_str_fromhex (
	_In_reads_bytes_ (length) PUCHAR buffer,
	_In_ SIZE_T length,
	_In_ BOOLEAN is_uppercase
);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_str_fromsecuritydescriptor (
	_In_ PSECURITY_DESCRIPTOR security_descriptor,
	_In_ SECURITY_INFORMATION security_information,
	_Out_ PR_STRING_PTR out_buffer
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_fromsid (
	_In_ PSID sid,
	_Out_ PR_STRING_PTR out_buffer
);

VOID _r_str_generaterandom (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ SIZE_T buffer_size,
	_In_ BOOLEAN is_uppercase
);

ULONG _r_str_gethash (
	_In_ LPCWSTR string,
	_In_ BOOLEAN is_ignorecase
);

ULONG _r_str_gethash2 (
	_In_ PR_STRING string,
	_In_ BOOLEAN is_ignorecase
);

ULONG _r_str_gethash3 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

SIZE_T _r_str_getlength (
	_In_ LPCWSTR string
);

SIZE_T _r_str_getlength2 (
	_In_ PR_STRING string
);

SIZE_T _r_str_getlength3 (
	_In_ PR_STRINGREF string
);

SIZE_T _r_str_getlength4 (
	_In_ PUNICODE_STRING string
);

SIZE_T _r_str_getlength_ex (
	_In_reads_or_z_ (max_count) LPCWSTR string,
	_In_ SIZE_T max_count
);

SIZE_T _r_str_getbytelength (
	_In_ LPCSTR string
);

SIZE_T _r_str_getbytelength2 (
	_In_ PR_BYTE string
);

SIZE_T _r_str_getbytelength3 (
	_In_ PR_BYTEREF string
);

SIZE_T _r_str_getbytelength_ex (
	_In_reads_or_z_ (max_count) LPCSTR string,
	_In_ SIZE_T max_count
);

BOOLEAN _r_str_isdigit (
	_In_ WCHAR chr
);

BOOLEAN _r_str_isequal (
	_In_ PR_STRINGREF string1,
	_In_ PR_STRINGREF string2,
	_In_ BOOLEAN is_ignorecase
);

BOOLEAN _r_str_isequal2 (
	_In_ PR_STRINGREF string1,
	_In_ LPCWSTR string2,
	_In_ BOOLEAN is_ignorecase
);

BOOLEAN _r_str_isnumeric (
	_In_ PR_STRINGREF string
);

BOOLEAN _r_str_isstartswith (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF prefix,
	_In_ BOOLEAN is_ignorecase
);

BOOLEAN _r_str_isstartswith2 (
	_In_ PR_STRINGREF string,
	_In_ LPCWSTR prefix,
	_In_ BOOLEAN is_ignorecase
);

BOOLEAN _r_str_isendsswith (
	_In_ PR_STRINGREF string,
	_In_ PR_STRINGREF suffix,
	_In_ BOOLEAN is_ignorecase
);

BOOLEAN _r_str_isendsswith2 (
	_In_ PR_STRINGREF string,
	_In_ LPCWSTR suffix,
	_In_ BOOLEAN is_ignorecase
);

_Success_ (return)
BOOLEAN _r_str_match (
	_In_ LPCWSTR string,
	_In_ LPCWSTR pattern,
	_In_ BOOLEAN is_ignorecase
);

VOID _r_str_printf (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_str_printf_v (
	_Out_writes_z_ (buffer_size) LPWSTR buffer,
	_In_ _In_range_ (1, PR_SIZE_MAX_STRING_LENGTH) SIZE_T buffer_size,
	_In_ _Printf_format_string_ LPCWSTR format,
	_In_ va_list arg_ptr
);

VOID _r_str_replacechar (
	_Inout_ PR_STRINGREF string,
	_In_ WCHAR char_from,
	_In_ WCHAR char_to
);

BOOLEAN _r_str_splitatchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR separator,
	_Out_ PR_STRINGREF first_part,
	_Out_ PR_STRINGREF second_part
);

BOOLEAN _r_str_splitatlastchar (
	_In_ PR_STRINGREF string,
	_In_ WCHAR separator,
	_Out_ PR_STRINGREF first_part,
	_Out_ PR_STRINGREF second_part
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_toguid (
	_In_ PR_STRINGREF string,
	_Out_ LPGUID guid
);

BOOLEAN _r_str_toboolean (
	_In_ PR_STRINGREF string
);

LONG _r_str_tolong (
	_In_ PR_STRINGREF string
);

LONG64 _r_str_tolong64 (
	_In_ PR_STRINGREF string
);

ULONG _r_str_toulong (
	_In_ PR_STRINGREF string
);

ULONG64 _r_str_toulong64 (
	_In_ PR_STRINGREF string
);

BOOLEAN _r_str_tointeger64 (
	_In_ PR_STRINGREF string,
	_In_opt_ ULONG base,
	_Out_opt_ PULONG new_base_ptr,
	_Out_ PLONG64 integer_ptr
);

BOOLEAN _r_str_touinteger64 (
	_In_ PR_STRINGREF string,
	_In_ ULONG base,
	_Out_ PULONG64 integer_ptr
);

VOID _r_str_tolower (
	_Inout_ PR_STRINGREF string
);

VOID _r_str_toupper (
	_Inout_ PR_STRINGREF string
);

VOID _r_str_trimstring (
	_Inout_ PR_STRING string,
	_In_ PR_STRINGREF charset,
	_In_ ULONG flags
);

VOID _r_str_trimstring2 (
	_Inout_ PR_STRING string,
	_In_ LPCWSTR charset,
	_In_ ULONG flags
);

VOID _r_str_trimstringref (
	_Inout_ PR_STRINGREF string,
	_In_ PR_STRINGREF charset,
	_In_ ULONG flags
);

VOID _r_str_trimstringref2 (
	_Inout_ PR_STRINGREF string,
	_In_ LPCWSTR charset,
	_In_ ULONG flags
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_multibyte2unicode (
	_In_ PR_BYTEREF string,
	_Out_ PR_STRING_PTR out_buffer
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_str_unicode2multibyte (
	_In_ PR_STRINGREF string,
	_Out_ PR_BYTE_PTR out_buffer
);

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (
	_In_ PR_STRINGREF string,
	_In_ WCHAR key_delimeter,
	_In_ WCHAR value_delimeter
);

ULONG64 _r_str_versiontoulong64 (
	_In_ PR_STRINGREF version
);

INT _r_str_versioncompare (
	_In_ PR_STRINGREF v1,
	_In_ PR_STRINGREF v2
);

ULONG _r_str_crc32 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

ULONG64 _r_str_crc64 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

ULONG _r_str_fnv32a (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

ULONG64 _r_str_fnv64a (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

ULONG _r_str_x65599 (
	_In_ PR_STRINGREF string,
	_In_ BOOLEAN is_ignorecase
);

#if defined(_WIN64)
#define _r_str_fromlong_ptr _r_str_fromlong64
#define _r_str_fromulong_ptr _r_str_fromulong64

#define _r_str_tolong_ptr _r_str_tolong64
#define _r_str_toulong_ptr _r_str_toulong64
#else
#define _r_str_fromlong_ptr _r_str_fromlong
#define _r_str_fromulong_ptr _r_str_fromulong

#define _r_str_tolong_ptr _r_str_tolong
#define _r_str_toulong_ptr _r_str_toulong
#endif // _WIN64

#define _r_str_trim StrTrimW

#define _r_str_lower RtlDowncaseUnicodeChar
#define _r_str_upper RtlUpcaseUnicodeChar

//
// Performance
//

FORCEINLINE LONG64 _r_perf_querycounter ()
{
	LARGE_INTEGER counter;

#if defined(APP_NO_DEPRECATIONS)
	// win7+
	if (!RtlQueryPerformanceCounter (&counter))
		return 0;
#else
	if (!QueryPerformanceCounter (&counter))
		return 0;
#endif // APP_NO_DEPRECATIONS

	return counter.QuadPart;
}

FORCEINLINE LONG64 _r_perf_queryfrequency ()
{
	LARGE_INTEGER frequency;

#if defined(APP_NO_DEPRECATIONS)
	// win7+
	if (!RtlQueryPerformanceFrequency (&frequency))
		return 0;
#else
	if (!QueryPerformanceFrequency (&frequency))
		return 0;
#endif // APP_NO_DEPRECATIONS

	return frequency.QuadPart;
};

LONG64 _r_perf_getexecutionstart ();

DOUBLE _r_perf_getexecutionfinal (
	_In_ LONG64 start_time
);

//
// System information
//

BOOLEAN _r_sys_iselevated ();

BOOLEAN _r_sys_isosversionequal (
	_In_ ULONG version
);

BOOLEAN _r_sys_isosversiongreaterorequal (
	_In_ ULONG version
);

BOOLEAN _r_sys_isosversionlower (
	_In_ ULONG version
);

BOOLEAN _r_sys_isosversionlowerorequal (
	_In_ ULONG version
);

BOOLEAN _r_sys_isprocessimmersive (
	_In_ HANDLE hprocess
);

BOOLEAN _r_sys_iswine ();

BOOLEAN _r_sys_iswow64 ();

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_formatmessage (
	_In_ ULONG error_code,
	_In_opt_ HINSTANCE hinstance,
	_In_opt_ ULONG lang_id,
	_Out_ PR_STRING_PTR out_buffer
);

R_TOKEN_ATTRIBUTES _r_sys_getcurrenttoken ();

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_getlocaleinfo (
	_In_ LCID locale_id,
	_In_ LCTYPE locale_type,
	_Out_ PR_STRING_PTR out_buffer
);

VOID _r_sys_getsystemroot (
	_Out_ PR_STRINGREF path
);

PR_STRING _r_sys_getsystemdirectory ();

PR_STRING _r_sys_gettempdirectory ();

BOOLEAN _r_sys_getopt (
	_In_ LPCWSTR args,
	_In_ LPCWSTR name,
	_Outptr_opt_result_maybenull_ PR_STRING_PTR out_value
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_getservicesid (
	_In_ PR_STRINGREF name,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_sys_getsessioninfo (
	_In_ WTS_INFO_CLASS info_class,
	_Out_ PR_STRING_PTR out_buffer
);

ULONG _r_sys_gettickcount ();

ULONG64 _r_sys_gettickcount64 ();

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_getusernamefromsid (
	_In_ PSID sid,
	_Out_ PR_STRING_PTR out_buffer
);

ULONG _r_sys_getwindowsversion ();

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_compressbuffer (
	_In_ USHORT format,
	_In_ PR_BYTEREF buffer,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_decompressbuffer (
	_In_ USHORT format,
	_In_ PR_BYTEREF buffer,
	_Out_ PR_BYTE_PTR out_buffer
);

_Ret_maybenull_
HINSTANCE _r_sys_loadlibrary (
	_In_ LPCWSTR lib_name
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createprocess (
	_In_opt_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createprocess_ex (
	_In_opt_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory,
	_In_opt_ LPSTARTUPINFO startup_info_ptr,
	_In_ WORD show_state,
	_In_ ULONG flags
);

NTSTATUS _r_sys_openprocess (
	_In_opt_ HANDLE process_id,
	_In_ ACCESS_MASK desired_access,
	_Out_ PHANDLE process_handle
);

NTSTATUS _r_sys_queryprocessstring (
	_In_ HANDLE process_handle,
	_In_ PROCESSINFOCLASS info_class,
	_Out_ PVOID_PTR file_name
);

BOOLEAN _r_sys_runasadmin (
	_In_ LPCWSTR file_name,
	_In_opt_ LPCWSTR command_line,
	_In_opt_ LPCWSTR current_directory
);

VOID _r_sys_sleep (
	_In_ ULONG milliseconds
);

NTSTATUS NTAPI _r_sys_basethreadstart (
	_In_ PVOID arglist
);

PR_FREE_LIST _r_sys_getthreadfreelist ();

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_sys_createthread (
	_In_ PUSER_THREAD_START_ROUTINE function_address,
	_In_opt_ PVOID arglist,
	_Out_opt_ PHANDLE thread_handle,
	_In_opt_ PR_ENVIRONMENT environment
);

_Ret_maybenull_
HICON _r_sys_loadicon (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR icon_name,
	_In_ LONG width
);

_Ret_maybenull_
HICON _r_sys_loadsharedicon (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR icon_name,
	_In_ LONG width
);

_Ret_maybenull_
PR_STRING _r_sys_querytaginformation (
	_In_ HANDLE hprocess,
	_In_ LPCVOID tag
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_querytokeninformation (
	_In_ HANDLE token_handle,
	_In_ TOKEN_INFORMATION_CLASS token_class,
	_Out_ PVOID_PTR token_info
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_setprocessprivilege (
	_In_ HANDLE process_handle,
	_In_reads_ (count) PULONG privileges,
	_In_ ULONG count,
	_In_ BOOLEAN is_enable
);

VOID _r_sys_setenvironment (
	_Out_ PR_ENVIRONMENT environment,
	_In_ LONG base_priority,
	_In_ ULONG io_priority,
	_In_ ULONG page_priority
);

VOID _r_sys_setdefaultprocessenvironment (
	_Out_ PR_ENVIRONMENT environment
);

VOID _r_sys_setdefaultthreadenvironment (
	_Out_ PR_ENVIRONMENT environment
);

VOID _r_sys_setprocessenvironment (
	_In_ HANDLE process_handle,
	_In_ PR_ENVIRONMENT environment
);

VOID _r_sys_setthreadenvironment (
	_In_ HANDLE thread_handle,
	_In_ PR_ENVIRONMENT environment
);

_Success_ (return != 0)
EXECUTION_STATE _r_sys_setthreadexecutionstate (
	_In_ EXECUTION_STATE new_state
);

#if defined(APP_NO_DEPRECATIONS)
#define _r_sys_exitprocess RtlExitUserProcess
#else
#define _r_sys_exitprocess ExitProcess
#endif // APP_NO_DEPRECATIONS

FORCEINLINE HINSTANCE _r_sys_getimagebase ()
{
	return NtCurrentPeb ()->ImageBaseAddress;
}

FORCEINLINE LPCWSTR _r_sys_getimagepath ()
{
	return NtCurrentPeb ()->ProcessParameters->ImagePathName.Buffer;
}

FORCEINLINE LPCWSTR _r_sys_getimagecommandline ()
{
	return NtCurrentPeb ()->ProcessParameters->CommandLine.Buffer;
}

FORCEINLINE LPCWSTR _r_sys_getcurrentdirectory ()
{
	return NtCurrentPeb ()->ProcessParameters->CurrentDirectory.DosPath.Buffer;
}

FORCEINLINE ULONG _r_sys_getprocessorscount ()
{
	return NtCurrentPeb ()->NumberOfProcessors;
}

FORCEINLINE HANDLE _r_sys_getstdout ()
{
	return NtCurrentPeb ()->ProcessParameters->StandardOutput;
}

//
// Unixtime
//

LONG64 _r_unixtime_now ();

LONG64 _r_unixtime_from_filetime (
	_In_ const FILETIME * file_time
);

LONG64 _r_unixtime_from_systemtime (
	_In_ const SYSTEMTIME * system_time
);

VOID _r_unixtime_to_filetime (
	_In_ LONG64 unixtime,
	_Out_ PFILETIME file_time
);

_Success_ (return)
BOOLEAN _r_unixtime_to_systemtime (
	_In_ LONG64 unixtime,
	_Out_ PSYSTEMTIME system_time
);

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
);

_Ret_maybenull_
HBITMAP _r_dc_bitmapfromicon (
	_In_ HICON hicon,
	_In_ LONG x,
	_In_ LONG y
);

_Ret_maybenull_
HICON _r_dc_bitmaptoicon (
	_In_ HBITMAP hbitmap,
	_In_ LONG x,
	_In_ LONG y
);

_Ret_maybenull_
HBITMAP _r_dc_imagetobitmap (
	_In_ LPCGUID format,
	_In_ WICInProcPointer buffer,
	_In_ ULONG buffer_length,
	_In_ LONG x,
	_In_ LONG y
);

BOOLEAN _r_dc_drawwindow (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_ BOOLEAN is_drawfooter
);

BOOLEAN _r_dc_drawimagelisticon (
	_In_ HDC hdc,
	_In_ HIMAGELIST himglist,
	_In_ INT index,
	_In_ LONG x,
	_In_ LONG y,
	_In_opt_ ULONG state,
	_In_opt_ UINT style
);

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (
	_In_ UINT action,
	_In_ UINT param1,
	_Pre_maybenull_ _Post_valid_ PVOID param2,
	_In_ LONG dpi_value
);

VOID _r_dc_fillrect (
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ COLORREF clr
);

COLORREF _r_dc_getcoloraccent ();

COLORREF _r_dc_getcolorbrightness (
	_In_ COLORREF clr
);

COLORREF _r_dc_getcolorinverse (
	_In_ COLORREF clr
);

COLORREF _r_dc_getcolorshade (
	_In_ COLORREF clr,
	_In_ ULONG percent
);

LONG _r_dc_getdpi (
	_In_ LONG number,
	_In_ LONG dpi_value
);

LONG _r_dc_getwindowdpi (
	_In_ HWND hwnd
);

LONG _r_dc_getmonitordpi (
	_In_ LPCRECT rect
);

LONG _r_dc_getdpivalue (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCRECT rect
);

LONG _r_dc_gettaskbardpi ();

_Success_ (return != 0)
LONG _r_dc_getfontwidth (
	_In_ HDC hdc,
	_In_ PR_STRINGREF string
);

VOID _r_dc_getsizedpivalue (
	_Inout_ PR_SIZE size,
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_unpack
);

LONG _r_dc_getsystemmetrics (
	_In_ INT index,
	_In_ LONG dpi_value
);

VOID _r_dc_fixcontrolfont (
	_In_ HDC hdc,
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

VOID _r_dc_fixwindowfont (
	_In_ HDC hdc,
	_In_ HWND hwnd
);

LONG _r_dc_fontheighttosize (
	_In_ LONG height,
	_In_ LONG dpi_value
);

LONG _r_dc_fontsizetoheight (
	_In_ LONG size,
	_In_ LONG dpi_value
);

//
// File dialog
//

_Success_ (return)
BOOLEAN _r_filedialog_initialize (
	_Out_ PR_FILE_DIALOG file_dialog,
	_In_ ULONG flags
);

_Success_ (return)
BOOLEAN _r_filedialog_show (
	_In_opt_ HWND hwnd,
	_In_ PR_FILE_DIALOG file_dialog
);

PR_STRING _r_filedialog_getpath (
	_In_ PR_FILE_DIALOG file_dialog
);

VOID _r_filedialog_setpath (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ LPCWSTR path
);

VOID _r_filedialog_setfilter (
	_Inout_ PR_FILE_DIALOG file_dialog,
	_In_ COMDLG_FILTERSPEC * filters,
	_In_ ULONG count
);

VOID _r_filedialog_destroy (
	_In_ PR_FILE_DIALOG file_dialog
);

//
// Window layout
//

_Success_ (return)
BOOLEAN _r_layout_initializemanager (
	_Out_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd
);

VOID _r_layout_destroymanager (
	_Inout_ PR_LAYOUT_MANAGER layout_manager
);

_Ret_maybenull_
PR_LAYOUT_ITEM _r_layout_additem (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ PR_LAYOUT_ITEM parent_item,
	_In_ HWND hwnd,
	_In_ ULONG flags
);

VOID _r_layout_enumcontrols (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ PR_LAYOUT_ITEM layout_item,
	_In_ HWND hwnd
);

ULONG _r_layout_getcontrolflags (
	_In_ HWND hwnd
);

BOOLEAN _r_layout_resize (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ WPARAM wparam
);

VOID _r_layout_resizeitem (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_Inout_ PR_LAYOUT_ITEM layout_item
);

VOID _r_layout_resizeminimumsize (
	_In_ PR_LAYOUT_MANAGER layout_manager,
	_Inout_ LPARAM lparam
);

VOID _r_layout_setoriginalsize (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ LONG width,
	_In_ LONG height
);

VOID _r_layout_setitemanchor (
	_Inout_ PR_LAYOUT_ITEM layout_item
);

BOOLEAN _r_layout_setwindowanchor (
	_Inout_ PR_LAYOUT_MANAGER layout_manager,
	_In_ HWND hwnd,
	_In_ ULONG anchor
);

//
// Window management
//

VOID _r_wnd_addstyle (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ LONG_PTR mask,
	_In_ LONG_PTR state_mask,
	_In_ INT index
);

VOID _r_wnd_adjustrectangletobounds (
	_Inout_ PR_RECTANGLE rectangle,
	_In_ PR_RECTANGLE bounds
);

VOID _r_wnd_adjustrectangletoworkingarea (
	_Inout_ PR_RECTANGLE rectangle,
	_In_opt_ HWND hwnd
);

VOID _r_wnd_calculateoverlappedrect (
	_In_ HWND hwnd,
	_Inout_ PRECT window_rect
);

VOID _r_wnd_center (
	_In_ HWND hwnd,
	_In_opt_ HWND hparent
);

VOID _r_wnd_centerwindowrect (
	_In_ PR_RECTANGLE rectangle,
	_In_ PR_RECTANGLE bounds
);

VOID _r_wnd_changemessagefilter (
	_In_ HWND hwnd,
	_In_reads_ (count) PUINT messages,
	_In_ SIZE_T count,
	_In_ ULONG action
);

VOID _r_wnd_copyrectangle (
	_Out_ PR_RECTANGLE rectangle_dst,
	_In_ PR_RECTANGLE rectangle_src
);

_Ret_maybenull_
HWND _r_wnd_createwindow (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
);

INT_PTR _r_wnd_createmodalwindow (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_opt_ HWND hparent,
	_In_ DLGPROC dlg_proc,
	_In_opt_ PVOID lparam
);

_Success_ (return)
BOOLEAN _r_wnd_getclientsize (
	_In_ HWND hwnd,
	_Out_ PR_RECTANGLE rectangle
);

_Success_ (return)
BOOLEAN _r_wnd_getposition (
	_In_ HWND hwnd,
	_Out_ PR_RECTANGLE rectangle
);

LONG_PTR _r_wnd_getstyle (
	_In_ HWND hwnd
);

LONG_PTR _r_wnd_getstyle_ex (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isdesktop (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isdialog (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isfullscreenconsolemode (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isfullscreenusermode ();

BOOLEAN _r_wnd_isfullscreenwindowmode (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isfullscreenmode ();

BOOLEAN _r_wnd_ismaximized (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_ismenu (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isminimized (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isoverlapped (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isundercursor (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isvisible (
	_In_ HWND hwnd
);

BOOLEAN _r_wnd_isvisible_ex (
	_In_ HWND hwnd
);

ULONG CALLBACK _r_wnd_message_callback (
	_In_ HWND main_wnd,
	_In_ LPCWSTR accelerator_table
);

VOID CALLBACK _r_wnd_message_dpichanged (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
);

VOID CALLBACK _r_wnd_message_settingchange (
	_In_ HWND hwnd,
	_In_opt_ WPARAM wparam,
	_In_opt_ LPARAM lparam
);

VOID _r_wnd_rectangletorect (
	_Out_ PRECT rect,
	_In_ PR_RECTANGLE rectangle
);

VOID _r_wnd_recttorectangle (
	_Out_ PR_RECTANGLE rectangle,
	_In_ LPCRECT rect
);

VOID _r_wnd_seticon (
	_In_ HWND hwnd,
	_In_opt_ HICON hicon_small,
	_In_opt_ HICON hicon_big
);

VOID _r_wnd_setrectangle (
	_Out_ PR_RECTANGLE rectangle,
	_In_ LONG left,
	_In_ LONG top,
	_In_ LONG width,
	_In_ LONG height
);

VOID _r_wnd_setposition (
	_In_ HWND hwnd,
	_In_opt_ PR_SIZE position,
	_In_opt_ PR_SIZE size
);

VOID _r_wnd_setstyle (
	_In_ HWND hwnd,
	_In_ LONG_PTR style
);

VOID _r_wnd_setstyle_ex (
	_In_ HWND hwnd,
	_In_ LONG_PTR ex_style
);

VOID _r_wnd_toggle (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_show
);

VOID _r_wnd_top (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
);

ULONG _r_wnd_getcontext_hash (
	_In_ HWND hwnd,
	_In_ ULONG property_id
);

_Ret_maybenull_
PVOID _r_wnd_getcontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id
);

VOID _r_wnd_setcontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id,
	_In_ PVOID context
);

VOID _r_wnd_removecontext (
	_In_ HWND hwnd,
	_In_ ULONG property_id
);

//
// Inernet access (WinHTTP)
//

_Ret_maybenull_
HINTERNET _r_inet_createsession (
	_In_opt_ PR_STRING useragent
);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (
	_In_ HINTERNET hsession,
	_In_ PR_STRING url,
	_Out_ LPHINTERNET hconnect_ptr,
	_Out_ LPHINTERNET hrequest_ptr,
	_Out_opt_ PULONG total_length_ptr
);

_Success_ (return)
BOOLEAN _r_inet_readrequest (
	_In_ HINTERNET hrequest,
	_Out_writes_bytes_ (buffer_size) PVOID buffer,
	_In_ ULONG buffer_size,
	_Out_ PULONG readed_ptr,
	_Inout_opt_ PULONG total_readed_ptr
);

ULONG _r_inet_querycontentlength (
	_In_ HINTERNET hrequest
);

LONG64 _r_inet_querylastmodified (
	_In_ HINTERNET hrequest
);
ULONG _r_inet_querystatuscode (
	_In_ HINTERNET hrequest
);

VOID _r_inet_initializedownload (
	_Out_ PR_DOWNLOAD_INFO download_info
);

VOID _r_inet_initializedownload_ex (
	_Out_ PR_DOWNLOAD_INFO download_info,
	_In_opt_ HANDLE hfile,
	_In_opt_ PR_INET_DOWNLOAD_CALLBACK download_callback,
	_In_opt_ PVOID lparam
);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_begindownload (
	_In_ HINTERNET hsession,
	_In_ PR_STRING url,
	_Inout_ PR_DOWNLOAD_INFO download_info
);

VOID _r_inet_destroydownload (
	_Inout_ PR_DOWNLOAD_INFO download_info
);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_queryurlparts (
	_In_ PR_STRING url,
	_In_ ULONG flags,
	_Out_ PR_URLPARTS url_parts
);

VOID _r_inet_destroyurlparts (
	_Inout_ PR_URLPARTS url_parts
);

#define _r_inet_close WinHttpCloseHandle

//
// Registry
//

_Ret_maybenull_
PR_BYTE _r_reg_querybinary (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
);

_Ret_maybenull_
PR_STRING _r_reg_querystring (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name
);

ULONG _r_reg_queryulong (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
);

ULONG64 _r_reg_queryulong64 (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_ LPCWSTR value_name
);

ULONG _r_reg_querysubkeylength (
	_In_ HKEY hkey
);

LONG64 _r_reg_querytimestamp (
	_In_ HKEY hkey
);

LSTATUS _r_reg_queryvalue (
	_In_ HKEY hkey,
	_In_opt_ LPCWSTR subkey,
	_In_opt_ LPCWSTR value_name,
	_Out_opt_ PULONG type,
	_Out_writes_bytes_to_opt_ (*buffer_length, *buffer_length) PBYTE buffer,
	_When_ (buffer == NULL, _Out_opt_) _When_ (buffer != NULL, _Inout_opt_) PULONG buffer_length
);

//
// Cryptography
//

VOID _r_crypt_initialize (
	_Out_ PR_CRYPT_CONTEXT crypt_context,
	_In_ BOOLEAN is_hashing
);

VOID _r_crypt_destroycryptcontext (
	_In_ PR_CRYPT_CONTEXT context
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_createcryptcontext (
	_Out_ PR_CRYPT_CONTEXT crypt_context,
	_In_ LPCWSTR algorithm_id
);

PR_BYTE _r_crypt_getkeyblock (
	_Inout_ PR_CRYPT_CONTEXT crypt_context
);

PR_BYTE _r_crypt_getivblock (
	_Inout_ PR_CRYPT_CONTEXT crypt_context
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_generatekey (
	_Inout_ PR_CRYPT_CONTEXT crypt_context,
	_In_ PR_BYTEREF key
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_encryptbuffer (
	_In_ PR_CRYPT_CONTEXT crypt_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_decryptbuffer (
	_In_ PR_CRYPT_CONTEXT crypt_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_createhashcontext (
	_Out_ PR_CRYPT_CONTEXT hash_context,
	_In_ LPCWSTR algorithm_id
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_hashbuffer (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_In_reads_bytes_ (buffer_length) PVOID buffer,
	_In_ ULONG buffer_length
);

_Ret_maybenull_
PR_STRING _r_crypt_finalhashcontext (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_In_ BOOLEAN is_uppercase
);

_Success_ (return == STATUS_SUCCESS)
NTSTATUS _r_crypt_finalhashcontext_ex (
	_In_ PR_CRYPT_CONTEXT hash_context,
	_Out_ PR_BYTE_PTR out_buffer
);

//
// Math
//

ULONG _r_math_exponentiate (
	_In_ ULONG base,
	_In_ ULONG exponent
);

ULONG64 _r_math_exponentiate64 (
	_In_ ULONG64 base,
	_In_ ULONG exponent
);

ULONG _r_math_getrandom ();

ULONG _r_math_getrandomrange (
	_In_ ULONG min_number,
	_In_ ULONG max_number
);

ULONG _r_math_hashinteger32 (
	_In_ ULONG value
);

ULONG _r_math_hashinteger64 (
	_In_ ULONG64 value
);

SIZE_T _r_math_rounduptopoweroftwo (
	_In_ SIZE_T number
);

_Success_ (return == ERROR_SUCCESS)
FORCEINLINE LONG _r_math_createguid (
	_Out_ LPGUID guid
)
{
	return UuidCreate (guid);
}

#ifdef _WIN64
#define _r_math_hashinteger_ptr _r_math_hashinteger64
#else
#define _r_math_hashinteger_ptr _r_math_hashinteger32
#endif

//
// Resources
//

BOOLEAN _r_res_loadresource (
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR name,
	_In_ LPCWSTR type,
	_Out_ PR_BYTEREF out_buffer
);

_Ret_maybenull_
PR_STRING _r_res_loadstring (
	_In_opt_ HINSTANCE hinstance,
	_In_ UINT string_id
);

_Ret_maybenull_
PR_STRING _r_res_querystring (
	_In_ LPCVOID ver_block,
	_In_ LPCWSTR entry_name,
	_In_ ULONG lcid
);

_Ret_maybenull_
PR_STRING _r_res_querystring_ex (
	_In_ LPCVOID ver_block,
	_In_ LPCWSTR entry_name,
	_In_ ULONG lcid
);

ULONG _r_res_querytranslation (
	_In_ LPCVOID ver_block
);

_Success_ (return)
BOOLEAN _r_res_queryversion (
	_In_ LPCVOID ver_block,
	_Out_ PVOID_PTR file_info
);

_Ret_maybenull_
PR_STRING _r_res_queryversionstring (
	_In_ LPCWSTR path
);

//
// Other
//

PR_HASHTABLE _r_parseini (
	_In_ PR_STRING path,
	_Inout_opt_ PR_LIST section_list
);

//
// Xml library
//

_Success_ (return == S_OK)
HRESULT _r_xml_initializelibrary (
	_Out_ PR_XML_LIBRARY xml_library,
	_In_ BOOLEAN is_reader
);

VOID _r_xml_destroylibrary (
	_Inout_ PR_XML_LIBRARY xml_library
);

_Success_ (return == S_OK)
HRESULT _r_xml_createfilestream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path,
	_In_ ULONG mode,
	_In_ BOOL is_create
);

_Success_ (return == S_OK)
HRESULT _r_xml_createstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_opt_ LPCVOID buffer,
	_In_ ULONG buffer_length
);

_Success_ (return == S_OK)
HRESULT _r_xml_parsefile (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR file_path
);

_Success_ (return == S_OK)
HRESULT _r_xml_parsestring (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCVOID buffer,
	_In_ ULONG buffer_length
);

_Success_ (return == S_OK)
HRESULT _r_xml_readstream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (return)
BOOLEAN _r_xml_getattribute (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_Out_ PR_STRINGREF value
);

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
);

BOOLEAN _r_xml_getattribute_boolean (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
);

LONG _r_xml_getattribute_long (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
);

LONG64 _r_xml_getattribute_long64 (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name
);

VOID _r_xml_setattribute_long (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_ LONG value
);

VOID _r_xml_setattribute_long64 (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_ LONG64 value
);

FORCEINLINE VOID _r_xml_setattribute (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR attrib_name,
	_In_opt_ LPCWSTR value
)
{
	IXmlWriter_WriteAttributeString (xml_library->writer, NULL, attrib_name, NULL, value);
}

FORCEINLINE VOID _r_xml_setattribute_boolean (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR name,
	_In_ BOOLEAN value
)
{
	_r_xml_setattribute (xml_library, name, value ? L"true" : L"false");
}

_Success_ (return)
BOOLEAN _r_xml_enumchilditemsbytagname (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR tag_name
);

_Success_ (return)
BOOLEAN _r_xml_findchildbytagname (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR tag_name
);

_Success_ (return == S_OK)
HRESULT _r_xml_resetlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library
);

_Success_ (return == S_OK)
HRESULT _r_xml_setlibrarystream (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ PR_XML_STREAM stream
);

FORCEINLINE VOID _r_xml_writewhitespace (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR whitespace
)
{
	IXmlWriter_WriteWhitespace (xml_library->writer, whitespace);
}

FORCEINLINE VOID _r_xml_writestartdocument (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteStartDocument (xml_library->writer, XmlStandalone_Omit);
}

FORCEINLINE VOID _r_xml_writeenddocument (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteEndDocument (xml_library->writer);

	IXmlWriter_Flush (xml_library->writer);
}

FORCEINLINE VOID _r_xml_writestartelement (
	_Inout_ PR_XML_LIBRARY xml_library,
	_In_ LPCWSTR name
)
{
	IXmlWriter_WriteStartElement (xml_library->writer, NULL, name, NULL);
}

FORCEINLINE VOID _r_xml_writeendelement (
	_Inout_ PR_XML_LIBRARY xml_library
)
{
	IXmlWriter_WriteEndElement (xml_library->writer);
}

//
// System tray
//

VOID _r_tray_initialize (
	_Inout_ PNOTIFYICONDATA nid,
	_In_ HWND hwnd,
	_In_ LPCGUID guid
);

VOID _r_tray_setversion (
	_Inout_ PNOTIFYICONDATA nid
);

BOOLEAN _r_tray_create (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ UINT code,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip,
	_In_ BOOLEAN is_hidden
);

BOOLEAN _r_tray_destroy (
	_In_ HWND hwnd,
	_In_ LPCGUID guid
);

BOOLEAN _r_tray_popup (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG icon_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR text
);

BOOLEAN _r_tray_popupformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ ULONG icon_id,
	_In_opt_ LPCWSTR title,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

BOOLEAN _r_tray_setinfo (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_opt_ LPCWSTR tooltip
);

BOOLEAN _r_tray_setinfoformat (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_opt_ HICON hicon,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

BOOLEAN _r_tray_toggle (
	_In_ HWND hwnd,
	_In_ LPCGUID guid,
	_In_ BOOLEAN is_show
);

//
// Control: common
//

BOOLEAN _r_ctrl_isenabled (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

INT _r_ctrl_isradiobuttonchecked (
	_In_ HWND hwnd,
	_In_ INT start_id,
	_In_ INT end_id
);

VOID _r_ctrl_enable (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id,
	_In_ BOOLEAN is_enable
);

LONG64 _r_ctrl_getinteger (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_Out_opt_ PULONG base_ptr
);

_Ret_maybenull_
PR_STRING _r_ctrl_getstring (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

LONG _r_ctrl_getwidth (
	_In_ HWND hwnd,
	_In_opt_ INT ctrl_id
);

VOID _r_ctrl_setbuttonmargins (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ LONG dpi_value
);

VOID _r_ctrl_settablestring (
	_In_ HWND hwnd,
	_In_ INT ctrl_id1,
	_In_ PR_STRINGREF text1,
	_In_ INT ctrl_id2,
	_In_ PR_STRINGREF text2
);

VOID _r_ctrl_setstringformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_ctrl_setstringlength (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ PR_STRINGREF string
);

_Ret_maybenull_
HWND _r_ctrl_createtip (
	_In_opt_ HWND hparent
);

VOID _r_ctrl_settiptext (
	_In_ HWND htip,
	_In_ HWND hparent,
	_In_ INT ctrl_id,
	_In_ LPCWSTR text
);

VOID _r_ctrl_settiptextformat (
	_In_ HWND htip,
	_In_ HWND hparent,
	_In_ INT ctrl_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

VOID _r_ctrl_settipstyle (
	_In_ HWND htip
);

VOID _r_ctrl_showballoontip (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT icon_id,
	_In_opt_ LPCWSTR title,
	_In_ LPCWSTR text
);

VOID _r_ctrl_showballoontipformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT icon_id,
	_In_opt_ LPCWSTR title,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

FORCEINLINE ULONG _r_ctrl_getstringlength (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);
}

FORCEINLINE VOID _r_ctrl_setstring (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ LPCWSTR text
)
{
	SetDlgItemText (hwnd, ctrl_id, text);
}

//
// Control: combobox
//

FORCEINLINE VOID _r_combobox_clear (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_RESETCONTENT, 0, 0);
}

FORCEINLINE INT _r_combobox_getcurrentitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0);
}

FORCEINLINE LPARAM _r_combobox_getitemparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	return SendDlgItemMessage (hwnd, ctrl_id, CB_GETITEMDATA, (WPARAM)item_id, 0);
}

FORCEINLINE VOID _r_combobox_insertitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ LPCWSTR string
)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, (WPARAM)item_id, (LPARAM)string);
}

FORCEINLINE VOID _r_combobox_setcurrentitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, (WPARAM)item_id, 0);
}

FORCEINLINE VOID _r_combobox_setitemparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ LPARAM lparam
)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_SETITEMDATA, (WPARAM)item_id, lparam);
}

//
// Control: editbox
//

FORCEINLINE VOID _r_edit_setcuebanner (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ LPCWSTR text
)
{
	SendDlgItemMessage (hwnd, ctrl_id, EM_SETCUEBANNER, FALSE, (LPARAM)text);
}

FORCEINLINE VOID _r_edit_setselection (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT selection
)
{
	SendDlgItemMessage (hwnd, ctrl_id, EM_SETSEL, 0, (LPARAM)selection);
}

FORCEINLINE VOID _r_edit_settextlimit (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ ULONG length
)
{
	SendDlgItemMessage (hwnd, ctrl_id, EM_LIMITTEXT, (WPARAM)length, 0);
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
);

VOID _r_menu_clearitems (
	_In_ HMENU hmenu
);

VOID _r_menu_setitembitmap (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ HBITMAP hbitmap
);

VOID _r_menu_setitemtext (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ LPCWSTR text
);

VOID _r_menu_setitemtextformat (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ BOOL is_byposition,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

INT _r_menu_popup (
	_In_ HMENU hmenu,
	_In_ HWND hwnd,
	_In_opt_ PPOINT point,
	_In_ BOOLEAN is_sendmessage
);

FORCEINLINE VOID _r_menu_enableitem (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ UINT position_flag,
	_In_ BOOLEAN is_enable
)
{
	EnableMenuItem (hmenu, item_id, position_flag | (is_enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

//
// Control: tab
//

VOID _r_tab_adjustchild (
	_In_ HWND hwnd,
	_In_ INT tab_id,
	_In_ HWND hchild
);

INT _r_tab_additem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
);

LPARAM _r_tab_getitemlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

INT _r_tab_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
);

VOID _r_tab_selectitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

FORCEINLINE INT _r_tab_getcurrentitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETCURSEL, 0, 0);
}

FORCEINLINE INT _r_tab_getitemcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEMCOUNT, 0, 0);
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
);

INT _r_listview_addgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ UINT align,
	_In_opt_ UINT state,
	_In_opt_ UINT state_mask
);

INT _r_listview_additem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ LPCWSTR text
);

INT _r_listview_additem_ex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_opt_ LPARAM lparam
);

_Success_ (return != -1)
INT _r_listview_finditem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT start_pos,
	_In_ LPARAM lparam
);

VOID _r_listview_deleteallcolumns (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

INT _r_listview_getcolumncount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

_Ret_maybenull_
PR_STRING _r_listview_getcolumntext (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id
);

INT _r_listview_getcolumnwidth (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id
);

INT _r_listview_getitemcheckedcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

INT _r_listview_getitemgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

LPARAM _r_listview_getitemlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

_Ret_maybenull_
PR_STRING _r_listview_getitemtext (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id
);

VOID _r_listview_redraw (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

VOID _r_listview_setcolumn (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id,
	_In_opt_ LPCWSTR text,
	_In_opt_ INT width
);

VOID _r_listview_setcolumnsortindex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id,
	_In_ INT arrow
);

VOID _r_listview_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id,
	_In_opt_ LPCWSTR text
);

VOID _r_listview_setitem_ex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ INT subitem_id,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_ INT group_id,
	_In_opt_ LPARAM lparam
);

VOID _r_listview_setitemcheck (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ BOOLEAN is_check
);

VOID _r_listview_setitemstate (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id,
	_In_ UINT state,
	_In_opt_ UINT state_mask
);

VOID _r_listview_setitemvisible (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
);

VOID _r_listview_setgroup (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT group_id,
	_In_opt_ LPCWSTR title,
	_In_opt_ UINT state,
	_In_opt_ UINT state_mask
);

VOID _r_listview_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_ BOOL is_groupview
);

FORCEINLINE VOID _r_listview_deleteallgroups (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteallitems (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEITEM, (WPARAM)item_id, 0);
}

FORCEINLINE VOID _r_listview_ensurevisible (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_ENSUREVISIBLE, (WPARAM)item_id, FALSE);
}

FORCEINLINE ULONG _r_listview_getstyle_ex (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
}

FORCEINLINE INT _r_listview_getitemcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, 0);
}

FORCEINLINE INT _r_listview_getselectedcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0);
}

FORCEINLINE INT _r_listview_getnextselected (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETNEXTITEM, (WPARAM)item_id, (LPARAM)LVNI_SELECTED);
}

FORCEINLINE ULONG _r_listview_getview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETVIEW, 0, 0);
}

FORCEINLINE BOOLEAN _r_listview_isgroupviewenabled (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_ISGROUPVIEWENABLED, 0, 0));
}

FORCEINLINE BOOLEAN _r_listview_isitemchecked (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	LONG state;

	state = (LONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item_id, (LPARAM)LVIS_STATEIMAGEMASK);

	return (state == INDEXTOSTATEIMAGEMASK (2));
}

FORCEINLINE BOOLEAN _r_listview_isitemselected (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	LONG state;

	state = (LONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item_id, (LPARAM)LVNI_SELECTED);

	return (state == LVNI_SELECTED);
}

FORCEINLINE BOOLEAN _r_listview_isitemvisible (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT item_id
)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, (WPARAM)item_id, 0));
}

FORCEINLINE VOID _r_listview_setimagelist (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HIMAGELIST himg
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
}

FORCEINLINE VOID _r_listview_setview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ LONG view_type
)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETVIEW, (WPARAM)view_type, 0);
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
);

LPARAM _r_treeview_getlparam (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HTREEITEM hitem
);

VOID _r_treeview_setitem (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HTREEITEM hitem,
	_In_opt_ LPCWSTR text,
	_In_ INT image_id,
	_In_opt_ LPARAM lparam
);

VOID _r_treeview_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style,
	_In_opt_ INT height,
	_In_opt_ INT indent
);

//
// Control: statusbar
//

LONG _r_status_getheight (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

VOID _r_status_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ INT height
);

VOID _r_status_settextformat (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT part_id,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
);

FORCEINLINE VOID _r_status_setparts (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_reads_ (count) PINT parts,
	_In_ INT count
)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETPARTS, (WPARAM)count, (LPARAM)parts);
}

FORCEINLINE VOID _r_status_settext (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT part_id,
	_In_opt_ LPCWSTR text
)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part_id, 0), (LPARAM)text);
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
);

VOID _r_rebar_deleteband (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT band_id
);

FORCEINLINE UINT _r_rebar_getcount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (UINT)SendDlgItemMessage (hwnd, ctrl_id, RB_GETBANDCOUNT, 0, 0);
}

FORCEINLINE LONG _r_rebar_getheight (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (LONG)SendDlgItemMessage (hwnd, ctrl_id, RB_GETBARHEIGHT, 0, 0);
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
);

VOID _r_toolbar_addseparator (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

INT _r_toolbar_getwidth (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

VOID _r_toolbar_setbutton (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT command_id,
	_In_opt_ LPCWSTR text,
	_In_opt_ INT style,
	_In_opt_ INT state,
	_In_ INT image);

VOID _r_toolbar_setstyle (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_opt_ ULONG ex_style
);

FORCEINLINE VOID _r_toolbar_enablebutton (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT command_id,
	_In_ BOOLEAN is_enable
)
{
	SendDlgItemMessage (hwnd, ctrl_id, TB_ENABLEBUTTON, (WPARAM)command_id, MAKELPARAM (is_enable, 0));
}

FORCEINLINE INT _r_toolbar_getbuttoncount (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONCOUNT, 0, 0);
}

FORCEINLINE ULONG _r_toolbar_getbuttonsize (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, TB_GETBUTTONSIZE, 0, 0);
}

FORCEINLINE BOOLEAN _r_toolbar_isbuttonenabled (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ UINT command_id
)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, TB_ISBUTTONENABLED, (WPARAM)command_id, 0));
}

FORCEINLINE VOID _r_toolbar_resize (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	SendDlgItemMessage (hwnd, ctrl_id, TB_AUTOSIZE, 0, 0);
}

//
// Control: progress bar
//

VOID _r_progress_setmarquee (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ BOOL is_enable
);

//
// Util
//

BOOL CALLBACK _r_util_activate_window_callback (
	_In_ HWND hwnd,
	_In_ LPARAM lparam
);

VOID _r_util_templatewritecontrol (
	_Inout_ PBYTE_PTR ptr,
	_In_ ULONG ctrl_id,
	_In_ ULONG style,
	_In_ SHORT x,
	_In_ SHORT y,
	_In_ SHORT cx,
	_In_ SHORT cy,
	_In_ LPCWSTR class_name
);

VOID _r_util_templatewriteshort (
	_Inout_ PBYTE_PTR ptr,
	_In_ WORD data
);

VOID _r_util_templatewritestring (
	_Inout_ PBYTE_PTR ptr,
	_In_ LPCWSTR string
);

VOID _r_util_templatewriteulong (
	_Inout_ PBYTE_PTR ptr,
	_In_ ULONG data
);

VOID _r_util_templatewrite_ex (
	_Inout_ PBYTE_PTR ptr,
	_In_bytecount_ (size) LPCVOID data,
	_In_ SIZE_T size
);

//
// Dereference procedures
//

VOID NTAPI _r_util_cleanarray_callback (
	_In_ PVOID entry
);

VOID NTAPI _r_util_cleanlist_callback (
	_In_ PVOID entry
);

VOID NTAPI _r_util_cleanhashtable_callback (
	_In_ PVOID entry
);

VOID NTAPI _r_util_cleanhashtablepointer_callback (
	_In_ PVOID entry
);
