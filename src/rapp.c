// routine.c
// project sdk library
//
// Copyright (c) 2012-2024 Henry++

#include "rapp.h"

//
// Application: seh
//

VOID _r_app_exceptionfilter_savedump (
	_In_ PEXCEPTION_POINTERS exception_ptr
)
{
	MINIDUMP_EXCEPTION_INFORMATION exception_info = {0};
	WCHAR path[512];
	HANDLE hfile;
	NTSTATUS status;

	_r_str_printf (
		path,
		RTL_NUMBER_OF (path),
		L"%s\\%s-%" TEXT (PR_LONG64) L".dmp",
		_r_app_getcrashdirectory (TRUE)->buffer,
		_r_app_getnameshort (),
		_r_unixtime_now ()
	);

	status = _r_fs_createfile (path, FILE_OVERWRITE_IF, GENERIC_WRITE, FILE_SHARE_WRITE, FILE_ATTRIBUTE_NORMAL, 0, FALSE, NULL, &hfile);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_r_fs_createfile", status, path);

		return;
	}

	exception_info.ThreadId = HandleToULong (NtCurrentThreadId ());
	exception_info.ExceptionPointers = exception_ptr;
	exception_info.ClientPointers = TRUE;

	MiniDumpWriteDump (NtCurrentProcess (), HandleToULong (NtCurrentProcessId ()), hfile, MiniDumpNormal, &exception_info, NULL, NULL);

	NtClose (hfile);
}

ULONG CALLBACK _r_app_exceptionfilter_callback (
	_In_ PEXCEPTION_POINTERS exception_ptr
)
{
	// Let the debugger handle the exception
	if (NtCurrentPeb ()->BeingDebugged)
		return EXCEPTION_CONTINUE_SEARCH;

	_r_app_exceptionfilter_savedump (exception_ptr);

	_r_report_error (APP_EXCEPTION_TITLE, NULL, exception_ptr->ExceptionRecord->ExceptionCode, ET_NATIVE);

	RtlExitUserProcess (exception_ptr->ExceptionRecord->ExceptionCode);

	return EXCEPTION_EXECUTE_HANDLER;
}

//
// Application: common
//

LPWSTR _r_app_getmutexname ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_name = NULL;

	if (_r_initonce_begin (&init_once))
	{
#if defined(APP_NO_MUTEX)
		cached_name = _r_format_string (
			L"%s-%" TEXT (PR_ULONG) L"-%" TEXT (PR_ULONG),
			_r_app_getnameshort (),
			_r_str_gethash (_r_sys_getimagepath (), TRUE),
			_r_str_gethash (_r_sys_getcommandline (), TRUE)
		);
#else
		cached_name = _r_obj_createstring (_r_app_getnameshort ());
#endif // APP_NO_MUTEX

		_r_initonce_end (&init_once);
	}

	return cached_name->buffer;
}

BOOLEAN _r_app_isportable ()
{
#if defined(APP_NO_APPDATA) || defined(APP_CONSOLE)
	return TRUE;
#else
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_portable = FALSE;

	LPCWSTR file_names[] = {L"portable", _r_app_getnameshort ()};
	LPCWSTR file_exts[] = {L"dat", L"ini"};
	PR_STRING directory;
	PR_STRING string;

	C_ASSERT (sizeof (file_names) == sizeof (file_exts));

	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_getopt (_r_sys_getcommandline (), L"portable", NULL))
		{
			is_portable = TRUE;
		}
		else
		{
			directory = _r_app_getdirectory ();

			for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (file_names); i++)
			{
				string = _r_obj_concatstrings (
					5,
					directory->buffer,
					L"\\",
					file_names[i],
					L".",
					file_exts[i]
				);

				if (_r_fs_exists (string->buffer))
					is_portable = TRUE;

				_r_obj_dereference (string);

				if (is_portable)
					break;
			}
		}

		_r_initonce_end (&init_once);
	}

	return is_portable;
#endif // !APP_NO_APPDATA || !APP_CONSOLE
}

BOOLEAN _r_app_initialize_com ()
{
	HRESULT status;

	// initialize COM library
	status = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED (status))
	{
		_r_report_error (NULL, APP_FAILED_COM_INITIALIZE, status, ET_WINDOWS);

		return FALSE;
	}

	return TRUE;
}

VOID _r_app_initialize_components ()
{
	// set locale information
	_r_obj_clearreference (&app_global.locale.default_name);

	app_global.locale.resource_name = _r_obj_createstring (APP_LANGUAGE_DEFAULT);

	_r_app_initialize_locale ();

	// register TaskbarCreated message
#if defined(APP_HAVE_TRAY)
	if (!app_global.main.taskbar_msg)
		app_global.main.taskbar_msg = RegisterWindowMessageW (L"TaskbarCreated");
#endif // APP_HAVE_TRAY

	// initialize objects
	app_global.update.info.components = _r_obj_createarray (sizeof (R_UPDATE_COMPONENT), NULL);

	app_global.settings.page_list = _r_obj_createarray (sizeof (R_SETTINGS_PAGE), NULL);
}

VOID _r_app_initialize_controls ()
{
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_WIN95_CLASSES;

	InitCommonControlsEx (&icex);
}

VOID _r_app_initialize_dll ()
{
	// Safe DLL loading
	// This prevents DLL planting in the application directory.
	SetDllDirectoryW (L"");

	// win7+
	SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

	// win7 has required KB2533623 or KB3063858
	SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
}

VOID _r_app_initialize_locale ()
{
	PR_STRING string;

	_r_obj_clearreference (&app_global.locale.default_name);

	string = _r_locale_getinfo (LOCALE_USER_DEFAULT, LOCALE_SENGLISHLANGUAGENAME);

	if (!string)
		string = _r_locale_getinfo (LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLISHLANGUAGENAME);

	app_global.locale.default_name = string;
}

VOID _r_app_initialize_mitigations ()
{
	PROCESS_MITIGATION_POLICY_INFORMATION policy = {0};
	NTSTATUS status;

	if (!_r_sys_isosversiongreaterorequal (WINDOWS_10))
		return;

	// the policy of a process that can restrict image loading to those images that are either signed by Microsoft,
	// by the Windows Store, or by Microsoft, the Windows Store and the WHQL
	policy.Policy = ProcessSignaturePolicy;
	policy.SignaturePolicy.Flags = 0;
	policy.SignaturePolicy.MicrosoftSignedOnly = TRUE;

	status = NtSetInformationProcess (NtCurrentProcess (), ProcessMitigationPolicy, &policy, sizeof (policy));

	//if (!NT_SUCCESS (status))
	//	_r_report_error (NULL, L"ProcessSignaturePolicy", status, ET_NATIVE);

	// prevents legacy extension point DLLs from being loaded into the process
	policy.Policy = ProcessExtensionPointDisablePolicy;
	policy.ExtensionPointDisablePolicy.Flags = 0;
	policy.ExtensionPointDisablePolicy.DisableExtensionPoints = TRUE;

	status = NtSetInformationProcess (NtCurrentProcess (), ProcessMitigationPolicy, &policy, sizeof (policy));

	//if (!NT_SUCCESS (status))
	//	_r_report_error (NULL, L"ProcessExtensionPointDisablePolicy", status, ET_NATIVE);

	// when turned on, the process cannot generate dynamic code or modify existing executable code
	policy.Policy = ProcessDynamicCodePolicy;
	policy.DynamicCodePolicy.Flags = 0;
	policy.DynamicCodePolicy.ProhibitDynamicCode = TRUE;

	status = NtSetInformationProcess (NtCurrentProcess (), ProcessMitigationPolicy, &policy, sizeof (policy));

	//if (!NT_SUCCESS (status))
	//	_r_report_error (NULL, L"ProcessDynamicCodePolicy", status, ET_NATIVE);

	// the RedirectionGuard policy of the process.
	//policy.Policy = ProcessRedirectionTrustPolicy;
	//policy.RedirectionTrustPolicy.Flags = 0;
	//policy.RedirectionTrustPolicy.EnforceRedirectionTrust = TRUE;
	//
	//status = NtSetInformationProcess (NtCurrentProcess (), ProcessMitigationPolicy, &policy, sizeof (policy));
	//
	//if (!NT_SUCCESS (status))
	//	_r_report_error (NULL, L"ProcessRedirectionTrustPolicy", status, ET_NATIVE);
}

VOID _r_app_initialize_seh ()
{
	ULONG error_mode = 0;
	NTSTATUS status;

	status = NtQueryInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG), NULL);

	if (NT_SUCCESS (status))
	{
		error_mode &= ~(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

		NtSetInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG));
	}

	RtlSetUnhandledExceptionFilter (&_r_app_exceptionfilter_callback);
}

BOOLEAN _r_app_initialize (
	_In_opt_ PR_CMDLINE_CALLBACK cmd_callback
)
{
	R_STRINGREF sr = {0};

	_r_app_initialize_mitigations ();

	// set main thread name (win10rs1+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		_r_sys_setthreadname (NtCurrentThread (), L"MainThread");

	// initialize controls
#if !defined(APP_CONSOLE)
	_r_app_initialize_controls ();
#endif // !APP_CONSOLE

	// initialize dll security
	_r_app_initialize_dll ();

	// set unhandled exception filter
#if !defined(_DEBUG)
	_r_app_initialize_seh ();
#endif // !_DEBUG

	_r_app_initialize_com ();

#if !defined(APP_CONSOLE)
	_r_app_initialize_components ();

#if defined(APP_NO_GUEST)
	// use "only admin"-mode
	if (!_r_sys_iselevated ())
	{
		if (!_r_app_runasadmin ())
			_r_report_error (NULL, APP_FAILED_ADMIN_RIGHTS, STATUS_ELEVATION_REQUIRED, ET_NATIVE);

		return FALSE;
	}

#elif defined(APP_HAVE_SKIPUAC)
	// use "skipuac" feature
	if (!_r_sys_iselevated () && _r_skipuac_run ())
		return FALSE;
#endif // APP_NO_GUEST
#endif // !APP_CONSOLE

	// check for wow64 working and show warning if it is TRUE!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 () && !_r_sys_getopt (_r_sys_getcommandline (), L"nowow64", NULL))
	{
		_r_report_error (APP_WARNING_WOW64_TITLE, APP_WARNING_WOW64_TEXT, STATUS_IMAGE_MACHINE_TYPE_MISMATCH_EXE, ET_NATIVE);

		return FALSE;
	}
#endif // !_DEBUG && !_WIN64

	// restart app on restart
	if (!_r_autorun_isenabled ())
		_r_sys_registerrestart (TRUE);

	AllowSetForegroundWindow (ASFW_ANY);

	// if profile directory does not exist, we cannot save configuration
	_r_app_getprofiledirectory ();

	if (cmd_callback)
	{
		if (_r_sys_getopt (_r_sys_getcommandline (), L"help", NULL))
		{
			if (cmd_callback (CmdlineHelp))
				return FALSE;
		}

		if (_r_sys_getopt (_r_sys_getcommandline (), L"clean", NULL))
		{
			if (cmd_callback (CmdlineClean))
				return FALSE;
		}

		if (_r_sys_getopt (_r_sys_getcommandline (), L"install", NULL))
		{
			if (cmd_callback (CmdlineInstall))
				return FALSE;
		}

		if (_r_sys_getopt (_r_sys_getcommandline (), L"uninstall", NULL))
		{
			if (cmd_callback (CmdlineUninstall))
				return FALSE;
		}
	}
	else
	{
		if (_r_sys_getopt (_r_sys_getcommandline (), L"help", NULL))
			return FALSE;

		if (_r_sys_getopt (_r_sys_getcommandline (), L"clean", NULL))
			return FALSE;

		if (_r_sys_getopt (_r_sys_getcommandline (), L"install", NULL))
			return FALSE;

		if (_r_sys_getopt (_r_sys_getcommandline (), L"uninstall", NULL))
			return FALSE;
	}

#if !defined(APP_CONSOLE)
	// prevent app duplicates
	if (_r_mutant_isexists (_r_app_getmutexname ()))
	{
		_r_obj_initializestringref (&sr, _r_app_getname ());

		EnumWindows (&_r_util_activate_window_callback, (LPARAM)&sr);

		return FALSE;
	}

	// set running flag
	_r_mutant_create (_r_app_getmutexname (), MUTANT_ALL_ACCESS, TRUE, &app_global.main.hmutex);
#endif // !APP_CONSOLE

	return TRUE;
}

PR_STRING _r_app_getdirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	R_STRINGREF sr;

	if (_r_initonce_begin (&init_once))
	{
		_r_obj_initializestringref (&sr, _r_sys_getimagepath ());

		cached_path = _r_path_getbasedirectory (&sr);

		_r_initonce_end (&init_once);
	}

	return cached_path;
}

PR_STRING _r_app_getconfigpath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	PR_STRING string;
	PR_STRING path;
	HRESULT status;

	if (_r_initonce_begin (&init_once))
	{
		// get configuration path
		if (_r_app_isportable ())
		{
			path = _r_app_getdirectory ();

			string = _r_obj_concatstrings (
				4,
				path->buffer,
				L"\\",
				_r_app_getnameshort (),
				L".ini"
			);

			cached_result = string;
		}
		else
		{
			status = _r_path_getknownfolder (&FOLDERID_RoamingAppData, 0, L"\\" APP_AUTHOR L"\\" APP_NAME L"\\" APP_NAME_SHORT L".ini", &string);

			if (SUCCEEDED (status))
				cached_result = string;
		}

		_r_initonce_end (&init_once);
	}

	return cached_result;
}

LPCWSTR _r_app_getcachedirectory (
	_In_ BOOLEAN is_create
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR cached_path[512] = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_str_printf (cached_path, RTL_NUMBER_OF (cached_path), L"%s\\cache", _r_app_getprofiledirectory ()->buffer);

		_r_initonce_end (&init_once);
	}

	if (is_create && !_r_fs_exists (cached_path))
		_r_fs_createdirectory (cached_path, FILE_ATTRIBUTE_NORMAL);

	return cached_path;
}

PR_STRING _r_app_getcrashdirectory (
	_In_ BOOLEAN is_create
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	HRESULT status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_path_getknownfolder (&FOLDERID_Desktop, 0, L"\\" APP_NAME_SHORT L"_crash", &cached_path);

		if (FAILED (status))
			cached_path = _r_format_string (L"%s\\" APP_NAME_SHORT L"_crash", _r_app_getprofiledirectory ()->buffer);

		_r_initonce_end (&init_once);
	}

	if (is_create && !_r_fs_exists (cached_path->buffer))
		_r_fs_createdirectory (cached_path->buffer, FILE_ATTRIBUTE_NORMAL);

	return cached_path;
}

PR_STRING _r_app_getlocalepath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_path = _r_format_string (L"%s\\%s.lng", _r_app_getdirectory ()->buffer, _r_app_getnameshort ());

		_r_initonce_end (&init_once);
	}

	return cached_path;
}

PR_STRING _r_app_getlogpath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_path = _r_obj_concatstrings (
			4,
			_r_app_getprofiledirectory ()->buffer,
			L"\\",
			_r_app_getnameshort (),
			L"_debug.log"
		);

		_r_initonce_end (&init_once);
	}

	return cached_path;
}

PR_STRING _r_app_getprofiledirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_app_isportable ())
		{
			cached_path = _r_app_getdirectory ();
		}
		else
		{
			_r_path_getknownfolder (&FOLDERID_RoamingAppData, 0, L"\\" APP_AUTHOR L"\\" APP_NAME, &cached_path);
		}

		_r_initonce_end (&init_once);
	}

	if (cached_path && !_r_fs_exists (cached_path->buffer))
		_r_fs_createdirectory (cached_path->buffer, FILE_ATTRIBUTE_NORMAL);

	return cached_path;
}

_Ret_maybenull_
PR_STRING _r_app_getproxyconfiguration ()
{
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxy = {0};
	PR_STRING proxy_string;

	proxy_string = _r_config_getstring (L"Proxy", NULL);

	if (_r_obj_isstringempty (proxy_string))
	{
		if (WinHttpGetIEProxyConfigForCurrentUser (&proxy))
		{
			if (!_r_str_isempty (proxy.lpszProxy))
				proxy_string = _r_obj_createstring (proxy.lpszProxy);

			SAFE_DELETE_GLOBAL (proxy.lpszProxy);
			SAFE_DELETE_GLOBAL (proxy.lpszProxyBypass);
			SAFE_DELETE_GLOBAL (proxy.lpszAutoConfigUrl);
		}
	}

	return proxy_string;
}

PR_STRING _r_app_getuseragent ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_agent = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_agent = _r_config_getstring (L"UserAgent", NULL);

		if (_r_obj_isstringempty (cached_agent))
		{
			cached_agent = _r_obj_concatstrings (
				6,
				_r_app_getname (),
				L"/",
				_r_app_getversion (),
				L" (+",
				_r_app_getwebsite_url (),
				L")"
			);
		}

		_r_initonce_end (&init_once);
	}

	return cached_agent;
}

LRESULT CALLBACK _r_app_maindlgproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	if (!app_global.main.wnd_proc)
		return FALSE;

	if (app_global.main.taskbar_msg && msg == app_global.main.taskbar_msg)
		return CallWindowProcW (app_global.main.wnd_proc, hwnd, RM_TASKBARCREATED, 0, 0);

	switch (msg)
	{
		case RM_LOCALIZE:
		{
			CallWindowProcW (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);

			RedrawWindow (hwnd, NULL, NULL, RDW_ERASENOW | RDW_INVALIDATE);

			DrawMenuBar (hwnd); // HACK!!!

			break;
		}

		//case WM_NCDESTROY:
		//{
		//	_r_wnd_sendmessage (hwnd, 0,RM_UNINITIALIZE_POST, 0, 0);
		//	break;
		//}

		case WM_DESTROY:
		{
			_r_window_saveposition (hwnd, L"window");
			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, TRUE); // required!

			return TRUE;
		}

		case WM_SIZE:
		{
#if defined(APP_HAVE_TRAY)
			if (wparam == SIZE_MINIMIZED)
			{
				if (_r_config_getboolean (L"IsMinimizeToTray", TRUE))
					ShowWindow (hwnd, SW_HIDE);
			}
#endif // APP_HAVE_TRAY

			break;
		}

		case WM_EXITSIZEMOVE:
		{
			_r_window_saveposition (hwnd, L"window");

			InvalidateRect (hwnd, NULL, TRUE); // HACK!!!

			break;
		}

#if defined(APP_HAVE_TRAY)
		case WM_SYSCOMMAND:
		{
			if (wparam == SC_CLOSE)
			{
				if (_r_config_getboolean (L"IsCloseToTray", TRUE))
				{
					ShowWindow (hwnd, SW_HIDE);

					return TRUE;
				}
			}

			break;
		}
#endif // APP_HAVE_TRAY

		case WM_SHOWWINDOW:
		{
			if (wparam)
			{
				if (app_global.main.is_needmaximize)
				{
					ShowWindow (hwnd, SW_SHOWMAXIMIZED);

					app_global.main.is_needmaximize = FALSE;
				}
			}

			break;
		}

		case WM_DPICHANGED:
		{
			_r_wnd_message_dpichanged (hwnd, wparam, lparam);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_message_settingchange (hwnd, wparam, lparam);
			break;
		}
	}

	return CallWindowProcW (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);
}

ULONG _r_app_getshowcode (
	_In_ HWND hwnd
)
{
	ULONG show_code;
	BOOLEAN is_windowhidden = FALSE;

	show_code = (NtCurrentPeb ()->ProcessParameters->WindowFlags & STARTF_USESHOWWINDOW) ? NtCurrentPeb ()->ProcessParameters->ShowWindowFlags : SW_SHOWNORMAL;

	if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
		is_windowhidden = TRUE;

	// if window have tray - check arguments
#if defined(APP_HAVE_TRAY)
	if (!is_windowhidden)
	{
		if (_r_config_getboolean (L"IsStartMinimized", FALSE) || _r_sys_getopt (_r_sys_getcommandline (), L"minimized", NULL))
			is_windowhidden = TRUE;
	}
#endif // APP_HAVE_TRAY

	if (_r_wnd_getstyle (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX)
	{
		if (show_code == SW_SHOWMAXIMIZED || _r_config_getboolean_ex (L"IsMaximized", FALSE, L"window"))
		{
			if (is_windowhidden)
			{
				app_global.main.is_needmaximize = TRUE;
			}
			else
			{
				show_code = SW_SHOWMAXIMIZED;
			}
		}
	}

#if defined(APP_HAVE_TRAY)
	if (is_windowhidden)
		show_code = SW_HIDE;
#endif // APP_HAVE_TRAY

	return show_code;
}

_Ret_maybenull_
HWND _r_app_createwindow (
	_In_ PVOID hinst,
	_In_ LPCWSTR dlg_name,
	_In_opt_ LPWSTR icon_name,
	_In_ DLGPROC dlg_proc
)
{
	UINT messages[] = {
		WM_COPYDATA,
		WM_COPYGLOBALDATA,
		WM_DROPFILES,
		app_global.main.taskbar_msg,
	};

	WCHAR locale_version[64];
	HWND hwnd;
	LONG dpi_value;
	LONG icon_small;
	LONG icon_large;

	// configure components
	_r_str_fromlong64 (locale_version, RTL_NUMBER_OF (locale_version), _r_locale_getversion ());

	_r_update_addcomponent (_r_app_getname (), _r_app_getnameshort (), _r_app_getversion (), _r_app_getdirectory (), TRUE);
	_r_update_addcomponent (L"Language pack", L"language", locale_version, _r_app_getlocalepath (), FALSE);

	// create main window
	hwnd = _r_wnd_createwindow (hinst, dlg_name, NULL, dlg_proc, NULL);

	_r_app_sethwnd (hwnd);

	if (!hwnd)
		return NULL;

	// set window title
	_r_ctrl_setstring (hwnd, 0, _r_app_getname ());

	// set window prop
	SetPropW (hwnd, _r_app_getname (), IntToPtr (42));

	// set window on top
	_r_wnd_top (hwnd, _r_config_getboolean (L"AlwaysOnTop", FALSE));

	// center window position
	_r_wnd_center (hwnd, NULL);

	// enable dark theme support
	_r_theme_initialize (hwnd, _r_theme_isenabled ());

	// enable messages bypass uipi (win7+)
	_r_wnd_changemessagefilter (hwnd, messages, RTL_NUMBER_OF (messages), MSGFLT_ALLOW);

	// subclass window
	app_global.main.wnd_proc = _r_wnd_setsubclass (hwnd, DWLP_DLGPROC, &_r_app_maindlgproc);

	// restore window position
	_r_window_restoreposition (hwnd, L"window");

	// set window icon
	if (icon_name)
	{
		dpi_value = _r_dc_getwindowdpi (hwnd);

		icon_small = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
		icon_large = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);

		_r_wnd_seticon (
			hwnd,
			_r_sys_loadsharedicon (hinst, icon_name, icon_small),
			_r_sys_loadsharedicon (hinst, icon_name, icon_large)
		);
	}

	// common initialization
	_r_wnd_sendmessage (hwnd, 0, RM_INITIALIZE, 0, 0);
	_r_wnd_sendmessage (hwnd, 0, RM_LOCALIZE, 0, 0);

	// restore window visibility (or not?)
#if !defined(APP_STARTMINIMIZED)
	ShowWindow (hwnd, _r_app_getshowcode (hwnd));
#endif // !APP_STARTMINIMIZED

	_r_wnd_sendmessage (hwnd, 0, RM_INITIALIZE_POST, 0, 0);

#if defined(APP_HAVE_UPDATES)
	if (_r_update_isenabled (TRUE))
		_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

	return hwnd;
}

