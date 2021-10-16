// rtypes.h
// project sdk library
//
// Copyright (c) 2021 Henry++

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

#include <windows.h>

//
// Types definition
//

typedef PVOID *PVOID_PTR;
typedef HICON *HICON_PTR;

//
// Calling convention
//

#ifndef _WIN64
#define FASTCALL __fastcall
#else
#define FASTCALL
#endif

//
// Exported function definitions
//

typedef BOOL (WINAPI *IW64P)(HANDLE hProcess, PBOOL Wow64Process); // IsWow64Process
typedef ULONG (NTAPI *IQTI) (PVOID, SC_SERVICE_TAG_QUERY_TYPE, PTAG_INFO_NAME_FROM_TAG); // I_QueryTagInformation

typedef HRESULT (WINAPI *LIWSD)(HINSTANCE hinst, LPCWSTR pszName, INT cx, INT cy, HICON *phico); // LoadIconWithScaleDown (vista+)
typedef HRESULT (WINAPI *SHQUNS)(QUERY_USER_NOTIFICATION_STATE *pquns); // SHQueryUserNotificationState (vista+)
typedef HRESULT (WINAPI *SFBSE)(ULONG64 ull, SFBS_FLAGS flags, LPWSTR pszBuf, INT cchBuf); // StrFormatByteSizeEx (vista+)
typedef HRESULT (WINAPI *TDI)(const TASKDIALOGCONFIG *pTaskConfig, PINT pnButton, PINT pnRadioButton, LPBOOL pfVerificationFlagChecked); // TaskDialogIndirect (vista+)

typedef BOOL (WINAPI *CWMFEX)(HWND hwnd, UINT message, ULONG action, PCHANGEFILTERSTRUCT pChangeFilterStruct); // ChangeWindowMessageFilterEx (win7+)
typedef BOOL (WINAPI *SSPM)(ULONG Flags); // SetSearchPathMode (win7+)
typedef BOOL (WINAPI *SDDD)(ULONG DirectoryFlags); // SetDefaultDllDirectories (win7sp1+)

typedef BOOL (WINAPI *IIP) (HANDLE); // IsImmersiveProcess (win8+)

typedef HRESULT (WINAPI *GDFM)(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, PUINT dpiX, PUINT dpiY); // GetDpiForMonitor (win81+)
typedef UINT (WINAPI *GDFW)(HWND hwnd); // GetDpiForWindow (win10rs1+)
typedef UINT (WINAPI *GDFS)(void); // GetDpiForSystem (win10rs1+)
typedef BOOL (WINAPI *AWRFD)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi); // AdjustWindowRectExForDpi (win10rs1+)
typedef INT (WINAPI *GSMFD)(INT nIndex, UINT dpi); // GetSystemMetricsForDpi (win10rs1+)
typedef BOOL (WINAPI *SPIFP)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi); // SystemParametersInfoForDpi (win10rs1+)

typedef INT (WINAPI *GLIEX)(_In_opt_ LPCWSTR lpLocaleName, _In_ LCTYPE LCType, _Out_writes_to_opt_ (cchData, return) LPWSTR lpLCData, _In_ int cchData); // GetLocaleInfoEx (win7+)

//
// Printf specifiers
//

#define PR_LONG "ld"
#define PR_LONG64 "lld"

#define PR_ULONG "lu"
#define PR_ULONG64 "llu"

#if defined(_WIN64)

#define PR_LONG_PTR PR_LONG64
#define PR_ULONG_PTR PR_ULONG64

#define PR_PTRDIFF PR_LONG64

#elif defined(_WIN32)

#define PR_LONG_PTR PR_LONG
#define PR_ULONG_PTR PR_ULONG

#define PR_PTRDIFF PRIi32

#else

#error "Some printf specifiers are undefined. Unknown architecture!"

#endif // _WIN64

#define PR_SIZE_T PR_ULONG_PTR

//
// Configuration
//

#define PR_DEVICE_COUNT 26
#define PR_DEVICE_PREFIX_LENGTH 64

#define PR_SIZE_TREEINDENT 12
#define PR_SIZE_ITEMHEIGHT 20
#define PR_SIZE_FOOTERHEIGHT 48
#define PR_SIZE_BUFFER_OVERFLOW (256 * 1024 * 1024)

