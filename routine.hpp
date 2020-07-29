// routine++
// Copyright (c) 2012-2020 Henry++

#pragma once

#if !defined(_APP_NO_DEPRECATIONS)
#undef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif // !_APP_NO_DEPRECATIONS

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN

// crt
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

// winapi
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dde.h>
#include <dwmapi.h>
#include <ntsecapi.h>
#include <psapi.h>
#include <process.h>
#include <sddl.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <smmintrin.h>
#include <subauth.h>
#include <uxtheme.h>
#include <winhttp.h>
#include <wtsapi32.h>

// stl
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "app.hpp"
#include "ntapi.hpp"
#include "rconfig.hpp"

// libs
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wtsapi32.lib")

extern SIZE_T _r_str_length (LPCWSTR text);

// callback functions
typedef BOOLEAN (NTAPI *_R_CALLBACK_HTTP_DOWNLOAD) (ULONG total_written, ULONG total_length, LONG_PTR lpdata);
typedef VOID (NTAPI *_R_CALLBACK_OBJECT_CLEANUP) (PVOID lpdata);

// memory allocation/cleanup
#ifndef SAFE_DELETE_MEMORY
#define SAFE_DELETE_MEMORY(p) {if(p) {_r_mem_free (p); (p)=NULL;}}
#endif

#ifndef SAFE_DELETE_REFERENCE
#define SAFE_DELETE_REFERENCE(p) {if(p) {_r_obj_clearreference ((LPVOID*)&(p));}}
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

typedef struct _R_OBJECT_HEADER
{
	struct
	{
		USHORT Flags;
		USHORT Type;

		volatile LONG RefCount;

		_R_CALLBACK_OBJECT_CLEANUP CleanupCallback;
	};

	QUAD_PTR Body;
} R_OBJECT_HEADER, *PR_OBJECT_HEADER;

typedef struct _R_BYTEREF
{
	SIZE_T Length;
	LPSTR Buffer;
} R_BYTEREF, *PR_BYTEGREF;

typedef struct _R_STRINGREF
{
	SIZE_T Length;
	LPWSTR Buffer;
} R_STRINGREF, *PR_STRINGREF;

typedef struct _R_BYTE
{
	// Header
	union
	{
		R_BYTEREF sr;
		struct
		{

			SIZE_T Length;
			LPSTR Buffer;
		};
	};

	SIZE_T AllocatedLength;

	// Data
	CHAR Data[1];
} R_BYTE, *PR_BYTE;

typedef struct _R_STRING
{
	// Header
	union
	{
		R_STRINGREF sr;
		struct
		{
			SIZE_T Length;
			LPWSTR Buffer;
		};
	};

	SIZE_T AllocatedLength;

	WCHAR Data[1];
} R_STRING, *PR_STRING;

extern FORCEINLINE BOOLEAN _r_str_isempty (PUNICODE_STRING string);

extern SIZE_T _r_str_hash (LPCWSTR string);
extern FORCEINLINE SIZE_T _r_str_hash (PR_STRING string);

extern SIZE_T _r_str_hash (LPCWSTR string);

extern INT _r_str_compare (LPCWSTR string1, LPCWSTR string2);
extern INT _r_str_compareex (LPCWSTR string1, LPCWSTR string2, ULONG flags, SIZE_T length);

struct OBJECTS_STRINGS_HASH
{
	SIZE_T operator() (const PR_STRING k) const
	{
		return _r_str_hash (k);
	}
};

struct OBJECTS_STRINGS_IS_EQUAL
{
	bool operator() (const PR_STRING lhs, const PR_STRING rhs) const
	{
		return _r_str_compare (lhs->Buffer, rhs->Buffer) == 0;
	}
};

typedef std::unordered_map<PR_STRING, PR_STRING, OBJECTS_STRINGS_HASH, OBJECTS_STRINGS_IS_EQUAL> OBJECTS_STRINGS_MAP1;
typedef std::unordered_map<PR_STRING, OBJECTS_STRINGS_MAP1, OBJECTS_STRINGS_HASH, OBJECTS_STRINGS_IS_EQUAL> OBJECTS_STRINGS_MAP2;
typedef std::vector<PR_STRING> OBJECTS_STRINGS_VEC;

/*
	Definitions
*/

#define _R_DEBUG_HEADER L"Date,Function,Code,Description,Version\r\n"

#define _R_DEVICE_COUNT 0x1A
#define _R_DEVICE_PREFIX_LENGTH 0x40

#define _R_STR_MAX_LENGTH (INT_MAX - 1)

/*
	Debugging
*/

#define RDBG(a, ...) _r_dbg_print_v (a, __VA_ARGS__)

VOID _r_dbg_print (LPCWSTR string);
VOID _r_dbg_print_v (LPCWSTR format, ...);

/*
	Format strings, dates, numbers
*/

PR_STRING _r_format_string (LPCWSTR format, ...);
PR_STRING _r_format_string_v (LPCWSTR format, va_list argPtr);

VOID _r_format_bytesize64 (LPWSTR buffer, UINT bufferSize, ULONG64 bytes);

VOID _r_format_dateex (LPWSTR buffer, UINT bufferSize, LPFILETIME lpft, ULONG flags);
VOID _r_format_dateex (LPWSTR buffer, UINT bufferSize, time_t ut, ULONG flags);
VOID _r_format_interval (LPWSTR buffer, UINT bufferSize, LONG64 seconds, INT digits);
VOID _r_format_number (LPWSTR buffer, UINT bufferSize, LONG64 number, UINT fractional_digits, BOOLEAN is_groupdigits);

FORCEINLINE VOID _r_fmt_date (LPWSTR buffer, UINT bufferSize, LPFILETIME lpft)
{
	_r_format_dateex (buffer, bufferSize, lpft, FDTF_DEFAULT);
}

FORCEINLINE VOID _r_fmt_date (LPWSTR buffer, UINT bufferSize, time_t ut)
{
	_r_format_dateex (buffer, bufferSize, ut, FDTF_DEFAULT);
}

/*
	Calculation macroses
*/

#define _r_calc_percentof(TypeCast,Length,TotalLength)(TypeCast)(ceil (((DOUBLE)(Length) / (DOUBLE)(TotalLength)) * 100.0))
#define _r_calc_percentval(TypeCast,Percent,TotalLength)(TypeCast)(((DOUBLE)(TotalLength) * (DOUBLE)(Percent)) / 100.0)