BOOLEAN _r_app_runasadmin ()
{
	_r_mutant_destroy (&app_global.main.hmutex);

#if defined(APP_HAVE_SKIPUAC)
	if (_r_skipuac_run ())
		return TRUE;
#endif // APP_HAVE_SKIPUAC

	if (_r_sys_runasadmin (_r_sys_getimagepath (), _r_sys_getcommandline (), _r_sys_getcurrentdirectory ()))
		return TRUE;

	// restore mutex on error
	_r_mutant_create (_r_app_getmutexname (), MUTANT_ALL_ACCESS, TRUE, &app_global.main.hmutex);

	_r_sys_sleep (500); // HACK!!! prevent loop

	return FALSE;
}

VOID _r_app_restart (
	_In_opt_ HWND hwnd
)
{
	HWND hmain;
	NTSTATUS status;

	if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, APP_QUESTION_RESTART) != IDYES)
		return;

	_r_mutant_destroy (&app_global.main.hmutex);

	status = _r_sys_createprocess (_r_sys_getimagepath (), _r_sys_getcommandline (), _r_sys_getcurrentdirectory (), FALSE);

	if (status != STATUS_SUCCESS)
	{
		// restore mutex on error
		_r_mutant_create (_r_app_getmutexname (), MUTANT_ALL_ACCESS, TRUE, &app_global.main.hmutex);

		return;
	}

	hmain = _r_app_gethwnd ();

	if (hmain)
	{
		DestroyWindow (hmain);

		_r_sys_waitforsingleobject (hmain, 4000); // wait for exit
	}

	RtlExitUserProcess (STATUS_SUCCESS);
}

//
// Configuration
//

VOID _r_config_initialize ()
{
	PR_HASHTABLE config_table;
	PR_STRING path;

	path = _r_app_getconfigpath ();

	_r_parseini (path, &config_table, NULL);

	_r_queuedlock_acquireexclusive (&app_global.config.lock);
	_r_obj_movereference (&app_global.config.table, config_table);
	_r_queuedlock_releaseexclusive (&app_global.config.lock);
}

BOOLEAN _r_config_getboolean (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN def_value
)
{
	BOOLEAN value;

	value = _r_config_getboolean_ex (key_name, def_value, NULL);

	return value;
}

BOOLEAN _r_config_getboolean_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ BOOLEAN def_value,
	_In_opt_ LPCWSTR section_name
)
{
	PR_STRING string;
	BOOLEAN result;

	string = _r_config_getstring_ex (key_name, def_value ? L"true" : L"false", section_name);

	if (!string)
		return FALSE;

	result = _r_str_toboolean (&string->sr);

	_r_obj_dereference (string);

	return result;
}

LONG _r_config_getlong (
	_In_ LPCWSTR key_name,
	_In_ LONG def_value
)
{
	LONG value;

	value = _r_config_getlong_ex (key_name, def_value, NULL);

	return value;
}

LONG _r_config_getlong_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LONG def_value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR number_string[64];
	PR_STRING string;
	LONG value;

	_r_str_fromlong (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstring_ex (key_name, number_string, section_name);

	if (!string)
		return 0;

	value = _r_str_tolong (&string->sr);

	_r_obj_dereference (string);

	return value;
}

LONG64 _r_config_getlong64 (
	_In_ LPCWSTR key_name,
	_In_ LONG64 def_value
)
{
	LONG64 value;

	value = _r_config_getlong64_ex (key_name, def_value, NULL);

	return value;
}

LONG64 _r_config_getlong64_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LONG64 def_value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR number_string[64];
	PR_STRING string;
	LONG64 value;

	_r_str_fromlong64 (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstring_ex (key_name, number_string, section_name);

	if (!string)
		return 0;

	value = _r_str_tolong64 (&string->sr);

	_r_obj_dereference (string);

	return value;
}

ULONG _r_config_getulong (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG def_value
)
{
	ULONG value;

	value = _r_config_getulong_ex (key_name, def_value, NULL);

	return value;
}

ULONG _r_config_getulong_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG def_value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR number_string[64];
	PR_STRING string;
	ULONG result;

	_r_str_fromulong (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstring_ex (key_name, number_string, section_name);

	if (!string)
		return 0;

	result = _r_str_toulong (&string->sr);

	_r_obj_dereference (string);

	return result;
}

ULONG64 _r_config_getulong64 (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG64 def_value
)
{
	ULONG64 value;

	value = _r_config_getulong64_ex (key_name, def_value, NULL);

	return value;
}

ULONG64 _r_config_getulong64_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ ULONG64 def_value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR number_string[64];
	PR_STRING string;
	ULONG64 value;

	_r_str_fromulong64 (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstring_ex (key_name, number_string, section_name);

	if (!string)
		return 0;

	value = _r_str_toulong64 (&string->sr);

	_r_obj_dereference (string);

	return value;
}

VOID _r_config_getfont (
	_In_ LPCWSTR key_name,
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value
)
{
	_r_config_getfont_ex (key_name, logfont, dpi_value, NULL);
}

VOID _r_config_getfont_ex (
	_In_ LPCWSTR key_name,
	_Inout_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_opt_ LPCWSTR section_name
)
{
	PR_STRING font_config;
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	LONG font_size;

	font_config = _r_config_getstring_ex (key_name, NULL, section_name);

	if (font_config)
	{
		_r_obj_initializestringref2 (&remaining_part, &font_config->sr);

		// get font face name
		if (_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part))
			_r_str_copystring (logfont->lfFaceName, RTL_NUMBER_OF (logfont->lfFaceName), &first_part);

		// get font size
		_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part);

		font_size = _r_str_tolong (&first_part);

		if (font_size)
			logfont->lfHeight = _r_dc_fontsizetoheight (font_size, dpi_value);

		// get font weight
		_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part);

		logfont->lfWeight = _r_str_tolong (&first_part); // weight

		_r_obj_dereference (font_config);
	}

	logfont->lfCharSet = DEFAULT_CHARSET;
	logfont->lfQuality = DEFAULT_QUALITY;
	logfont->lfPitchAndFamily = DEFAULT_PITCH;

	// fill missed font values
	_r_dc_getdefaultfont (logfont, dpi_value, FALSE);
}

VOID _r_config_getsize (
	_In_ LPCWSTR key_name,
	_Out_ PR_SIZE size,
	_In_opt_ PR_SIZE def_value,
	_In_opt_ LPCWSTR section_name
)
{
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING pair_config;

	size->cx = size->cy = 0; // initialize size values

	pair_config = _r_config_getstring_ex (key_name, NULL, section_name);

	if (pair_config)
	{
		_r_obj_initializestringref2 (&remaining_part, &pair_config->sr);

		// get x-value
		_r_str_splitatchar (&remaining_part, L',', &first_part, &remaining_part);

		size->cx = _r_str_tolong (&first_part);

		// get y-value
		_r_str_splitatchar (&remaining_part, L',', &first_part, &remaining_part);

		size->cy = _r_str_tolong (&first_part);

		_r_obj_dereference (pair_config);
	}

	if (def_value)
	{
		if (!size->cx)
			size->cx = def_value->cx;

		if (!size->cy)
			size->cy = def_value->cy;
	}
}

_Ret_maybenull_
PR_STRING _r_config_getstringexpand (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value
)
{
	PR_STRING value;

	value = _r_config_getstringexpand_ex (key_name, def_value, NULL);

	return value;
}

_Ret_maybenull_
PR_STRING _r_config_getstringexpand_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value,
	_In_opt_ LPCWSTR section_name
)
{
	PR_STRING string;
	PR_STRING value;
	NTSTATUS status;

	value = _r_config_getstring_ex (key_name, def_value, section_name);

	if (!value)
		return NULL;

	status = _r_str_environmentexpandstring (NULL, &value->sr, &string);

	if (!NT_SUCCESS (status))
		string = _r_obj_createstring2 (&value->sr);

	_r_obj_dereference (value);

	return string;
}

_Ret_maybenull_
PR_STRING _r_config_getstring (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value
)
{
	PR_STRING value;

	value = _r_config_getstring_ex (key_name, def_value, NULL);

	return value;
};

_Ret_maybenull_
PR_STRING _r_config_getstring_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR def_value,
	_In_opt_ LPCWSTR section_name
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;

	PR_OBJECT_POINTER object_ptr;
	PR_STRING section_string;
	ULONG hash_code;

	if (_r_initonce_begin (&init_once))
	{
		if (!app_global.config.table)
			_r_config_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.config.table)
		return NULL;

	if (section_name)
	{
		section_string = _r_obj_concatstrings (
			5,
			_r_app_getnameshort (),
			L"\\",
			section_name,
			L"\\",
			key_name
		);
	}
	else
	{
		section_string = _r_obj_concatstrings (
			3,
			_r_app_getnameshort (),
			L"\\",
			key_name
		);
	}

	hash_code = _r_str_gethash2 (&section_string->sr, TRUE);

	_r_obj_dereference (section_string);

	if (!hash_code)
		return NULL;

	_r_queuedlock_acquireshared (&app_global.config.lock);
	object_ptr = _r_obj_findhashtable (app_global.config.table, hash_code);
	_r_queuedlock_releaseshared (&app_global.config.lock);

	if (!object_ptr)
	{
		_r_queuedlock_acquireexclusive (&app_global.config.lock);
		object_ptr = _r_obj_addhashtablepointer (app_global.config.table, hash_code, NULL);
		_r_queuedlock_releaseexclusive (&app_global.config.lock);
	}

	if (!object_ptr)
		return NULL;

	if (!object_ptr->object_body)
	{
		if (!_r_str_isempty (def_value))
			_r_obj_movereference (&object_ptr->object_body, _r_obj_createstring (def_value));
	}

	return _r_obj_referencesafe (object_ptr->object_body);
}

VOID _r_config_setboolean (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN value
)
{
	_r_config_setstring_ex (key_name, value ? L"true" : L"false", NULL);
}

VOID _r_config_setboolean_ex (
	_In_ LPCWSTR key_name,
	_In_ BOOLEAN value,
	_In_opt_ LPCWSTR section_name
)
{
	_r_config_setstring_ex (key_name, value ? L"true" : L"false", section_name);
}

VOID _r_config_setlong (
	_In_ LPCWSTR key_name,
	_In_ LONG value
)
{
	_r_config_setlong_ex (key_name, value, NULL);
}

VOID _r_config_setlong_ex (
	_In_ LPCWSTR key_name,
	_In_ LONG value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[64];

	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setlong64 (
	_In_ LPCWSTR key_name,
	_In_ LONG64 value
)
{
	_r_config_setlong64_ex (key_name, value, NULL);
}

VOID _r_config_setlong64_ex (
	_In_ LPCWSTR key_name,
	_In_ LONG64 value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[64];

	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setulong (
	_In_ LPCWSTR key_name,
	_In_ ULONG value
)
{
	_r_config_setulong_ex (key_name, value, NULL);
}

VOID _r_config_setulong_ex (
	_In_ LPCWSTR key_name,
	_In_ ULONG value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[64];

	_r_str_fromulong (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setulong64 (
	_In_ LPCWSTR key_name,
	_In_ ULONG64 value
)
{
	_r_config_setulong64_ex (key_name, value, NULL);
}

VOID _r_config_setulong64_ex (
	_In_ LPCWSTR key_name,
	_In_ ULONG64 value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[64];

	_r_str_fromulong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setfont (
	_In_ LPCWSTR key_name,
	_In_ PLOGFONT logfont,
	_In_ LONG dpi_value
)
{
	_r_config_setfont_ex (key_name, logfont, dpi_value, NULL);
}

VOID _r_config_setfont_ex (
	_In_ LPCWSTR key_name,
	_In_ PLOGFONT logfont,
	_In_ LONG dpi_value,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[128];

	_r_str_printf (
		value_text,
		RTL_NUMBER_OF (value_text),
		L"%s;%" TEXT (PR_LONG) L";%" TEXT (PR_LONG),
		logfont->lfFaceName,
		logfont->lfHeight ? _r_dc_fontheighttosize (logfont->lfHeight, dpi_value) : 0,
		logfont->lfWeight
	);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setsize (
	_In_ LPCWSTR key_name,
	_In_ PR_SIZE size,
	_In_opt_ LPCWSTR section_name
)
{
	WCHAR value_text[128];

	_r_str_printf (
		value_text,
		RTL_NUMBER_OF (value_text),
		L"%" TEXT (PR_LONG) L",%" TEXT (PR_LONG),
		size->cx,
		size->cy
	);

	_r_config_setstring_ex (key_name, value_text, section_name);
}

VOID _r_config_setstringexpand (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value
)
{
	_r_config_setstringexpand_ex (key_name, value, NULL);
}

VOID _r_config_setstringexpand_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value,
	_In_opt_ LPCWSTR section_name
)
{
	PR_STRING string;

	string = value ? _r_str_environmentunexpandstring (value) : _r_obj_referenceemptystring ();

	_r_config_setstring_ex (key_name, _r_obj_getstring (string), section_name);

	if (string)
		_r_obj_dereference (string);
}

VOID _r_config_setstring (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value
)
{
	_r_config_setstring_ex (key_name, value, NULL);
}

VOID _r_config_setstring_ex (
	_In_ LPCWSTR key_name,
	_In_opt_ LPCWSTR value,
	_In_opt_ LPCWSTR section_name
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;

	PR_OBJECT_POINTER object_ptr;
	WCHAR section_string[128];
	PR_STRING section_string_full;
	PR_STRING path;
	ULONG hash_code;

	if (_r_initonce_begin (&init_once))
	{
		if (!app_global.config.table)
			_r_config_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.config.table)
		return;

	if (section_name)
	{
		_r_str_printf (section_string, RTL_NUMBER_OF (section_string), L"%s\\%s", _r_app_getnameshort (), section_name);

		section_string_full = _r_obj_concatstrings (
			5,
			_r_app_getnameshort (),
			L"\\",
			section_name,
			L"\\",
			key_name
		);
	}
	else
	{
		_r_str_copy (section_string, RTL_NUMBER_OF (section_string), _r_app_getnameshort ());

		section_string_full = _r_obj_concatstrings (
			3,
			_r_app_getnameshort (),
			L"\\",
			key_name
		);
	}

	hash_code = _r_str_gethash2 (&section_string_full->sr, TRUE);

	_r_obj_dereference (section_string_full);

	if (!hash_code)
		return;

	_r_queuedlock_acquireshared (&app_global.config.lock);
	object_ptr = _r_obj_findhashtable (app_global.config.table, hash_code);
	_r_queuedlock_releaseshared (&app_global.config.lock);

	if (!object_ptr)
	{
		_r_queuedlock_acquireexclusive (&app_global.config.lock);
		object_ptr = _r_obj_addhashtablepointer (app_global.config.table, hash_code, NULL);
		_r_queuedlock_releaseexclusive (&app_global.config.lock);
	}

	if (!object_ptr)
		return;

	if (value)
	{
		_r_obj_movereference (&object_ptr->object_body, _r_obj_createstring (value));
	}
	else
	{
		_r_obj_clearreference (&object_ptr->object_body);
	}

	path = _r_app_getconfigpath ();

	WritePrivateProfileStringW (section_string, key_name, _r_obj_getstring (object_ptr->object_body), path->buffer);
}

//
// Localization
//

VOID _r_locale_initialize ()
{
	PR_HASHTABLE locale_table;
	PR_STRING language_config;
	PR_STRING language_path;
	PR_LIST locale_names;

	language_config = _r_config_getstring (L"Language", NULL);

	if (!language_config)
	{
		_r_obj_swapreference (&app_global.locale.current_name, app_global.locale.default_name);
	}
	else
	{
		if (_r_str_isstartswith (&language_config->sr, &app_global.locale.resource_name->sr, TRUE))
		{
			_r_obj_swapreference (&app_global.locale.current_name, app_global.locale.resource_name);
		}
		else
		{
			_r_obj_swapreference (&app_global.locale.current_name, language_config);
		}

		_r_obj_dereference (language_config);
	}

	language_path = _r_app_getlocalepath ();

	locale_names = _r_obj_createlist (&_r_obj_dereference);

	_r_parseini (language_path, &locale_table, locale_names);

	_r_queuedlock_acquireexclusive (&app_global.locale.lock);

	_r_obj_movereference (&app_global.locale.available_list, locale_names);
	_r_obj_movereference (&app_global.locale.table, locale_table);

	_r_queuedlock_releaseexclusive (&app_global.locale.lock);
}

VOID _r_locale_apply (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
)
{
	PR_STRING locale_name;
	ULONG_PTR locale_index;
	HWND hwindow;
	INT item_id;
	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		locale_index = (menu_id == ctrl_id) ? SIZE_MAX : (UINT_PTR)ctrl_id - (UINT_PTR)menu_id - 2;
	}
	else
	{
		item_id = _r_combobox_getcurrentitem (hwnd, ctrl_id);

		if (item_id == CB_ERR)
			return;

		locale_index = _r_combobox_getitemlparam (hwnd, ctrl_id, item_id);
	}

	if (locale_index == SIZE_MAX)
	{
		_r_obj_swapreference (&app_global.locale.current_name, app_global.locale.resource_name);
	}
	else
	{
		locale_name = _r_obj_getlistitem (app_global.locale.available_list, locale_index);

		_r_obj_swapreference (&app_global.locale.current_name, locale_name);
	}

	_r_config_setstring (L"Language", _r_obj_getstring (app_global.locale.current_name));

	// refresh main window
	hwindow = _r_app_gethwnd ();

	if (hwindow)
		_r_wnd_sendmessage (hwindow, 0, RM_LOCALIZE, 0, 0);

	// refresh settings window
	hwindow = _r_settings_getwindow ();

	if (hwindow)
	{
		_r_wnd_sendmessage (hwindow, 0, RM_LOCALIZE, 0, 0);

		_r_theme_initialize (hwindow, _r_theme_isenabled ()); // HACK!
	}
}

VOID _r_locale_enum (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
)
{
	PR_STRING locale_name;
	HMENU hsubmenu = NULL;
	ULONG_PTR locale_count;
	UINT index;
	UINT menu_index;
	BOOLEAN is_current;
	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		hsubmenu = GetSubMenu (hwnd, ctrl_id);

		// clear menu
		_r_menu_clearitems (hsubmenu);

		_r_menu_additem (hsubmenu, menu_id, _r_obj_getstringorempty (app_global.locale.resource_name));

		_r_menu_checkitem (hsubmenu, menu_id, menu_id, MF_BYCOMMAND, menu_id);

		_r_menu_enableitem (hwnd, ctrl_id, FALSE, FALSE);
	}
	else
	{
		_r_combobox_clear (hwnd, ctrl_id);

		_r_combobox_insertitem (hwnd, ctrl_id, 0, _r_obj_getstringorempty (app_global.locale.resource_name), (LPARAM)SIZE_MAX);

		_r_combobox_setcurrentitem (hwnd, ctrl_id, 0);

		_r_ctrl_enable (hwnd, ctrl_id, FALSE);
	}

	locale_count = _r_locale_getcount ();

	if (!locale_count)
		return;

	if (is_menu)
	{
		_r_menu_enableitem (hwnd, ctrl_id, FALSE, TRUE);

		_r_menu_additem (hsubmenu, 0, NULL);
	}
	else
	{
		_r_ctrl_enable (hwnd, ctrl_id, TRUE);
	}

	index = 1;

	_r_queuedlock_acquireshared (&app_global.locale.lock);

	for (ULONG_PTR i = 0; i < locale_count; i++)
	{
		locale_name = _r_obj_getlistitem (app_global.locale.available_list, i);

		if (!locale_name)
			continue;

		is_current = (!_r_obj_isstringempty (app_global.locale.current_name) && (_r_str_isequal (&app_global.locale.current_name->sr, &locale_name->sr, TRUE)));

		if (is_menu)
		{
			menu_index = menu_id + (INT)(INT_PTR)i + 2;

			_r_menu_additem (hsubmenu, menu_index, locale_name->buffer);

			if (is_current)
				_r_menu_checkitem (hsubmenu, menu_id, menu_id + (INT)(INT_PTR)locale_count + 1, MF_BYCOMMAND, menu_index);
		}
		else
		{
			_r_combobox_insertitem (hwnd, ctrl_id, index, locale_name->buffer, i);

			if (is_current)
				_r_combobox_setcurrentitem (hwnd, ctrl_id, index);
		}

		index += 1;
	}

	_r_queuedlock_releaseshared (&app_global.locale.lock);
}

ULONG_PTR _r_locale_getcount ()
{
	ULONG_PTR count = 0;

	if (!app_global.locale.available_list)
		_r_locale_initialize ();

	if (app_global.locale.available_list)
	{
		_r_queuedlock_acquireshared (&app_global.locale.lock);
		count = _r_obj_getlistsize (app_global.locale.available_list);
		_r_queuedlock_releaseshared (&app_global.locale.lock);
	}

	return count;
}

_Ret_maybenull_
PR_STRING _r_locale_getstring_ex (
	_In_ ULONG uid
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;

	PR_STRING hash_string = NULL;
	PR_STRING value_string;
	ULONG hash_code;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_obj_isempty (app_global.locale.table))
			_r_locale_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.locale.table)
		return NULL;

	if (app_global.locale.current_name)
	{
		hash_string = _r_format_string (L"%s\\%03d", app_global.locale.current_name->buffer, uid);

		hash_code = _r_str_gethash2 (&hash_string->sr, TRUE);

		_r_queuedlock_acquireshared (&app_global.locale.lock);
		value_string = _r_obj_findhashtablepointer (app_global.locale.table, hash_code);
		_r_queuedlock_releaseshared (&app_global.locale.lock);
	}
	else
	{
		value_string = NULL;
	}

	if (!value_string)
	{
		if (app_global.locale.resource_name)
		{
			hash_string = _r_format_string (L"%s\\%03d", app_global.locale.resource_name->buffer, uid);

			hash_code = _r_str_gethash2 (&hash_string->sr, TRUE);

			_r_queuedlock_acquireshared (&app_global.locale.lock);
			value_string = _r_obj_findhashtablepointer (app_global.locale.table, hash_code);
			_r_queuedlock_releaseshared (&app_global.locale.lock);

			if (!value_string)
			{
				status = _r_res_loadstring (_r_sys_getimagebase (), uid, &value_string);

				if (NT_SUCCESS (status))
				{
					_r_queuedlock_acquireexclusive (&app_global.locale.lock);
					_r_obj_addhashtablepointer (app_global.locale.table, hash_code, _r_obj_reference (value_string));
					_r_queuedlock_releaseexclusive (&app_global.locale.lock);
				}
			}
		}
	}

	if (hash_string)
		_r_obj_dereference (hash_string);

	return value_string;
}

// TODO: in theory this is not good, so redesign it in future.
LPWSTR _r_locale_getstring (
	_In_ ULONG uid
)
{
	PR_STRING string;
	LPWSTR result;

	string = _r_locale_getstring_ex (uid);

	if (!string)
		return NULL;

	result = string->buffer;

	_r_obj_dereference (string);

	return result;
}

LONG64 _r_locale_getversion ()
{
	WCHAR timestamp_string[64];
	R_STRINGREF string;
	PR_STRING path;
	ULONG length;

	path = _r_app_getlocalepath ();

	// HACK!!! Use "Russian" section and default timestamp key (000) for compatibility with old releases...
	length = GetPrivateProfileStringW (L"Russian", L"000", NULL, timestamp_string, RTL_NUMBER_OF (timestamp_string), path->buffer);

	if (!length)
		return 0;

	_r_obj_initializestringref_ex (&string, timestamp_string, length * sizeof (WCHAR));

	return _r_str_tolong64 (&string);
}

//
// Autorun (optional feature)
//

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_autorun_enable (
	_In_opt_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	PR_STRING string;
	HANDLE hkey;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);

	if (!NT_SUCCESS (status))
		return status;

	if (is_enable)
	{
		string = _r_obj_concatstrings (
			3,
			L"\"",
			_r_sys_getimagepath (),
			L"\" -minimized"
		);

		status = _r_reg_setvalue (hkey, _r_app_getname (), REG_SZ, string->buffer, (ULONG)(string->length + sizeof (UNICODE_NULL)));

		_r_sys_registerrestart (FALSE);

		_r_obj_dereference (string);
	}
	else
	{
		status = _r_reg_deletevalue (hkey, _r_app_getname ());

		if (status == STATUS_OBJECT_NAME_NOT_FOUND)
			status = STATUS_SUCCESS;

		_r_sys_registerrestart (TRUE);
	}

	NtClose (hkey);

	if (hwnd)
	{
		if (!NT_SUCCESS (status))
			_r_show_errormessage (hwnd, NULL, status, NULL, ET_NATIVE);
	}

	return status;
}

