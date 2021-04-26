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
#include <commctrl.h>
#include <commdlg.h>
#include <dde.h>
#include <dwmapi.h>
#include <ntsecapi.h>
#include <psapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <smmintrin.h>
#include <subauth.h>
#include <taskschd.h>
#include <uxtheme.h>
#include <winhttp.h>
#include <wtsapi32.h>
#include <xmllite.h>

#include "app.h"
#include "ntapi.h"
#include "rconfig.h"

// libs
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "xmllite.lib")

// extern
_Check_return_
extern SIZE_T _r_str_length (_In_ LPCWSTR text);

_Check_return_
extern SIZE_T _r_str_hash (_In_ LPCWSTR string);

extern FORCEINLINE VOID _r_str_trim (_Inout_ LPWSTR string, _In_ LPCWSTR trim);

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
#define SAFE_DELETE_LIBRARY(p) {if(_r_fs_isvalidhandle ((p))) {FreeLibrary ((p)); (p)=NULL;}}
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

/*
	Synchronization
*/

FORCEINLINE BOOLEAN InterlockedBitTestAndSetPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *base, _In_ LONG_PTR bit)
{
#ifdef _WIN64
	return _interlockedbittestandset64 ((PLONG64)base, (LONG64)bit);
#else
	return _interlockedbittestandset ((PLONG)base, (LONG)bit);
#endif
}

FORCEINLINE LONG_PTR InterlockedExchangeAddPointer (_Inout_ _Interlocked_operand_ LONG_PTR volatile *addend, _In_ LONG_PTR value)
{
#ifdef _WIN64
	return (LONG_PTR)_InterlockedExchangeAdd64 ((PLONG64)addend, (LONG64)value);
#else
	return (LONG_PTR)_InterlockedExchangeAdd ((PLONG)addend, (LONG)value);
#endif
}

/*
	Synchronization: Spinlock
*/

//	FastLock is a port of FastResourceLock from PH 1.x.
//	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
//	1.x for details.
//	The fast lock is around 7% faster than the critical section when there is no contention, when
//	used solely for mutual exclusion. It is also much smaller than the critical section.
//	https://github.com/processhacker2/processhacker

#if defined(APP_NO_DEPRECATIONS)

#define RTL_SRWLOCK_OWNED_BIT 0
#define RTL_SRWLOCK_OWNED (1 << RTL_SRWLOCK_OWNED_BIT)

#define R_SPINLOCK RTL_SRWLOCK
#define PR_SPINLOCK PRTL_SRWLOCK

FORCEINLINE VOID _r_spinlock_initialize (PR_SPINLOCK spin_lock)
{
	RtlInitializeSRWLock (spin_lock);
}

FORCEINLINE VOID _r_spinlock_acquireexclusive (PR_SPINLOCK spin_lock)
{
	RtlAcquireSRWLockExclusive (spin_lock);
}

FORCEINLINE VOID _r_spinlock_acquireshared (PR_SPINLOCK spin_lock)
{
	RtlAcquireSRWLockShared (spin_lock);
}

FORCEINLINE VOID _r_spinlock_releaseexclusive (PR_SPINLOCK spin_lock)
{
	RtlReleaseSRWLockExclusive (spin_lock);
}

FORCEINLINE VOID _r_spinlock_releaseshared (PR_SPINLOCK spin_lock)
{
	RtlReleaseSRWLockShared (spin_lock);
}

FORCEINLINE BOOLEAN _r_spinlock_tryacquireexclusive (PR_SPINLOCK spin_lock)
{
	return RtlTryAcquireSRWLockExclusive (spin_lock);
}

FORCEINLINE BOOLEAN _r_spinlock_tryacquireshared (PR_SPINLOCK spin_lock)
{
	return RtlTryAcquireSRWLockShared (spin_lock);
}

#else

#define PR_SPINLOCK_OWNED 0x1
#define PR_SPINLOCK_EXCLUSIVE_WAKING 0x2

#define PR_SPINLOCK_SHARED_OWNERS_SHIFT 2
#define PR_SPINLOCK_SHARED_OWNERS_MASK 0x3ff
#define PR_SPINLOCK_SHARED_OWNERS_INC 0x4

#define PR_SPINLOCK_SHARED_WAITERS_SHIFT 12
#define PR_SPINLOCK_SHARED_WAITERS_MASK 0x3ff
#define PR_SPINLOCK_SHARED_WAITERS_INC 0x1000

#define PR_SPINLOCK_EXCLUSIVE_WAITERS_SHIFT 22
#define PR_SPINLOCK_EXCLUSIVE_WAITERS_MASK 0x3ff
#define PR_SPINLOCK_EXCLUSIVE_WAITERS_INC 0x400000

#define PR_SPINLOCK_EXCLUSIVE_MASK (PR_SPINLOCK_EXCLUSIVE_WAKING | (PR_SPINLOCK_EXCLUSIVE_WAITERS_MASK << PR_SPINLOCK_EXCLUSIVE_WAITERS_SHIFT))

typedef struct tagR_SPINLOCK
{
	volatile ULONG value;

	HANDLE exclusive_wake_event;
	HANDLE shared_wake_event;
} R_SPINLOCK, *PR_SPINLOCK;

FORCEINLINE ULONG _r_spinlock_getspincount ()
{
	return (NtCurrentPeb ()->NumberOfProcessors > 1) ? 4000 : 0;
}

FORCEINLINE VOID _r_spinlock_ensureeventcreated (PHANDLE phandle)
{
	HANDLE handle;

	if (*phandle != NULL)
		return;

	NtCreateSemaphore (&handle, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);

	if (InterlockedCompareExchangePointer (phandle, handle, NULL) != NULL)
		NtClose (handle);
}

VOID _r_spinlock_initialize (PR_SPINLOCK plock);

VOID _r_spinlock_acquireexclusive (PR_SPINLOCK plock);
VOID _r_spinlock_acquireshared (PR_SPINLOCK plock);

VOID _r_spinlock_releaseexclusive (PR_SPINLOCK plock);
VOID _r_spinlock_releaseshared (PR_SPINLOCK plock);

BOOLEAN _r_spinlock_tryacquireexclusive (PR_SPINLOCK plock);
BOOLEAN _r_spinlock_tryacquireshared (PR_SPINLOCK plock);

#endif // APP_NO_DEPRECATIONS

FORCEINLINE BOOLEAN _r_spinlock_islocked (const PR_SPINLOCK plock)
{
	BOOLEAN owned;

	// Need two memory barriers because we don't want the compiler re-ordering the following check
	// in either direction.
	MemoryBarrier ();

#if defined(APP_NO_DEPRECATIONS)
	owned = ((*(volatile LONG_PTR*)&plock->Ptr) & RTL_SRWLOCK_OWNED) != 0;
#else
	owned = (plock->value & PR_SPINLOCK_OWNED) != 0;
#endif // APP_NO_DEPRECATIONS

	MemoryBarrier ();

	return owned;
}

/*
	Synchronization: Timer
*/

#if defined(APP_NO_DEPRECATIONS)

#else

typedef struct tagR_EVENT
{
	union
	{
		ULONG_PTR value;
		struct
		{
			USHORT is_set : 1;
			USHORT ref_count : 15;
			UCHAR reserved;
			UCHAR available_for_use;
#ifdef _WIN64
			ULONG spare;
#endif
		};
	};
	HANDLE event_handle;
} R_EVENT, *PR_EVENT;

#define PR_EVENT_SET 0x1
#define PR_EVENT_SET_SHIFT 0
#define PR_EVENT_REFCOUNT_SHIFT 1
#define PR_EVENT_REFCOUNT_INC 0x2
#define PR_EVENT_REFCOUNT_MASK (((ULONG_PTR)1 << 15) - 1)

#define PR_EVENT_INIT { { PR_EVENT_REFCOUNT_INC }, NULL }

C_ASSERT (sizeof (R_EVENT) == sizeof (ULONG_PTR) + sizeof (HANDLE));

VOID FASTCALL _r_event_reset (_Inout_ PR_EVENT event);
VOID FASTCALL _r_event_set (_Inout_ PR_EVENT event);
BOOLEAN FASTCALL _r_event_waitex (_Inout_ PR_EVENT event, _In_opt_ PLARGE_INTEGER timeout);

FORCEINLINE VOID FASTCALL _r_event_intialize (_Out_ PR_EVENT event)
{
	event->value = PR_EVENT_REFCOUNT_INC;
	event->event_handle = NULL;
}

FORCEINLINE BOOLEAN _r_event_test (_In_ PR_EVENT event)
{
	return !!event->is_set;
}

FORCEINLINE VOID _r_event_reference (_Inout_ PR_EVENT event)
{
	InterlockedExchangeAddPointer ((PLONG_PTR)(&event->value), PR_EVENT_REFCOUNT_INC);
}

