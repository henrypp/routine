// routine++
// Copyright (c) 2012-2017 Henry++

#pragma once

#ifndef _APP_NO_WINXP
#undef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif // _APP_NO_WINXP

#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <psapi.h>
#include <uxtheme.h>
#include <time.h>
#include <lm.h>
#include <process.h>
#include <winhttp.h>
#include <subauth.h>
#include <sddl.h>

#include "rconfig.hpp"
#include "rstring.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "winhttp.lib")

#ifndef LVM_RESETEMPTYTEXT
#define LVM_RESETEMPTYTEXT (LVM_FIRST + 84)
#endif

/*
	Unit conversion
*/

#define _R_BYTESIZE_KB (1024)
#define _R_BYTESIZE_MB (1024 * _R_BYTESIZE_KB)

#define _R_SECONDSCLOCK_MSEC (1000)
#define _R_SECONDSCLOCK_MIN(minutes)(60 * (minutes))
#define _R_SECONDSCLOCK_HOUR(hours)((_R_SECONDSCLOCK_MIN (1) * 60) * (hours))
#define _R_SECONDSCLOCK_DAY(days)((_R_SECONDSCLOCK_HOUR (1) * 24) * (days))

/*
	Color shader
*/

#define _R_COLOR_SHADE(clr, percent) RGB ((BYTE)((float)GetRValue (clr) * percent / 100.0), (BYTE)((float)GetGValue (clr) * percent / 100.0), (BYTE)((float)GetBValue (clr) * percent / 100.0))

/*
	Percentage calculation
*/

#define _R_PERCENT_OF(val, total) INT(ceil((double((val)) / double((total))) * 100.0))
#define _R_PERCENT_VAL(val, total) INT(double((total)) * double((val)) / 100.0)

/*
	Debug logging
*/

#define WDBG1(a) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, L"%s", a)
#define WDBG2(a, ...) _r_dbg (__FUNCTIONW__, TEXT(__FILE__), __LINE__, a, __VA_ARGS__)
#define WDBG(a, ...) _r_dbg (nullptr, nullptr, 0, a, __VA_ARGS__)

#define _R_DEBUG_FORMAT L"%s() failed with error code 0x%.8lx (%s)"

void _r_dbg (LPCWSTR function, LPCWSTR file, DWORD line, LPCWSTR format, ...);

void _r_dbg_write (LPCWSTR appname, LPCWSTR appversion, LPCWSTR fn, DWORD result, LPCWSTR desc);
rstring _r_dbg_getpath (LPCWSTR appname);

/*
	Format strings, dates, numbers
*/

rstring _r_fmt (LPCWSTR format, ...);

rstring _r_fmt_date (const LPFILETIME ft, const DWORD flags = FDTF_DEFAULT); // see SHFormatDateTime flags definition
rstring _r_fmt_date (const __time64_t ut, const DWORD flags = FDTF_DEFAULT);

rstring _r_fmt_size64 (ULONGLONG size);

/*
	FastLock is a port of FastResourceLock from PH 1.x.

	The code contains no comments because it is a direct port. Please see FastResourceLock.cs in PH
	1.x for details.

	The fast lock is around 7% faster than the critical section when there is no contention, when
	used solely for mutual exclusion. It is also much smaller than the critical section.

	https://github.com/processhacker2/processhacker
*/

#ifdef _APP_HAVE_FASTLOCK
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
	_R_FASTLOCK ()
	{
		this->Value = 0;
		this->ExclusiveWakeEvent = CreateSemaphoreEx (nullptr, 0, MAXLONG, nullptr, 0, SEMAPHORE_ALL_ACCESS);
		this->SharedWakeEvent = CreateSemaphoreEx (nullptr, 0, MAXLONG, nullptr, 0, SEMAPHORE_ALL_ACCESS);
	}

	~_R_FASTLOCK()
	{
		if (this->ExclusiveWakeEvent)
		{
			CloseHandle (this->ExclusiveWakeEvent);
			this->ExclusiveWakeEvent = nullptr;
		}

		if (this->SharedWakeEvent)
		{
			CloseHandle (this->SharedWakeEvent);
			this->SharedWakeEvent = nullptr;
		}

	}

	ULONG Value;
	HANDLE ExclusiveWakeEvent;
	HANDLE SharedWakeEvent;
} R_FASTLOCK, *P_FASTLOCK;

ULONG _r_fastlock_islocked (P_FASTLOCK plock);

