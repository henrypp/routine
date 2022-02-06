// routine.c
// project sdk library
//
// Copyright (c) 2020-2022 Henry++

#pragma once

//
// Libraries
//

#pragma comment(lib, "ntdll.lib")

//
// Calling convention
//

#ifndef _WIN64
#define FASTCALL __fastcall
#else
#define FASTCALL
#endif

//
// Errors
//

#ifndef NT_FACILITY_MASK
#define NT_FACILITY_MASK 0xfff
#endif

#ifndef NT_FACILITY_SHIFT
#define NT_FACILITY_SHIFT 16
#endif

#ifndef NT_FACILITY
#define NT_FACILITY(Status) ((((ULONG)(Status)) >> NT_FACILITY_SHIFT) & NT_FACILITY_MASK)
#endif

#ifndef NT_NTWIN32
#define NT_NTWIN32(Status) (NT_FACILITY(Status) == FACILITY_NTWIN32)
#endif

#ifndef WIN32_FROM_NTSTATUS
#define WIN32_FROM_NTSTATUS(Status) (((ULONG)(Status)) & 0xffff)
#endif

//
// Memory
//

#ifndef PTR_ADD_OFFSET
#define PTR_ADD_OFFSET(pointer, offset) ((PVOID)((ULONG_PTR)(pointer) + (ULONG_PTR)(offset)))
#endif

#ifndef PTR_SUB_OFFSET
#define PTR_SUB_OFFSET(pointer, offset) ((PVOID)((ULONG_PTR)(pointer) - (ULONG_PTR)(offset)))
#endif

#define ALIGN_UP_BY(address, align) (((ULONG_PTR)(address) + (align) - 1) & ~((align) - 1))
#define ALIGN_UP_POINTER_BY(pointer, align) ((PVOID)ALIGN_UP_BY(pointer, align))
#define ALIGN_UP(address, type) ALIGN_UP_BY(address, sizeof(type))
#define ALIGN_UP_POINTER(pointer, type) ((PVOID)ALIGN_UP(pointer, type))
#define ALIGN_DOWN_BY(address, align) ((ULONG_PTR)(address) & ~((ULONG_PTR)(align) - 1))
#define ALIGN_DOWN_POINTER_BY(pointer, align) ((PVOID)ALIGN_DOWN_BY(pointer, align))
#define ALIGN_DOWN(address, type) ALIGN_DOWN_BY(address, sizeof(type))
#define ALIGN_DOWN_POINTER(pointer, type) ((PVOID)ALIGN_DOWN(pointer, type))

#define PAGE_SIZE 0x1000

//
// Icons (shell32)
//

#ifndef SIH_APPLICATION
#define SIH_APPLICATION 32512
#endif

#ifndef SIH_ERROR
#define SIH_ERROR 32513
#endif

#ifndef SIH_QUESTION
#define SIH_QUESTION 32514
#endif

#ifndef SIH_EXCLAMATION
#define SIH_EXCLAMATION 32515
#endif

#ifndef SIH_INFORMATION
#define SIH_INFORMATION 32516
#endif

#ifndef SIH_WINLOGO
#define SIH_WINLOGO 32517
#endif

#ifndef SIH_SHIELD
#define SIH_SHIELD 32518
#endif

//
// Path separator
//

#ifndef OBJ_NAME_PATH_SEPARATOR
#define OBJ_NAME_PATH_SEPARATOR L'\\'
#endif

//
// Undocumented codes
//

#ifndef LVM_RESETEMPTYTEXT
#define LVM_RESETEMPTYTEXT (LVM_FIRST + 84)
#endif

#ifndef WM_COPYGLOBALDATA
#define WM_COPYGLOBALDATA 0x0049
#endif

//
// Privileges
//

#define SE_MIN_WELL_KNOWN_PRIVILEGE (2L)
#define SE_CREATE_TOKEN_PRIVILEGE (2L)
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE (3L)
#define SE_LOCK_MEMORY_PRIVILEGE (4L)
#define SE_INCREASE_QUOTA_PRIVILEGE (5L)
#define SE_MACHINE_ACCOUNT_PRIVILEGE (6L)
#define SE_TCB_PRIVILEGE (7L)
#define SE_SECURITY_PRIVILEGE (8L)
#define SE_TAKE_OWNERSHIP_PRIVILEGE (9L)
#define SE_LOAD_DRIVER_PRIVILEGE (10L)
#define SE_SYSTEM_PROFILE_PRIVILEGE (11L)
#define SE_SYSTEMTIME_PRIVILEGE (12L)
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE (13L)
#define SE_INC_BASE_PRIORITY_PRIVILEGE (14L)
#define SE_CREATE_PAGEFILE_PRIVILEGE (15L)
#define SE_CREATE_PERMANENT_PRIVILEGE (16L)
#define SE_BACKUP_PRIVILEGE (17L)
#define SE_RESTORE_PRIVILEGE (18L)
#define SE_SHUTDOWN_PRIVILEGE (19L)
#define SE_DEBUG_PRIVILEGE (20L)
#define SE_AUDIT_PRIVILEGE (21L)
#define SE_SYSTEM_ENVIRONMENT_PRIVILEGE (22L)
#define SE_CHANGE_NOTIFY_PRIVILEGE (23L)
#define SE_REMOTE_SHUTDOWN_PRIVILEGE (24L)
#define SE_UNDOCK_PRIVILEGE (25L)
#define SE_SYNC_AGENT_PRIVILEGE (26L)
#define SE_ENABLE_DELEGATION_PRIVILEGE (27L)
#define SE_MANAGE_VOLUME_PRIVILEGE (28L)
#define SE_IMPERSONATE_PRIVILEGE (29L)
#define SE_CREATE_GLOBAL_PRIVILEGE (30L)
#define SE_TRUSTED_CREDMAN_ACCESS_PRIVILEGE (31L)
#define SE_RELABEL_PRIVILEGE (32L)
#define SE_INC_WORKING_SET_PRIVILEGE (33L)
#define SE_TIME_ZONE_PRIVILEGE (34L)
#define SE_CREATE_SYMBOLIC_LINK_PRIVILEGE (35L)
#define SE_DELEGATE_SESSION_USER_IMPERSONATE_PRIVILEGE (36L)
#define SE_MAX_WELL_KNOWN_PRIVILEGE SE_DELEGATE_SESSION_USER_IMPERSONATE_PRIVILEGE

// This isn't in NT, but it's useful.
typedef struct DECLSPEC_ALIGN (MEMORY_ALLOCATION_ALIGNMENT) _QUAD_PTR
{
	ULONG_PTR DoNotUseThisField1;
	ULONG_PTR DoNotUseThisField2;
} QUAD_PTR, *PQUAD_PTR;

#include <pshpack1.h>
typedef struct _DLGTEMPLATEEX
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cdit;
	SHORT x;
	SHORT y;
	SHORT cx;
	SHORT cy;

	// Everything else in this structure is variable length,
	// and therefore must be determined dynamically

	// sz_Or_Ord menu;			// name or ordinal of a menu resource
	// sz_Or_Ord windowClass;	// name or ordinal of a window class
	// WCHAR title[titleLen];	// title string of the dialog box
	// SHORT pointsize;			// only if DS_SETFONT is set
	// SHORT weight;			// only if DS_SETFONT is set
	// SHORT bItalic;			// only if DS_SETFONT is set
	// WCHAR font[fontLen];		// typeface name, if DS_SETFONT is set
} DLGTEMPLATEEX, *LPDLGTEMPLATEEX;

typedef struct _DLGITEMTEMPLATEEX
{
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	SHORT x;
	SHORT y;
	SHORT cx;
	SHORT cy;
	DWORD id;

	// Everything else in this structure is variable length,
	// and therefore must be determined dynamically

	// sz_Or_Ord windowClass;	// name or ordinal of a window class
	// sz_Or_Ord title;			// title string or ordinal of a resource
	// WORD extraCount;			// bytes following creation data
} DLGITEMTEMPLATEEX, *LPDLGITEMTEMPLATEEX;
#include <poppack.h>

//
// types
//

typedef UCHAR KIRQL, *PKIRQL;
typedef LONG KPRIORITY;
typedef USHORT RTL_ATOM, *PRTL_ATOM;