#define PR_SIZE_MAX_STRING_LENGTH (INT_MAX - 1)
#define PR_SIZE_CONCAT_LENGTH_CACHE 16

//
// Logging
//

typedef enum R_LOG_LEVEL
{
	LOG_LEVEL_DEBUG = 1,
	LOG_LEVEL_INFO = 2,
	LOG_LEVEL_WARNING = 3,
	LOG_LEVEL_ERROR = 4,
	LOG_LEVEL_CRITICAL = 5,
} R_LOG_LEVEL, *PR_LOG_LEVEL;

typedef struct R_ERROR_INFO
{
	HINSTANCE hmodule;
	LPCWSTR description;
} R_ERROR_INFO, *PR_ERROR_INFO;

//
// Synchronization: Event
//

typedef struct R_EVENT
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
			ULONG spare_bit;
#endif
		};
	};
	HANDLE event_handle;
} R_EVENT, *PR_EVENT;

#define PR_EVENT_SET 0x01
#define PR_EVENT_SET_SHIFT 0x00
#define PR_EVENT_REFCOUNT_SHIFT 0x01
#define PR_EVENT_REFCOUNT_INC 0x02
#define PR_EVENT_REFCOUNT_MASK (((ULONG_PTR)1 << 15) - 1)

#define PR_EVENT_INIT { { PR_EVENT_REFCOUNT_INC }, NULL }

C_ASSERT (sizeof (R_EVENT) == sizeof (ULONG_PTR) + sizeof (HANDLE));

//
// Synchronization: One-time initialization
//

#if defined(APP_NO_DEPRECATIONS)

#define R_INITONCE RTL_RUN_ONCE
#define PR_INITONCE PRTL_RUN_ONCE

#define PR_INITONCE_INIT RTL_RUN_ONCE_INIT

#else

typedef struct R_INITONCE
{
	R_EVENT event_object;
} R_INITONCE, *PR_INITONCE;

#define PR_INITONCE_SHIFT 31
#define PR_INITONCE_INITIALIZING (0x1 << PR_INITONCE_SHIFT)
#define PR_INITONCE_INITIALIZING_SHIFT PR_INITONCE_SHIFT

#define PR_INITONCE_INIT { PR_EVENT_INIT }

C_ASSERT (PR_INITONCE_SHIFT >= FIELD_OFFSET (R_EVENT, available_for_use) * 8);

#endif // APP_NO_DEPRECATIONS

//
// Synchronization: Auto-dereference pool
//

// The size of the static array in an auto-release pool.
#define PR_AUTO_POOL_STATIC_SIZE 64

// The maximum size of the dynamic array for it to be kept after the auto-release pool is drained.
#define PR_AUTO_POOL_DYNAMIC_BIG_SIZE 256

typedef struct R_AUTO_POOL
{
	ULONG static_count;
	PVOID static_objects[PR_AUTO_POOL_STATIC_SIZE];

	ULONG dynamic_count;
	ULONG dynamic_allocated;
	PVOID_PTR dynamic_objects;

	struct R_AUTO_POOL *next_pool;
} R_AUTO_POOL, *PR_AUTO_POOL;

//
// Synchronization: Free list
//

typedef struct R_FREE_LIST
{
	SLIST_HEADER list_head;

	SIZE_T size;

	volatile ULONG count;
	volatile ULONG maximum_count;
} R_FREE_LIST, *PR_FREE_LIST;

typedef struct R_FREE_LIST_ENTRY
{
	SLIST_ENTRY list_entry;
	QUAD_PTR body;
} R_FREE_LIST_ENTRY, *PR_FREE_LIST_ENTRY;

#ifdef _WIN64
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, list_entry) == 0x00);
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, body) == 0x10);
#else
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, list_entry) == 0x00);
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, body) == 0x08);
#endif

//
// Synchronization: Queued lock
//

typedef struct R_QUEUED_LOCK
{
	ULONG_PTR value;
} R_QUEUED_LOCK, *PR_QUEUED_LOCK;

