// rtypes.h
// project sdk library
//
// Copyright (c) 2019-2024 Henry++

#pragma once

#include <mscat.h>

//
// Types definition
//

typedef PSYSTEM_PROCESS_INFORMATION *PSYSTEM_PROCESS_INFORMATION_PTR;
typedef PSECURITY_DESCRIPTOR *PSECURITY_DESCRIPTOR_PTR;
typedef HCATADMIN *HCATADMIN_PTR;
typedef HIMAGELIST *HIMAGELIST_PTR;
typedef HBITMAP *HBITMAP_PTR;
typedef PVOID *PVOID_PTR;
typedef PBYTE *PBYTE_PTR;
typedef HICON *HICON_PTR;
typedef HDWP *HDWP_PTR;
typedef PACL *PACL_PTR;

typedef COMDLG_FILTERSPEC *LPCOMDLG_FILTERSPEC;
typedef TASKDIALOGCONFIG *LPTASKDIALOGCONFIG;
typedef IRegisteredTask *IRegisteredTaskPtr;

//
// Exported function definitions
//

// I_QueryTagInformation
typedef ULONG (NTAPI *IQTI) (
	_In_opt_ PCWSTR MachineName,
	_In_ TAG_INFO_LEVEL InfoLevel,
	_Inout_ PTAG_INFO_NAME_FROM_TAG TagInfo
	);

// AdjustWindowRectExForDpi (win10rs1+)
typedef BOOL (WINAPI *AWRFD)(
	_Inout_ LPRECT lpRect,
	_In_ ULONG dwStyle,
	_In_ BOOL bMenu,
	_In_ ULONG dwExStyle,
	_In_ UINT dpi
	);

// GetDpiForSystem (win10rs1+)
typedef UINT (WINAPI *GDFS)(
	VOID
	);

// GetDpiForWindow (win10rs1+)
typedef UINT (WINAPI *GDFW)(
	_In_ HWND hwnd
	);

// GetSystemMetricsForDpi (win10rs1+)
typedef INT (WINAPI *GSMFD)(
	_In_ INT nIndex,
	_In_ UINT dpi
	);

// SystemParametersInfoForDpi (win10rs1+)
typedef BOOL (WINAPI *SPIFP)(
	_In_ UINT uiAction,
	_In_ UINT uiParam,
	_Pre_maybenull_ _Post_valid_ PVOID pvParam,
	_In_ UINT fWinIni,
	_In_ UINT dpi
	);

// OpenThemeDataForDpi (win10rs2+)
typedef HTHEME (WINAPI *OTDFD)(
	_In_opt_ HWND hwnd,
	_In_ LPCWSTR pszClassList,
	_In_ UINT dpi
	);

// AllowDarkModeForApp (win10rs5+)
typedef BOOL (WINAPI* ADMFA)(
	_In_ BOOL Enabled
	);

// FlushMenuThemes (win10rs5+)
typedef VOID (WINAPI* FMT)();

// SetPreferredAppMode (win10rs5+)
typedef BOOL (WINAPI* SPAM)(
	_In_ PreferredAppMode AppMode
	);

// RefreshImmersiveColorPolicyState (win10rs5+)
typedef VOID (WINAPI* RICPS)();

// GetIsImmersiveColorUsingHighContrast (win10rs5+)
typedef BOOL (WINAPI* GIICUHC)(
	_In_ IMMERSIVE_HC_CACHE_MODE Mode
	);

// OpenNcThemeData (win10rs5+)
typedef HTHEME (WINAPI* ONCTD)(
	_In_ HWND hWnd,
	_In_ LPCWSTR classList
	);

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

//
// Configuration
//

#define PR_DEVICE_COUNT 26
#define PR_DEVICE_PREFIX_LENGTH 64

#define PR_SIZE_TREEINDENT 12
#define PR_SIZE_ITEMHEIGHT 20
#define PR_SIZE_FOOTERHEIGHT 48

#define PR_SIZE_BUFFER_OVERFLOW (256 * 1024 * 1024) // 256 MB
#define PR_SIZE_BUFFER 0x8000
#define PR_SIZE_CONCAT_LENGTH_CACHE 16
#define PR_SIZE_MAX_STRING_LENGTH (LONG_MAX - 1)