//
// enums
//

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
	SystemVdmInstemulInformation, // q: SYSTEM_VDM_INSTEMUL_INFO
	SystemVdmBopInformation, // not implemented // 20
	SystemFileCacheInformation, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemCache)
	SystemPoolTagInformation, // q: SYSTEM_POOLTAG_INFORMATION
	SystemInterruptInformation, // q: SYSTEM_INTERRUPT_INFORMATION
	SystemDpcBehaviorInformation, // q: SYSTEM_DPC_BEHAVIOR_INFORMATION; s: SYSTEM_DPC_BEHAVIOR_INFORMATION (requires SeLoadDriverPrivilege)
	SystemFullMemoryInformation, // not implemented // SYSTEM_MEMORY_USAGE_INFORMATION
	SystemLoadGdiDriverInformation, // s (kernel-mode only)
	SystemUnloadGdiDriverInformation, // s (kernel-mode only)
	SystemTimeAdjustmentInformation, // q: SYSTEM_QUERY_TIME_ADJUST_INFORMATION; s: SYSTEM_SET_TIME_ADJUST_INFORMATION (requires SeSystemtimePrivilege)
	SystemSummaryMemoryInformation, // not implemented // SYSTEM_MEMORY_USAGE_INFORMATION
	SystemMirrorMemoryInformation, // s (requires license value "Kernel-MemoryMirroringSupported") (requires SeShutdownPrivilege) // 30
	SystemPerformanceTraceInformation, // q; s: (type depends on EVENT_TRACE_INFORMATION_CLASS)
	SystemObsolete0, // not implemented
	SystemExceptionInformation, // q: SYSTEM_EXCEPTION_INFORMATION
	SystemCrashDumpStateInformation, // s: SYSTEM_CRASH_DUMP_STATE_INFORMATION (requires SeDebugPrivilege)
	SystemKernelDebuggerInformation, // q: SYSTEM_KERNEL_DEBUGGER_INFORMATION
	SystemContextSwitchInformation, // q: SYSTEM_CONTEXT_SWITCH_INFORMATION
	SystemRegistryQuotaInformation, // q: SYSTEM_REGISTRY_QUOTA_INFORMATION; s (requires SeIncreaseQuotaPrivilege)
	SystemExtendServiceTableInformation, // s (requires SeLoadDriverPrivilege) // loads win32k only
	SystemPrioritySeperation, // s (requires SeTcbPrivilege)
	SystemVerifierAddDriverInformation, // s (requires SeDebugPrivilege) // 40
	SystemVerifierRemoveDriverInformation, // s (requires SeDebugPrivilege)
	SystemProcessorIdleInformation, // q: SYSTEM_PROCESSOR_IDLE_INFORMATION
	SystemLegacyDriverInformation, // q: SYSTEM_LEGACY_DRIVER_INFORMATION
	SystemCurrentTimeZoneInformation, // q; s: RTL_TIME_ZONE_INFORMATION
	SystemLookasideInformation, // q: SYSTEM_LOOKASIDE_INFORMATION
	SystemTimeSlipNotification, // s: HANDLE (NtCreateEvent) (requires SeSystemtimePrivilege)
	SystemSessionCreate, // not implemented
	SystemSessionDetach, // not implemented
	SystemSessionInformation, // not implemented (SYSTEM_SESSION_INFORMATION)
	SystemRangeStartInformation, // q: SYSTEM_RANGE_START_INFORMATION // 50
	SystemVerifierInformation, // q: SYSTEM_VERIFIER_INFORMATION; s (requires SeDebugPrivilege)
	SystemVerifierThunkExtend, // s (kernel-mode only)
	SystemSessionProcessInformation, // q: SYSTEM_SESSION_PROCESS_INFORMATION
	SystemLoadGdiDriverInSystemSpace, // s: SYSTEM_GDI_DRIVER_INFORMATION (kernel-mode only) (same as SystemLoadGdiDriverInformation)
	SystemNumaProcessorMap, // q: SYSTEM_NUMA_INFORMATION
	SystemPrefetcherInformation, // q; s: PREFETCHER_INFORMATION // PfSnQueryPrefetcherInformation
	SystemExtendedProcessInformation, // q: SYSTEM_PROCESS_INFORMATION
	SystemRecommendedSharedDataAlignment, // q: ULONG // KeGetRecommendedSharedDataAlignment
	SystemComPlusPackage, // q; s: ULONG
	SystemNumaAvailableMemory, // q: SYSTEM_NUMA_INFORMATION // 60
	SystemProcessorPowerInformation, // q: SYSTEM_PROCESSOR_POWER_INFORMATION
	SystemEmulationBasicInformation, // q: SYSTEM_BASIC_INFORMATION
	SystemEmulationProcessorInformation, // q: SYSTEM_PROCESSOR_INFORMATION
	SystemExtendedHandleInformation, // q: SYSTEM_HANDLE_INFORMATION_EX
	SystemLostDelayedWriteInformation, // q: ULONG
	SystemBigPoolInformation, // q: SYSTEM_BIGPOOL_INFORMATION
	SystemSessionPoolTagInformation, // q: SYSTEM_SESSION_POOLTAG_INFORMATION
	SystemSessionMappedViewInformation, // q: SYSTEM_SESSION_MAPPED_VIEW_INFORMATION
	SystemHotpatchInformation, // q; s: SYSTEM_HOTPATCH_CODE_INFORMATION
	SystemObjectSecurityMode, // q: ULONG // 70
	SystemWatchdogTimerHandler, // s: SYSTEM_WATCHDOG_HANDLER_INFORMATION // (kernel-mode only)
	SystemWatchdogTimerInformation, // q: SYSTEM_WATCHDOG_TIMER_INFORMATION // (kernel-mode only)
	SystemLogicalProcessorInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION
	SystemWow64SharedInformationObsolete, // not implemented
	SystemRegisterFirmwareTableInformationHandler, // s: SYSTEM_FIRMWARE_TABLE_HANDLER // (kernel-mode only)
	SystemFirmwareTableInformation, // SYSTEM_FIRMWARE_TABLE_INFORMATION
	SystemModuleInformationEx, // q: RTL_PROCESS_MODULE_INFORMATION_EX
	SystemVerifierTriageInformation, // not implemented
	SystemSuperfetchInformation, // q; s: SUPERFETCH_INFORMATION // PfQuerySuperfetchInformation
	SystemMemoryListInformation, // q: SYSTEM_MEMORY_LIST_INFORMATION; s: SYSTEM_MEMORY_LIST_COMMAND (requires SeProfileSingleProcessPrivilege) // 80
	SystemFileCacheInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (same as SystemFileCacheInformation)
	SystemThreadPriorityClientIdInformation, // s: SYSTEM_THREAD_CID_PRIORITY_INFORMATION (requires SeIncreaseBasePriorityPrivilege)
	SystemProcessorIdleCycleTimeInformation, // q: SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION[]
	SystemVerifierCancellationInformation, // SYSTEM_VERIFIER_CANCELLATION_INFORMATION // name:wow64:whNT32QuerySystemVerifierCancellationInformation
	SystemProcessorPowerInformationEx, // not implemented
	SystemRefTraceInformation, // q; s: SYSTEM_REF_TRACE_INFORMATION // ObQueryRefTraceInformation
	SystemSpecialPoolInformation, // q; s: SYSTEM_SPECIAL_POOL_INFORMATION (requires SeDebugPrivilege) // MmSpecialPoolTag, then MmSpecialPoolCatchOverruns != 0
	SystemProcessIdInformation, // q: SYSTEM_PROCESS_ID_INFORMATION
	SystemErrorPortInformation, // s (requires SeTcbPrivilege)
	SystemBootEnvironmentInformation, // q: SYSTEM_BOOT_ENVIRONMENT_INFORMATION // 90
	SystemHypervisorInformation, // q: SYSTEM_HYPERVISOR_QUERY_INFORMATION
	SystemVerifierInformationEx, // q; s: SYSTEM_VERIFIER_INFORMATION_EX
	SystemTimeZoneInformation, // q; s: RTL_TIME_ZONE_INFORMATION (requires SeTimeZonePrivilege)
	SystemImageFileExecutionOptionsInformation, // s: SYSTEM_IMAGE_FILE_EXECUTION_OPTIONS_INFORMATION (requires SeTcbPrivilege)
	SystemCoverageInformation, // q: COVERAGE_MODULES s: COVERAGE_MODULE_REQUEST // ExpCovQueryInformation (requires SeDebugPrivilege)
	SystemPrefetchPatchInformation, // SYSTEM_PREFETCH_PATCH_INFORMATION
	SystemVerifierFaultsInformation, // s: SYSTEM_VERIFIER_FAULTS_INFORMATION (requires SeDebugPrivilege)
	SystemSystemPartitionInformation, // q: SYSTEM_SYSTEM_PARTITION_INFORMATION
	SystemSystemDiskInformation, // q: SYSTEM_SYSTEM_DISK_INFORMATION
	SystemProcessorPerformanceDistribution, // q: SYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION // 100
	SystemNumaProximityNodeInformation, // q; s: SYSTEM_NUMA_PROXIMITY_MAP
	SystemDynamicTimeZoneInformation, // q; s: RTL_DYNAMIC_TIME_ZONE_INFORMATION (requires SeTimeZonePrivilege)
	SystemCodeIntegrityInformation, // q: SYSTEM_CODEINTEGRITY_INFORMATION // SeCodeIntegrityQueryInformation
	SystemProcessorMicrocodeUpdateInformation, // s: SYSTEM_PROCESSOR_MICROCODE_UPDATE_INFORMATION
	SystemProcessorBrandString, // q: CHAR[] // HaliQuerySystemInformation -> HalpGetProcessorBrandString, info class 23
	SystemVirtualAddressInformation, // q: SYSTEM_VA_LIST_INFORMATION[]; s: SYSTEM_VA_LIST_INFORMATION[] (requires SeIncreaseQuotaPrivilege) // MmQuerySystemVaInformation
	SystemLogicalProcessorAndGroupInformation, // q: SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX // since WIN7 // KeQueryLogicalProcessorRelationship
	SystemProcessorCycleTimeInformation, // q: SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION[]
	SystemStoreInformation, // q; s: SYSTEM_STORE_INFORMATION (requires SeProfileSingleProcessPrivilege) // SmQueryStoreInformation
	SystemRegistryAppendString, // s: SYSTEM_REGISTRY_APPEND_STRING_PARAMETERS // 110
	SystemAitSamplingValue, // s: ULONG (requires SeProfileSingleProcessPrivilege)
	SystemVhdBootInformation, // q: SYSTEM_VHD_BOOT_INFORMATION
	SystemCpuQuotaInformation, // q; s: PS_CPU_QUOTA_QUERY_INFORMATION
	SystemNativeBasicInformation, // q: SYSTEM_BASIC_INFORMATION
	SystemErrorPortTimeouts, // SYSTEM_ERROR_PORT_TIMEOUTS
	SystemLowPriorityIoInformation, // q: SYSTEM_LOW_PRIORITY_IO_INFORMATION
	SystemTpmBootEntropyInformation, // q: TPM_BOOT_ENTROPY_NT_RESULT // ExQueryTpmBootEntropyInformation
	SystemVerifierCountersInformation, // q: SYSTEM_VERIFIER_COUNTERS_INFORMATION
	SystemPagedPoolInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypePagedPool)
	SystemSystemPtesInformationEx, // q: SYSTEM_FILECACHE_INFORMATION; s (requires SeIncreaseQuotaPrivilege) (info for WorkingSetTypeSystemPtes) // 120
	SystemNodeDistanceInformation,
	SystemAcpiAuditInformation, // q: SYSTEM_ACPI_AUDIT_INFORMATION // HaliQuerySystemInformation -> HalpAuditQueryResults, info class 26
	SystemBasicPerformanceInformation, // q: SYSTEM_BASIC_PERFORMANCE_INFORMATION // name:wow64:whNtQuerySystemInformation_SystemBasicPerformanceInformation
	SystemQueryPerformanceCounterInformation, // q: SYSTEM_QUERY_PERFORMANCE_COUNTER_INFORMATION // since WIN7 SP1
	SystemSessionBigPoolInformation, // q: SYSTEM_SESSION_POOLTAG_INFORMATION // since WIN8
	SystemBootGraphicsInformation, // q; s: SYSTEM_BOOT_GRAPHICS_INFORMATION (kernel-mode only)
	SystemScrubPhysicalMemoryInformation, // q; s: MEMORY_SCRUB_INFORMATION
	SystemBadPageInformation,
	SystemProcessorProfileControlArea, // q; s: SYSTEM_PROCESSOR_PROFILE_CONTROL_AREA
	SystemCombinePhysicalMemoryInformation, // s: MEMORY_COMBINE_INFORMATION, MEMORY_COMBINE_INFORMATION_EX, MEMORY_COMBINE_INFORMATION_EX2 // 130
	SystemEntropyInterruptTimingInformation, // q; s: SYSTEM_ENTROPY_TIMING_INFORMATION
	SystemConsoleInformation, // q: SYSTEM_CONSOLE_INFORMATION
	SystemPlatformBinaryInformation, // q: SYSTEM_PLATFORM_BINARY_INFORMATION (requires SeTcbPrivilege)
	SystemPolicyInformation, // q: SYSTEM_POLICY_INFORMATION
	SystemHypervisorProcessorCountInformation, // q: SYSTEM_HYPERVISOR_PROCESSOR_COUNT_INFORMATION
	SystemDeviceDataInformation, // q: SYSTEM_DEVICE_DATA_INFORMATION
	SystemDeviceDataEnumerationInformation, // q: SYSTEM_DEVICE_DATA_INFORMATION
	SystemMemoryTopologyInformation, // q: SYSTEM_MEMORY_TOPOLOGY_INFORMATION
	SystemMemoryChannelInformation, // q: SYSTEM_MEMORY_CHANNEL_INFORMATION
	SystemBootLogoInformation, // q: SYSTEM_BOOT_LOGO_INFORMATION // 140
	SystemProcessorPerformanceInformationEx, // q: SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_EX // since WINBLUE
	SystemCriticalProcessErrorLogInformation,
	SystemSecureBootPolicyInformation, // q: SYSTEM_SECUREBOOT_POLICY_INFORMATION
	SystemPageFileInformationEx, // q: SYSTEM_PAGEFILE_INFORMATION_EX
	SystemSecureBootInformation, // q: SYSTEM_SECUREBOOT_INFORMATION
	SystemEntropyInterruptTimingRawInformation,
	SystemPortableWorkspaceEfiLauncherInformation, // q: SYSTEM_PORTABLE_WORKSPACE_EFI_LAUNCHER_INFORMATION
	SystemFullProcessInformation, // q: SYSTEM_PROCESS_INFORMATION with SYSTEM_PROCESS_INFORMATION_EXTENSION (requires admin)
	SystemKernelDebuggerInformationEx, // q: SYSTEM_KERNEL_DEBUGGER_INFORMATION_EX
	SystemBootMetadataInformation, // 150
	SystemSoftRebootInformation, // q: ULONG
	SystemElamCertificateInformation, // s: SYSTEM_ELAM_CERTIFICATE_INFORMATION
	SystemOfflineDumpConfigInformation, // q: OFFLINE_CRASHDUMP_CONFIGURATION_TABLE_V2
	SystemProcessorFeaturesInformation, // q: SYSTEM_PROCESSOR_FEATURES_INFORMATION
	SystemRegistryReconciliationInformation, // s: NULL (requires admin) (flushes registry hives)
	SystemEdidInformation, // q: SYSTEM_EDID_INFORMATION
	SystemManufacturingInformation, // q: SYSTEM_MANUFACTURING_INFORMATION // since THRESHOLD
	SystemEnergyEstimationConfigInformation, // q: SYSTEM_ENERGY_ESTIMATION_CONFIG_INFORMATION
	SystemHypervisorDetailInformation, // q: SYSTEM_HYPERVISOR_DETAIL_INFORMATION
	SystemProcessorCycleStatsInformation, // q: SYSTEM_PROCESSOR_CYCLE_STATS_INFORMATION // 160
	SystemVmGenerationCountInformation,
	SystemTrustedPlatformModuleInformation, // q: SYSTEM_TPM_INFORMATION
	SystemKernelDebuggerFlags, // SYSTEM_KERNEL_DEBUGGER_FLAGS
	SystemCodeIntegrityPolicyInformation, // q: SYSTEM_CODEINTEGRITYPOLICY_INFORMATION
	SystemIsolatedUserModeInformation, // q: SYSTEM_ISOLATED_USER_MODE_INFORMATION
	SystemHardwareSecurityTestInterfaceResultsInformation,
	SystemSingleModuleInformation, // q: SYSTEM_SINGLE_MODULE_INFORMATION
	SystemAllowedCpuSetsInformation,
	SystemVsmProtectionInformation, // q: SYSTEM_VSM_PROTECTION_INFORMATION (previously SystemDmaProtectionInformation)
	SystemInterruptCpuSetsInformation, // q: SYSTEM_INTERRUPT_CPU_SET_INFORMATION // 170
	SystemSecureBootPolicyFullInformation, // q: SYSTEM_SECUREBOOT_POLICY_FULL_INFORMATION
	SystemCodeIntegrityPolicyFullInformation,
	SystemAffinitizedInterruptProcessorInformation, // (requires SeIncreaseBasePriorityPrivilege)
	SystemRootSiloInformation, // q: SYSTEM_ROOT_SILO_INFORMATION
	SystemCpuSetInformation, // q: SYSTEM_CPU_SET_INFORMATION // since THRESHOLD2
	SystemCpuSetTagInformation, // q: SYSTEM_CPU_SET_TAG_INFORMATION
	SystemWin32WerStartCallout,
	SystemSecureKernelProfileInformation, // q: SYSTEM_SECURE_KERNEL_HYPERGUARD_PROFILE_INFORMATION
	SystemCodeIntegrityPlatformManifestInformation, // q: SYSTEM_SECUREBOOT_PLATFORM_MANIFEST_INFORMATION // since REDSTONE
	SystemInterruptSteeringInformation, // SYSTEM_INTERRUPT_STEERING_INFORMATION_INPUT // 180
	SystemSupportedProcessorArchitectures, // in: HANDLE, out: SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION[] (Max 5 structs) // NtQuerySystemInformationEx
	SystemMemoryUsageInformation, // q: SYSTEM_MEMORY_USAGE_INFORMATION
	SystemCodeIntegrityCertificateInformation, // q: SYSTEM_CODEINTEGRITY_CERTIFICATE_INFORMATION
	SystemPhysicalMemoryInformation, // q: SYSTEM_PHYSICAL_MEMORY_INFORMATION // since REDSTONE2
	SystemControlFlowTransition,
	SystemKernelDebuggingAllowed, // s: ULONG
	SystemActivityModerationExeState, // SYSTEM_ACTIVITY_MODERATION_EXE_STATE
	SystemActivityModerationUserSettings, // SYSTEM_ACTIVITY_MODERATION_USER_SETTINGS
	SystemCodeIntegrityPoliciesFullInformation,
	SystemCodeIntegrityUnlockInformation, // SYSTEM_CODEINTEGRITY_UNLOCK_INFORMATION // 190
	SystemIntegrityQuotaInformation,
	SystemFlushInformation, // q: SYSTEM_FLUSH_INFORMATION
	SystemProcessorIdleMaskInformation, // q: ULONG_PTR // since REDSTONE3
	SystemSecureDumpEncryptionInformation,
	SystemWriteConstraintInformation, // SYSTEM_WRITE_CONSTRAINT_INFORMATION
	SystemKernelVaShadowInformation, // SYSTEM_KERNEL_VA_SHADOW_INFORMATION
	SystemHypervisorSharedPageInformation, // SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION // since REDSTONE4
	SystemFirmwareBootPerformanceInformation,
	SystemCodeIntegrityVerificationInformation, // SYSTEM_CODEINTEGRITYVERIFICATION_INFORMATION
	SystemFirmwarePartitionInformation, // SYSTEM_FIRMWARE_PARTITION_INFORMATION // 200
	SystemSpeculationControlInformation, // SYSTEM_SPECULATION_CONTROL_INFORMATION // (CVE-2017-5715) REDSTONE3 and above.
	SystemDmaGuardPolicyInformation, // SYSTEM_DMA_GUARD_POLICY_INFORMATION
	SystemEnclaveLaunchControlInformation, // SYSTEM_ENCLAVE_LAUNCH_CONTROL_INFORMATION
	SystemWorkloadAllowedCpuSetsInformation, // SYSTEM_WORKLOAD_ALLOWED_CPU_SET_INFORMATION // since REDSTONE5
	SystemCodeIntegrityUnlockModeInformation,
	SystemLeapSecondInformation, // SYSTEM_LEAP_SECOND_INFORMATION
	SystemFlags2Information, // q: SYSTEM_FLAGS_INFORMATION
	SystemSecurityModelInformation, // SYSTEM_SECURITY_MODEL_INFORMATION // since 19H1
	SystemCodeIntegritySyntheticCacheInformation,
	SystemFeatureConfigurationInformation, // SYSTEM_FEATURE_CONFIGURATION_INFORMATION // since 20H1 // 210
	SystemFeatureConfigurationSectionInformation, // SYSTEM_FEATURE_CONFIGURATION_SECTIONS_INFORMATION
	SystemFeatureUsageSubscriptionInformation, // SYSTEM_FEATURE_USAGE_SUBSCRIPTION_DETAILS
	SystemSecureSpeculationControlInformation, // SECURE_SPECULATION_CONTROL_INFORMATION
	SystemSpacesBootInformation, // since 20H2
	SystemFwRamdiskInformation, // SYSTEM_FIRMWARE_RAMDISK_INFORMATION
	SystemWheaIpmiHardwareInformation,
	SystemDifSetRuleClassInformation,
	SystemDifClearRuleClassInformation,
	SystemDifApplyPluginVerificationOnDriver,
	SystemDifRemovePluginVerificationOnDriver, // 220
	SystemShadowStackInformation, // SYSTEM_SHADOW_STACK_INFORMATION
	SystemBuildVersionInformation, // SYSTEM_BUILD_VERSION_INFORMATION
	SystemPoolLimitInformation, // SYSTEM_POOL_LIMIT_INFORMATION
	SystemCodeIntegrityAddDynamicStore,
	SystemCodeIntegrityClearDynamicStores,
	SystemDifPoolTrackingInformation,
	SystemPoolZeroingInformation, // SYSTEM_POOL_ZEROING_INFORMATION
	MaxSystemInfoClass
} SYSTEM_INFORMATION_CLASS;