typedef struct DECLSPEC_ALIGN (16) R_QUEUED_WAIT_BLOCK
{
	// A pointer to the next wait block, i.e. the wait block pushed onto the list before this one.
	struct R_QUEUED_WAIT_BLOCK *next_block;

	// A pointer to the previous wait block, i.e. the wait block pushed onto the list after this
	// one.
	struct R_QUEUED_WAIT_BLOCK *previous_block;

	// A pointer to the last wait block, i.e. the first waiter pushed onto the list.
	struct R_QUEUED_WAIT_BLOCK *last_block;

	ULONG shared_owners;
	ULONG flags;
} R_QUEUED_WAIT_BLOCK, *PR_QUEUED_WAIT_BLOCK;

#define PR_QUEUED_LOCK_INIT { 0 }

#define PR_QUEUED_WAITER_EXCLUSIVE 0x1
#define PR_QUEUED_WAITER_SPINNING 0x2
#define PR_QUEUED_WAITER_SPINNING_SHIFT 1

#define PR_QUEUED_LOCK_OWNED ((ULONG_PTR)0x1)
#define PR_QUEUED_LOCK_OWNED_SHIFT 0
#define PR_QUEUED_LOCK_WAITERS ((ULONG_PTR)0x2)

// Valid only if Waiters = 0
#define PR_QUEUED_LOCK_SHARED_INC ((ULONG_PTR)0x4)
#define PR_QUEUED_LOCK_SHARED_SHIFT 2

// Valid only if Waiters = 1
#define PR_QUEUED_LOCK_TRAVERSING ((ULONG_PTR)0x4)
#define PR_QUEUED_LOCK_MULTIPLE_SHARED ((ULONG_PTR)0x8)

#define PR_QUEUED_LOCK_FLAGS ((ULONG_PTR)0xf)

#define _r_queuedlock_getsharedowners(value) \
    ((ULONG_PTR)(value) >> PR_QUEUED_LOCK_SHARED_SHIFT)

#define _r_queuedlock_getwaitblock(value) \
    ((PR_QUEUED_WAIT_BLOCK)((ULONG_PTR)(value) & ~PR_QUEUED_LOCK_FLAGS))

//
// Synchronization: Condition
//

typedef struct R_QUEUED_LOCK R_CONDITION, *PR_CONDITION;

#define PR_CONDITION_WAIT_QUEUED_LOCK 0x001
#define PR_CONDITION_WAIT_CRITICAL_SECTION 0x002
#define PR_CONDITION_WAIT_FAST_LOCK 0x004
#define PR_CONDITION_WAIT_LOCK_TYPE_MASK 0xfff

#define PR_CONDITION_WAIT_SHARED 0x1000
#define PR_CONDITION_WAIT_SPIN 0x2000

#define PR_CONDITION_INIT PR_QUEUED_LOCK_INIT

//
// Synchronization: Rundown protection
//

typedef struct R_RUNDOWN_WAIT_BLOCK
{
	R_EVENT wake_event;
	ULONG_PTR count;
} R_RUNDOWN_WAIT_BLOCK, *PR_RUNDOWN_WAIT_BLOCK;

typedef struct R_QUEUED_LOCK R_RUNDOWN_PROTECT, *PR_RUNDOWN_PROTECT;

#define PR_RUNDOWN_ACTIVE 0x1
#define PR_RUNDOWN_REF_SHIFT 1
#define PR_RUNDOWN_REF_INC 0x2

#define PR_RUNDOWN_PROTECT_INIT PR_QUEUED_LOCK_INIT

//
// Synchronization: Workqueue
//

typedef VOID (NTAPI *PR_WORKQUEUE_FUNCTION) (_In_ PVOID arglist, _In_ ULONG busy_count);

typedef struct R_ENVIRONMENT
{
	LONG base_priority : 6; // Base priority increment
	ULONG io_priority : 3; // I/O priority hint
	ULONG page_priority : 3; // Page/memory priority
	ULONG is_forced : 1;
	ULONG spare_bits : 19;
} R_ENVIRONMENT, *PR_ENVIRONMENT;

typedef struct R_WORKQUEUE_ITEM
{
	LIST_ENTRY list_entry;
	PR_WORKQUEUE_FUNCTION function_address;
	PVOID context;
} R_WORKQUEUE_ITEM, *PR_WORKQUEUE_ITEM;