FORCEINLINE VOID _r_event_dereference (_Inout_ PR_EVENT event, _In_opt_ HANDLE event_handle)
{
	ULONG_PTR value;

	value = InterlockedExchangeAddPointer ((PLONG_PTR)(&event->value), -PR_EVENT_REFCOUNT_INC);

	if (((value >> PR_EVENT_REFCOUNT_SHIFT) & PR_EVENT_REFCOUNT_MASK) - 1 == 0)
	{
		if (event_handle)
		{
			NtClose (event_handle);

			event->event_handle = NULL;
		}
	}
}

FORCEINLINE BOOLEAN _r_event_wait (_Inout_ PR_EVENT event, _In_opt_ PLARGE_INTEGER timeout)
{
	if (_r_event_test (event))
		return TRUE;

	return _r_event_waitex (event, timeout);
}

#endif // APP_NO_DEPRECATIONS

/*
	Synchronization: One-time initialization
*/

#if defined(APP_NO_DEPRECATIONS)

#define R_INITONCE RTL_RUN_ONCE
#define PR_INITONCE PRTL_RUN_ONCE

#define PR_INITONCE_INIT RTL_RUN_ONCE_INIT

FORCEINLINE BOOLEAN _r_initonce_begin (PR_INITONCE init_once)
{
	if (NT_SUCCESS (RtlRunOnceBeginInitialize (init_once, RTL_RUN_ONCE_CHECK_ONLY, NULL)))
		return FALSE;

	return (RtlRunOnceBeginInitialize (init_once, 0, NULL) == STATUS_PENDING);
}

FORCEINLINE VOID _r_initonce_end (PR_INITONCE init_once)
{
	RtlRunOnceComplete (init_once, 0, NULL);
}

#else

typedef struct tagR_INITONCE
{
	R_EVENT event;
} R_INITONCE, *PR_INITONCE;

#define PR_INITONCE_SHIFT 31
#define PR_INITONCE_INITIALIZING (0x1 << PR_INITONCE_SHIFT)
#define PR_INITONCE_INITIALIZING_SHIFT PR_INITONCE_SHIFT

#define PR_INITONCE_INIT { PR_EVENT_INIT }

C_ASSERT (PR_INITONCE_SHIFT >= FIELD_OFFSET (R_EVENT, available_for_use) * 8);

BOOLEAN FASTCALL _r_initonce_beginex (_Inout_ PR_INITONCE init_once);

FORCEINLINE BOOLEAN _r_initonce_begin (_Inout_ PR_INITONCE init_once)
{
	if (_r_event_test (&init_once->event))
		return FALSE;

	return _r_initonce_beginex (init_once);
}

FORCEINLINE VOID FASTCALL _r_initonce_end (_Inout_ PR_INITONCE init_once)
{
	_r_event_set (&init_once->event);
}
#endif // APP_NO_DEPRECATIONS

/*
	Synchronization: Mutex
*/

BOOLEAN _r_mutex_create (_In_ LPCWSTR name, _Inout_ PHANDLE hmutex);
BOOLEAN _r_mutex_destroy (_Inout_ PHANDLE hmutex);
BOOLEAN _r_mutex_isexists (_In_ LPCWSTR name);

/*
	Memory allocation
*/

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
	return RtlAllocateHeap (_r_mem_getheap (), 0, bytes_count);
}

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocatezero (_In_ SIZE_T bytes_count)
{
	return RtlAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);
}

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_allocateandcopy (_In_ PVOID src, _In_ SIZE_T bytes_count)
{
	PVOID dst = RtlAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, bytes_count);

	memcpy (dst, src, bytes_count);

	return dst;
}

//	// If RtlReAllocateHeap fails, the original memory is not freed, and the original handle and pointer are still valid.
//	// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_reallocate (_Frees_ptr_opt_ PVOID memory_address, _In_ SIZE_T bytes_count)
{
	return RtlReAllocateHeap (_r_mem_getheap (), HEAP_GENERATE_EXCEPTIONS, memory_address, bytes_count);
}

_Ret_maybenull_
_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID NTAPI _r_mem_reallocatesafe (_Frees_ptr_opt_ PVOID memory_address, _In_ SIZE_T bytes_count)
{
	return RtlReAllocateHeap (_r_mem_getheap (), 0, memory_address, bytes_count);
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

/*
	Objects reference
*/

typedef VOID (NTAPI *PR_OBJECT_CLEANUP_FUNCTION) (_In_ PVOID object_body);

typedef struct tagR_OBJECT_HEADER
{
	struct
	{
		PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;

		volatile LONG ref_count;
	};

	QUAD_PTR body;
} R_OBJECT_HEADER, *PR_OBJECT_HEADER;

_Post_writable_byte_size_ (bytes_count)
PVOID _r_obj_allocateex (_In_ SIZE_T bytes_count, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

PVOID _r_obj_reference (_In_ PVOID object_body);
VOID _r_obj_dereferenceex (_In_ PVOID object_body, _In_ LONG ref_count);

_Post_writable_byte_size_ (bytes_count)
FORCEINLINE PVOID _r_obj_allocate (_In_ SIZE_T bytes_count)
{
	return _r_obj_allocateex (bytes_count, NULL);
}

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_referencesafe (_In_opt_ PVOID object_body)
{
	if (!object_body)
		return NULL;

	return _r_obj_reference (object_body);
}

FORCEINLINE VOID _r_obj_dereference (_In_ PVOID object_body)
{
	_r_obj_dereferenceex (object_body, 1);
}

FORCEINLINE VOID _r_obj_movereference (_Inout_ PVOID * object_body, _In_opt_ PVOID new_object)
{
	PVOID old_object;

	old_object = *object_body;
	*object_body = new_object;

	if (old_object)
		_r_obj_dereference (old_object);
}

FORCEINLINE VOID _r_obj_clearreference (_Inout_ PVOID * object_body)
{
	_r_obj_movereference (object_body, NULL);
}

/*
	8-bit string object
*/

typedef struct tagR_BYTEREF
{
	SIZE_T length;
	LPSTR buffer;
} R_BYTEREF, *PR_BYTEGREF;

typedef struct tagR_BYTE
{
	union
	{
		R_BYTEREF sr;
		struct
		{

			SIZE_T length;
			LPSTR buffer;
		};
	};

	CHAR data[1];
} R_BYTE, *PR_BYTE;

PR_BYTE _r_obj_createbyteex (_In_opt_ LPSTR buffer, _In_ SIZE_T length);

//_Check_return_
//FORCEINLINE BOOLEAN _r_obj_isbyteempty (_In_opt_ PR_BYTE string)
//{
//	return !string || !string->length || !string->buffer || (*string->buffer == ANSI_NULL);
//}

#define _r_obj_isbyteempty(string) \
    (!(string) || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == ANSI_NULL)

FORCEINLINE VOID _r_obj_writebytenullterminator (_In_ PR_BYTE string)
{
	*(LPSTR)PTR_ADD_OFFSET (string->buffer, string->length) = ANSI_NULL;
}

/*
	16-bit string object
*/

typedef struct tagR_STRINGREF
{
	SIZE_T length;
	LPWSTR buffer;
} R_STRINGREF, *PR_STRINGREF;

typedef struct tagR_STRING
{
	union
	{
		R_STRINGREF sr;
		struct
		{
			SIZE_T length;
			LPWSTR buffer;
		};
	};

	WCHAR data[1];
} R_STRING, *PR_STRING;

PR_STRING _r_obj_createstringex (_In_opt_ LPCWSTR buffer, _In_ SIZE_T length);

FORCEINLINE PR_STRING _r_obj_createstring (_In_ LPCWSTR string)
{
	return _r_obj_createstringex (string, _r_str_length (string) * sizeof (WCHAR));
}

FORCEINLINE PR_STRING _r_obj_createstringfromstring (_In_ PR_STRING string)
{
	return _r_obj_createstringex (string->buffer, string->length);
}

FORCEINLINE PR_STRING _r_obj_createstringfromstringref (_In_ PR_STRINGREF string)
{
	return _r_obj_createstringex (string->buffer, string->length);
}

_Ret_maybenull_
FORCEINLINE PR_STRING _r_obj_createstringfromunicodestring (_In_ PUNICODE_STRING us)
{
	return _r_obj_createstringex (us->Buffer, us->Length);
}

FORCEINLINE BOOLEAN _r_obj_createunicodestringfromstring (_Out_ PUNICODE_STRING us, _In_ PR_STRING string)
{
	us->Length = (USHORT)string->length;
	us->MaximumLength = (USHORT)string->length + sizeof (UNICODE_NULL);
	us->Buffer = string->buffer;

	return string->length <= UNICODE_STRING_MAX_BYTES;
}

//_Check_return_
//FORCEINLINE BOOLEAN _r_obj_isstringempty (_In_opt_ PR_STRING string)
//{
//	return !string || !string->length || !string->buffer || (string->buffer[0] == UNICODE_NULL);
//}

#define _r_obj_isstringempty(string) \
    (!(string) || (string)->length == 0 || (string)->buffer == NULL || (string)->buffer[0] == UNICODE_NULL)

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

_Check_return_
FORCEINLINE SIZE_T _r_obj_getstringlength (_In_opt_ PR_STRING string)
{
	if (string)
		return string->length / sizeof (WCHAR);

	return 0;
}

_Check_return_
FORCEINLINE SIZE_T _r_obj_getstringsize (_In_opt_ PR_STRING string)
{
	if (string)
		return string->length;

	return 0;
}

_Check_return_
FORCEINLINE SIZE_T _r_obj_getstringhash (_In_opt_ PR_STRING string)
{
	if (string)
		return _r_str_hash (string->buffer);

	return 0;
}

VOID _r_obj_removestring (_In_ PR_STRING string, _In_ SIZE_T start_pos, _In_ SIZE_T length);

FORCEINLINE VOID _r_obj_writestringnullterminator (_In_ PR_STRING string)
{
	assert (!(string->length & 0x01));

	*(LPWSTR)PTR_ADD_OFFSET (string->buffer, string->length) = UNICODE_NULL;
}

FORCEINLINE VOID _r_obj_setstringsize (_Inout_ PR_STRING string, _In_ SIZE_T length)
{
	if (string->length <= length)
		return;

	if (length & 0x01)
		length += 1;

	string->length = length;
	_r_obj_writestringnullterminator (string);
}

FORCEINLINE VOID _r_obj_trimstringtonullterminator (_In_ PR_STRING string)
{
	string->length = _r_str_length (string->buffer) * sizeof (WCHAR);

	_r_obj_writestringnullterminator (string); // terminate
}

FORCEINLINE VOID _r_obj_trimstring (_Inout_ PR_STRING string, _In_ LPCWSTR trim)
{
	if (!string)
		return;

	_r_str_trim (string->buffer, trim);

	_r_obj_trimstringtonullterminator (string);
}

/*
	String builder
*/

typedef struct tagR_STRINGBUILDER
{
	SIZE_T allocated_length;
	PR_STRING string;
} R_STRINGBUILDER, *PR_STRINGBUILDER;

FORCEINLINE VOID _r_obj_initializestringbuilder (_Out_ PR_STRINGBUILDER string)
{
	string->allocated_length = 512;

	string->string = _r_obj_createstringex (NULL, string->allocated_length);

	string->string->length = 0;
	string->string->buffer[0] = UNICODE_NULL;
}

FORCEINLINE VOID _r_obj_deletestringbuilder (_Inout_ PR_STRINGBUILDER string)
{
	if (string->string)
		_r_obj_clearreference (&string->string);
}

FORCEINLINE PR_STRING _r_obj_finalstringbuilder (_In_ PR_STRINGBUILDER string)
{
	return string->string;
}

VOID _r_obj_appendstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ LPCWSTR text, _In_ SIZE_T length);
VOID _r_obj_appendstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);
VOID _r_obj_insertstringbuilderex (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ LPCWSTR text, _In_ SIZE_T length);
VOID _r_obj_insertstringbuilderformat_v (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);
VOID _r_obj_resizestringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T new_capacity);