void _r_fastlock_acquireexclusive (P_FASTLOCK plock);
void _r_fastlock_acquireshared (P_FASTLOCK plock);

void _r_fastlock_releaseexclusive (P_FASTLOCK plock);
void _r_fastlock_releaseshared (P_FASTLOCK plock);
#endif // _APP_HAVE_FASTLOCK

/*
	System messages
*/

#define WMSG(a, ...) _r_msg (nullptr, 0, nullptr, nullptr, a, __VA_ARGS__)

INT _r_msg (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR format, ...);
bool _r_msg_taskdialog (const TASKDIALOGCONFIG* ptd, INT* pbutton, INT* pradiobutton, BOOL* pcheckbox); // vista TaskDialogIndirect
HRESULT CALLBACK _r_msg_callback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR ref);

/*
	Clipboard operations
*/

rstring _r_clipboard_get (HWND hwnd);
void _r_clipboard_set (HWND hwnd, LPCWSTR text, SIZE_T length);

/*
	Filesystem
*/

bool _r_fs_delete (LPCWSTR path, bool allowundo = false);
bool _r_fs_exists (LPCWSTR path);
bool _r_fs_mkdir (LPCWSTR path);
void _r_fs_rmdir (LPCWSTR path);
bool _r_fs_readfile (HANDLE h, LPVOID result, DWORD64 size);
DWORD64 _r_fs_size (HANDLE h);
bool _r_fs_move (LPCWSTR path_from, LPCWSTR path_to, DWORD flags = 0);
bool _r_fs_copy (LPCWSTR path_from, LPCWSTR path_to, DWORD flags = 0);

/*
	Paths
*/

rstring _r_path_expand (rstring path);
rstring _r_path_unexpand (rstring path);
rstring _r_path_compact (rstring path, UINT length);
rstring _r_path_extractdir (rstring path);
rstring _r_path_extractfile (rstring path);
#ifdef _APP_HAVE_NTDLL
rstring _r_path_dospathfromnt (LPCWSTR path);
#endif // _APP_HAVE_NTDLL
DWORD _r_path_ntpathfromdos (rstring& path);

/*
	Processes
*/

BOOL _r_process_getpath (HANDLE h, LPWSTR path, DWORD length);

[[deprecated ("May cause troubles on WOW64!")]]
BOOL _r_process_is_exists (LPCWSTR path, const size_t len);

/*
	Strings
*/

WCHAR _r_str_lower (WCHAR chr);
WCHAR _r_str_upper (WCHAR chr);

size_t _r_str_hash (LPCWSTR text);
INT _r_str_versioncompare (LPCWSTR v1, LPCWSTR v2);

/*
	System information
*/

bool _r_sys_adminstate ();

#ifndef _WIN64
bool _r_sys_iswow64 ();
#endif // _WIN64

bool _r_sys_setsecurityattributes (LPSECURITY_ATTRIBUTES sa, DWORD length, PSECURITY_DESCRIPTOR sd);
bool _r_sys_setprivilege (LPCWSTR privileges[], UINT count, bool is_enable);
bool _r_sys_uacstate ();
bool _r_sys_validversion (DWORD major, DWORD minor, DWORD build = 0, BYTE condition = VER_GREATER_EQUAL);
void _r_sleep (DWORD milliseconds);

/*
	Unixtime
*/

__time64_t _r_unixtime_now ();
void _r_unixtime_to_filetime (__time64_t ut, const LPFILETIME pft);
void _r_unixtime_to_systemtime (__time64_t ut, const LPSYSTEMTIME pst);
__time64_t _r_unixtime_from_filetime (const FILETIME* pft);
__time64_t _r_unixtime_from_systemtime (const LPSYSTEMTIME pst);

/*
	Painting
*/

void _r_dc_fillrect (HDC dc, LPRECT rc, COLORREF clr);
int _r_dc_fontsizetoheight (INT size);
int _r_dc_fontheighttosize (INT size);

/*
	Window management
*/

void _r_wnd_addstyle (HWND hwnd, UINT ctrl_id, LONG mask, LONG stateMask, INT index);
void _r_wnd_center (HWND hwnd);
void _r_wnd_changemessagefilter (HWND hwnd, UINT msg, DWORD action);
void _r_wnd_toggle (HWND hwnd, bool show);
void _r_wnd_top (HWND hwnd, bool is_enable);
bool _r_wnd_undercursor (HWND hwnd);

/*
	Inernet access (WinHTTP)
*/