#define _r_calc_rectheight(TypeCast,Rect)(TypeCast)((Rect)->bottom - (Rect)->top)
#define _r_calc_rectwidth(TypeCast,Rect)(TypeCast)((Rect)->right - (Rect)->left)

#define _r_calc_clamp(TypeCast,Val,MinVal,MaxVal)(TypeCast)(min (max ((Val), (MinVal)), (MaxVal)))

/*
	Size calculation
*/

#define _r_calc_kilobytes2bytes(TypeCast,Count)(TypeCast)((Count) * (TypeCast)1024)
#define _r_calc_megabytes2bytes(TypeCast,Count)(TypeCast)((Count) * (TypeCast)1048576)

#define _r_calc_seconds2milliseconds(TypeCast,Seconds)(TypeCast)((Seconds) * (TypeCast)1000)
#define _r_calc_minutes2seconds(TypeCast,Minutes)(TypeCast)((Minutes) * (TypeCast)60)
#define _r_calc_hours2seconds(TypeCast,Hours)((Hours) * (TypeCast)3600)
#define _r_calc_days2seconds(TypeCast,Days)((Days) * (TypeCast)86400)

/*
	Byteswap
*/

FORCEINLINE USHORT _r_byteswap_ushort (USHORT number)
{
	return _byteswap_ushort (number);
}

FORCEINLINE ULONG _r_byteswap_ulong (ULONG number)
{
	return _byteswap_ulong (number);
}

FORCEINLINE ULONG64 _r_byteswap_ulong64 (ULONG64 number)
{
	return _byteswap_uint64 (number);
}

/*
	FastLock is a port of FastResourceLock from PH 1.x.
	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.
	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.
	https://github.com/processhacker2/processhacker
*/

#if defined(_APP_NO_DEPRECATIONS)

#define RTL_SRWLOCK_OWNED_BIT 0
#define RTL_SRWLOCK_CONTENDED_BIT 1
#define RTL_SRWLOCK_SHARED_BIT 2
#define RTL_SRWLOCK_CONTENTION_LOCK_BIT 3
#define RTL_SRWLOCK_OWNED (1 << RTL_SRWLOCK_OWNED_BIT)
#define RTL_SRWLOCK_CONTENDED (1 << RTL_SRWLOCK_CONTENDED_BIT)
#define RTL_SRWLOCK_SHARED (1 << RTL_SRWLOCK_SHARED_BIT)
#define RTL_SRWLOCK_CONTENTION_LOCK (1 << RTL_SRWLOCK_CONTENTION_LOCK_BIT)
#define RTL_SRWLOCK_MASK (RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_CONTENTION_LOCK)
#define RTL_SRWLOCK_BITS 4

#define R_FASTLOCK RTL_SRWLOCK
#define _R_FASTLOCK _RTL_SRWLOCK
#define PR_FASTLOCK PRTL_SRWLOCK

#define _r_fastlock_initialize RtlInitializeSRWLock

#define _r_fastlock_acquireexclusive RtlAcquireSRWLockExclusive
#define _r_fastlock_acquireshared RtlAcquireSRWLockShared

#define _r_fastlock_releaseexclusive RtlReleaseSRWLockExclusive
#define _r_fastlock_releaseshared RtlReleaseSRWLockShared

#define _r_fastlock_tryacquireexclusive RtlTryAcquireSRWLockExclusive
#define _r_fastlock_tryacquireshared RtlTryAcquireSRWLockShared

#else

#define _R_FASTLOCK_OWNED 0x1
#define _R_FASTLOCK_EXCLUSIVE_WAKING 0x2

#define _R_FASTLOCK_SHARED_OWNERS_SHIFT 2
#define _R_FASTLOCK_SHARED_OWNERS_MASK 0x3ff
#define _R_FASTLOCK_SHARED_OWNERS_INC 0x4

#define _R_FASTLOCK_SHARED_WAITERS_SHIFT 12
#define _R_FASTLOCK_SHARED_WAITERS_MASK 0x3ff
#define _R_FASTLOCK_SHARED_WAITERS_INC 0x1000

#define _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT 22
#define _R_FASTLOCK_EXCLUSIVE_WAITERS_MASK 0x3ff
#define _R_FASTLOCK_EXCLUSIVE_WAITERS_INC 0x400000

#define _R_FASTLOCK_EXCLUSIVE_MASK (_R_FASTLOCK_EXCLUSIVE_WAKING | (_R_FASTLOCK_EXCLUSIVE_WAITERS_MASK << _R_FASTLOCK_EXCLUSIVE_WAITERS_SHIFT))

typedef struct _R_FASTLOCK
{
	volatile ULONG Value;

	HANDLE ExclusiveWakeEvent;
	HANDLE SharedWakeEvent;
} R_FASTLOCK, *PR_FASTLOCK;

FORCEINLINE ULONG _r_fastlock_getspincount ()
{
	SYSTEM_INFO si = {0};
	GetNativeSystemInfo (&si);

	return (si.dwNumberOfProcessors > 1) ? 4000 : 0;
}

FORCEINLINE VOID _r_fastlock_ensureeventcreated (PHANDLE phandle)
{
	HANDLE handle;

	if (*phandle != NULL)
		return;

	NtCreateSemaphore (&handle, SEMAPHORE_ALL_ACCESS, NULL, 0, MAXLONG);

	if (InterlockedCompareExchangePointer (phandle, handle, NULL) != NULL)
		NtClose (handle);
}

VOID _r_fastlock_initialize (PR_FASTLOCK plock);

VOID _r_fastlock_acquireexclusive (PR_FASTLOCK plock);
VOID _r_fastlock_acquireshared (PR_FASTLOCK plock);

VOID _r_fastlock_releaseexclusive (PR_FASTLOCK plock);
VOID _r_fastlock_releaseshared (PR_FASTLOCK plock);

BOOLEAN _r_fastlock_tryacquireexclusive (PR_FASTLOCK plock);
BOOLEAN _r_fastlock_tryacquireshared (PR_FASTLOCK plock);

#endif // _APP_NO_DEPRECATIONS

FORCEINLINE BOOLEAN _r_fastlock_islocked (const PR_FASTLOCK plock)
{
	BOOLEAN owned;

	// Need two memory barriers because we don't want the compiler re-ordering the following check
	// in either direction.
	MemoryBarrier ();

#if defined(_APP_NO_DEPRECATIONS)
	owned = ((*(volatile LONG_PTR*)&plock->Ptr) & RTL_SRWLOCK_OWNED);
#else
	owned = (plock->Value & _R_FASTLOCK_OWNED);
#endif // _APP_NO_DEPRECATIONS

	MemoryBarrier ();

	return owned;
}