FORCEINLINE VOID _r_obj_appendstringbuilder (_Inout_ PR_STRINGBUILDER string, _In_ LPCWSTR text)
{
	_r_obj_appendstringbuilderex (string, text, _r_str_length (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_appendstringbuilder2 (_Inout_ PR_STRINGBUILDER string, _In_ PR_STRING text)
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
	_r_obj_insertstringbuilderex (string, index, text, _r_str_length (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_insertstringbuilder2 (_Inout_ PR_STRINGBUILDER string, _In_ SIZE_T index, _In_ PR_STRING text)
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

/*
	Array object
*/

typedef struct tagR_ARRAY
{
	PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
	SIZE_T allocated_count;
	SIZE_T count;
	SIZE_T item_size;
	PVOID items;
} R_ARRAY, *PR_ARRAY;

PR_ARRAY _r_obj_createarrayex (_In_ SIZE_T item_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);

_Ret_maybenull_
PVOID _r_obj_getarrayitem (_In_opt_ PR_ARRAY array_node, _In_ SIZE_T index);

VOID _r_obj_cleararray (_Inout_ PR_ARRAY array_node);
VOID _r_obj_resizearray (_Inout_ PR_ARRAY array_node, _In_ SIZE_T new_capacity);

_Success_ (return != SIZE_MAX)
SIZE_T _r_obj_addarrayitem (_Inout_ PR_ARRAY array_node, _In_ PVOID item);

VOID _r_obj_addarrayitems (_Inout_ PR_ARRAY array_node, _In_ PVOID items, _In_ SIZE_T count);
VOID _r_obj_removearrayitems (_Inout_ PR_ARRAY array_node, _In_ SIZE_T start_pos, _In_ SIZE_T count);

FORCEINLINE PR_ARRAY _r_obj_createarray (_In_ SIZE_T item_size, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createarrayex (item_size, 10, cleanup_callback);
}

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_getarrayitem (_In_opt_ PR_ARRAY array_node, _In_ SIZE_T index)
{
	if (array_node)
		return PTR_ADD_OFFSET (array_node->items, index * array_node->item_size);

	return NULL;
}

_Check_return_
FORCEINLINE SIZE_T _r_obj_getarraysize (_In_opt_ PR_ARRAY array_node)
{
	if (array_node)
		return array_node->count;

	return 0;
}

//_Check_return_
//FORCEINLINE BOOLEAN _r_obj_isarrayempty (_In_opt_ PR_ARRAY array_node)
//{
//	return !array_node || array_node->count == 0;
//}

#define _r_obj_isarrayempty(array_node) \
    (!(array_node) || (array_node)->count == 0)

FORCEINLINE VOID _r_obj_removearrayitem (_In_ PR_ARRAY array_node, _In_ SIZE_T index)
{
	_r_obj_removearrayitems (array_node, index, 1);
}

/*
	List object
*/

typedef struct tagR_LIST
{
	PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
	SIZE_T allocated_count;
	SIZE_T count;
	PVOID *items;
} R_LIST, *PR_LIST;

PR_LIST _r_obj_createlistex (_In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);
SIZE_T _r_obj_addlistitem (_Inout_ PR_LIST list_node, _In_ PVOID item);
VOID _r_obj_clearlist (_Inout_ PR_LIST list_node);
SIZE_T _r_obj_findlistitem (_In_ PR_LIST list_node, _In_ PVOID list_item);
VOID _r_obj_insertlistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ PVOID * items, _In_ SIZE_T count);
VOID _r_obj_removelistitems (_Inout_ PR_LIST list_node, _In_ SIZE_T start_pos, _In_ SIZE_T count);
VOID _r_obj_resizelist (_Inout_ PR_LIST list_node, _In_ SIZE_T new_capacity);

FORCEINLINE PR_LIST _r_obj_createlist (_In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createlistex (10, cleanup_callback);
}

_Ret_maybenull_
FORCEINLINE PVOID _r_obj_getlistitem (_In_opt_ PR_LIST list_node, _In_ SIZE_T index)
{
	if (list_node)
		return list_node->items[index];

	return NULL;
}

_Check_return_
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

//_Check_return_
//FORCEINLINE BOOLEAN _r_obj_islistempty (_In_opt_ PR_LIST list_node)
//{
//	return !list_node || list_node->count == 0;
//}

#define _r_obj_islistempty(list_node) \
    (!(list_node) || (list_node)->count == 0)

/*
	Hashtable
*/

typedef struct tagR_HASHTABLE_ENTRY
{
	SIZE_T next;
	SIZE_T hash_code;

	QUAD_PTR body;
} R_HASHTABLE_ENTRY, *PR_HASHTABLE_ENTRY;

typedef struct tagR_HASHTABLE
{
	PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
	PSIZE_T buckets;
	PVOID entries;
	SIZE_T free_entry;
	SIZE_T next_entry;
	SIZE_T entry_size;
	SIZE_T allocated_buckets;
	SIZE_T allocated_entries;
	SIZE_T count;
} R_HASHTABLE, *PR_HASHTABLE;

PR_HASHTABLE _r_obj_createhashtableex (_In_ SIZE_T entry_size, _In_ SIZE_T initial_capacity, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback);
VOID _r_obj_resizehashtable (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T new_capacity);

_Ret_maybenull_
PVOID _r_obj_addhashtableitem (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code, _In_opt_ PVOID entry);

VOID _r_obj_clearhashtable (_Inout_ PR_HASHTABLE hashtable);

_Success_ (return)
BOOLEAN _r_obj_enumhashtable (_In_ PR_HASHTABLE hashtable, _Outptr_ PVOID * entry, _Out_opt_ PSIZE_T hash_code, _Inout_ PSIZE_T enum_key);

_Ret_maybenull_
PVOID _r_obj_findhashtable (_In_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code);

BOOLEAN _r_obj_removehashtableentry (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code);

FORCEINLINE PR_HASHTABLE _r_obj_createhashtable (_In_ SIZE_T entry_size, _In_opt_ PR_OBJECT_CLEANUP_FUNCTION cleanup_callback)
{
	return _r_obj_createhashtableex (entry_size, 10, cleanup_callback);
}

_Check_return_
FORCEINLINE SIZE_T _r_obj_gethashtablesize (_In_opt_ PR_HASHTABLE hashtable)
{
	if (hashtable)
		return hashtable->count;

	return 0;
}

//_Check_return_
//FORCEINLINE SIZE_T _r_obj_ishashtableempty (_In_opt_ PR_HASHTABLE hashtable)
//{
//	return !hashtable || hashtable->count == 0;
//}

#define _r_obj_ishashtableempty(hashtable) \
    (!(hashtable) || (hashtable)->count == 0)

/*
	Hashtable: store
*/

typedef struct tagR_HASHSTORE
{
	PR_STRING value_string;
	LONG value_number;
} R_HASHSTORE, *PR_HASHSTORE;

FORCEINLINE VOID _r_obj_initializehashstore (_Out_ PR_HASHSTORE hashstore, _In_opt_ PR_STRING string, _In_opt_ LONG number)
{
	hashstore->value_string = string;
	hashstore->value_number = number;
}

/*
	Debugging
*/

FORCEINLINE VOID _r_debug (_In_ LPCWSTR string)
{
	OutputDebugString (string);
}

VOID _r_debug_v (_In_ _Printf_format_string_ LPCWSTR format, ...);

#define RDBG(a) OutputDebugString ((a))
#define RDBG2(a, ...) _r_debug_v ((a), __VA_ARGS__)

/*
	Format strings, dates, numbers
*/

_Ret_maybenull_
PR_STRING _r_format_string (_In_ _Printf_format_string_ LPCWSTR format, ...);

_Ret_maybenull_
PR_STRING _r_format_string_v (_In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);

_Success_ (return)
BOOLEAN _r_format_bytesize64 (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ ULONG64 bytes);

_Success_ (return)
BOOLEAN _r_format_filetimeex (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LPFILETIME file_time, _In_ ULONG flags);

_Success_ (return)
BOOLEAN _r_format_unixtimeex (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 unixtime, _In_ ULONG flags);

_Success_ (return)
BOOLEAN _r_format_interval (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 seconds, _In_ INT digits);

_Success_ (return)
BOOLEAN _r_format_number (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 number);

_Success_ (return)
FORCEINLINE BOOLEAN _r_fmt_filetime (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LPFILETIME file_time)
{
	return _r_format_filetimeex (buffer, buffer_size, file_time, FDTF_DEFAULT);
}

_Success_ (return)
FORCEINLINE BOOLEAN _r_fmt_unixtime (_Out_writes_ (buffer_size) LPWSTR buffer, _In_ UINT buffer_size, _In_ LONG64 unixtime)
{
	return _r_format_unixtimeex (buffer, buffer_size, unixtime, FDTF_DEFAULT);
}

/*
	Calculation
*/

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

/*
	Byteswap
*/

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

/*
	Modal dialogs
*/

_Success_ (return)
BOOLEAN _r_msg_taskdialog (_In_ const TASKDIALOGCONFIG * ptd, _Out_opt_ PINT pbutton, _Out_opt_ PINT pradiobutton, _Out_opt_ LPBOOL pis_flagchecked); // vista+ TaskDialogIndirect
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);

/*
	Clipboard operations
*/

_Ret_maybenull_
PR_STRING _r_clipboard_get (_In_opt_ HWND hwnd);
VOID _r_clipboard_set (_In_opt_ HWND hwnd, _In_ LPCWSTR string, _In_ SIZE_T length);

/*
	Filesystem
*/

BOOLEAN _r_fs_deletefile (_In_ LPCWSTR path, _In_ BOOLEAN is_force);
BOOLEAN _r_fs_deletedirectory (_In_ LPCWSTR path, _In_ BOOLEAN is_recurse);
LONG64 _r_fs_getfilesize (_In_ LPCWSTR path);
BOOLEAN _r_fs_makebackup (_In_ LPCWSTR path, _In_opt_ LONG64 timestamp, _In_ BOOLEAN is_removesourcefile);
BOOLEAN _r_fs_mkdir (_In_ LPCWSTR path);
PR_BYTE _r_fs_readfile (_In_ HANDLE hfile, _In_ ULONG file_size);

//_Check_return_
//FORCEINLINE BOOLEAN _r_fs_isvalidhandle (_In_opt_ HANDLE handle)
//{
//	return (handle != NULL) && (handle != INVALID_HANDLE_VALUE);
//}

#define _r_fs_isvalidhandle(handle) \
    ((handle) != NULL && (handle) != INVALID_HANDLE_VALUE)

_Check_return_
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

_Check_return_
FORCEINLINE LONG64 _r_fs_getsize (_In_ HANDLE hfile)
{
	LARGE_INTEGER size;

	if (GetFileSizeEx (hfile, &size))
		return size.QuadPart;

	return 0;
}

/*
	Paths
*/

LPCWSTR _r_path_getbasename (_In_ LPCWSTR path);
PR_STRING _r_path_getbasedirectory (_In_ LPCWSTR path);

_Ret_maybenull_
LPCWSTR _r_path_getbaseextension (_In_ LPCWSTR path);
PR_STRING _r_path_getfullpath (_In_ LPCWSTR path);

_Ret_maybenull_
PR_STRING _r_path_getknownfolder (_In_ ULONG folder, _In_opt_ LPCWSTR append);

_Ret_maybenull_
PR_STRING _r_path_getmodulepath (_In_opt_ HMODULE hmodule);

_Ret_maybenull_
PR_STRING _r_path_compact (_In_ LPCWSTR path, _In_ UINT length);

_Ret_maybenull_
PR_STRING _r_path_makeunique (_In_ LPCWSTR path);

_Ret_maybenull_
PR_STRING _r_path_search (_In_ LPCWSTR path);

PR_STRING _r_path_dospathfromnt (_In_ LPCWSTR path);

_Success_ (return == ERROR_SUCCESS)
ULONG _r_path_ntpathfromdos (_In_ LPCWSTR path, _Outptr_ PR_STRING * ptr_nt_path);

/*
	Shell
*/

VOID _r_shell_openfile (_In_ LPCWSTR path);

FORCEINLINE VOID _r_shell_opendefault (_In_ LPCWSTR path)
{
	ShellExecute (NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
}

/*
	String reference object
*/

FORCEINLINE VOID _r_obj_initializestringrefex (_Out_ PR_STRINGREF string, _In_opt_ LPWSTR buffer, _In_opt_ SIZE_T length)
{
	string->buffer = buffer;
	string->length = length;
}

FORCEINLINE VOID _r_obj_initializestringref (_Out_ PR_STRINGREF string, _In_ LPWSTR buffer)
{
	_r_obj_initializestringrefex (string, buffer, _r_str_length (buffer) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_obj_initializestringref2 (_Out_ PR_STRINGREF string, _In_ PR_STRING buffer)
{
	_r_obj_initializestringrefex (string, buffer->buffer, buffer->length);
}

FORCEINLINE VOID _r_obj_initializeemptystringref (_Out_ PR_STRINGREF string)
{
	_r_obj_initializestringrefex (string, NULL, 0);
}

/*
	Strings
*/

//_Check_return_
//FORCEINLINE BOOLEAN _r_str_isempty (_In_opt_ LPCWSTR string)
//{
//	return !string || (string[0] == UNICODE_NULL);
//}

#define _r_str_isempty(string) \
    (!(string) || (string)[0] == UNICODE_NULL)

//_Check_return_
//FORCEINLINE BOOLEAN _r_str_isempty_a (_In_opt_ LPCSTR string)
//{
//	return !string || (string[0] == ANSI_NULL);
//}

#define _r_str_isempty_a(string) \
    (!(string) || (string)[0] == ANSI_NULL)

_Check_return_
BOOLEAN _r_str_isnumeric (_In_ LPCWSTR string);

_Success_ (return)
BOOLEAN _r_str_append (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string);

_Success_ (return)
BOOLEAN _r_str_appendformat (_Inout_updates_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...);

_Success_ (return)
BOOLEAN _r_str_copy (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ LPCWSTR string);

_Check_return_
SIZE_T _r_str_length (_In_ LPCWSTR string);

_Success_ (return)
BOOLEAN _r_str_printf (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, ...);

_Success_ (return)
BOOLEAN _r_str_printf_v (_Out_writes_ (buffer_size) _Always_ (_Post_z_) LPWSTR buffer, _In_ SIZE_T buffer_size, _In_ _Printf_format_string_ LPCWSTR format, _In_ va_list arg_ptr);

_Check_return_
SIZE_T _r_str_hash (_In_ LPCWSTR string);

_Check_return_
INT _r_str_compare (_In_ LPCWSTR string1, _In_ LPCWSTR string2);

_Check_return_
INT _r_str_compare_length (_In_ LPCWSTR string1, _In_ LPCWSTR string2, _In_ SIZE_T length);

_Check_return_
FORCEINLINE INT _r_str_compare_logical (_In_ LPCWSTR string1, _In_ LPCWSTR string2)
{
	return StrCmpLogicalW (string1, string2);
}

_Ret_maybenull_
PR_STRING _r_str_expandenvironmentstring (_In_ LPCWSTR string);

_Ret_maybenull_
PR_STRING _r_str_unexpandenvironmentstring (_In_ LPCWSTR string);

_Ret_maybenull_
PR_STRING _r_str_fromguid (_In_ LPCGUID lpguid);

_Ret_maybenull_
PR_STRING _r_str_fromsecuritydescriptor (_In_ PSECURITY_DESCRIPTOR lpsd, _In_ SECURITY_INFORMATION information);

_Ret_maybenull_
PR_STRING _r_str_fromsid (_In_ PSID lpsid);

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

BOOLEAN _r_str_toboolean (_In_ LPCWSTR string);
LONG _r_str_tolongex (_In_ LPCWSTR string, _In_ INT radix);
LONG64 _r_str_tolong64 (_In_ LPCWSTR string);
ULONG _r_str_toulongex (_In_ LPCWSTR string, _In_ INT radix);
ULONG64 _r_str_toulong64 (_In_ LPCWSTR string);

FORCEINLINE LONG _r_str_tolong (_In_ LPCWSTR string)
{
	return _r_str_tolongex (string, 10);
}

FORCEINLINE ULONG _r_str_toulong (_In_ LPCWSTR string)
{
	return _r_str_toulongex (string, 10);
}

FORCEINLINE INT _r_str_tointeger (_In_ LPCWSTR string)
{
	return (INT)_r_str_tolong (string);
}

FORCEINLINE UINT _r_str_touinteger (_In_ LPCWSTR string)
{
	return (UINT)_r_str_toulong (string);
}

#if defined(_WIN64)
#define _r_str_tolongptr _r_str_tolong64
#define _r_str_toulongptr _r_str_toulong64
#else
#define _r_str_tolongptr _r_str_tolong
#define _r_str_toulongptr _r_str_toulong
#endif // _WIN64

BOOLEAN _r_str_toboolean_a (_In_ LPCSTR string);
LONG _r_str_tolong_a (_In_ LPCSTR string);
LONG64 _r_str_tolong64_a (_In_ LPCSTR string);
ULONG _r_str_toulong_a (_In_ LPCSTR string);
ULONG64 _r_str_toulong64_a (_In_ LPCSTR string);

FORCEINLINE INT _r_str_tointeger_a (_In_ LPCSTR string)
{
	return (INT)_r_str_tolong_a (string);
}

FORCEINLINE UINT _r_str_touinteger_a (_In_ LPCSTR string)
{
	return (UINT)_r_str_toulong_a (string);
}

#if defined(_WIN64)
#define _r_str_tolongptr_a _r_str_tolong64_a
#define _r_str_toulongptr_a _r_str_toulong64_a
#else
#define _r_str_tolongptr_a _r_str_tolong_a
#define _r_str_toulongptr_a _r_str_toulong_a
#endif // _WIN64

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findchar (_In_ LPCWSTR string, _In_ WCHAR character);

_Success_ (return != SIZE_MAX)
SIZE_T _r_str_findlastchar (_In_ LPCWSTR string, _In_ SIZE_T length, _In_ WCHAR character);

VOID _r_str_replacechar (_Inout_ LPWSTR string, _In_ WCHAR char_from, _In_ WCHAR char_to);

BOOLEAN _r_str_match (_In_ LPCWSTR string, _In_ LPCWSTR pattern, _In_ BOOLEAN is_ignorecase);

FORCEINLINE VOID _r_str_skipchar (_Inout_ PR_STRINGREF string, _In_ SIZE_T length)
{
	string->buffer = (PWCHAR)PTR_ADD_OFFSET (string->buffer, length);
	string->length -= length;
}

FORCEINLINE VOID _r_str_trim (_Inout_ LPWSTR string, _In_ LPCWSTR trim)
{
	if (!_r_str_isempty (string))
		StrTrim (string, trim);
}

FORCEINLINE VOID _r_str_trim_a (_Inout_ LPSTR string, _In_ LPCSTR trim)
{
	if (!_r_str_isempty_a (string))
		StrTrimA (string, trim);
}

FORCEINLINE WCHAR _r_str_lower (_In_ WCHAR chr)
{
	return RtlDowncaseUnicodeChar (chr);
}

FORCEINLINE WCHAR _r_str_upper (_In_ WCHAR chr)
{
	return RtlUpcaseUnicodeChar (chr);
}

VOID _r_str_tolower (_Inout_ LPWSTR string);
VOID _r_str_toupper (_Inout_ LPWSTR string);

_Ret_maybenull_
PR_STRING _r_str_extractex (_In_ LPCWSTR string, _In_ SIZE_T length, _In_ SIZE_T start_pos, _In_ SIZE_T extract_length);

_Ret_maybenull_
FORCEINLINE PR_STRING _r_str_extract (_In_ PR_STRING string, _In_ SIZE_T start_pos, _In_ SIZE_T extract_length)
{
	return _r_str_extractex (string->buffer, _r_obj_getstringlength (string), start_pos, extract_length);
}

_Ret_maybenull_
PR_STRING _r_str_multibyte2unicodeex (_In_ LPCSTR string, _In_ SIZE_T string_size);

_Ret_maybenull_
PR_BYTE _r_str_unicode2multibyteex (_In_ LPCWSTR string, _In_ SIZE_T string_size);

_Ret_maybenull_
FORCEINLINE PR_STRING _r_str_multibyte2unicode (_In_ LPCSTR string)
{
	return _r_str_multibyte2unicodeex (string, strnlen_s (string, PR_STR_MAX_LENGTH));
}

_Ret_maybenull_
FORCEINLINE PR_BYTE _r_str_unicode2multibyte (_In_ LPCWSTR string)
{
	return _r_str_unicode2multibyteex (string, _r_str_length (string) * sizeof (WCHAR));
}

PR_STRING _r_str_splitatchar (_In_ PR_STRINGREF string, _Out_ PR_STRINGREF token, _In_ WCHAR separator);
PR_STRING _r_str_splitatlastchar (_In_ PR_STRINGREF string, _Out_ PR_STRINGREF token, _In_ WCHAR separator);

_Ret_maybenull_
PR_HASHTABLE _r_str_unserialize (_In_ PR_STRING string, _In_ WCHAR key_delimeter, _In_ WCHAR value_delimeter);

INT _r_str_versioncompare (_In_ LPCWSTR v1, _In_ LPCWSTR v2);

/*
	System information
*/

#define WINDOWS_XP 51
#define WINDOWS_XP_64 52
#define WINDOWS_VISTA 60
#define WINDOWS_7 61
#define WINDOWS_8 62
#define WINDOWS_8_1 63
#define WINDOWS_10 100
#define WINDOWS_10_1507 WINDOWS_10
#define WINDOWS_10_1511 101
#define WINDOWS_10_1607 102
#define WINDOWS_10_1703 103
#define WINDOWS_10_1709 104
#define WINDOWS_10_1803 105
#define WINDOWS_10_1809 106
#define WINDOWS_10_1903 107
#define WINDOWS_10_1909 108
#define WINDOWS_10_2004 109
#define WINDOWS_10_20H2 110
#define WINDOWS_10_21H1 111

_Check_return_
BOOLEAN _r_sys_iselevated ();

_Check_return_
ULONG _r_sys_getwindowsversion ();

FORCEINLINE ULONG _r_sys_getprocessorscount ()
{
	return NtCurrentPeb ()->NumberOfProcessors;
}

_Check_return_
FORCEINLINE BOOLEAN _r_sys_isosversionequal (_In_ ULONG required_version)
{
	return _r_sys_getwindowsversion () == required_version;
}

_Check_return_
FORCEINLINE BOOLEAN _r_sys_isosversiongreaterorequal (_In_ ULONG required_version)
{
	return _r_sys_getwindowsversion () >= required_version;
}

BOOLEAN _r_sys_createprocessex (_In_opt_ LPCWSTR file_name, _In_opt_ LPCWSTR command_line, _In_opt_ LPCWSTR current_directory, _In_ WORD show_state, _In_ ULONG flags);

FORCEINLINE BOOLEAN _r_sys_createprocess (_In_opt_ LPCWSTR file_name, _In_opt_ LPCWSTR command_line, _In_opt_ LPCWSTR current_directory)
{
	return _r_sys_createprocessex (file_name, command_line, current_directory, SW_SHOWDEFAULT, 0);
}

#define THREAD_API NTSTATUS NTAPI
typedef PUSER_THREAD_START_ROUTINE THREAD_CALLBACK;

typedef struct tagR_THREAD_DATA
{
	HANDLE handle;
	ULONG tid;
	BOOLEAN is_handleused;
} R_THREAD_DATA, *PR_THREAD_DATA;

typedef struct tagR_THREAD_CONTEXT
{
	THREAD_CALLBACK start_address;
	PVOID arglist;
	HANDLE thread;
	BOOLEAN is_handleused;
} R_THREAD_CONTEXT, *PR_THREAD_CONTEXT;

THREAD_API _r_sys_basethreadstart (_In_ PVOID arglist);

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_sys_createthreadex (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist, _Out_opt_ PHANDLE hthread, _In_ INT priority);

_Success_ (NT_SUCCESS (return))
FORCEINLINE NTSTATUS _r_sys_createthread (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist, _Out_opt_ PHANDLE hthread)
{
	return _r_sys_createthreadex (start_address, arglist, hthread, THREAD_PRIORITY_NORMAL);
}

_Success_ (NT_SUCCESS (return))
FORCEINLINE NTSTATUS _r_sys_createthread2 (_In_ THREAD_CALLBACK start_address, _In_opt_ PVOID arglist)
{
	return _r_sys_createthreadex (start_address, arglist, NULL, THREAD_PRIORITY_NORMAL);
}

_Success_ (NT_SUCCESS (return))
FORCEINLINE NTSTATUS _r_sys_resumethread (_In_ HANDLE hthread)
{
	return NtResumeThread (hthread, NULL);
}

FORCEINLINE HINSTANCE _r_sys_getimagebase ()
{
	return NtCurrentPeb ()->ImageBaseAddress;
}

FORCEINLINE LPCWSTR _r_sys_getimagepathname ()
{
	return NtCurrentPeb ()->ProcessParameters->ImagePathName.Buffer;
}

FORCEINLINE LPCWSTR _r_sys_getimagecommandline ()
{
	return NtCurrentPeb ()->ProcessParameters->CommandLine.Buffer;
}

FORCEINLINE HANDLE _r_sys_getstdout ()
{
	return NtCurrentPeb ()->ProcessParameters->StandardOutput;
}

PR_STRING _r_sys_getsessioninfo (_In_ WTS_INFO_CLASS info);
PR_STRING _r_sys_getusernamefromsid (_In_ PSID sid);

_Success_ (return)
BOOLEAN _r_sys_getopt (_In_ LPCWSTR args, _In_ LPCWSTR option_key, _Outptr_opt_ PR_STRING * option_value);

#if !defined(_WIN64)
BOOLEAN _r_sys_iswow64 ();
#endif // !_WIN64

VOID _r_sys_setprivilege (_In_ PULONG privileges, _In_ ULONG count, _In_ BOOLEAN is_enable);

FORCEINLINE ULONG _r_sys_gettickcount ()
{
#ifdef _WIN64

	return (ULONG)((USER_SHARED_DATA->TickCountQuad * USER_SHARED_DATA->TickCountMultiplier) >> 24);

#else

	ULARGE_INTEGER tickCount;

	while (TRUE)
	{
		tickCount.HighPart = (ULONG)USER_SHARED_DATA->TickCount.High1Time;
		tickCount.LowPart = USER_SHARED_DATA->TickCount.LowPart;

		if (tickCount.HighPart == (ULONG)USER_SHARED_DATA->TickCount.High2Time)
			break;

		YieldProcessor ();
	}

	return (ULONG)((UInt32x32To64 (tickCount.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) + UInt32x32To64 ((tickCount.HighPart << 8) & 0xffffffff, USER_SHARED_DATA->TickCountMultiplier));

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

FORCEINLINE VOID _r_sys_setthreadexecutionstate (ULONG state)
{
	ULONG old_state;

	NtSetThreadExecutionState (state, &old_state);
}

/*
	Unixtime
*/

LONG64 _r_unixtime_now ();
VOID _r_unixtime_to_filetime (_In_ LONG64 unixtime, _Out_ PFILETIME file_time);

_Success_ (return)
BOOLEAN _r_unixtime_to_systemtime (_In_ LONG64 unixtime, _Out_ PSYSTEMTIME system_time);

LONG64 _r_unixtime_from_filetime (_In_ const FILETIME * file_time);
LONG64 _r_unixtime_from_systemtime (_In_ const SYSTEMTIME * system_time);

/*
	Device context (Draw/Calculation etc...)
*/

typedef HRESULT (WINAPI *GDFM)(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, PUINT dpiX, PUINT dpiY); // GetDpiForMonitor (win81+)
typedef UINT (WINAPI *GDFW)(HWND hwnd); // GetDpiForWindow (win10rs1+)
typedef UINT (WINAPI *GDFS)(void); // GetDpiForSystem (win10rs1+)
typedef INT (WINAPI *GSMFD)(INT nIndex, UINT dpi); // GetSystemMetricsForDpi (win10rs1+)
typedef BOOL (WINAPI *SPIFP)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi); // SystemParametersInfoForDpi (win10rs1+)
typedef BOOL (WINAPI *AWRFD)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi); // AdjustWindowRectExForDpi (win10rs1+)

_Success_ (return)
BOOLEAN _r_dc_adjustwindowrect (_In_opt_ HWND hwnd, _Inout_ LPRECT rect, _In_ ULONG style, _In_ ULONG style_ex, _In_ BOOL is_menu);

_Success_ (return)
BOOLEAN _r_dc_getsystemparametersinfo (_In_opt_ HWND hwnd, _In_ UINT action, _In_ UINT param1, _In_ PVOID param2);

VOID _r_dc_fillrect (_In_ HDC hdc, _In_ PRECT lprc, _In_ COLORREF clr);
COLORREF _r_dc_getcolorbrightness (_In_ COLORREF clr);
COLORREF _r_dc_getcolorshade (_In_ COLORREF clr, _In_ INT percent);

LONG _r_dc_getdpivalue (_In_opt_ HWND hwnd, _In_opt_ PRECT rect);
INT _r_dc_getsystemmetrics (_In_opt_ HWND hwnd, _In_ INT index);
LONG _r_dc_getfontwidth (_In_ HDC hdc, _In_ LPCWSTR string, _In_ SIZE_T length);

FORCEINLINE LONG _r_dc_getdpi (_In_opt_ HWND hwnd, _In_ INT scale)
{
	return _r_calc_multipledividesigned (scale, _r_dc_getdpivalue (hwnd, NULL), USER_DEFAULT_SCREEN_DPI);
}

FORCEINLINE LONG _r_dc_fontheighttosize (_In_opt_ HWND hwnd, _In_ INT height)
{
	return _r_calc_multipledividesigned (-height, 72, _r_dc_getdpivalue (hwnd, NULL));
}

FORCEINLINE LONG _r_dc_fontsizetoheight (_In_opt_ HWND hwnd, _In_ INT size)
{
	return -_r_calc_multipledividesigned (size, _r_dc_getdpivalue (hwnd, NULL), 72);
}

/*
	File dialog
*/

typedef struct tagR_FILE_DIALOG
{
	union
	{
		IFileDialog *ifd;
		LPOPENFILENAME ofn;
	} u;

	ULONG flags;
} R_FILE_DIALOG, *PR_FILE_DIALOG;

#define PR_FILEDIALOG_OPENDIR 0x0001
#define PR_FILEDIALOG_OPENFILE 0x0002
#define PR_FILEDIALOG_SAVEFILE 0x0004
#define PR_FILEDIALOG_ISIFILEDIALOG 0x0008

_Success_ (return)
BOOLEAN _r_filedialog_initialize (_Out_ PR_FILE_DIALOG file_dialog, _In_ ULONG flags);

_Success_ (return)
BOOLEAN _r_filedialog_show (_In_opt_ HWND hwnd, _In_ PR_FILE_DIALOG file_dialog);

PR_STRING _r_filedialog_getpath (_In_ PR_FILE_DIALOG file_dialog);
VOID _r_filedialog_setpath (_Inout_ PR_FILE_DIALOG file_dialog, _In_ LPCWSTR path);
VOID _r_filedialog_setfilter (_Inout_ PR_FILE_DIALOG file_dialog, _In_ COMDLG_FILTERSPEC * filters, _In_ ULONG count);

VOID _r_filedialog_destroy (_In_ PR_FILE_DIALOG file_dialog);

/*
	Window layout
*/

#define PR_LAYOUT_ANCHOR_LEFT 0x0001
#define PR_LAYOUT_ANCHOR_TOP 0x0002
#define PR_LAYOUT_ANCHOR_RIGHT 0x0004
#define PR_LAYOUT_ANCHOR_BOTTOM 0x0008
#define PR_LAYOUT_ANCHOR_ALL 0x000F

#define PR_LAYOUT_FORCE_INVALIDATE 0x1000 // invalidate the control when it is resized
#define PR_LAYOUT_SEND_NOTIFY 0x2000 // send WM_SIZE message on resize
#define PR_LAYOUT_NO_ANCHOR 0x4000 // do not calculate anchors for control

typedef struct tagR_LAYOUT_ITEM
{
	HWND hwnd;
	HDWP defer_handle;
	struct tagR_LAYOUT_ITEM *parent_item;
	RECT rect;
	RECT anchor;
	RECT margin;
	ULONG number_of_children;
	ULONG flags;
} R_LAYOUT_ITEM, *PR_LAYOUT_ITEM;

typedef struct tagR_LAYOUT_MANAGER
{
	R_LAYOUT_ITEM root_item;
	POINT original_size;
	PR_LIST list;
	BOOLEAN is_initialized;
} R_LAYOUT_MANAGER, *PR_LAYOUT_MANAGER;

typedef struct tagR_LAYOUT_ENUM
{
	HWND root_hwnd;
	PR_LAYOUT_ITEM layout_item;
	PR_LAYOUT_MANAGER layout_manager;
} R_LAYOUT_ENUM, *PR_LAYOUT_ENUM;

BOOLEAN _r_layout_initializemanager (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND hwnd);

_Ret_maybenull_
PR_LAYOUT_ITEM _r_layout_additem (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ PR_LAYOUT_ITEM parent_item, _In_ HWND hwnd, _In_ ULONG flags);

VOID _r_layout_edititemanchors (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ HWND * hwnds, _In_ PULONG anchors, _In_ SIZE_T count);
VOID _r_layout_resizeitem (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item);
BOOLEAN _r_layout_resize (_Inout_ PR_LAYOUT_MANAGER layout_manager, _In_ WPARAM wparam);

VOID _r_layout_setitemanchor (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ PR_LAYOUT_ITEM layout_item, _In_ ULONG flags);

FORCEINLINE VOID _r_layout_resizeminimumsize (_In_ PR_LAYOUT_MANAGER layout_manager, _Inout_ LPARAM lparam)
{
	PMINMAXINFO mmi;

	if (!layout_manager->is_initialized)
		return;

	mmi = (PMINMAXINFO)lparam;

	mmi->ptMinTrackSize = layout_manager->original_size;
}

FORCEINLINE VOID _r_layout_destroymanager (_Inout_ PR_LAYOUT_MANAGER layout_manager)
{
	layout_manager->is_initialized = FALSE;

	if (layout_manager->list)
		_r_obj_clearreference (&layout_manager->list);
}

/*
	Window management
*/

typedef struct tagR_RECTANGLE
{
	union
	{
		SIZE position;

		struct
		{
			LONG left;
			LONG top;
		};
	};

	union
	{
		SIZE size;

		struct
		{
			LONG width;
			LONG height;
		};
	};
} R_RECTANGLE, *PR_RECTANGLE;

C_ASSERT (sizeof (SIZE) == sizeof (LONG) * 2);

VOID _r_wnd_addstyle (_In_ HWND hwnd, INT _In_opt_ ctrl_id, _In_ LONG_PTR mask, _In_ LONG_PTR state_mask, _In_ INT index);
VOID _r_wnd_adjustworkingarea (_In_opt_ HWND hwnd, _Inout_ PR_RECTANGLE rectangle);
VOID _r_wnd_center (_In_ HWND hwnd, _In_opt_ HWND hparent);
VOID _r_wnd_changemessagefilter (_In_ HWND hwnd, _In_count_ (count) PUINT messages, _In_ SIZE_T count, _In_ ULONG action);
VOID _r_wnd_changesettings (_In_ HWND hwnd, _In_opt_ WPARAM wparam, _In_opt_ LPARAM lparam);
VOID _r_wnd_enablenonclientscaling (_In_ HWND hwnd);
BOOLEAN _r_wnd_isfullscreenmode ();
BOOLEAN _r_wnd_isoverlapped (_In_ HWND hwnd);
BOOLEAN _r_wnd_isundercursor (_In_ HWND hwnd);
VOID _r_wnd_setposition (_In_ HWND hwnd, _In_opt_ PSIZE position, _In_opt_ PSIZE size);
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

FORCEINLINE BOOLEAN _r_wnd_ismenu (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8000); // #32768
}

FORCEINLINE BOOLEAN _r_wnd_isdesktop (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8001); // #32769
}

FORCEINLINE BOOLEAN _r_wnd_isdialog (HWND hwnd)
{
	return (GetClassLongPtr (hwnd, GCW_ATOM) == 0x8002); // #32770
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

/*
	Inernet access (WinHTTP)
*/

typedef BOOLEAN (NTAPI *PR_INET_DOWNLOAD_FUNCTION) (_In_ ULONG total_written, _In_ ULONG total_length, _In_opt_ PVOID pdata);

typedef struct tagR_DOWNLOAD_INFO
{
	HANDLE hfile;
	PR_STRING string;
	PR_INET_DOWNLOAD_FUNCTION download_callback;
	PVOID data;
} R_DOWNLOAD_INFO, *PR_DOWNLOAD_INFO;

typedef struct tagR_URLPARTS
{
	ULONG flags;

	PR_STRING host;
	PR_STRING path;
	PR_STRING user;
	PR_STRING pass;

	INTERNET_SCHEME scheme;
	INTERNET_PORT port;

} R_URLPARTS, *PR_URLPARTS;

#define PR_URLPARTS_SCHEME 0x000001
#define PR_URLPARTS_PORT 0x000002
#define PR_URLPARTS_HOST 0x000004
#define PR_URLPARTS_PATH 0x000008
#define PR_URLPARTS_USER 0x000010
#define PR_URLPARTS_PASS 0x000020

_Check_return_
_Ret_maybenull_
HINTERNET _r_inet_createsession (_In_opt_ LPCWSTR useragent);

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_openurl (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Out_ LPHINTERNET pconnect, _Out_ LPHINTERNET prequest, _Out_opt_ PULONG total_length);

_Check_return_
_Success_ (return)
BOOLEAN _r_inet_readrequest (_In_ HINTERNET hrequest, _Out_writes_bytes_ (buffer_size) PVOID buffer, _In_ ULONG buffer_size, _Out_opt_ PULONG readed, _Inout_opt_ PULONG total_readed);

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_parseurlparts (_In_ LPCWSTR url, _Inout_ PR_URLPARTS url_parts, _In_ ULONG flags);

VOID _r_inet_destroyurlparts (_Inout_ PR_URLPARTS url_parts);

FORCEINLINE VOID _r_inet_initializedownload (_Out_ PR_DOWNLOAD_INFO pdi, _In_opt_ HANDLE hfile, _In_opt_ PR_INET_DOWNLOAD_FUNCTION download_callback, _In_opt_ PVOID data)
{
	pdi->hfile = hfile;
	pdi->string = NULL;
	pdi->download_callback = download_callback;
	pdi->data = data;
}

_Check_return_
_Success_ (return == ERROR_SUCCESS)
ULONG _r_inet_begindownload (_In_ HINTERNET hsession, _In_ LPCWSTR url, _Inout_ PR_DOWNLOAD_INFO pdi);

VOID _r_inet_destroydownload (_In_ PR_DOWNLOAD_INFO pdi);

FORCEINLINE VOID _r_inet_close (_In_ HINTERNET handle)
{
	WinHttpCloseHandle (handle);
}

/*
	Registry
*/

_Ret_maybenull_
PR_BYTE _r_reg_querybinary (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);

ULONG _r_reg_queryulong (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);
ULONG64 _r_reg_queryulong64 (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);

_Ret_maybenull_
PR_STRING _r_reg_querystring (_In_ HKEY hkey, _In_opt_ LPCWSTR subkey, _In_opt_ LPCWSTR value);

ULONG _r_reg_querysubkeylength (_In_ HKEY hkey);
LONG64 _r_reg_querytimestamp (_In_ HKEY hkey);

/*
	Math
*/

ULONG _r_math_exponentiate (_In_ ULONG base, _In_ ULONG exponent);
ULONG64 _r_math_exponentiate64 (_In_ ULONG64 base, _In_ ULONG exponent);
ULONG _r_math_rand (_In_ ULONG min_number, _In_ ULONG max_number);
SIZE_T _r_math_rounduptopoweroftwo (_In_ SIZE_T number);

/*
	Resources
*/

_Ret_maybenull_
PR_STRING _r_res_getbinaryversion (_In_ LPCWSTR path);

_Success_ (return != NULL)
PVOID _r_res_loadresource (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ LPCWSTR type, _Out_opt_ PULONG buffer_size);

/*
	Other
*/

_Ret_maybenull_
HICON _r_loadicon (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ INT size);

PR_HASHTABLE _r_parseini (_In_ LPCWSTR path, _Inout_opt_ PR_LIST section_list);
VOID _r_sleep (_In_ LONG64 milliseconds);

/*
	Xml library
*/

typedef IStream* PR_XML_STREAM;
typedef VOID (NTAPI *PR_XML_STREAM_CALLBACK) (_Inout_ PR_XML_STREAM xml_library);

typedef struct tagR_XML_LIBRARY
{
	union
	{
		IXmlReader* reader;
		IXmlWriter* writer;
	};

	PR_XML_STREAM stream;
	PR_XML_STREAM_CALLBACK stream_callback;

	BOOLEAN is_reader;
	BOOLEAN is_initialized;

} R_XML_LIBRARY, *PR_XML_LIBRARY;

_Success_ (return == S_OK)
HRESULT _r_xml_initializelibrary (_Out_ PR_XML_LIBRARY xml_library, _In_ BOOLEAN is_reader, _In_opt_ PR_XML_STREAM_CALLBACK stream_callback);

_Success_ (return == S_OK)
HRESULT _r_xml_parsefile (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR file_path);

_Success_ (return == S_OK)
HRESULT _r_xml_parsestring (_Inout_ PR_XML_LIBRARY xml_library, _In_ PVOID buffer, _In_ ULONG buffer_size);

_Success_ (return)
BOOLEAN _r_xml_getattribute (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name, _Out_ LPCWSTR * attrib_value, _Out_opt_ PUINT attrib_length);

_Ret_maybenull_
PR_STRING _r_xml_getattribute_string (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);

BOOLEAN _r_xml_getattribute_boolean (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);
INT _r_xml_getattribute_integer (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);
LONG64 _r_xml_getattribute_long64 (_Inout_ PR_XML_LIBRARY xml_library, _In_ LPCWSTR attrib_name);

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

/*
	System tray
*/

BOOLEAN _r_tray_create (_In_ HWND hwnd, _In_ UINT uid, _In_ UINT code, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip, _In_ BOOLEAN is_hidden);
BOOLEAN _r_tray_popup (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_ LPCWSTR text);
BOOLEAN _r_tray_popupformat (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ ULONG icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...);
BOOLEAN _r_tray_setinfo (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ HICON hicon, _In_opt_ LPCWSTR tooltip);
BOOLEAN _r_tray_setinfoformat (_In_ HWND hwnd, _In_ UINT uid, _In_opt_ HICON hicon, _In_ _Printf_format_string_ LPCWSTR format, ...);
BOOLEAN _r_tray_toggle (_In_ HWND hwnd, _In_ UINT uid, _In_ BOOLEAN is_show);
BOOLEAN _r_tray_destroy (_In_ HWND hwnd, _In_ UINT uid);

/*
	Control: common
*/

INT _r_ctrl_isradiobuttonchecked (_In_ HWND hwnd, _In_ INT start_id, _In_ INT end_id);

_Ret_maybenull_
PR_STRING _r_ctrl_gettext (_In_ HWND hwnd, _In_ INT ctrl_id);

VOID _r_ctrl_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...);

FORCEINLINE ULONG _r_ctrl_gettextlength (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (ULONG)SendDlgItemMessage (hwnd, ctrl_id, WM_GETTEXTLENGTH, 0, 0);
}

FORCEINLINE VOID _r_ctrl_settext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ LPCWSTR text)
{
	SetDlgItemText (hwnd, ctrl_id, text);
}

VOID _r_ctrl_setbuttonmargins (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _r_ctrl_settabletext (_In_ HWND hwnd, _In_ INT ctrl_id1, _In_ LPCWSTR text1, _In_ INT ctrl_id2, _In_ LPCWSTR text2);

_Ret_maybenull_
HWND _r_ctrl_createtip (_In_opt_ HWND hparent);
VOID _r_ctrl_settiptext (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ LPCWSTR text);
VOID _r_ctrl_settiptextformat (_In_ HWND htip, _In_ HWND hparent, _In_ INT ctrl_id, _In_ _Printf_format_string_ LPCWSTR format, ...);
VOID _r_ctrl_settipstyle (_In_ HWND htip);
VOID _r_ctrl_showballoontip (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ LPCWSTR text);
VOID _r_ctrl_showballoontipformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT icon_id, _In_opt_ LPCWSTR title, _In_ _Printf_format_string_ LPCWSTR format, ...);

FORCEINLINE BOOLEAN _r_ctrl_isenabled (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return !!IsWindowEnabled (GetDlgItem (hwnd, ctrl_id));
}

FORCEINLINE VOID _r_ctrl_enable (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOLEAN is_enable)
{
	HWND hctrl = GetDlgItem (hwnd, ctrl_id);

	if (hctrl)
		EnableWindow (hctrl, is_enable);
}

/*
	Control: combobox
*/

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

/*
	Control: editbox
*/

FORCEINLINE VOID _r_edit_settextlimit (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ SIZE_T length)
{
	SendDlgItemMessage (hwnd, ctrl_id, EM_LIMITTEXT, (WPARAM)length, 0);
}

/*
	Control: menu
*/

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

/*
	Control: tab
*/

VOID _r_tab_adjustchild (_In_ HWND hwnd, _In_ INT tab_id, _In_ HWND hchild);
INT _r_tab_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam);
LPARAM _r_tab_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index);
INT _r_tab_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam);
VOID _r_tab_selectitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT index);