BOOLEAN _r_autorun_isenabled ()
{
	R_STRINGREF sr1;
	R_STRINGREF sr2;
	PR_STRING path;
	HANDLE hkey;
	BOOLEAN is_enabled = FALSE;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey);

	if (NT_SUCCESS (status))
	{
		status = _r_reg_querystring (hkey, _r_app_getname (), &path);

		if (NT_SUCCESS (status))
		{
			_r_path_parsecommandlinefuzzy (&path->sr, &sr1, &sr2, NULL);

			_r_obj_initializestringref3 (&sr2, &NtCurrentPeb ()->ProcessParameters->ImagePathName);

			is_enabled = _r_str_isequal (&sr1, &sr2, TRUE);

			_r_obj_dereference (path);
		}

		NtClose (hkey);
	}

	return is_enabled;
}

//
// Update checker (optional feature)
//

BOOLEAN NTAPI _r_update_downloadcallback (
	_In_ ULONG total_written,
	_In_ ULONG total_length,
	_In_ PVOID lparam
)
{
	PR_UPDATE_INFO update_info;

	update_info = lparam;

	if (update_info->htaskdlg)
	{
		_r_wnd_sendmessage (update_info->htaskdlg, 0, TDM_SET_PROGRESS_BAR_POS, (WPARAM)_r_calc_percentof (total_written, total_length), 0);

		if (update_info->htaskbar)
		{
			_r_taskbar_setprogressstate (update_info->htaskbar, update_info->htaskdlg, TBPF_NORMAL);

			_r_taskbar_setprogressvalue (update_info->htaskbar, update_info->htaskdlg, total_written, total_length);
		}
	}

	return TRUE;
}

NTSTATUS _r_update_downloadupdate (
	_In_ PR_UPDATE_INFO update_info,
	_Inout_ PR_UPDATE_COMPONENT update_component
)
{
	R_DOWNLOAD_INFO download_info;
	LPCWSTR path;
	HANDLE hfile;
	NTSTATUS status;

	path = _r_app_getcachedirectory (TRUE);

	status = _r_fs_createfile (
		update_component->cache_path->buffer,
		FILE_OVERWRITE_IF,
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY,
		0,
		FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
		return status;

	_r_inet_initializedownload (&download_info, hfile, &_r_update_downloadcallback, update_info);

	status = _r_inet_begindownload (update_info->hsession, &update_component->url->sr, &download_info);

	NtClose (hfile); // required!

	if (status != STATUS_SUCCESS)
		return status;

	if (update_component->flags & PR_UPDATE_FLAG_FILE)
	{
		// copy required files
		if (_r_fs_exists (update_component->target_path->buffer))
			_r_fs_deletefile (update_component->target_path->buffer, NULL);

		// move target files
		status = _r_fs_movefile (update_component->cache_path->buffer, update_component->target_path->buffer, FALSE);

		if (!NT_SUCCESS (status))
			_r_fs_copyfile (update_component->cache_path->buffer, update_component->target_path->buffer, FALSE);

		// remove if it exists
		if (_r_fs_exists (update_component->cache_path->buffer))
			_r_fs_deletefile (update_component->cache_path->buffer, NULL);

		update_component->flags &= ~PR_UPDATE_FLAG_AVAILABLE;

		_r_obj_movereference (&update_component->current_version, update_component->new_version);

		update_component->new_version = NULL;
	}

	// remove cache directory if it is empty
	_r_fs_deletedirectory (path, FALSE);

	return STATUS_SUCCESS;
}

NTSTATUS NTAPI _r_update_downloadthread (
	_In_ PVOID arglist
)
{
	TASKDIALOG_COMMON_BUTTON_FLAGS buttons;
	PR_UPDATE_COMPONENT update_component;
	PR_UPDATE_INFO update_info;
	LPCWSTR str_content;
	LPCWSTR main_icon = NULL;
	ULONG update_flags = 0;
	ULONG status = STATUS_SUCCESS;

	update_info = arglist;

	for (ULONG_PTR i = 0; i < _r_obj_getarraysize (update_info->components); i++)
	{
		update_component = _r_obj_getarrayitem (update_info->components, i);

		if (!(update_component->flags & PR_UPDATE_FLAG_AVAILABLE))
			continue;

		status = _r_update_downloadupdate (update_info, update_component);

		if (status != STATUS_SUCCESS)
			break;

		update_flags |= update_component->flags;
	}

	// set result text and navigate taskdialog
	if (status == STATUS_SUCCESS)
	{
		if (update_flags & PR_UPDATE_FLAG_FILE)
			_r_update_applyconfig ();

		if (update_flags & PR_UPDATE_FLAG_INSTALLER)
		{
#if defined(IDS_UPDATE_INSTALL)
			str_content = _r_locale_getstring (IDS_UPDATE_INSTALL);
#else
			str_content = L"Update available. Do you want to install it now?";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL

			buttons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
		}
		else
		{
#if defined(IDS_UPDATE_DONE)
			str_content = _r_locale_getstring (IDS_UPDATE_DONE);
#else
			str_content = L"Downloading update finished.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE

			buttons = TDCBF_CLOSE_BUTTON;
		}
	}
	else
	{
		main_icon = TD_WARNING_ICON;
		buttons = TDCBF_CLOSE_BUTTON;

#if defined(IDS_UPDATE_ERROR)
		str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
		str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
	}

	_r_update_navigate (update_info, buttons, 0, main_icon, NULL, str_content, status, ET_WINHTTP);

	return STATUS_SUCCESS;
}

NTSTATUS NTAPI _r_update_checkthread (
	_In_ PVOID arglist
)
{
	PR_UPDATE_COMPONENT update_component;
	PR_UPDATE_INFO update_info;
	WCHAR str_updates[256] = {0};
	LPCWSTR str_content;
	R_DOWNLOAD_INFO download_info;
	R_STRINGREF sr;
	PR_HASHTABLE string_table;
	PR_STRING string_value = NULL;
	PR_STRING update_url;
	PR_STRING string;
	R_STRINGREF remaining_part;
	R_STRINGREF new_version_sr;
	R_STRINGREF new_url_sr;
	ULONG_PTR downloads_count = 0;
	ULONG hash_code;
	NTSTATUS status;

	update_info = arglist;

	update_url = _r_obj_createstring (_r_app_getupdate_url ());

	_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

	status = _r_inet_begindownload (update_info->hsession, &update_url->sr, &download_info);

	if (status != STATUS_SUCCESS)
	{
		if (update_info->hparent)
		{
#if defined(IDS_UPDATE_ERROR)
			str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
			str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

			_r_update_navigate (update_info, TDCBF_CLOSE_BUTTON, 0, TD_WARNING_ICON, NULL, str_content, status, ET_WINHTTP);
		}

		goto CleanupExit;
	}

	string_table = _r_str_unserialize (&download_info.string->sr, L';', L'=');

	if (string_table)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (update_info->components); i++)
		{
			update_component = _r_obj_getarrayitem (update_info->components, i);

			hash_code = _r_str_gethash2 (&update_component->short_name->sr, TRUE);
			string = _r_obj_findhashtablepointer (string_table, hash_code);

			_r_obj_movereference (&string_value, string);

			if (!string_value)
				continue;

			if (!_r_str_splitatchar (&string_value->sr, L'|', &new_version_sr, &remaining_part))
				continue;

			if (_r_str_versioncompare (&update_component->current_version->sr, &new_version_sr) != -1)
				continue;

			_r_str_splitatchar (&remaining_part, L'|', &new_url_sr, &remaining_part);

			_r_obj_movereference (&update_component->url, _r_obj_createstring2 (&new_url_sr));
			_r_obj_movereference (&update_component->new_version, _r_obj_createstring2 (&new_version_sr));

			update_component->flags |= PR_UPDATE_FLAG_AVAILABLE;

			string = _r_str_formatversion (update_component->new_version);

			_r_str_appendformat (str_updates, RTL_NUMBER_OF (str_updates), L"%s - %s\r\n", update_component->full_name->buffer, string->buffer);

			_r_obj_dereference (string);

			if (update_component->flags & PR_UPDATE_FLAG_INSTALLER)
			{
				// do not check components when new version of application is available
				update_info->flags |= PR_UPDATE_FLAG_INSTALLER;

				break;
			}
			else if (update_component->flags & PR_UPDATE_FLAG_FILE)
			{
				if (_r_config_getboolean (L"IsAutoinstallUpdates", FALSE))
				{
					status = _r_update_downloadupdate (update_info, update_component);

					if (status != STATUS_SUCCESS)
						break;

					downloads_count += 1;
				}
				else
				{
					update_info->flags |= PR_UPDATE_FLAG_FILE;
				}
			}
		}

		if (string_value)
			_r_obj_dereference (string_value);

		_r_obj_dereference (string_table);
	}

	if (downloads_count)
		_r_update_applyconfig ();

	if (update_info->flags)
	{
		_r_obj_initializestringref (&sr, str_updates);

		_r_str_trimstring2 (&sr, L"\r\n ", 0);

#if defined(IDS_UPDATE_YES)
		str_content = _r_locale_getstring (IDS_UPDATE_YES);
#else
		str_content = L"Update available. Download and install now?";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

		_r_update_navigate (update_info, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, 0, NULL, str_content, sr.buffer, 0, ET_WINDOWS);
	}
	else
	{
		if (update_info->hparent)
		{
			if (status != STATUS_SUCCESS)
			{
#if defined(IDS_UPDATE_ERROR)
				str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
				str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
			}
			else
			{
				if (downloads_count)
				{
#if defined(IDS_UPDATE_DONE)
					str_content = _r_locale_getstring (IDS_UPDATE_DONE);
#else
					str_content = L"Downloading update finished.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
				}
				else
				{
#if defined(IDS_UPDATE_NO)
					str_content = _r_locale_getstring (IDS_UPDATE_NO);
#else
					str_content = L"No updates available.";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO
				}
			}

			_r_update_navigate (update_info, TDCBF_CLOSE_BUTTON, 0, NULL, NULL, str_content, status, ET_WINHTTP);
		}
	}

	_r_config_setlong64 (L"CheckUpdatesLast", _r_unixtime_now ());

CleanupExit:

	_r_inet_destroydownload (&download_info);

	_r_obj_dereference (update_url);

	_InterlockedDecrement (&update_info->lock);

	return STATUS_SUCCESS;
}

VOID _r_update_enable (
	_In_ BOOLEAN is_enable
)
{
	_r_config_setlong (L"CheckUpdatesPeriod", is_enable ? APP_UPDATE_PERIOD : 0);
}

_Success_ (return)
BOOLEAN _r_update_isenabled (
	_In_ BOOLEAN is_checktimestamp
)
{
	LONG64 update_last_timestamp;
	LONG64 current_timestamp;
	LONG update_period;

	update_period = _r_config_getlong (L"CheckUpdatesPeriod", APP_UPDATE_PERIOD);

	if (update_period <= 0)
		return FALSE;

	if (is_checktimestamp)
	{
		current_timestamp = _r_unixtime_now ();

		update_last_timestamp = _r_config_getlong64 (L"CheckUpdatesLast", 0);

		if ((current_timestamp - update_last_timestamp) <= _r_calc_hours2seconds (update_period))
			return FALSE;
	}

	return TRUE;
}

_Success_ (return)
BOOLEAN _r_update_check (
	_In_opt_ HWND hparent
)
{
	PR_UPDATE_INFO update_info;
	R_ENVIRONMENT environment;
	LPCWSTR str_content;
	HANDLE hthread;
	NTSTATUS status;

	update_info = &app_global.update.info;

	if (!hparent && !_r_update_isenabled (TRUE))
		return FALSE;

	if (_InterlockedCompareExchange (&update_info->lock, 0, 0) != 0)
		return FALSE;

	if (!update_info->hsession)
		update_info->hsession = _r_inet_createsession (_r_app_getuseragent (), _r_app_getproxyconfiguration ());

	if (!update_info->hsession)
		return FALSE;

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_LOWEST, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	status = _r_sys_createthread (&hthread, NtCurrentProcess (), &_r_update_checkthread, update_info, &environment, L"UpdateThread");

	if (!NT_SUCCESS (status))
		return FALSE;

	_InterlockedIncrement (&update_info->lock);

	update_info->hparent = hparent;
	update_info->htaskdlg = NULL;
	update_info->hthread = NULL;

	update_info->flags = 0;

	update_info->is_clicked = FALSE;

	if (hparent)
	{
#if defined(IDS_UPDATE_INIT)
		str_content = _r_locale_getstring (IDS_UPDATE_INIT);
#else
		str_content = L"Checking for new releases...";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

		update_info->hthread = hthread;

		_r_update_navigate (update_info, TDCBF_CANCEL_BUTTON, TDF_SHOW_PROGRESS_BAR, NULL, NULL, str_content, 0, ET_WINDOWS);
	}
	else
	{
		NtResumeThread (hthread, NULL);

		NtClose (hthread);
	}

	return TRUE;
}