//
// Dark theme
//

typedef struct _R_THEME_CONTEXT
{
	POINT pt;

	WNDPROC wnd_proc;

	HTHEME htheme;

	HDC hdc;

	BOOLEAN is_mouseactive;
} R_THEME_CONTEXT, *PR_THEME_CONTEXT;

//
// Messages
//

// not really used in our case but part of the other structures
typedef struct _UAHMENUPOPUPMETRICS
{
	ULONG rgcx[4];
	ULONG fUpdateMaxWidths : 2; // from kernel symbols, padded to full dword
} UAHMENUPOPUPMETRICS, *LPUAHMENUPOPUPMETRICS;

// describes the sizes of the menu bar or menu item
typedef union _UAHMENUITEMMETRICS
{
	// cx appears to be 14 / 0xE less than rcItem's width!
	// cy 0x14 seems stable, i wonder if it is 4 less than rcItem's height which is always 24 atm
	struct
	{
		ULONG cx;
		ULONG cy;
	} rgSizeBar[2];

	struct
	{
		ULONG cx;
		ULONG cy;
	} rgSizePopup[4];
} UAHMENUITEMMETRICS, *LPUAHMENUITEMMETRICS;

// menu items are always referred to by iPosition here
typedef struct _UAHMENUITEM
{
	LONG iPosition; // 0-based position of menu item in menubar
	UAHMENUITEMMETRICS umim;
	UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM, *LPUAHMENUITEM;

// hmenu is the main window menu; hdc is the context to draw in
typedef struct _UAHMENU
{
	HMENU hmenu;
	HDC hdc;
	ULONG dwFlags; // no idea what these mean, in my testing it's either 0x00000a00 or sometimes 0x00000a10
} UAHMENU, *LPUAHMENU;

// the DRAWITEMSTRUCT contains the states of the menu items, as well as
// the position index of the item in the menu, which is duplicated in
// the UAHMENUITEM's iPosition as well
typedef struct _UAHDRAWMENUITEM
{
	DRAWITEMSTRUCT dis;
	UAHMENU um;
	UAHMENUITEM umi;
} UAHDRAWMENUITEM, *LPUAHDRAWMENUITEM;

// window messages related to menu bar drawing
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092

#define EM_SETBKGNDCOLOR (WM_USER + 67)

#define HRGN_FULL ((HRGN)1) // passed by WM_NCPAINT even though it's completely undocumented (wj32)

#define DCX_USESTYLE 0x00010000
#define DCX_NODELETERGN 0x00040000

//
// Logging
//

typedef enum _R_LOG_LEVEL
{
	LOG_LEVEL_DISABLED = 0,
	LOG_LEVEL_DEBUG = 1,
	LOG_LEVEL_INFO = 2,
	LOG_LEVEL_WARNING = 3,
	LOG_LEVEL_ERROR = 4,
	LOG_LEVEL_CRITICAL = 5,
} R_LOG_LEVEL, *PR_LOG_LEVEL;

typedef enum _R_ERROR_TYPE
{
	ET_WINDOWS = 0,
	ET_NATIVE = 1,
	ET_WINHTTP = 2,
} R_ERROR_TYPE, *PR_ERROR_TYPE;

//
// Synchronization: A fast event object.
//

typedef struct _R_EVENT
{
	union
	{
		volatile ULONG_PTR value;

		struct
		{
			USHORT is_set : 1;
			USHORT ref_count : 15;
			UCHAR reserved;
			UCHAR available_for_use;

#if defined(_WIN64)
			ULONG spare_bit;
#endif // _WIN64
		};
	};

	HANDLE event_handle;
} R_EVENT, *PR_EVENT;

C_ASSERT (sizeof (R_EVENT) == sizeof (ULONG_PTR) + sizeof (HANDLE));

#define PR_EVENT_SET 0x01
#define PR_EVENT_SET_SHIFT 0x00
#define PR_EVENT_REFCOUNT_SHIFT 0x01
#define PR_EVENT_REFCOUNT_INC 0x02
#define PR_EVENT_REFCOUNT_MASK (((ULONG_PTR)1 << 15) - 1)

#define PR_EVENT_INIT { { PR_EVENT_REFCOUNT_INC }, NULL }

//
// Synchronization: One-time initialization
//

typedef RTL_RUN_ONCE R_INITONCE;
typedef PRTL_RUN_ONCE PR_INITONCE;

#define PR_INITONCE_INIT RTL_RUN_ONCE_INIT

//
// Synchronization: Auto-dereference pool
//

// The size of the static array in an auto-release pool.
#define PR_AUTO_POOL_STATIC_SIZE 64

// The maximum size of the dynamic array for it to be kept after the auto-release pool is drained.
#define PR_AUTO_POOL_DYNAMIC_BIG_SIZE 256

typedef struct _R_AUTO_POOL
{
	PVOID static_objects[PR_AUTO_POOL_STATIC_SIZE];
	ULONG_PTR static_count;

	ULONG_PTR dynamic_count;
	ULONG_PTR dynamic_allocated;
	PVOID_PTR dynamic_objects;

	struct _R_AUTO_POOL *next_pool;
} R_AUTO_POOL, *PR_AUTO_POOL;

//
// Synchronization: Free list
//

typedef struct _R_FREE_LIST
{
	SLIST_HEADER list_head;

	ULONG_PTR size;

	ULONG count;
	ULONG maximum_count;
} R_FREE_LIST, *PR_FREE_LIST;

typedef struct _R_FREE_LIST_ENTRY
{
	SLIST_ENTRY list_entry;
	QUAD_PTR body;
} R_FREE_LIST_ENTRY, *PR_FREE_LIST_ENTRY;

#if defined(_WIN64)
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, list_entry) == 0x00);
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, body) == 0x10);
#else
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, list_entry) == 0x00);
C_ASSERT (FIELD_OFFSET (R_FREE_LIST_ENTRY, body) == 0x08);
#endif // _WIN64

