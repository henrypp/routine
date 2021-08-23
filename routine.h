// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#pragma once

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

#if !defined(CINTERFACE)
#define CINTERFACE
#endif // !CINTERFACE

#if !defined(COBJMACROS)
#define COBJMACROS
#endif // !COBJMACROS

#if !defined(UMDF_USING_NTSTATUS)
#define UMDF_USING_NTSTATUS
#endif // !UMDF_USING_NTSTATUS

#if !defined(_UCRT_DISABLED_WARNINGS)
#define _UCRT_DISABLED_WARNINGS 4996 //_CRT_SECURE_NO_WARNINGS
#endif // !_UCRT_DISABLED_WARNINGS

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
#include <winhttp.h>
#include <wtsapi32.h>
#include <xmllite.h>

#if !defined(_ARM64_)
#include <smmintrin.h>
#endif

#include "app.h"
#include "ntapi.h"
#include "rconfig.h"
#include "rtypes.h"
#include "rlist.h"

// libs
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
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

// extern
extern FORCEINLINE SIZE_T _r_str_getlength (_In_ LPCWSTR string);
extern FORCEINLINE SIZE_T _r_str_getbytelength (_In_ LPCSTR string);
extern ULONG _r_str_crc32 (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
extern ULONG64 _r_str_crc64 (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
extern ULONG _r_str_fnv32a (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
extern ULONG64 _r_str_fnv64a (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
extern R_TOKEN_ATTRIBUTES _r_sys_getcurrenttoken ();

// safe clenup memory
#ifndef SAFE_DELETE_MEMORY
#define SAFE_DELETE_MEMORY(p) {if(p) {_r_mem_free (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_REFERENCE
#define SAFE_DELETE_REFERENCE(p) {if(p) {_r_obj_clearreference (&(p));}}
#endif

#ifndef SAFE_DELETE_DC
#define SAFE_DELETE_DC(p) {if(p) {DeleteDC (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_OBJECT
#define SAFE_DELETE_OBJECT(p) {if(p) {DeleteObject (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_HANDLE
#define SAFE_DELETE_HANDLE(p) {if(_r_fs_isvalidhandle ((p))) {CloseHandle ((p)); (p)=NULL;}}
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
// Dereference procedures
//

VOID NTAPI _r_util_dereferencearrayprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencelistprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencehashtableprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencehashtableptrprocedure (_In_ PVOID entry);

//
// Synchronization
//

FORCEINLINE BOOLEAN _InterlockedBitTestAndSetPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *base, _In_ LONG_PTR bit)
{
#ifdef _WIN64
	return _interlockedbittestandset64 ((PLONG64)base, (LONG64)bit);
#else
	return _interlockedbittestandset ((PLONG)base, (LONG)bit);
#endif
}

FORCEINLINE LONG_PTR _InterlockedExchangeAddPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend, _In_ LONG_PTR value)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedExchangeAdd64 ((PLONG64)addend, (LONG64)value);
#else
	return (LONG_PTR)_InterlockedExchangeAdd ((PLONG)addend, (LONG)value);
#endif
}

FORCEINLINE LONG_PTR _InterlockedIncrementPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedIncrement64 ((PLONG64)addend);
#else
	return (LONG_PTR)_InterlockedIncrement ((PLONG)addend);
#endif
}

FORCEINLINE LONG_PTR _InterlockedDecrementPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend)
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

VOID _r_autopool_initialize (_Out_ PR_AUTO_POOL auto_pool);
VOID _r_autopool_delete (_Inout_ PR_AUTO_POOL auto_pool);

//
// Synchronization: Event
//

VOID FASTCALL _r_event_set (_Inout_ PR_EVENT event_object);
VOID FASTCALL _r_event_reset (_Inout_ PR_EVENT event_object);
BOOLEAN FASTCALL _r_event_wait_ex (_Inout_ PR_EVENT event_object, _In_opt_ PLARGE_INTEGER timeout);

FORCEINLINE VOID FASTCALL _r_event_intialize (_Out_ PR_EVENT event_object)
{
	event_object->value = PR_EVENT_REFCOUNT_INC;
	event_object->event_handle = NULL;
}

FORCEINLINE BOOLEAN _r_event_test (_In_ PR_EVENT event_object)
{
	return !!event_object->is_set;
}

FORCEINLINE VOID _r_event_reference (_Inout_ PR_EVENT event_object)
{
	_InterlockedExchangeAddPointer ((PLONG_PTR)(&event_object->value), PR_EVENT_REFCOUNT_INC);
}

FORCEINLINE VOID _r_event_dereference (_Inout_ PR_EVENT event_object, _In_opt_ HANDLE event_handle)
{
	ULONG_PTR value;

	value = _InterlockedExchangeAddPointer ((PLONG_PTR)(&event_object->value), -PR_EVENT_REFCOUNT_INC);

	if (((value >> PR_EVENT_REFCOUNT_SHIFT) & PR_EVENT_REFCOUNT_MASK) - 1 == 0)
	{
		if (event_handle)
		{
			NtClose (event_handle);

			event_object->event_handle = NULL;
		}
	}
}

FORCEINLINE BOOLEAN _r_event_wait (_Inout_ PR_EVENT event_object, _In_opt_ PLARGE_INTEGER timeout)
{
	if (_r_event_test (event_object))
		return TRUE;

	return _r_event_wait_ex (event_object, timeout);
}

//
// Synchronization: One-time initialization
//

#if defined(APP_NO_DEPRECATIONS)

FORCEINLINE BOOLEAN _r_initonce_begin (_Inout_ PR_INITONCE init_once)
{
	if (NT_SUCCESS (RtlRunOnceBeginInitialize (init_once, RTL_RUN_ONCE_CHECK_ONLY, NULL)))
		return FALSE;

	return (RtlRunOnceBeginInitialize (init_once, 0, NULL) == STATUS_PENDING);
}

FORCEINLINE VOID _r_initonce_end (_Inout_ PR_INITONCE init_once)
{
	RtlRunOnceComplete (init_once, 0, NULL);
}

#else

BOOLEAN FASTCALL _r_initonce_begin_ex (_Inout_ PR_INITONCE init_once);

FORCEINLINE BOOLEAN _r_initonce_begin (_Inout_ PR_INITONCE init_once)
{
	if (_r_event_test (&init_once->event_object))
		return FALSE;

	return _r_initonce_begin_ex (init_once);
}

FORCEINLINE VOID FASTCALL _r_initonce_end (_Inout_ PR_INITONCE init_once)
{
	_r_event_set (&init_once->event_object);
}
#endif // APP_NO_DEPRECATIONS

//
// Synchronization: Free list
//

VOID _r_freelist_initialize (_Out_ PR_FREE_LIST free_list, _In_ SIZE_T size, _In_ ULONG maximum_count);
PVOID _r_freelist_allocateitem (_Inout_ PR_FREE_LIST free_list);
VOID _r_freelist_destroy (_Inout_ PR_FREE_LIST free_list);
VOID _r_freelist_deleteitem (_Inout_ PR_FREE_LIST free_list, _In_ PVOID memory);

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

FORCEINLINE VOID _r_queuedlock_initialize (_Out_ PR_QUEUED_LOCK queued_lock)
{
	queued_lock->value = 0;
}

VOID FASTCALL _r_queuedlock_acquireexclusive_ex (_Inout_ PR_QUEUED_LOCK queued_lock);
VOID FASTCALL _r_queuedlock_acquireshared_ex (_Inout_ PR_QUEUED_LOCK queued_lock);

VOID FASTCALL _r_queuedlock_releaseexclusive_ex (_Inout_ PR_QUEUED_LOCK queued_lock);
VOID FASTCALL _r_queuedlock_releaseshared_ex (_Inout_ PR_QUEUED_LOCK queued_lock);

VOID FASTCALL _r_queuedlock_optimizelist (_Inout_ PR_QUEUED_LOCK queued_lock, _In_ ULONG_PTR value);

VOID FASTCALL _r_queuedlock_wake (_Inout_ PR_QUEUED_LOCK queued_lock, _In_ ULONG_PTR value);
VOID FASTCALL _r_queuedlock_wake_ex (_Inout_ PR_QUEUED_LOCK queued_lock, _In_ ULONG_PTR value, _In_ BOOLEAN is_ignoreowned, _In_ BOOLEAN is_wakeall);
VOID FASTCALL _r_queuedlock_wakeforrelease (_Inout_ PR_QUEUED_LOCK queued_lock, _In_ ULONG_PTR value);

_Acquires_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_acquireexclusive (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	if (_InterlockedBitTestAndSetPointer ((PLONG_PTR)&queued_lock->value, PR_QUEUED_LOCK_OWNED_SHIFT))
	{
		// Owned bit was already set. Slow path.
		_r_queuedlock_acquireexclusive_ex (queued_lock);
	}
}

_Acquires_shared_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_acquireshared (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	ULONG_PTR value;

	value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, IntToPtr (PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_SHARED_INC), IntToPtr (0));

	if (value != 0)
	{
		_r_queuedlock_acquireshared_ex (queued_lock);
	}
}

_Releases_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_releaseexclusive (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	ULONG_PTR value;

	value = (ULONG_PTR)_InterlockedExchangeAddPointer ((PLONG_PTR)&queued_lock->value, -(LONG_PTR)PR_QUEUED_LOCK_OWNED);

	if ((value & (PR_QUEUED_LOCK_WAITERS | PR_QUEUED_LOCK_TRAVERSING)) == PR_QUEUED_LOCK_WAITERS)
	{
		_r_queuedlock_wakeforrelease (queued_lock, value - PR_QUEUED_LOCK_OWNED);
	}
}

_Releases_exclusive_lock_ (*queued_lock)
FORCEINLINE VOID _r_queuedlock_releaseshared (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_SHARED_INC;

	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&queued_lock->value, IntToPtr (0), (PVOID)value);

	if (new_value != value)
	{
		_r_queuedlock_releaseshared_ex (queued_lock);
	}
}

_When_ (return != FALSE, _Acquires_exclusive_lock_ (*queued_lock))
FORCEINLINE BOOLEAN _r_queuedlock_tryacquireexclusive (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	if (!_InterlockedBitTestAndSetPointer ((PLONG_PTR)&queued_lock->value, PR_QUEUED_LOCK_OWNED_SHIFT))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

_When_ (return != FALSE, _Acquires_shared_lock_ (*queued_lock))
FORCEINLINE BOOLEAN _r_queuedlock_tryacquireshared (_Inout_ PR_QUEUED_LOCK queued_lock)
{
	if (!_InterlockedBitTestAndSetPointer ((PLONG_PTR)&queued_lock->value, PR_QUEUED_LOCK_OWNED | PR_QUEUED_LOCK_SHARED_INC))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

FORCEINLINE BOOLEAN _r_queuedlock_islocked (_In_ const PR_QUEUED_LOCK spin_lock)
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

FORCEINLINE VOID _r_condition_initialize (_Out_ PR_CONDITION condition)
{
	condition->value = 0;
}

VOID FASTCALL _r_condition_pulse (_Inout_ PR_CONDITION condition);
VOID FASTCALL _r_condition_waitfor (_Inout_ PR_CONDITION condition, _Inout_ PR_QUEUED_LOCK queued_lock, _In_opt_ PLARGE_INTEGER timeout);

//
// Synchronization: Rundown protection
//

FORCEINLINE VOID _r_protection_initialize (_Out_ PR_RUNDOWN_PROTECT protection)
{
	protection->value = 0;
}

BOOLEAN FASTCALL _r_protection_acquire_ex (_Inout_ PR_RUNDOWN_PROTECT protection);
VOID FASTCALL _r_protection_release_ex (_Inout_ PR_RUNDOWN_PROTECT protection);
VOID FASTCALL _r_protection_waitfor_ex (_Inout_ PR_RUNDOWN_PROTECT protection);

FORCEINLINE BOOLEAN _r_protection_acquire (_Inout_ PR_RUNDOWN_PROTECT protection)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = protection->value & ~PR_RUNDOWN_ACTIVE; // fail fast path when rundown is active

	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, (PVOID)(value + PR_RUNDOWN_REF_INC), (PVOID)value);

	if (new_value == value)
	{
		return TRUE;
	}
	else
	{
		return _r_protection_acquire_ex (protection);
	}
}