/*
	Memory allocation reference
*/

HANDLE _r_mem_getheap ();
PVOID _r_mem_allocateex (SIZE_T bytes_count, ULONG flags);
PVOID _r_mem_reallocateex (PVOID pmemory, SIZE_T bytes_count, ULONG flags);
VOID _r_mem_free (PVOID pmemory);

FORCEINLINE PVOID _r_mem_allocate (SIZE_T bytes_count)
{
	return _r_mem_allocateex (bytes_count, HEAP_GENERATE_EXCEPTIONS);
}

FORCEINLINE PVOID _r_mem_allocatesafe (SIZE_T bytes_count)
{
	return _r_mem_allocateex (bytes_count, 0);
}

FORCEINLINE PVOID _r_mem_allocatezero (SIZE_T bytes_count)
{
	return _r_mem_allocateex (bytes_count, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY);
}

FORCEINLINE PVOID _r_mem_reallocate (PVOID heapMemory, SIZE_T bytes_count)
{
	return _r_mem_reallocateex (heapMemory, bytes_count, HEAP_GENERATE_EXCEPTIONS);
}

FORCEINLINE PVOID _r_mem_reallocatesafe (PVOID heapMemory, SIZE_T bytes_count)
{
	return _r_mem_reallocateex (heapMemory, bytes_count, 0);
}

FORCEINLINE PVOID _r_mem_reallocatezero (PVOID heapMemory, SIZE_T bytes_count)
{
	return _r_mem_reallocateex (heapMemory, bytes_count, HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY);
}

/*
	Objects reference
*/

//PVOID _r_obj2_allocateex (PVOID pdata, _R_CALLBACK_OBJECT_CLEANUP CleanupCallback);
//PVOID _r_obj2_reference (PVOID pdata);
//VOID _r_obj2_dereferenceex (PVOID pdata, LONG ref_count);
//
//FORCEINLINE VOID _r_obj2_dereference (PVOID pdata)
//{
//	_r_obj2_dereferenceex (pdata, 1);
//}

PVOID _r_obj_allocateex (SIZE_T size, _R_CALLBACK_OBJECT_CLEANUP CleanupCallback);
PVOID _r_obj_reference (PVOID pdata);
VOID _r_obj_dereferenceex (PVOID pdata, LONG ref_count);

FORCEINLINE PVOID _r_obj_allocate (SIZE_T size)
{
	return _r_obj_allocateex (size, NULL);
}

FORCEINLINE VOID _r_obj_dereference (PVOID pobj)
{
	_r_obj_dereferenceex (pobj, 1);
}

FORCEINLINE VOID _r_obj_movereference (PVOID *ObjectReference, PVOID NewObject)
{
	PVOID oldObject;

	oldObject = *ObjectReference;
	*ObjectReference = NewObject;

	if (oldObject)
		_r_obj_dereference (oldObject);
}

FORCEINLINE VOID _r_obj_clearreference (PVOID *ObjectReference)
{
	_r_obj_movereference (ObjectReference, NULL);
}

#ifdef __cplusplus
FORCEINLINE PR_STRING _r_obj_reference (PR_STRING pdata)
{
	return (PR_STRING)_r_obj_reference ((PVOID)pdata);
}

FORCEINLINE VOID _r_obj_movereference (PR_STRING *ObjectReference, PVOID NewObject)
{
	_r_obj_movereference ((PVOID*)ObjectReference, NewObject);
}

FORCEINLINE VOID _r_obj_clearreference (PR_STRING *ObjectReference)
{
	_r_obj_movereference (ObjectReference, NULL);
}
#endif // __cplusplus

#define ObjectToObjectHeader(Object) (CONTAINING_RECORD((Object), R_OBJECT_HEADER, Body))
#define ObjectHeaderToObject(ObjectHeader) ((PVOID)&((PR_OBJECT_HEADER)(ObjectHeader))->Body)

/*
	Modal dialogs
*/

BOOLEAN _r_msg_taskdialog (const TASKDIALOGCONFIG* ptd, PINT pbutton, PINT pradiobutton, LPBOOL pis_flagchecked); // vista+ TaskDialogIndirect
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata);

/*
	Clipboard operations
*/

PR_STRING _r_clipboard_get (HWND hwnd);
VOID _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length);

/*
	Filesystem
*/

FORCEINLINE BOOLEAN _r_fs_isvalidhandle (HANDLE handle)
{
	return (handle != NULL) && (handle != INVALID_HANDLE_VALUE);
}

FORCEINLINE BOOLEAN _r_fs_exists (LPCWSTR path)
{
	return RtlDoesFileExists_U (path);
}

FORCEINLINE BOOLEAN _r_fs_copy (LPCWSTR path_from, LPCWSTR path_to, ULONG flags)
{
	return !!CopyFileEx (path_from, path_to, NULL, NULL, NULL, flags);
}

FORCEINLINE BOOLEAN _r_fs_move (LPCWSTR path_from, LPCWSTR path_to, ULONG flags)
{
	return !!MoveFileEx (path_from, path_to, flags);
}

BOOLEAN _r_fs_makebackup (LPCWSTR path, time_t timestamp, BOOLEAN is_removesourcefile);
BOOLEAN _r_fs_mkdir (LPCWSTR path);
BOOLEAN _r_fs_readfile (HANDLE hfile, PVOID result, ULONG size);

#define _R_FLAG_REMOVE_USERECYCLER 0x01
#define _R_FLAG_REMOVE_FORCE 0x02
#define _R_FLAG_REMOVE_USERECURSION 0x04

BOOLEAN _r_fs_remove (LPCWSTR path, ULONG flags);

FORCEINLINE BOOLEAN _r_fs_setpos (HANDLE hfile, LONG64 pos, ULONG method)
{
	LARGE_INTEGER lpos = {0};

	lpos.QuadPart = pos;

	return !!SetFilePointerEx (hfile, lpos, NULL, method);
}

FORCEINLINE LONG64 _r_fs_size (HANDLE hfile)
{
	LARGE_INTEGER size = {0};
	GetFileSizeEx (hfile, &size);

	return size.QuadPart;
}