FORCEINLINE INT _r_tab_getcurrentitem (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETCURSEL, 0, 0);
}

FORCEINLINE INT _r_tab_getitemcount (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (INT)SendDlgItemMessage (hwnd, ctrl_id, TCM_GETITEMCOUNT, 0, 0);
}

/*
	Control: listview
*/

INT _r_listview_addcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR title, _In_opt_ INT width, _In_opt_ INT fmt);
INT _r_listview_addgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT align, _In_opt_ UINT state, _In_opt_ UINT state_mask);
INT _r_listview_additemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_ INT group_id, _In_opt_ LPARAM lparam);

_Success_ (return != -1)
INT _r_listview_finditem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT start_pos, _In_ LPARAM lparam);

VOID _r_listview_deleteallcolumns (_In_ HWND hwnd, _In_ INT ctrl_id);

INT _r_listview_getcolumncount (_In_ HWND hwnd, _In_ INT ctrl_id);
PR_STRING _r_listview_getcolumntext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id);
INT _r_listview_getcolumnwidth (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id);
INT _r_listview_getitemcheckedcount (_In_ HWND hwnd, _In_ INT ctrl_id);
INT _r_listview_getitemgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item);
LPARAM _r_listview_getitemlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item);
PR_STRING _r_listview_getitemtext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem);