FORCEINLINE VOID _r_protection_release (_Inout_ PR_RUNDOWN_PROTECT protection)
{
	ULONG_PTR value;
	ULONG_PTR new_value;

	value = protection->value & ~PR_RUNDOWN_ACTIVE; // Fail fast path when rundown is active
	new_value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, (PVOID)(value - PR_RUNDOWN_REF_INC), (PVOID)value);

	if (new_value != value)
	{
		_r_protection_release_ex (protection);
	}
}

FORCEINLINE VOID _r_protection_waitfor (_Inout_ PR_RUNDOWN_PROTECT protection)
{
	ULONG_PTR value;

	value = (ULONG_PTR)_InterlockedCompareExchangePointer ((PVOID_PTR)&protection->value, IntToPtr (PR_RUNDOWN_ACTIVE), IntToPtr (0));

	if (value != 0 && value != PR_RUNDOWN_ACTIVE)
	{
		_r_protection_waitfor_ex (protection);
	}
}

//
// Synchronization: Workqueue
//

VOID _r_workqueue_initialize (_Out_ PR_WORKQUEUE work_queue, _In_ ULONG minimum_threads, _In_ ULONG maximum_threads, _In_ ULONG no_work_timeout, _In_opt_ PR_THREAD_ENVIRONMENT environment);
VOID _r_workqueue_destroy (_Inout_ PR_WORKQUEUE work_queue);

PR_WORKQUEUE_ITEM _r_workqueue_createitem (_In_ THREAD_CALLBACK function, _In_opt_ PVOID context);
VOID _r_workqueue_destroyitem (_In_ PR_WORKQUEUE_ITEM work_queue_item);

VOID _r_workqueue_queueitem (_Inout_ PR_WORKQUEUE work_queue, _In_ THREAD_CALLBACK function, _In_opt_ PVOID context);
VOID _r_workqueue_waitforfinish (_Inout_ PR_WORKQUEUE work_queue);

//
// Synchronization: Mutex
//

BOOLEAN _r_mutex_create (_In_ LPCWSTR name, _Inout_ PHANDLE hmutex);
BOOLEAN _r_mutex_destroy (_Inout_ PHANDLE hmutex);
BOOLEAN _r_mutex_isexists (_In_ LPCWSTR name);

//
// Memory allocation
//

HANDLE NTAPI _r_mem_getheap ();

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocate (_In_ SIZE_T bytes_count)
{
	return RtlAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS, bytes_count);
}

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocatesafe (_In_ SIZE_T bytes_count)
{
	return RtlAllocateHeap (_r_mem_getheap (), HEAP_ZERO_MEMORY, bytes_count);
}

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocatezero (_In_ SIZE_T bytes_count)
{
	return RtlAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);
}

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocateandcopy (_In_ PVOID src, _In_ SIZE_T bytes_count)
{
	PVOID dst = _r_mem_allocatezero (bytes_count);

	RtlCopyMemory (dst, src, bytes_count);

	return dst;
}

// // If RtlReAllocateHeap fails, the original memory is not freed, and the original handle and pointer are still valid.
// // https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_reallocate (_Frees_ptr_opt_ PVOID memory_address, _In_ SIZE_T bytes_count)
{
	return RtlReAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS, memory_address, bytes_count);
}

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_reallocatesafe (_Frees_ptr_opt_ PVOID memory_address, _In_ SIZE_T bytes_count)
{
	return RtlReAllocateHeap (_r_mem_getheap (), HEAP_ZERO_MEMORY, memory_address, bytes_count);
}

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_reallocatezero (_Frees_ptr_opt_ PVOID memory_address, _In_ SIZE_T bytes_count)
{
	return RtlReAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, memory_address, bytes_count);
}

FORCEINLINE VOID NTAPI _r_mem_free (_Frees_ptr_opt_ PVOID memory_address)
{
	RtlFreeHeap (_r_mem_getheap (), 0, memory_address);
}

//
// Objects reference
//

_Post_writable_byte_size_ (bytes_count)
PVOID _r_obj_allocate (_In_ SIZE_T bytes_count, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

PVOID _r_obj_reference (_In_ PVOID object_body);
VOID _r_obj_dereference_ex (_In_ PVOID object_body, _In_ LONG ref_count);

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_referencesafe (_In_opt_ PVOID object_body)
{
	if (!object_body)
		return NULL;

	return _r_obj_reference (object_body);
}

FORCEINLINE VOID _r_obj_dereference (_In_ PVOID object_body)
{
	_r_obj_dereference_ex (object_body, 1);
}

FORCEINLINE VOID _r_obj_dereferencelist (_In_reads_ (count) PVOID_PTR objects, _In_ SIZE_T count)
{
	for (SIZE_T i = 0; i < count; i++)
	{
		_r_obj_dereference_ex (objects[i], 1);
	}
}

FORCEINLINE VOID _r_obj_movereference (_Inout_ PVOID_PTR object_body, _In_opt_ PVOID new_object)
{
	PVOID old_object;

	old_object = *object_body;
	*object_body = new_object;

	if (old_object)
		_r_obj_dereference (old_object);
}

FORCEINLINE VOID _r_obj_clearreference (_Inout_ PVOID_PTR object_body)
{
	_r_obj_movereference (object_body, NULL);
}

//
// 8-bit string object
//

#define _r_obj_isbyteempty(string) \
    ((string) == NULL || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == ANSI_NULL)

PR_BYTE _r_obj_createbyteex (_In_opt_ LPSTR buffer, _In_ SIZE_T length);

VOID _r_obj_setbytelength (_Inout_ PR_BYTE string, _In_ SIZE_T length);

FORCEINLINE VOID _r_obj_writebytenullterminator (_In_ PR_BYTE string)
{
	*(LPSTR)PTR_ADD_OFFSET (string->buffer, string->length) = ANSI_NULL;
}

//
// 16-bit string object
//

#define _r_obj_isstringempty(string) \
    ((string) == NULL || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == UNICODE_NULL)

#define _r_obj_isstringempty2(string) \
    ((string)->length == 0 || (string)->buffer[0] == UNICODE_NULL)

PR_STRING _r_obj_createstringex (_In_opt_ LPCWSTR buffer, _In_ SIZE_T length);

PR_STRING _r_obj_concatstrings (_In_ SIZE_T count, ...);
PR_STRING _r_obj_concatstrings_v (_In_ SIZE_T count, _In_ va_list arg_ptr);

PR_STRING _r_obj_concatstringrefs (_In_ SIZE_T count, ...);
PR_STRING _r_obj_concatstringrefs_v (_In_ SIZE_T count, _In_ va_list arg_ptr);

VOID _r_obj_removestring (_In_ PR_STRING string, _In_ SIZE_T start_pos, _In_ SIZE_T length);

VOID _r_obj_setstringlength (_Inout_ PR_STRING string, _In_ SIZE_T length);

FORCEINLINE PR_STRING _r_obj_createstring (_In_ LPCWSTR string)
{
	return _r_obj_createstringex (string, _r_str_getlength (string) * sizeof (WCHAR));
}

FORCEINLINE PR_STRING _r_obj_createstring2 (_In_ PR_STRING string)
{
	return _r_obj_createstringex (string->buffer, string->length);
}

FORCEINLINE PR_STRING _r_obj_createstring3 (_In_ PR_STRINGREF string)
{
	return _r_obj_createstringex (string->buffer, string->length);
}

FORCEINLINE PR_STRING _r_obj_createstringfromunicodestring (_In_ PUNICODE_STRING string)
{
	return _r_obj_createstringex (string->Buffer, string->Length);
}

_Ret_maybenull_
FORCEINLINE LPCWSTR _r_obj_getstring (_In_opt_ PR_STRING string)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return NULL;
}

FORCEINLINE LPCWSTR _r_obj_getstringorempty (_In_opt_ PR_STRING string)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return L"";
}

FORCEINLINE LPCWSTR _r_obj_getstringordefault (_In_opt_ PR_STRING string, _In_opt_ LPCWSTR def)
{
	if (!_r_obj_isstringempty (string))
		return string->buffer;

	return def;
}

FORCEINLINE SIZE_T _r_obj_getstringrefsize (_In_ PR_STRINGREF string)
{
	return string->length;
}

FORCEINLINE SIZE_T _r_obj_getstringreflength (_In_ PR_STRINGREF string)
{
	return string->length / sizeof (WCHAR);
}

FORCEINLINE ULONG _r_obj_getstringrefhash (_In_ PR_STRINGREF string)
{
	return _r_str_fnv32a (string, TRUE);
}

FORCEINLINE SIZE_T _r_obj_getstringlength (_In_ PR_STRING string)
{
	return string->length / sizeof (WCHAR);
}

FORCEINLINE ULONG _r_obj_getstringhash (_In_ PR_STRING string)
{
	return _r_str_fnv32a (&string->sr, TRUE);
}

FORCEINLINE VOID _r_obj_writestringnullterminator (_In_ PR_STRING string)
{
	assert (!(string->length & 0x01));

	*(LPWSTR)PTR_ADD_OFFSET (string->buffer, string->length) = UNICODE_NULL;
}

FORCEINLINE VOID _r_obj_trimstringtonullterminator (_In_ PR_STRING string)
{
	string->length = _r_str_getlength (string->buffer) * sizeof (WCHAR);

	_r_obj_writestringnullterminator (string); // terminate
}

//
// 8-bit string reference object
//

FORCEINLINE VOID _r_obj_initializebyterefex (_Out_ PR_BYTEREF string, _In_opt_ LPSTR buffer, _In_opt_ SIZE_T length)
{
	string->buffer = buffer;
	string->length = length;
}

FORCEINLINE VOID _r_obj_initializebyterefempty (_Out_ PR_BYTEREF string)
{
	_r_obj_initializebyterefex (string, NULL, 0);
}

FORCEINLINE VOID _r_obj_initializebyterefconst (_Out_ PR_BYTEREF string, _In_ LPCSTR buffer)
{
	_r_obj_initializebyterefex (string, (LPSTR)buffer, _r_str_getbytelength (buffer));
}

FORCEINLINE VOID _r_obj_initializebyteref (_Out_ PR_BYTEREF string, _In_ LPSTR buffer)
{
	_r_obj_initializebyterefex (string, buffer, _r_str_getbytelength (buffer));
}