//
// Synchronization: Queued lock
//

typedef struct _R_QUEUED_LOCK
{
	ULONG_PTR value;
} R_QUEUED_LOCK, *PR_QUEUED_LOCK;

typedef struct DECLSPEC_ALIGN (16) _R_QUEUED_WAIT_BLOCK
{
	// A pointer to the next wait block, i.e. the wait block pushed onto the list before this one.
	struct _R_QUEUED_WAIT_BLOCK *next_block;

	// A pointer to the previous wait block, i.e. the wait block pushed onto the list after this one.
	struct _R_QUEUED_WAIT_BLOCK *previous_block;

	// A pointer to the last wait block, i.e. the first waiter pushed onto the list.
	struct _R_QUEUED_WAIT_BLOCK *last_block;

	ULONG shared_owners;
	ULONG flags;
} R_QUEUED_WAIT_BLOCK, *PR_QUEUED_WAIT_BLOCK;

#define PR_QUEUED_LOCK_INIT {0}

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

typedef struct _R_QUEUED_LOCK R_CONDITION, *PR_CONDITION;

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

typedef struct _R_RUNDOWN_WAIT_BLOCK
{
	R_EVENT wake_event;

	volatile ULONG_PTR count;
} R_RUNDOWN_WAIT_BLOCK, *PR_RUNDOWN_WAIT_BLOCK;

typedef struct _R_QUEUED_LOCK R_RUNDOWN_PROTECT, *PR_RUNDOWN_PROTECT;

#define PR_RUNDOWN_ACTIVE 0x1
#define PR_RUNDOWN_REF_SHIFT 1
#define PR_RUNDOWN_REF_INC 0x2

#define PR_RUNDOWN_PROTECT_INIT PR_QUEUED_LOCK_INIT

//
// Synchronization: Workqueue
//

typedef VOID (NTAPI *PR_WORKQUEUE_FUNCTION) (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
	);

typedef struct _R_ENVIRONMENT
{
	KPRIORITY base_priority : 6; // Base priority increment
	ULONG io_priority : 3; // I/O priority hint
	ULONG page_priority : 3; // Page/memory priority
	ULONG spare_bits : 20;
} R_ENVIRONMENT, *PR_ENVIRONMENT;