HINTERNET _r_inet_createsession (LPCWSTR useragent);
bool _r_inet_openurl (HINTERNET hsession, LPCWSTR url, HINTERNET* pconnect, HINTERNET* prequest, PDWORD ptotallength);
bool _r_inet_readrequest (HINTERNET hrequest, LPSTR buffer, DWORD length, PDWORD ptotallength);
void _r_inet_close (HINTERNET hinet);

/*
	Other
*/

HICON _r_loadicon (HINSTANCE h, LPCWSTR name, INT d);
bool _r_run (LPCWSTR filename, LPCWSTR cmdline, LPCWSTR cd = nullptr, WORD sw = SW_SHOWDEFAULT);
size_t _r_rnd (size_t start, size_t end);

/*
	Control: common
*/

void _r_ctrl_enable (HWND hwnd, UINT ctrl_id, bool is_enable);

rstring _r_ctrl_gettext (HWND hwnd, UINT ctrl_id);
void _r_ctrl_settext (HWND hwnd, UINT ctrl_id, LPCWSTR str, ...);

bool _r_ctrl_settip (HWND hwnd, UINT ctrl_id, LPWSTR text);
bool _r_ctrl_showtip (HWND hwnd, UINT ctrl_id, INT icon_id, LPCWSTR title, LPCWSTR text);

/*
	Control: listview
*/

INT _r_listview_addcolumn (HWND hwnd, UINT ctrl_id, size_t column_id, LPCWSTR text, UINT width, INT fmt);
INT _r_listview_addgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR text, UINT align, UINT state);
INT _r_listview_additem (HWND hwnd, UINT ctrl_id, size_t item_id, size_t subitem, LPCWSTR text, size_t image = LAST_VALUE, size_t group_id = LAST_VALUE, LPARAM lparam = 0);

void _r_listview_deleteallcolumns (HWND hwnd, UINT ctrl_id);
void _r_listview_deleteallgroups (HWND hwnd, UINT ctrl_id);
void _r_listview_deleteallitems (HWND hwnd, UINT ctrl_id);

INT _r_listview_getcolumnwidth (HWND hwnd, UINT ctrl_id, INT column);
size_t _r_listview_getitemcount (HWND hwnd, UINT ctrl_id, bool list_checked = false);
INT _r_listview_getcolumncount (HWND hwnd, UINT ctrl_id);
LPARAM _r_listview_getitemlparam (HWND hwnd, UINT ctrl_id, size_t item);
rstring _r_listview_getitemtext (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem);

bool _r_listview_isitemchecked (HWND hwnd, UINT ctrl_id, size_t item);
bool _r_listview_isitemvisible (HWND hwnd, UINT ctrl_id, size_t item);

void _r_listview_redraw (HWND hwnd, UINT ctrl_id);

DWORD _r_listview_setstyle (HWND hwnd, UINT ctrl_id, DWORD exstyle);
void _r_listview_setcolumn (HWND hwnd, UINT ctrl_id, UINT column_id, LPCWSTR text, INT width);
void _r_listview_setcolumnsortindex (HWND hwnd, UINT ctrl_id, INT column_id, INT arrow);
INT _r_listview_setitem (HWND hwnd, UINT ctrl_id, size_t item, size_t subitem, LPCWSTR text, size_t image = LAST_VALUE, size_t group_id = LAST_VALUE, LPARAM lparam = 0);
BOOL _r_listview_setitemcheck (HWND hwnd, UINT ctrl_id, size_t item, bool state);
INT _r_listview_setgroup (HWND hwnd, UINT ctrl_id, size_t group_id, LPCWSTR title);

/*
	Control: treeview
*/

HTREEITEM _r_treeview_additem (HWND hwnd, UINT ctrl_id, LPCWSTR text, HTREEITEM parent = nullptr, size_t image = LAST_VALUE, LPARAM lparam = 0);
LPARAM _r_treeview_getlparam (HWND hwnd, UINT ctrl_id, HTREEITEM item);
DWORD _r_treeview_setstyle (HWND hwnd, UINT ctrl_id, DWORD exstyle, INT height);

/*
	Control: statusbar
*/

void _r_status_settext (HWND hwnd, UINT ctrl_id, INT part, LPCWSTR text);
void _r_status_setstyle (HWND hwnd, UINT ctrl_id, INT height);

/*
	Exported function definitions
*/