FORCEINLINE VOID _r_obj_initializebyteref2 (_Out_ PR_BYTEREF string, _In_ PR_BYTE buffer)
{
	_r_obj_initializebyterefex (string, buffer->buffer, buffer->length);
}

FORCEINLINE VOID _r_obj_initializebyteref3 (_Out_ PR_BYTEREF string, _In_ PR_BYTEREF buffer)
{
	_r_obj_initializebyterefex (string, buffer->buffer, buffer->length);
}

//
// 16-bit string reference object
//

FORCEINLINE VOID _r_obj_initializestringrefex (_Out_ PR_STRINGREF string, _In_opt_ LPWSTR buffer, _In_opt_ SIZE_T length)
{
	string->buffer = buffer;
	string->length = length;
}

FORCEINLINE VOID _r_obj_initializestringrefempty (_Out_ PR_STRINGREF string)
{
	_r_obj_initializestringrefex (string, NULL, 0);
}

FORCEINLINE VOID _r_obj_initializestringrefconst (_Out_ PR_STRINGREF string, _In_ LPCWSTR buffer)
{
	_r_obj_initializestringrefex (string, (LPWSTR)buffer, _r_str_getlength (buffer) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_initializestringref (_Out_ PR_STRINGREF string, _In_ LPWSTR buffer)
{
	_r_obj_initializestringrefex (string, buffer, _r_str_getlength (buffer) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_initializestringref2 (_Out_ PR_STRINGREF string, _In_ PR_STRING buffer)
{
	_r_obj_initializestringrefex (string, buffer->buffer, buffer->length);
}

FORCEINLINE VOID _r_obj_initializestringref3 (_Out_ PR_STRINGREF string, _In_ PR_STRINGREF buffer)
{
	_r_obj_initializestringrefex (string, buffer->buffer, buffer->length);
}

FORCEINLINE VOID _r_obj_initializestringref4 (_Out_ PR_STRINGREF string, _In_ PUNICODE_STRING buffer)
{
	_r_obj_initializestringrefex (string, buffer->Buffer, buffer->Length);
}

//
// Unicode string object
//

FORCEINLINE BOOLEAN _r_obj_initializeunicodestringex (_Out_ PUNICODE_STRING string, _In_opt_ LPWSTR buffer, _In_opt_ USHORT length, _In_opt_ USHORT max_length)
{
	string->Length = length;
	string->MaximumLength = max_length;
	string->Buffer = buffer;

	return string->Length <= UNICODE_STRING_MAX_BYTES;
}

FORCEINLINE BOOLEAN _r_obj_initializeunicodestring2 (_Out_ PUNICODE_STRING string, _In_ PR_STRING buffer)
{
	return _r_obj_initializeunicodestringex (string, buffer->buffer, (USHORT)buffer->length, (USHORT)buffer->length + sizeof (UNICODE_NULL));
}

FORCEINLINE BOOLEAN _r_obj_initializeunicodestring3 (_Out_ PUNICODE_STRING string, _In_ PR_STRINGREF buffer)
{
	return _r_obj_initializeunicodestringex (string, buffer->buffer, (USHORT)buffer->length, (USHORT)buffer->length + sizeof (UNICODE_NULL));
}

//
// String builder
//

VOID _r_obj_initializestringbuilder (_Out_ PR_STRINGBUILDER string);
VOID _r_obj_appendstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ LPCWSTR text, _In_ SIZE_T length);
VOID _r_obj_appendstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);
VOID _r_obj_insertstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ LPCWSTR text, _In_ SIZE_T length);
VOID _r_obj_insertstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);
VOID _r_obj_resizestringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T new_capacity);