typedef struct _R_WORKQUEUE_ITEM
{
	LIST_ENTRY list_entry;
	PR_WORKQUEUE_FUNCTION base_address;
	PVOID context;
} R_WORKQUEUE_ITEM, *PR_WORKQUEUE_ITEM;

typedef struct _R_WORKQUEUE
{
	LIST_ENTRY queue_list_head;

	R_QUEUED_LOCK queue_lock;
	R_QUEUED_LOCK state_lock;

	R_RUNDOWN_PROTECT rundown_protect;
	R_CONDITION queue_empty_condition;

	R_ENVIRONMENT environment;
	PVOID thread_name; // PR_STRING

	HANDLE semaphore_handle;

	ULONG maximum_threads;

	volatile ULONG current_threads;
	volatile ULONG busy_count;

	BOOLEAN is_terminating;
} R_WORKQUEUE, *PR_WORKQUEUE;

//
// Objects reference
//

typedef VOID (NTAPI *PR_OBJECT_CLEANUP_CALLBACK) (
	_In_ PVOID object_body
	);

typedef struct _R_OBJECT_HEADER
{
	struct
	{
		PR_OBJECT_CLEANUP_CALLBACK cleanup_callback;

		volatile LONG ref_count;
	};

	QUAD_PTR body;
} R_OBJECT_HEADER, *PR_OBJECT_HEADER;

#define PR_OBJECT_HEADER_TO_OBJECT(object_header) (&((PR_OBJECT_HEADER)(object_header))->body)

#define PR_OBJECT_TO_OBJECT_HEADER(object) (CONTAINING_RECORD((object), R_OBJECT_HEADER, body))

//
// 8-bit string object
//

typedef struct _R_BYTEREF
{
	ULONG_PTR length;
	LPSTR buffer;
} R_BYTEREF, *PR_BYTEREF;

typedef struct _R_BYTE
{
	union
	{
		R_BYTEREF sr;

		struct
		{
			ULONG_PTR length;
			LPSTR buffer;
		};
	};

	CHAR data[1];
} R_BYTE, *PR_BYTE;

typedef PR_BYTE *PR_BYTE_PTR;

#define PR_BYTEREF_INIT(string) { sizeof(string) - sizeof(ANSI_NULL), (string) }

//
// 16-bit string object
//

typedef struct _R_STRINGREF
{
	ULONG_PTR length;
	LPWSTR buffer;
} R_STRINGREF, *PR_STRINGREF;

typedef struct _R_STRING
{
	union
	{
		R_STRINGREF sr;

		struct
		{
			ULONG_PTR length;
			LPWSTR buffer;
		};
	};

	WCHAR data[1];
} R_STRING, *PR_STRING;

typedef PR_STRING *PR_STRING_PTR;

#define PR_STRINGREF_INIT(string) { sizeof(string) - sizeof(UNICODE_NULL), (string) }

//
// String builder
//

typedef struct _R_STRINGBUILDER
{
	PR_STRING string;
	ULONG_PTR allocated_length;
} R_STRINGBUILDER, *PR_STRINGBUILDER;

//
// Array object
//

typedef struct _R_ARRAY
{
	PR_OBJECT_CLEANUP_CALLBACK cleanup_callback;
	ULONG_PTR allocated_count;
	ULONG_PTR count;
	ULONG_PTR item_size;
	PVOID items;
} R_ARRAY, *PR_ARRAY;

typedef PR_ARRAY *PR_ARRAY_PTR;

//
// List object
//

typedef struct _R_LIST
{
	PR_OBJECT_CLEANUP_CALLBACK cleanup_callback;
	ULONG_PTR allocated_count;
	ULONG_PTR count;
	PVOID_PTR items;
} R_LIST, *PR_LIST;

typedef PR_LIST *PR_LIST_PTR;

//
// Hashtable object
//

typedef struct _R_HASHTABLE_ENTRY
{
	ULONG_PTR next;
	ULONG_PTR hash_code;
	QUAD_PTR body;
} R_HASHTABLE_ENTRY, *PR_HASHTABLE_ENTRY;