typedef enum _PROCESSINFOCLASS
{
	ProcessBasicInformation, // q: PROCESS_BASIC_INFORMATION, PROCESS_EXTENDED_BASIC_INFORMATION
	ProcessQuotaLimits, // qs: QUOTA_LIMITS, QUOTA_LIMITS_EX
	ProcessIoCounters, // q: IO_COUNTERS
	ProcessVmCounters, // q: VM_COUNTERS, VM_COUNTERS_EX, VM_COUNTERS_EX2
	ProcessTimes, // q: KERNEL_USER_TIMES
	ProcessBasePriority, // s: KPRIORITY
	ProcessRaisePriority, // s: ULONG
	ProcessDebugPort, // q: HANDLE
	ProcessExceptionPort, // s: PROCESS_EXCEPTION_PORT
	ProcessAccessToken, // s: PROCESS_ACCESS_TOKEN
	ProcessLdtInformation, // qs: PROCESS_LDT_INFORMATION // 10
	ProcessLdtSize, // s: PROCESS_LDT_SIZE
	ProcessDefaultHardErrorMode, // qs: ULONG
	ProcessIoPortHandlers, // (kernel-mode only) // PROCESS_IO_PORT_HANDLER_INFORMATION
	ProcessPooledUsageAndLimits, // q: POOLED_USAGE_AND_LIMITS
	ProcessWorkingSetWatch, // q: PROCESS_WS_WATCH_INFORMATION[]; s: void
	ProcessUserModeIOPL, // qs: ULONG (requires SeTcbPrivilege)
	ProcessEnableAlignmentFaultFixup, // s: BOOLEAN
	ProcessPriorityClass, // qs: PROCESS_PRIORITY_CLASS
	ProcessWx86Information, // qs: ULONG (requires SeTcbPrivilege) (VdmAllowed)
	ProcessHandleCount, // q: ULONG, PROCESS_HANDLE_INFORMATION // 20
	ProcessAffinityMask, // s: KAFFINITY
	ProcessPriorityBoost, // qs: ULONG
	ProcessDeviceMap, // qs: PROCESS_DEVICEMAP_INFORMATION, PROCESS_DEVICEMAP_INFORMATION_EX
	ProcessSessionInformation, // q: PROCESS_SESSION_INFORMATION
	ProcessForegroundInformation, // s: PROCESS_FOREGROUND_BACKGROUND
	ProcessWow64Information, // q: ULONG_PTR
	ProcessImageFileName, // q: UNICODE_STRING
	ProcessLUIDDeviceMapsEnabled, // q: ULONG
	ProcessBreakOnTermination, // qs: ULONG
	ProcessDebugObjectHandle, // q: HANDLE // 30
	ProcessDebugFlags, // qs: ULONG
	ProcessHandleTracing, // q: PROCESS_HANDLE_TRACING_QUERY; s: size 0 disables, otherwise enables
	ProcessIoPriority, // qs: IO_PRIORITY_HINT
	ProcessExecuteFlags, // qs: ULONG
	ProcessTlsInformation, // PROCESS_TLS_INFORMATION // ProcessResourceManagement 
	ProcessCookie, // q: ULONG
	ProcessImageInformation, // q: SECTION_IMAGE_INFORMATION
	ProcessCycleTime, // q: PROCESS_CYCLE_TIME_INFORMATION // since VISTA
	ProcessPagePriority, // q: PAGE_PRIORITY_INFORMATION
	ProcessInstrumentationCallback, // s: PVOID or PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION // 40
	ProcessThreadStackAllocation, // s: PROCESS_STACK_ALLOCATION_INFORMATION, PROCESS_STACK_ALLOCATION_INFORMATION_EX
	ProcessWorkingSetWatchEx, // q: PROCESS_WS_WATCH_INFORMATION_EX[]
	ProcessImageFileNameWin32, // q: UNICODE_STRING
	ProcessImageFileMapping, // q: HANDLE (input)
	ProcessAffinityUpdateMode, // qs: PROCESS_AFFINITY_UPDATE_MODE
	ProcessMemoryAllocationMode, // qs: PROCESS_MEMORY_ALLOCATION_MODE
	ProcessGroupInformation, // q: USHORT[]
	ProcessTokenVirtualizationEnabled, // s: ULONG
	ProcessConsoleHostProcess, // q: ULONG_PTR // ProcessOwnerInformation
	ProcessWindowInformation, // q: PROCESS_WINDOW_INFORMATION // 50
	ProcessHandleInformation, // q: PROCESS_HANDLE_SNAPSHOT_INFORMATION // since WIN8
	ProcessMitigationPolicy, // s: PROCESS_MITIGATION_POLICY_INFORMATION
	ProcessDynamicFunctionTableInformation,
	ProcessHandleCheckingMode, // qs: ULONG; s: 0 disables, otherwise enables
	ProcessKeepAliveCount, // q: PROCESS_KEEPALIVE_COUNT_INFORMATION
	ProcessRevokeFileHandles, // s: PROCESS_REVOKE_FILE_HANDLES_INFORMATION
	ProcessWorkingSetControl, // s: PROCESS_WORKING_SET_CONTROL
	ProcessHandleTable, // q: ULONG[] // since WINBLUE
	ProcessCheckStackExtentsMode, // qs: ULONG // KPROCESS->CheckStackExtents (CFG)
	ProcessCommandLineInformation, // q: UNICODE_STRING // 60
	ProcessProtectionInformation, // q: PS_PROTECTION
	ProcessMemoryExhaustion, // PROCESS_MEMORY_EXHAUSTION_INFO // since THRESHOLD
	ProcessFaultInformation, // PROCESS_FAULT_INFORMATION
	ProcessTelemetryIdInformation, // q: PROCESS_TELEMETRY_ID_INFORMATION
	ProcessCommitReleaseInformation, // PROCESS_COMMIT_RELEASE_INFORMATION
	ProcessDefaultCpuSetsInformation,
	ProcessAllowedCpuSetsInformation,
	ProcessSubsystemProcess,
	ProcessJobMemoryInformation, // q: PROCESS_JOB_MEMORY_INFO
	ProcessInPrivate, // s: void // ETW // since THRESHOLD2 // 70
	ProcessRaiseUMExceptionOnInvalidHandleClose, // qs: ULONG; s: 0 disables, otherwise enables
	ProcessIumChallengeResponse,
	ProcessChildProcessInformation, // q: PROCESS_CHILD_PROCESS_INFORMATION
	ProcessHighGraphicsPriorityInformation, // qs: BOOLEAN (requires SeTcbPrivilege)
	ProcessSubsystemInformation, // q: SUBSYSTEM_INFORMATION_TYPE // since REDSTONE2
	ProcessEnergyValues, // q: PROCESS_ENERGY_VALUES, PROCESS_EXTENDED_ENERGY_VALUES
	ProcessPowerThrottlingState, // qs: POWER_THROTTLING_PROCESS_STATE
	ProcessReserved3Information, // ProcessActivityThrottlePolicy // PROCESS_ACTIVITY_THROTTLE_POLICY
	ProcessWin32kSyscallFilterInformation, // q: WIN32K_SYSCALL_FILTER
	ProcessDisableSystemAllowedCpuSets, // 80
	ProcessWakeInformation, // PROCESS_WAKE_INFORMATION
	ProcessEnergyTrackingState, // PROCESS_ENERGY_TRACKING_STATE
	ProcessManageWritesToExecutableMemory, // MANAGE_WRITES_TO_EXECUTABLE_MEMORY // since REDSTONE3
	ProcessCaptureTrustletLiveDump,
	ProcessTelemetryCoverage,
	ProcessEnclaveInformation,
	ProcessEnableReadWriteVmLogging, // PROCESS_READWRITEVM_LOGGING_INFORMATION
	ProcessUptimeInformation, // q: PROCESS_UPTIME_INFORMATION
	ProcessImageSection, // q: HANDLE
	ProcessDebugAuthInformation, // since REDSTONE4 // 90
	ProcessSystemResourceManagement, // PROCESS_SYSTEM_RESOURCE_MANAGEMENT
	ProcessSequenceNumber, // q: ULONGLONG
	ProcessLoaderDetour, // since REDSTONE5
	ProcessSecurityDomainInformation, // PROCESS_SECURITY_DOMAIN_INFORMATION
	ProcessCombineSecurityDomainsInformation, // PROCESS_COMBINE_SECURITY_DOMAINS_INFORMATION
	ProcessEnableLogging, // PROCESS_LOGGING_INFORMATION
	ProcessLeapSecondInformation, // PROCESS_LEAP_SECOND_INFORMATION
	ProcessFiberShadowStackAllocation, // PROCESS_FIBER_SHADOW_STACK_ALLOCATION_INFORMATION // since 19H1
	ProcessFreeFiberShadowStackAllocation, // PROCESS_FREE_FIBER_SHADOW_STACK_ALLOCATION_INFORMATION
	ProcessAltSystemCallInformation, // qs: BOOLEAN (kernel-mode only) // INT2E // since 20H1 // 100
	ProcessDynamicEHContinuationTargets, // PROCESS_DYNAMIC_EH_CONTINUATION_TARGETS_INFORMATION
	ProcessDynamicEnforcedCetCompatibleRanges, // PROCESS_DYNAMIC_ENFORCED_ADDRESS_RANGE_INFORMATION // since 20H2
	MaxProcessInfoClass
} PROCESSINFOCLASS;