FORCEINLINE VOID _r_obj_appendstringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ LPCWSTR text)
{
	_r_obj_appendstringbuilderex (string, text, _r_str_getlength (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_appendstringbuilder2 (_Inout_ PR_STRINGBUILDER string, _In_ PR_STRING text)
{
	_r_obj_appendstringbuilderex (string, text->buffer, text->length);
}

FORCEINLINE VOID _r_obj_appendstringbuilder3 (_Inout_ PR_STRINGBUILDER string, _In_ PR_STRINGREF text)
{
	_r_obj_appendstringbuilderex (string, text->buffer, text->length);
}

FORCEINLINE VOID _r_obj_appendstringbuilderformat (_Inout_ PR_STRINGBUILDER string, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_appendstringbuilderformat_v (string, format, arg_ptr);
	va_end (arg_ptr);
}

FORCEINLINE VOID _r_obj_insertstringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ LPCWSTR text)
{
	_r_obj_insertstringbuilderex (string, index, text, _r_str_getlength (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_insertstringbuilder2 (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ PR_STRING text)
{
	_r_obj_insertstringbuilderex (string, index, text->buffer, text->length);
}

FORCEINLINE VOID _r_obj_insertstringbuilder3 (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ PR_STRINGREF text)
{
	_r_obj_insertstringbuilderex (string, index, text->buffer, text->length);
}

FORCEINLINE VOID _r_obj_insertstringbuilderformat (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_obj_insertstringbuilderformat_v (string, index, format, arg_ptr);
	va_end (arg_ptr);
}

FORCEINLINE VOID _r_obj_deletestringbuilder (_Inout_ PR_STRINGBUILDER string)
{
	if (string->string)
	{
		_r_obj_clearreference (&string->string);
	}
}

FORCEINLINE PR_STRING _r_obj_finalstringbuilder (_In_ PR_STRINGBUILDER string)
{
	return string->string;
}

//
// Array object
//

#define _r_obj_isarrayempty(array_node) \
    ((array_node) == NULL || (array_node)->count == 0)

PR_ARRAY _r_obj_createarrayex (_In_ SIZE_T item_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

_Ret_maybenull_
PVOID _r_obj_getarrayitem (_In_opt_ PR_ARRAY array_node, _In_ SIZE_T index);

VOID _r_obj_cleararray (_Inout_ PR_ARRAY array_node);
VOID _r_obj_resizearray (_Inout_ PR_ARRAY array_node, _In_ SIZE_T new_capacity);

_Ret_maybenull_
PVOID _r_obj_addarrayitemex (_Inout_ PR_ARRAY array_node, _In_opt_ PVOID item, _Out_opt_ PSIZE_T new_index);

VOID _r_obj_addarrayitems (_Inout_ PR_ARRAY array_node, _In_ PVOID items, _In_ SIZE_T count);
VOID _r_obj_removearrayitems (_Inout_ PR_ARRAY array_node, _In_ SIZE_T start_pos, _In_ SIZE_T count);

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_addarrayitem (_Inout_ PR_ARRAY array_node, _In_opt_ PVOID item)
{
	return _r_obj_addarrayitemex (array_node, item, NULL);
}

FORCEINLINE PR_ARRAY _r_obj_createarray (_In_ SIZE_T item_size, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createarrayex (item_size, 2, cleanup_callback);
}

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_getarrayitem (_In_opt_ PR_ARRAY array_node, _In_ SIZE_T index)
{
	if (array_node)
		return PTR_ADD_OFFSET (array_node->items, index * array_node->item_size);

	return NULL;
}

FORCEINLINE SIZE_T _r_obj_getarraysize (_In_opt_ PR_ARRAY array_node)
{
	if (array_node)
		return array_node->count;

	return 0;
}

FORCEINLINE VOID _r_obj_removearrayitem (_In_ PR_ARRAY array_node, _In_ SIZE_T index)
{
	_r_obj_removearrayitems (array_node, index, 1);
}

//
// List object
//

#define _r_obj_islistempty(list_node) \
    ((list_node) == NULL || (list_node)->count == 0)

PR_LIST _r_obj_createlistex (_In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

_Ret_maybenull_
PVOID _r_obj_addlistitemex (_Inout_ PR_LIST list_node, _In_opt_ PVOID item, _Out_opt_ PSIZE_T new_index);

VOID _r_obj_clearlist (_Inout_ PR_LIST list_node);
SIZE_T _r_obj_findlistitem (_In_ PR_LIST list_node, _In_ PVOID list_item);
VOID _r_obj_insertlistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ PVOID_PTR items, _In_ SIZE_T count);
VOID _r_obj_removelistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ SIZE_T count);
VOID _r_obj_resizelist (_Inout_ PR_LIST list_node, _In_ SIZE_T new_capacity);
VOID _r_obj_setlistitem (_Inout_ PR_LIST list_node, _In_ SIZE_T index, _In_opt_ PVOID item);

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_addlistitem (_Inout_ PR_LIST list_node, _In_opt_ PVOID item)
{
	return _r_obj_addlistitemex (list_node, item, NULL);
}

FORCEINLINE PR_LIST _r_obj_createlist (_In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createlistex (2, cleanup_callback);
}

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_getlistitem (_In_opt_ PR_LIST list_node, _In_ SIZE_T index)
{
	if (list_node)
		return list_node->items[index];

	return NULL;
}

FORCEINLINE SIZE_T _r_obj_getlistsize (_In_opt_ PR_LIST list_node)
{
	if (list_node)
		return list_node->count;

	return 0;
}

FORCEINLINE VOID _r_obj_removelistitem (_Inout_ PR_LIST list_node, _In_ SIZE_T index)
{
	_r_obj_removelistitems (list_node, index, 1);
}

//
// Hashtable object
//

// A hashtable with power-of-two bucket sizes and with all entries stored in a single
// array. This improves locality but may be inefficient when resizing the hashtable. It is a good
// idea to store pointers to objects as entries, as opposed to the objects themselves.

#define _r_obj_ishashtableempty(hashtable) \
    ((hashtable) == NULL || (hashtable)->count == 0)

PR_HASHTABLE _r_obj_createhashtableex (_In_ SIZE_T entry_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

_Ret_maybenull_
PVOID _r_obj_addhashtableitem (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code, _In_opt_ PVOID entry);

VOID _r_obj_clearhashtable (_Inout_ PR_HASHTABLE hashtable);

_Success_ (return)
BOOLEAN _r_obj_enumhashtable (_In_ PR_HASHTABLE hashtable, _Outptr_opt_ PVOID_PTR entry, _Out_opt_ PULONG_PTR hash_code, _Inout_ PSIZE_T enum_key);

_Ret_maybenull_
PVOID _r_obj_findhashtable (_In_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code);

BOOLEAN _r_obj_removehashtableitem (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code);
VOID _r_obj_resizehashtable (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T new_capacity);

FORCEINLINE PR_HASHTABLE _r_obj_createhashtable (_In_ SIZE_T entry_size, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createhashtableex (entry_size, 2, cleanup_callback);
}

FORCEINLINE SIZE_T _r_obj_gethashtablesize (_In_opt_ PR_HASHTABLE hashtable)
{
	if (hashtable)
		return hashtable->count;

	return 0;
}

//
// Hashtable pointer object
//

FORCEINLINE PR_HASHTABLE _r_obj_createhashtablepointer (_In_ SIZE_T initial_capacity)
{
	return _r_obj_createhashtableex (sizeof (R_OBJECT_POINTER), initial_capacity, &_r_util_dereferencehashtableptrprocedure);
}

_Ret_maybenull_
PVOID _r_obj_addhashtablepointer (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code, _In_opt_ PVOID value);

_Success_ (return)
BOOLEAN _r_obj_enumhashtablepointer (_In_ PR_HASHTABLE hashtable, _Outptr_opt_ PVOID_PTR entry, _Out_opt_ PULONG_PTR hash_code, _Inout_ PSIZE_T enum_key);

_Ret_maybenull_
PVOID _r_obj_findhashtablepointer (_In_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code);

FORCEINLINE BOOLEAN _r_obj_removehashtablepointer (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code)
{
	return _r_obj_removehashtableitem (hashtable, hash_code);
}

//
// Debugging
//

FORCEINLINE VOID _r_debug (_In_ LPCWSTR string)
{
	OutputDebugString (string);
}

VOID _r_debug_v (_In_ _Printf_format_string_ LPCWSTR format, ...);

#define RDBG(a) _r_debug ((a))
#define RDBG2(a, ...) _r_debug_v ((a), __VA_ARGS__)

//
// Format strings, dates, numbers
//

PR_STRING _r_format_string (_In_ _Printf_format_string_ LPCWSTR format, ...);
PR_STRING _r_format_string_v (_In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);

_Success_ (return)
BOOLEAN _r_format_bytesize64 (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ ULONG64 bytes);

_Ret_maybenull_
PR_STRING _r_format_filetimeex (_In_ LPFILETIME file_time, _In_ ULONG flags);

_Ret_maybenull_
PR_STRING _r_format_unixtimeex (_In_ LONG64 unixtime, _In_ ULONG flags);

_Ret_maybenull_
PR_STRING _r_format_interval (_In_ LONG64 seconds, _In_ INT digits);

_Success_ (return)
BOOLEAN _r_format_number (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ ULONG buffer_size, _In_ LONG64 number);

_Ret_maybenull_
FORCEINLINE PR_STRING _r_fmt_unixtime (_In_ LONG64 unixtime)
{
	return _r_format_unixtimeex (unixtime, FDTF_DEFAULT);
}

//
// Calculation
//

FORCEINLINE INT _r_calc_clamp (_In_ INT value, _In_ INT min_value, _In_ INT max_value)
{
	return min (max (value, min_value), max_value);
}

FORCEINLINE LONG _r_calc_clamp32 (_In_ LONG value, _In_ LONG min_value, _In_ LONG max_value)
{
	return min (max (value, min_value), max_value);
}

FORCEINLINE LONG64 _r_calc_clamp64 (_In_ LONG64 value, _In_ LONG64 min_value, _In_ LONG64 max_value)
{
	return min (max (value, min_value), max_value);
}

FORCEINLINE LONG _r_calc_percentof (_In_ LONG length, _In_ LONG total_length)
{
	return (LONG)(((DOUBLE)length / (DOUBLE)total_length) * 100.0);
}

FORCEINLINE LONG64 _r_calc_percentof64 (_In_ LONG64 length, _In_ LONG64 total_length)
{
	return (LONG64)(((DOUBLE)length / (DOUBLE)total_length) * 100.0);
}

FORCEINLINE LONG _r_calc_percentval (_In_ LONG percent, _In_ LONG total_length)
{
	return (total_length * percent) / 100;
}

FORCEINLINE LONG64 _r_calc_percentval64 (_In_ LONG64 percent, _In_ LONG64 total_length)
{
	return (total_length * percent) / 100;
}

FORCEINLINE LONG _r_calc_rectheight (_In_ PRECT rect)
{
	return rect->bottom - rect->top;
}

FORCEINLINE LONG _r_calc_rectwidth (_In_ PRECT rect)
{
	return rect->right - rect->left;
}

FORCEINLINE LONG _r_calc_kilobytes2bytes (_In_ LONG kilobytes)
{
	return kilobytes * 1024L;
}

FORCEINLINE LONG64 _r_calc_kilobytes2bytes64 (_In_ LONG64 kilobytes)
{
	return kilobytes * 1024LL;
}

FORCEINLINE LONG _r_calc_megabytes2bytes (_In_ LONG megabytes)
{
	return megabytes * 1048576L;
}

FORCEINLINE LONG64 _r_calc_megabytes2bytes64 (_In_ LONG64 megabytes)
{
	return megabytes * 1048576LL;
}

FORCEINLINE LONG _r_calc_seconds2milliseconds (_In_ LONG seconds)
{
	return seconds * 1000L;
}

FORCEINLINE LONG _r_calc_minutes2seconds (_In_ LONG minutes)
{
	return minutes * 60L;
}

FORCEINLINE LONG _r_calc_hours2seconds (_In_ LONG hours)
{
	return hours * 3600L;
}

FORCEINLINE LONG _r_calc_days2seconds (_In_ LONG days)
{
	return days * 86400L;
}

FORCEINLINE VOID _r_calc_millisecondstolargeinteger (_Out_ PLARGE_INTEGER timeout, _In_ ULONG milliseconds)
{
	if (milliseconds == 0 || milliseconds == INFINITE)
	{
		timeout->QuadPart = 0;
	}
	else
	{
		timeout->QuadPart = -(LONGLONG)UInt32x32To64 (milliseconds, 10000);
	}
}

FORCEINLINE ULONG _r_calc_multipledivide (_In_ ULONG number, _In_ ULONG numerator, _In_ ULONG denominator)
{
	return (ULONG)(((ULONG64)number * (ULONG64)numerator + denominator / 2) / (ULONG64)denominator);
}

FORCEINLINE LONG _r_calc_multipledividesigned (_In_ LONG number, _In_ ULONG numerator, _In_ ULONG denominator)
{
	if (number >= 0)
	{
		return _r_calc_multipledivide (number, numerator, denominator);
	}
	else
	{
		return -(LONG)_r_calc_multipledivide (-number, numerator, denominator);
	}
}

//
// Byteswap
//

FORCEINLINE USHORT _r_byteswap_ushort (_In_ USHORT number)
{
	return _byteswap_ushort (number);
}

FORCEINLINE ULONG _r_byteswap_ulong (_In_ ULONG number)
{
	return _byteswap_ulong (number);
}

FORCEINLINE ULONG64 _r_byteswap_ulong64 (_In_ ULONG64 number)
{
	return _byteswap_uint64 (number);
}

//
// Modal dialogs
//

HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);

#if !defined(APP_NO_DEPRECATIONS)
_Success_ (return)
BOOLEAN _r_msg_taskdialog (_In_ const TASKDIALOGCONFIG * task_dialog, _Out_opt_ PINT button, _Out_opt_ PINT radio_button, _Out_opt_ LPBOOL is_flagchecked); // vista+ TaskDialogIndirect
#else
_Success_ (return)
FORCEINLINE BOOLEAN _r_msg_taskdialog (_In_ const TASKDIALOGCONFIG * task_dialog, _Out_opt_ PINT button, _Out_opt_ PINT radio_button, _Out_opt_ LPBOOL is_flagchecked)
{
	return (TaskDialogIndirect (task_dialog, button, radio_button, is_flagchecked) == S_OK);
}
#endif // APP_NO_DEPRECATIONS

//
// Clipboard operations
//

_Ret_maybenull_
PR_STRING _r_clipboard_get (_In_opt_ HWND hwnd);
BOOLEAN _r_clipboard_set (_In_opt_ HWND hwnd, _In_ PR_STRINGREF string);

//
// Filesystem
//

BOOLEAN _r_fs_deletefile (_In_ LPCWSTR path, _In_ BOOLEAN is_force);
BOOLEAN _r_fs_deletedirectory (_In_ LPCWSTR path, _In_ BOOLEAN is_recurse);
LONG64 _r_fs_getfilesize (_In_ LPCWSTR path);
BOOLEAN _r_fs_makebackup (_In_ LPCWSTR path, _In_ BOOLEAN is_removesourcefile);
BOOLEAN _r_fs_mkdir (_In_ LPCWSTR path);

_Ret_maybenull_
PR_BYTE _r_fs_readfile (_In_ HANDLE hfile);

#define _r_fs_isvalidhandle(handle) \
    ((handle) != NULL && (handle) != INVALID_HANDLE_VALUE)

FORCEINLINE BOOLEAN _r_fs_exists (_In_ LPCWSTR path)
{
	return RtlDoesFileExists_U (path);
}

FORCEINLINE BOOLEAN _r_fs_copyfile (_In_ LPCWSTR path_from, _In_ LPCWSTR path_to, _In_ ULONG flags)
{
	return !!CopyFileEx (path_from, path_to, NULL, NULL, NULL, flags);
}

FORCEINLINE BOOLEAN _r_fs_movefile (_In_ LPCWSTR path_from, _In_opt_ LPCWSTR path_to, _In_ ULONG flags)
{
	return !!MoveFileEx (path_from, path_to, flags);
}

FORCEINLINE BOOLEAN _r_fs_setpos (_In_ HANDLE hfile, _In_ LONG64 pos, _In_ ULONG method)
{
	LARGE_INTEGER lpos = {0};

	lpos.QuadPart = pos;

	return !!SetFilePointerEx (hfile, lpos, NULL, method);
}

FORCEINLINE LONG64 _r_fs_getsize (_In_ HANDLE hfile)
{
	LARGE_INTEGER size;

	if (GetFileSizeEx (hfile, &size))
		return size.QuadPart;

	return 0;
}

//
// Paths
//

_Ret_maybenull_
PR_STRING _r_path_compact (_In_ LPCWSTR path, _In_ UINT length);

_Success_ (return)
BOOLEAN _r_path_getpathinfo (_In_ PR_STRINGREF path, _Out_opt_ PR_STRINGREF directory, _Out_opt_ PR_STRINGREF basename);

_Ret_maybenull_
PR_STRING _r_path_getbasedirectory (_In_ PR_STRINGREF path);

LPCWSTR _r_path_getbasename (_In_ LPCWSTR path);

PR_STRING _r_path_getbasenamestring (_In_ PR_STRINGREF path);

_Ret_maybenull_
LPCWSTR _r_path_getextension (_In_ LPCWSTR path);

_Ret_maybenull_
PR_STRING _r_path_getextensionstring (_In_ PR_STRINGREF path);

_Ret_maybenull_
PR_STRING _r_path_getfullpath (_In_ LPCWSTR path);

_Ret_maybenull_
PR_STRING _r_path_getknownfolder (_In_ ULONG folder, _In_opt_ LPCWSTR append);

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (_In_opt_ HMODULE hmodule);

_Ret_maybenull_
PR_STRING _r_path_search (_In_ LPCWSTR path);

PR_STRING _r_path_dospathfromnt (_In_ LPCWSTR path);

_Ret_maybenull_
PR_STRING _r_path_ntpathfromdos (_In_ LPCWSTR path, _Out_opt_ PULONG error_code);

//
// Shell
//

VOID _r_shell_showfile (_In_ LPCWSTR path);

FORCEINLINE VOID _r_shell_opendefault (_In_ LPCWSTR path)
{
	ShellExecute (NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
}

//
// Strings
//

#define _r_str_isempty(string) \
    ((string) == NULL || (string)[0] == UNICODE_NULL)

#define _r_str_isbyteempty(string) \
	((string) == NULL || (string)[0] == ANSI_NULL)

BOOLEAN _r_str_isequal (_In_ PR_STRINGREF string1, _In_ PR_STRINGREF string2, _In_ BOOLEAN is_ignorecase);
BOOLEAN _r_str_isequal2 (_In_ PR_STRINGREF string1, _In_ LPCWSTR string2, _In_ BOOLEAN is_ignorecase);

BOOLEAN _r_str_isnumeric (_In_ PR_STRINGREF string);

BOOLEAN _r_str_isstartswith (_In_ PR_STRINGREF string, _In_ PR_STRINGREF prefix, _In_ BOOLEAN is_ignorecase);
BOOLEAN _r_str_isstartswith2 (_In_ PR_STRINGREF string, _In_ LPCWSTR prefix, _In_ BOOLEAN is_ignorecase);

BOOLEAN _r_str_isendsswith (_In_ PR_STRINGREF string, _In_ PR_STRINGREF suffix, _In_ BOOLEAN is_ignorecase);
BOOLEAN _r_str_isendsswith2 (_In_ PR_STRINGREF string, _In_ LPCWSTR prefix, _In_ BOOLEAN is_ignorecase);

_Success_ (return)
BOOLEAN _r_str_append (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string);

_Success_ (return)
BOOLEAN _r_str_appendformat (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...);

_Success_ (return)
BOOLEAN _r_str_copy (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string);

_Success_ (return)
BOOLEAN _r_str_copystring (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ PR_STRINGREF string);

_Success_ (return)
BOOLEAN _r_str_printf (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...);

_Success_ (return)
BOOLEAN _r_str_printf_v (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);

ULONG _r_str_crc32 (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
ULONG64 _r_str_crc64 (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
ULONG _r_str_fnv32a (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);
ULONG64 _r_str_fnv64a (_In_ PR_STRINGREF string, _In_ BOOLEAN is_ignorecase);

ULONG _r_str_gethash (_In_ LPCWSTR string);

_Ret_maybenull_
PR_STRING _r_str_expandenvironmentstring (_In_ PR_STRINGREF string);

_Ret_maybenull_
PR_STRING _r_str_unexpandenvironmentstring (_In_ LPCWSTR string);

_Ret_maybenull_
PR_STRING _r_str_fromguid (_In_ LPCGUID lpguid);

_Ret_maybenull_
PR_STRING _r_str_fromsecuritydescriptor (_In_ PSECURITY_DESCRIPTOR lpsd, _In_ SECURITY_INFORMATION information);

_Ret_maybenull_
PR_STRING _r_str_fromsid (_In_ PSID lpsid);

BOOLEAN _r_str_touinteger64 (_In_ PR_STRINGREF string, _In_ ULONG base, _Out_ PULONG64 integer);
BOOLEAN _r_str_tointeger64 (_In_ PR_STRINGREF string, _In_ ULONG base, _Out_opt_ PULONG new_base, _Out_ PLONG64 integer);

BOOLEAN _r_str_toboolean (_In_ PR_STRINGREF string);
LONG _r_str_tolongex (_In_ PR_STRINGREF string, _In_ ULONG base);
LONG64 _r_str_tolong64 (_In_ PR_STRINGREF string);
ULONG _r_str_toulongex (_In_ PR_STRINGREF string, _In_ ULONG base);
ULONG64 _r_str_toulong64 (_In_ PR_STRINGREF string);

FORCEINLINE LONG _r_str_tolong (_In_ PR_STRINGREF string)
{
	return _r_str_tolongex (string, 10);
}

FORCEINLINE ULONG _r_str_toulong (_In_ PR_STRINGREF string)
{
	return _r_str_toulongex (string, 10);
}

FORCEINLINE INT _r_str_tointeger (_In_ PR_STRINGREF string)
{
	return (INT)_r_str_tolong (string);
}

FORCEINLINE UINT _r_str_touinteger (_In_ PR_STRINGREF string)
{
	return (UINT)_r_str_toulong (string);
}

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

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findchar (_In_ PR_STRINGREF string, _In_ WCHAR character, _In_ BOOLEAN is_ignorecase);

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findlastchar (_In_ PR_STRINGREF string, _In_ WCHAR character, _In_ BOOLEAN is_ignorecase);

_Success_ (return)
BOOLEAN _r_str_match (_In_ LPCWSTR string, _In_ LPCWSTR pattern, _In_ BOOLEAN is_ignorecase);

BOOLEAN _r_str_splitatchar (_In_ PR_STRINGREF string, _In_ WCHAR separator, _Out_ PR_STRINGREF first_part, _Out_ PR_STRINGREF second_part);
BOOLEAN _r_str_splitatlastchar (_In_ PR_STRINGREF string, _In_ WCHAR separator, _Out_ PR_STRINGREF first_part, _Out_ PR_STRINGREF second_part);

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (_In_ PR_STRINGREF string, _In_ WCHAR key_delimeter, _In_ WCHAR value_delimeter);

VOID _r_str_replacechar (_Inout_ PR_STRINGREF string, _In_ WCHAR char_from, _In_ WCHAR char_to);

VOID _r_str_trimstringref (_Inout_ PR_STRINGREF string, _In_ PR_STRINGREF charset);
VOID _r_str_trimstringref2 (_Inout_ PR_STRINGREF string, _In_ LPCWSTR charset);

VOID _r_str_tolower (_Inout_ PR_STRINGREF string);
VOID _r_str_toupper (_Inout_ PR_STRINGREF string);

INT _r_str_versioncompare (_In_ PR_STRINGREF v1, _In_ PR_STRINGREF v2);

_Ret_maybenull_
PR_STRING _r_str_multibyte2unicode (_In_ PR_BYTEREF string);

_Ret_maybenull_
PR_BYTE _r_str_unicode2multibyte (_In_ PR_STRINGREF string);

FORCEINLINE BOOLEAN _r_str_isdigit (_In_ WCHAR chr)
{
	return (USHORT)(chr - L'0') < 10;
}

FORCEINLINE INT _r_str_compare (_In_ LPCWSTR string1, _In_ LPCWSTR string2)
{
	return _wcsicmp (string1, string2);
}

FORCEINLINE INT _r_str_compare_length (_In_ LPCWSTR string1, _In_ LPCWSTR string2, _In_ SIZE_T length)
{
	return _wcsnicmp (string1, string2, length);
}

FORCEINLINE INT _r_str_compare_logical (_In_ PR_STRING string1, _In_ PR_STRING string2)
{
	return StrCmpLogicalW (string1->buffer, string2->buffer);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_frominteger (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ INT value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PRIi32), value);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_fromuinteger (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ UINT value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PRIu32), value);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_fromlong (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LONG value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PR_LONG), value);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_fromlong64 (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LONG64 value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PR_LONG64), value);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_fromulong (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ ULONG value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PR_ULONG), value);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_str_fromulong64 (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ ULONG64 value)
{
	return _r_str_printf (buffer, buffer_size, L"%" TEXT (PR_ULONG64), value);
}

FORCEINLINE SIZE_T _r_str_getlength (_In_ LPCWSTR string)
{
	return wcsnlen_s (string, PR_SIZE_MAX_STRING_LENGTH);
}

FORCEINLINE SIZE_T _r_str_getbytelength (_In_ LPCSTR string)
{
	return strnlen_s (string, PR_SIZE_MAX_STRING_LENGTH);
}

FORCEINLINE VOID _r_str_skiplength (_Inout_ PR_STRINGREF string, _In_ SIZE_T length)
{
	string->buffer = (LPWSTR)PTR_ADD_OFFSET (string->buffer, length);
	string->length -= length;
}

FORCEINLINE VOID _r_str_trim (_Inout_ LPWSTR string, _In_ LPCWSTR charset)
{
	StrTrim (string, charset);
}

FORCEINLINE VOID _r_str_trimstring (_Inout_ PR_STRING string, _In_ PR_STRINGREF charset)
{
	_r_str_trimstringref (&string->sr, charset);

	_r_obj_writestringnullterminator (string);
}

FORCEINLINE VOID _r_str_trimstring2 (_Inout_ PR_STRING string, _In_ LPCWSTR charset)
{
	_r_str_trimstringref2 (&string->sr, charset);

	_r_obj_writestringnullterminator (string);
}

FORCEINLINE WCHAR _r_str_lower (_In_ WCHAR chr)
{
	return RtlDowncaseUnicodeChar (chr);
}

FORCEINLINE WCHAR _r_str_upper (_In_ WCHAR chr)
{
	return RtlUpcaseUnicodeChar (chr);
}

//
// System information
//


BOOLEAN _r_sys_iselevated ();
BOOLEAN _r_sys_isprocessimmersive (_In_ HANDLE hprocess);
BOOLEAN _r_sys_iswine ();
BOOLEAN _r_sys_iswow64 ();

R_TOKEN_ATTRIBUTES _r_sys_getcurrenttoken ();

LPCWSTR _r_sys_getsystemdirectory ();
LPCWSTR _r_sys_gettempdirectory ();

BOOLEAN _r_sys_getopt (_In_ LPCWSTR args, _In_ LPCWSTR name, _Out_opt_ PR_STRING* out_value);

_Ret_maybenull_
PSID _r_sys_getservicesid (_In_ PR_STRINGREF name);

_Ret_maybenull_
PR_STRING _r_sys_getsessioninfo (_In_ WTS_INFO_CLASS wts_info);

_Ret_maybenull_
PR_STRING _r_sys_getusernamefromsid (_In_ PSID sid);

ULONG _r_sys_getwindowsversion ();

BOOLEAN _r_sys_createprocessex (_In_opt_ LPCWSTR file_name, _In_opt_ LPCWSTR command_line, _In_opt_ LPCWSTR current_directory, _In_opt_ LPSTARTUPINFO startup_info_ptr, _In_ WORD show_state, _In_ ULONG flags);
NTSTATUS _r_sys_openprocess (_In_opt_ HANDLE process_id, _In_ ACCESS_MASK desired_access, _Out_ PHANDLE process_handle);
NTSTATUS _r_sys_queryprocessstring (_In_ HANDLE process_handle, _In_ PROCESSINFOCLASS info_class, _Out_ PVOID_PTR file_name);

THREAD_API _r_sys_basethreadstart (_In_ PVOID arglist);

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_createthreadex (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist, _Out_opt_ PHANDLE thread_handle, _In_opt_ PR_THREAD_ENVIRONMENT environment);

_Ret_maybenull_
PR_STRING _r_sys_querytaginformation (_In_ HANDLE hprocess, _In_ LPCVOID tag);

NTSTATUS _r_sys_querytokeninformation (_In_ HANDLE token_handle, _In_ TOKEN_INFORMATION_CLASS token_class, _Out_ PVOID_PTR token_info);

VOID _r_sys_setthreadpriority (_In_ HANDLE thread_handle, _In_ PR_THREAD_ENVIRONMENT environment);

NTSTATUS _r_sys_setprivilege (_In_reads_ (count) PULONG privileges, _In_ ULONG count, _In_ BOOLEAN is_enable);

FORCEINLINE BOOLEAN _r_sys_createprocess (_In_opt_ LPCWSTR file_name, _In_opt_ LPCWSTR command_line, _In_opt_ LPCWSTR current_directory)
{
	return _r_sys_createprocessex (file_name, command_line, current_directory, NULL, SW_SHOWDEFAULT, 0);
}

_Success_ (NT_SUCCESS (return))
FORCEINLINE NTSTATUS _r_sys_createthread (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist, _Out_opt_ PHANDLE hthread)
{
	return _r_sys_createthreadex (start_address, arglist, hthread, NULL);
}

FORCEINLINE NTSTATUS _r_sys_createthread2 (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist)
{
	return _r_sys_createthreadex (start_address, arglist, NULL, NULL);
}

FORCEINLINE NTSTATUS _r_sys_resumethread (_In_ HANDLE hthread)
{
	return NtResumeThread (hthread, NULL);
}

FORCEINLINE VOID _r_sys_setenvironment (_Out_ PR_THREAD_ENVIRONMENT environment, _In_ LONG base_priority, _In_ ULONG io_priority, _In_ ULONG page_priority)
{
	environment->base_priority = base_priority;
	environment->io_priority = io_priority;
	environment->page_priority = page_priority;
	environment->is_forced = FALSE;
}

FORCEINLINE VOID _r_sys_setdefaultenvironment (_Out_ PR_THREAD_ENVIRONMENT environment)
{
	_r_sys_setenvironment (environment, THREAD_PRIORITY_NORMAL, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);
}

FORCEINLINE VOID _r_sys_setthreadexecutionstate (_In_ ULONG state)
{
	ULONG old_state;

	NtSetThreadExecutionState (state, &old_state);
}

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

FORCEINLINE ULONG _r_sys_getprocessorscount ()
{
	return NtCurrentPeb ()->NumberOfProcessors;
}

FORCEINLINE HANDLE _r_sys_getstdout ()
{
	return NtCurrentPeb ()->ProcessParameters->StandardOutput;
}

FORCEINLINE ULONG _r_sys_gettickcount ()
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

	return (ULONG)((UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) + UInt32x32To64 ((tick_count.HighPart << 8) & 0xffffffff, USER_SHARED_DATA->TickCountMultiplier));

#endif
}

FORCEINLINE ULONG64 _r_sys_gettickcount64 ()
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

	return (UInt32x32To64 (tick_count.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) + (UInt32x32To64 (tick_count.HighPart, USER_SHARED_DATA->TickCountMultiplier) << 8);
}

FORCEINLINE BOOLEAN _r_sys_isosversionequal (_In_ ULONG version)
{
	return _r_sys_getwindowsversion () == version;
}

FORCEINLINE BOOLEAN _r_sys_isosversiongreaterorequal (_In_ ULONG version)
{
	return _r_sys_getwindowsversion () >= version;
}

FORCEINLINE BOOLEAN _r_sys_isosversionlower (_In_ ULONG version)
{
	return _r_sys_getwindowsversion () < version;
}

FORCEINLINE LONG64 _r_sys_getexecutiontime ()
{
	LARGE_INTEGER li;

	if (QueryPerformanceCounter (&li))
	{
		return li.QuadPart;
	}

	return 0;
}

FORCEINLINE DOUBLE _r_sys_finalexecutiontime (_In_ LONG64 start_time)
{
	LARGE_INTEGER li;

	if (QueryPerformanceFrequency (&li))
	{
		return ((_r_sys_getexecutiontime () - start_time) * 1000.0) / li.QuadPart / 1000.0;
	}

	return 0.0;
}

//
// Unixtime
//

LONG64 _r_unixtime_now ();
VOID _r_unixtime_to_filetime (_In_ LONG64 unixtime, _Out_ PFILETIME file_time);

_Success_ (return)
BOOLEAN _r_unixtime_to_systemtime (_In_ LONG64 unixtime, _Out_ PSYSTEMTIME system_time);

LONG64 _r_unixtime_from_filetime (_In_ const FILETIME * file_time);
LONG64 _r_unixtime_from_systemtime (_In_ const SYSTEMTIME * system_time);

//
// Device context
//

_Success_ (return)
BOOLEAN _r_dc_adjustwindowrect (_Inout_ LPRECT rect, _In_ ULONG style, _In_ ULONG style_ex, _In_ LONG dpi_value, _In_ BOOL is_menu);

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (_In_ UINT action, _In_ UINT param1, _In_ PVOID param2, _In_ LONG dpi_value);

VOID _r_dc_fillrect (_In_ HDC hdc, _In_ PRECT lprc, _In_ COLORREF clr);
COLORREF _r_dc_getcolorbrightness (_In_ COLORREF clr);
COLORREF _r_dc_getcolorshade (_In_ COLORREF clr, _In_ INT percent);

LONG _r_dc_getdpivalue (_In_opt_ HWND hwnd, _In_opt_ PRECT rect);
INT _r_dc_getsystemmetrics (_In_ INT index, _In_ LONG dpi_value);
LONG _r_dc_getfontwidth (_In_ HDC hdc, _In_ PR_STRINGREF string);
VOID _r_dc_getsizedpivalue (_Inout_ PR_SIZE size, _In_ LONG dpi_value, _In_ BOOLEAN is_unpack);

FORCEINLINE LONG _r_dc_getdpi (_In_ INT number, _In_ LONG dpi_value)
{
	return _r_calc_multipledividesigned (number, dpi_value, USER_DEFAULT_SCREEN_DPI);
}

FORCEINLINE LONG _r_dc_getsystemdpi ()
{
	return _r_dc_getdpivalue (NULL, NULL);
}

FORCEINLINE LONG _r_dc_getwindowdpi (_In_ HWND hwnd)
{
	return _r_dc_getdpivalue (hwnd, NULL);
}

FORCEINLINE LONG _r_dc_getmonitordpi (_In_ PRECT rect)
{
	return _r_dc_getdpivalue (NULL, rect);
}

FORCEINLINE LONG _r_dc_fontheighttosize (_In_ INT height, _In_ LONG dpi_value)
{
	return _r_calc_multipledividesigned (-height, 72, dpi_value);
}

FORCEINLINE LONG _r_dc_fontsizetoheight (_In_ INT size, _In_ LONG dpi_value)
{
	return -_r_calc_multipledividesigned (size, dpi_value, 72);
}

//
// File dialog
//

_Success_ (return)
BOOLEAN _r_filedialog_initialize (_Out_ PR_FILE_DIALOG file_dialog, _In_ ULONG flags);

_Success_ (return)
BOOLEAN _r_filedialog_show (_In_opt_ HWND hwnd, _In_ PR_FILE_DIALOG file_dialog);

PR_STRING _r_filedialog_getpath (_In_ PR_FILE_DIALOG file_dialog);
VOID _r_filedialog_setpath (_Inout_ PR_FILE_DIALOG file_dialog, _In_ LPCWSTR path);
VOID _r_filedialog_setfilter (_Inout_ PR_FILE_DIALOG file_dialog, _In_ COMDLG_FILTERSPEC * filters, _In_ ULONG count);

VOID _r_filedialog_destroy (_In_ PR_FILE_DIALOG file_dialog);

//
// Window layout
//

BOOLEAN _r_layout_initializemanager (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND hwnd);
VOID _r_layout_destroymanager (_Inout_ PR_LAYOUT_MANAGER layout_manager);

_Ret_maybenull_
PR_LAYOUT_ITEM _r_layout_additem (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ PR_LAYOUT_ITEM parent_item, _In_ HWND hwnd, _In_ ULONG flags);

VOID _r_layout_resizeitem (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item);
BOOLEAN _r_layout_resize (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ WPARAM wparam);

VOID _r_layout_setitemanchor (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item, _In_ ULONG anchor);
BOOLEAN _r_layout_setitemsanchorbyhandle (_In_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND hwnd, _In_ ULONG anchor);

FORCEINLINE VOID _r_layout_resizeminimumsize (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ LPARAM lparam)
{
	PMINMAXINFO mmi;

	if (!layout_manager->is_initialized)
		return;

	mmi = (PMINMAXINFO)lparam;

	mmi->ptMinTrackSize = layout_manager->original_size;
}

FORCEINLINE VOID _r_layout_setoriginalsize (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ LONG width, _In_ LONG height)
{
	layout_manager->original_size.x = width;
	layout_manager->original_size.y = height;
}

//
// Window management
//

VOID _r_wnd_addstyle (_In_ HWND hwnd, _In_opt_ INT ctrl_id, _In_ LONG_PTR mask, _In_ LONG_PTR state_mask, _In_ INT index);
VOID _r_wnd_adjustworkingarea (_In_opt_ HWND hwnd, _Inout_ PR_RECTANGLE rectangle);
VOID _r_wnd_calculateoverlappedrect (_In_ HWND hwnd, _Inout_ PRECT window_rect);
VOID _r_wnd_center (_In_ HWND hwnd, _In_opt_ HWND hparent);
VOID _r_wnd_changemessagefilter (_In_ HWND hwnd, _In_reads_ (count) PUINT messages, _In_ SIZE_T count, _In_ ULONG action);
VOID _r_wnd_changesettings (_In_ HWND hwnd, _In_opt_ WPARAM wparam, _In_opt_ LPARAM lparam);
BOOLEAN _r_wnd_isfullscreenmode ();
BOOLEAN _r_wnd_isoverlapped (_In_ HWND hwnd);
BOOLEAN _r_wnd_isundercursor (_In_ HWND hwnd);
VOID _r_wnd_setposition (_In_ HWND hwnd, _In_opt_ PR_SIZE position, _In_opt_ PR_SIZE size);
VOID _r_wnd_toggle (_In_ HWND hwnd, _In_ BOOLEAN is_show);

_Success_ (return)
BOOLEAN _r_wnd_getposition (_In_ HWND hwnd, _Out_ PR_RECTANGLE rectangle);

FORCEINLINE LONG_PTR _r_wnd_getstyle (_In_ HWND hwnd)
{
	return GetWindowLongPtr (hwnd, GWL_STYLE);
}

FORCEINLINE LONG_PTR _r_wnd_getstyle_ex (_In_ HWND hwnd)
{
	return GetWindowLongPtr (hwnd, GWL_EXSTYLE);
}

FORCEINLINE VOID _r_wnd_setstyle (_In_ HWND hwnd, _In_ LONG_PTR style)
{
	SetWindowLongPtr (hwnd, GWL_STYLE, style);
}

FORCEINLINE VOID _r_wnd_setstyle_ex (_In_ HWND hwnd, _In_ LONG_PTR exstyle)
{
	SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle);
}

FORCEINLINE VOID _r_wnd_adjustrectangletobounds (_Inout_ PR_RECTANGLE rectangle, _In_ PR_RECTANGLE bounds)
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

FORCEINLINE VOID _r_wnd_centerwindowrect (_In_ PR_RECTANGLE rectangle, _In_ PR_RECTANGLE bounds)
{
	rectangle->left = bounds->left + (bounds->width - rectangle->width) / 2;
	rectangle->top = bounds->top + (bounds->height - rectangle->height) / 2;
}

FORCEINLINE VOID _r_wnd_setrectangle (_Out_ PR_RECTANGLE rectangle, _In_ LONG left, _In_ LONG top, _In_ LONG width, _In_ LONG height)
{
	rectangle->left = left;
	rectangle->top = top;
	rectangle->width = width;
	rectangle->height = height;
}

FORCEINLINE VOID _r_wnd_recttorectangle (_Out_ PR_RECTANGLE rectangle, _In_ PRECT rect)
{
	_r_wnd_setrectangle (rectangle, rect->left, rect->top, _r_calc_rectwidth (rect), _r_calc_rectheight (rect));
}

FORCEINLINE VOID _r_wnd_rectangletorect (_Out_ PRECT rect, _In_ PR_RECTANGLE rectangle)
{
	SetRect (rect, rectangle->left, rectangle->top, rectangle->left + rectangle->width, rectangle->top + rectangle->height);
}

FORCEINLINE BOOLEAN _r_wnd_isdesktop (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8001); // #32769
}

FORCEINLINE BOOLEAN _r_wnd_isdialog (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8002); // #32770
}