HRESULT CALLBACK _r_update_pagecallback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam,
	_In_ LONG_PTR context
)
{
	PR_UPDATE_INFO update_info = (PR_UPDATE_INFO)context;

	UNREFERENCED_PARAMETER (lparam);

	switch (msg)
	{
		case TDN_CREATED:
		{
			HWND hparent;

			update_info->htaskdlg = hwnd;

			_r_wnd_sendmessage (hwnd, 0, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			_r_wnd_sendmessage (hwnd, 0, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			_r_taskbar_initialize (&update_info->htaskbar);

			hparent = GetParent (hwnd);

			_r_wnd_center (hwnd, hparent);

			_r_wnd_top (hwnd, TRUE); // always on top

			break;
		}

		case TDN_BUTTON_CLICKED:
		{
			PR_UPDATE_COMPONENT update_component;
			LPCWSTR str_content;
			NTSTATUS status;

			if (wparam == IDYES)
			{
				status = _r_sys_createthread (&update_info->hthread, NtCurrentProcess (), &_r_update_downloadthread, update_info, NULL, L"UpdateThread");

				if (NT_SUCCESS (status))
				{
					if (update_info->is_clicked)
					{
						_r_config_setboolean (L"IsAutoinstallUpdates", TRUE);

						update_info->is_clicked = FALSE;
					}

#if defined(IDS_UPDATE_DOWNLOAD)
					str_content = _r_locale_getstring (IDS_UPDATE_DOWNLOAD);
#else
					str_content = L"Downloading update...";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

					_r_update_navigate (update_info, TDCBF_CANCEL_BUTTON, TDF_SHOW_PROGRESS_BAR, NULL, NULL, str_content, 0, ET_WINDOWS);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				for (ULONG_PTR i = 0; i < _r_obj_getarraysize (update_info->components); i++)
				{
					update_component = _r_obj_getarrayitem (update_info->components, i);

					if (update_component->flags & (PR_UPDATE_FLAG_INSTALLER | PR_UPDATE_FLAG_AVAILABLE))
						_r_update_install (update_component);
				}
			}
			else if (wparam == IDCANCEL)
			{
				if (update_info->hthread)
				{
					NtTerminateThread (update_info->hthread, STATUS_CANCELLED);
					NtClose (update_info->hthread);

					update_info->hthread = NULL;
				}
			}

			break;
		}

		case TDN_DESTROYED:
		{
			if (update_info->hthread)
			{
				NtTerminateThread (update_info->hthread, STATUS_CANCELLED);
				NtClose (update_info->hthread);

				update_info->hthread = NULL;
			}

			if (update_info->htaskbar)
			{
				_r_taskbar_destroy (update_info->htaskbar);

				update_info->htaskbar = NULL;
			}

			if (update_info->htaskdlg)
				update_info->htaskdlg = NULL;

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			//_r_theme_initialize (hwnd, _r_theme_isenabled ());

			if (_r_sys_isosversiongreaterorequal (WINDOWS_11))
				DwmSetWindowAttribute (hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &(DWM_WINDOW_CORNER_PREFERENCE){ DWMWCP_DONOTROUND }, sizeof (DWM_WINDOW_CORNER_PREFERENCE));

			if (update_info->hthread)
			{
				NtResumeThread (update_info->hthread, NULL);
				NtClose (update_info->hthread);

				update_info->hthread = NULL;
			}

			break;
		}

		case TDN_VERIFICATION_CLICKED:
		{
			update_info->is_clicked = !!wparam;
			break;
		}
	}

	return S_OK;
}

VOID _r_update_navigate (
	_In_ PR_UPDATE_INFO update_info,
	_In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
	_In_opt_ TASKDIALOG_FLAGS flags,
	_In_opt_ LPCWSTR icon,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR content,
	_In_opt_ LONG error_code,
	_In_ R_ERROR_TYPE type
)
{
	TASKDIALOGCONFIG tdc = {0};
	WCHAR buffer[0x0800];
	PR_STRING string = NULL;
	PVOID hlib;
	BOOL is_checked;
	NTSTATUS status;

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | flags;
	tdc.hwndParent = update_info->hparent ? update_info->hparent : _r_app_gethwnd ();
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.dwCommonButtons = buttons;
	tdc.pfCallback = &_r_update_pagecallback;
	tdc.lpCallbackData = (LONG_PTR)update_info;

	tdc.pszWindowTitle = _r_app_getname ();

#if defined(IDI_MAIN)
	tdc.pszMainIcon = icon ? icon : MAKEINTRESOURCEW (IDI_MAIN);
#endif // IDI_MAIN

	if (title)
		tdc.pszMainInstruction = title;

	if (content)
		tdc.pszContent = content;

	if (error_code != STATUS_SUCCESS)
	{
		status = _r_sys_loadlibrarytype (type, &hlib);

		if (NT_SUCCESS (status))
		{
			_r_sys_formatmessage (error_code, hlib, 0, &string);

			_r_str_printf (
				buffer,
				RTL_NUMBER_OF (buffer),
				L"%s\r\n\r\nDescription:\r\n%s\r\nStatus:\r\n%" TEXT (PR_LONG) L" (0x%08" TEXT (PRIX32)")",
				content,
				_r_obj_getstringordefault (string, L"n/a"),
				error_code,
				error_code
			);

			tdc.pszContent = buffer;

			_r_sys_freelibrary (hlib);
		}
	}

	if (buttons & TDCBF_YES_BUTTON)
	{
		if (update_info->flags & PR_UPDATE_FLAG_FILE)
		{
#if defined(IDS_UPDATE_AUTOINSTALL)
			tdc.pszVerificationText = _r_locale_getstring (IDS_UPDATE_AUTOINSTALL);
#else
			tdc.pszVerificationText = L"Automatically install non-executable updates";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_UPDATE_AUTOINSTALL)
#endif // IDS_UPDATE_AUTOINSTALL
		}
	}

	if (update_info->htaskdlg)
	{
		_r_wnd_sendmessage (update_info->htaskdlg, 0, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);
	}
	else
	{
		_r_msg_taskdialog (&tdc, NULL, NULL, &is_checked);
	}

	if (string)
		_r_obj_dereference (string);
}

VOID _r_update_addcomponent (
	_In_ LPCWSTR full_name,
	_In_ LPCWSTR short_name,
	_In_ LPCWSTR version,
	_In_ PR_STRING target_path,
	_In_ BOOLEAN is_installer
)
{
	R_UPDATE_COMPONENT update_component = {0};
	WCHAR rnd_string[8];

	update_component.full_name = _r_obj_createstring (full_name);
	update_component.short_name = _r_obj_createstring (short_name);
	update_component.current_version = _r_obj_createstring (version);
	update_component.target_path = _r_obj_reference (target_path);

	_r_str_generaterandom (rnd_string, RTL_NUMBER_OF (rnd_string), FALSE);

	update_component.cache_path = _r_obj_concatstrings (
		7,
		_r_app_getcachedirectory (FALSE),
		L"\\",
		L"update-",
		short_name,
		L"-",
		rnd_string,
		is_installer ? L".exe" : L".tmp"
	);

	update_component.flags = is_installer ? PR_UPDATE_FLAG_INSTALLER : PR_UPDATE_FLAG_FILE;

	_r_obj_addarrayitem (app_global.update.info.components, &update_component);
}

VOID _r_update_applyconfig ()
{
	HWND hwnd;

	_r_config_initialize (); // reload config
	_r_locale_initialize (); // reload locale

	hwnd = _r_app_gethwnd ();

	if (!hwnd)
		return;

	_r_wnd_sendmessage (hwnd, 0, RM_CONFIG_UPDATE, 0, 0);
	_r_wnd_sendmessage (hwnd, 0, RM_INITIALIZE, 0, 0);

	_r_wnd_sendmessage (hwnd, 0, RM_LOCALIZE, 0, 0);
}

VOID _r_update_install (
	_In_ PR_UPDATE_COMPONENT update_component
)
{
	PR_STRING cmd_string;

	if (!_r_fs_exists (update_component->cache_path->buffer))
		return;

	cmd_string = _r_format_string (L"\"%s\" /u /S /D=%s", update_component->cache_path->buffer, update_component->target_path->buffer);

	if (!_r_sys_runasadmin (update_component->cache_path->buffer, cmd_string->buffer, NULL))
		_r_show_errormessage (NULL, NULL, NtLastError (), update_component->cache_path->buffer, ET_WINDOWS);

	_r_obj_dereference (cmd_string);
}

//
// Logging
//

BOOLEAN _r_log_isenabled (
	_In_ R_LOG_LEVEL log_level_check
)
{
	R_LOG_LEVEL log_level;

	log_level = _r_config_getlong (L"ErrorLevel", LOG_LEVEL_DEBUG);

	if (log_level == LOG_LEVEL_DISABLED)
		return FALSE;

	if (log_level > log_level_check)
		return FALSE;

	return TRUE;
}

_Ret_maybenull_
HANDLE _r_log_getfilehandle ()
{
	LARGE_INTEGER file_size;
	BYTE bom[] = {0xFF, 0xFE};
	HANDLE hfile;
	NTSTATUS status;

	status = _r_fs_createfile (
		_r_app_getlogpath ()->buffer,
		FILE_OPEN_IF,
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_ATTRIBUTE_NORMAL,
		FILE_WRITE_THROUGH | FILE_SEQUENTIAL_ONLY,
		FALSE,
		NULL,
		&hfile
	);

	if (NT_SUCCESS (status))
	{
		status = _r_fs_getsize (NULL, hfile, &file_size);

		if (NT_SUCCESS (status))
		{
			if (!file_size.QuadPart)
			{
				// write utf-16 le byte order mask
				_r_fs_writefile (hfile, bom, sizeof (bom));

				// adds csv header
				_r_fs_writefile (hfile, PR_DEBUG_HEADER, (ULONG)(_r_str_getlength (PR_DEBUG_HEADER) * sizeof (WCHAR)));
			}

			_r_fs_setpos (hfile, &file_size);
		}
	}

	return hfile;
}

VOID _r_log (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ LONG error_code,
	_In_opt_ LPCWSTR description
)
{
	PR_STRING error_string;
	PR_STRING date_string;
	LPCWSTR level_string;
	HANDLE hfile;
	LONG64 timestamp;
	ULONG number = 0;

	if (!_r_log_isenabled (log_level))
		return;

	timestamp = _r_unixtime_now ();
	date_string = _r_format_unixtime (timestamp, FDTF_SHORTDATE | FDTF_LONGTIME | FDTF_NOAUTOREADINGORDER);

	level_string = _r_log_leveltostring (log_level);

	// print log for debuggers
	_r_debug (L"[%s], %s, 0x%08" TEXT (PRIX32) L", %s\r\n", level_string, title, error_code, description);

	hfile = _r_log_getfilehandle ();

	if (hfile)
	{
		error_string = _r_format_string (
			PR_DEBUG_BODY,
			level_string,
			_r_obj_getstring (date_string),
			title,
			error_code,
			description,
			_r_app_getversion (),
			NtCurrentPeb ()->OSMajorVersion,
			NtCurrentPeb ()->OSMinorVersion,
			NtCurrentPeb ()->OSBuildNumber
		);

		_r_fs_writefile (hfile, error_string->buffer, (ULONG)error_string->length);

		NtClose (hfile);

		_r_obj_dereference (error_string);
	}

	if (date_string)
		_r_obj_dereference (date_string);

	// show tray balloon
#if defined(APP_HAVE_TRAY)
	if (tray_guid)
	{
		if (_r_config_getboolean (L"IsErrorNotificationsEnabled", TRUE))
		{
			number = _r_log_leveltrayicon (log_level);

			if (!_r_config_getboolean (L"IsNotificationsSound", TRUE))
				number |= NIIF_NOSOUND;

			// check for timeout (sec.)
			if ((timestamp - app_global.error.last_timestamp) > APP_ERROR_PERIOD)
			{
				_r_tray_popup (_r_app_gethwnd (), tray_guid, number, _r_app_getname (), APP_WARNING_LOG_TEXT);

				app_global.error.last_timestamp = timestamp;
			}
		}
	}
#endif // APP_HAVE_TRAY
}

VOID _r_log_v (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ LONG error_code,
	_In_ _Printf_format_string_ LPCWSTR format,
	...
)
{
	WCHAR string[512];
	va_list arg_ptr;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	_r_log (log_level, tray_guid, title, error_code, string);
}

LPCWSTR _r_log_leveltostring (
	_In_ R_LOG_LEVEL log_level
)
{
	switch (log_level)
	{
		case LOG_LEVEL_DISABLED:
		{
			return L"Disabled";
		}

		case LOG_LEVEL_DEBUG:
		{
			return L"Debug";
		}

		case LOG_LEVEL_INFO:
		{
			return L"Info";
		}

		case LOG_LEVEL_WARNING:
		{
			return L"Warning";
		}

		case LOG_LEVEL_ERROR:
		{
			return L"Error";
		}

		case LOG_LEVEL_CRITICAL:
		{
			return L"Critical";
		}
	}

	return NULL;
}

ULONG _r_log_leveltrayicon (
	_In_ R_LOG_LEVEL log_level
)
{
	switch (log_level)
	{
		case LOG_LEVEL_DEBUG:
		case LOG_LEVEL_INFO:
		{
			return NIIF_INFO;
		}

		case LOG_LEVEL_WARNING:
		{
			return NIIF_WARNING;
		}

		case LOG_LEVEL_ERROR:
		case LOG_LEVEL_CRITICAL:
		{
			return NIIF_ERROR;
		}

		case LOG_LEVEL_DISABLED:
		default:
		{
			return NIIF_NONE;
		}
	}
}

//
// Messages
//

VOID _r_report_error (
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR string,
	_In_ LONG status,
	_In_ R_ERROR_TYPE type
)
{
#if defined(APP_CONSOLE)
	_r_console_writestringformat (L"EXCEPTION: %s (%d, 0x%08" TEXT (PRIX32) L")!\r\n", string, status, status);
#else
	_r_show_errormessage (NULL, title, status, string, type);
#endif // APP_CONSOLE
}

VOID _r_show_aboutmessage (
	_In_opt_ HWND hwnd
)
{
	static BOOLEAN is_opened = FALSE;

	TASKDIALOGCONFIG tdc = {0};
	LPCWSTR str_title;
	WCHAR str_content[512];

	if (is_opened)
		return;

	is_opened = TRUE;

#if defined(IDS_ABOUT)
	str_title = _r_locale_getstring (IDS_ABOUT);
#else
	str_title = L"About";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_ABOUT)
#endif

	_r_str_printf (
		str_content,
		RTL_NUMBER_OF (str_content),
		L"Version %s %s, %" TEXT (PR_LONG) L"-bit (Unicode)\r\n%s\r\n\r\n" \
		L"<a href=\"%s\">%s</a>",
		_r_app_getversion (),
		_r_app_getversiontype (),
		_r_app_getarch (),
		_r_app_getcopyright (),
		_r_app_getwebsite_url (),
		_r_app_getwebsite_url ()
	);

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_EXPAND_FOOTER_AREA | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
	tdc.pszFooterIcon = TD_INFORMATION_ICON;
	tdc.pszWindowTitle = str_title;
	tdc.pszMainInstruction = _r_app_getname ();
	tdc.pszContent = str_content;
	tdc.pfCallback = &_r_msg_callback;
	tdc.pszExpandedInformation = APP_ABOUT_DONATE;

#if defined(IDI_MAIN)
	tdc.pszMainIcon = MAKEINTRESOURCEW (IDI_MAIN);
#endif // IDI_MAIN

#if defined(IDS_DONATE)
	tdc.pszCollapsedControlText = _r_locale_getstring (IDS_DONATE);
#else
	tdc.pszCollapsedControlText = L"Donate!";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_DONATE)
#endif // IDS_DONATE

	tdc.pszFooter = APP_ABOUT_FOOTER;

	_r_msg_taskdialog (&tdc, NULL, NULL, NULL);

	is_opened = FALSE;
}

BOOLEAN _r_show_confirmmessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR title,
	_In_opt_ LPCWSTR content,
	_In_opt_ LPCWSTR config_key
)
{
	TASKDIALOGCONFIG tdc = {0};
	INT command_id = 0;
	BOOL is_flagchecked = FALSE;

	if (config_key && !_r_config_getboolean (config_key, TRUE))
		return TRUE;

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	if (config_key)
	{
#if defined(IDS_QUESTION_FLAG_CHK)
		tdc.pszVerificationText = _r_locale_getstring (IDS_QUESTION_FLAG_CHK);
#else
		tdc.pszVerificationText = L"Do not ask again";
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK
	}

	if (title)
		tdc.pszMainInstruction = title;

	if (content)
		tdc.pszContent = content;

	_r_msg_taskdialog (&tdc, &command_id, NULL, &is_flagchecked);

	if (command_id == IDYES)
	{
		if (config_key && is_flagchecked)
			_r_config_setboolean (config_key, FALSE);

		return TRUE;
	}

	return FALSE;
}

NTSTATUS _r_show_errormessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR title,
	_In_ LONG error_code,
	_In_opt_ LPCWSTR description,
	_In_ R_ERROR_TYPE type
)
{
	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[3] = {0};
	WCHAR str_content[1024] = {0};
	PR_STRING string = NULL;
	PR_STRING path;
	R_STRINGREF sr;
	PVOID hmodule;
	UINT btn_cnt = 0;
	INT command_id;
	NTSTATUS status;

	status = _r_sys_loadlibrarytype (type, &hmodule);

	if (!NT_SUCCESS (status))
		return status;

	if (hmodule)
	{
		status = _r_sys_formatmessage (error_code, hmodule, 0, &string);

		if (NT_SUCCESS (status))
			_r_str_trimstring2 (&string->sr, L"\r\n", PR_TRIM_END_ONLY);
	}
	else
	{
		if (description)
			string = _r_obj_createstring (description);
	}

	if (type == ET_NONE)
	{
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_obj_getstringordefault (string, L"n/a"));
	}
	else
	{
		_r_str_printf (
			str_content,
			RTL_NUMBER_OF (str_content),
			L"Message:\r\n%s\r\n\r\nStatus:\r\n%" TEXT (PR_LONG) " (0x%08" TEXT (PRIX32) L")",
			_r_obj_getstringordefault (string, L"n/a"),
			error_code,
			error_code
		);

		if (!_r_str_isempty (description))
			_r_str_appendformat (str_content, RTL_NUMBER_OF (str_content), L"\r\n\r\nDescription:\r\n%s", description);
	}

	path = _r_app_getcrashdirectory (FALSE);

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.pszFooterIcon = TD_INFORMATION_ICON;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pszMainInstruction = !_r_str_isempty (title) ? title : APP_FAILED_MESSAGE_TITLE;
	tdc.pszContent = str_content;
	tdc.pszFooter = APP_FAILED_MESSAGE_FOOTER;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	if (_r_fs_exists (path->buffer))
	{
		// add "Crash dumps" button
		td_buttons[btn_cnt].pszButtonText = L"Crash dumps";
		td_buttons[btn_cnt].nButtonID = IDYES;

		btn_cnt += 1;
	}

	// add "Copy" button
	td_buttons[btn_cnt].pszButtonText = L"Copy";
	td_buttons[btn_cnt].nButtonID = IDNO;

	btn_cnt += 1;

	// add "Close" button
	td_buttons[btn_cnt].pszButtonText = L"Close";
	td_buttons[btn_cnt].nButtonID = IDCLOSE;

	btn_cnt += 1;

	tdc.pButtons = td_buttons;
	tdc.cButtons = btn_cnt;

	tdc.nDefaultButton = IDCLOSE;

	if (SUCCEEDED (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL)))
	{
		if (command_id == IDYES)
		{
			_r_shell_opendefault (path->buffer);
		}
		else if (command_id == IDNO)
		{
			_r_obj_initializestringref (&sr, str_content);

			_r_clipboard_set (NULL, &sr);
		}
	}

	if (hmodule)
		_r_sys_freelibrary (hmodule);

	if (string)
		_r_obj_dereference (string);

	return status;
}

INT _r_show_message (
	_In_opt_ HWND hwnd,
	_In_ ULONG flags,
	_In_opt_ LPCWSTR title,
	_In_ LPCWSTR content
)
{
	TASKDIALOGCONFIG tdc = {0};
	INT command_id;

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pfCallback = &_r_msg_callback;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pszMainInstruction = title;
	tdc.pszContent = content;

	// set icons
	if ((flags & MB_ICONMASK) == MB_USERICON)
	{
#if defined(IDI_MAIN)
		tdc.pszMainIcon = MAKEINTRESOURCEW (IDI_MAIN);
#endif // IDI_MAIN
	}
	else if ((flags & MB_ICONMASK) == MB_ICONWARNING)
	{
		tdc.pszMainIcon = TD_WARNING_ICON;
	}
	else if ((flags & MB_ICONMASK) == MB_ICONERROR)
	{
		tdc.pszMainIcon = TD_ERROR_ICON;
	}
	else if ((flags & MB_ICONMASK) == MB_ICONINFORMATION || (flags & MB_ICONMASK) == MB_ICONQUESTION)
	{
		tdc.pszMainIcon = TD_INFORMATION_ICON;
	}

	// set buttons
	if ((flags & MB_TYPEMASK) == MB_YESNO)
	{
		tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	}
	else if ((flags & MB_TYPEMASK) == MB_YESNOCANCEL)
	{
		tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
	}
	else if ((flags & MB_TYPEMASK) == MB_OKCANCEL)
	{
		tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
	}
	else if ((flags & MB_TYPEMASK) == MB_RETRYCANCEL)
	{
		tdc.dwCommonButtons = TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;
	}
	else
	{
		tdc.dwCommonButtons = TDCBF_OK_BUTTON;
	}

	// set default buttons
	//if ((flags & MB_DEFMASK) == MB_DEFBUTTON2)
	//	tdc.nDefaultButton = IDNO;

	_r_msg_taskdialog (&tdc, &command_id, NULL, NULL);

	return command_id;
}

VOID _r_window_restoreposition (
	_In_ HWND hwnd,
	_In_ LPCWSTR window_name
)
{
	R_RECTANGLE rectangle_new = {0};
	R_RECTANGLE rectangle_current;
	LONG_PTR style;
	LONG dpi_value;

	if (!_r_wnd_getposition (hwnd, &rectangle_current))
		return;

	style = _r_wnd_getstyle (hwnd, GWL_STYLE);

	_r_config_getsize (L"Position", &rectangle_new.position, &rectangle_current.position, window_name);

	if (style & WS_SIZEBOX)
	{
		dpi_value = _r_dc_getwindowdpi (hwnd);

		_r_dc_getsizedpivalue (&rectangle_current.size, dpi_value, FALSE);

		_r_config_getsize (L"Size", &rectangle_new.size, &rectangle_current.size, window_name);

		_r_dc_getsizedpivalue (&rectangle_new.size, dpi_value, TRUE);
	}
	else
	{
		rectangle_new.size = rectangle_current.size;
	}

	_r_wnd_adjustrectangletoworkingarea (&rectangle_new, NULL);

	_r_wnd_setposition (hwnd, &rectangle_new.position, (style & WS_SIZEBOX) ? &rectangle_new.size : NULL);

	if (style & WS_MAXIMIZEBOX)
	{
		// HACK!!! Do not maximize main window!
		if (_r_str_compare (window_name, L"window", 0) != 0)
		{
			if (_r_config_getboolean_ex (L"IsMaximized", FALSE, window_name))
				ShowWindow (hwnd, SW_SHOWMAXIMIZED);
		}
	}
}

VOID _r_window_saveposition (
	_In_ HWND hwnd,
	_In_ LPCWSTR window_name
)
{
	WINDOWPLACEMENT placement = {0};
	MONITORINFO monitor_info = {0};
	R_RECTANGLE rectangle;
	HMONITOR hmonitor;
	LONG_PTR style;
	LONG dpi_value;
	BOOLEAN is_maximized;

	placement.length = sizeof (placement);

	if (!GetWindowPlacement (hwnd, &placement))
		return;

	style = _r_wnd_getstyle (hwnd, GWL_STYLE);

	is_maximized = (placement.showCmd == SW_MAXIMIZE) || (placement.showCmd == SW_SHOWMINIMIZED);

	if (style & WS_MAXIMIZEBOX)
		_r_config_setboolean_ex (L"IsMaximized", is_maximized, window_name);

	_r_wnd_recttorectangle (&rectangle, &placement.rcNormalPosition);

	hmonitor = MonitorFromRect (&placement.rcNormalPosition, MONITOR_DEFAULTTOPRIMARY);

	monitor_info.cbSize = sizeof (monitor_info);

	// The rectangle is in workspace coordinates. Convert the values back to screen coordinates.
	if (GetMonitorInfoW (hmonitor, &monitor_info))
	{
		rectangle.left += monitor_info.rcWork.left - monitor_info.rcMonitor.left;
		rectangle.top += monitor_info.rcWork.top - monitor_info.rcMonitor.top;
	}

	if (!is_maximized)
	{
		_r_config_setsize (L"Position", &rectangle.position, window_name);

		if (style & WS_SIZEBOX)
		{
			dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_dc_getsizedpivalue (&rectangle.size, dpi_value, FALSE);

			_r_config_setsize (L"Size", &rectangle.size, window_name);
		}
	}
}

//
// Settings
//

VOID _r_settings_addpage (
	_In_ INT dlg_id,
	_In_ UINT locale_id
)
{
	R_SETTINGS_PAGE settings_page = {0};

	settings_page.locale_id = locale_id;
	settings_page.dlg_id = dlg_id;

	_r_obj_addarrayitem (app_global.settings.page_list, &settings_page);
}

VOID _r_settings_adjustchild (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ HWND hchild
)
{
#if !defined(APP_HAVE_SETTINGS_TABS)
	RECT rc_tree;
	RECT rc_child;

	if (!GetWindowRect (GetDlgItem (hwnd, ctrl_id), &rc_tree) || !GetClientRect (hchild, &rc_child))
		return;

	MapWindowPoints (HWND_DESKTOP, hwnd, (PPOINT)&rc_tree, 2);

	SetWindowPos (
		hchild,
		NULL,
		(rc_tree.left * 2) + _r_calc_rectwidth (&rc_tree),
		rc_tree.top,
		rc_child.right,
		rc_child.bottom,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER
	);

#else
	_r_tab_adjustchild (hwnd, ctrl_id, hchild);
#endif // !APP_HAVE_SETTINGS_TABS
}

VOID _r_settings_createwindow (
	_In_opt_ HWND hparent,
	_In_opt_ DLGPROC dlg_proc,
	_In_opt_ LONG dlg_id
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SHORT width = 0;
	static SHORT height = 0;

	LPDLGTEMPLATEEX dlg_template;
	PR_SETTINGS_PAGE ptr_page;
	R_STORAGE dlg_buffer;
	PBYTE buffer_ptr;
	PVOID buffer;
	HWND hsettings;
	ULONG_PTR size;
	WORD controls;
	NTSTATUS status;

	assert (!_r_obj_isempty (app_global.settings.page_list));

	hsettings = _r_settings_getwindow ();

	if (hsettings)
	{
		_r_wnd_toggle (hsettings, TRUE);

		return;
	}

	// calculate maximum dialog size
	if (_r_initonce_begin (&init_once))
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
		{
			ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

			if (!ptr_page->dlg_id)
				continue;

			status = _r_res_loadresource (_r_sys_getimagebase (), RT_DIALOG, MAKEINTRESOURCEW (ptr_page->dlg_id), 0, &dlg_buffer);

			if (!NT_SUCCESS (status))
				continue;

			dlg_template = (LPDLGTEMPLATEEX)dlg_buffer.buffer;

			if (dlg_template->style & WS_CHILD)
			{
				if (width < dlg_template->cx)
					width = dlg_template->cx;

				if (height < dlg_template->cy)
					height = dlg_template->cy;
			}
		}

		height += 38;

#if defined(APP_HAVE_SETTINGS_TABS)
		width += 18;
#else
		width += 112;
#endif // APP_HAVE_SETTINGS_TABS

		_r_initonce_end (&init_once);
	}

#if defined(APP_HAVE_SETTINGS_TABS)
	controls = 4;
#else
	controls = 3;