typedef struct R_WORKQUEUE
{
	LIST_ENTRY queue_list_head;

	R_QUEUED_LOCK queue_lock;
	R_QUEUED_LOCK state_lock;

	R_RUNDOWN_PROTECT rundown_protect;
	R_CONDITION queue_empty_condition;

	R_ENVIRONMENT environment;

	HANDLE semaphore_handle;

	ULONG maximum_threads;
	ULONG minimum_threads;
	ULONG no_work_timeout;

	volatile ULONG current_threads;
	volatile ULONG busy_count;

	BOOLEAN is_terminating;
} R_WORKQUEUE, *PR_WORKQUEUE;

//
// Objects reference
//

typedef VOID (NTAPI *PR_OBJECT_CLEANUP_FUNCTION) (_In_ PVOID object_body);

typedef struct R_OBJECT_HEADER
{
	struct
	{
		PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
		volatile LONG ref_count;
	};

	QUAD_PTR body;
} R_OBJECT_HEADER, *PR_OBJECT_HEADER;

#define PR_OBJECT_HEADER_TO_OBJECT(object_header) (&((PR_OBJECT_HEADER)(object_header))->body)
#define PR_OBJECT_TO_OBJECT_HEADER(object) (CONTAINING_RECORD((object), R_OBJECT_HEADER, body))

//
// 8-bit string object
//

typedef struct R_BYTEREF
{
	SIZE_T length;
	LPSTR buffer;
} R_BYTEREF, *PR_BYTEREF;

typedef struct R_BYTE
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

//
// 16-bit string object
//

typedef struct R_STRINGREF
{
	SIZE_T length;
	LPWSTR buffer;
} R_STRINGREF, *PR_STRINGREF;

typedef struct R_STRING
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

typedef PR_STRING *PR_STRING_PTR;

#define PR_STRINGREF_INIT(string) { sizeof(string) - sizeof(UNICODE_NULL), (string) }
#define PR_BYTESREF_INIT(string) { sizeof(string) - sizeof(ANSI_NULL), (string) }

//
// String builder
//

typedef struct R_STRINGBUILDER
{
	PR_STRING string;
	SIZE_T allocated_length;
} R_STRINGBUILDER, *PR_STRINGBUILDER;

//
// Array object
//

typedef struct R_ARRAY
{
	PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
	SIZE_T allocated_count;
	SIZE_T count;
	SIZE_T item_size;
	PVOID items;
} R_ARRAY, *PR_ARRAY;

//
// List object
//

typedef struct R_LIST
{
	PR_OBJECT_CLEANUP_FUNCTION cleanup_callback;
	SIZE_T allocated_count;
	SIZE_T count;
	PVOID_PTR items;
} R_LIST, *PR_LIST;

//
// Hashtable object
//

typedef struct R_HASHTABLE_ENTRY
{
	SIZE_T next;
	ULONG_PTR hash_code;
	QUAD_PTR body;
} R_HASHTABLE_ENTRY, *PR_HASHTABLE_ENTRY;

typedef struct R_HASHTABLE
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

#define HASHTABLE_ENTRY_SIZE(inner_size) (UFIELD_OFFSET(R_HASHTABLE_ENTRY, body) + (inner_size))
#define HASHTABLE_GET_ENTRY(hashtable, index) ((PR_HASHTABLE_ENTRY)PTR_ADD_OFFSET((hashtable)->entries, HASHTABLE_ENTRY_SIZE((hashtable)->entry_size) * (index)))
#define HASHTABLE_GET_ENTRY_INDEX(hashtable, entry) ((SIZE_T)(PTR_ADD_OFFSET(entry, -(hashtable)->entries) / HASHTABLE_ENTRY_SIZE((hashtable)->entry_size)))
#define HASHTABLE_INIT_VALUE 0xFF

//
// Hashtable object pointer object
//

typedef struct R_OBJECT_POINTER
{
	PVOID object_body;
} R_OBJECT_POINTER, *PR_OBJECT_POINTER;

//
// Strings
//

#define PR_MAKE_VERSION_ULONG64(major, minor, build, revision) \
	(((ULONG64)(major) << 48) | \
	((ULONG64)(minor) << 32) | \
	((ULONG64)(build) << 16) | \
	((ULONG64)(revision) << 0))

//
// Cryptography
//