FORCEINLINE BOOLEAN _r_wnd_ismenu (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8000); // #32768
}

FORCEINLINE BOOLEAN _r_wnd_ismaximized (_In_ HWND hwnd)
{
	return !!IsZoomed (hwnd);
}

FORCEINLINE BOOLEAN _r_wnd_isminimized (_In_ HWND hwnd)
{
	return !!IsIconic (hwnd);
}

FORCEINLINE BOOLEAN _r_wnd_isvisible (_In_ HWND hwnd)
{
	return !!IsWindowVisible (hwnd);
}

FORCEINLINE BOOLEAN _r_wnd_isvisiblefull (_In_ HWND hwnd)
{
	return _r_wnd_isvisible (hwnd) && !_r_wnd_isminimized (hwnd);
}

FORCEINLINE VOID _r_wnd_seticon (_In_ HWND hwnd, _In_opt_ HICON hicon_small, _In_opt_ HICON hicon_big)
{
	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon_small);
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon_big);
}

FORCEINLINE VOID _r_wnd_top (_In_ HWND hwnd, _In_ BOOLEAN is_enable)
{
	SetWindowPos (hwnd, is_enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

//
// Inernet access (WinHTTP)
//

_Ret_maybenull_
HINTERNET _r_inet_createsession (_In_opt_ LPCWSTR useragent);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Out_ LPHINTERNET pconnect, _Out_ LPHINTERNET prequest, _Out_opt_ PULONG total_length);

_Success_ (return)
BOOLEAN _r_inet_readrequest (_In_ HINTERNET hrequest, _Out_writes_bytes_ (buffer_size) PVOID buffer, _In_ ULONG buffer_size, _Out_opt_ PULONG readed, _Inout_opt_ PULONG total_readed);

FORCEINLINE VOID _r_inet_initializedownload (_Out_ PR_DOWNLOAD_INFO pdi, _In_opt_ HANDLE hfile, _In_opt_ PR_INET_DOWNLOAD_FUNCTION download_callback, _In_opt_ PVOID data)
{
	pdi->hfile = hfile;
	pdi->string = NULL;
	pdi->download_callback = download_callback;
	pdi->data = data;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_begindownload (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Inout_ PR_DOWNLOAD_INFO pdi);

VOID _r_inet_destroydownload (_Inout_ PR_DOWNLOAD_INFO pdi);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_queryurlparts (_In_ LPCWSTR url, _Out_ PR_URLPARTS url_parts, _In_ ULONG flags);

VOID _r_inet_destroyurlparts (_Inout_ PR_URLPARTS url_parts);

FORCEINLINE VOID _r_inet_close (_In_ HINTERNET handle)
{
	WinHttpCloseHandle (handle);
}

//
// Registry
//

_Ret_maybenull_
PR_BYTE _r_reg_querybinary (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);

_Ret_maybenull_
PR_STRING _r_reg_querystring (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);

ULONG _r_reg_queryulong (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_ LPCWSTR value);
ULONG64 _r_reg_queryulong64 (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_ LPCWSTR value);

ULONG _r_reg_querysubkeylength (_In_ HKEY hkey);
LONG64 _r_reg_querytimestamp (_In_ HKEY hkey);

//
// Math
//

ULONG _r_math_exponentiate (_In_ ULONG base, _In_ ULONG exponent);
ULONG64 _r_math_exponentiate64 (_In_ ULONG64 base, _In_ ULONG exponent);
ULONG _r_math_getrandom ();
SIZE_T _r_math_rounduptopoweroftwo (_In_ SIZE_T number);

_Success_ (return == ERROR_SUCCESS)
FORCEINLINE LONG _r_math_createguid (_Out_ LPGUID guid)
{
	return UuidCreate (guid);
}

FORCEINLINE ULONG _r_math_getrandomrange (_In_ ULONG min_number, _In_ ULONG max_number)
{
	return min_number + (_r_math_getrandom () % (max_number - min_number + 1));
}

FORCEINLINE ULONG _r_math_hashinteger32 (_In_ ULONG value)
{
	// Java style.
	value ^= (value >> 20) ^ (value >> 12);

	return value ^ (value >> 7) ^ (value >> 4);
}

FORCEINLINE ULONG _r_math_hashinteger64 (_In_ ULONG64 value)
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

FORCEINLINE ULONG _r_math_hashinteger_ptr (_In_ ULONG_PTR value)
{
#ifdef _WIN64
	return _r_math_hashinteger64 (value);
#else
	return _r_math_hashinteger32 (value);
#endif
}

//
// Resources
//

_Success_ (return != NULL)
PVOID _r_res_loadresource (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ LPCWSTR type, _Out_opt_ PULONG buffer_size);

_Ret_maybenull_
PR_STRING _r_res_loadstring (_In_opt_ HINSTANCE hinst, _In_ UINT string_id);

_Ret_maybenull_
PR_STRING _r_res_querystring (_In_ PVOID block, _In_ LPCWSTR entry_name, _In_ UINT lang_id, _In_ UINT code_page);

_Success_ (return)
BOOLEAN _r_res_querytranslation (_In_ PVOID block, _Out_ PUINT lang_id, _Out_ PUINT code_page);

_Ret_maybenull_
PR_STRING _r_res_queryversionstring (_In_ LPCWSTR path);

_Success_ (return)
FORCEINLINE BOOLEAN _r_res_queryversion (_In_ PVOID block, _Out_ VS_FIXEDFILEINFO **ver_info)
{
	UINT length;

	return !!VerQueryValue (block, L"\\", ver_info, &length);
}

//
// Other
//

_Ret_maybenull_
HICON _r_loadicon (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR icon_name, _In_ LONG icon_size, _In_ BOOLEAN is_shared);

PR_HASHTABLE _r_parseini (_In_ LPCWSTR path, _Inout_opt_ PR_LIST section_list);

VOID _r_sleep (_In_ ULONG milliseconds);

//
// Xml library
//

_Success_ (return == S_OK)
HRESULT _r_xml_initializelibrary (_Out_ PR_XML_LIBRARY xml_library, _In_ BOOLEAN is_reader, _In_opt_ PR_XML_STREAM_CALLBACK stream_callback);

_Success_ (return == S_OK)
HRESULT _r_xml_parsefile (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR file_path);

_Success_ (return == S_OK)
HRESULT _r_xml_parsestring (_Inout_ PR_XML_LIBRARY xml_library, _In_ PVOID buffer, _In_ ULONG buffer_size);

_Success_ (return)
BOOLEAN _r_xml_getattribute (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name, _Out_ PR_STRINGREF value);

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);

BOOLEAN _r_xml_getattribute_boolean (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);
INT _r_xml_getattribute_integer (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);
LONG64 _r_xml_getattribute_long64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);
ULONG64 _r_xml_getattribute_ulong64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);