typedef BOOL (WINAPI *CWMF) (UINT, DWORD); // ChangeWindowMessageFilter
typedef BOOL (WINAPI *CWMFEX) (HWND, UINT, DWORD, PCHANGEFILTERSTRUCT); // ChangeWindowMessageFilterEx
typedef BOOL (WINAPI *IW64P) (HANDLE, PBOOL); // IsWow64Process
typedef HRESULT (WINAPI *LIWSD) (HINSTANCE, PCWSTR, INT, INT, HICON*); // LoadIconWithScaleDown
typedef HRESULT (WINAPI *TDI) (const TASKDIALOGCONFIG*, INT*, INT*, BOOL*); // TaskDialogIndirect
typedef int (WINAPI *SHCDEX) (HWND, LPCTSTR, const SECURITY_ATTRIBUTES*); // SHCreateDirectoryEx
typedef BOOL (WINAPI *QFPIN) (HANDLE, DWORD, LPWSTR, PDWORD); // QueryFullProcessImageName
typedef BOOL (WINAPI *MFACS) (PCWSTR, PDWORD, PDWORD); // MapFileAndCheckSumW

/*
	NTDLL Definitions
*/

#ifdef _APP_HAVE_NTDLL

// rev
// private
// source:http://www.microsoft.com/whdc/system/Sysinternals/MoreThan64proc.mspx
typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBasicInformation, // q: SYSTEM_BASIC_INFORMATION
	SystemProcessorInformation, // q: SYSTEM_PROCESSOR_INFORMATION
	SystemPerformanceInformation, // q: SYSTEM_PERFORMANCE_INFORMATION
	SystemTimeOfDayInformation, // q: SYSTEM_TIMEOFDAY_INFORMATION
	SystemPathInformation, // not implemented
	SystemProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
	SystemCallCountInformation, // q: SYSTEM_CALL_COUNT_INFORMATION
	SystemDeviceInformation, // q: SYSTEM_DEVICE_INFORMATION
	SystemProcessorPerformanceInformation, // q: SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
	SystemFlagsInformation, // q: SYSTEM_FLAGS_INFORMATION
	SystemCallTimeInformation, // not implemented // SYSTEM_CALL_TIME_INFORMATION // 10
	SystemModuleInformation, // q: RTL_PROCESS_MODULES
	SystemLocksInformation, // q: RTL_PROCESS_LOCKS
	SystemStackTraceInformation, // q: RTL_PROCESS_BACKTRACES
	SystemPagedPoolInformation, // not implemented
	SystemNonPagedPoolInformation, // not implemented
	SystemHandleInformation, // q: SYSTEM_HANDLE_INFORMATION
	SystemObjectInformation, // q: SYSTEM_OBJECTTYPE_INFORMATION mixed with SYSTEM_OBJECT_INFORMATION
	SystemPageFileInformation, // q: SYSTEM_PAGEFILE_INFORMATION
	SystemVdmInstemulInformation, // q
	SystemVdmBopInformation, // not implemented // 20
	SystemFileCacheInformation, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemCache)
	SystemPoolTagInformation, // q: SYSTEM_POOLTAG_INFORMATION
	SystemInterruptInformation, // q: SYSTEM_INTERRUPT_INFORMATION
	SystemDpcBehaviorInformation, // q: SYSTEM_DPC_BEHAVIOR_INFORMATION; s: SYSTEM_DPC_BEHAVIOR_INFORMATION (requires SeLoadDriverPrivilege)
	SystemFullMemoryInformation, // not implemented
	SystemLoadGdiDriverInformation, // s (kernel-mode only)
	SystemUnloadGdiDriverInformation, // s (kernel-mode only)
	SystemTimeAdjustmentInformation, // q: SYSTEM_QUERY_TIME_ADJUST_INFORMATION; s: SYSTEM_SET_TIME_ADJUST_INFORMATION (requires SeSystemtimePrivilege)
	SystemSummaryMemoryInformation, // not implemented
	SystemMirrorMemoryInformation, // s (requires license value "Kernel-MemoryMirroringSupported") (requires SeShutdownPrivilege) // 30
	SystemPerformanceTraceInformation, // q; s: (type depends on EVENT_TRACE_INFORMATION_CLASS)
	SystemObsolete0, // not implemented
	SystemExceptionInformation, // q: SYSTEM_EXCEPTION_INFORMATION
	SystemCrashDumpStateInformation, // s (requires SeDebugPrivilege)
	SystemKernelDebuggerInformation, // q: SYSTEM_KERNEL_DEBUGGER_INFORMATION
	SystemContextSwitchInformation, // q: SYSTEM_CONTEXT_SWITCH_INFORMATION
	SystemRegistryQuotaInformation, // q: SYSTEM_REGISTRY_QUOTA_INFORMATION; s (requires SeIncreaseQuotaPrivilege)
	SystemExtendServiceTableInformation, // s (requires SeLoadDriverPrivilege) // loads win32k only
	SystemPrioritySeperation, // s (requires SeTcbPrivilege)
	SystemVerifierAddDriverInformation, // s (requires SeDebugPrivilege) // 40
	SystemVerifierRemoveDriverInformation, // s (requires SeDebugPrivilege)
	SystemProcessorIdleInformation, // q: SYSTEM_PROCESSOR_IDLE_INFORMATION
	SystemLegacyDriverInformation, // q: SYSTEM_LEGACY_DRIVER_INFORMATION
	SystemCurrentTimeZoneInformation, // q
	SystemLookasideInformation, // q: SYSTEM_LOOKASIDE_INFORMATION
	SystemTimeSlipNotification, // s (requires SeSystemtimePrivilege)
	SystemSessionCreate, // not implemented
	SystemSessionDetach, // not implemented
	SystemSessionInformation, // not implemented
	SystemRangeStartInformation, // q: SYSTEM_RANGE_START_INFORMATION // 50
	SystemVerifierInformation, // q: SYSTEM_VERIFIER_INFORMATION; s (requires SeDebugPrivilege)
	SystemVerifierThunkExtend, // s (kernel-mode only)
	SystemSessionProcessInformation, // q: SYSTEM_SESSION_PROCESS_INFORMATION
	SystemLoadGdiDriverInSystemSpace, // s (kernel-mode only) (same as SystemLoadGdiDriverInformation)
	SystemNumaProcessorMap, // q
	SystemPrefetcherInformation, // q: PREFETCHER_INFORMATION; s: PREFETCHER_INFORMATION // PfSnQueryPrefetcherInformation
	SystemExtendedProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
	SystemRecommendedSharedDataAlignment, // q
	SystemComPlusPackage, // q; s
	SystemNumaAvailableMemory, // 60
	SystemProcessorPowerInformation, // q: SYSTEM_PROCESSOR_POWER_INFORMATION
	SystemEmulationBasicInformation, // q
	SystemEmulationProcessorInformation,
	SystemExtendedHandleInformation, // q: SYSTEM_HANDLE_INFORMATION_EX
	SystemLostDelayedWriteInformation, // q: ULONG
	SystemBigPoolInformation, // q: SYSTEM_BIGPOOL_INFORMATION
	SystemSessionPoolTagInformation, // q: SYSTEM_SESSION_POOLTAG_INFORMATION
	SystemSessionMappedViewInformation, // q: SYSTEM_SESSION_MAPPED_VIEW_INFORMATION
	SystemHotpatchInformation, // q; s
	SystemObjectSecurityMode, // q // 70
	SystemWatchdogTimerHandler, // s (kernel-mode only)
	SystemWatchdogTimerInformation, // q (kernel-mode only); s (kernel-mode only)
	SystemLogicalProcessorInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION
	SystemWow64SharedInformationObsolete, // not implemented
	SystemRegisterFirmwareTableInformationHandler, // s (kernel-mode only)
	SystemFirmwareTableInformation, // SYSTEM_FIRMWARE_TABLE_INFORMATION
	SystemModuleInformationEx, // q: RTL_PROCESS_MODULE_INFORMATION_EX
	SystemVerifierTriageInformation, // not implemented
	SystemSuperfetchInformation, // q; s: SUPERFETCH_INFORMATION // PfQuerySuperfetchInformation
	SystemMemoryListInformation, // q: SYSTEM_MEMORY_LIST_INFORMATION; s: SYSTEM_MEMORY_LIST_COMMAND (requires SeProfileSingleProcessPrivilege) // 80
	SystemFileCacheInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (same as SystemFileCacheInformation)
	SystemThreadPriorityClientIdInformation, // s: SYSTEM_THREAD_CID_PRIORITY_INFORMATION (requires SeIncreaseBasePriorityPrivilege)
	SystemProcessorIdleCycleTimeInformation, // q: SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION[]
	SystemVerifierCancellationInformation, // not implemented // name:wow64:whNT32QuerySystemVerifierCancellationInformation
	SystemProcessorPowerInformationEx, // not implemented
	SystemRefTraceInformation, // q; s: SYSTEM_REF_TRACE_INFORMATION // ObQueryRefTraceInformation
	SystemSpecialPoolInformation, // q; s (requires SeDebugPrivilege) // MmSpecialPoolTag, then MmSpecialPoolCatchOverruns != 0
	SystemProcessIdInformation, // q: SYSTEM_PROCESS_ID_INFORMATION
	SystemErrorPortInformation, // s (requires SeTcbPrivilege)
	SystemBootEnvironmentInformation, // q: SYSTEM_BOOT_ENVIRONMENT_INFORMATION // 90
	SystemHypervisorInformation, // q; s (kernel-mode only)
	SystemVerifierInformationEx, // q; s: SYSTEM_VERIFIER_INFORMATION_EX
	SystemTimeZoneInformation, // s (requires SeTimeZonePrivilege)
	SystemImageFileExecutionOptionsInformation, // s: SYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION (requires SeTcbPrivilege)
	SystemCoverageInformation, // q; s // name:wow64:whNT32QuerySystemCoverageInformation; ExpCovQueryInformation
	SystemPrefetchPatchInformation, // not implemented
	SystemVerifierFaultsInformation, // s (requires SeDebugPrivilege)
	SystemSystemPartitionInformation, // q: SYSTEM_SYSTEM_PARTITION_INFORMATION
	SystemSystemDiskInformation, // q: SYSTEM_SYSTEM_DISK_INFORMATION
	SystemProcessorPerformanceDistribution, // q: SYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION // 100
	SystemNumaProximityNodeInformation, // q
	SystemDynamicTimeZoneInformation, // q; s (requires SeTimeZonePrivilege)
	SystemCodeIntegrityInformation, // q: SYSTEM_CODEINTEGRITY_INFORMATION // SeCodeIntegrityQueryInformation
	SystemProcessorMicrocodeUpdateInformation, // s
	SystemProcessorBrandString, // q // HaliQuerySystemInformation -> HalpGetProcessorBrandString, info class 23
	SystemVirtualAddressInformation, // q: SYSTEM_VA_LIST_INFORMATION[]; s: SYSTEM_VA_LIST_INFORMATION[] (requires SeIncreaseQuotaPrivilege) // MmQuerySystemVaInformation
	SystemLogicalProcessorAndGroupInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX // since WIN7 // KeQueryLogicalProcessorRelationship
	SystemProcessorCycleTimeInformation, // q: SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION[]
	SystemStoreInformation, // q; s // SmQueryStoreInformation
	SystemRegistryAppendString, // s: SYSTEM_REGISTRY_APPEND_STRING_PARAMETERS // 110
	SystemAitSamplingValue, // s: ULONG (requires SeProfileSingleProcessPrivilege)
	SystemVhdBootInformation, // q: SYSTEM_VHD_BOOT_INFORMATION
	SystemCpuQuotaInformation, // q; s // PsQueryCpuQuotaInformation
	SystemNativeBasicInformation, // not implemented
	SystemSpare1, // not implemented
	SystemLowPriorityIoInformation, // q: SYSTEM_LOW_PRIORITY_IO_INFORMATION
	SystemTpmBootEntropyInformation, // q: TPM_BOOT_ENTROPY_NT_RESULT // ExQueryTpmBootEntropyInformation
	SystemVerifierCountersInformation, // q: SYSTEM_VERIFIER_COUNTERS_INFORMATION
	SystemPagedPoolInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypePagedPool)
	SystemSystemPtesInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemPtes) // 120
	SystemNodeDistanceInformation, // q
	SystemAcpiAuditInformation, // q: SYSTEM_ACPI_AUDIT_INFORMATION // HaliQuerySystemInformation -> HalpAuditQueryResults, info class 26
	SystemBasicPerformanceInformation, // q: SYSTEM_BASIC_PERFORMANCE_INFORMATION // name:wow64:whNtQuerySystemInformation_SystemBasicPerformanceInformation
	SystemQueryPerformanceCounterInformation, // q: SYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION // since WIN7 SP1
	SystemSessionBigPoolInformation, // q: SYSTEM_SESSION_POOLTAG_INFORMATION // since WIN8
	SystemBootGraphicsInformation, // q; s: SYSTEM_BOOT_GRAPHICS_INFORMATION (kernel-mode only)
	SystemScrubPhysicalMemoryInformation, // q; s: MEMORY_SCRUB_INFORMATION
	SystemBadPageInformation,
	SystemProcessorProfileControlArea, // q; s: SYSTEM_PROCESSOR_PROFILE_CONTROL_AREA
	SystemCombinePhysicalMemoryInformation, // s: MEMORY_COMBINE_INFORMATION, MEMORY_COMBINE_INFORMATION_EX, MEMORY_COMBINE_INFORMATION_EX2 // 130
	SystemEntropyInterruptTimingCallback,
	SystemConsoleInformation, // q: SYSTEM_CONSOLE_INFORMATION
	SystemPlatformBinaryInformation, // q: SYSTEM_PLATFORM_BINARY_INFORMATION
	SystemThrottleNotificationInformation,
	SystemHypervisorProcessorCountInformation, // q: SYSTEM_HYPERVISOR_PROCESSOR_COUNT_INFORMATION
	SystemDeviceDataInformation, // q: SYSTEM_DEVICE_DATA_INFORMATION
	SystemDeviceDataEnumerationInformation,
	SystemMemoryTopologyInformation, // q: SYSTEM_MEMORY_TOPOLOGY_INFORMATION
	SystemMemoryChannelInformation, // q: SYSTEM_MEMORY_CHANNEL_INFORMATION
	SystemBootLogoInformation, // q: SYSTEM_BOOT_LOGO_INFORMATION // 140
	SystemProcessorPerformanceInformationEx, // q: SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_EX // since WINBLUE
	SystemSpare0,
	SystemSecureBootPolicyInformation, // q: SYSTEM_SECUREBOOT_POLICY_INFORMATION
	SystemPageFileInformationEx, // q: SYSTEM_PAGEFILE_INFORMATION_EX
	SystemSecureBootInformation, // q: SYSTEM_SECUREBOOT_INFORMATION
	SystemEntropyInterruptTimingRawInformation,
	SystemPortableWorkspaceEfiLauncherInformation, // q: SYSTEM_PORTABLE_WORKSPACE_EFI_LAUNCHER_INFORMATION
	SystemFullProcessInformation, // q: SYSTEM_PROCESS_INFORMATION with SYSTEM_PROCESS_INFORMATION_EXTENSION (requires admin)
	SystemKernelDebuggerInformationEx, // q: SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX
	SystemBootMetadataInformation, // 150
	SystemSoftRebootInformation,
	SystemElamCertificateInformation, // s: SYSTEM_ELAM_CERTIFICATE_INFORMATION
	SystemOfflineDumpConfigInformation,
	SystemProcessorFeaturesInformation, // q: SYSTEM_PROCESSOR_FEATURES_INFORMATION
	SystemRegistryReconciliationInformation,
	SystemEdidInformation,
	SystemManufacturingInformation, // q: SYSTEM_MANUFACTURING_INFORMATION // since THRESHOLD
	SystemEnergyEstimationConfigInformation, // q: SYSTEM_ENERGY_ESTIMATION_CONFIG_INFORMATION
	SystemHypervisorDetailInformation, // q: SYSTEM_HYPERVISOR_DETAIL_INFORMATION
	SystemProcessorCycleStatsInformation, // q: SYSTEM_PROCESSOR_CYCLE_STATS_INFORMATION // 160
	SystemVmGenerationCountInformation,
	SystemTrustedPlatformModuleInformation, // q: SYSTEM_TPM_INFORMATION
	SystemKernelDebuggerFlags,
	SystemCodeIntegrityPolicyInformation, // q: SYSTEM_CODEINTEGRITYPOLICY_INFORMATION
	SystemIsolatedUserModeInformation, // q: SYSTEM_ISOLATED_USER_MODE_INFORMATION
	SystemHardwareSecurityTestInterfaceResultsInformation,
	SystemSingleModuleInformation, // q: SYSTEM_SINGLE_MODULE_INFORMATION
	SystemAllowedCpuSetsInformation,
	SystemDmaProtectionInformation, // q: SYSTEM_DMA_PROTECTION_INFORMATION
	SystemInterruptCpuSetsInformation, // q: SYSTEM_INTERRUPT_CPU_SET_INFORMATION // 170
	SystemSecureBootPolicyFullInformation, // q: SYSTEM_SECUREBOOT_POLICY_FULL_INFORMATION
	SystemCodeIntegrityPolicyFullInformation,
	SystemAffinitizedInterruptProcessorInformation,
	SystemRootSiloInformation, // q: SYSTEM_ROOT_SILO_INFORMATION
	SystemCpuSetInformation, // q: SYSTEM_CPU_SET_INFORMATION // since THRESHOLD2
	SystemCpuSetTagInformation, // q: SYSTEM_CPU_SET_TAG_INFORMATION
	SystemWin32WerStartCallout,
	SystemSecureKernelProfileInformation, // q: SYSTEM_SECURE_KERNEL_HYPERGUARD_PROFILE_INFORMATION
	SystemCodeIntegrityPlatformManifestInformation, // q: SYSTEM_SECUREBOOT_PLATFORM_MANIFEST_INFORMATION // since REDSTONE
	SystemInterruptSteeringInformation, // 180
	SystemSupportedProcessorArchitectures,
	SystemMemoryUsageInformation, // q: SYSTEM_MEMORY_USAGE_INFORMATION
	SystemCodeIntegrityCertificateInformation, // q: SYSTEM_CODEINTEGRITY_CERTIFICATE_INFORMATION
	SystemPhysicalMemoryInformation, // q: SYSTEM_PHYSICAL_MEMORY_INFORMATION // since REDSTONE2
	SystemControlFlowTransition,
	SystemKernelDebuggingAllowed,
	SystemActivityModerationExeState, // SYSTEM_ACTIVITY_MODERATION_EXE_STATE
	SystemActivityModerationUserSettings, // SYSTEM_ACTIVITY_MODERATION_USER_SETTINGS
	SystemCodeIntegrityPoliciesFullInformation,
	SystemCodeIntegrityUnlockInformation, // SYSTEM_CODEINTEGRITY_UNLOCK_INFORMATION // 190
	SystemIntegrityQuotaInformation,
	SystemFlushInformation, // q: SYSTEM_FLUSH_INFORMATION
	MaxSystemInfoClass
} SYSTEM_INFORMATION_CLASS;