typedef enum _THREADINFOCLASS
{
	ThreadBasicInformation, // q: THREAD_BASIC_INFORMATION
	ThreadTimes, // q: KERNEL_USER_TIMES
	ThreadPriority, // s: KPRIORITY (requires SeIncreaseBasePriorityPrivilege)
	ThreadBasePriority, // s: LONG
	ThreadAffinityMask, // s: KAFFINITY
	ThreadImpersonationToken, // s: HANDLE
	ThreadDescriptorTableEntry, // q: DESCRIPTOR_TABLE_ENTRY (or WOW64_DESCRIPTOR_TABLE_ENTRY)
	ThreadEnableAlignmentFaultFixup, // s: BOOLEAN
	ThreadEventPair,
	ThreadQuerySetWin32StartAddress, // q: ULONG_PTR
	ThreadZeroTlsCell, // s: ULONG // TlsIndex // 10
	ThreadPerformanceCount, // q: LARGE_INTEGER
	ThreadAmILastThread, // q: ULONG
	ThreadIdealProcessor, // s: ULONG
	ThreadPriorityBoost, // qs: ULONG
	ThreadSetTlsArrayAddress, // s: ULONG_PTR
	ThreadIsIoPending, // q: ULONG
	ThreadHideFromDebugger, // q: BOOLEAN; s: void
	ThreadBreakOnTermination, // qs: ULONG
	ThreadSwitchLegacyState, // s: void // NtCurrentThread // NPX/FPU
	ThreadIsTerminated, // q: ULONG // 20
	ThreadLastSystemCall, // q: THREAD_LAST_SYSCALL_INFORMATION
	ThreadIoPriority, // qs: IO_PRIORITY_HINT (requires SeIncreaseBasePriorityPrivilege)
	ThreadCycleTime, // q: THREAD_CYCLE_TIME_INFORMATION
	ThreadPagePriority, // q: ULONG
	ThreadActualBasePriority, // s: LONG (requires SeIncreaseBasePriorityPrivilege)
	ThreadTebInformation, // q: THREAD_TEB_INFORMATION (requires THREAD_GET_CONTEXT + THREAD_SET_CONTEXT)
	ThreadCSwitchMon,
	ThreadCSwitchPmu,
	ThreadWow64Context, // qs: WOW64_CONTEXT
	ThreadGroupInformation, // q: GROUP_AFFINITY // 30
	ThreadUmsInformation, // q: THREAD_UMS_INFORMATION
	ThreadCounterProfiling, // q: BOOLEAN; s: THREAD_PROFILING_INFORMATION?
	ThreadIdealProcessorEx, // q: PROCESSOR_NUMBER
	ThreadCpuAccountingInformation, // q: BOOLEAN; s: HANDLE (NtOpenSession) // NtCurrentThread // since WIN8
	ThreadSuspendCount, // q: ULONG // since WINBLUE
	ThreadHeterogeneousCpuPolicy, // q: KHETERO_CPU_POLICY // since THRESHOLD
	ThreadContainerId, // q: GUID
	ThreadNameInformation, // qs: THREAD_NAME_INFORMATION
	ThreadSelectedCpuSets,
	ThreadSystemThreadInformation, // q: SYSTEM_THREAD_INFORMATION // 40
	ThreadActualGroupAffinity, // q: GROUP_AFFINITY // since THRESHOLD2
	ThreadDynamicCodePolicyInfo, // q: ULONG; s: ULONG (NtCurrentThread)
	ThreadExplicitCaseSensitivity, // qs: ULONG; s: 0 disables, otherwise enables
	ThreadWorkOnBehalfTicket, // RTL_WORK_ON_BEHALF_TICKET_EX
	ThreadSubsystemInformation, // q: SUBSYSTEM_INFORMATION_TYPE // since REDSTONE2
	ThreadDbgkWerReportActive, // s: ULONG; s: 0 disables, otherwise enables
	ThreadAttachContainer, // s: HANDLE (job object) // NtCurrentThread
	ThreadManageWritesToExecutableMemory, // MANAGE_WRITES_TO_EXECUTABLE_MEMORY // since REDSTONE3
	ThreadPowerThrottlingState, // POWER_THROTTLING_THREAD_STATE
	ThreadWorkloadClass, // THREAD_WORKLOAD_CLASS // since REDSTONE5 // 50
	MaxThreadInfoClass
} THREADINFOCLASS;

typedef enum _KWAIT_REASON
{
	Executive,
	FreePage,
	PageIn,
	PoolAllocation,
	DelayExecution,
	Suspended,
	UserRequest,
	WrExecutive,
	WrFreePage,
	WrPageIn,
	WrPoolAllocation,
	WrDelayExecution,
	WrSuspended,
	WrUserRequest,
	WrEventPair,
	WrQueue,
	WrLpcReceive,
	WrLpcReply,
	WrVirtualMemory,
	WrPageOut,
	WrRendezvous,
	WrKeyedEvent,
	WrTerminated,
	WrProcessInSwap,
	WrCpuRateControl,
	WrCalloutStack,
	WrKernel,
	WrResource,
	WrPushLock,
	WrMutex,
	WrQuantumEnd,
	WrDispatchInt,
	WrPreempted,
	WrYieldExecution,
	WrFastMutex,
	WrGuardedMutex,
	WrRundown,
	WrAlertByThreadId,
	WrDeferredPreempt,
	MaximumWaitReason
} KWAIT_REASON, *PKWAIT_REASON;

typedef enum _OBJECT_INFORMATION_CLASS
{
	ObjectBasicInformation, // q: OBJECT_BASIC_INFORMATION
	ObjectNameInformation, // q: OBJECT_NAME_INFORMATION
	ObjectTypeInformation, // q: OBJECT_TYPE_INFORMATION
	ObjectTypesInformation, // q: OBJECT_TYPES_INFORMATION
	ObjectHandleFlagInformation, // qs: OBJECT_HANDLE_FLAG_INFORMATION
	ObjectSessionInformation, // s: void // change object session // (requires SeTcbPrivilege)
	ObjectSessionObjectInformation, // s: void // change object session // (requires SeTcbPrivilege)
	MaxObjectInfoClass
} OBJECT_INFORMATION_CLASS;

typedef enum _TAG_INFO_LEVEL
{
	TagInfoLevelNameFromTag = 1, // TAG_INFO_NAME_FROM_TAG
	TagInfoLevelNamesReferencingModule, // TAG_INFO_NAMES_REFERENCING_MODULE
	TagInfoLevelNameTagMapping, // TAG_INFO_NAME_TAG_MAPPING
	TagInfoLevelMax
} TAG_INFO_LEVEL, *PTAG_INFO_LEVEL;

// private
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

typedef enum _KTHREAD_STATE
{
	Initialized,
	Ready,
	Running,
	Standby,
	Terminated,
	Waiting,
	Transition,
	DeferredReady,
	GateWaitObsolete,
	WaitingForProcessInSwap,
	MaximumThreadState
} KTHREAD_STATE, *PKTHREAD_STATE;

//
// structs
//

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
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	KTHREAD_STATE ThreadState;
	KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

// Use with both ProcessPagePriority and ThreadPagePriority
typedef struct _PAGE_PRIORITY_INFORMATION
{
	ULONG PagePriority;
} PAGE_PRIORITY_INFORMATION, *PPAGE_PRIORITY_INFORMATION;

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
	KPRIORITY BasePriority;
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
	SYSTEM_THREAD_INFORMATION Threads[1]; // SystemProcessInformation
	// SYSTEM_EXTENDED_THREAD_INFORMATION Threads[1]; // SystemExtendedProcessinformation
	// SYSTEM_EXTENDED_THREAD_INFORMATION + SYSTEM_PROCESS_INFORMATION_EXTENSION // SystemFullProcessInformation
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef struct _SYSTEM_FILECACHE_INFORMATION
{
	ULONG_PTR CurrentSize;
	ULONG_PTR PeakSize;
	ULONG PageFaultCount;
	ULONG_PTR MinimumWorkingSet;
	ULONG_PTR MaximumWorkingSet;
	ULONG_PTR CurrentSizeIncludingTransitionInPages;
	ULONG_PTR PeakSizeIncludingTransitionInPages;
	ULONG TransitionRePurposeCount;
	ULONG Flags;
} SYSTEM_FILECACHE_INFORMATION, *PSYSTEM_FILECACHE_INFORMATION;

// Can be used instead of SYSTEM_FILECACHE_INFORMATION
typedef struct _SYSTEM_BASIC_WORKING_SET_INFORMATION
{
	SIZE_T CurrentSize;
	SIZE_T PeakSize;
	ULONG PageFaultCount;
} SYSTEM_BASIC_WORKING_SET_INFORMATION, *PSYSTEM_BASIC_WORKING_SET_INFORMATION;

// private
typedef struct _MEMORY_COMBINE_INFORMATION
{
	HANDLE Handle;
	ULONG_PTR PagesCombined;
} MEMORY_COMBINE_INFORMATION, *PMEMORY_COMBINE_INFORMATION;

// rev
#define MEMORY_COMBINE_FLAGS_COMMON_PAGES_ONLY 0x4

// private
typedef struct _MEMORY_COMBINE_INFORMATION_EX
{
	HANDLE Handle;
	ULONG_PTR PagesCombined;
	ULONG Flags;
} MEMORY_COMBINE_INFORMATION_EX, *PMEMORY_COMBINE_INFORMATION_EX;

// private
typedef struct _MEMORY_COMBINE_INFORMATION_EX2
{
	HANDLE Handle;
	ULONG_PTR PagesCombined;
	ULONG Flags;
	HANDLE ProcessHandle;
} MEMORY_COMBINE_INFORMATION_EX2, *PMEMORY_COMBINE_INFORMATION_EX2;

typedef struct _TAG_INFO_NAME_FROM_TAG
{
	struct
	{
		ULONG dwPid;
		ULONG dwTag;
	} InParams;

	struct
	{
		ULONG eTagType;
		PWSTR pszName;
	} OutParams;
} TAG_INFO_NAME_FROM_TAG, *PTAG_INFO_NAME_FROM_TAG;

#define PROCESS_PRIORITY_CLASS_UNKNOWN 0
#define PROCESS_PRIORITY_CLASS_IDLE 1
#define PROCESS_PRIORITY_CLASS_NORMAL 2
#define PROCESS_PRIORITY_CLASS_HIGH 3
#define PROCESS_PRIORITY_CLASS_REALTIME 4
#define PROCESS_PRIORITY_CLASS_BELOW_NORMAL 5
#define PROCESS_PRIORITY_CLASS_ABOVE_NORMAL 6

typedef struct _PROCESS_PRIORITY_CLASS
{
	BOOLEAN Foreground;
	UCHAR PriorityClass;
} PROCESS_PRIORITY_CLASS, *PPROCESS_PRIORITY_CLASS;

//
// Synchronization enumerations
//

typedef enum _EVENT_TYPE
{
	NotificationEvent,
	SynchronizationEvent
} EVENT_TYPE;

typedef enum _TIMER_TYPE
{
	NotificationTimer,
	SynchronizationTimer
} TIMER_TYPE;

typedef enum _WAIT_TYPE
{
	WaitAll,
	WaitAny,
	WaitNotification
} WAIT_TYPE;

typedef enum _IO_PRIORITY_HINT
{
	IoPriorityVeryLow = 0, // Specifies the lowest possible priority hint level. The system uses this value for background I/O operations.
	IoPriorityLow, // Specifies a low-priority hint level.
	IoPriorityNormal, // Specifies a normal-priority hint level. This value is the default setting for an IRP.
	IoPriorityHigh, // Specifies a high-priority hint level. This value is reserved for use by the system.
	IoPriorityCritical, // Specifies the highest-priority hint level. This value is reserved for use by the system.
	MaxIoPriorityTypes // Marks the limit for priority hints. Any priority hint value must be less than MaxIoPriorityTypes.
} IO_PRIORITY_HINT;

//
// Object attributes
//

#define OBJ_INHERIT 0x00000002
#define OBJ_PERMANENT 0x00000010
#define OBJ_EXCLUSIVE 0x00000020
#define OBJ_CASE_INSENSITIVE 0x00000040
#define OBJ_OPENIF 0x00000080
#define OBJ_OPENLINK 0x00000100
#define OBJ_KERNEL_HANDLE 0x00000200
#define OBJ_FORCE_ACCESS_CHECK 0x00000400
#define OBJ_IGNORE_IMPERSONATED_DEVICEMAP 0x00000800
#define OBJ_DONT_REPARSE 0x00001000
#define OBJ_VALID_ATTRIBUTES 0x00001ff2

#define SYMBOLIC_LINK_QUERY 0x0001

typedef struct _OBJECT_ATTRIBUTES
{
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
	PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef const OBJECT_ATTRIBUTES *PCOBJECT_ATTRIBUTES;

typedef struct _PROCESS_DEVICEMAP_INFORMATION
{
	union
	{
		struct
		{
			HANDLE DirectoryHandle;
		} Set;
		struct
		{
			ULONG DriveMap;
			UCHAR DriveType[32];
		} Query;
	};
} PROCESS_DEVICEMAP_INFORMATION, *PPROCESS_DEVICEMAP_INFORMATION;

typedef struct _PROCESS_DEVICEMAP_INFORMATION_EX
{
	union
	{
		struct
		{
			HANDLE DirectoryHandle;
		} Set;
		struct
		{
			ULONG DriveMap;
			UCHAR DriveType[32];
		} Query;
	};
	ULONG Flags; // PROCESS_LUID_DOSDEVICES_ONLY
} PROCESS_DEVICEMAP_INFORMATION_EX, *PPROCESS_DEVICEMAP_INFORMATION_EX;

#define NtCurrentProcess() ((HANDLE)(LONG_PTR)-1)
#define ZwCurrentProcess() NtCurrentProcess()
#define NtCurrentThread() ((HANDLE)(LONG_PTR)-2)
#define ZwCurrentThread() NtCurrentThread()
#define NtCurrentSession() ((HANDLE)(LONG_PTR)-3)
#define ZwCurrentSession() NtCurrentSession()
#define NtCurrentPeb() (NtCurrentTeb()->ProcessEnvironmentBlock)
#define RtlProcessHeap() (NtCurrentPeb()->ProcessHeap)

//
// Windows 8 and above
//

#define NtCurrentProcessToken() ((HANDLE)(LONG_PTR)-4) // NtOpenProcessToken(NtCurrentProcess())
#define NtCurrentThreadToken() ((HANDLE)(LONG_PTR)-5) // NtOpenThreadToken(NtCurrentThread())
#define NtCurrentEffectiveToken() ((HANDLE)(LONG_PTR)-6) // NtOpenThreadToken(NtCurrentThread()) + NtOpenProcessToken(NtCurrentProcess())
#define NtCurrentSilo() ((HANDLE)(LONG_PTR)-1)

// Not NT, but useful.
#define NtCurrentProcessId() (NtCurrentTeb()->ClientId.UniqueProcess)
#define NtCurrentThreadId() (NtCurrentTeb()->ClientId.UniqueThread)

#define InitializeObjectAttributes(p, n, a, r, s) { \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES); \
    (p)->RootDirectory = r; \
    (p)->Attributes = a; \
    (p)->ObjectName = n; \
    (p)->SecurityDescriptor = s; \
    (p)->SecurityQualityOfService = NULL; \
    }

//
// Kernel-user shared data
//

#define PROCESSOR_FEATURE_MAX 64