FORCEINLINE VOID _r_xml_setattribute (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ LPCWSTR value)
{
	IXmlWriter_WriteAttributeString (xml_library->writer, NULL, name, NULL, value);
}

FORCEINLINE VOID _r_xml_setattribute_boolean (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ BOOLEAN value)
{
	_r_xml_setattribute (xml_library, name, value ? L"true" : L"false");
}

VOID _r_xml_setattribute_integer (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ INT value);
VOID _r_xml_setattribute_long64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ LONG64 value);
VOID _r_xml_setattribute_ulong64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name, _In_ ULONG64 value);

_Success_ (return)
BOOLEAN _r_xml_enumchilditemsbytagname (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR tag_name);

_Success_ (return)
BOOLEAN _r_xml_findchildbytagname (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR tag_name);

_Success_ (return == S_OK)
HRESULT _r_xml_resetlibrarystream (_Inout_ PR_XML_LIBRARY xml_library);

_Success_ (return == S_OK)
HRESULT _r_xml_setlibrarystream (_Inout_ PR_XML_LIBRARY xml_library, _In_ PR_XML_STREAM stream);

_Success_ (return == S_OK)
FORCEINLINE HRESULT _r_xml_createfile (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR file_path)
{
	return _r_xml_parsefile (xml_library, file_path);
}