VOID _r_listview_redraw (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item_id);

VOID _r_listview_setcolumn (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_opt_ LPCWSTR text, _In_opt_ INT width);
VOID _r_listview_setcolumnsortindex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id, _In_ INT);
VOID _r_listview_setitemex (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_ INT group_id, _In_opt_ LPARAM lparam);
VOID _r_listview_setitemcheck (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ BOOLEAN is_check);
VOID _r_listview_setitemvisible (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item);
VOID _r_listview_setgroup (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT group_id, _In_opt_ LPCWSTR title, _In_opt_ UINT state, _In_opt_ UINT state_mask);
VOID _r_listview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_ BOOL is_groupview);

FORCEINLINE INT _r_listview_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_ LPCWSTR text)
{
	return _r_listview_additemex (hwnd, ctrl_id, item, subitem, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

FORCEINLINE VOID _r_listview_deleteallgroups (HWND hwnd, INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_REMOVEALLGROUPS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteallitems (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEALLITEMS, 0, 0);
}

FORCEINLINE VOID _r_listview_deleteitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_DELETEITEM, (WPARAM)item, 0);
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

FORCEINLINE BOOLEAN _r_listview_isitemchecked (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2));
}

FORCEINLINE BOOLEAN _r_listview_isitemselected (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMSTATE, (WPARAM)item, LVNI_SELECTED) == LVNI_SELECTED);
}