typedef struct _R_HASH_CONTEXT
{
	BCRYPT_ALG_HANDLE hash_alg_handle;
	BCRYPT_ALG_HANDLE sign_alg_handle;
	BCRYPT_KEY_HANDLE key_handle;
	BCRYPT_HASH_HANDLE hash_handle;
	PVOID hash_object;
	PVOID hash_data;
	ULONG hash_object_length;
	ULONG hash_data_length;
} R_HASH_CONTEXT, *PR_HASH_CONTEXT;

//
// System information
//

#define WINDOWS_2000 0x0500
#define WINDOWS_XP 0x0501
#define WINDOWS_XP_64 0x0502
#define WINDOWS_VISTA 0x0600
#define WINDOWS_7 0x0601
#define WINDOWS_8 0x0602
#define WINDOWS_8_1 0x0603
#define WINDOWS_10 0x0A00
#define WINDOWS_10_1507 WINDOWS_10 // LTSB 2015
#define WINDOWS_10_1511 0x0A01
#define WINDOWS_10_1607 0x0A02 // LTSB 2016
#define WINDOWS_10_1703 0x0A03
#define WINDOWS_10_1709 0x0A04
#define WINDOWS_10_1803 0x0A05
#define WINDOWS_10_1809 0x0A06 // LTSC 2019
#define WINDOWS_10_1903 0x0A07
#define WINDOWS_10_1909 0x0A08
#define WINDOWS_10_2004 0x0A09
#define WINDOWS_10_20H2 0x0A0A
#define WINDOWS_10_21H1 0x0A0B
#define WINDOWS_10_21H2 0x0A0C // LTSC 2022
#define WINDOWS_10_21H2_SERVER 0x0A0D
#define WINDOWS_11 0x0B00

typedef struct R_THREAD_CONTEXT
{
	PUSER_THREAD_START_ROUTINE function_address;
	PVOID arglist;
} R_THREAD_CONTEXT, *PR_THREAD_CONTEXT;

typedef struct R_TOKEN_ATTRIBUTES
{
	HANDLE token_handle;
	PSID token_sid;

	struct
	{
		ULONG is_elevated : 1;
		ULONG elevation_type : 2;
		ULONG spare_bits : 29;
	};

	ULONG reserved;
} R_TOKEN_ATTRIBUTES, *PR_TOKEN_ATTRIBUTES;

//
// File dialog
//

typedef struct R_FILE_DIALOG
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

//
// Window layout
//

typedef struct R_LAYOUT_ITEM
{
	HWND hwnd;
	HDWP defer_handle;
	struct R_LAYOUT_ITEM *parent_item;
	RECT rect;
	RECT anchor;
	RECT margin;
	ULONG number_of_children;
	ULONG flags;
} R_LAYOUT_ITEM, *PR_LAYOUT_ITEM;

typedef struct R_LAYOUT_MANAGER
{
	R_LAYOUT_ITEM root_item;
	POINT original_size;
	PR_LIST list;
	BOOLEAN is_initialized;
} R_LAYOUT_MANAGER, *PR_LAYOUT_MANAGER;

typedef struct R_LAYOUT_ENUM
{
	HWND root_hwnd;
	PR_LAYOUT_ITEM layout_item;
	PR_LAYOUT_MANAGER layout_manager;
} R_LAYOUT_ENUM, *PR_LAYOUT_ENUM;

#define PR_LAYOUT_ANCHOR_LEFT 0x0001
#define PR_LAYOUT_ANCHOR_TOP 0x0002
#define PR_LAYOUT_ANCHOR_RIGHT 0x0004
#define PR_LAYOUT_ANCHOR_BOTTOM 0x0008
#define PR_LAYOUT_ANCHOR_ALL 0x000F

#define PR_LAYOUT_FORCE_INVALIDATE 0x1000 // invalidate the control when it is resized
#define PR_LAYOUT_SEND_NOTIFY 0x2000 // send WM_SIZE message on resize
#define PR_LAYOUT_NO_ANCHOR 0x4000 // do not calculate anchors for control

//
// Window management
//

typedef struct R_SIZE
{
	LONG cx;
	LONG cy;
} R_SIZE, *PR_SIZE;

typedef struct R_RECTANGLE
{
	union
	{
		R_SIZE position;
		struct
		{
			LONG left;
			LONG top;
		};
	};

	union
	{
		R_SIZE size;
		struct
		{
			LONG width;
			LONG height;
		};
	};
} R_RECTANGLE, *PR_RECTANGLE;