FORCEINLINE VOID _r_xml_writewhitespace (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR whitespace)
{
	IXmlWriter_WriteWhitespace (xml_library->writer, whitespace);
}

FORCEINLINE VOID _r_xml_writestartdocument (_Inout_ PR_XML_LIBRARY xml_library)
{
	IXmlWriter_WriteStartDocument (xml_library->writer, XmlStandalone_Omit);
}

FORCEINLINE VOID _r_xml_writeenddocument (_Inout_ PR_XML_LIBRARY xml_library)
{
	IXmlWriter_WriteEndDocument (xml_library->writer);

	IXmlWriter_Flush (xml_library->writer);
}

FORCEINLINE VOID _r_xml_writestartelement (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR name)
{
	IXmlWriter_WriteStartElement (xml_library->writer, NULL, name, NULL);
}

FORCEINLINE VOID _r_xml_writeendelement (_Inout_ PR_XML_LIBRARY xml_library)
{
	IXmlWriter_WriteEndElement (xml_library->writer);
}

VOID _r_xml_destroylibrary (_Inout_ PR_XML_LIBRARY xml_library);

//
// System tray
//

BOOLEAN _r_tray_create (_In_ HWND hwnd, _In_ LPCGUID guid, _In_ UINT code, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip, _In_ BOOLEAN is_hidden);
BOOLEAN _r_tray_popup (_In_ HWND hwnd, _In_ LPCGUID guid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_opt_ LPCWSTR text);
BOOLEAN _r_tray_popupformat (_In_ HWND hwnd, _In_ LPCGUID guid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...);
BOOLEAN _r_tray_setinfo (_In_ HWND hwnd, _In_ LPCGUID guid, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip);
BOOLEAN _r_tray_setinfoformat (_In_ HWND hwnd, _In_ LPCGUID guid, _In_opt_ HICON hicon, _In_ _Printf_format_string_ LPCWSTR format, ...);
BOOLEAN _r_tray_toggle (_In_ HWND hwnd, _In_ LPCGUID guid, _In_ BOOLEAN is_show);
BOOLEAN _r_tray_destroy (_In_ HWND hwnd, _In_ LPCGUID guid);

//
// Control: common
//

BOOLEAN _r_ctrl_isenabled (_In_ HWND hwnd, _In_ INT ctrl_id);
INT _r_ctrl_isradiobuttonchecked (_In_ HWND hwnd, _In_ INT start_id, _In_ INT end_id);

LONG64 _r_ctrl_getinteger (_In_ HWND hwnd, _In_ INT ctrl_id, _Out_opt_ PULONG base);

_Ret_maybenull_
PR_STRING _r_ctrl_gettext (_In_ HWND hwnd, _In_ INT ctrl_id);