FORCEINLINE BOOLEAN _r_listview_isitemvisible (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item)
{
	return !!((INT)SendDlgItemMessage (hwnd, ctrl_id, LVM_ISITEMVISIBLE, (WPARAM)item, 0));
}

FORCEINLINE VOID _r_listview_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT item, _In_ INT subitem, _In_opt_ LPCWSTR text)
{
	_r_listview_setitemex (hwnd, ctrl_id, item, subitem, text, I_IMAGENONE, I_GROUPIDNONE, 0);
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

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ LPCWSTR text, _In_opt_ HTREEITEM hparent, _In_ INT image, _In_opt_ LPARAM lparam);
LPARAM _r_treeview_getlparam (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem);
VOID _r_treeview_setitem (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HTREEITEM hitem, _In_opt_ LPCWSTR text, _In_ INT image, _In_opt_ LPARAM lparam);
VOID _r_treeview_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ ULONG exstyle, _In_opt_ INT height, _In_opt_ INT indent);

/*
	Control: statusbar
*/

VOID _r_status_settext (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part, _In_ LPCWSTR text);
VOID _r_status_settextformat (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT part, _In_ _Printf_format_string_ LPCWSTR format, ...);
VOID _r_status_setstyle (_In_ HWND hwnd, _In_ INT ctrl_id, _In_opt_ INT height);

FORCEINLINE LONG _r_status_getheight (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	RECT rect;

	if (GetClientRect (GetDlgItem (hwnd, ctrl_id), &rect))
		return _r_calc_rectheight (&rect);

	return 0;
}

/*
	Control: rebar
*/

FORCEINLINE LONG _r_rebar_getheight (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	return (LONG)SendDlgItemMessage (hwnd, ctrl_id, RB_GETBARHEIGHT, 0, 0);
}

/*
	Control: toolbar
*/

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

/*
	Control: progress bar
*/

VOID _r_progress_setmarquee (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOL is_enable);

/*
	Util
*/

FORCEINLINE VOID _r_util_templatewrite (_Inout_ PBYTE * ptr, _In_bytecount_ (size) LPCVOID data, _In_ SIZE_T size)
{
	memcpy (*ptr, data, size);
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
PR_STRING _r_util_versionformat (_In_ PR_STRING string);
BOOL CALLBACK _r_util_activate_window_callback (_In_ HWND hwnd, _In_ LPARAM lparam);

VOID NTAPI _r_util_dereferencearrayprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencelistprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencehashtableprocedure (_In_ PVOID entry);
VOID NTAPI _r_util_dereferencehashstoreprocedure (_In_ PVOID entry);