//
// Inernet access (WinHTTP)
//

typedef BOOLEAN (NTAPI *PR_INET_DOWNLOAD_FUNCTION) (_In_ ULONG total_written, _In_ ULONG total_length, _In_opt_ PVOID pdata);

typedef struct R_DOWNLOAD_INFO
{
	HANDLE hfile;
	PR_STRING string;
	PR_INET_DOWNLOAD_FUNCTION download_callback;
	PVOID data;
} R_DOWNLOAD_INFO, *PR_DOWNLOAD_INFO;

typedef struct R_URLPARTS
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

//
// Resources
//

#define PR_LANG_TO_LCID(lang_id,codepage_id) (((lang_id) << 16) + (codepage_id))

typedef struct _R_VERSION_TRANSLATION
{
	WORD lang_id;
	WORD code_page;
} R_VERSION_TRANSLATION, *PR_VERSION_TRANSLATION;

//
// Xml library
//

typedef IStream *PR_XML_STREAM;
typedef VOID (NTAPI *PR_XML_STREAM_CALLBACK) (_Inout_ PR_XML_STREAM xml_library);

typedef struct R_XML_LIBRARY
{
	union
	{
		IXmlReader *reader;
		IXmlWriter *writer;
	};

	PR_XML_STREAM stream;
	PR_XML_STREAM_CALLBACK stream_callback;

	struct
	{
		ULONG is_reader : 1;
		ULONG is_initialized : 1;
		ULONG spare_bits : 30;
	};
} R_XML_LIBRARY, *PR_XML_LIBRARY;

//
// Application structures
//

typedef struct R_SETTINGS_PAGE
{
	HWND hwnd;
	UINT locale_id;
	INT dlg_id;
} R_SETTINGS_PAGE, *PR_SETTINGS_PAGE;

typedef struct R_UPDATE_COMPONENT
{
	PR_STRING full_name;
	PR_STRING short_name;
	PR_STRING version;
	PR_STRING new_version;
	PR_STRING temp_path;
	PR_STRING target_path;
	PR_STRING url;
	BOOLEAN is_installer;
	BOOLEAN is_haveupdate;
} R_UPDATE_COMPONENT, *PR_UPDATE_COMPONENT;

typedef struct R_UPDATE_INFO
{
	PR_ARRAY components;
	HWND htaskdlg;
	HWND hparent;
	BOOLEAN is_checking;
	BOOLEAN is_downloaded;
} R_UPDATE_INFO, *PR_UPDATE_INFO;

typedef struct R_SHARED_IMAGE
{
	HINSTANCE hinst;
	HICON hicon;
	INT icon_id;
	INT icon_size;
} R_SHARED_IMAGE, *PR_SHARED_IMAGE;

typedef struct APP_GLOBAL_CONFIG
{
	struct
	{
		R_QUEUED_LOCK lock;
		PR_HASHTABLE table;
	} config;

#if !defined(APP_CONSOLE)
	struct
	{
		WNDPROC wnd_proc;
		HANDLE hmutex;
		HWND hwnd;

#if defined(APP_HAVE_TRAY)
		UINT taskbar_msg;
#endif // APP_HAVE_TRAY

		BOOLEAN is_needmaximize;
	} main;
#endif // !APP_CONSOLE

#if !defined(APP_CONSOLE)
	struct
	{
		R_QUEUED_LOCK lock;
		PR_HASHTABLE table;
		PR_LIST available_list;

		PR_STRING resource_name;
		PR_STRING default_name;
		PR_STRING current_name;
	} locale;
#endif // !APP_CONSOLE

#if defined(APP_HAVE_UPDATES) && !defined(APP_CONSOLE)
	struct
	{
		R_UPDATE_INFO info;
	} update;
#endif // APP_HAVE_UPDATES && !APP_CONSOLE

#if defined(APP_HAVE_SETTINGS) && !defined(APP_CONSOLE)
	struct
	{
		DLGPROC wnd_proc;
		PR_ARRAY page_list;
		HWND hwnd;
	} settings;
#endif // APP_HAVE_SETTINGS && !APP_CONSOLE
} APP_GLOBAL_CONFIG, *PAPP_GLOBAL_CONFIG;