typedef struct _R_HASHTABLE
{
	PR_OBJECT_CLEANUP_CALLBACK cleanup_callback;
	PULONG_PTR buckets;
	PVOID entries;
	ULONG_PTR free_entry;
	ULONG_PTR next_entry;
	ULONG_PTR entry_size;
	ULONG_PTR allocated_buckets;
	ULONG_PTR allocated_entries;
	ULONG_PTR count;
} R_HASHTABLE, *PR_HASHTABLE;

typedef PR_HASHTABLE *PR_HASHTABLE_PTR;

#define PR_HASHTABLE_ENTRY_SIZE(inner_size) \
	(UFIELD_OFFSET(R_HASHTABLE_ENTRY, body) + (inner_size))

#define PR_HASHTABLE_GET_ENTRY(hashtable, index) \
	((PR_HASHTABLE_ENTRY)PTR_ADD_OFFSET((hashtable)->entries, PR_HASHTABLE_ENTRY_SIZE((hashtable)->entry_size) * (index)))

#define PR_HASHTABLE_GET_ENTRY_INDEX(hashtable, entry) \
	((ULONG_PTR)(PTR_ADD_OFFSET(entry, -(hashtable)->entries) / PR_HASHTABLE_ENTRY_SIZE((hashtable)->entry_size)))

#define PR_HASHTABLE_INIT_VALUE 0xFF

//
// Hashtable object pointer object
//

typedef struct _R_OBJECT_POINTER
{
	PVOID object_body;
} R_OBJECT_POINTER, *PR_OBJECT_POINTER;

//
// Pointer storage
//

typedef struct _R_STORAGE
{
	ULONG length;
	PVOID buffer;
} R_STORAGE, *PR_STORAGE;

//
// Strings
//

#define PR_MAKE_VERSION_ULONG64(major, minor, build, revision) \
	(((ULONG64)(major) << 48) | \
	((ULONG64)(minor) << 32) | \
	((ULONG64)(build) << 16) | \
	((ULONG64)(revision) << 0))

#define PR_TRIM_START_ONLY 0x0001
#define PR_TRIM_END_ONLY 0x0002

//
// Cryptography
//

typedef struct _R_CRYPT_CONTEXT
{
	BCRYPT_ALG_HANDLE alg_handle;

	union
	{
		BCRYPT_HASH_HANDLE hash_handle;
		BCRYPT_KEY_HANDLE key_handle;
	} u;

	PR_BYTE object_data; // cipher - key object, hashing - hash object
	PR_BYTE block_data; // cipher - block iv, hashing - hash value

	BOOLEAN is_hashing;
} R_CRYPT_CONTEXT, *PR_CRYPT_CONTEXT;

//
// Memory
//

typedef struct _R_MEMORY_OBJECT
{
	ULONG64 total_bytes;
	ULONG64 free_bytes;
	ULONG64 used_bytes;
	ULONG percent;
} R_MEMORY_OBJECT, *PR_MEMORY_OBJECT;

typedef struct _R_MEMORY_INFO
{
	R_MEMORY_OBJECT physical_memory;
	R_MEMORY_OBJECT page_file;
	R_MEMORY_OBJECT system_cache;
} R_MEMORY_INFO, *PR_MEMORY_INFO;

//
// System information
//

//#define WINDOWS_7 0x0601
//#define WINDOWS_8 0x0602
#define WINDOWS_8_1 0x0603
#define WINDOWS_10_TH1 0x0A00 // build 10240 [TH1]
#define WINDOWS_10_TH2 0x0A01 // build 10586 [TH2]
#define WINDOWS_10_RS1 0x0A02 // build 14393 [RS1]
#define WINDOWS_10_RS2 0x0A03 // build 15063 [RS2]
#define WINDOWS_10_RS3 0x0A04 // build 16299 [RS3]
#define WINDOWS_10_RS4 0x0A05 // build 17763 [RS4]
#define WINDOWS_10_RS5 0x0A06 // build 17763 [RS5]
#define WINDOWS_10_19H1 0x0A07 // build 18362 [19H1]
#define WINDOWS_10_19H2 0x0A08 // build 18363 [19H2]
#define WINDOWS_10_20H1 0x0A09 // build 19041 [20H1]
#define WINDOWS_10_20H2 0x0A0A // build 19042 [20H2]
#define WINDOWS_10_21H1 0x0A0B // build 19043 [21H1]
#define WINDOWS_10_21H2 0x0A0C // build 19044 [21H2]
#define WINDOWS_10_22H2 0x0A0D // build 19045 [22H2]
#define WINDOWS_11_21H2 0x0B00 // build 22000 [21H2]
#define WINDOWS_11_22H2 0x0B01 // build 22621 [22H2]
#define WINDOWS_11_23H2 0x0B02 // build 22631 [23H2]