LONG _r_ctrl_getwidth (_In_ HWND hwnd, _In_opt_ INT ctrl_id);

VOID _r_ctrl_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...);

VOID _r_ctrl_setbuttonmargins (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _r_ctrl_settabletext (_In_ HWND hwnd, _In_ INT ctrl_id1, _In_ PR_STRINGREF text1, _In_ INT ctrl_id2, _In_ PR_STRINGREF text2);
VOID _r_ctrl_settextlength (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ PR_STRINGREF text);

_Ret_maybenull_
HWND _r_ctrl_createtip (_In_opt_ HWND hparent);
VOID _r_ctrl_settiptext (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ LPCWSTR text);
VOID _r_ctrl_settiptextformat (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...);
VOID _r_ctrl_settipstyle (_In_ HWND htip);

VOID _r_ctrl_showballoontip (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ LPCWSTR text);
VOID _r_ctrl_showballoontipformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...);

FORCEINLINE VOID _r_ctrl_enable (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOLEAN is_enable)
{
	HWND hctrl = GetDlgItem (hwnd, ctrl_id);

	if (hctrl)
		EnableWindow (hctrl, is_enable);
}

FORCEINLINE ULONG _r_ctrl_gettextlength (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);
}

FORCEINLINE VOID _r_ctrl_settext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ LPCWSTR text)
{
	SetDlgItemText (hwnd, ctrl_id, text);
}

//
// Control: combobox
//

FORCEINLINE VOID _r_combobox_clear (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_RESETCONTENT, 0, 0);
}

FORCEINLINE INT _r_combobox_getcurrentitem (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0);
}

FORCEINLINE LPARAM _r_combobox_getitemparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	return SendDlgItemMessage (hwnd, ctrl_id, CB_GETITEMDATA, (WPARAM)item_id, 0);
}

FORCEINLINE VOID _r_combobox_insertitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ LPCWSTR string)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, (WPARAM)item_id, (LPARAM)string);
}

FORCEINLINE VOID _r_combobox_setcurrentitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, (WPARAM)item_id, 0);
}

FORCEINLINE VOID _r_combobox_setitemparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ LPARAM lparam)
{
	SendDlgItemMessage (hwnd, ctrl_id, CB_SETITEMDATA, (WPARAM)item_id, lparam);
}

//
// Control: editbox
//

FORCEINLINE VOID _r_edit_settextlimit (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ SIZE_T length)
{
	SendDlgItemMessage (hwnd, ctrl_id, EM_LIMITTEXT, (WPARAM)length, 0);
}

//
// Control: menu
//

VOID _r_menu_checkitem (_In_ HMENU hmenu, _In_ UINT item_id_start, _In_opt_ UINT item_id_end, _In_ UINT position_flag, _In_ UINT check_id);
VOID _r_menu_clearitems (_In_ HMENU hmenu);
VOID _r_menu_setitembitmap (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ HBITMAP hbitmap);
VOID _r_menu_setitemtext (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ LPCWSTR text);
VOID _r_menu_setitemtextformat (_In_ HMENU hmenu, _In_ UINT item_id, _In_ BOOL is_byposition, _In_ _Printf_format_string_ LPCWSTR format, ...);
INT _r_menu_popup (_In_ HMENU hmenu, _In_ HWND hwnd, _In_opt_ PPOINT point, _In_ BOOLEAN is_sendmessage);

FORCEINLINE VOID _r_menu_enableitem (_In_ HMENU hmenu, _In_ UINT item_id, _In_ UINT position_flag, _In_ BOOLEAN is_enable)
{
	EnableMenuItem (hmenu, item_id, position_flag | (is_enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

//
// Control: tab
//

VOID _r_tab_adjustchild (_In_ HWND hwnd, _In_ INT tab_id, _In_ HWND hchild);
INT _r_tab_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_opt_ LPCWSTR text, _In_ INT image_id, _In_opt_ LPARAM lparam);
LPARAM _r_tab_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);
INT _r_tab_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_opt_ LPCWSTR text, _In_ INT image_id, _In_opt_ LPARAM lparam);
VOID _r_tab_selectitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);

FORCEINLINE INT _r_tab_getcurrentitem (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETCURSEL, 0, 0);
}

FORCEINLINE INT _r_tab_getitemcount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEMCOUNT, 0, 0);
}

//
// Control: listview
//

INT _r_listview_addcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR title, _In_opt_ INT width, _In_opt_ INT fmt);
INT _r_listview_addgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT align, _In_opt_ UINT state, _In_opt_ UINT state_mask);
INT _r_listview_additemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_opt_ LPCWSTR text, _In_ INT image_id, _In_ INT group_id, _In_opt_ LPARAM lparam);

_Success_ (return != -1)
INT _r_listview_finditem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT start_pos, _In_ LPARAM lparam);

VOID _r_listview_deleteallcolumns (_In_ HWND hwnd, _In_ INT ctrl_id);

INT _r_listview_getcolumncount (_In_ HWND hwnd, _In_ INT ctrl_id);

_Ret_maybenull_
PR_STRING _r_listview_getcolumntext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id);

INT _r_listview_getcolumnwidth (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id);
INT _r_listview_getitemcheckedcount (_In_ HWND hwnd, _In_ INT ctrl_id);
INT _r_listview_getitemgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);
LPARAM _r_listview_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);

_Ret_maybenull_
PR_STRING _r_listview_getitemtext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ INT subitem_id);

VOID _r_listview_redraw (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);

VOID _r_listview_setcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR text, _In_opt_ INT width);
VOID _r_listview_setcolumnsortindex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_ INT);
VOID _r_listview_setitemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ INT subitem_id, _In_opt_ LPCWSTR text, _In_ INT image_id, _In_ INT group_id, _In_opt_ LPARAM lparam);
VOID _r_listview_setitemcheck (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ BOOLEAN is_check);
VOID _r_listview_setitemvisible (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);
VOID _r_listview_setgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT state, _In_opt_ UINT state_mask);
VOID _r_listview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_ BOOL is_groupview);

FORCEINLINE INT _r_listview_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ LPCWSTR text)
{
	return _r_listview_additemex (hwnd, ctrl_id, item_id, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

FORCEINLINE VOID _r_listview_deleteallgroups (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteallitems (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEITEM, (WPARAM)item_id, 0);
}

FORCEINLINE ULONG _r_listview_getexstyle (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
}

FORCEINLINE INT _r_listview_getitemcount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, 0);
}

FORCEINLINE INT _r_listview_getselectedcount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0);
}

FORCEINLINE ULONG _r_listview_getview (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETVIEW, 0, 0);
}

FORCEINLINE BOOLEAN _r_listview_isgroupviewenabled (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_ISGROUPVIEWENABLED, 0, 0));
}

FORCEINLINE BOOLEAN _r_listview_isitemchecked (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item_id, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2));
}

FORCEINLINE BOOLEAN _r_listview_isitemselected (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item_id, LVNI_SELECTED) == LVNI_SELECTED);
}

FORCEINLINE BOOLEAN _r_listview_isitemvisible (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, (WPARAM)item_id, 0));
}

FORCEINLINE VOID _r_listview_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id, _In_ INT subitem_id, _In_opt_ LPCWSTR text)
{
	_r_listview_setitemex (hwnd, ctrl_id, item_id, subitem_id, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

FORCEINLINE VOID _r_listview_setimagelist (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HIMAGELIST himg)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
}

FORCEINLINE VOID _r_listview_setview (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT view_type)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETVIEW, (WPARAM)view_type, 0);
}

//
// Control: treeview
//

HTREEITEM _r_treeview_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ LPCWSTR text, _In_opt_ HTREEITEM hparent, _In_ INT image_id, _In_opt_ LPARAM lparam);
LPARAM _r_treeview_getlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem);
VOID _r_treeview_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam);
VOID _r_treeview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_opt_ INT height, _In_opt_ INT indent);

//
// Control: statusbar
//

LONG _r_status_getheight (_In_ HWND hwnd, _In_ INT ctrl_id);

VOID _r_status_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ INT height);
VOID _r_status_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part_id, _In_ _Printf_format_string_ LPCWSTR format, ...);

FORCEINLINE VOID _r_status_settext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part_id, _In_opt_ LPCWSTR text)
{
	SendDlgItemMessage (hwnd, ctrl_id, SB_SETTEXT, MAKEWPARAM (part_id, 0), (LPARAM)text);
}

//
// Control: rebar
//

FORCEINLINE LONG _r_rebar_getheight (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (LONG)SendDlgItemMessage (hwnd, ctrl_id, RB_GETBARHEIGHT, 0, 0);
}

//
// Control: toolbar
//

VOID _r_toolbar_addbutton (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id, _In_ INT style, _In_opt_ INT_PTR text, _In_ INT state, _In_ INT image);
INT _r_toolbar_getwidth (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _r_toolbar_setbutton (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id, _In_opt_ LPCWSTR text, _In_opt_ INT style, _In_opt_ INT state, _In_ INT image);
VOID _r_toolbar_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle);

FORCEINLINE VOID _r_toolbar_addseparator (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	_r_toolbar_addbutton (hwnd, ctrl_id, 0, BTNS_SEP, 0, 0, I_IMAGENONE);
}

FORCEINLINE VOID _r_toolbar_enablebutton (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id, _In_ BOOLEAN is_enable)
{
	SendDlgItemMessage (hwnd, ctrl_id, TB_ENABLEBUTTON, (WPARAM)command_id, MAKELPARAM (is_enable, 0));
}

FORCEINLINE INT _r_toolbar_getbuttoncount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TB_BUTTONCOUNT, 0, 0);
}

FORCEINLINE BOOLEAN _r_toolbar_isbuttonenabled (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ UINT command_id)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, TB_ISBUTTONENABLED, (WPARAM)command_id, 0));
}

FORCEINLINE VOID _r_toolbar_resize (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, TB_AUTOSIZE, 0, 0);
}

//
// Control: progress bar
//

VOID _r_progress_setmarquee (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOL is_enable);

//
// Util
//

FORCEINLINE VOID _r_util_templatewrite (_Inout_ PBYTE * ptr, _In_bytecount_ (size) LPCVOID data, _In_ SIZE_T size)
{
	RtlCopyMemory (*ptr, data, size);
	*ptr += size;
}

FORCEINLINE VOID _r_util_templatewriteulong (_Inout_ PBYTE * ptr, _In_ ULONG data)
{
	_r_util_templatewrite (ptr, &data, sizeof (data));
}

FORCEINLINE VOID _r_util_templatewriteshort (_Inout_ PBYTE * ptr, _In_ WORD data)
{
	_r_util_templatewrite (ptr, &data, sizeof (data));
}

VOID _r_util_templatewritestring (_Inout_ PBYTE * ptr, _In_ LPCWSTR string);
VOID _r_util_templatewritecontrol (_Inout_ PBYTE * ptr, _In_ ULONG ctrl_id, _In_ ULONG style, _In_ SHORT x, _In_ SHORT y, _In_ SHORT cx, _In_ SHORT cy, _In_ LPCWSTR class_name);
PR_STRING _r_util_versionformat (_In_ PR_STRINGREF string);
BOOL CALLBACK _r_util_activate_window_callback (_In_ HWND hwnd, _In_ LPARAM lparam);