LONG64 _r_fs_size (LPCWSTR path);

/*
	Paths
*/

LPCWSTR _r_path_getbasename (LPCWSTR path);
PR_STRING _r_path_getbasedirectory (LPCWSTR path);
LPCWSTR _r_path_getbaseextension (LPCWSTR path);
PR_STRING _r_path_getfullpath (LPCWSTR path);
PR_STRING _r_path_getknownfolder (ULONG Folder, LPCWSTR append);
PR_STRING _r_path_getmodulepath (HMODULE hmodule);
VOID _r_path_explore (LPCWSTR path);
PR_STRING _r_path_compact (LPCWSTR path, UINT length);
PR_STRING _r_path_expand (LPCWSTR path);
PR_STRING _r_path_unexpand (LPCWSTR path);
PR_STRING _r_path_makeunique (LPCWSTR path);
PR_STRING _r_path_dospathfromnt (LPCWSTR path);
DWORD _r_path_ntpathfromdos (LPCWSTR dosPath, PR_STRING* ntPath);

/*
	Referencing string object
*/

PR_BYTE _r_obj_createbyteex (LPSTR buffer, SIZE_T length);
PR_STRING _r_obj_createstringex (LPCWSTR buffer, SIZE_T length);
PR_STRING _r_obj_referenceemptystring ();

VOID _r_string_appendex (PR_STRING* string, LPCWSTR text, SIZE_T length);
VOID _r_string_appendformat_v (PR_STRING* string, LPCWSTR format, va_list argPtr);
VOID _r_string_insertex (PR_STRING* string, SIZE_T index, LPCWSTR text, SIZE_T length);
VOID _r_string_insertformat_v (PR_STRING* string, SIZE_T index, LPCWSTR format, va_list argPtr);
VOID _r_string_remove (PR_STRING string, SIZE_T start_pos, SIZE_T length);
VOID _r_string_resizestring (PR_STRING* string, SIZE_T NewCapacity);
VOID _r_string_setsize (PR_STRING string, SIZE_T length);