#endif // APP_HAVE_SETTINGS_TABS

	size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) + ((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;

	buffer = _r_mem_allocate (size);
	buffer_ptr = buffer;

	if (dlg_id)
		_r_config_setlong (L"SettingsLastPage", dlg_id);

	if (dlg_proc)
		app_global.settings.wnd_proc = dlg_proc;

	//
	// set dialog information by filling DLGTEMPLATEEX structure
	//

	// dlgVer
	_r_util_templatewriteshort (&buffer_ptr, 1);

	// signature
	_r_util_templatewriteshort (&buffer_ptr, USHRT_MAX);

	// helpID
	_r_util_templatewriteulong (&buffer_ptr, 0);

	// exStyle
	_r_util_templatewriteulong (&buffer_ptr, WS_EX_APPWINDOW | WS_EX_CONTROLPARENT);

	// style
	_r_util_templatewriteulong (&buffer_ptr, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME);

	// cDlgItems
	_r_util_templatewriteshort (&buffer_ptr, controls);

	// x
	_r_util_templatewriteshort (&buffer_ptr, 0);

	// y
	_r_util_templatewriteshort (&buffer_ptr, 0);

	// cx
	_r_util_templatewriteshort (&buffer_ptr, width);

	// cy
	_r_util_templatewriteshort (&buffer_ptr, height);

	// menu
	_r_util_templatewritestring (&buffer_ptr, L"");

	// windowClass
	_r_util_templatewritestring (&buffer_ptr, L"");

	// title
	_r_util_templatewritestring (&buffer_ptr, L"");

	//
	// set dialog font
	//

	 // pointsize
	_r_util_templatewriteshort (&buffer_ptr, 8);

	// weight
	_r_util_templatewriteshort (&buffer_ptr, FW_NORMAL);

	// bItalic
	_r_util_templatewriteshort (&buffer_ptr, FALSE);

	// font
	_r_util_templatewritestring (&buffer_ptr, L"MS Shell Dlg");

	// insert dialog controls
#if defined(APP_HAVE_SETTINGS_TABS)
	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_NAV,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TCS_HOTTRACK | TCS_TOOLTIPS,
		8,
		6,
		width - 16,
		height - 34,
		WC_TABCONTROL
	);

	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_RESET,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON,
		8,
		(height - 22),
		50,
		14,
		WC_BUTTON
	);

	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_SAVE,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_DEFPUSHBUTTON,
		(width - 112),
		(height - 22),
		50,
		14,
		WC_BUTTON
	);
#else
	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_NAV,
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS | \
		TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_TRACKSELECT | TVS_INFOTIP | TVS_NOHSCROLL,
		8,
		6,
		88,
		height - 14,
		WC_TREEVIEW
	);

	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_RESET,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON,
		88 + 14,
		(height - 22),
		50,
		14,
		WC_BUTTON
	);

#endif // APP_HAVE_SETTINGS_TABS

	_r_util_templatewritecontrol (
		&buffer_ptr,
		IDC_CLOSE,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON,
		(width - 58),
		(height - 22),
		50,
		14,
		WC_BUTTON
	);

	DialogBoxIndirectParamW (_r_sys_getimagebase (), buffer, hparent, &_r_settings_wndproc, 0);

	_r_mem_free (buffer);
}

INT_PTR CALLBACK _r_settings_wndproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			PR_SETTINGS_PAGE ptr_page;
			LONG icon_small;
			LONG icon_large;
			LONG dpi_value;
			LONG dlg_id;

#if !defined(APP_HAVE_SETTINGS_TABS)
			HTREEITEM hitem;
#endif // !APP_HAVE_SETTINGS_TABS

#if defined(APP_HAVE_SETTINGS_TABS)
			INT index = 0;
#endif // APP_HAVE_SETTINGS_TABS

			app_global.settings.hwnd = hwnd;

			dpi_value = _r_dc_getwindowdpi (hwnd);

			icon_small = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
			icon_large = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);

#if defined(IDI_MAIN)
			_r_wnd_seticon (
				hwnd,
				_r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCEW (IDI_MAIN), icon_small),
				_r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCEW (IDI_MAIN), icon_large)
			);
#endif // IDI_MAIN

			// configure window
			_r_wnd_top (hwnd, TRUE);

			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			dlg_id = _r_config_getlong (L"SettingsLastPage", 0);

			for (ULONG_PTR i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
			{
				ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

				if (!ptr_page->dlg_id)
					continue;

				ptr_page->hwnd = _r_wnd_createwindow (_r_sys_getimagebase (), MAKEINTRESOURCEW (ptr_page->dlg_id), hwnd, app_global.settings.wnd_proc, 0);

				if (!ptr_page->hwnd)
					continue;

				BringWindowToTop (ptr_page->hwnd); // HACK!!!

				_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);

#if defined(APP_HAVE_SETTINGS_TABS)
				EnableThemeDialogTexture (ptr_page->hwnd, ETDT_ENABLETAB);

				_r_tab_additem (hwnd, IDC_NAV, index, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, (LPARAM)ptr_page);

				if (dlg_id && ptr_page->dlg_id == dlg_id)
					_r_tab_selectitem (hwnd, IDC_NAV, index);

				index += 1;
#else
				hitem = _r_treeview_additem (hwnd, IDC_NAV, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0, NULL, (LPARAM)ptr_page);

				if (dlg_id && ptr_page->dlg_id == dlg_id)
					_r_treeview_selectitem (hwnd, IDC_NAV, hitem);
#endif // APP_HAVE_SETTINGS_TABS

				_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page->hwnd);
			}

#if defined(APP_HAVE_SETTINGS_TABS)
			if (_r_tab_getcurrentitem (hwnd, IDC_NAV) <= 0)
				_r_tab_selectitem (hwnd, IDC_NAV, 0);
#endif // APP_HAVE_SETTINGS_TABS

			_r_wnd_sendmessage (hwnd, 0, RM_LOCALIZE, 0, 0);

			_r_theme_initialize (hwnd, _r_theme_isenabled ());

			break;
		}

		case RM_LOCALIZE:
		{
			PR_SETTINGS_PAGE ptr_page;
#if !defined(APP_HAVE_SETTINGS_TABS)
			HTREEITEM hitem;
			LONG dpi_value;
#endif // !APP_HAVE_SETTINGS_TABS

			// localize window
#if defined(IDS_SETTINGS)
			_r_ctrl_setstring (hwnd, 0, _r_locale_getstring (IDS_SETTINGS));
#else
			_r_ctrl_setstring (hwnd, 0, L"Settings");
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_SETTINGS)
#endif // IDS_SETTINGS

			// localize navigation
#if defined(APP_HAVE_SETTINGS_TABS)
			for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
			{
				ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

				if (ptr_page)
				{
					_r_tab_setitem (hwnd, IDC_NAV, i, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd && _r_wnd_isvisible (ptr_page->hwnd, FALSE))
						_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}
			}
#else
			dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_treeview_setstyle (
				hwnd,
				IDC_NAV,
				TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS,
				_r_dc_getdpi (PR_SIZE_ITEMHEIGHT, dpi_value),
				_r_dc_getdpi (PR_SIZE_TREEINDENT, dpi_value)
			);

			hitem = (HTREEITEM)_r_wnd_sendmessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_ROOT, 0);

			while (hitem)
			{
				ptr_page = (PR_SETTINGS_PAGE)_r_treeview_getitemlparam (hwnd, IDC_NAV, hitem);

				if (ptr_page)
				{
					_r_treeview_setitem (hwnd, IDC_NAV, hitem, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd)
					{
						if (_r_wnd_isvisible (ptr_page->hwnd, FALSE))
							_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
					}
				}

				hitem = (HTREEITEM)_r_wnd_sendmessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hitem);
			}
#endif // APP_HAVE_SETTINGS_TABS

			// localize buttons
#if defined(IDS_RESET)
			_r_ctrl_setstring (hwnd, IDC_RESET, _r_locale_getstring (IDS_RESET));
#else
			_r_ctrl_setstring (hwnd, IDC_RESET, L"Reset");
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_RESET)
#endif // IDS_RESET

#if defined(APP_HAVE_SETTINGS_TABS)
#if defined(IDS_SAVE)
			_r_ctrl_setstring (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
#else
			_r_ctrl_setstring (hwnd, IDC_SAVE, L"Save");
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_SAVE)
#endif // IDS_SAVE
#endif // APP_HAVE_SETTINGS_TABS

#if defined(IDS_CLOSE)
			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));
#else
			_r_ctrl_setstring (hwnd, IDC_CLOSE, L"Close");
#pragma PR_PRINT_WARNING_UNDEFINED(IDS_CLOSE)
#endif // IDS_CLOSE

			break;
		}

		case WM_DPICHANGED:
		{
			_r_wnd_message_dpichanged (hwnd, wparam, lparam);

			_r_wnd_sendmessage (hwnd, 0, RM_LOCALIZE, 0, 0);

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_message_settingchange (hwnd, wparam, lparam);
			break;
		}

		case WM_CLOSE:
		{
			EndDialog (hwnd, 0);
			break;
		}

		case WM_DESTROY:
		{
			PR_SETTINGS_PAGE ptr_page;

			for (ULONG_PTR i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
			{
				ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

				if (ptr_page->hwnd)
				{
					DestroyWindow (ptr_page->hwnd);

					ptr_page->hwnd = NULL;
				}
			}

			app_global.settings.hwnd = NULL;

			_r_wnd_top (_r_app_gethwnd (), _r_config_getboolean (L"AlwaysOnTop", FALSE));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr;
			INT ctrl_id;

			lphdr = (LPNMHDR)lparam;
			ctrl_id = (INT)(INT_PTR)lphdr->idFrom;

			switch (lphdr->code)
			{
#if defined(APP_HAVE_SETTINGS_TABS)
				case TCN_SELCHANGING:
				{
					PR_SETTINGS_PAGE ptr_page;
					INT tab_id;

					tab_id = _r_tab_getcurrentitem (hwnd, ctrl_id);
					ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, ctrl_id, tab_id);

					if (ptr_page)
						ShowWindow (ptr_page->hwnd, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					PR_SETTINGS_PAGE ptr_page;
					INT tab_id;

					tab_id = _r_tab_getcurrentitem (hwnd, ctrl_id);
					ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, ctrl_id, tab_id);

					if (!ptr_page || !ptr_page->hwnd || _r_wnd_isvisible (ptr_page->hwnd, FALSE))
						break;

					_r_config_setlong (L"SettingsLastPage", ptr_page->dlg_id);

					_r_tab_adjustchild (hwnd, ctrl_id, ptr_page->hwnd);

					_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

					ShowWindow (ptr_page->hwnd, SW_SHOW);

					break;
				}
#else
				case TVN_SELCHANGING:
				{
					LPNMTREEVIEWW lpnmtv;
					PR_SETTINGS_PAGE ptr_page_old;
					PR_SETTINGS_PAGE ptr_page_new;

					lpnmtv = (LPNMTREEVIEWW)lparam;

					ptr_page_old = (PR_SETTINGS_PAGE)lpnmtv->itemOld.lParam;
					ptr_page_new = (PR_SETTINGS_PAGE)lpnmtv->itemNew.lParam;

					if (ptr_page_old && ptr_page_old->hwnd && _r_wnd_isvisible (ptr_page_old->hwnd, FALSE))
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (!ptr_page_new || !ptr_page_new->hwnd || _r_wnd_isvisible (ptr_page_new->hwnd, FALSE))
						break;

					_r_config_setlong (L"SettingsLastPage", ptr_page_new->dlg_id);

					_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page_new->hwnd);

					_r_wnd_sendmessage (ptr_page_new->hwnd, 0, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

					ShowWindow (ptr_page_new->hwnd, SW_SHOW);

					break;
				}
#endif // APP_HAVE_SETTINGS_TABS
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
#if defined(APP_HAVE_SETTINGS_TABS)
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					PR_SETTINGS_PAGE ptr_page;
					LRESULT result;
					BOOLEAN is_saved = TRUE;

					for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
					{
						ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

						if (ptr_page && ptr_page->hwnd)
						{
							result = _r_wnd_sendmessage (ptr_page->hwnd, 0, RM_CONFIG_SAVE, (WPARAM)ptr_page->dlg_id, 0);

							if (result == -1)
							{
								is_saved = FALSE;

								break;
							}
						}
					}

					if (!is_saved)
						break;

					FALLTHROUGH;
				}
#endif // APP_HAVE_SETTINGS_TABS

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}

				case IDC_RESET:
				{
					PR_SETTINGS_PAGE ptr_page;
					PR_STRING path;
					HWND hwindow;

					if (_r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, NULL, APP_QUESTION_RESET) != IDYES)
						break;

					// made backup of existing configuration
					path = _r_app_getconfigpath ();

					_r_path_makebackup (path, TRUE);

					// reinitialize configuration
					_r_config_initialize (); // reload config
					_r_locale_initialize (); // reload locale

#if defined(APP_HAVE_AUTORUN)
					_r_autorun_enable (NULL, FALSE);
#endif // APP_HAVE_AUTORUN

#if defined(APP_HAVE_SKIPUAC)
					_r_skipuac_enable (NULL, FALSE);