typedef enum _NT_PRODUCT_TYPE
{
	NtProductWinNt = 1,
	NtProductLanManNt,
	NtProductServer
} NT_PRODUCT_TYPE, *PNT_PRODUCT_TYPE;

typedef enum _ALTERNATIVE_ARCHITECTURE_TYPE
{
	StandardDesign,
	NEC98x86,
	EndAlternatives
} ALTERNATIVE_ARCHITECTURE_TYPE;

#include <pshpack4.h>
typedef struct _KSYSTEM_TIME
{
	ULONG LowPart;
	LONG High1Time;
	LONG High2Time;
} KSYSTEM_TIME, *PKSYSTEM_TIME;

typedef struct _KUSER_SHARED_DATA
{
	ULONG TickCountLowDeprecated;
	ULONG TickCountMultiplier;
	volatile KSYSTEM_TIME InterruptTime;
	volatile KSYSTEM_TIME SystemTime;
	volatile KSYSTEM_TIME TimeZoneBias;
	USHORT ImageNumberLow;
	USHORT ImageNumberHigh;
	WCHAR NtSystemRoot[260];
	ULONG MaxStackTraceDepth;
	ULONG CryptoExponent;
	ULONG TimeZoneId;
	ULONG LargePageMinimum;
	ULONG AitSamplingValue;
	ULONG AppCompatFlag;
	ULONGLONG RNGSeedVersion;
	ULONG GlobalValidationRunlevel;
	LONG TimeZoneBiasStamp;
	ULONG NtBuildNumber;
	NT_PRODUCT_TYPE NtProductType;
	BOOLEAN ProductTypeIsValid;
	UCHAR Reserved0[1];
	USHORT NativeProcessorArchitecture;
	ULONG NtMajorVersion;
	ULONG NtMinorVersion;
	BOOLEAN ProcessorFeatures[PROCESSOR_FEATURE_MAX];
	ULONG Reserved1;
	ULONG Reserved3;
	volatile ULONG TimeSlip;
	ALTERNATIVE_ARCHITECTURE_TYPE AlternativeArchitecture;
	ULONG BootId;
	LARGE_INTEGER SystemExpirationDate;
	ULONG SuiteMask;
	BOOLEAN KdDebuggerEnabled;
	union
	{
		UCHAR MitigationPolicies;
		struct
		{
			UCHAR NXSupportPolicy : 2;
			UCHAR SEHValidationPolicy : 2;
			UCHAR CurDirDevicesSkippedForDlls : 2;
			UCHAR Reserved : 2;
		};
	};
	USHORT CyclesPerYield;
	volatile ULONG ActiveConsoleId;
	volatile ULONG DismountCount;
	ULONG ComPlusPackage;
	ULONG LastSystemRITEventTickCount;
	ULONG NumberOfPhysicalPages;
	BOOLEAN SafeBootMode;
	UCHAR VirtualizationFlags;
	UCHAR Reserved12[2];
	union
	{
		ULONG SharedDataFlags;
		struct
		{
			ULONG DbgErrorPortPresent : 1;
			ULONG DbgElevationEnabled : 1;
			ULONG DbgVirtEnabled : 1;
			ULONG DbgInstallerDetectEnabled : 1;
			ULONG DbgLkgEnabled : 1;
			ULONG DbgDynProcessorEnabled : 1;
			ULONG DbgConsoleBrokerEnabled : 1;
			ULONG DbgSecureBootEnabled : 1;
			ULONG DbgMultiSessionSku : 1;
			ULONG DbgMultiUsersInSessionSku : 1;
			ULONG DbgStateSeparationEnabled : 1;
			ULONG SpareBits : 21;
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;
	ULONG DataFlagsPad[1];
	ULONGLONG TestRetInstruction;
	LONGLONG QpcFrequency;
	ULONG SystemCall;
	union
	{
		ULONG AllFlags;
		struct
		{
			ULONG Win32Process : 1;
			ULONG Sgx2Enclave : 1;
			ULONG VbsBasicEnclave : 1;
			ULONG SpareBits : 29;
		};
	} UserCetAvailableEnvironments;
	ULONGLONG SystemCallPad[2];
	union
	{
		volatile KSYSTEM_TIME TickCount;
		volatile ULONG64 TickCountQuad;
		struct
		{
			ULONG ReservedTickCountOverlay[3];
			ULONG TickCountPad[1];
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;
	ULONG Cookie;
	ULONG CookiePad[1];
	LONGLONG ConsoleSessionForegroundProcessId;
	ULONGLONG TimeUpdateLock;
	ULONGLONG BaselineSystemTimeQpc;
	ULONGLONG BaselineInterruptTimeQpc;
	ULONGLONG QpcSystemTimeIncrement;
	ULONGLONG QpcInterruptTimeIncrement;
	UCHAR QpcSystemTimeIncrementShift;
	UCHAR QpcInterruptTimeIncrementShift;
	USHORT UnparkedProcessorCount;
	ULONG EnclaveFeatureMask[4];
	ULONG TelemetryCoverageRound;
	USHORT UserModeGlobalLogger[16];
	ULONG ImageFileExecutionOptions;
	ULONG LangGenerationCount;
	ULONGLONG Reserved4;
	volatile ULONGLONG InterruptTimeBias;
	volatile ULONGLONG QpcBias;
	ULONG ActiveProcessorCount;
	volatile UCHAR ActiveGroupCount;
	UCHAR Reserved9;
	union
	{
		USHORT QpcData;
		struct
		{
			volatile UCHAR QpcBypassEnabled : 1;
			UCHAR QpcShift : 1;
		};
	};
	LARGE_INTEGER TimeZoneBiasEffectiveStart;
	LARGE_INTEGER TimeZoneBiasEffectiveEnd;
	XSTATE_CONFIGURATION XState;
	KSYSTEM_TIME FeatureConfigurationChangeStamp;
	ULONG Spare;
} KUSER_SHARED_DATA, *PKUSER_SHARED_DATA;
#include <poppack.h>

C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, TickCountMultiplier) == 0x0004);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, InterruptTime) == 0x0008);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, SystemTime) == 0x0014);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, TimeZoneBias) == 0x0020);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ImageNumberLow) == 0x002c);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ImageNumberHigh) == 0x002e);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, NtSystemRoot) == 0x0030);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, MaxStackTraceDepth) == 0x0238);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, CryptoExponent) == 0x023c);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, TimeZoneId) == 0x0240);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, LargePageMinimum) == 0x0244);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, AitSamplingValue) == 0x0248);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, RNGSeedVersion) == 0x0250);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, NtProductType) == 0x0264);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, NtMajorVersion) == 0x026c);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, NtMinorVersion) == 0x0270);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ProcessorFeatures) == 0x0274);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, Reserved1) == 0x02b4);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, Reserved3) == 0x02b8);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, KdDebuggerEnabled) == 0x02d4);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ActiveConsoleId) == 0x02d8);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, NumberOfPhysicalPages) == 0x02e8);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, SafeBootMode) == 0x02ec);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, TickCount) == 0x0320);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, TickCountQuad) == 0x0320);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, Cookie) == 0x0330);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, QpcBias) == 0x03b8);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ActiveProcessorCount) == 0x03c0);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, ActiveGroupCount) == 0x03c4);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, Reserved9) == 0x03c5);
C_ASSERT (FIELD_OFFSET (KUSER_SHARED_DATA, XState) == 0x03d8);

#define USER_SHARED_DATA ((KUSER_SHARED_DATA * const)0x7ffe0000)

//
// Teb/Peb
//

#ifndef _WIN64
#define GDI_HANDLE_BUFFER_SIZE 34
#else
#define GDI_HANDLE_BUFFER_SIZE 60
#endif

#define GDI_BATCH_BUFFER_SIZE 310

#define RTL_MAX_DRIVE_LETTERS 32
#define RTL_DRIVE_LETTER_VALID (USHORT)0x0001

typedef struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME
{
	struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME *Previous;
	struct _ACTIVATION_CONTEXT *ActivationContext;
	ULONG                                       Flags;
} RTL_ACTIVATION_CONTEXT_STACK_FRAME, *PRTL_ACTIVATION_CONTEXT_STACK_FRAME;

typedef struct _ACTIVATION_CONTEXT_STACK
{
	RTL_ACTIVATION_CONTEXT_STACK_FRAME *ActiveFrame;
	LIST_ENTRY                          FrameListCache;
	ULONG                               Flags;
	ULONG                               NextCookieSequenceNumber;
	ULONG_PTR                           StackId;
} ACTIVATION_CONTEXT_STACK, *PACTIVATION_CONTEXT_STACK;

typedef struct _GDI_TEB_BATCH
{
	union
	{
		ULONG Offset;
		struct
		{
			ULONG Offset : 31; // Win 8.1 Update 1+
			ULONG HasRenderingCommand : 1;
		} bits;
	} dword0;
	ULONG_PTR HDC;
	ULONG Buffer[GDI_BATCH_BUFFER_SIZE];
} GDI_TEB_BATCH, *PGDI_TEB_BATCH;

typedef struct _TEB_ACTIVE_FRAME_CONTEXT
{
	ULONG       Flags;
	PSTR FrameName;
} TEB_ACTIVE_FRAME_CONTEXT, *PTEB_ACTIVE_FRAME_CONTEXT;

typedef struct _TEB_ACTIVE_FRAME
{
	ULONG Flags;
	struct _TEB_ACTIVE_FRAME *Previous;
	TEB_ACTIVE_FRAME_CONTEXT *Context;
} TEB_ACTIVE_FRAME, *PTEB_ACTIVE_FRAME;