FORCEINLINE VOID _r_string_append (PR_STRING* string, LPCWSTR text)
{
	_r_string_appendex (string, text, _r_str_length (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_string_append2 (PR_STRING* string, PR_STRING text)
{
	_r_string_appendex (string, text->Buffer, text->Length);
}

FORCEINLINE VOID _r_string_appendformat (PR_STRING* string, LPCWSTR format, ...)
{
	va_list argPtr;

	va_start (argPtr, format);
	_r_string_appendformat_v (string, format, argPtr);
	va_end (argPtr);
}

FORCEINLINE VOID _r_string_insert (PR_STRING* string, SIZE_T index, LPCWSTR text)
{
	_r_string_insertex (string, index, text, _r_str_length (text) * sizeof (WCHAR));
}

FORCEINLINE VOID _r_string_insert2 (PR_STRING* string, SIZE_T index, PR_STRING text)
{
	_r_string_insertex (string, index, text->Buffer, text->Length);
}

FORCEINLINE VOID _r_string_insertformat (PR_STRING* string, SIZE_T index, LPCWSTR format, ...)
{
	va_list argPtr;

	va_start (argPtr, format);
	_r_string_insertformat_v (string, index, format, argPtr);
	va_end (argPtr);
}

FORCEINLINE PR_STRING _r_string_fromunicodestring (PUNICODE_STRING UnicodeString)
{
	if (_r_str_isempty (UnicodeString))
		return _r_obj_referenceemptystring ();

	return _r_obj_createstringex (UnicodeString->Buffer, UnicodeString->Length);
}

FORCEINLINE BOOLEAN _r_string_tounicodestring (PR_STRING string, PUNICODE_STRING unicodeString)
{
	unicodeString->Length = (USHORT)string->Length;
	unicodeString->MaximumLength = (USHORT)string->Length + sizeof (UNICODE_NULL);
	unicodeString->Buffer = string->Buffer;

	return string->Length <= UNICODE_STRING_MAX_BYTES;
}

FORCEINLINE VOID _r_string_writenullterminator (PR_STRING string)
{
	assert (!(string->Length & 0x01));

	*(LPWSTR)PTR_ADD_OFFSET (string->Buffer, string->Length) = UNICODE_NULL;
}

FORCEINLINE VOID _r_string_trimtonullterminator (PR_STRING string)
{
	SIZE_T length = _r_str_length (string->Buffer);

	string->Length = length * sizeof (WCHAR);

	_r_string_writenullterminator (string); // terminate
}

FORCEINLINE PR_STRING _r_obj_createstringbuilder (SIZE_T length)
{
	if (length & 0x01)
		length += 1;

	PR_STRING string = _r_obj_createstringex (NULL, length);

	string->Length = 0;

	return string;
}

FORCEINLINE PR_STRING _r_obj_createstring (LPCWSTR string)
{
	return _r_obj_createstringex (string, _r_str_length (string) * sizeof (WCHAR));
}

FORCEINLINE PR_STRING _r_obj_createstring2 (PR_STRING string)
{
	return _r_obj_createstringex (string->Buffer, string->Length);
}

FORCEINLINE PR_STRING _r_obj_createstring3 (PR_STRINGREF string)
{
	return _r_obj_createstringex (string->Buffer, string->Length);
}

FORCEINLINE LPCWSTR _r_obj_getstring (PR_STRING string)
{
	if (string && string->Length && string->Buffer)
		return string->Buffer;

	return NULL;
}

FORCEINLINE LPCWSTR _r_obj_getstringorempty (PR_STRING string)
{
	if (string && string->Length && string->Buffer)
		return string->Buffer;

	return L"";
}

FORCEINLINE LPCWSTR _r_obj_getstringordefault (PR_STRING string, LPCWSTR defvalue)
{
	if (string && string->Length && string->Buffer)
		return string->Buffer;

	return defvalue;
}

FORCEINLINE VOID _r_obj_initializeemptystringref (PR_STRINGREF String)
{
	String->Length = 0;
	String->Buffer = NULL;
}

FORCEINLINE SIZE_T _r_obj_getstringsize (PR_STRING string)
{
	if (string)
		return string->Length;
	else
		return 0;
}

FORCEINLINE SIZE_T _r_obj_getstringlength (PR_STRING string)
{
	if (string)
		return string->Length / sizeof (WCHAR);
	else
		return 0;
}

/*
	Strings
*/

FORCEINLINE BOOLEAN _r_str_isempty (LPCWSTR string)
{
	return !string || (*string == UNICODE_NULL);
}

FORCEINLINE BOOLEAN _r_str_isempty (PR_BYTE string)
{
	return !string || !string->Length || !string->Buffer || (*string->Buffer == ANSI_NULL);
}

FORCEINLINE BOOLEAN _r_str_isempty (PR_STRING string)
{
	return !string || !string->Length || !string->Buffer || (*string->Buffer == UNICODE_NULL);
}

FORCEINLINE BOOLEAN _r_str_isempty (PUNICODE_STRING string)
{
	return !string || !string->Length || !string->Buffer || (*string->Buffer == UNICODE_NULL);
}

BOOLEAN _r_str_isnumeric (LPCWSTR string);

VOID _r_str_append (LPWSTR buffer, SIZE_T length, LPCWSTR text);
VOID _r_str_appendformat (LPWSTR buffer, SIZE_T length, LPCWSTR format, ...);
VOID _r_str_copy (LPWSTR buffer, SIZE_T length, LPCWSTR text);
SIZE_T _r_str_length (LPCWSTR text);
VOID _r_str_printf (LPWSTR buffer, SIZE_T length, LPCWSTR format, ...);
VOID _r_str_printf_v (LPWSTR buffer, SIZE_T length, LPCWSTR format, va_list argPtr);

SIZE_T _r_str_hash (LPCWSTR string);

FORCEINLINE SIZE_T _r_str_hash (PR_STRING string)
{
	return _r_str_hash (_r_obj_getstring (string));
}

#define _R_FLAG_COMPARE_LOGICAL 0x01
#define _R_FLAG_COMPARE_SENSITIVE 0x02

INT _r_str_compareex (LPCWSTR string1, LPCWSTR string2, ULONG flags, SIZE_T length);

FORCEINLINE INT _r_str_compare (LPCWSTR string1, LPCWSTR string2)
{
	return _r_str_compareex (string1, string2, 0, INVALID_SIZE_T);
}

FORCEINLINE INT _r_str_compare_length (LPCWSTR string1, LPCWSTR string2, SIZE_T length)
{
	return _r_str_compareex (string1, string2, 0, length);
}

FORCEINLINE INT _r_str_compare_logical (LPCWSTR string1, LPCWSTR string2)
{
	return _r_str_compareex (string1, string2, _R_FLAG_COMPARE_LOGICAL, INVALID_SIZE_T);
}

PR_STRING _r_str_expandenvironmentstring (PR_STRING string);

PR_STRING _r_str_fromguid (LPGUID lpguid);
PR_STRING _r_str_fromsecuritydescriptor (PSECURITY_DESCRIPTOR lpsd, SECURITY_INFORMATION information);
PR_STRING _r_str_fromsid (PSID lpsid);

BOOLEAN _r_str_toboolean (LPCWSTR string);
INT _r_str_tointeger (LPCWSTR string);
UINT _r_str_touinteger (LPCWSTR string);
LONG _r_str_tolongex (LPCWSTR string, INT radix);
LONG64 _r_str_tolong64 (LPCWSTR string);
ULONG _r_str_toulongex (LPCWSTR string, INT radix);
ULONG64 _r_str_toulong64 (LPCWSTR string);

#if defined(_WIN64)
#define _r_str_tolongptr _r_str_tolong64
#define _r_str_toulongptr _r_str_toulong64
#else
#define _r_str_tolongptr _r_str_tolong
#define _r_str_toulongptr _r_str_toulong
#endif // _WIN64

FORCEINLINE LONG _r_str_tolong (LPCWSTR string)
{
	return _r_str_tolongex (string, 10);
}

FORCEINLINE ULONG _r_str_toulong (LPCWSTR string)
{
	return _r_str_toulongex (string, 10);
}

SIZE_T _r_str_findchar (LPCWSTR string, SIZE_T length, WCHAR character, BOOLEAN is_ignorecase);
SIZE_T _r_str_findlastchar (LPCWSTR string, SIZE_T length, WCHAR character, BOOLEAN is_ignorecase);

VOID _r_str_replacechar (LPWSTR string, WCHAR char_from, WCHAR char_to);

BOOLEAN _r_str_match (LPCWSTR string, LPCWSTR pattern, BOOLEAN is_ignorecase);
VOID _r_str_trim (LPWSTR string, LPCWSTR trim);
VOID _r_str_trim (PR_STRING string, LPCWSTR trim);

FORCEINLINE WCHAR _r_str_lower (WCHAR chr)
{
	return RtlDowncaseUnicodeChar (chr);
}

FORCEINLINE WCHAR _r_str_upper (WCHAR chr)
{
	return RtlUpcaseUnicodeChar (chr);
}

VOID _r_str_tolower (LPWSTR string);
VOID _r_str_toupper (LPWSTR string);

FORCEINLINE VOID _r_str_tolower (PR_STRING string)
{
	for (SIZE_T i = 0; i < _r_obj_getstringlength (string); i++)
		*string->Buffer = _r_str_lower (*string->Buffer);
}

FORCEINLINE VOID _r_str_toupper (PR_STRING string)
{
	for (SIZE_T i = 0; i < _r_obj_getstringlength (string); i++)
		*string->Buffer = _r_str_upper (*string->Buffer);
}

PR_STRING _r_str_extract (PR_STRING string, SIZE_T start_pos, SIZE_T extract_length);

PR_STRING _r_str_multibyte2unicode (LPCSTR string);
PR_BYTE _r_str_unicode2multibyte (LPCWSTR string);

PR_STRING _r_str_splitatchar (PR_STRINGREF Input, PR_STRINGREF SecondPart, WCHAR separator, BOOLEAN is_ignorecase);
PR_STRING _r_str_splitatlastchar (PR_STRINGREF Input, PR_STRINGREF SecondPart, WCHAR separator, BOOLEAN is_ignorecase);

VOID _r_str_unserialize (PR_STRING string, WCHAR key_delimeter, WCHAR value_delimeter, OBJECTS_STRINGS_MAP1* valuesMap);

INT _r_str_versioncompare (LPCWSTR v1, LPCWSTR v2);

FORCEINLINE VOID _r_stringref_initializeex (PR_STRINGREF string, LPWSTR buffer, SIZE_T length)
{
	string->Buffer = buffer;
	string->Length = length;
}

FORCEINLINE VOID _r_stringref_initialize (PR_STRINGREF string, LPWSTR buffer)
{
	_r_stringref_initializeex (string, buffer, _r_str_length (buffer) * sizeof (WCHAR));
}

/*
	System information
*/

#define WINDOWS_XP 51
#define WINDOWS_VISTA 60
#define WINDOWS_7 61
#define WINDOWS_8 62
#define WINDOWS_8_1 63
#define WINDOWS_10 100 // TH1
#define WINDOWS_10_TH1 WINDOWS_10
#define WINDOWS_10_TH2 101
#define WINDOWS_10_RS1 102
#define WINDOWS_10_RS2 103
#define WINDOWS_10_RS3 104
#define WINDOWS_10_RS4 105
#define WINDOWS_10_RS5 106
#define WINDOWS_10_19H1 107
#define WINDOWS_10_19H2 108
#define WINDOWS_10_20H1 109
#define WINDOWS_10_20H2 110
#define WINDOWS_10_21H1 111

BOOLEAN _r_sys_iselevated ();
BOOLEAN _r_sys_isosversiongreaterorequal (ULONG requiredVersion);

BOOLEAN _r_sys_createprocessex (LPCWSTR fileName, LPCWSTR commandLine, LPCWSTR currentDirectory, WORD showState, ULONG flags);

FORCEINLINE BOOLEAN _r_sys_createprocess (LPCWSTR fileName, LPCWSTR commandLine, LPCWSTR currentDirectory)
{
	return _r_sys_createprocessex (fileName, commandLine, currentDirectory, SW_SHOWDEFAULT, 0);
}

#if defined(_APP_HAVE_NATIVETHREAD)
#define THREAD_FN NTSTATUS NTAPI
#define THREAD_CALLBACK PUSER_THREAD_START_ROUTINE
#else
#define THREAD_FN UINT WINAPI
#define THREAD_CALLBACK _beginthreadex_proc_type
#endif // _APP_HAVE_NATIVETHREAD

HANDLE _r_sys_createthreadex (THREAD_CALLBACK proc, PVOID lparam, BOOLEAN is_createsuspended, INT priority);

FORCEINLINE HANDLE _r_sys_createthread (THREAD_CALLBACK proc, PVOID lparam, BOOLEAN is_createsuspended)
{
	return _r_sys_createthreadex (proc, lparam, is_createsuspended, THREAD_PRIORITY_NORMAL);
}

FORCEINLINE ULONG _r_sys_endthread (ULONG exit_code)
{
#if defined(_APP_HAVE_NATIVETHREAD)
	RtlExitUserThread (exit_code);
#else
	_endthreadex (exit_code);
#endif // _APP_HAVE_NATIVETHREAD

	return exit_code;
}

FORCEINLINE ULONG _r_sys_resumethread (HANDLE hthread)
{
	return NtResumeThread (hthread, NULL);
}

PR_STRING _r_sys_getsessioninfo (WTS_INFO_CLASS info);
PR_STRING _r_sys_getusernamefromsid (PSID psid);

#if !defined(_WIN64)
BOOLEAN _r_sys_iswow64 ();
#endif // !_WIN64

VOID _r_sys_setprivilege (const PULONG privileges, ULONG count, BOOLEAN is_enable);

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

	return (ULONG)((UInt32x32To64 (tickCount.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) +
				   UInt32x32To64 ((tickCount.HighPart << 8) & 0xffffffff, USER_SHARED_DATA->TickCountMultiplier));

#endif
}

FORCEINLINE ULONG64 _r_sys_gettickcount64 ()
{
	ULARGE_INTEGER tickCount;

#ifdef _WIN64

	tickCount.QuadPart = USER_SHARED_DATA->TickCountQuad;

#else

	while (TRUE)
	{
		tickCount.HighPart = (ULONG)USER_SHARED_DATA->TickCount.High1Time;
		tickCount.LowPart = USER_SHARED_DATA->TickCount.LowPart;

		if (tickCount.HighPart == (ULONG)USER_SHARED_DATA->TickCount.High2Time)
			break;

		YieldProcessor ();
	}

#endif

	return (UInt32x32To64 (tickCount.LowPart, USER_SHARED_DATA->TickCountMultiplier) >> 24) +
		(UInt32x32To64 (tickCount.HighPart, USER_SHARED_DATA->TickCountMultiplier) << 8);
}

/*
	Unixtime
*/

time_t _r_unixtime_now ();
VOID _r_unixtime_to_filetime (time_t ut, FILETIME* pfileTime);
VOID _r_unixtime_to_systemtime (time_t ut, SYSTEMTIME* psystemTime);
time_t _r_unixtime_from_filetime (const FILETIME* pfileTime);
time_t _r_unixtime_from_systemtime (const SYSTEMTIME* psystemTime);

/*
	Device context (Draw/Calculation etc...)
*/

INT _r_dc_getdpivalue (HWND hwnd);
COLORREF _r_dc_getcolorbrightness (COLORREF clr);
COLORREF _r_dc_getcolorshade (COLORREF clr, INT percent);
INT _r_dc_getsystemmetrics (HWND hwnd, INT index);
VOID _r_dc_fillrect (HDC hdc, const LPRECT lprc, COLORREF clr);
LONG _r_dc_fontwidth (HDC hdc, LPCWSTR text, SIZE_T length);

FORCEINLINE INT _r_dc_getdpi (HWND hwnd, INT scale)
{
	return MulDiv (scale, _r_dc_getdpivalue (hwnd), USER_DEFAULT_SCREEN_DPI);
}

FORCEINLINE INT _r_dc_fontheighttosize (HWND hwnd, INT height)
{
	return MulDiv (-height, 72, _r_dc_getdpivalue (hwnd));
}

FORCEINLINE INT _r_dc_fontsizetoheight (HWND hwnd, INT size)
{
	return -MulDiv (size, _r_dc_getdpivalue (hwnd), 72);
}

/*
	Window management
*/

VOID _r_wnd_addstyle (HWND hwnd, INT ctrl_id, LONG_PTR mask, LONG_PTR stateMask, INT index);
VOID _r_wnd_adjustwindowrect (HWND hwnd, LPRECT lprect);
VOID _r_wnd_center (HWND hwnd, HWND hparent);
VOID _r_wnd_changemessagefilter (HWND hwnd, PUINT pmsg, SIZE_T count, ULONG action);
VOID _r_wnd_changesettings (HWND hwnd, WPARAM wparam, LPARAM lparam);
VOID _r_wnd_enablenonclientscaling (HWND hwnd);
BOOLEAN _r_wnd_isfullscreenmode ();
BOOLEAN _r_wnd_isundercursor (HWND hwnd);
VOID _r_wnd_toggle (HWND hwnd, BOOLEAN is_show);

FORCEINLINE VOID _r_wnd_top (HWND hwnd, BOOLEAN is_enable)
{
	SetWindowPos (hwnd, is_enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

#if defined(_APP_HAVE_DARKTHEME)
BOOL _r_wnd_isdarktheme ();
VOID _r_wnd_setdarkframe (HWND hwnd, BOOL is_enable);
VOID _r_wnd_setdarkwindow (HWND hwnd, BOOL is_enable);
VOID _r_wnd_setdarktheme (HWND hwnd);
#endif // _APP_HAVE_DARKTHEME

FORCEINLINE VOID _r_wnd_centerwindowrect (LPRECT lprect, const LPRECT lpparent)
{
	lprect->left = lpparent->left + ((_r_calc_rectwidth (LONG, lpparent) - _r_calc_rectwidth (LONG, lprect)) / 2);
	lprect->top = lpparent->top + ((_r_calc_rectheight (LONG, lpparent) - _r_calc_rectheight (LONG, lprect)) / 2);
}

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent, LPCWSTR proxy_addr);
ULONG _r_inet_openurl (HINTERNET hsession, LPCWSTR url, LPCWSTR proxy_addr, LPHINTERNET pconnect, LPHINTERNET prequest, PULONG ptotallength);
BOOLEAN _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, ULONG buffer_length, PULONG preaded, PULONG ptotalreaded);
ULONG _r_inet_parseurl (LPCWSTR url, PINT scheme_ptr, LPWSTR host_ptr, LPWORD port_ptr, LPWSTR path_ptr, LPWSTR user_ptr, LPWSTR pass_ptr);
ULONG _r_inet_downloadurl (HINTERNET hsession, LPCWSTR proxy_addr, LPCWSTR url, LPVOID* lpdest, BOOLEAN is_filepath, _R_CALLBACK_HTTP_DOWNLOAD Callback, LONG_PTR lpdata);

FORCEINLINE VOID _r_inet_close (HINTERNET handle)
{
	WinHttpCloseHandle (handle);
}

/*
	Registry
*/

PBYTE _r_reg_querybinary (HKEY hkey, LPCWSTR value);
ULONG _r_reg_queryulong (HKEY hkey, LPCWSTR value);
ULONG64 _r_reg_queryulong64 (HKEY hkey, LPCWSTR value);
PR_STRING _r_reg_querystring (HKEY hkey, LPCWSTR value);
ULONG _r_reg_querysubkeylength (HKEY hkey);
time_t _r_reg_querytimestamp (HKEY hkey);

/*
	Other
*/

HICON _r_loadicon (HINSTANCE hinst, LPCWSTR name, INT size);
PVOID _r_loadresource (HINSTANCE hinst, LPCWSTR name, LPCWSTR type, PULONG psize);
VOID _r_parseini (LPCWSTR path, OBJECTS_STRINGS_MAP2* outputMap, OBJECTS_STRINGS_VEC* sectionArr);
ULONG _r_rand (ULONG min_number, ULONG max_number);
VOID _r_sleep (LONG64 milliseconds);

/*
	System tray
*/

BOOLEAN _r_tray_create (HWND hwnd, UINT uid, UINT code, HICON hicon, LPCWSTR tooltip, BOOLEAN is_hidden);
BOOLEAN _r_tray_popup (HWND hwnd, UINT uid, ULONG icon_id, LPCWSTR title, LPCWSTR text);
BOOLEAN _r_tray_popupformat (HWND hwnd, UINT uid, ULONG icon_id, LPCWSTR title, LPCWSTR format, ...);
BOOLEAN _r_tray_setinfo (HWND hwnd, UINT uid, HICON hicon, LPCWSTR tooltip);
BOOLEAN _r_tray_setinfoformat (HWND hwnd, UINT uid, HICON hicon, LPCWSTR format, ...);
BOOLEAN _r_tray_toggle (HWND hwnd, UINT uid, BOOLEAN is_show);
BOOLEAN _r_tray_destroy (HWND hwnd, UINT uid);

/*
	Control: common
*/

INT _r_ctrl_isradiobuttonchecked (HWND hwnd, INT start_id, INT end_id);

PR_STRING _r_ctrl_gettext (HWND hwnd, INT ctrl_id);
VOID _r_ctrl_settextformat (HWND hwnd, INT ctrl_id, LPCWSTR format, ...);

FORCEINLINE VOID _r_ctrl_settext (HWND hwnd, INT ctrl_id, LPCWSTR text)
{
	SetDlgItemText (hwnd, ctrl_id, text);
}

VOID _r_ctrl_setbuttonmargins (HWND hwnd, INT ctrl_id);
VOID _r_ctrl_settabletext (HWND hwnd, INT ctrl_id1, LPCWSTR text1, INT ctrl_id2, LPCWSTR text2);

HWND _r_ctrl_createtip (HWND hparent);
VOID _r_ctrl_settiptext (HWND htip, HWND hparent, INT ctrl_id, LPCWSTR text);
VOID _r_ctrl_settiptextformat (HWND htip, HWND hparent, INT ctrl_id, LPCWSTR format, ...);
VOID _r_ctrl_settipstyle (HWND htip);
VOID _r_ctrl_showballoontip (HWND hwnd, INT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR text);
VOID _r_ctrl_showballoontipformat (HWND hwnd, INT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR format, ...);

FORCEINLINE BOOLEAN _r_ctrl_isenabled (HWND hwnd, INT ctrl_id)
{
	return !!IsWindowEnabled (GetDlgItem (hwnd, ctrl_id));
}

FORCEINLINE VOID _r_ctrl_enable (HWND hwnd, INT ctrl_id, BOOLEAN is_enable)
{
	EnableWindow (GetDlgItem (hwnd, ctrl_id), is_enable);
}

/*
	Control: menu
*/

VOID _r_menu_checkitem (HMENU hmenu, UINT item_id_start, UINT item_id_end, UINT position_flag, UINT check_id);
VOID _r_menu_setitembitmap (HMENU hmenu, UINT item_id, HBITMAP hbitmap, BOOL is_byposition);
VOID _r_menu_setitemtext (HMENU hmenu, UINT item_id, LPCWSTR text, BOOL is_byposition);
INT _r_menu_popup (HMENU hmenu, HWND hwnd, LPPOINT lpmouse, BOOLEAN is_sendmessage);

FORCEINLINE VOID _r_menu_enableitem (HMENU hmenu, UINT item_id, UINT position_flag, BOOLEAN is_enable)
{
	EnableMenuItem (hmenu, item_id, position_flag | (is_enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

/*
	Control: tab
*/

VOID _r_tab_adjustchild (HWND hwnd, INT tab_id, HWND hchild);
INT _r_tab_additem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam);
LPARAM _r_tab_getlparam (HWND hwnd, INT ctrl_id, INT index);
INT _r_tab_setitem (HWND hwnd, INT ctrl_id, INT index, LPCWSTR text, INT image, LPARAM lparam);
VOID _r_tab_selectitem (HWND hwnd, INT ctrl_id, INT index);

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, INT ctrl_id, INT column_id, LPCWSTR title, INT width, INT fmt);
INT _r_listview_addgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT align, UINT state, UINT state_mask);
INT _r_listview_additemex (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam);

FORCEINLINE INT _r_listview_additem (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text)
{
	return _r_listview_additemex (hwnd, ctrl_id, item, subitem, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

VOID _r_listview_deleteallcolumns (HWND hwnd, INT ctrl_id);
VOID _r_listview_deleteallgroups (HWND hwnd, INT ctrl_id);
VOID _r_listview_deleteallitems (HWND hwnd, INT ctrl_id);

INT _r_listview_getcolumncount (HWND hwnd, INT ctrl_id);
PR_STRING _r_listview_getcolumntext (HWND hwnd, INT ctrl_id, INT column_id);
INT _r_listview_getcolumnwidth (HWND hwnd, INT ctrl_id, INT column_id);

INT _r_listview_getitemcount (HWND hwnd, INT ctrl_id, BOOLEAN is_listchecked);
LPARAM _r_listview_getitemlparam (HWND hwnd, INT ctrl_id, INT item);
PR_STRING _r_listview_getitemtext (HWND hwnd, INT ctrl_id, INT item, INT subitem);

BOOLEAN _r_listview_isitemchecked (HWND hwnd, INT ctrl_id, INT item);
BOOLEAN _r_listview_isitemvisible (HWND hwnd, INT ctrl_id, INT item);

VOID _r_listview_redraw (HWND hwnd, INT ctrl_id, INT item_id);

VOID _r_listview_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle, BOOL is_groupview);
VOID _r_listview_setcolumn (HWND hwnd, INT ctrl_id, INT column_id, LPCWSTR text, INT width);
VOID _r_listview_setcolumnsortindex (HWND hwnd, INT ctrl_id, INT column_id, INT arrow);
VOID _r_listview_setitemex (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text, INT image, INT group_id, LPARAM lparam);

FORCEINLINE VOID _r_listview_setitem (HWND hwnd, INT ctrl_id, INT item, INT subitem, LPCWSTR text)
{
	_r_listview_setitemex (hwnd, ctrl_id, item, subitem, text, I_IMAGENONE, I_GROUPIDNONE, 0);
}

VOID _r_listview_setitemcheck (HWND hwnd, INT ctrl_id, INT item, BOOLEAN state);
VOID _r_listview_setgroup (HWND hwnd, INT ctrl_id, INT group_id, LPCWSTR title, UINT state, UINT state_mask);

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, INT ctrl_id, LPCWSTR text, HTREEITEM hparent, INT image, LPARAM lparam);
LPARAM _r_treeview_getlparam (HWND hwnd, INT ctrl_id, HTREEITEM hitem);
VOID _r_treeview_setitem (HWND hwnd, INT ctrl_id, HTREEITEM hitem, LPCWSTR text, INT image, LPARAM lparam);
VOID _r_treeview_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle, INT height, INT indent);

/*
	Control: statusbar
*/

VOID _r_status_settext (HWND hwnd, INT ctrl_id, INT part, LPCWSTR text);
VOID _r_status_settextformat (HWND hwnd, INT ctrl_id, INT part, LPCWSTR format, ...);
VOID _r_status_setstyle (HWND hwnd, INT ctrl_id, INT height);

/*
	Control: toolbar
*/

VOID _r_toolbar_addbutton (HWND hwnd, INT ctrl_id, UINT command_id, INT style, INT_PTR text, INT state, INT image);
INT _r_toolbar_getwidth (HWND hwnd, INT ctrl_id);
VOID _r_toolbar_setbutton (HWND hwnd, INT ctrl_id, UINT command_id, LPCWSTR text, INT style, INT state, INT image);
VOID _r_toolbar_setstyle (HWND hwnd, INT ctrl_id, ULONG exstyle);

/*
	Control: progress bar
*/

VOID _r_progress_setmarquee (HWND hwnd, INT ctrl_id, BOOL is_enable);

/*
	Util
*/

FORCEINLINE VOID _r_util_templatewrite (BYTE **pPtr, LPCVOID pdata, SIZE_T size)
{
	RtlCopyMemory (*pPtr, pdata, size);
	*pPtr += size;
}

FORCEINLINE VOID _r_util_templatewriteulong (BYTE **pPtr, ULONG data)
{
	_r_util_templatewrite (pPtr, &data, sizeof (data));
}

FORCEINLINE VOID _r_util_templatewriteshort (BYTE **pPtr, WORD data)
{
	_r_util_templatewrite (pPtr, &data, sizeof (data));
}

VOID _r_util_templatewritestring (BYTE **pPtr, LPCWSTR string);
VOID _r_util_templatewritecontrol (BYTE **pPtr, ULONG ctrl_id, ULONG style, SHORT x, SHORT y, SHORT cx, SHORT cy, LPCWSTR class_name);
PR_STRING _r_util_versionformat (PR_STRING string);
BOOL CALLBACK _r_util_activate_window_callback (HWND hwnd, LPARAM lparam);
VOID _r_util_clear_objects_strings_map1 (OBJECTS_STRINGS_MAP1* ptr_map);
VOID _r_util_clear_objects_strings_map2 (OBJECTS_STRINGS_MAP2* ptr_map);
VOID _r_util_clear_objects_strings_vector (OBJECTS_STRINGS_VEC* ptr_vec);