#endif // APP_HAVE_SKIPUAC

					// reinitialize application
					hwindow = _r_app_gethwnd ();

					if (hwindow)
					{
						_r_wnd_sendmessage (hwindow, 0, RM_INITIALIZE, 0, 0);
						_r_wnd_sendmessage (hwindow, 0, RM_LOCALIZE, 0, 0);

						_r_wnd_sendmessage (hwindow, 0, WM_EXITSIZEMOVE, 0, 0); // reset size and pos

						_r_wnd_sendmessage (hwindow, 0, RM_CONFIG_RESET, 0, 0);
					}

					// reinitialize settings
					for (ULONG_PTR i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
					{
						ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

						if (!ptr_page->hwnd)
							continue;

						if (_r_wnd_isvisible (ptr_page->hwnd, FALSE))
							_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

						_r_wnd_sendmessage (ptr_page->hwnd, 0, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

HRESULT _r_skipuac_checkmodulepath (
	_In_ IRegisteredTaskPtr registered_task
)
{
	IActionCollection *action_collection = NULL;
	ITaskDefinition *task_definition = NULL;
	IExecAction *exec_action = NULL;
	IAction *action = NULL;
	BSTR task_path = NULL;
	LONG count;
	HRESULT status;

	status = IRegisteredTask_get_Definition (registered_task, &task_definition);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskDefinition_get_Actions (task_definition, &action_collection);

	if (FAILED (status))
		goto CleanupExit;

	// check actions count is equal to 1
	status = IActionCollection_get_Count (action_collection, &count);

	if (FAILED (status))
		goto CleanupExit;

	if (count != 1)
	{
		status = SCHED_E_INVALID_TASK;

		goto CleanupExit;
	}

	status = IActionCollection_get_Item (action_collection, 1, &action);

	if (FAILED (status))
		goto CleanupExit;

	status = IAction_QueryInterface (action, &IID_IExecAction, &exec_action);

	if (FAILED (status))
		goto CleanupExit;

	status = IExecAction_get_Path (exec_action, &task_path);

	if (FAILED (status))
		goto CleanupExit;

	// check path is for current module
	PathUnquoteSpacesW (task_path);

	if (_r_str_compare (task_path, _r_sys_getimagepath (), 0) != 0)
		status = SCHED_E_INVALID_TASK;

CleanupExit:

	if (task_path)
		SysFreeString (task_path);

	if (exec_action)
		IExecAction_Release (exec_action);

	if (action)
		IAction_Release (action);

	if (action_collection)
		IActionCollection_Release (action_collection);

	if (task_definition)
		ITaskDefinition_Release (task_definition);

	return status;
}

BOOLEAN _r_skipuac_isenabled ()
{
	IRegisteredTask *registered_task = NULL;
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	VARIANT empty = {0};
	BSTR task_name = NULL;
	HRESULT status;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	V_VT (&empty) = VT_NULL;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_GetFolder (task_service, NULL, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	task_name = SysAllocString (APP_SKIPUAC_NAME);

	status = ITaskFolder_GetTask (task_folder, task_name, &registered_task);

	if (FAILED (status))
		goto CleanupExit;

	status = _r_skipuac_checkmodulepath (registered_task);

CleanupExit:

	if (task_name)
		SysFreeString (task_name);

	if (registered_task)
		IRegisteredTask_Release (registered_task);

	if (task_folder)
		ITaskFolder_Release (task_folder);

	if (task_service)
		ITaskService_Release (task_service);

	return SUCCEEDED (status);
}

HRESULT _r_skipuac_enable (
	_In_opt_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	IActionCollection *action_collection = NULL;
	IRegistrationInfo *registration_info = NULL;
	IRegisteredTask *registered_task = NULL;
	ITaskDefinition *task_definition = NULL;
	ITaskSettings2 *task_settings2 = NULL;
	ITaskSettings *task_settings = NULL;
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	IExecAction *exec_action = NULL;
	IPrincipal *principal = NULL;
	IAction *action = NULL;
	VARIANT empty = {0};
	BSTR task_time_limit = NULL;
	BSTR task_directory = NULL;
	BSTR task_author = NULL;
	BSTR task_name = NULL;
	BSTR task_path = NULL;
	BSTR task_args = NULL;
	BSTR task_url = NULL;
	HRESULT status;

	if (hwnd && is_enable)
	{
		if (!_r_path_issecurelocation (_r_sys_getimagepath ()))
		{
			if (!_r_show_confirmmessage (hwnd, APP_SECURITY_TITLE, APP_WARNING_UAC_TEXT, NULL))
				return E_ABORT;
		}
	}

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	V_VT (&empty) = VT_NULL;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_GetFolder (task_service, NULL, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	task_name = SysAllocString (APP_SKIPUAC_NAME);

	if (is_enable)
	{
		status = ITaskService_NewTask (task_service, 0, &task_definition);

		if (FAILED (status))
			goto CleanupExit;

		status = ITaskDefinition_get_RegistrationInfo (task_definition, &registration_info);

		if (FAILED (status))
			goto CleanupExit;

		task_author = SysAllocString (_r_app_getauthor ());
		task_url = SysAllocString (_r_app_getwebsite_url ());

		IRegistrationInfo_put_Author (registration_info, task_author);
		IRegistrationInfo_put_URI (registration_info, task_url);

		status = ITaskDefinition_get_Settings (task_definition, &task_settings);

		if (FAILED (status))
			goto CleanupExit;

		// Set task compatibility (win7+)
		// Set task compatibility (win7+)
		//
		// TASK_COMPATIBILITY_V2_4 - win10
		// TASK_COMPATIBILITY_V2_3 - win8.1
		// TASK_COMPATIBILITY_V2_2 - win8
		// TASK_COMPATIBILITY_V2_1 - win7

		for (TASK_COMPATIBILITY i = TASK_COMPATIBILITY_V2_4; i != TASK_COMPATIBILITY_V2; --i)
		{
			status = ITaskSettings_put_Compatibility (task_settings, i);

			if (SUCCEEDED (status))
				break;
		}

		// Set task settings (win7+)
		task_time_limit = SysAllocString (L"PT0S");

		ITaskSettings_put_MultipleInstances (task_settings, TASK_INSTANCES_PARALLEL);
		ITaskSettings_put_DisallowStartIfOnBatteries (task_settings, VARIANT_FALSE);
		ITaskSettings_put_StopIfGoingOnBatteries (task_settings, VARIANT_FALSE);
		ITaskSettings_put_ExecutionTimeLimit (task_settings, task_time_limit);
		ITaskSettings_put_AllowHardTerminate (task_settings, VARIANT_FALSE);
		ITaskSettings_put_StartWhenAvailable (task_settings, VARIANT_TRUE);
		ITaskSettings_put_AllowDemandStart (task_settings, VARIANT_TRUE);
		ITaskSettings_put_Priority (task_settings, 1); // HIGH_PRIORITY_CLASS

		// win7+
		status = ITaskSettings_QueryInterface (task_settings, &IID_ITaskSettings2, &task_settings2);

		if (SUCCEEDED (status))
		{
			ITaskSettings2_put_UseUnifiedSchedulingEngine (task_settings2, VARIANT_TRUE);
			ITaskSettings2_put_DisallowStartOnRemoteAppSession (task_settings2, VARIANT_TRUE);

			ITaskSettings2_Release (task_settings2);
		}

		status = ITaskDefinition_get_Principal (task_definition, &principal);

		if (FAILED (status))
			goto CleanupExit;

		IPrincipal_put_RunLevel (principal, TASK_RUNLEVEL_HIGHEST);
		IPrincipal_put_LogonType (principal, TASK_LOGON_INTERACTIVE_TOKEN);

		status = ITaskDefinition_get_Actions (task_definition, &action_collection);

		if (FAILED (status))
			goto CleanupExit;

		status = IActionCollection_Create (action_collection, TASK_ACTION_EXEC, &action);

		if (FAILED (status))
			goto CleanupExit;

		status = IAction_QueryInterface (action, &IID_IExecAction, &exec_action);

		if (FAILED (status))
			goto CleanupExit;

		task_path = SysAllocString (_r_sys_getimagepath ());
		task_directory = SysAllocString (_r_app_getdirectory ()->buffer);
		task_args = SysAllocString (L"$(Arg0)");

		IExecAction_put_Path (exec_action, task_path);
		IExecAction_put_WorkingDirectory (exec_action, task_directory);
		IExecAction_put_Arguments (exec_action, task_args);

		ITaskFolder_DeleteTask (task_folder, task_name, 0);

		status = ITaskFolder_RegisterTaskDefinition (
			task_folder,
			task_name,
			task_definition,
			TASK_CREATE_OR_UPDATE,
			empty,
			empty,
			TASK_LOGON_INTERACTIVE_TOKEN,
			empty,
			&registered_task
		);
	}
	else
	{
		status = ITaskFolder_DeleteTask (task_folder, task_name, 0);

		if (status == E_NOT_SET)
			status = S_OK;
	}

CleanupExit:

	if (task_name)
		SysFreeString (task_name);

	if (task_author)
		SysFreeString (task_author);

	if (task_url)
		SysFreeString (task_url);

	if (task_time_limit)
		SysFreeString (task_time_limit);

	if (task_path)
		SysFreeString (task_path);

	if (task_directory)
		SysFreeString (task_directory);

	if (task_args)
		SysFreeString (task_args);

	if (registered_task)
		IRegisteredTask_Release (registered_task);

	if (exec_action)
		IExecAction_Release (exec_action);

	if (action)
		IAction_Release (action);

	if (action_collection)
		IActionCollection_Release (action_collection);

	if (principal)
		IPrincipal_Release (principal);

	if (task_settings)
		ITaskSettings_Release (task_settings);

	if (registration_info)
		IRegistrationInfo_Release (registration_info);

	if (task_definition)
		ITaskDefinition_Release (task_definition);

	if (task_folder)
		ITaskFolder_Release (task_folder);

	if (task_service)
		ITaskService_Release (task_service);

	if (hwnd && FAILED (status))
		_r_show_errormessage (hwnd, NULL, status, NULL, ET_WINDOWS);

	return status;
}

BOOLEAN _r_skipuac_run ()
{
	IRegisteredTask *registered_task = NULL;
	IRunningTask *running_task = NULL;
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	VARIANT empty = {0};
	BSTR task_name = NULL;
	BSTR task_args = NULL;
	WCHAR arguments[512] = {0};
	VARIANT params = {0};
	LPWSTR *arga;
	ULONG attempts = 6;
	TASK_STATE state;
	INT numargs;
	HRESULT status;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	V_VT (&empty) = VT_NULL;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_GetFolder (task_service, NULL, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	task_name = SysAllocString (APP_SKIPUAC_NAME);

	status = ITaskFolder_GetTask (task_folder, task_name, &registered_task);

	if (FAILED (status))
		goto CleanupExit;

	status = _r_skipuac_checkmodulepath (registered_task);

	if (FAILED (status))
		goto CleanupExit;

	// set arguments for task
	arga = CommandLineToArgvW (_r_sys_getcommandline (), &numargs);

	if (arga)
	{
		if (numargs > 1)
		{
			for (INT i = 1; i < numargs; i++)
			{
				_r_str_appendformat (arguments, RTL_NUMBER_OF (arguments), L"%s ", arga[i]);
			}

			StrTrimW (arguments, L" ");
		}

		LocalFree (arga);
	}

	if (!_r_str_isempty2 (arguments))
	{
		task_args = SysAllocString (arguments);

		params.vt = VT_BSTR;
		params.bstrVal = task_args;
	}
	else
	{
		params = empty;
	}

	status = IRegisteredTask_RunEx (registered_task, params, TASK_RUN_AS_SELF, 0, NULL, &running_task);

	if (FAILED (status))
		goto CleanupExit;

	status = E_ABORT;

	// check if started succesfull
	do
	{
		IRunningTask_Refresh (running_task);

		if (SUCCEEDED (IRunningTask_get_State (running_task, &state)))
		{
			if (state == TASK_STATE_DISABLED)
			{
				status = SCHED_S_TASK_DISABLED;

				break;
			}
			else if (state == TASK_STATE_RUNNING)
			{
				status = S_OK;

				break;
			}
		}

		_r_sys_sleep (250);
	}
	while (--attempts);

CleanupExit:

	if (task_name)
		SysFreeString (task_name);

	if (task_args)
		SysFreeString (task_args);

	if (running_task)
		IRunningTask_Release (running_task);

	if (registered_task)
		IRegisteredTask_Release (registered_task);

	if (task_folder)
		ITaskFolder_Release (task_folder);

	if (task_service)
		ITaskService_Release (task_service);

	return SUCCEEDED (status);
}

//
// Theme
//

VOID _r_theme_cleanup (
	_In_ HWND hwnd,
	_In_ PR_THEME_CONTEXT context
)
{
	_r_wnd_removecontext (hwnd, 666);
	_r_wnd_removesubclass (hwnd);

	if (context->htheme)
		CloseThemeData (context->htheme);

	_r_mem_free (context);
}

VOID _r_theme_enable (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	_r_config_setboolean (L"IsDarkThemeEnabled", is_enable);

	_r_theme_initialize (hwnd, is_enable);
}

BOOL CALLBACK _r_theme_enumchildwindows (
	_In_ HWND hwnd,
	_In_ LPARAM context
)
{
	WINDOWPLACEMENT pos = {0};
	COLORSCHEME scheme = {0};
	COMBOBOXINFO info = {0};
	PR_STRING class_name;
	LONG_PTR ex_style;
	LONG_PTR style;
	HWND htip;
	BOOLEAN is_enable;

	is_enable = !!context;

	class_name = _r_wnd_getclassname (hwnd);

	if (!class_name)
		return TRUE;

	if (_r_str_isequal2 (&class_name->sr, L"#32770", TRUE))
	{
		_r_theme_initialize (hwnd, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, L"DirectUIHWND", TRUE))
	{
		GetWindowPlacement (GetParent (hwnd), &pos);

		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
			_r_theme_setdarkmode (hwnd, is_enable);

		SetWindowPlacement (GetParent (hwnd), &pos);
	}
	//else if (_r_str_isequal2 (&class_name->sr, PROGRESS_CLASSW, TRUE))
	//{
	//	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
	//		_r_theme_setdarkmode (hwnd, is_enable);
	//
	//	_r_progress_setbarcolor (hwnd, 0, is_enable ? WND_TEXT_CLR : CLR_DEFAULT);
	//	_r_progress_setbkcolor (hwnd, 0, is_enable ? WND_BACKGROUND2_CLR : CLR_DEFAULT);
	//}
	else if (_r_str_isequal2 (&class_name->sr, REBARCLASSNAMEW, TRUE))
	{
		_r_theme_initializecontext (hwnd, NULL, &_r_theme_rebar_subclass, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, STATUSCLASSNAMEW, TRUE))
	{
		_r_theme_initializecontext (hwnd, VSCLASS_STATUS, &_r_theme_statusbar_subclass, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, TOOLBARCLASSNAMEW, TRUE))
	{
		scheme.dwSize = sizeof (COLORSCHEME);

		scheme.clrBtnHighlight = is_enable ? WND_TEXT_CLR : CLR_DEFAULT;
		scheme.clrBtnShadow = is_enable ? WND_TEXT_CLR : CLR_DEFAULT;

		_r_toolbar_setcolorscheme (hwnd, 0, &scheme);

		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
		{
			htip = _r_toolbar_gettooltips (hwnd, 0);

			if (htip)
				_r_theme_setdarkmode (htip, is_enable);
		}
	}
	else if (_r_str_isequal2 (&class_name->sr, UPDOWN_CLASSW, TRUE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
			_r_theme_setdarkmode (hwnd, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_BUTTONW, TRUE))
	{
		style = _r_wnd_getstyle (hwnd, GWL_STYLE);

		switch (style & BS_TYPEMASK)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			case BS_PUSHBUTTON:
			case BS_DEFPUSHBUTTON:
			case BS_SPLITBUTTON:
			case BS_DEFSPLITBUTTON:
			{
				if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
					_r_theme_setdarkmode (hwnd, is_enable);

				break;
			}

			case BS_GROUPBOX:
			{
				_r_theme_initializecontext (hwnd, VSCLASS_BUTTON, &_r_theme_groupbox_subclass, is_enable);
				break;
			}
		}
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_COMBOBOXW, TRUE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
		{
			style = _r_wnd_getstyle (hwnd, GWL_STYLE);

			if ((style & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST || (style & CBS_DROPDOWN) == CBS_DROPDOWN)
			{
				info.cbSize = sizeof (COMBOBOXINFO);

				if (GetComboBoxInfo (hwnd, &info))
				{
					// dark scrollbar for listbox of combobox
					if (info.hwndList)
						_r_theme_setdarkmode (info.hwndList, is_enable);
				}
			}
		}

		_r_theme_initializecontext (hwnd, VSCLASS_COMBOBOX, &_r_theme_combobox_subclassproc, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_EDITW, TRUE))
	{
		ex_style = _r_wnd_getstyle (hwnd, GWL_EXSTYLE);
		style = _r_wnd_getstyle (hwnd, GWL_STYLE);

		GetWindowPlacement (hwnd, &pos);

		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
			_r_theme_setdarkmode (hwnd, is_enable); // dark scrollbar for edit control

		if ((style & WS_BORDER) == WS_BORDER || (ex_style & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE)
			_r_theme_initializecontext (hwnd, NULL, &_r_theme_edit_subclass, is_enable);

		SetWindowPlacement (hwnd, &pos);

		_r_wnd_sendmessage (hwnd, 0, WM_THEMECHANGED, 0, 0);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_HEADERW, TRUE))
	{
		SetWindowTheme (hwnd, is_enable ? L"DarkMode_ItemsView" : L"Explorer", NULL);

		_r_theme_initializecontext (hwnd, VSCLASS_HEADER, &_r_theme_header_subclass, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_LISTVIEWW, TRUE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
		{
			_r_theme_setdarkmode (hwnd, is_enable);

			htip = _r_listview_gettooltips (hwnd, 0);

			if (htip)
				_r_theme_setdarkmode (htip, is_enable);
		}

		_r_wnd_sendmessage (hwnd, 0, LVM_SETTEXTBKCOLOR, 0, is_enable ? WND_BACKGROUND_CLR : GetSysColor (COLOR_WINDOW));
		_r_wnd_sendmessage (hwnd, 0, LVM_SETBKCOLOR, 0, is_enable ? WND_BACKGROUND_CLR : GetSysColor (COLOR_WINDOW));
		_r_wnd_sendmessage (hwnd, 0, LVM_SETTEXTCOLOR, 0, is_enable ? WND_TEXT_CLR : GetSysColor (COLOR_WINDOWTEXT));
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_SCROLLBARW, TRUE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
			_r_theme_setdarkmode (hwnd, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_TABCONTROLW, TRUE))
	{
		_r_theme_initializecontext (hwnd, VSCLASS_HEADER, &_r_theme_tabcontrol_subclass, is_enable);
	}
	else if (_r_str_isequal2 (&class_name->sr, WC_TREEVIEWW, TRUE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
		{
			_r_theme_setdarkmode (hwnd, is_enable);

			htip = _r_treeview_gettooltips (hwnd, 0);

			if (htip)
				_r_theme_setdarkmode (htip, is_enable);
		}

		_r_wnd_sendmessage (hwnd, 0, TVM_SETBKCOLOR, 0, is_enable ? WND_BACKGROUND_CLR : -1);
		_r_wnd_sendmessage (hwnd, 0, TVM_SETTEXTCOLOR, 0, is_enable ? WND_TEXT_CLR : -1);
	}

	InvalidateRect (hwnd, NULL, FALSE);

	_r_obj_dereference (class_name);

	return TRUE;
}

VOID _r_theme_initialize (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SPAM _SetPreferredAppMode = NULL;
	static FMT _FlushMenuThemes = NULL;

	PVOID huxtheme;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
		{
			status = _r_sys_loadlibrary (L"uxtheme.dll", 0, &huxtheme);

			if (NT_SUCCESS (status))
			{
				_SetPreferredAppMode = (SPAM)_r_sys_getprocaddress (huxtheme, NULL, 135);
				_FlushMenuThemes = (FMT)_r_sys_getprocaddress (huxtheme, NULL, 136);

				//_r_sys_freelibrary (huxtheme);
			}
		}

		app_global.theme.bg_brush = CreateSolidBrush (WND_BACKGROUND_CLR);

		_r_initonce_end (&init_once);
	}

	if (_SetPreferredAppMode)
		_SetPreferredAppMode (is_enable ? AllowDark : Default);

	if (_FlushMenuThemes)
		_FlushMenuThemes ();

	_r_theme_setwindowframe (hwnd, is_enable);

	_r_wnd_setsubclass (hwnd, GWLP_WNDPROC, &_r_theme_subclassproc);

	EnumChildWindows (hwnd, &_r_theme_enumchildwindows, is_enable);

	InvalidateRect (hwnd, NULL, FALSE);

	DrawMenuBar (hwnd); // HACK!
}

VOID _r_theme_initializecontext (
	_In_ HWND hwnd,
	_In_opt_ LPCWSTR class_list,
	_In_ WNDPROC subclass_proc,
	_In_ BOOLEAN is_enable
)
{
	PR_THEME_CONTEXT context;

	context = _r_wnd_getcontext (hwnd, 666);

	if (context)
	{
		_r_wnd_removecontext (hwnd, 666);

		_r_mem_free (context);
	}

	_r_wnd_removesubclass (hwnd);

	if (is_enable)
	{
		context = _r_mem_allocate (sizeof (R_THEME_CONTEXT));

		if (class_list)
			context->htheme = _r_dc_openthemedata (hwnd, class_list, _r_dc_getwindowdpi (hwnd));

		context->pt.x = LONG_MIN;
		context->pt.y = LONG_MIN;

		_r_wnd_setcontext (hwnd, 666, context);

		context->wnd_proc = _r_wnd_setsubclass (hwnd, GWLP_WNDPROC, subclass_proc);
	}
}

BOOLEAN _r_theme_isenabled ()
{
	return _r_config_getboolean (L"IsDarkThemeEnabled", _r_wnd_isdarkmodeenabled ());
}

VOID _r_theme_setwindowframe (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_RS5))
	{
		if (FAILED (DwmSetWindowAttribute (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &(BOOL){ is_enable }, sizeof (BOOL))))
			DwmSetWindowAttribute (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE - 1, &(BOOL){ is_enable }, sizeof (BOOL));
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_11))
	{
		if (_r_config_getboolean (L"IsWindowBorderEnabled", TRUE))
			DwmSetWindowAttribute (hwnd, DWMWA_BORDER_COLOR, &(COLORREF){ is_enable ? WND_BORDER_CLR : DWMWA_COLOR_DEFAULT}, sizeof (COLORREF));

		DwmSetWindowAttribute (hwnd, DWMWA_CAPTION_COLOR, &(COLORREF){ is_enable ? WND_BACKGROUND_CLR : DWMWA_COLOR_DEFAULT}, sizeof (COLORREF));
		DwmSetWindowAttribute (hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &(DWM_WINDOW_CORNER_PREFERENCE){ DWMWCP_DONOTROUND }, sizeof (DWM_WINDOW_CORNER_PREFERENCE));
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_11_22H2))
		DwmSetWindowAttribute (hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &(ULONG){ DWMSBT_NONE }, sizeof (ULONG));
}

VOID _r_theme_comboboxrender (
	_In_ PR_THEME_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ HDC hdc_buffer,
	_In_ PRECT client_rect,
	_In_ WNDPROC wnd_proc
)
{
	COMBOBOXINFO info = {0};
	RECT rect;
	SIZE size = {0};
	PR_STRING string;
	LONG_PTR style;
	INT index;

	style = _r_wnd_getstyle (hwnd, GWL_STYLE);

	// exclude rect
	if ((style & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST || (style & CBS_DROPDOWN) != CBS_DROPDOWN)
	{
		info.cbSize = sizeof (COMBOBOXINFO);

		if (CallWindowProcW (wnd_proc, hwnd, CB_GETCOMBOBOXINFO, 0, (LPARAM)&info))
			ExcludeClipRect (hdc, info.rcItem.left, info.rcItem.top, info.rcItem.right, info.rcItem.bottom);
	}

	SetRect (&rect, 0, 0, client_rect->right - client_rect->left, client_rect->bottom - client_rect->top);

	SetBkMode (hdc_buffer, TRANSPARENT);
	SetTextColor (hdc_buffer, WND_TEXT_CLR);

	SelectObject (hdc_buffer, (HGDIOBJ)CallWindowProcW (wnd_proc, hwnd, WM_GETFONT, 0, 0));

	_r_dc_fillrect (hdc_buffer, client_rect, WND_BACKGROUND2_CLR);

	_r_dc_framerect (hdc_buffer, client_rect, PtInRect (client_rect, context->pt) || GetFocus () == hwnd ? WND_HOT_CLR : WND_HIGHLIGHT_CLR);

	if (context->htheme)
	{
		GetThemePartSize (context->htheme, hdc_buffer, CP_DROPDOWNBUTTONRIGHT, CBXSR_NORMAL, NULL, TS_TRUE, &size);

		rect.left = client_rect->right - size.cy;

		DrawThemeBackground (context->htheme, hdc_buffer, CP_DROPDOWNBUTTONRIGHT, CBXSR_DISABLED, &rect, NULL);

		rect.left = 0;
	}
	else
	{
		DrawFrameControl (hdc_buffer, &rect, DFC_SCROLL, DFCS_SCROLLDOWN);
	}

	if ((style & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST)
	{
		index = _r_combobox_getcurrentitem (hwnd, 0);

		if (index == CB_ERR)
			return;

		string = _r_combobox_getitemtext (hwnd, 0, index);

		if (!string)
			return;

		rect.left += 6;
		rect.right -= 14;

		_r_dc_drawtext (context->htheme, hdc_buffer, &string->sr, &rect, CP_DROPDOWNITEM, CBXSR_NORMAL, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, WND_TEXT_CLR);

		_r_obj_dereference (string);
	}
}

LRESULT CALLBACK _r_theme_combobox_subclassproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_THEMECHANGED:
		{
			if (context->htheme)
				CloseThemeData (context->htheme);

			context->htheme = _r_dc_openthemedata (hwnd, VSCLASS_COMBOBOX, _r_dc_getwindowdpi (hwnd));

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_MOUSEMOVE:
		{
			context->pt.x = GET_X_LPARAM (lparam);
			context->pt.y = GET_Y_LPARAM (lparam);

			break;
		}

		case WM_MOUSELEAVE:
		{
			context->pt.x = LONG_MIN;
			context->pt.y = LONG_MIN;

			break;
		}

		case WM_PAINT:
		{
			RECT rect = {0};
			HBITMAP holdbitmap;
			HBITMAP hbitmap;
			HDC hdc_buffer;
			HDC hdc;

			GetClientRect (hwnd, &rect);

			hdc = GetDC (hwnd);

			hdc_buffer = CreateCompatibleDC (hdc);

			hbitmap = CreateCompatibleBitmap (hdc, rect.right, rect.bottom);
			holdbitmap = SelectObject (hdc_buffer, hbitmap);

			_r_theme_comboboxrender (context, hwnd, hdc, hdc_buffer, &rect, wnd_proc);

			BitBlt (hdc, rect.left, rect.top, rect.right, rect.bottom, hdc_buffer, 0, 0, SRCCOPY);

			SelectObject (hdc_buffer, holdbitmap);
			DeleteObject (hbitmap);

			DeleteDC (hdc_buffer);
			ReleaseDC (hwnd, hdc);

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK _r_theme_edit_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_DPICHANGED:
		case WM_DPICHANGED_AFTERPARENT:
		{
			InvalidateRect (hwnd, NULL, FALSE);
			break;
		}

		case WM_NCPAINT:
		{
			RECT rect = {0};
			RECT inner;
			POINT pt = {0};
			HDC hdc;
			LONG_PTR style;
			LONG dpi_value;
			COLORREF clr;

			// The searchbox control does it's own theme drawing.
			if (_r_wnd_getcontext (hwnd, SHORT_MAX))
				break;

			CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			hdc = GetWindowDC (hwnd);

			if (hdc)
			{
				dpi_value = _r_dc_getwindowdpi (hwnd);

				GetClientRect (hwnd, &rect);

				rect.right += 2 * _r_dc_getsystemmetrics (SM_CXEDGE, dpi_value);

				style = _r_wnd_getstyle (hwnd, GWL_STYLE);

				if ((style & WS_HSCROLL) == WS_HSCROLL)
					rect.right += _r_dc_getsystemmetrics (SM_CYHSCROLL, dpi_value);

				rect.bottom += 2 * _r_dc_getsystemmetrics (SM_CYEDGE, dpi_value);

				if ((style & WS_VSCROLL) == WS_VSCROLL)
					rect.right += _r_dc_getsystemmetrics (SM_CXVSCROLL, dpi_value);

				inner = rect;

				InflateRect (&inner, -1, -1);

				_r_dc_framerect (hdc, &inner, WND_BACKGROUND_CLR);

				GetCursorPos (&pt);
				ScreenToClient (hwnd, &pt);

				clr = ((context->is_mouseactive && PtInRect (&rect, pt)) || GetFocus () == hwnd) ? WND_HOT_CLR : WND_BACKGROUND2_CLR;

				_r_dc_framerect (hdc, &rect, clr);

				ReleaseDC (hwnd, hdc);

				return 0;
			}

			break;
		}

		case WM_NCCALCSIZE:
		{
			LPRECT rect;
			LONG_PTR style;
			LONG dpi_value;

			// The searchbox control does it's own theme drawing.
			if (_r_wnd_getcontext (hwnd, SHORT_MAX))
				break;

			rect = (LPRECT)lparam;

			dpi_value = _r_dc_getwindowdpi (hwnd);

			InflateRect (rect, -_r_dc_getsystemmetrics (SM_CXEDGE, dpi_value), -_r_dc_getsystemmetrics (SM_CYEDGE, dpi_value));

			style = _r_wnd_getstyle (hwnd, GWL_STYLE);

			if ((style & WS_VSCROLL) == WS_VSCROLL)
				rect->right -= _r_dc_getsystemmetrics (SM_CXVSCROLL, dpi_value);

			if ((style & WS_HSCROLL) == WS_HSCROLL)
				rect->bottom -= _r_dc_getsystemmetrics (SM_CYHSCROLL, dpi_value);

			return 0;
		}

		case WM_PAINT:
		{
			HBITMAP holdbitmap;
			HBITMAP hbitmap;
			PR_STRING string;
			RECT rect;
			HDC buffer_dc;
			HDC hdc;

			string = _r_edit_getcuebanner (hwnd, 0);

			if (
				_r_obj_isstringempty (string) ||
				GetFocus () == hwnd ||
				CallWindowProcW (wnd_proc, hwnd, WM_GETTEXTLENGTH, 0, 0) > 0
				)
			{
				if (string)
					_r_obj_dereference (string);

				return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
			}

			hdc = (HDC)wparam ? (HDC)wparam : GetDC (hwnd);

			if (hdc)
			{
				GetClientRect (hwnd, &rect);

				buffer_dc = CreateCompatibleDC (hdc);
				hbitmap = CreateCompatibleBitmap (hdc, rect.right, rect.bottom);

				holdbitmap = SelectObject (buffer_dc, hbitmap);

				SetBkMode (buffer_dc, TRANSPARENT);

				if (_r_theme_isenabled ())
				{
					SetTextColor (buffer_dc, RGB (170, 170, 170));

					_r_dc_fillrect (buffer_dc, &rect, WND_BACKGROUND2_CLR);
				}
				else
				{
					SetTextColor (buffer_dc, GetSysColor (COLOR_GRAYTEXT));

					_r_dc_fillrect (buffer_dc, &rect, GetSysColor (COLOR_WINDOW));
				}

				_r_dc_fixfont (buffer_dc, hwnd, 0);

				rect.left += 2;

				_r_dc_drawtext (NULL, buffer_dc, &string->sr, &rect, 0, 0, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP, 0);

				rect.left -= 2;

				BitBlt (hdc, rect.left, rect.top, rect.right, rect.bottom, buffer_dc, 0, 0, SRCCOPY);

				SelectObject (buffer_dc, holdbitmap);

				DeleteObject (hbitmap);
				DeleteDC (buffer_dc);

				if (!(HDC)wparam)
					ReleaseDC (hwnd, hdc);
			}

			if (string)
				_r_obj_dereference (string);

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}

		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};

			if (GetFocus () == hwnd)
				break;

			if (!context->is_mouseactive)
			{
				context->is_mouseactive = TRUE;

				InvalidateRect (hwnd, NULL, FALSE);
			}

			tme.cbSize = sizeof (TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hwnd;
			tme.dwHoverTime = HOVER_DEFAULT;

			TrackMouseEvent (&tme);

			break;
		}

		case WM_MOUSELEAVE:
		{
			TRACKMOUSEEVENT tme = {0};

			if (context->is_mouseactive)
			{
				context->is_mouseactive = FALSE;

				InvalidateRect (hwnd, NULL, FALSE);
			}

			tme.cbSize = sizeof (TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE | TME_CANCEL;
			tme.hwndTrack = hwnd;
			tme.dwHoverTime = HOVER_DEFAULT;

			TrackMouseEvent (&tme);

			break;
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

VOID _r_theme_drawgroupbox (
	_In_opt_ HTHEME htheme,
	_In_ PPAINTSTRUCT ps,
	_In_ HWND hwnd
)
{
	RECT rc_background;
	RECT rc_content;
	RECT rc_text;
	RECT rect;
	SIZE size = {0};
	PR_STRING string;
	LONG_PTR style;
	ULONG flags;
	INT state_id;
	BOOLEAN is_disabled;

	string = _r_ctrl_getstring (hwnd, 0);

	if (!string)
		string = _r_obj_referenceemptystring ();

	_r_dc_fixfont (ps->hdc, hwnd, 0);

	GetClientRect (hwnd, &rect);

	rc_background = rect;
	rc_text = rect;

	_r_dc_getfontwidth (ps->hdc, &string->sr, &size);

	rc_background.top += size.cy / 2;

	style = _r_wnd_getstyle (hwnd, GWL_STYLE);

	is_disabled = (style & WS_DISABLED) == WS_DISABLED;

	rc_text.left += ((style & BS_CENTER) == BS_CENTER) ? ((rc_background.right - rc_background.left - size.cx) / 2) : 8;
	rc_text.bottom = rc_text.top + size.cy;
	rc_text.right = rc_text.left + size.cx + 4;

	ExcludeClipRect (ps->hdc, rc_text.left, rc_text.top, rc_text.right, rc_text.bottom);

	state_id = is_disabled ? GBS_DISABLED : GBS_NORMAL;

	if (htheme)
	{
		rc_content = rc_background;

		GetThemeBackgroundContentRect (htheme, ps->hdc, BP_GROUPBOX, state_id, &rc_background, &rc_content);

		ExcludeClipRect (ps->hdc, rc_content.left, rc_content.top, rc_content.right, rc_content.bottom);
	}

	_r_dc_framerect (ps->hdc, &rc_background, WND_BACKGROUND2_CLR);

	SelectClipRgn (ps->hdc, NULL);

	flags = (((style & BS_CENTER) == BS_CENTER) ? DT_CENTER : DT_LEFT) | DT_SINGLELINE;

	if (_r_ctrl_getuistate (hwnd, 0))
		flags |= DT_HIDEPREFIX;

	rc_text.right -= 2;
	rc_text.left += 2;

	_r_dc_drawtext (htheme, ps->hdc, &string->sr, &rc_text, BP_GROUPBOX, state_id, flags, is_disabled ? WND_GRAYTEXT_CLR : WND_TEXT_CLR);

	_r_obj_dereference (string);
}

LRESULT CALLBACK _r_theme_groupbox_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_THEMECHANGED:
		{
			if (context->htheme)
				CloseThemeData (context->htheme);

			context->htheme = _r_dc_openthemedata (hwnd, VSCLASS_BUTTON, _r_dc_getwindowdpi (hwnd));

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};

			if (BeginPaint (hwnd, &ps))
			{
				_r_theme_drawgroupbox (context->htheme, &ps, hwnd);

				EndPaint (hwnd, &ps);
			}

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

VOID _r_theme_headercontrol (
	_In_ PR_THEME_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT rect
)
{
	HDITEMW hdi = {0};
	WCHAR buffer[128] = {0};
	R_STRINGREF sr;
	RECT arrow_rect;
	RECT head_rect;
	SIZE arrow_size = {0};
	INT flags = DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX | DT_WORD_ELLIPSIS;
	INT state_id = 0;
	INT count;

	SetBkMode (hdc, TRANSPARENT);

	SetTextColor (hdc, WND_TEXT_CLR);

	_r_dc_fixfont (hdc, hwnd, 0);

	_r_dc_fillrect (hdc, rect, WND_BACKGROUND_CLR);

	count = (INT)CallWindowProcW (context->wnd_proc, hwnd, HDM_GETITEMCOUNT, 0, 0);

	for (INT i = 0; i < count; i++)
	{
		RtlZeroMemory (&head_rect, sizeof (head_rect));

		if (!CallWindowProcW (context->wnd_proc, hwnd, HDM_GETITEMRECT, (WPARAM)i, (LPARAM)&head_rect))
			continue;

		if (PtInRect (&head_rect, context->pt))
		{
			_r_dc_fillrect (hdc, &head_rect, WND_HOT_CLR);
		}
		else
		{
			_r_dc_fillrect (hdc, &head_rect, WND_BACKGROUND_CLR);

			if (context->htheme)
			{
				DrawThemeEdge (context->htheme, hdc, HP_HEADERITEM, HIS_NORMAL, &head_rect, BDR_RAISEDOUTER, BF_RIGHT, NULL); // vista+
			}
			else
			{
				DrawEdge (hdc, &head_rect, BDR_RAISEDOUTER, BF_RIGHT);
			}
		}

		RtlZeroMemory (&hdi, sizeof (hdi));

		hdi.mask = HDI_TEXT | HDI_FORMAT;
		hdi.pszText = buffer;
		hdi.cchTextMax = RTL_NUMBER_OF (buffer);

		if (!CallWindowProcW (context->wnd_proc, hwnd, HDM_GETITEM, (WPARAM)i, (LPARAM)&hdi))
			break;

		if (hdi.fmt & HDF_SORTUP)
		{
			state_id = HSAS_SORTEDUP;
		}
		else if (hdi.fmt & HDF_SORTDOWN)
		{
			state_id = HSAS_SORTEDDOWN;
		}

		if ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP || (hdi.fmt & HDF_SORTDOWN) == HDF_SORTDOWN)
		{
			if (context->htheme)
			{
				arrow_rect = head_rect;

				if (SUCCEEDED (GetThemePartSize (context->htheme, hdc, HP_HEADERSORTARROW, state_id, NULL, TS_TRUE, &arrow_size))) // vista+
					arrow_rect.bottom = arrow_size.cy;

				DrawThemeBackground (context->htheme, hdc, HP_HEADERSORTARROW, state_id, &arrow_rect, NULL); // vista+
			}
		}

		if (hdi.fmt & HDF_RIGHT)
		{
			flags |= DT_RIGHT;
		}
		else if (hdi.fmt & HDF_CENTER)
		{
			flags |= DT_CENTER;
		}
		else
		{
			flags |= DT_LEFT;
		}

		head_rect.left += 4;
		head_rect.right -= 8;

		_r_obj_initializestringref (&sr, buffer);

		_r_dc_drawtext (NULL, hdc, &sr, &head_rect, 0, 0, flags, 0);
	}
}

LRESULT CALLBACK _r_theme_header_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_THEMECHANGED:
		{
			if (context->htheme)
				CloseThemeData (context->htheme);

			context->htheme = _r_dc_openthemedata (hwnd, VSCLASS_HEADER, _r_dc_getwindowdpi (hwnd));

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_CONTEXTMENU:
		{
			LRESULT result;

			result = CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			InvalidateRect (hwnd, NULL, TRUE);

			return result;
		}

		case WM_PAINT:
		{
			RECT rect = {0};
			HBITMAP holdbitmap;
			HBITMAP hbitmap;
			HDC hdc_buffer;
			HDC hdc;

			GetClientRect (hwnd, &rect);

			hdc = GetDC (hwnd);

			hdc_buffer = CreateCompatibleDC (hdc);
			hbitmap = CreateCompatibleBitmap (hdc, rect.right, rect.bottom);

			holdbitmap = SelectObject (hdc_buffer, hbitmap);

			_r_theme_headercontrol (context, hwnd, hdc_buffer, &rect);

			BitBlt (hdc, rect.left, rect.top, rect.right, rect.bottom, hdc_buffer, 0, 0, SRCCOPY);

			SelectObject (hdc_buffer, holdbitmap);

			DeleteObject (hbitmap);
			DeleteDC (hdc_buffer);

			ReleaseDC (hwnd, hdc);

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}

		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};

			if (GetCapture () == hwnd)
				break;

			if (!context->is_mouseactive)
			{
				tme.cbSize = sizeof (TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;

				TrackMouseEvent (&tme);

				context->is_mouseactive = TRUE;
			}

			context->pt.x = GET_X_LPARAM (lparam);
			context->pt.y = GET_Y_LPARAM (lparam);

			InvalidateRect (hwnd, NULL, FALSE);

			break;
		}

		case WM_MOUSELEAVE:
		{
			LRESULT result;

			result = CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			context->is_mouseactive = FALSE;
			context->pt.x = LONG_MIN;
			context->pt.y = LONG_MIN;

			InvalidateRect (hwnd, NULL, TRUE);

			return result;
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK _r_theme_rebar_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wparam;

			SetBkMode (hdc, TRANSPARENT);
			SetTextColor (hdc, WND_TEXT_CLR);
			SetDCBrushColor (hdc, WND_BACKGROUND2_CLR);

			return (LRESULT)GetStockObject (DC_BRUSH);
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;

			SetBkMode (hdc, TRANSPARENT);
			SetTextColor (hdc, WND_TEXT_CLR);
			SetBkColor (hdc, WND_BACKGROUND_CLR);

			return (LRESULT)app_global.theme.bg_brush;
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK _r_theme_statusbar_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_THEMECHANGED:
		{
			if (context->htheme)
				CloseThemeData (context->htheme);

			context->htheme = _r_dc_openthemedata (hwnd, VSCLASS_STATUS, _r_dc_getwindowdpi (hwnd));

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			RECT intersect = {0};
			RECT rect = {0};
			RECT part = {0};
			SIZE grip = {0};
			RECT divider;
			PR_STRING string;
			HDC hdc;
			LONG borders[3] = {0};
			LONG parts;
			INT ctrl_id;

			hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				_r_dc_fixfont (hdc, hwnd, 0);

				GetClientRect (hwnd, &rect);

				_r_dc_fillrect (hdc, &ps.rcPaint, WND_BACKGROUND_CLR);

				parts = _r_status_getparts (hwnd, 0);

				ctrl_id = GetDlgCtrlID (hwnd);

				_r_status_getborders (hwnd, 0, &borders);

				for (LONG i = 0; i < parts; i++)
				{
					if (!_r_status_getrect (hwnd, 0, i, &part))
						continue;

					if (!IntersectRect (&intersect, &part, &ps.rcPaint))
						continue;

					SetBkMode (hdc, TRANSPARENT);
					SetTextColor (hdc, WND_TEXT_CLR);

					part.left += borders[2];
					part.right -= borders[1];

					string = _r_status_gettext (hwnd, 0, i, NULL);

					if (string)
					{
						_r_dc_drawtext (context->htheme, hdc, &string->sr, &part, SP_PANE, 0, DT_SINGLELINE | DT_VCENTER | DT_LEFT, WND_TEXT_CLR);

						_r_obj_dereference (string);
					}

					if (i < parts)
					{
						SetRect (&divider, part.right - borders[1], part.top, part.right, part.bottom);

						_r_dc_fillrect (hdc, &divider, WND_BACKGROUND2_CLR);
					}
				}

				if (_r_wnd_getstyle (GetParent (hwnd), GWL_STYLE) & WS_SIZEBOX)
				{
					GetThemePartSize (context->htheme, hdc, SP_GRIPPER, 0, &rect, TS_DRAW, &grip);

					intersect = rect;

					intersect.left = intersect.right - grip.cx;
					intersect.top = intersect.bottom - grip.cy;

					if (context->htheme)
					{
						DrawThemeBackground (context->htheme, hdc, SP_GRIPPER, 0, &intersect, &intersect);
					}
					else
					{
						DrawFrameControl (hdc, &intersect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
					}
				}

				EndPaint (hwnd, &ps);
			}

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

VOID _r_theme_rendertabcontrol (
	_In_ PR_THEME_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ PRECT client_rect
)
{
	TCITEMW tci = {0};
	WCHAR buffer[128] = {0};
	RECT rect = {0};
	R_STRINGREF sr;
	INT current_item;
	INT count;

	SetBkMode (hdc, TRANSPARENT);
	SetTextColor (hdc, WND_TEXT_CLR);

	_r_dc_fixfont (hdc, hwnd, 0);

	_r_dc_fillrect (hdc, client_rect, WND_BACKGROUND_CLR);

	current_item = _r_tab_getcurrentitem (hwnd, 0);
	count = _r_tab_getitemcount (hwnd, 0);

	for (INT i = 0; i < count; i++)
	{
		_r_wnd_sendmessage (hwnd, 0, TCM_GETITEMRECT, (WPARAM)i, (LPARAM)&rect);

		if (PtInRect (&rect, context->pt))
		{
			OffsetRect (&rect, 2, 2);

			_r_dc_fillrect (hdc, &rect, WND_HOT_CLR);
		}
		else
		{
			OffsetRect (&rect, 2, 2);

			_r_dc_fillrect (hdc, &rect, (current_item == i) ? WND_BACKGROUND2_CLR : WND_BACKGROUND_CLR);
		}

		tci.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
		tci.dwStateMask = TCIS_BUTTONPRESSED | TCIS_HIGHLIGHTED;
		tci.pszText = buffer;
		tci.cchTextMax = RTL_NUMBER_OF (buffer);

		if (_r_wnd_sendmessage (hwnd, 0, TCM_GETITEM, (WPARAM)i, (LPARAM)&tci))
		{
			_r_obj_initializestringref (&sr, tci.pszText);

			_r_dc_drawtext (NULL, hdc, &sr, &rect, 0, 0, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX, 0);
		}
	}
}

LRESULT CALLBACK _r_theme_tabcontrol_subclass (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PR_THEME_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, 666);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_theme_cleanup (hwnd, context);
			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_MOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};

			if (GetCapture () == hwnd)
				break;

			if (!context->is_mouseactive)
			{
				tme.cbSize = sizeof (TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;

				TrackMouseEvent (&tme);

				context->is_mouseactive = TRUE;
			}

			context->pt.x = GET_X_LPARAM (lparam);
			context->pt.y = GET_Y_LPARAM (lparam);

			InvalidateRect (hwnd, NULL, FALSE);

			break;
		}

		case WM_MOUSELEAVE:
		{
			context->pt.x = LONG_MIN;
			context->pt.y = LONG_MIN;

			context->is_mouseactive = FALSE;

			InvalidateRect (hwnd, NULL, FALSE);

			break;
		}

		case WM_PAINT:
		{
			RECT client_rect;
			RECT rect;
			HDC buffer_dc;
			HDC hdc;
			HBITMAP hold_bitmap;
			HBITMAP hbitmap;

			GetClientRect (hwnd, &client_rect);

			hdc = GetDC (hwnd);

			buffer_dc = CreateCompatibleDC (hdc);
			hbitmap = CreateCompatibleBitmap (hdc, client_rect.right, client_rect.bottom);
			hold_bitmap = SelectObject (buffer_dc, hbitmap);

			rect = client_rect;

			_r_wnd_sendmessage (hwnd, 0, TCM_ADJUSTRECT, 0, (LPARAM)&rect);

			ExcludeClipRect (hdc, rect.left, rect.top, rect.right, rect.bottom);

			_r_theme_rendertabcontrol (context, hwnd, buffer_dc, &client_rect);

			BitBlt (hdc, client_rect.left, client_rect.top, client_rect.right, client_rect.bottom, buffer_dc, 0, 0, SRCCOPY);

			SelectObject (buffer_dc, hold_bitmap);

			DeleteObject (hbitmap);
			DeleteDC (buffer_dc);

			ReleaseDC (hwnd, hdc);

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}

ULONG _r_theme_getbuttondrawstyle (
	_In_ HWND hwnd,
	_In_ LONG_PTR style
)
{
	ULONG flags = DT_LEFT; // DT_LEFT is 0

	switch (style & BS_TYPEMASK)
	{
		case BS_PUSHBUTTON:
		case BS_DEFPUSHBUTTON:
		case BS_SPLITBUTTON:
		case BS_DEFSPLITBUTTON:
		{
			style |= BS_CENTER | DT_VCENTER;
			break;
		}
	}

	flags |= (style & BS_MULTILINE) ? DT_WORDBREAK : DT_SINGLELINE;
	flags |= ((style & BS_CENTER) == BS_CENTER) ? DT_CENTER : (style & BS_RIGHT) ? DT_RIGHT : 0;
	flags |= ((style & BS_VCENTER) == BS_VCENTER) ? DT_VCENTER : (style & BS_BOTTOM) ? DT_BOTTOM : 0;
	flags |= (_r_ctrl_getuistate (hwnd, 0) & UISF_HIDEACCEL) ? DT_HIDEPREFIX : 0;

	if (!(style & BS_MULTILINE) && !(style & BS_BOTTOM) && !(style & BS_TOP))
		flags |= DT_VCENTER;

	return flags;
}

VOID _r_theme_rendercheckbox (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ HTHEME htheme,
	_In_ HDC hdc,
	_In_ PR_STRING string,
	_In_ INT part_id,
	_In_ INT state_id,
	_In_ LONG_PTR style
)
{
	LOGFONT lf = {0};
	RECT rect_text = {0};
	RECT rect_focus;
	RECT rect_client;
	RECT rect_bg;
	SIZE size = {0};
	HFONT hnew_font = NULL;
	HFONT hold_font = NULL;
	HFONT hfont = NULL;
	COLORREF clr_text;
	ULONG flags;
	HRESULT status;

	if (SUCCEEDED (GetThemeFont (htheme, hdc, part_id, state_id, TMT_FONT, &lf)))
	{
		hnew_font = CreateFontIndirectW (&lf);

		hfont = hnew_font;
	}

	if (!hfont)
		hfont = _r_ctrl_getfont (draw_info->hdr.hwndFrom, 0);

	if (hfont)
		hold_font = SelectObject (hdc, hfont);

	status = GetThemeBackgroundContentRect (htheme, hdc, part_id, state_id, &draw_info->rc, &rect_text);

	if (FAILED (status))
		rect_text = draw_info->rc;

	rect_bg = draw_info->rc;

	GetThemePartSize (htheme, hdc, part_id, state_id, NULL, TS_DRAW, &size);

	flags = _r_theme_getbuttondrawstyle (draw_info->hdr.hwndFrom, style);

	if (flags & DT_SINGLELINE)
		rect_bg.top += (_r_calc_rectheight (&rect_text) - size.cy) / 2;

	rect_bg.bottom = rect_bg.top + size.cy;
	rect_bg.right = rect_bg.left + size.cx;

	rect_text.left = rect_bg.right + 4;

	rect_client = draw_info->rc;

	DrawThemeParentBackground (draw_info->hdr.hwndFrom, hdc, &rect_client);

	DrawThemeBackground (htheme, hdc, part_id, state_id, &rect_bg, NULL);

	clr_text = style & WS_DISABLED ? WND_GRAYTEXT_CLR : WND_TEXT_CLR;

	_r_dc_drawtext (htheme, hdc, &string->sr, &rect_text, part_id, state_id, flags, clr_text);

	if (draw_info->uItemState & BST_FOCUS)
	{
		_r_dc_drawtext (htheme, hdc, &string->sr, &rect_text, part_id, state_id, flags | DTT_CALCRECT, clr_text);

		rect_focus = rect_text;

		InflateRect (&rect_focus, -GetSystemMetrics (SM_CXEDGE), -GetSystemMetrics (SM_CYEDGE));

		DrawFocusRect (hdc, &rect_focus);
	}

	if (hnew_font)
		DeleteObject (hnew_font);

	if (hold_font)
		SelectObject (hdc, hold_font);
}

VOID _r_theme_paintcheckbox (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ HTHEME htheme,
	_In_ PR_STRING string,
	_In_ INT part_id,
	_In_ INT state_id,
	_In_ LONG_PTR style
)
{
	BP_ANIMATIONPARAMS anim_params = {0};
	HANIMATIONBUFFER hanimation;
	HDC hdc_from = NULL;
	HDC hdc_to = NULL;

	if (BufferedPaintRenderAnimation (draw_info->hdr.hwndFrom, draw_info->hdc))
		return;

	anim_params.cbSize = sizeof (BP_ANIMATIONPARAMS);
	anim_params.style = BPAS_LINEAR;

	GetThemeTransitionDuration (htheme, part_id, state_id, state_id, TMT_TRANSITIONDURATIONS, &anim_params.dwDuration);

	hanimation = BeginBufferedAnimation (draw_info->hdr.hwndFrom, draw_info->hdc, &draw_info->rc, BPBF_COMPATIBLEBITMAP, NULL, &anim_params, &hdc_from, &hdc_to);

	if (hanimation)
	{
		if (hdc_from)
			_r_theme_rendercheckbox (draw_info, htheme, hdc_from, string, part_id, state_id, style);

		if (hdc_to)
			_r_theme_rendercheckbox (draw_info, htheme, hdc_to, string, part_id, state_id, style);

		EndBufferedAnimation (hanimation, TRUE);
	}
	else
	{
		_r_theme_rendercheckbox (draw_info, htheme, draw_info->hdc, string, part_id, state_id, style);
	}
}

VOID _r_theme_drawicon (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ PR_STRING string,
	_In_ HICON hicon,
	_In_ LPCRECT rect,
	_In_ LONG dpi_value
)
{
	BUTTON_IMAGELIST btn_imagelist = {0};
	ICONINFO ii = {0};
	BITMAP bmp = {0};
	SIZE size = {0};

	size.cx = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	size.cy = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

	if (GetIconInfo (hicon, &ii))
	{
		if (ii.hbmColor)
		{
			if (GetObjectW (ii.hbmColor, sizeof (BITMAP), &bmp))
			{
				size.cx = bmp.bmWidth;
				size.cy = bmp.bmHeight;
			}

			DeleteObject (ii.hbmColor);
		}
		else if (ii.hbmMask)
		{
			if (GetObjectW (ii.hbmMask, sizeof (BITMAP), &bmp))
			{
				size.cx = bmp.bmWidth;
				size.cy = bmp.bmHeight / 2;
			}

			DeleteObject (ii.hbmMask);
		}

		DrawIconEx (
			draw_info->hdc,
			_r_str_getlength2 (&string->sr) > 1 ? rect->left + (size.cx / 2) : rect->left + (_r_calc_rectwidth (rect) - size.cx) / 2,
			rect->top + (_r_calc_rectheight (rect) - size.cy) / 2,
			hicon,
			size.cx,
			size.cy,
			0,
			NULL,
			DI_NORMAL
		);
	}
	else
	{
		// HACK
		if (_r_ctrl_getbuttonimagelist (draw_info->hdr.hwndFrom, 0, &btn_imagelist) && btn_imagelist.himl)
		{
			ImageList_GetIconSize (btn_imagelist.himl, &size.cx, &size.cy);

			_r_imagelist_draw (
				btn_imagelist.himl,
				0,
				draw_info->hdc,
				_r_str_getlength2 (&string->sr) > 1 ? rect->left + (size.cx / 2) : rect->left + (_r_calc_rectwidth (rect) - size.cx) / 2,
				rect->top + (_r_calc_rectheight (rect) - size.cy) / 2,
				size.cx,
				size.cy,
				0,
				0,
				ILD_NORMAL,
				TRUE
			);
		}
	}
}

VOID _r_theme_drawsplitglyph (
	_In_ const PBUTTON_SPLITINFO split_info,
	_In_ HDC hdc,
	_Inout_ LPRECT rect,
	_In_ LONG dpi_value
)
{
	RECT dropdown_rect;
	LOGFONTW logfont = {0};
	HFONT old_font;
	HFONT hfont;
	INT width;
	INT height;

	dropdown_rect = *rect;

	// The dropdown takes priority if the client rect is too small, it will only have a dropdown
	if (split_info->uSplitStyle & BCSS_ALIGNLEFT)
	{
		dropdown_rect.right = min (rect->left + split_info->size.cx, rect->right);

		rect->right += _r_dc_getdpi (8, dpi_value);
	}
	else
	{
		dropdown_rect.left = max (rect->right - split_info->size.cy, rect->left);

		rect->left -= _r_dc_getdpi (8, dpi_value);
	}

	if (split_info->mask & BCSIF_IMAGE)
	{
		// When the glyph is an image list, Windows is very buggy with BCSS_STRETCH,
		// positions it weirdly and doesn't even stretch it, but instead extends the
		// image, leaking into other images in the list (or black if none). Instead,
		// we'll ignore this and just position it at center as without BCSS_STRETCH.
		if (!ImageList_GetIconSize (split_info->himlGlyph, &width, &height))
			return;

		ImageList_Draw (
			split_info->himlGlyph,
			0,
			hdc,
			rect->left + (_r_calc_rectwidth (rect) - width) / 2,
			rect->top + (_r_calc_rectheight (rect) - height) / 2,
			ILD_NORMAL
		);
	}
	else if (split_info->size.cy >= 0)
	{
		logfont.lfQuality = ANTIALIASED_QUALITY;
		logfont.lfCharSet = SYMBOL_CHARSET;
		logfont.lfWeight = FW_NORMAL;

		_r_str_copy (logfont.lfFaceName, RTL_NUMBER_OF (logfont.lfFaceName), L"Marlett");

		if (split_info->size.cy)
		{
			// BCSS_STRETCH preserves aspect ratio, uses minimum as size
			if (split_info->uSplitStyle & BCSS_STRETCH)
			{
				logfont.lfHeight = min (split_info->size.cx, split_info->size.cy);
			}
			else
			{
				logfont.lfWidth = split_info->size.cx;
				logfont.lfHeight = split_info->size.cy;
			}
		}
		else
		{
			logfont.lfHeight = split_info->size.cx;
		}

		hfont = CreateFontIndirectW (&logfont);

		if (hfont)
		{
			SetBkMode (hdc, TRANSPARENT);
			SetTextColor (hdc, WND_TEXT_CLR);

			old_font = SelectObject (hdc, hfont);

			dropdown_rect.top = _r_dc_getdpi (2, dpi_value);
			dropdown_rect.left -= _r_dc_getdpi (10, dpi_value);

			DrawTextW (hdc, L"u", 1, &dropdown_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX);

			SelectObject (hdc, old_font);
			DeleteObject (hfont);
		}
	}
}

LRESULT CALLBACK _r_theme_drawbutton (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ LONG_PTR style
)
{
	switch (draw_info->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			BUTTON_SPLITINFO split_info = {0};
			PR_STRING string;
			RECT rect_content = {0};
			RECT rect_client = {0};
			HTHEME htheme;
			HICON hicon;
			COLORREF clr = WND_BUTTON_CLR;
			LONG dpi_value;
			ULONG flags;
			INT state_id = PBS_NORMAL;

			dpi_value = _r_dc_getwindowdpi (draw_info->hdr.hwndFrom);

			htheme = _r_dc_openthemedata (draw_info->hdr.hwndFrom, VSCLASS_BUTTON, dpi_value);

			if (!htheme)
				return CDRF_DODEFAULT;

			switch (style & BS_TYPEMASK)
			{
				case BS_PUSHBUTTON:
				case BS_DEFPUSHBUTTON:
				case BS_SPLITBUTTON:
				case BS_DEFSPLITBUTTON:
				{
					break;
				}

				default:
				{
					return CDRF_DODEFAULT;
				}
			}

			if ((draw_info->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
			{
				state_id = PBS_PRESSED;
			}
			else if ((draw_info->uItemState & CDIS_DISABLED) == CDIS_DISABLED)
			{
				state_id = PBS_DISABLED;
			}
			else if ((draw_info->uItemState & CDIS_FOCUS) == CDIS_FOCUS || (draw_info->uItemState & CDIS_HOT) == CDIS_HOT)
			{
				state_id = PBS_HOT;
			}
			else if ((draw_info->uItemState & CDIS_DEFAULT) == CDIS_DEFAULT)
			{
				state_id = PBS_DEFAULTED;
			}

			GetClientRect (draw_info->hdr.hwndFrom, &rect_client);

			flags = _r_theme_getbuttondrawstyle (draw_info->hdr.hwndFrom, style);

			if (IsThemeBackgroundPartiallyTransparent (htheme, BP_PUSHBUTTON, state_id))
				DrawThemeParentBackground (draw_info->hdr.hwndFrom, draw_info->hdc, NULL);

			rect_content = rect_client;

			GetThemeBackgroundContentRect (htheme, draw_info->hdc, BP_PUSHBUTTON, state_id, &rect_client, &rect_content);

			string = _r_ctrl_getstring (draw_info->hdr.hwndFrom, 0);

			if (!string)
				string = _r_obj_referenceemptystring ();

			if ((draw_info->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
			{
				clr = WND_GRAYTEXT_CLR;
			}
			else if ((draw_info->uItemState & CDIS_HOT) == CDIS_HOT)
			{
				clr = WND_HOT_CLR;
			}

			_r_dc_fillrect (draw_info->hdc, &draw_info->rc, clr);

			if (((style & BS_TYPEMASK) & BS_SPLITBUTTON) == BS_SPLITBUTTON || ((style & BS_TYPEMASK) & BS_DEFSPLITBUTTON) == BS_DEFSPLITBUTTON)
			{
				if (_r_ctrl_getbuttonsplitinfo (draw_info->hdr.hwndFrom, 0, &split_info))
					_r_theme_drawsplitglyph (&split_info, draw_info->hdc, &rect_content, dpi_value);
			}

			hicon = _r_ctrl_geticon (draw_info->hdr.hwndFrom, 0);

			if (hicon)
				_r_theme_drawicon (draw_info, string, hicon, &rect_content, dpi_value);

			_r_dc_drawtext (htheme, draw_info->hdc, &string->sr, &rect_content, BP_PUSHBUTTON, state_id, flags, (style & WS_DISABLED) ? WND_GRAYTEXT_CLR : WND_TEXT_CLR);

			if ((draw_info->uItemState & CDIS_FOCUS) == CDIS_FOCUS)
			{
				DrawFocusRect (draw_info->hdc, &draw_info->rc);
			}
			else
			{
				_r_dc_framerect (draw_info->hdc, &draw_info->rc, WND_BACKGROUND2_CLR);
			}

			_r_obj_dereference (string);

			CloseThemeData (htheme);

			return CDRF_SKIPDEFAULT;
		}
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK _r_theme_drawcheckbox (
	_In_ LPNMCUSTOMDRAW draw_info,
	_In_ LONG_PTR style
)
{
	switch (draw_info->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			PR_STRING string;
			HTHEME htheme;
			LONG dpi_value;
			INT part_id;
			INT state_id = RBS_UNCHECKEDNORMAL;
			BOOLEAN is_checked;

			switch (style & BS_TYPEMASK)
			{
				case BS_CHECKBOX:
				case BS_AUTOCHECKBOX:
				case BS_3STATE:
				case BS_AUTO3STATE:
				{
					part_id = BP_CHECKBOX;
					break;
				}

				case BS_RADIOBUTTON:
				case BS_AUTORADIOBUTTON:
				{
					part_id = BP_RADIOBUTTON;
					break;
				}

				default:
				{
					return CDRF_DODEFAULT;
				}
			}

			is_checked = _r_ctrl_isbuttonchecked (draw_info->hdr.hwndFrom, 0);

			// states of BP_CHECKBOX and BP_RADIOBUTTON are the same
			if ((draw_info->uItemState & CDIS_DISABLED) == CDIS_DISABLED)
			{
				state_id = is_checked ? RBS_CHECKEDDISABLED : RBS_UNCHECKEDDISABLED;
			}
			else if ((draw_info->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
			{
				state_id = is_checked ? RBS_CHECKEDPRESSED : RBS_UNCHECKEDPRESSED;
			}
			else if ((draw_info->uItemState & CDIS_HOT) == CDIS_HOT || (draw_info->uItemState & CDIS_FOCUS) == CDIS_FOCUS)
			{
				state_id = is_checked ? RBS_CHECKEDHOT : RBS_UNCHECKEDHOT;
			}
			else
			{
				state_id = is_checked ? RBS_CHECKEDNORMAL : RBS_UNCHECKEDNORMAL;
			}

			dpi_value = _r_dc_getwindowdpi (draw_info->hdr.hwndFrom);

			htheme = _r_dc_openthemedata (draw_info->hdr.hwndFrom, VSCLASS_BUTTON, dpi_value);

			if (!htheme)
				return CDRF_DODEFAULT;

			string = _r_ctrl_getstring (draw_info->hdr.hwndFrom, 0);

			if (!string)
				string = _r_obj_referenceemptystring ();

			_r_theme_paintcheckbox (draw_info, htheme, string, part_id, state_id, style);

			_r_obj_dereference (string);

			CloseThemeData (htheme);

			return CDRF_SKIPDEFAULT;
		}
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK _r_theme_drawrebar (
	_In_ LPNMCUSTOMDRAW draw_info
)
{
	// Temp chevron workaround until location/state supported.
	if (draw_info->dwDrawStage != CDDS_PREPAINT)
		return CDRF_DODEFAULT;

	switch (draw_info->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			SetTextColor (draw_info->hdc, WND_TEXT_CLR);

			_r_dc_fillrect (draw_info->hdc, &draw_info->rc, WND_BACKGROUND_CLR);

			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			return CDRF_SKIPDEFAULT;
		}
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK _r_theme_drawtoolbar (
	_In_ LPNMTBCUSTOMDRAW draw_info
)
{
	switch (draw_info->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPREPAINT:
		{
			draw_info->clrText = WND_TEXT_CLR;
			draw_info->clrBtnHighlight = WND_HIGHLIGHT_CLR;
			draw_info->clrHighlightHotTrack = WND_HOT_CLR;

			draw_info->nStringBkMode = TRANSPARENT;
			draw_info->nHLStringBkMode = TRANSPARENT;

			return TBCDRF_HILITEHOTTRACK | TBCDRF_USECDCOLORS;
		}
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK _r_theme_drawlistviewgroup (
	_In_ LPNMLVCUSTOMDRAW draw_info
)
{
	switch (draw_info->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			LVGROUP lvg = {0};
			R_STRINGREF sr;
			ULONG flags = DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX;
			LONG dpi_value;

			dpi_value = _r_dc_getwindowdpi (draw_info->nmcd.hdr.hwndFrom);

			SetBkMode (draw_info->nmcd.hdc, TRANSPARENT);

			_r_dc_fixfont (draw_info->nmcd.hdc, draw_info->nmcd.hdr.hwndFrom, 0);

			lvg.cbSize = sizeof (LVGROUP);
			lvg.mask = LVGF_HEADER | LVGF_ALIGN;

			if (_r_wnd_sendmessage (draw_info->nmcd.hdr.hwndFrom, 0, LVM_GETGROUPINFO, draw_info->nmcd.dwItemSpec, (LPARAM)&lvg) != INT_ERROR)
			{
				SetTextColor (draw_info->nmcd.hdc, WND_TEXT_CLR);

				draw_info->rcText.top += _r_dc_getdpi (2, dpi_value);
				draw_info->rcText.bottom -= _r_dc_getdpi (2, dpi_value);

				_r_dc_fillrect (draw_info->nmcd.hdc, &draw_info->rcText, WND_BUTTON_CLR);

				draw_info->rcText.top -= _r_dc_getdpi (2, dpi_value);
				draw_info->rcText.bottom += _r_dc_getdpi (2, dpi_value);

				if (lvg.pszHeader)
				{
					draw_info->rcText.left += _r_dc_getdpi (10, dpi_value);

					if (lvg.uAlign & LVGA_HEADER_LEFT)
					{
						flags |= DT_LEFT;
					}
					else if (lvg.uAlign & LVGA_HEADER_CENTER)
					{
						flags |= DT_CENTER;
					}
					else if (lvg.uAlign & LVGA_HEADER_RIGHT)
					{
						flags |= DT_RIGHT;
					}

					_r_obj_initializestringref (&sr, lvg.pszHeader);

					_r_dc_drawtext (NULL, draw_info->nmcd.hdc, &sr, &draw_info->rcText, 0, 0, flags, 0);

					draw_info->rcText.left -= _r_dc_getdpi (10, dpi_value);
				}
			}

			return CDRF_SKIPDEFAULT;
		}
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK _r_theme_subclassproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	WNDPROC wnd_proc;

	wnd_proc = _r_wnd_getsubclass (hwnd);

	if (!wnd_proc)
		return FALSE;

	if (!_r_theme_isenabled ())
		return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_r_wnd_removesubclass (hwnd);
			break;
		}

		case WM_UAHDRAWMENU:
		{
			MENUBARINFO mbi = {0};
			LPUAHMENU menu_item;
			RECT rect;

			menu_item = (LPUAHMENU)lparam;

			mbi.cbSize = sizeof (mbi);

			if (!GetWindowRect (hwnd, &rect))
				break;

			// get the menubar rect
			if (!GetMenuBarInfo (hwnd, OBJID_MENU, 0, &mbi))
				break;

			// the rcBar is offset by the window rect
			OffsetRect (&mbi.rcBar, -rect.left, -rect.top);

			mbi.rcBar.top -= 1;

			_r_dc_fillrect (menu_item->hdc, &mbi.rcBar, WND_BACKGROUND_CLR);

			return TRUE;
		}

		case WM_UAHDRAWMENUITEM:
		{
			LPUAHDRAWMENUITEM menu_item;
			PR_STRING string;
			HTHEME htheme;
			ULONG flags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
			COLORREF bg_clr = WND_BACKGROUND_CLR;
			COLORREF text_clr = WND_TEXT_CLR;
			INT state_id = MBI_NORMAL; // normal display (default)

			menu_item = (LPUAHDRAWMENUITEM)lparam;

			string = _r_menu_getitemtext (menu_item->um.hmenu, menu_item->umi.iPosition, TRUE);

			if (!string)
				break;

			if (menu_item->dis.itemState & ODS_HOTLIGHT)
			{
				state_id = MBI_HOT; // hot tracking

				bg_clr = WND_HIGHLIGHT_CLR;
			}

			if (menu_item->dis.itemState & ODS_SELECTED)
			{
				state_id = MBI_PUSHED; // clicked

				bg_clr = WND_HIGHLIGHT_CLR;
			}

			if ((menu_item->dis.itemState & ODS_GRAYED) || (menu_item->dis.itemState & ODS_DISABLED))
				state_id = MBI_DISABLED; // disabled

			if (menu_item->dis.itemState & ODS_NOACCEL)
				flags |= DT_HIDEPREFIX;

			htheme = _r_dc_openthemedata (hwnd, VSCLASS_MENU, _r_dc_getwindowdpi (hwnd));

			if (htheme)
			{
				if (state_id == MBI_NORMAL || state_id == MBI_HOT || state_id == MBI_PUSHED || state_id == MBI_DISABLED)
				{
					_r_dc_fillrect (menu_item->um.hdc, &menu_item->dis.rcItem, bg_clr);
				}
				else
				{
					DrawThemeBackground (htheme, menu_item->um.hdc, MENU_BARITEM, state_id, &menu_item->dis.rcItem, NULL);
				}

				if (state_id == MBI_DISABLED || state_id == MBI_DISABLEDHOT || state_id == MBI_DISABLEDPUSHED)
					text_clr = WND_GRAYTEXT_CLR;

				_r_dc_drawtext (htheme, menu_item->um.hdc, &string->sr, &menu_item->dis.rcItem, MENU_BARITEM, state_id, flags, text_clr);

				CloseThemeData (htheme);
			}

			_r_obj_dereference (string);

			return TRUE;
		}

		case WM_NOTIFY:
		{
			LPNMHDR data = (LPNMHDR)lparam;

			switch (data->code)
			{
				case NM_CUSTOMDRAW:
				{
					LPNMCUSTOMDRAW custom_draw;
					LPNMLVCUSTOMDRAW lvcd;
					WCHAR class_name[128];
					LONG_PTR style;

					custom_draw = (LPNMCUSTOMDRAW)lparam;

					if (!GetClassNameW (custom_draw->hdr.hwndFrom, class_name, RTL_NUMBER_OF (class_name)))
						break;

					if (_r_str_compare (class_name, WC_BUTTONW, 0) == 0)
					{
						// The control does it's own theme drawing.
						if (_r_wnd_getcontext (custom_draw->hdr.hwndFrom, SHORT_MAX))
							break;

						style = _r_wnd_getstyle (custom_draw->hdr.hwndFrom, GWL_STYLE);

						switch (style & BS_TYPEMASK)
						{
							case BS_PUSHBUTTON:
							case BS_DEFPUSHBUTTON:
							case BS_SPLITBUTTON:
							case BS_DEFSPLITBUTTON:
							{
								return _r_theme_drawbutton (custom_draw, style);
							}

							case BS_CHECKBOX:
							case BS_AUTOCHECKBOX:
							case BS_3STATE:
							case BS_AUTO3STATE:
							case BS_RADIOBUTTON:
							case BS_AUTORADIOBUTTON:
							{
								return _r_theme_drawcheckbox (custom_draw, style);
							}
						}
					}
					else if (_r_str_compare (class_name, REBARCLASSNAMEW, 0) == 0)
					{
						return _r_theme_drawrebar (custom_draw);
					}
					else if (_r_str_compare (class_name, TOOLBARCLASSNAMEW, 0) == 0)
					{
						return _r_theme_drawtoolbar ((LPNMTBCUSTOMDRAW)custom_draw);
					}
					else if (_r_str_compare (class_name, WC_LISTVIEWW, 0) == 0)
					{
						lvcd = (LPNMLVCUSTOMDRAW)custom_draw;

						if (lvcd->dwItemType == LVCDI_GROUP)
							return _r_theme_drawlistviewgroup (lvcd);
					}

					break;
				}
			}

			break;
		}

		case WM_SYNCPAINT:
		{
			RedrawWindow (hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
			break;
		}

		case WM_PRINTCLIENT:
		{
			return TRUE;
		}

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wparam;

			SetBkMode (hdc, TRANSPARENT);
			SetTextColor (hdc, WND_TEXT_CLR);
			SetDCBrushColor (hdc, WND_BACKGROUND2_CLR);

			return (LRESULT)GetStockObject (DC_BRUSH);
		}

		case WM_CTLCOLORBTN:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORLISTBOX:
		{
			HDC hdc = (HDC)wparam;

			SetBkMode (hdc, TRANSPARENT);
			SetTextColor (hdc, WND_TEXT_CLR);
			SetBkColor (hdc, WND_BACKGROUND_CLR);

			return (LRESULT)app_global.theme.bg_brush;
		}

		case WM_NCACTIVATE:
		case WM_NCPAINT:
		{
			RECT client_rect;
			RECT rect;
			RECT line;
			HDC hdc;
			LRESULT result;

			result = CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			if (GetMenu (hwnd))
			{
				GetClientRect (hwnd, &client_rect);
				GetWindowRect (hwnd, &rect);

				MapWindowPoints (hwnd, NULL, (PPOINT)&client_rect, 2);

				OffsetRect (&client_rect, -rect.left, -rect.top);

				// the rcBar is offset by the window rect (thanks to adzm) (dmex)
				line = client_rect;

				line.bottom = line.top;
				line.top -= 1;

				hdc = GetWindowDC (hwnd);

				if (hdc)
				{
					_r_dc_fillrect (hdc, &line, WND_BACKGROUND_CLR);

					ReleaseDC (hwnd, hdc);
				}
			}

			return result;
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}