typedef struct _CLIENT_ID
{
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef struct _SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	LONG Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef enum _OBJECT_INFORMATION_CLASS
{
	ObjectBasicInformation, // OBJECT_BASIC_INFORMATION
	ObjectNameInformation, // OBJECT_NAME_INFORMATION
} OBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_NAME_INFORMATION
{
	UNICODE_STRING Name; // defined in winternl.h
	WCHAR NameBuffer;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER WorkingSetPrivateSize; // since VISTA
	ULONG HardFaultCount; // since WIN7
	ULONG NumberOfThreadsHighWatermark; // since WIN7
	ULONGLONG CycleTime; // since WIN7
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	LONG BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey; // since VISTA (requires SystemExtendedProcessInformation)
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[1];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

struct SYSTEM_CACHE_INFORMATION
{
	ULONG_PTR	CurrentSize;
	ULONG_PTR	PeakSize;
	ULONG_PTR	PageFaultCount;
	ULONG_PTR	MinimumWorkingSet;
	ULONG_PTR	MaximumWorkingSet;
	ULONG_PTR	TransitionSharedPages;
	ULONG_PTR	PeakTransitionSharedPages;
	DWORD		Unused[2];
};

typedef enum _SYSTEM_MEMORY_LIST_COMMAND
{
	MemoryCaptureAccessedBits,
	MemoryCaptureAndResetAccessedBits,
	MemoryEmptyWorkingSets,
	MemoryFlushModifiedList,
	MemoryPurgeStandbyList,
	MemoryPurgeLowPriorityStandbyList,
	MemoryCommandMax
} SYSTEM_MEMORY_LIST_COMMAND;

typedef struct _MEMORY_COMBINE_INFORMATION_EX
{
	HANDLE Handle;
	ULONG_PTR PagesCombined;
	ULONG Flags;
} MEMORY_COMBINE_INFORMATION_EX, *PMEMORY_COMBINE_INFORMATION_EX;

extern "C" {
	NTSYSCALLAPI
		NTSTATUS
		NTAPI
		NtQueryObject (
		_In_ HANDLE Handle,
		_In_ UINT ObjectInformationClass,
		_Out_writes_bytes_opt_ (ObjectInformationLength) PVOID ObjectInformation,
		_In_ ULONG ObjectInformationLength,
		_Out_opt_ PULONG ReturnLength
		);

	NTSYSCALLAPI
		NTSTATUS
		NTAPI
		NtQuerySystemInformation (
		_In_ UINT SystemInformationClass,
		_Out_writes_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
		_In_ ULONG SystemInformationLength,
		_Out_opt_ PULONG ReturnLength
		);

	NTSYSCALLAPI
		NTSTATUS
		NTAPI
		NtSetSystemInformation (
		_In_ UINT SystemInformationClass,
		_In_reads_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
		_In_ ULONG SystemInformationLength
		);
};
#endif // _APP_HAVE_NTDLL