#define WINDOWS_10 WINDOWS_10_TH1
#define WINDOWS_11 WINDOWS_11_21H2

typedef struct _R_THREAD_CONTEXT
{
	PUSER_THREAD_START_ROUTINE base_address;
	PVOID arglist;
} R_THREAD_CONTEXT, *PR_THREAD_CONTEXT;

typedef struct _R_TOKEN_ATTRIBUTES
{
	PSID token_sid;

	struct
	{
		ULONG is_elevated : 1;
		TOKEN_ELEVATION_TYPE elevation_type : 2;
		ULONG spare_bits : 29;
	};

	ULONG reserved;
} R_TOKEN_ATTRIBUTES, *PR_TOKEN_ATTRIBUTES;

//
// File dialog
//

typedef struct _R_FILE_DIALOG
{
	IFileDialog *ifd; // vista+

	ULONG flags;
} R_FILE_DIALOG, *PR_FILE_DIALOG;

#define PR_FILEDIALOG_OPENDIR 0x0001
#define PR_FILEDIALOG_OPENFILE 0x0002
#define PR_FILEDIALOG_SAVEFILE 0x0004

//
// Window management
//

typedef struct _R_SIZE
{
	LONG cx;
	LONG cy;
} R_SIZE, *PR_SIZE;

C_ASSERT (sizeof (R_SIZE) == sizeof (POINT));

C_ASSERT (FIELD_OFFSET (R_SIZE, cx) == FIELD_OFFSET (POINT, x));
C_ASSERT (FIELD_OFFSET (R_SIZE, cy) == FIELD_OFFSET (POINT, y));

typedef struct _R_RECTANGLE
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

C_ASSERT (sizeof (R_RECTANGLE) == sizeof (RECT));

C_ASSERT (FIELD_OFFSET (R_RECTANGLE, left) == FIELD_OFFSET (RECT, left));
C_ASSERT (FIELD_OFFSET (R_RECTANGLE, top) == FIELD_OFFSET (RECT, top));
C_ASSERT (FIELD_OFFSET (R_RECTANGLE, width) == FIELD_OFFSET (RECT, right));
C_ASSERT (FIELD_OFFSET (R_RECTANGLE, height) == FIELD_OFFSET (RECT, bottom));

//
// Window layout
//

typedef struct _R_LAYOUT_ITEM
{
	HWND hwnd;
	HDWP defer_handle;
	struct _R_LAYOUT_ITEM *parent_item;
	R_RECTANGLE rect;
	R_RECTANGLE prev_rect;
	ULONG number_of_children;
	ULONG flags;
} R_LAYOUT_ITEM, *PR_LAYOUT_ITEM;

typedef struct _R_LAYOUT_MANAGER
{
	R_LAYOUT_ITEM root_item;
	R_SIZE original_size;
	PR_LIST list;
	LONG dpi_value;
} R_LAYOUT_MANAGER, *PR_LAYOUT_MANAGER;

typedef struct _R_LAYOUT_ENUM
{
	PR_LAYOUT_MANAGER layout_manager;
	PR_LAYOUT_ITEM layout_item;
	HWND root_hwnd;
} R_LAYOUT_ENUM, *PR_LAYOUT_ENUM;

#define PR_LAYOUT_ANCHOR_LEFT 0x000001
#define PR_LAYOUT_ANCHOR_TOP 0x000002
#define PR_LAYOUT_ANCHOR_RIGHT 0x000004
#define PR_LAYOUT_ANCHOR_BOTTOM 0x000008

#define PR_LAYOUT_DOCK_LEFT 0x000010
#define PR_LAYOUT_DOCK_TOP 0x000020
#define PR_LAYOUT_DOCK_RIGHT 0x000040
#define PR_LAYOUT_DOCK_BOTTOM 0x000080