typedef struct _PEB_LDR_DATA
{
	ULONG Length;
	BOOLEAN Initialized;
	HANDLE SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID EntryInProgress;
	BOOLEAN ShutdownInProgress;
	HANDLE ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _CURDIR
{
	UNICODE_STRING DosPath;
	HANDLE Handle;
} CURDIR, *PCURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR
{
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS
{
	ULONG MaximumLength;
	ULONG Length;

	ULONG Flags;
	ULONG DebugFlags;

	HANDLE ConsoleHandle;
	ULONG ConsoleFlags;
	HANDLE StandardInput;
	HANDLE StandardOutput;
	HANDLE StandardError;

	CURDIR CurrentDirectory;
	UNICODE_STRING DllPath;
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
	PVOID Environment;

	ULONG StartingPositionLeft;
	ULONG StartingPositionTop;
	ULONG Width;
	ULONG Height;
	ULONG CharWidth;
	ULONG CharHeight;
	ULONG ConsoleTextAttributes;

	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	UNICODE_STRING WindowTitle;
	UNICODE_STRING DesktopName;
	UNICODE_STRING ShellInfo;
	UNICODE_STRING RuntimeData;
	RTL_DRIVE_LETTER_CURDIR CurrentDirectories[RTL_MAX_DRIVE_LETTERS];

	ULONG_PTR EnvironmentSize;
	ULONG_PTR EnvironmentVersion;
	PVOID PackageDependencyData;
	ULONG ProcessGroupId;
	ULONG LoaderThreads;

	UNICODE_STRING RedirectionDllName; // REDSTONE4
	UNICODE_STRING HeapPartitionName; // 19H1
	ULONG_PTR DefaultThreadpoolCpuSetMasks;
	ULONG DefaultThreadpoolCpuSetMaskCount;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB
{
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;

	union
	{
		BOOLEAN BitField; // NT3.51-late WS03
		struct
		{
			BOOLEAN ImageUsesLargePages : 1; // WS03_SP1+
			BOOLEAN IsProtectedProcess : 1; // Vista+
			BOOLEAN IsImageDynamicallyRelocated : 1;
			BOOLEAN SkipPatchingUser32Forwarders : 1; // Vista_SP1+
			BOOLEAN IsPackagedProcess : 1; // Win8_BETA+
			BOOLEAN IsAppContainer : 1; // Win8_RTM+
			BOOLEAN IsProtectedProcessLight : 1;
			BOOLEAN IsLongPathAwareProcess : 1;
		};
	};

	HANDLE Mutant;

	PVOID ImageBaseAddress;
	PPEB_LDR_DATA Ldr;
	PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PRTL_CRITICAL_SECTION FastPebLock;

	union
	{
		PVOID FastPebLockRoutine; //NT3.51-Win2k
		PVOID SparePtr1; // early WS03
		PSLIST_HEADER AtlThunkSListPtr; // late WS03
	};

	union
	{
		PVOID FastPebUnlockRoutine; // NT3.51-XP
		PVOID SparePtr2; // WS03
		PVOID IFEOKey; // Vista+
	};

	union
	{
		ULONG CrossProcessFlags;
		struct
		{
			ULONG ProcessInJob : 1; // Vista+
			ULONG ProcessInitializing : 1;
			ULONG ProcessUsingVEH : 1; // Vista_SP1+
			ULONG ProcessUsingVCH : 1;
			ULONG ProcessUsingFTH : 1; // Win7_BETA+
			ULONG ProcessPreviouslyThrottled : 1;
			ULONG ProcessCurrentlyThrottled : 1;
			ULONG ProcessImagesHotPatched : 1; // REDSTONE5
			ULONG ReservedBits0 : 24;
		};
	};

	union
	{
		PVOID KernelCallbackTable; // Vista+
		PVOID UserSharedInfoPtr;
	};

	ULONG SystemReserved; // NT3.51-XP

	// Microsoft seems to keep changing their mind with DWORD 0x34
	union
	{
		ULONG SystemReserved2; // NT3.51-Win2k
		ULONG SpareUlong; // WS03-Vista
		ULONG AtlThunkSListPtr32; // late XP,Win7
	};

	union
	{
		PVOID FreeList; // NT3.51-early Vista
		ULONG SparePebPtr0; // last Vista
		PVOID ApiSetMap; // API_SET_NAMESPACE Win7+
	};

	ULONG TlsExpansionCounter;
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];

	PVOID ReadOnlySharedMemoryBase;

	union
	{
		PVOID ReadOnlyShareMemoryHeap; // NT3.51-WS03
		PVOID HotpatchInformation; // Vista+
	};

	PVOID *ReadOnlyStaticServerData;

	PVOID AnsiCodePageData; // PCPTABLEINFO
	PVOID OemCodePageData; // PCPTABLEINFO
	PVOID UnicodeCaseTableData; // PNLSTABLEINFO

	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;

	ULARGE_INTEGER CriticalSectionTimeout;
	SIZE_T HeapSegmentReserve;
	SIZE_T HeapSegmentCommit;
	SIZE_T HeapDeCommitTotalFreeThreshold;
	SIZE_T HeapDeCommitFreeBlockThreshold;

	ULONG NumberOfHeaps;
	ULONG MaximumNumberOfHeaps;
	PVOID *ProcessHeaps; // PHEAP

	PVOID GdiSharedHandleTable;

	// end of NT 3.51 members / members that follow available on NT 4.0 and up
	PVOID ProcessStarterHelper;
	ULONG GdiDCAttributeList;

	PRTL_CRITICAL_SECTION LoaderLock; // Win2k+

	ULONG OSMajorVersion;
	ULONG OSMinorVersion;
	USHORT OSBuildNumber;
	USHORT OSCSDVersion;
	ULONG OSPlatformId;

	ULONG ImageSubsystem;
	ULONG ImageSubsystemMajorVersion;
	ULONG ImageSubsystemMinorVersion;

	union
	{
		KAFFINITY ImageProcessAffinityMask; // NT4-early Vista
		KAFFINITY ActiveProcessAffinityMask; // late Vista+
	};

	ULONG GdiHandleBuffer[GDI_HANDLE_BUFFER_SIZE];
	PVOID PostProcessInitRoutine; // void (*PostProcessInitRoutine) (void);

	// members that follow available on Windows 2000 and up
	PVOID TlsExpansionBitmap;
	ULONG TlsExpansionBitmapBits[32];

	ULONG SessionId;

	ULARGE_INTEGER AppCompatFlags;
	ULARGE_INTEGER AppCompatFlagsUser;
	PVOID pShimData;
	PVOID AppCompatInfo; // APPCOMPAT_EXE_DATA

	UNICODE_STRING CSDVersion;

	// members that follow available on Windows XP and up
	PVOID ActivationContextData; // ACTIVATION_CONTEXT_DATA
	PVOID ProcessAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP
	PVOID SystemDefaultActivationContextData; // ACTIVATION_CONTEXT_DATA
	PVOID SystemAssemblyStorageMap; // ASSEMBLY_STORAGE_MAP

	SIZE_T MinimumStackCommit;

	// members that follow available on Windows Server 2003 and up
	PVOID SparePointers[4]; // 19H1 (previously FlsCallback to FlsHighIndex)
	ULONG SpareUlongs[5]; // 19H1
	//PVOID* FlsCallback;
	//LIST_ENTRY FlsListHead;
	//PVOID FlsBitmap;
	//ULONG FlsBitmapBits[FLS_MAXIMUM_AVAILABLE / (sizeof(ULONG) * 8)];
	//ULONG FlsHighIndex;

	// members that follow available on Windows Vista and up
	PVOID WerRegistrationData;
	PVOID WerShipAssertPtr;

	// members that follow available on Windows 7 BETA and up
	union
	{
		PVOID pContextData; // prior to Windows 8
		PVOID pUnused; // Windows 8+
	};

	PVOID pImageHeaderHash;

	// members that follow available on Windows 7 RTM and up
	union
	{
		ULONG TracingFlags;
		struct
		{
			ULONG HeapTracingEnabled : 1;
			ULONG CritSecTracingEnabled : 1;
			ULONG LibLoaderTracingEnabled : 1;
			ULONG SpareTracingBits : 29;
		};
	};

	// members that follow available on Windows 8 and up
	ULONGLONG CsrServerReadOnlySharedMemoryBase;

	// members that follow available on Windows 10 and up
	PRTL_CRITICAL_SECTION TppWorkerpListLock;

	union // conflicting reports about what 0x254 points to
	{
		LIST_ENTRY TppWorkerpList;
		ULONG dwSystemCallMode; // set to 2 under 64-bit Windows in a 32-bit process (WOW64)
	};

	PVOID WaitOnAddressHashTable[128];
	PVOID TelemetryCoverageHeader; // REDSTONE3
	ULONG CloudFileFlags;
	ULONG CloudFileDiagFlags; // REDSTONE4
	CHAR PlaceholderCompatibilityMode;
	CHAR PlaceholderCompatibilityModeReserved[7];
	struct _LEAP_SECOND_DATA *LeapSecondData; // REDSTONE5

	union
	{
		ULONG LeapSecondFlags;
		struct
		{
			ULONG SixtySecondEnabled : 1;
			ULONG Reserved : 31;
		};
	};

	ULONG NtGlobalFlag2;
} PEB, *PPEB;

#ifdef _WIN64
C_ASSERT (FIELD_OFFSET (PEB, KernelCallbackTable) == 0x0058);
C_ASSERT (FIELD_OFFSET (PEB, SessionId) == 0x02C0);
C_ASSERT (FIELD_OFFSET (PEB, MinimumStackCommit) == 0x0318);

//C_ASSERT(sizeof(PEB) == 0x7B0); // REDSTONE3
//C_ASSERT(sizeof(PEB) == 0x7B8); // REDSTONE4
C_ASSERT (sizeof (PEB) == 0x7C8); // REDSTONE5 // 19H1

#else

C_ASSERT (FIELD_OFFSET (PEB, KernelCallbackTable) == 0x002c);
C_ASSERT (FIELD_OFFSET (PEB, SessionId) == 0x01d4);
C_ASSERT (FIELD_OFFSET (PEB, MinimumStackCommit) == 0x0208);

//C_ASSERT(sizeof(PEB) == 0x468); // REDSTONE3
//C_ASSERT(sizeof(PEB) == 0x470); // REDSTONE4
C_ASSERT (sizeof (PEB) == 0x480); // REDSTONE5 // 19H1
#endif

typedef struct _TEB
{
	NT_TIB NtTib;

	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	PPEB ProcessEnvironmentBlock;

	ULONG LastErrorValue;
	ULONG CountOfOwnedCriticalSections;
	PVOID CsrClientThread;
	PVOID Win32ThreadInfo;
	ULONG User32Reserved[26];
	ULONG UserReserved[5];
	PVOID WOW32Reserved; // user-mode 32-bit (WOW64) -> 64-bit context switch function prior to kernel-mode transition
	LCID CurrentLocale;
	ULONG FpSoftwareStatusRegister;
	PVOID ReservedForDebuggerInstrumentation[16]; // Win10 PRE-RTM+

#ifdef _WIN64
	PVOID SystemReserved1[30];
#else
	PVOID SystemReserved1[26];
#endif

	CHAR PlaceholderCompatibilityMode;
	BOOLEAN PlaceholderHydrationAlwaysExplicit;
	CHAR PlaceholderReserved[10];

	ULONG ProxiedProcessId;
	ACTIVATION_CONTEXT_STACK ActivationContextStack; // XP-early WS03

	UCHAR WorkingOnBehalfOfTicket[8];
	NTSTATUS ExceptionCode;

	PACTIVATION_CONTEXT_STACK ActivationContextStackPointer; // WS03+
	ULONG_PTR InstrumentationCallbackSp; // Win10+
	ULONG_PTR InstrumentationCallbackPreviousPc; // Win10+
	ULONG_PTR InstrumentationCallbackPreviousSp; // Win10+

#ifdef _WIN64
	ULONG TxFsContext;
#endif

	BOOLEAN InstrumentationCallbackDisabled;

#ifdef _WIN64
	BOOLEAN UnalignedLoadStoreExceptions;
#else
	UCHAR SpareBytes[23];
	ULONG TxFsContext;
#endif

	GDI_TEB_BATCH GdiTebBatch;
	CLIENT_ID RealClientId;
	HANDLE GdiCachedProcessHandle;
	ULONG GdiClientPID;
	ULONG GdiClientTID;
	PVOID GdiThreadLocalInfo;
	ULONG_PTR Win32ClientInfo[62];
	PVOID glDispatchTable[233];
	ULONG_PTR glReserved1[29];
	PVOID glReserved2;
	PVOID glSectionInfo;
	PVOID glSection;
	PVOID glTable;
	PVOID glCurrentRC;
	PVOID glContext;

	NTSTATUS LastStatusValue;
	UNICODE_STRING StaticUnicodeString;
	WCHAR StaticUnicodeBuffer[261];

	PVOID DeallocationStack;
	PVOID TlsSlots[64];
	LIST_ENTRY TlsLinks;

	PVOID Vdm;
	PVOID ReservedForNtRpc;
	PVOID DbgSsReserved[2];

	ULONG HardErrorMode;

#ifdef _WIN64
	PVOID Instrumentation[11];
#else
	PVOID Instrumentation[9];
#endif

	GUID ActivityId;

	PVOID SubProcessTag;
	PVOID PerflibData;
	PVOID EtwTraceData;
	PVOID WinSockData;
	ULONG GdiBatchCount;

	union
	{
		PROCESSOR_NUMBER CurrentIdealProcessor;
		ULONG IdealProcessorValue;
		struct
		{
			UCHAR ReservedPad0;
			UCHAR ReservedPad1;
			UCHAR ReservedPad2;
			UCHAR IdealProcessor;
		};
	};

	ULONG GuaranteedStackBytes; // late WS03+
	PVOID ReservedForPerf;
	PVOID ReservedForOle;
	ULONG WaitingOnLoaderLock;
	PVOID SavedPriorityState;
	ULONG_PTR ReservedForCodeCoverage;
	PVOID ThreadPoolData;
	PVOID *TlsExpansionSlots;

#ifdef _WIN64
	PVOID DeallocationBStore;
	PVOID BStoreLimit;
#endif

	ULONG MuiGeneration;
	ULONG IsImpersonating;
	PVOID NlsCache;
	PVOID ShimData;
	ULONG HeapData;
	HANDLE CurrentTransactionHandle;
	PTEB_ACTIVE_FRAME ActiveFrame;
	PVOID FlsData;

	PVOID PreferredLanguages;
	PVOID UserPrefLanguages;
	PVOID MergedPrefLanguages;
	ULONG MuiImpersonation;

	union
	{
		USHORT CrossTebFlags;
		USHORT SpareCrossTebBits : 16;
	};

	union
	{
		USHORT SameTebFlags;
		struct
		{
			USHORT SafeThunkCall : 1;
			USHORT InDebugPrint : 1;
			USHORT HasFiberData : 1;
			USHORT SkipThreadAttach : 1;
			USHORT WerInShipAssertCode : 1;
			USHORT RanProcessInit : 1;
			USHORT ClonedThread : 1;
			USHORT SuppressDebugMsg : 1;
			USHORT DisableUserStackWalk : 1;
			USHORT RtlExceptionAttached : 1;
			USHORT InitialThread : 1;
			USHORT SessionAware : 1;
			USHORT LoadOwner : 1;
			USHORT LoaderWorker : 1;
			USHORT SkipLoaderInit : 1;
			USHORT SpareSameTebBits : 1;
		};
	};

	PVOID TxnScopeEnterCallback;
	PVOID TxnScopeExitCallback;
	PVOID TxnScopeContext;
	ULONG LockCount;
	LONG WowTebOffset;
	PVOID ResourceRetValue;
	PVOID ReservedForWdf;
	ULONGLONG ReservedForCrt;
	GUID EffectiveContainerId;
} TEB, *PTEB;

#ifdef _WIN64
C_ASSERT (FIELD_OFFSET (TEB, ProcessEnvironmentBlock) == 0x0060);
C_ASSERT (FIELD_OFFSET (TEB, Win32ThreadInfo) == 0x0078);
C_ASSERT (FIELD_OFFSET (TEB, SystemReserved1) == 0x0190);
C_ASSERT (FIELD_OFFSET (TEB, ReservedForDebuggerInstrumentation) == 0x0110);
C_ASSERT (FIELD_OFFSET (TEB, ExceptionCode) == 0x02C0);
C_ASSERT (FIELD_OFFSET (TEB, InstrumentationCallbackDisabled) == 0x02EC);
C_ASSERT (FIELD_OFFSET (TEB, LastStatusValue) == 0x1250);
#else
C_ASSERT (FIELD_OFFSET (TEB, ProcessEnvironmentBlock) == 0x0030);
C_ASSERT (FIELD_OFFSET (TEB, Win32ThreadInfo) == 0x0040);
C_ASSERT (FIELD_OFFSET (TEB, SystemReserved1) == 0x010C);
C_ASSERT (FIELD_OFFSET (TEB, ReservedForDebuggerInstrumentation) == 0x00CC);
C_ASSERT (FIELD_OFFSET (TEB, ExceptionCode) == 0x01A4);
C_ASSERT (FIELD_OFFSET (TEB, InstrumentationCallbackDisabled) == 0x01B8);
C_ASSERT (FIELD_OFFSET (TEB, LastStatusValue) == 0x0BF4);
#endif

//
// Heaps
//

typedef NTSTATUS (NTAPI *PRTL_HEAP_COMMIT_ROUTINE)(
	_In_ PVOID Base,
	_Inout_ PVOID *CommitAddress,
	_Inout_ PSIZE_T CommitSize
	);

typedef struct _RTL_HEAP_PARAMETERS
{
	ULONG Length;
	SIZE_T SegmentReserve;
	SIZE_T SegmentCommit;
	SIZE_T DeCommitFreeBlockThreshold;
	SIZE_T DeCommitTotalFreeThreshold;
	SIZE_T MaximumAllocationSize;
	SIZE_T VirtualMemoryThreshold;
	SIZE_T InitialCommit;
	SIZE_T InitialReserve;
	PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
	SIZE_T Reserved[2];
} RTL_HEAP_PARAMETERS, *PRTL_HEAP_PARAMETERS;

typedef enum _HEAP_COMPATIBILITY_MODE
{
	HEAP_COMPATIBILITY_STANDARD = 0UL,
	HEAP_COMPATIBILITY_LAL = 1UL,
	HEAP_COMPATIBILITY_LFH = 2UL,
} HEAP_COMPATIBILITY_MODE;

#define HEAP_SETTABLE_USER_VALUE 0x00000100
#define HEAP_SETTABLE_USER_FLAG1 0x00000200
#define HEAP_SETTABLE_USER_FLAG2 0x00000400
#define HEAP_SETTABLE_USER_FLAG3 0x00000800
#define HEAP_SETTABLE_USER_FLAGS 0x00000e00

#define HEAP_CLASS_0 0x00000000 // Process heap
#define HEAP_CLASS_1 0x00001000 // Private heap
#define HEAP_CLASS_2 0x00002000 // Kernel heap
#define HEAP_CLASS_3 0x00003000 // GDI heap
#define HEAP_CLASS_4 0x00004000 // User heap
#define HEAP_CLASS_5 0x00005000 // Console heap
#define HEAP_CLASS_6 0x00006000 // User desktop heap
#define HEAP_CLASS_7 0x00007000 // CSR shared heap
#define HEAP_CLASS_8 0x00008000 // CSR port heap
#define HEAP_CLASS_MASK 0x0000f000

typedef NTSTATUS (NTAPI *PUSER_THREAD_START_ROUTINE)(
	_In_ PVOID ThreadParameter
	);

typedef struct _OBJECT_DIRECTORY_INFORMATION
{
	UNICODE_STRING Name;
	UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;

#define DIRECTORY_QUERY 0x0001
#define DIRECTORY_TRAVERSE 0x0002
#define DIRECTORY_CREATE_OBJECT 0x0004
#define DIRECTORY_CREATE_SUBDIRECTORY 0x0008
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xf)

typedef struct _RTLP_CURDIR_REF
{
	LONG ReferenceCount;
	HANDLE DirectoryHandle;
} RTLP_CURDIR_REF, *PRTLP_CURDIR_REF;

typedef struct _RTL_RELATIVE_NAME_U
{
	UNICODE_STRING RelativeName;
	HANDLE ContainingDirectory;
	PRTLP_CURDIR_REF CurDirRef;
} RTL_RELATIVE_NAME_U, *PRTL_RELATIVE_NAME_U;

typedef enum _RTL_PATH_TYPE
{
	RtlPathTypeUnknown,
	RtlPathTypeUncAbsolute,
	RtlPathTypeDriveAbsolute,
	RtlPathTypeDriveRelative,
	RtlPathTypeRooted,
	RtlPathTypeRelative,
	RtlPathTypeLocalDevice,
	RtlPathTypeRootLocalDevice
} RTL_PATH_TYPE;

typedef struct _IO_STATUS_BLOCK
{
	union
	{
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

//
// nt functions
//

EXTERN_C_START

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlCreateUserThread (
	_In_ HANDLE Process,
	_In_opt_ PSECURITY_DESCRIPTOR ThreadSecurityDescriptor,
	_In_ BOOLEAN CreateSuspended,
	_In_opt_ ULONG ZeroBits,
	_In_opt_ SIZE_T MaximumStackSize,
	_In_opt_ SIZE_T CommittedStackSize,
	_In_ PUSER_THREAD_START_ROUTINE StartAddress,
	_In_opt_ PVOID Parameter,
	_Out_opt_ PHANDLE Thread,
	_Out_opt_ PCLIENT_ID ClientId
);

DECLSPEC_NORETURN
NTSYSCALLAPI
VOID
NTAPI
RtlExitUserThread (
	_In_ NTSTATUS ExitStatus
);

DECLSPEC_NORETURN
NTSYSCALLAPI
VOID
NTAPI
RtlExitUserProcess (
	_In_ NTSTATUS ExitStatus
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenThread (
	_Out_ PHANDLE ThreadHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtTerminateThread (
	_In_opt_ HANDLE ThreadHandle,
	_In_ NTSTATUS ExitStatus
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSuspendThread (
	_In_ HANDLE ThreadHandle,
	_Out_opt_ PULONG PreviousSuspendCount
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtResumeThread (
	_In_ HANDLE ThreadHandle,
	_Out_opt_ PULONG PreviousSuspendCount
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtGetContextThread (
	_In_ HANDLE ThreadHandle,
	_Inout_ PCONTEXT ThreadContext
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetContextThread (
	_In_ HANDLE ThreadHandle,
	_In_ PCONTEXT ThreadContext
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryInformationThread (
	_In_ HANDLE ThreadHandle,
	_In_ THREADINFOCLASS ThreadInformationClass,
	_Out_writes_bytes_ (ThreadInformationLength) PVOID ThreadInformation,
	_In_ ULONG ThreadInformationLength,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetInformationThread (
	_In_ HANDLE ThreadHandle,
	_In_ THREADINFOCLASS ThreadInformationClass,
	_In_reads_bytes_ (ThreadInformationLength) PVOID ThreadInformation,
	_In_ ULONG ThreadInformationLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlertThread (
	_In_ HANDLE ThreadHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAlertResumeThread (
	_In_ HANDLE ThreadHandle,
	_Out_opt_ PULONG PreviousSuspendCount
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtTestAlert (
	VOID
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtImpersonateThread (
	_In_ HANDLE ServerThreadHandle,
	_In_ HANDLE ClientThreadHandle,
	_In_ PSECURITY_QUALITY_OF_SERVICE SecurityQos
);

NTSYSCALLAPI
PVOID
NTAPI
RtlCreateHeap (
	_In_ ULONG Flags,
	_In_opt_ PVOID HeapBase,
	_In_opt_ SIZE_T ReserveSize,
	_In_opt_ SIZE_T CommitSize,
	_In_opt_ PVOID Lock,
	_In_opt_ PRTL_HEAP_PARAMETERS Parameters
);

NTSYSCALLAPI
PVOID
NTAPI
RtlDestroyHeap (
	_In_ _Post_invalid_ PVOID HeapHandle
);

NTSYSCALLAPI
PVOID
NTAPI
RtlAllocateHeap (
	_In_ PVOID HeapHandle,
	_In_opt_ ULONG Flags,
	_In_ SIZE_T Size
);

NTSYSCALLAPI
PVOID
NTAPI
RtlReAllocateHeap (
	_In_ PVOID HeapHandle,
	_In_ ULONG Flags,
	_Frees_ptr_opt_ PVOID BaseAddress,
	_In_ SIZE_T Size
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlFreeHeap (
	_In_ PVOID HeapHandle,
	_In_opt_ ULONG Flags,
	_Frees_ptr_opt_ PVOID BaseAddress
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlSetHeapInformation (
	_In_ PVOID HeapHandle,
	_In_ HEAP_INFORMATION_CLASS HeapInformationClass,
	_In_opt_ PVOID HeapInformation,
	_In_opt_ SIZE_T HeapInformationLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDelayExecution (
	_In_ BOOLEAN Alertable,
	_In_opt_ PLARGE_INTEGER DelayInterval
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryObject (
	_In_opt_ HANDLE Handle,
	_In_ OBJECT_INFORMATION_CLASS ObjectInformationClass,
	_Out_writes_bytes_opt_ (ObjectInformationLength) PVOID ObjectInformation,
	_In_ ULONG ObjectInformationLength,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetSystemInformation (
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_In_reads_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQuerySystemInformation (
	_In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
	_Out_writes_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
	_In_ ULONG SystemInformationLength,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenProcess (
	_Out_ PHANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_opt_ PCLIENT_ID ClientId
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtTerminateProcess (
	_In_opt_ HANDLE ProcessHandle,
	_In_ NTSTATUS ExitStatus
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSuspendProcess (
	_In_ HANDLE ProcessHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtResumeProcess (
	_In_ HANDLE ProcessHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetInformationProcess (
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_In_reads_bytes_ (ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryInformationProcess (
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_Out_writes_bytes_ (ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenSymbolicLinkObject (
	_Out_ PHANDLE LinkHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQuerySymbolicLinkObject (
	_In_ HANDLE LinkHandle,
	_Inout_ PUNICODE_STRING LinkTarget,
	_Out_opt_ PULONG ReturnedLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateToken (
	_Out_ PHANDLE TokenHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ TOKEN_TYPE TokenType,
	_In_ PLUID AuthenticationId,
	_In_ PLARGE_INTEGER ExpirationTime,
	_In_ PTOKEN_USER User,
	_In_ PTOKEN_GROUPS Groups,
	_In_ PTOKEN_PRIVILEGES Privileges,
	_In_opt_ PTOKEN_OWNER Owner,
	_In_ PTOKEN_PRIMARY_GROUP PrimaryGroup,
	_In_opt_ PTOKEN_DEFAULT_DACL DefaultDacl,
	_In_ PTOKEN_SOURCE TokenSource
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenProcessToken (
	_In_ HANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_Out_ PHANDLE TokenHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenProcessTokenEx (
	_In_ HANDLE ProcessHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ ULONG HandleAttributes,
	_Out_ PHANDLE TokenHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenThreadToken (
	_In_ HANDLE ThreadHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ BOOLEAN OpenAsSelf,
	_Out_ PHANDLE TokenHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenThreadTokenEx (
	_In_ HANDLE ThreadHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ BOOLEAN OpenAsSelf,
	_In_ ULONG HandleAttributes,
	_Out_ PHANDLE TokenHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDuplicateToken (
	_In_ HANDLE ExistingTokenHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ BOOLEAN EffectiveOnly,
	_In_ TOKEN_TYPE Type,
	_Out_ PHANDLE NewTokenHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryInformationToken (
	_In_ HANDLE TokenHandle,
	_In_ TOKEN_INFORMATION_CLASS TokenInformationClass,
	_Out_writes_bytes_to_opt_ (TokenInformationLength, *ReturnLength) PVOID TokenInformation,
	_In_ ULONG TokenInformationLength,
	_Out_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetInformationToken (
	_In_ HANDLE TokenHandle,
	_In_ TOKEN_INFORMATION_CLASS TokenInformationClass,
	_In_reads_bytes_ (TokenInformationLength) PVOID TokenInformation,
	_In_ ULONG TokenInformationLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtAdjustPrivilegesToken (
	_In_ HANDLE TokenHandle,
	_In_ BOOLEAN DisableAllPrivileges,
	_In_opt_ PTOKEN_PRIVILEGES NewState,
	_In_ ULONG BufferLength,
	_Out_writes_bytes_to_opt_ (BufferLength, *ReturnLength) PTOKEN_PRIVILEGES PreviousState,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtPrivilegeCheck (
	_In_ HANDLE ClientToken,
	_Inout_ PPRIVILEGE_SET RequiredPrivileges,
	_Out_ PBOOLEAN Result
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtImpersonateAnonymousToken (
	_In_ HANDLE ThreadHandle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateFile (
	_Out_ PHANDLE FileHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_opt_ PLARGE_INTEGER AllocationSize,
	_In_ ULONG FileAttributes,
	_In_ ULONG ShareAccess,
	_In_ ULONG CreateDisposition,
	_In_ ULONG CreateOptions,
	_In_reads_bytes_opt_ (EaLength) PVOID EaBuffer,
	_In_ ULONG EaLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDeleteFile (
	_In_ POBJECT_ATTRIBUTES ObjectAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtFlushBuffersFile (
	_In_ HANDLE FileHandle,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtClose (
	_In_ HANDLE Handle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlGetVersion (
	_Out_ PRTL_OSVERSIONINFOEXW VersionInformation // PRTL_OSVERSIONINFOW
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlDoesFileExists_U (
	_In_ PCWSTR FileName
);

NTSYSCALLAPI
ULONG
NTAPI
RtlGetFullPathName_U (
	_In_ PCWSTR FileName,
	_In_ ULONG BufferLength,
	_Out_writes_bytes_ (BufferLength) PWSTR Buffer,
	_Out_opt_ PWSTR *FilePart
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlDosPathNameToNtPathName_U (
	_In_ PCWSTR DosFileName,
	_Out_ PUNICODE_STRING NtFileName,
	_Out_opt_ PWSTR *FilePart,
	_Out_opt_ PRTL_RELATIVE_NAME_U RelativeName
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlDosPathNameToNtPathName_U_WithStatus (
	_In_ PCWSTR DosFileName,
	_Out_ PUNICODE_STRING NtFileName,
	_Out_opt_ PWSTR *FilePart,
	_Out_opt_ PRTL_RELATIVE_NAME_U RelativeName
);

// server2003+
NTSYSCALLAPI
VOID
NTAPI
RtlReleaseRelativeName (
	_Inout_ PRTL_RELATIVE_NAME_U RelativeName
);

NTSYSCALLAPI
VOID
NTAPI
RtlInitUnicodeString (
	_Out_ PUNICODE_STRING DestinationString,
	_In_opt_z_ PCWSTR SourceString
);

NTSYSCALLAPI
VOID
NTAPI
RtlFreeUnicodeString (
	_In_ PUNICODE_STRING UnicodeString
);

NTSYSCALLAPI
WCHAR
NTAPI
RtlUpcaseUnicodeChar (
	_In_ WCHAR SourceCharacter
);

NTSYSCALLAPI
WCHAR
NTAPI
RtlDowncaseUnicodeChar (
	_In_ WCHAR SourceCharacter
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlStringFromGUID (
	_In_ LPGUID Guid,
	_Out_ PUNICODE_STRING GuidString
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlGUIDFromString (
	_In_ PUNICODE_STRING GuidString,
	_Out_ LPGUID Guid
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlCreateServiceSid (
	_In_ PUNICODE_STRING ServiceName,
	_Out_writes_bytes_opt_ (*ServiceSidLength) PSID ServiceSid,
	_Inout_ PULONG ServiceSidLength
);

NTSYSCALLAPI
PULONG
NTAPI
RtlSubAuthoritySid (
	_In_ PSID Sid,
	_In_ ULONG SubAuthority
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlConvertSidToUnicodeString (
	_Inout_ PUNICODE_STRING UnicodeString,
	_In_ PSID Sid,
	_In_ BOOLEAN AllocateDestinationString
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlValidSid (
	_In_ PSID Sid
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlEqualSid (
	_In_ PSID Sid1,
	_In_ PSID Sid2
);

NTSYSCALLAPI
ULONG
NTAPI
RtlLengthSid (
	_In_ PSID Sid
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlCreateSecurityDescriptor (
	_Out_ PSECURITY_DESCRIPTOR SecurityDescriptor,
	_In_ ULONG Revision
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlValidSecurityDescriptor (
	_In_ PSECURITY_DESCRIPTOR SecurityDescriptor
);

NTSYSCALLAPI
ULONG
NTAPI
RtlLengthSecurityDescriptor (
	_In_ PSECURITY_DESCRIPTOR SecurityDescriptor
);

NTSYSCALLAPI
ULONG
NTAPI
RtlRandomEx (
	_Inout_ PULONG Seed
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlUnicodeToMultiByteSize (
	_Out_ PULONG BytesInMultiByteString,
	_In_reads_bytes_ (BytesInUnicodeString) PCWCH UnicodeString,
	_In_ ULONG BytesInUnicodeString
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlUnicodeToMultiByteN (
	_Out_writes_bytes_to_ (MaxBytesInMultiByteString, *BytesInMultiByteString) PCHAR MultiByteString,
	_In_ ULONG MaxBytesInMultiByteString,
	_Out_opt_ PULONG BytesInMultiByteString,
	_In_reads_bytes_ (BytesInUnicodeString) PCWCH UnicodeString,
	_In_ ULONG BytesInUnicodeString
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlMultiByteToUnicodeSize (
	_Out_ PULONG BytesInUnicodeString,
	_In_reads_bytes_ (BytesInMultiByteString) PCSTR MultiByteString,
	_In_ ULONG BytesInMultiByteString
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlMultiByteToUnicodeN (
	_Out_writes_bytes_to_ (MaxBytesInUnicodeString, *BytesInUnicodeString) PWCH UnicodeString,
	_In_ ULONG MaxBytesInUnicodeString,
	_Out_opt_ PULONG BytesInUnicodeString,
	_In_reads_bytes_ (BytesInMultiByteString) PCSTR MultiByteString,
	_In_ ULONG BytesInMultiByteString
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlExpandEnvironmentStrings_U (
	_In_opt_ PVOID Environment,
	_In_ PUNICODE_STRING Source,
	_Inout_ PUNICODE_STRING Destination,
	_Out_opt_ PULONG ReturnedLength
);

NTSYSCALLAPI
ULONG
NTAPI
RtlNtStatusToDosError (
	_In_ NTSTATUS Status
);

NTSYSCALLAPI
ULONG
NTAPI
RtlNtStatusToDosErrorNoTeb (
	_In_ NTSTATUS Status
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlGetLastNtStatus (
	VOID
);

NTSYSCALLAPI
LONG
NTAPI
RtlGetLastWin32Error (
	VOID
);

NTSYSCALLAPI
VOID
NTAPI
RtlSetLastWin32ErrorAndNtStatusFromNtStatus (
	_In_ NTSTATUS Status
);

NTSYSCALLAPI
VOID
NTAPI
RtlSetLastWin32Error (
	_In_ LONG Win32Error
);

NTSYSCALLAPI
VOID
NTAPI
RtlRestoreLastWin32Error (
	_In_ LONG Win32Error
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenDirectoryObject (
	_Out_ PHANDLE DirectoryHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryDirectoryObject (
	_In_ HANDLE DirectoryHandle,
	_Out_writes_bytes_opt_ (Length) PVOID Buffer,
	_In_ ULONG Length,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_ BOOLEAN RestartScan,
	_Inout_ PULONG Context,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateSemaphore (
	_Out_ PHANDLE SemaphoreHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ LONG InitialCount,
	_In_ LONG MaximumCount
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReleaseSemaphore (
	_In_ HANDLE SemaphoreHandle,
	_In_ LONG ReleaseCount,
	_Out_opt_ PLONG PreviousCount
);

NTSYSCALLAPI
BOOLEAN
NTAPI
RtlDispatchException (
	_In_ PEXCEPTION_RECORD ExceptionRecord,
	_In_ PCONTEXT ContextRecord
);

NTSYSCALLAPI
DECLSPEC_NORETURN
VOID
NTAPI
RtlRaiseStatus (
	_In_ NTSTATUS Status
);

NTSYSCALLAPI
VOID
NTAPI
RtlRaiseException (
	_In_ PEXCEPTION_RECORD ExceptionRecord
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtContinue (
	_In_ PCONTEXT ContextRecord,
	_In_ BOOLEAN TestAlert
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtRaiseException (
	_In_ PEXCEPTION_RECORD ExceptionRecord,
	_In_ PCONTEXT ContextRecord,
	_In_ BOOLEAN FirstChance
);

__analysis_noreturn
NTSYSCALLAPI
VOID
NTAPI
RtlAssert (
	_In_ PVOID VoidFailedAssertion,
	_In_ PVOID VoidFileName,
	_In_ ULONG LineNumber,
	_In_opt_ PSTR MutableMessage
);

// Vectored Exception Handlers

NTSYSCALLAPI
PVOID
NTAPI
RtlAddVectoredExceptionHandler (
	_In_ ULONG First,
	_In_ PVECTORED_EXCEPTION_HANDLER Handler
);

NTSYSCALLAPI
ULONG
NTAPI
RtlRemoveVectoredExceptionHandler (
	_In_ PVOID Handle
);

NTSYSCALLAPI
PVOID
NTAPI
RtlAddVectoredContinueHandler (
	_In_ ULONG First,
	_In_ PVECTORED_EXCEPTION_HANDLER Handler
);

NTSYSCALLAPI
ULONG
NTAPI
RtlRemoveVectoredContinueHandler (
	_In_ PVOID Handle
);

// Runtime exception handling

typedef ULONG (NTAPI *PRTLP_UNHANDLED_EXCEPTION_FILTER)(
	_In_ PEXCEPTION_POINTERS ExceptionInfo
	);

// vista+
NTSYSCALLAPI
VOID
NTAPI
RtlSetUnhandledExceptionFilter (
	_In_ PRTLP_UNHANDLED_EXCEPTION_FILTER UnhandledExceptionFilter
);

// rev
NTSYSCALLAPI
LONG
NTAPI
RtlUnhandledExceptionFilter (
	_In_ PEXCEPTION_POINTERS ExceptionPointers
);

// rev
NTSYSCALLAPI
LONG
NTAPI
RtlUnhandledExceptionFilter2 (
	_In_ PEXCEPTION_POINTERS ExceptionPointers,
	_In_ ULONG Flags
);

// rev
NTSYSCALLAPI
LONG
NTAPI
RtlKnownExceptionFilter (
	_In_ PEXCEPTION_POINTERS ExceptionPointers
);

// winbase:InitializeSRWLock
NTSYSCALLAPI
VOID
NTAPI
RtlInitializeSRWLock (
	_Out_ PRTL_SRWLOCK SRWLock
);

// winbase:AcquireSRWLockExclusive
NTSYSCALLAPI
VOID
NTAPI
RtlAcquireSRWLockExclusive (
	_Inout_ PRTL_SRWLOCK SRWLock
);

// winbase:AcquireSRWLockShared
NTSYSCALLAPI
VOID
NTAPI
RtlAcquireSRWLockShared (
	_Inout_ PRTL_SRWLOCK SRWLock
);

// winbase:ReleaseSRWLockExclusive
NTSYSCALLAPI
VOID
NTAPI
RtlReleaseSRWLockExclusive (
	_Inout_ PRTL_SRWLOCK SRWLock
);

// winbase:ReleaseSRWLockShared
NTSYSCALLAPI
VOID
NTAPI
RtlReleaseSRWLockShared (
	_Inout_ PRTL_SRWLOCK SRWLock
);

// winbase:TryAcquireSRWLockExclusive
NTSYSCALLAPI
BOOLEAN
NTAPI
RtlTryAcquireSRWLockExclusive (
	_Inout_ PRTL_SRWLOCK SRWLock
);

// winbase:TryAcquireSRWLockShared
NTSYSCALLAPI
BOOLEAN
NTAPI
RtlTryAcquireSRWLockShared (
	_Inout_ PRTL_SRWLOCK SRWLock
);

NTSYSCALLAPI
NTSTATUS
RtlRunOnceBeginInitialize (
	_Inout_ PRTL_RUN_ONCE RunOnce,
	_In_ ULONG Flags,
	_In_opt_ PVOID *Context
);

NTSYSCALLAPI
NTSTATUS
RtlRunOnceComplete (
	_Inout_ PRTL_RUN_ONCE RunOnce,
	_In_ ULONG Flags,
	_In_opt_ PVOID Context
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateEvent (
	_Out_ PHANDLE EventHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ EVENT_TYPE EventType,
	_In_ BOOLEAN InitialState
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetEvent (
	_In_ HANDLE EventHandle,
	_Out_opt_ PLONG PreviousState
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtWaitForSingleObject (
	_In_ HANDLE Handle,
	_In_ BOOLEAN Alertable,
	_In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtSetThreadExecutionState (
	_In_ EXECUTION_STATE NewFlags, // ES_* flags
	_Out_ EXECUTION_STATE *PreviousFlags
);

// Keyed Event

#define KEYEDEVENT_WAIT 0x0001
#define KEYEDEVENT_WAKE 0x0002
#define KEYEDEVENT_ALL_ACCESS \
    (STANDARD_RIGHTS_REQUIRED | KEYEDEVENT_WAIT | KEYEDEVENT_WAKE)

NTSYSCALLAPI
NTSTATUS
NTAPI
NtCreateKeyedEvent (
	_Out_ PHANDLE KeyedEventHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ ULONG Flags
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtOpenKeyedEvent (
	_Out_ PHANDLE KeyedEventHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReleaseKeyedEvent (
	_In_ HANDLE KeyedEventHandle,
	_In_ PVOID KeyValue,
	_In_ BOOLEAN Alertable,
	_In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtWaitForKeyedEvent (
	_In_ HANDLE KeyedEventHandle,
	_In_ PVOID KeyValue,
	_In_ BOOLEAN Alertable,
	_In_opt_ PLARGE_INTEGER Timeout
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlGetCompressionWorkSpaceSize (
	_In_ USHORT CompressionFormatAndEngine,
	_Out_ PULONG CompressBufferWorkSpaceSize,
	_Out_ PULONG CompressFragmentWorkSpaceSize
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlCompressBuffer (
	_In_ USHORT CompressionFormatAndEngine,
	_In_reads_bytes_ (UncompressedBufferSize) PUCHAR UncompressedBuffer,
	_In_ ULONG UncompressedBufferSize,
	_Out_writes_bytes_to_ (CompressedBufferSize, *FinalCompressedSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_In_ ULONG UncompressedChunkSize,
	_Out_ PULONG FinalCompressedSize,
	_In_ PVOID WorkSpace
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlDecompressBuffer (
	_In_ USHORT CompressionFormat,
	_Out_writes_bytes_to_ (UncompressedBufferSize, *FinalUncompressedSize) PUCHAR UncompressedBuffer,
	_In_ ULONG UncompressedBufferSize,
	_In_reads_bytes_ (CompressedBufferSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_Out_ PULONG FinalUncompressedSize
);

NTSYSCALLAPI// win8+
NTSTATUS
NTAPI
RtlDecompressBufferEx (
	_In_ USHORT CompressionFormat,
	_Out_writes_bytes_to_ (UncompressedBufferSize, *FinalUncompressedSize) PUCHAR UncompressedBuffer,
	_In_ ULONG UncompressedBufferSize,
	_In_reads_bytes_ (CompressedBufferSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_Out_ PULONG FinalUncompressedSize,
	_In_opt_ PVOID WorkSpace
);

// win81+
NTSYSCALLAPI
NTSTATUS
NTAPI
RtlDecompressBufferEx2 (
	_In_ USHORT CompressionFormat,
	_Out_writes_bytes_to_ (UncompressedBufferSize, *FinalUncompressedSize) PUCHAR UncompressedBuffer,
	_In_ ULONG UncompressedBufferSize,
	_In_reads_bytes_ (CompressedBufferSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_In_ ULONG UncompressedChunkSize,
	_Out_ PULONG FinalUncompressedSize,
	_In_opt_ PVOID WorkSpace
);

NTSYSCALLAPI
NTSTATUS
NTAPI
RtlDecompressFragment (
	_In_ USHORT CompressionFormat,
	_Out_writes_bytes_to_ (UncompressedFragmentSize, *FinalUncompressedSize) PUCHAR UncompressedFragment,
	_In_ ULONG UncompressedFragmentSize,
	_In_reads_bytes_ (CompressedBufferSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_In_range_ (< , CompressedBufferSize) ULONG FragmentOffset,
	_Out_ PULONG FinalUncompressedSize,
	_In_ PVOID WorkSpace
);

// win81+
NTSYSCALLAPI
NTSTATUS
NTAPI
RtlDecompressFragmentEx (
	_In_ USHORT CompressionFormat,
	_Out_writes_bytes_to_ (UncompressedFragmentSize, *FinalUncompressedSize) PUCHAR UncompressedFragment,
	_In_ ULONG UncompressedFragmentSize,
	_In_reads_bytes_ (CompressedBufferSize) PUCHAR CompressedBuffer,
	_In_ ULONG CompressedBufferSize,
	_In_range_ (< , CompressedBufferSize) ULONG FragmentOffset,
	_In_ ULONG UncompressedChunkSize,
	_Out_ PULONG FinalUncompressedSize,
	_In_ PVOID WorkSpace
);

// extern c end
EXTERN_C_END