#define PR_LAYOUT_ANCHOR_ALL 0x0000FF

// invalidate the control when it is resized
#define PR_LAYOUT_FORCE_INVALIDATE 0x001000

// send WM_SIZE message on resize
#define PR_LAYOUT_SEND_NOTIFY 0x002000

// do not calculate anchors for control
#define PR_LAYOUT_NO_ANCHOR 0x004000

//
// Inernet access (WinHTTP)
//

typedef BOOLEAN (NTAPI *PR_INET_DOWNLOAD_CALLBACK) (
	_In_ ULONG total_written,
	_In_ ULONG total_length,
	_In_opt_ PVOID lparam
	);

typedef struct _R_DOWNLOAD_INFO
{
	union
	{
		HANDLE hfile;
		PR_STRING string;
	};

	PR_INET_DOWNLOAD_CALLBACK download_callback;
	PVOID lparam;

	BOOLEAN is_savetofile;
} R_DOWNLOAD_INFO, *PR_DOWNLOAD_INFO;

typedef struct _R_URLPARTS
{
	PR_STRING host;
	PR_STRING path;
	PR_STRING user;
	PR_STRING pass;

	INTERNET_SCHEME scheme;
	INTERNET_PORT port;
} R_URLPARTS, *PR_URLPARTS;

#define PR_URLPARTS_SCHEME 0x000001
#define PR_URLPARTS_HOST 0x000002
#define PR_URLPARTS_PORT 0x000004
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

typedef struct _R_XML_LIBRARY
{
	union
	{
		IXmlReader *reader;
		IXmlWriter *writer;
	};

	PR_XML_STREAM hstream;

	BOOLEAN is_reader;
} R_XML_LIBRARY, *PR_XML_LIBRARY;

//
// Application structures
//

typedef struct _R_SETTINGS_PAGE
{
	HWND hwnd;
	ULONG locale_id;
	LONG dlg_id;
} R_SETTINGS_PAGE, *PR_SETTINGS_PAGE;

#define PR_UPDATE_FLAG_AVAILABLE 0x000001
#define PR_UPDATE_FLAG_INSTALLER 0x000002
#define PR_UPDATE_FLAG_FILE 0x000004

typedef struct _R_UPDATE_COMPONENT
{
	PR_STRING full_name;
	PR_STRING short_name;
	PR_STRING current_version;
	PR_STRING new_version;
	PR_STRING cache_path;
	PR_STRING target_path;
	PR_STRING url;
	ULONG flags;
} R_UPDATE_COMPONENT, *PR_UPDATE_COMPONENT;

typedef struct _R_UPDATE_INFO
{
	PR_ARRAY components;
	HWND htaskdlg;
	HWND hparent;
	HANDLE hthread;
	HINTERNET hsession;
	ULONG flags;
	volatile LONG lock;
	BOOLEAN is_clicked;
} R_UPDATE_INFO, *PR_UPDATE_INFO;

typedef struct _R_SHARED_IMAGE
{
	PVOID hinst;
	HICON hicon;
	LONG icon_id;
	LONG icon_size;
} R_SHARED_IMAGE, *PR_SHARED_IMAGE;

typedef struct _APP_GLOBAL_CONFIG
{
	struct
	{
		WNDPROC wnd_proc;
		HANDLE hmutex;
		HWND hwnd;

		ULONG taskbar_msg;

		BOOLEAN is_needmaximize;
	} main;

	struct
	{
		R_QUEUED_LOCK lock;
		PR_HASHTABLE table;
		PR_LIST available_list;

		PR_STRING resource_name;
		PR_STRING default_name;
		PR_STRING current_name;
	} locale;

	struct
	{
		R_QUEUED_LOCK lock;
		PR_HASHTABLE table;
	} config;

	struct
	{
		DLGPROC wnd_proc;
		PR_ARRAY page_list;
		HWND hwnd;
	} settings;

	struct
	{
		R_UPDATE_INFO info;
	} update;

	struct
	{
		HBRUSH bg_brush;
	} theme;

	struct
	{
		LONG64 last_timestamp;
	} error;
} APP_GLOBAL_CONFIG, *PAPP_GLOBAL_CONFIG;
