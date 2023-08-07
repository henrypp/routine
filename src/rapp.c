// routine.c
// project sdk library
//
// Copyright (c) 2012-2023 Henry++

#include "rapp.h"

//
// Application: seh
//

#if !defined(_DEBUG)
VOID _r_app_exceptionfilter_savedump (
	_In_ PEXCEPTION_POINTERS exception_ptr
)
{
	MINIDUMP_EXCEPTION_INFORMATION minidump_info = {0};
	WCHAR dump_path[512];
	LONG64 current_time;
	HANDLE hfile;
	NTSTATUS status;

	current_time = _r_unixtime_now ();

	_r_str_printf (
		dump_path,
		RTL_NUMBER_OF (dump_path),
		L"%s\\%s-%" TEXT (PR_LONG64) L".dmp",
		_r_app_getcrashdirectory (TRUE),
		_r_app_getnameshort (),
		current_time
	);

	status = _r_fs_createfile (dump_path, FILE_OVERWRITE_IF, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL, 0, FALSE, NULL, &hfile);

	if (!_r_fs_isvalidhandle (hfile))
		return;

	minidump_info.ThreadId = HandleToUlong (NtCurrentThreadId ());
	minidump_info.ExceptionPointers = exception_ptr;
	minidump_info.ClientPointers = FALSE;

	MiniDumpWriteDump (
		NtCurrentProcess (),
		HandleToUlong (NtCurrentProcessId ()),
		hfile,
		MiniDumpNormal,
		&minidump_info,
		NULL,
		NULL
	);

	NtClose (hfile);
}

LONG NTAPI _r_app_exceptionfilter_callback (
	_In_ PEXCEPTION_POINTERS exception_ptr
)
{
#if !defined(APP_CONSOLE)
	R_ERROR_INFO error_info;
#endif // !APP_CONSOLE

	_r_app_exceptionfilter_savedump (exception_ptr);

#if defined(APP_CONSOLE)
	_r_console_writestringformat (
		APP_EXCEPTION_TITLE L" 0x%08X\r\n",
		exception_ptr->ExceptionRecord->ExceptionCode
	);
#else
	_r_error_initialize_ex (&error_info, NULL, NULL, exception_ptr);

	_r_show_errormessage (NULL, APP_EXCEPTION_TITLE, exception_ptr->ExceptionRecord->ExceptionCode, &error_info);
#endif // APP_CONSOLE

	_r_sys_exitprocess (exception_ptr->ExceptionRecord->ExceptionCode);

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif // !_DEBUG

//
// Application: common
//

LPCWSTR _r_app_getmutexname ()
{
#if defined(APP_NO_MUTEX)
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_name = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_name = _r_format_string (
			L"%s-%" TEXT (PR_ULONG) L"-%" TEXT (PR_ULONG),
			_r_app_getnameshort (),
			_r_str_gethash (_r_sys_getimagepath (), TRUE),
			_r_str_gethash (_r_sys_getimagecommandline (), TRUE)
		);

		_r_initonce_end (&init_once);
	}

	return cached_name->buffer;
#else
	return _r_app_getnameshort ();
#endif // !APP_NO_MUTEX
}

BOOLEAN _r_app_isportable ()
{
#if !defined(APP_NO_APPDATA) || !defined(APP_CONSOLE)
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_portable = FALSE;

	PR_STRING string;
	PR_STRING directory;
	LPCWSTR file_names[] = {L"portable", _r_app_getnameshort ()};
	LPCWSTR file_exts[] = {L"dat", L"ini"};

	C_ASSERT (sizeof (file_names) == sizeof (file_exts));

	if (_r_initonce_begin (&init_once))
	{
		if (_r_sys_getopt (_r_sys_getimagecommandline (), L"portable", NULL))
		{
			is_portable = TRUE;
		}
		else
		{
			directory = _r_app_getdirectory ();

			for (SIZE_T i = 0; i < RTL_NUMBER_OF (file_names); i++)
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
#else
	return TRUE;
#endif // !APP_NO_APPDATA || !APP_CONSOLE
}

BOOLEAN _r_app_isreadonly ()
{
#if !defined(APP_NO_CONFIG) || !defined(APP_CONSOLE)
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_readonly = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		is_readonly = _r_sys_getopt (_r_sys_getimagecommandline (), L"readonly", NULL);

		_r_initonce_end (&init_once);
	}

	return is_readonly;
#else
	return TRUE;
#endif // !APP_NO_CONFIG || !APP_CONSOLE
}

BOOLEAN _r_app_initialize_com ()
{
	HRESULT status;

	// initialize COM library
	status = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED (status))
	{
#if defined(APP_CONSOLE)
		_r_console_writestringformat (APP_FAILED_COM_INITIALIZE L" 0x%08" TEXT (PRIX32) L"!\r\n", status);
#else
		_r_show_errormessage (NULL, APP_FAILED_COM_INITIALIZE, status, NULL);
#endif // APP_CONSOLE

		return FALSE;
	}

	return TRUE;
}

#if !defined(APP_CONSOLE)
VOID _r_app_initialize_components ()
{
	// set locale information
	if (app_global.locale.default_name)
		_r_obj_dereference (app_global.locale.default_name);

	app_global.locale.resource_name = _r_obj_createstring (APP_LANGUAGE_DEFAULT);

	_r_app_initialize_locale ();

	// register TaskbarCreated message
#if defined(APP_HAVE_TRAY)
	if (!app_global.main.taskbar_msg)
		app_global.main.taskbar_msg = RegisterWindowMessage (L"TaskbarCreated");
#endif // APP_HAVE_TRAY

	// set updates path
#if defined(APP_HAVE_UPDATES)
	// initialize objects
	app_global.update.info.components = _r_obj_createarray (sizeof (R_UPDATE_COMPONENT), NULL);
#endif // APP_HAVE_UPDATES

#if defined(APP_HAVE_SETTINGS)
	app_global.settings.page_list = _r_obj_createarray (sizeof (R_SETTINGS_PAGE), NULL);
#endif // APP_HAVE_SETTINGS
}
#endif // !APP_CONSOLE

#if !defined(APP_CONSOLE)
VOID _r_app_initialize_controls ()
{
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;

	InitCommonControlsEx (&icex);
}
#endif // !APP_CONSOLE

VOID _r_app_initialize_dll ()
{
	// Safe DLL loading
	// This prevents DLL planting in the application directory.

	SetDllDirectory (L"");

	// win7+
	SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

	SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
}

#if !defined(APP_CONSOLE)
VOID _r_app_initialize_locale ()
{
	ULONG status;

	_r_obj_clearreference (&app_global.locale.default_name);

	status = _r_sys_getlocaleinfo (GetUserDefaultUILanguage (), LOCALE_SENGLISHLANGUAGENAME, &app_global.locale.default_name);

	if (status != ERROR_SUCCESS)
		_r_sys_getlocaleinfo (LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLISHLANGUAGENAME, &app_global.locale.default_name);
}
#endif // !APP_CONSOLE

#if !defined(_DEBUG)
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
#endif // !_DEBUG

BOOLEAN _r_app_initialize ()
{
#if !defined(APP_CONSOLE)
	R_STRINGREF sr;
#endif // !APP_CONSOLE

	// set main thread name (win10rs1+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1607))
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
	// prevent app duplicates
	if (_r_mutex_isexists (_r_app_getmutexname ()))
	{
		_r_obj_initializestringref (&sr, _r_app_getname ());

		EnumWindows (&_r_util_activate_window_callback, (LPARAM)&sr);

		return FALSE;
	}

	_r_app_initialize_components ();

#if defined(APP_NO_GUEST)
	// use "only admin"-mode
	if (!_r_sys_iselevated ())
	{
		if (!_r_app_runasadmin ())
			_r_show_errormessage (NULL, APP_FAILED_ADMIN_RIGHTS, ERROR_DS_INSUFF_ACCESS_RIGHTS, NULL);

		return FALSE;
	}

#elif defined(APP_HAVE_SKIPUAC)
	// use "skipuac" feature
	if (!_r_sys_iselevated () && _r_skipuac_run ())
		return FALSE;
#endif // APP_NO_GUEST

	// set running flag
	_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex);

	// check for wow64 working and show warning if it is TRUE!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 () && !_r_sys_getopt (_r_sys_getimagecommandline (), L"nowow64", NULL))
	{
		_r_show_message (NULL, MB_OK | MB_ICONWARNING | MB_TOPMOST, APP_WARNING_WOW64_TITLE, APP_WARNING_WOW64_TEXT);

		return FALSE;
	}
#endif // !_DEBUG && !_WIN64
#endif // !APP_CONSOLE

	// if profile directory does not exist, we cannot save configuration
	if (!_r_app_isreadonly ())
		_r_app_getprofiledirectory (); // create profile directory if it does not exists

	return TRUE;
}

PR_STRING _r_app_getdirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_path = NULL;

	R_STRINGREF sr;

	if (_r_initonce_begin (&init_once))
	{
		_r_obj_initializestringrefconst (&sr, _r_sys_getimagepath ());

		cached_path = _r_path_getbasedirectory (&sr);

		_r_initonce_end (&init_once);
	}

	return cached_path;
}

PR_STRING _r_app_getconfigpath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	PR_STRING buffer;
	PR_STRING string;
	PR_STRING path;
	PR_STRING new_result;
	HANDLE hfile;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		new_result = NULL;

		// parse config path from arguments
		if (_r_sys_getopt (_r_sys_getimagecommandline (), L"ini", &buffer))
		{
			if (buffer)
			{
				_r_str_trimstring2 (buffer, L".\\/\" ", 0);

				status = _r_str_environmentexpandstring (&buffer->sr, &string);

				if (NT_SUCCESS (status))
					_r_obj_movereference (&buffer, string);

				if (!_r_obj_isstringempty2 (buffer))
				{
					if (PathGetDriveNumber (buffer->buffer) == -1)
					{
						path = _r_app_getdirectory ();

						string = _r_obj_concatstrings (
							3,
							path->buffer,
							L"\\",
							buffer->buffer
						);

						_r_obj_movereference (&new_result, string);
					}
					else
					{
						_r_obj_swapreference (&new_result, buffer);
					}

					// trying to create file
					if (!_r_fs_exists (new_result->buffer))
					{
						status = _r_fs_createfile (new_result->buffer, FILE_OPEN_IF, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL, 0, FALSE, NULL, &hfile);

						if (NT_SUCCESS (status))
						{
							NtClose (hfile);
						}
						else
						{
							_r_obj_clearreference (&new_result);
						}
					}
				}

				_r_obj_dereference (buffer);
			}
		}

		// get configuration path
		if (!new_result || !_r_fs_exists (new_result->buffer))
		{
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
			}
			else
			{
				string = _r_path_getknownfolder (CSIDL_APPDATA, L"\\" APP_AUTHOR L"\\" APP_NAME L"\\" APP_NAME_SHORT L".ini");
			}

			_r_obj_movereference (&new_result, string);
		}

		cached_result = new_result;

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

	if (is_create)
		_r_fs_mkdir (cached_path);

	return cached_path;
}

LPCWSTR _r_app_getcrashdirectory (
	_In_ BOOLEAN is_create
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR cached_path[512] = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_str_printf (cached_path, RTL_NUMBER_OF (cached_path), L"%s\\crashdump", _r_app_getprofiledirectory ()->buffer);

		_r_initonce_end (&init_once);
	}

	if (is_create)
		_r_fs_mkdir (cached_path);

	return cached_path;
}

#if !defined(APP_CONSOLE)
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
#endif // !APP_CONSOLE

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
			cached_path = _r_path_getknownfolder (CSIDL_APPDATA, L"\\" APP_AUTHOR L"\\" APP_NAME);
		}

		_r_initonce_end (&init_once);
	}

	if (cached_path)
		_r_fs_mkdir (cached_path->buffer);

	return cached_path;
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

#if !defined(APP_CONSOLE)
LRESULT CALLBACK _r_app_maindlgproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
#if defined(APP_HAVE_TRAY)
	if (app_global.main.taskbar_msg)
	{
		if (msg == app_global.main.taskbar_msg)
		{
			if (app_global.main.wnd_proc)
				return CallWindowProc (app_global.main.wnd_proc, hwnd, RM_TASKBARCREATED, 0, 0);

			return FALSE;
		}
	}
#endif // APP_HAVE_TRAY

	switch (msg)
	{
		case RM_LOCALIZE:
		{
			if (!app_global.main.wnd_proc)
				break;

			CallWindowProc (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);

			RedrawWindow (hwnd, NULL, NULL, RDW_ERASENOW | RDW_INVALIDATE);

			DrawMenuBar (hwnd); // HACK!!!

			break;
		}

		//case WM_NCDESTROY:
		//{
		//	SendMessage (hwnd, RM_UNINITIALIZE_POST, 0, 0);
		//	break;
		//}

		case WM_DESTROY:
		{
			_r_window_saveposition (hwnd, L"window");
			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
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

	if (!app_global.main.wnd_proc)
		return FALSE;

	return CallWindowProc (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);
}

ULONG _r_app_getshowcode (
	_In_ HWND hwnd
)
{
	ULONG show_code;
	BOOLEAN is_windowhidden;

	is_windowhidden = FALSE;

	if (NtCurrentPeb ()->ProcessParameters->WindowFlags & STARTF_USESHOWWINDOW)
	{
		show_code = NtCurrentPeb ()->ProcessParameters->ShowWindowFlags;
	}
	else
	{
		show_code = SW_SHOWNORMAL;
	}

	if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
		is_windowhidden = TRUE;

	// if window have tray - check arguments
#if defined(APP_HAVE_TRAY)
	if (!is_windowhidden)
	{
		if (_r_config_getboolean (L"IsStartMinimized", FALSE) || _r_sys_getopt (_r_sys_getimagecommandline (), L"minimized", NULL))
		{
			is_windowhidden = TRUE;
		}
	}
#endif // APP_HAVE_TRAY

	if (_r_wnd_getstyle (hwnd) & WS_MAXIMIZEBOX)
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
	_In_opt_ HINSTANCE hinstance,
	_In_ LPCWSTR dlg_name,
	_In_opt_ LPCWSTR icon_name,
	_In_ DLGPROC dlg_proc
)
{
	UINT messages[] = {
		WM_COPYDATA,
		WM_COPYGLOBALDATA,
		WM_DROPFILES,
#if defined(APP_HAVE_TRAY)
		app_global.main.taskbar_msg,
#endif // APP_HAVE_TRAY
	};

	HWND hwnd;
	LONG dpi_value;
	LONG icon_small;
	LONG icon_large;

#ifdef APP_HAVE_UPDATES
	// configure components
	WCHAR locale_version[64];

	_r_str_fromlong64 (locale_version, RTL_NUMBER_OF (locale_version), _r_locale_getversion ());

	_r_update_addcomponent (_r_app_getname (), _r_app_getnameshort (), _r_app_getversion (), _r_app_getdirectory (), TRUE);
	_r_update_addcomponent (L"Language pack", L"language", locale_version, _r_app_getlocalepath (), FALSE);
#endif // APP_HAVE_UPDATES

	// create main window
	hwnd = _r_wnd_createwindow (hinstance, dlg_name, NULL, dlg_proc, NULL);

	_r_app_sethwnd (hwnd);

	if (!hwnd)
	{
		RtlRaiseStatus (GetLastError ());

		return NULL;
	}

	// set window title
	_r_ctrl_setstring (hwnd, 0, _r_app_getname ());

	// set window icon
	if (icon_name)
	{
		dpi_value = _r_dc_getwindowdpi (hwnd);

		icon_small = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
		icon_large = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);

		_r_wnd_seticon (
			hwnd,
			_r_sys_loadsharedicon (hinstance, icon_name, icon_small),
			_r_sys_loadsharedicon (hinstance, icon_name, icon_large)
		);
	}

	// set window prop
	SetProp (hwnd, _r_app_getname (), IntToPtr (42));

	// set window on top
	_r_wnd_top (hwnd, _r_config_getboolean (L"AlwaysOnTop", FALSE));

	// center window position
	_r_wnd_center (hwnd, NULL);

	// enable messages bypass uipi (win7+)
	_r_wnd_changemessagefilter (hwnd, messages, RTL_NUMBER_OF (messages), MSGFLT_ALLOW);

	// subclass window
	app_global.main.wnd_proc = (WNDPROC)GetWindowLongPtr (hwnd, DWLP_DLGPROC);
	SetWindowLongPtr (hwnd, DWLP_DLGPROC, (LONG_PTR)_r_app_maindlgproc);

	// restore window position
	_r_window_restoreposition (hwnd, L"window");

	// common initialization
	SendMessage (hwnd, RM_INITIALIZE, 0, 0);
	SendMessage (hwnd, RM_LOCALIZE, 0, 0);

	// restore window visibility (or not?)
#if !defined(APP_STARTMINIMIZED)
	ShowWindow (hwnd, _r_app_getshowcode (hwnd));
#endif // !APP_STARTMINIMIZED

	PostMessage (hwnd, RM_INITIALIZE_POST, 0, 0);

#if defined(APP_HAVE_UPDATES)
	if (_r_update_isenabled (TRUE))
		_r_update_check (hwnd);
#endif // APP_HAVE_UPDATES

	return hwnd;
}

BOOLEAN _r_app_runasadmin ()
{
	_r_mutex_destroy (&app_global.main.hmutex);

#if defined(APP_HAVE_SKIPUAC)
	if (_r_skipuac_run ())
		return TRUE;
#endif // APP_HAVE_SKIPUAC

	if (_r_sys_runasadmin (_r_sys_getimagepath (), _r_sys_getimagecommandline (), _r_sys_getcurrentdirectory ()))
		return TRUE;

	// restore mutex on error
	_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex);

	_r_sys_sleep (500); // HACK!!! prevent loop

	return FALSE;
}

VOID _r_app_restart (
	_In_opt_ HWND hwnd
)
{
	LARGE_INTEGER timeout;
	HWND hmain;
	NTSTATUS status;

	if (_r_show_message (hwnd, MB_YESNO | MB_ICONQUESTION, NULL, APP_QUESTION_RESTART) != IDYES)
		return;

	_r_mutex_destroy (&app_global.main.hmutex);

	status = _r_sys_createprocess (_r_sys_getimagepath (), _r_sys_getimagecommandline (), _r_sys_getcurrentdirectory ());

	if (!NT_SUCCESS (status))
	{
		// restore mutex on error
		_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex);

		return;
	}

	hmain = _r_app_gethwnd ();

	if (hmain)
	{
		DestroyWindow (hmain);

		_r_calc_millisecondstolargeinteger (&timeout, 4000);

		NtWaitForSingleObject (hmain, FALSE, &timeout); // wait for exit
	}

	_r_sys_exitprocess (STATUS_SUCCESS);
}
#endif // !APP_CONSOLE

//
// Configuration
//

VOID _r_config_initialize ()
{
	PR_HASHTABLE config_table;
	PR_STRING path;

	config_table = NULL;

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

_Success_ (return != 0)
LONG _r_config_getlong (
	_In_ LPCWSTR key_name,
	_In_ LONG def_value
)
{
	LONG value;

	value = _r_config_getlong_ex (key_name, def_value, NULL);

	return value;
}

_Success_ (return != 0)
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

_Success_ (return != 0)
LONG64 _r_config_getlong64 (
	_In_ LPCWSTR key_name,
	_In_ LONG64 def_value
)
{
	LONG64 value;

	value = _r_config_getlong64_ex (key_name, def_value, NULL);

	return value;
}

_Success_ (return != 0)
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

_Success_ (return != 0)
ULONG _r_config_getulong (
	_In_ LPCWSTR key_name,
	_In_ ULONG def_value
)
{
	ULONG value;

	value = _r_config_getulong_ex (key_name, def_value, NULL);

	return value;
}

_Success_ (return != 0)
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

_Success_ (return != 0)
ULONG64 _r_config_getulong64 (
	_In_ LPCWSTR key_name,
	_In_ ULONG64 def_value
)
{
	ULONG64 value;

	value = _r_config_getulong64_ex (key_name, def_value, NULL);

	return value;
}

_Success_ (return != 0)
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
		_r_obj_initializestringref2 (&remaining_part, font_config);

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

	if (!_r_str_isempty2 (logfont->lfFaceName) && !_r_dc_isfontexists (logfont))
		logfont->lfFaceName[0] = UNICODE_NULL;

	if (!_r_str_isempty2 (logfont->lfFaceName) && logfont->lfHeight && logfont->lfWeight)
		return;

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
		_r_obj_initializestringref2 (&remaining_part, pair_config);

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
	PR_STRING config_value;
	NTSTATUS status;

	config_value = _r_config_getstring_ex (key_name, def_value, section_name);

	if (!config_value)
		return NULL;

	status = _r_str_environmentexpandstring (&config_value->sr, &string);

	if (NT_SUCCESS (status))
	{
		_r_obj_dereference (config_value);

		return string;
	}

	string = _r_obj_createstring2 (config_value);

	_r_obj_dereference (config_value);

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

	hash_code = _r_str_gethash2 (section_string, TRUE);

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

	if (value)
	{
		string = _r_str_environmentunexpandstring (value);
	}
	else
	{
		string = NULL;
	}

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

	WCHAR section_string[128];
	PR_STRING path;
	PR_STRING section_string_full;
	PR_OBJECT_POINTER object_ptr;
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

	hash_code = _r_str_gethash2 (section_string_full, TRUE);

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

	// write to configuration file
	if (_r_app_isreadonly ())
		return;

	path = _r_app_getconfigpath ();

	WritePrivateProfileString (section_string, key_name, _r_obj_getstring (object_ptr->object_body), path->buffer);
}

//
// Localization
//

#if !defined(APP_CONSOLE)
VOID _r_locale_initialize ()
{
	PR_HASHTABLE locale_table;
	PR_LIST locale_names;
	PR_STRING language_config;
	PR_STRING language_path;

	locale_table = NULL;

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

	_r_obj_movereference (&app_global.locale.table, locale_table);
	_r_obj_movereference (&app_global.locale.available_list, locale_names);

	_r_queuedlock_releaseexclusive (&app_global.locale.lock);
}

VOID _r_locale_apply (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
)
{
	PR_STRING locale_name;
	SIZE_T locale_index;
	HWND hwindow;

#if defined(APP_HAVE_SETTINGS)
	INT item_id;
#endif // APP_HAVE_SETTINGS

	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		if (menu_id == ctrl_id)
		{
			locale_index = SIZE_MAX;
		}
		else
		{
			locale_index = (UINT_PTR)ctrl_id - (UINT_PTR)menu_id - 2;
		}
	}
	else
	{
#if defined(APP_HAVE_SETTINGS)
		item_id = _r_combobox_getcurrentitem (hwnd, ctrl_id);

		if (item_id == CB_ERR)
			return;

		locale_index = _r_combobox_getitemparam (hwnd, ctrl_id, item_id);
#else
		return;
#endif // APP_HAVE_SETTINGS
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
		PostMessage (hwindow, RM_LOCALIZE, 0, 0);

#if defined(APP_HAVE_SETTINGS)
	// refresh settings window
	hwindow = _r_settings_getwindow ();

	if (hwindow)
		PostMessage (hwindow, RM_LOCALIZE, 0, 0);
#endif // APP_HAVE_SETTINGS
}

VOID _r_locale_enum (
	_In_ PVOID hwnd,
	_In_ INT ctrl_id,
	_In_opt_ UINT menu_id
)
{
	PR_STRING locale_name;
	HMENU hsubmenu;
	SIZE_T locale_count;
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

		_r_menu_enableitem (hwnd, ctrl_id, MF_BYPOSITION, FALSE);
	}
	else
	{
#if defined(APP_HAVE_SETTINGS)
		hsubmenu = NULL; // fix warning!

		_r_combobox_clear (hwnd, ctrl_id);

		_r_combobox_insertitem (hwnd, ctrl_id, 0, _r_obj_getstringorempty (app_global.locale.resource_name));

		_r_combobox_setitemparam (hwnd, ctrl_id, 0, SIZE_MAX);

		_r_combobox_setcurrentitem (hwnd, ctrl_id, 0);

		_r_ctrl_enable (hwnd, ctrl_id, FALSE);
#else
		return;
#endif // APP_HAVE_SETTINGS
	}

	locale_count = _r_locale_getcount ();

	if (!locale_count)
		return;

	if (is_menu)
	{
		_r_menu_enableitem (hwnd, ctrl_id, MF_BYPOSITION, TRUE);
		_r_menu_additem (hsubmenu, 0, NULL);
	}
	else
	{
		_r_ctrl_enable (hwnd, ctrl_id, TRUE);
	}

	index = 1;

	_r_queuedlock_acquireshared (&app_global.locale.lock);

	for (SIZE_T i = 0; i < locale_count; i++)
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
#if defined(APP_HAVE_SETTINGS)
			_r_combobox_insertitem (hwnd, ctrl_id, index, locale_name->buffer);
			_r_combobox_setitemparam (hwnd, ctrl_id, index, i);

			if (is_current)
				_r_combobox_setcurrentitem (hwnd, ctrl_id, index);
#endif // APP_HAVE_SETTINGS
		}

		index += 1;
	}

	_r_queuedlock_releaseshared (&app_global.locale.lock);
}

SIZE_T _r_locale_getcount ()
{
	SIZE_T count;

	_r_queuedlock_acquireshared (&app_global.locale.lock);
	count = _r_obj_getlistsize (app_global.locale.available_list);
	_r_queuedlock_releaseshared (&app_global.locale.lock);

	return count;
}

_Ret_maybenull_
PR_STRING _r_locale_getstring_ex (
	_In_ UINT uid
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;

	PR_STRING hash_string;
	PR_STRING value_string;
	ULONG hash_code;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_obj_ishashtableempty (app_global.locale.table))
			_r_locale_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.locale.table)
		return NULL;

	if (app_global.locale.current_name)
	{
		hash_string = _r_format_string (L"%s\\%03" TEXT (PRIu32), app_global.locale.current_name->buffer, uid);

		hash_code = _r_str_gethash2 (hash_string, TRUE);

		_r_queuedlock_acquireshared (&app_global.locale.lock);
		value_string = _r_obj_findhashtablepointer (app_global.locale.table, hash_code);
		_r_queuedlock_releaseshared (&app_global.locale.lock);

		_r_obj_dereference (hash_string);
	}
	else
	{
		value_string = NULL;
	}

	if (!value_string)
	{
		if (app_global.locale.resource_name)
		{
			hash_string = _r_format_string (L"%s\\%03" TEXT (PRIu32), app_global.locale.resource_name->buffer, uid);

			hash_code = _r_str_gethash2 (hash_string, TRUE);

			_r_queuedlock_acquireshared (&app_global.locale.lock);
			value_string = _r_obj_findhashtablepointer (app_global.locale.table, hash_code);
			_r_queuedlock_releaseshared (&app_global.locale.lock);

			if (!value_string)
			{
				value_string = _r_res_loadstring (_r_sys_getimagebase (), uid);

				if (value_string)
				{
					_r_queuedlock_acquireexclusive (&app_global.locale.lock);

					_r_obj_addhashtablepointer (app_global.locale.table, hash_code, _r_obj_reference (value_string));

					_r_queuedlock_releaseexclusive (&app_global.locale.lock);
				}
			}

			_r_obj_dereference (hash_string);
		}
	}

	return value_string;
}

// TODO: in theory this is not good, so redesign in future.
LPCWSTR _r_locale_getstring (
	_In_ UINT uid
)
{
	PR_STRING string;
	LPCWSTR result;

	string = _r_locale_getstring_ex (uid);

	if (!string)
		return NULL;

	result = string->buffer;

	_r_obj_dereference (string);

	return result;
}

LONG64 _r_locale_getversion ()
{
	PR_STRING path;
	WCHAR timestamp_string[64];
	R_STRINGREF string;
	ULONG length;

	path = _r_app_getlocalepath ();

	// HACK!!! Use "Russian" section and default timestamp key (000) for compatibility with old releases...
	length = GetPrivateProfileString (L"Russian", L"000", NULL, timestamp_string, RTL_NUMBER_OF (timestamp_string), path->buffer);

	if (!length)
		return 0;

	_r_obj_initializestringref_ex (&string, timestamp_string, length * sizeof (WCHAR));

	return _r_str_tolong64 (&string);
}
#endif // !APP_CONSOLE

#if defined(APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ()
{
	HANDLE hkey;
	PR_STRING path;
	BOOLEAN is_enabled = FALSE;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_READ, &hkey);

	if (NT_SUCCESS (status))
	{
		status = _r_reg_querystring (hkey, _r_app_getname (), &path);

		if (NT_SUCCESS (status))
		{
			PathRemoveArgs (path->buffer);
			PathUnquoteSpaces (path->buffer);

			_r_obj_trimstringtonullterminator (path);

			if (_r_str_isequal2 (&path->sr, _r_sys_getimagepath (), TRUE))
				is_enabled = TRUE;

			_r_obj_dereference (path);
		}

		NtClose (hkey);
	}

	return is_enabled;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _r_autorun_enable (
	_In_opt_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	HANDLE hkey;
	PR_STRING string;
	NTSTATUS status;

	status = _r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_WRITE, &hkey);

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

		_r_obj_dereference (string);
	}
	else
	{
		status = _r_reg_deletevalue (hkey, _r_app_getname ());

		if (status == STATUS_OBJECT_NAME_NOT_FOUND)
			status = STATUS_SUCCESS;
	}

	NtClose (hkey);

	if (hwnd && status != ERROR_SUCCESS)
		_r_show_errormessage (hwnd, NULL, status, NULL);

	return status;
}
#endif // APP_HAVE_AUTORUN

#if defined(APP_HAVE_UPDATES)
BOOLEAN NTAPI _r_update_downloadcallback (
	_In_ ULONG total_written,
	_In_ ULONG total_length,
	_In_ PVOID lparam
)
{
	PR_UPDATE_INFO update_info;
	LONG percent;

	update_info = lparam;

	if (update_info->htaskdlg)
	{
		percent = _r_calc_percentof (total_written, total_length);

		SendMessage (update_info->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
	}

	return TRUE;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _r_update_downloadupdate (
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
		FILE_SHARE_READ,
		FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY,
		0,
		FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
		return status;

	_r_inet_initializedownload_ex (&download_info, hfile, &_r_update_downloadcallback, update_info);

	status = _r_inet_begindownload (update_info->hsession, update_component->url, &download_info);

	NtClose (hfile); // required!

	if (status != ERROR_SUCCESS)
		return status;

	if (update_component->flags & PR_UPDATE_FLAG_FILE)
	{
		// copy required files
		if (_r_fs_exists (update_component->target_path->buffer))
			_r_fs_deletefile (update_component->target_path->buffer, TRUE);

		// move target files
		status = _r_fs_movefile (update_component->cache_path->buffer, update_component->target_path->buffer);

		if (!NT_SUCCESS (status))
			_r_fs_copyfile (update_component->cache_path->buffer, update_component->target_path->buffer);

		// remove if it exists
		if (_r_fs_exists (update_component->cache_path->buffer))
			_r_fs_deletefile (update_component->cache_path->buffer, TRUE);

		update_component->flags &= ~PR_UPDATE_FLAG_AVAILABLE;

		_r_obj_movereference (&update_component->current_version, update_component->new_version);
		update_component->new_version = NULL;
	}

	// remove cache directory if it is empty
	RemoveDirectory (path);

	return ERROR_SUCCESS;
}

NTSTATUS NTAPI _r_update_downloadthread (
	_In_ PVOID arglist
)
{
	PR_UPDATE_INFO update_info = arglist;
	PR_UPDATE_COMPONENT update_component;
	TASKDIALOG_COMMON_BUTTON_FLAGS buttons;
	LPCWSTR str_content;
	LPCWSTR main_icon = NULL;
	ULONG update_flags = 0;
	ULONG status = ERROR_SUCCESS;

	for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
	{
		update_component = _r_obj_getarrayitem (update_info->components, i);

		if (!(update_component->flags & PR_UPDATE_FLAG_AVAILABLE))
			continue;

		status = _r_update_downloadupdate (update_info, update_component);

		if (status != ERROR_SUCCESS)
			break;

		update_flags |= update_component->flags;
	}

	// set result text and navigate taskdialog
	if (status == ERROR_SUCCESS && update_flags)
	{
		if (update_flags & PR_UPDATE_FLAG_FILE)
			_r_update_applyconfig ();

		if (update_flags & PR_UPDATE_FLAG_INSTALLER)
		{
			buttons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;

#ifdef IDS_UPDATE_INSTALL
			str_content = _r_locale_getstring (IDS_UPDATE_INSTALL);
#else
			str_content = L"Update available. Do you want to install it now?";
#pragma PR_PRINT_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL
		}
		else
		{
			buttons = TDCBF_CLOSE_BUTTON;

#ifdef IDS_UPDATE_DONE
			str_content = _r_locale_getstring (IDS_UPDATE_DONE);
#else
			str_content = L"Downloading update finished.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
		}
	}
	else
	{
		main_icon = TD_WARNING_ICON;
		buttons = TDCBF_CLOSE_BUTTON;

#ifdef IDS_UPDATE_ERROR
		str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
		str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
	}

	_r_update_navigate (update_info, buttons, 0, main_icon, NULL, str_content, status);

	return STATUS_SUCCESS;
}

NTSTATUS NTAPI _r_update_checkthread (
	_In_ PVOID arglist
)
{
	PR_UPDATE_INFO update_info;
	PR_UPDATE_COMPONENT update_component;
	WCHAR str_updates[256] = {0};
	LPCWSTR str_content;
	R_DOWNLOAD_INFO download_info;
	PR_HASHTABLE string_table;
	PR_STRING update_url;
	PR_STRING string_value;
	PR_STRING string;
	R_STRINGREF remaining_part;
	R_STRINGREF new_version_sr;
	R_STRINGREF new_url_sr;
	SIZE_T downloads_count = 0;
	ULONG hash_code;
	ULONG status;

	update_info = arglist;

	update_url = _r_obj_createstring (_r_app_getupdate_url ());

	_r_inet_initializedownload (&download_info);

	status = _r_inet_begindownload (update_info->hsession, update_url, &download_info);

	if (status != ERROR_SUCCESS)
	{
		if (update_info->hparent)
		{
#ifdef IDS_UPDATE_ERROR
			str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
			str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

			_r_update_navigate (update_info, TDCBF_CLOSE_BUTTON, 0, TD_WARNING_ICON, NULL, str_content, status);
		}

		goto CleanupExit;
	}

	string_table = _r_str_unserialize (&download_info.u.string->sr, L';', L'=');

	if (string_table)
	{
		string_value = NULL;

		for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
		{
			update_component = _r_obj_getarrayitem (update_info->components, i);

			hash_code = _r_str_gethash2 (update_component->short_name, TRUE);
			string = _r_obj_findhashtablepointer (string_table, hash_code);

			_r_obj_movereference (&string_value, string);

			if (!string_value)
				continue;

			if (!_r_str_splitatchar (&string_value->sr, L'|', &new_version_sr, &remaining_part))
				continue;

			if (_r_str_versioncompare (&update_component->current_version->sr, &new_version_sr) != -1)
				continue;

			_r_str_splitatchar (&remaining_part, L'|', &new_url_sr, &remaining_part);

			_r_obj_movereference (&update_component->url, _r_obj_createstring3 (&new_url_sr));
			_r_obj_movereference (&update_component->new_version, _r_obj_createstring3 (&new_version_sr));

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

					if (status != ERROR_SUCCESS)
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
		_r_str_trim (str_updates, L"\r\n ");

#ifdef IDS_UPDATE_YES
		str_content = _r_locale_getstring (IDS_UPDATE_YES);
#else
		str_content = L"Update available. Download and install now?";
#pragma PR_PRINT_WARNING(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

		_r_update_navigate (update_info, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, 0, NULL, str_content, str_updates, 0);
	}
	else
	{
		if (update_info->hparent)
		{
			if (status != ERROR_SUCCESS)
			{
#ifdef IDS_UPDATE_ERROR
				str_content = _r_locale_getstring (IDS_UPDATE_ERROR);
#else
				str_content = L"Update server connection error.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
			}
			else
			{
				if (downloads_count)
				{
#ifdef IDS_UPDATE_DONE
					str_content = _r_locale_getstring (IDS_UPDATE_DONE);
#else
					str_content = L"Downloading update finished.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
				}
				else
				{
#ifdef IDS_UPDATE_NO
					str_content = _r_locale_getstring (IDS_UPDATE_NO);
#else
					str_content = L"No updates available.";
#pragma PR_PRINT_WARNING(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO
				}
			}

			_r_update_navigate (update_info, TDCBF_CLOSE_BUTTON, 0, NULL, NULL, str_content, status);
		}
	}

	_r_config_setlong64 (L"CheckUpdatesLast", _r_unixtime_now ());

CleanupExit:

	_r_inet_destroydownload (&download_info);

	_r_obj_dereference (update_url);

	InterlockedDecrement (&update_info->lock);

	return STATUS_SUCCESS;
}

VOID _r_update_enable (
	_In_ BOOLEAN is_enable
)
{
	LONG value;

	if (is_enable)
	{
		value = APP_UPDATE_PERIOD;
	}
	else
	{
		value = 0;
	}

	_r_config_setlong (L"CheckUpdatesPeriod", value);
}

_Success_ (return)
BOOLEAN _r_update_isenabled (
	_In_ BOOLEAN is_checktimestamp
)
{
	LONG64 current_timestamp;
	LONG64 update_last_timestamp;
	LONG update_period;
	LONG update_current_timestamp;

	update_period = _r_config_getlong (L"CheckUpdatesPeriod", APP_UPDATE_PERIOD);

	if (update_period <= 0)
		return FALSE;

	if (is_checktimestamp)
	{
		current_timestamp = _r_unixtime_now ();

		update_last_timestamp = _r_config_getlong64 (L"CheckUpdatesLast", 0);
		update_current_timestamp = _r_calc_days2seconds (update_period);

		if ((current_timestamp - update_last_timestamp) <= update_current_timestamp)
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

	if (InterlockedCompareExchange (&update_info->lock, 0, 0) != 0)
		return FALSE;

	if (!update_info->hsession)
		update_info->hsession = _r_inet_createsession (_r_app_getuseragent ());

	if (!update_info->hsession)
		return FALSE;

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_LOWEST, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	status = _r_sys_createthread (&_r_update_checkthread, update_info, &hthread, &environment, L"UpdateThread");

	if (!NT_SUCCESS (status))
		return FALSE;

	InterlockedIncrement (&update_info->lock);

	update_info->hparent = hparent;
	update_info->htaskdlg = NULL;
	update_info->hthread = NULL;

	update_info->flags = 0;

	update_info->is_clicked = FALSE;

	if (hparent)
	{
#ifdef IDS_UPDATE_INIT
		str_content = _r_locale_getstring (IDS_UPDATE_INIT);
#else
		str_content = L"Checking for new releases...";
#pragma PR_PRINT_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

		update_info->hthread = hthread;

		_r_update_navigate (update_info, TDCBF_CANCEL_BUTTON, TDF_SHOW_PROGRESS_BAR, NULL, NULL, str_content, 0);
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
	_In_ LONG_PTR param
)
{
	PR_UPDATE_INFO update_info = (PR_UPDATE_INFO)param;

	switch (msg)
	{
		case TDN_CREATED:
		{
			HWND hparent;

			update_info->htaskdlg = hwnd;

			SendMessage (hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			SendMessage (hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			hparent = GetParent (hwnd);

			_r_wnd_center (hwnd, hparent);

			if (update_info->hparent)
				_r_wnd_top (hwnd, TRUE);

			break;
		}

		case TDN_VERIFICATION_CLICKED:
		{
			update_info->is_clicked = !!wparam;
			break;
		}

		case TDN_BUTTON_CLICKED:
		{
			PR_UPDATE_COMPONENT update_component;
			R_ENVIRONMENT environment;
			LPCWSTR str_content;
			NTSTATUS status;

			if (wparam == IDYES)
			{
				_r_sys_setenvironment (&environment, THREAD_PRIORITY_LOWEST, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

				status = _r_sys_createthread (&_r_update_downloadthread, update_info, &update_info->hthread, &environment, L"UpdateThread");

				if (NT_SUCCESS (status))
				{
					if (update_info->is_clicked)
					{
						_r_config_setboolean (L"IsAutoinstallUpdates", TRUE);

						update_info->is_clicked = FALSE;
					}

#ifdef IDS_UPDATE_DOWNLOAD
					str_content = _r_locale_getstring (IDS_UPDATE_DOWNLOAD);
#else
					str_content = L"Downloading update...";
#pragma PR_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

					_r_update_navigate (update_info, TDCBF_CANCEL_BUTTON, TDF_SHOW_PROGRESS_BAR, NULL, NULL, str_content, 0);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
				{
					update_component = _r_obj_getarrayitem (update_info->components, i);

					if (update_component->flags & (PR_UPDATE_FLAG_INSTALLER | PR_UPDATE_FLAG_AVAILABLE))
						_r_update_install (update_component);
				}
			}
			else if (wparam == IDCANCEL)
			{
				if (!update_info->hthread)
					break;

				NtTerminateThread (update_info->hthread, STATUS_CANCELLED);
				NtClose (update_info->hthread);

				update_info->hthread = NULL;
			}

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			if (!update_info->hthread)
				break;

			NtResumeThread (update_info->hthread, NULL);
			NtClose (update_info->hthread);

			update_info->hthread = NULL;

			break;
		}

		case TDN_DESTROYED:
		{
			update_info->htaskdlg = NULL;

			if (!update_info->hthread)
				break;

			NtClose (update_info->hthread);

			update_info->hthread = NULL;

			break;
		}
	}

	return S_OK;
}

VOID _r_update_navigate (
	_In_ PR_UPDATE_INFO update_info,
	_In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
	_In_opt_ TASKDIALOG_FLAGS flags,
	_In_opt_ LPCWSTR main_icon,
	_In_opt_ LPCWSTR main,
	_In_opt_ LPCWSTR content,
	_In_opt_ ULONG error_code
)
{
	TASKDIALOGCONFIG tdc = {0};
	WCHAR buffer[64];
	BOOL is_checked;

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND | flags;
	tdc.hwndParent = update_info->hparent ? update_info->hparent : _r_app_gethwnd ();
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.dwCommonButtons = buttons;
	tdc.pfCallback = &_r_update_pagecallback;
	tdc.lpCallbackData = (LONG_PTR)update_info;

	if (main_icon)
	{
		tdc.pszMainIcon = main_icon;
	}
	else
	{
#if defined(IDI_MAIN)
		tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
		tdc.pszMainIcon = MAKEINTRESOURCE (100);
#pragma PR_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN
	}

	tdc.pszWindowTitle = _r_app_getname ();

	if (main)
		tdc.pszMainInstruction = main;

	if (content)
		tdc.pszContent = content;

	if (error_code != ERROR_SUCCESS)
	{
		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"Status: %" TEXT (PR_ULONG), error_code);

		tdc.pszExpandedInformation = buffer;
	}

	if (buttons & TDCBF_YES_BUTTON)
	{
		if (update_info->flags & PR_UPDATE_FLAG_FILE)
		{
#ifdef IDS_UPDATE_AUTOINSTALL
			tdc.pszVerificationText = _r_locale_getstring (IDS_UPDATE_AUTOINSTALL);
#else
			tdc.pszVerificationText = L"Automatically install non-executable updates";
#pragma PR_PRINT_WARNING(IDS_UPDATE_AUTOINSTALL)
#endif // IDS_UPDATE_AUTOINSTALL
		}
	}

	if (update_info->htaskdlg)
	{
		SendMessage (update_info->htaskdlg, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);
	}
	else
	{
		_r_msg_taskdialog (&tdc, NULL, NULL, &is_checked);
	}
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

	if (is_installer)
	{
		update_component.flags = PR_UPDATE_FLAG_INSTALLER;
	}
	else
	{
		update_component.flags = PR_UPDATE_FLAG_FILE;
	}

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

	SendMessage (hwnd, RM_CONFIG_UPDATE, 0, 0);
	SendMessage (hwnd, RM_INITIALIZE, 0, 0);

	PostMessage (hwnd, RM_LOCALIZE, 0, 0);
}

VOID _r_update_install (
	_In_ PR_UPDATE_COMPONENT update_component
)
{
	R_ERROR_INFO error_info;
	PR_STRING cmd_string;

	if (!_r_fs_exists (update_component->cache_path->buffer))
		return;

	cmd_string = _r_format_string (L"\"%s\" /u /S /D=%s", update_component->cache_path->buffer, update_component->target_path->buffer);

	if (!_r_sys_runasadmin (update_component->cache_path->buffer, cmd_string->buffer, NULL))
	{
		_r_error_initialize (&error_info, NULL, update_component->cache_path->buffer);

		_r_show_errormessage (NULL, NULL, GetLastError (), &error_info);
	}

	_r_obj_dereference (cmd_string);
}
#endif // APP_HAVE_UPDATES

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
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE hfile = NULL;

	LARGE_INTEGER file_size;
	BYTE bom[] = {0xFF, 0xFE};
	IO_STATUS_BLOCK isb;
	PR_STRING string;
	NTSTATUS status;

	// write to file only when readonly mode is not specified
	if (_r_app_isreadonly ())
		return NULL;

	if (_r_initonce_begin (&init_once))
	{
		string = _r_app_getlogpath ();

		if (string)
		{
			status = _r_fs_createfile (string->buffer, FILE_OPEN_IF, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL, 0, FALSE, NULL, &hfile);

			if (status == STATUS_OBJECT_NAME_COLLISION)
			{
				// write utf-16 le byte order mask
				NtWriteFile (hfile, NULL, NULL, NULL, &isb, bom, sizeof (bom), NULL, NULL);

				// adds csv header
				NtWriteFile (hfile, NULL, NULL, NULL, &isb, PR_DEBUG_HEADER, (ULONG)(_r_str_getlength (PR_DEBUG_HEADER) * sizeof (WCHAR)), NULL, NULL);
			}
			else
			{
				if (NT_SUCCESS (status))
				{
					status = _r_fs_getsize (hfile, &file_size);

					if (NT_SUCCESS (status))
						_r_fs_setpos (hfile, &file_size);
				}
			}
		}

		_r_initonce_end (&init_once);
	}

	return hfile;
}

VOID _r_log (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ ULONG error_code,
	_In_opt_ LPCWSTR description
)
{
	PR_STRING error_string;
	PR_STRING date_string;
	LPCWSTR level_string;
	IO_STATUS_BLOCK isb;
	ULONG number = 0;
	LONG64 current_timestamp;
	HANDLE hfile;

	if (!_r_log_isenabled (log_level))
		return;

	current_timestamp = _r_unixtime_now ();
	date_string = _r_format_unixtime_ex (current_timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	level_string = _r_log_leveltostring (log_level);

	// print log for debuggers
	_r_debug (L"[%s],%s,0x%08" TEXT (PRIX32) L",%s\r\n", level_string, title, error_code, description);

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

		NtWriteFile (hfile, NULL, NULL, NULL, &isb, error_string->buffer, (ULONG)error_string->length, NULL, NULL);

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
			if ((current_timestamp - app_global.error.last_timestamp) > APP_ERROR_PERIOD)
			{
				_r_tray_popup (_r_app_gethwnd (), tray_guid, number, _r_app_getname (), APP_WARNING_LOG_TEXT);

				app_global.error.last_timestamp = current_timestamp;
			}
		}
	}
#endif // APP_HAVE_TRAY
}

VOID _r_log_v (
	_In_ R_LOG_LEVEL log_level,
	_In_opt_ LPCGUID tray_guid,
	_In_ LPCWSTR title,
	_In_ ULONG error_code,
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
			return L"Disabled";

		case LOG_LEVEL_DEBUG:
			return L"Debug";

		case LOG_LEVEL_INFO:
			return L"Info";

		case LOG_LEVEL_WARNING:
			return L"Warning";

		case LOG_LEVEL_ERROR:
			return L"Error";

		case LOG_LEVEL_CRITICAL:
			return L"Critical";
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
			return NIIF_INFO;

		case LOG_LEVEL_WARNING:
			return NIIF_WARNING;

		case LOG_LEVEL_ERROR:
		case LOG_LEVEL_CRITICAL:
			return NIIF_ERROR;

		case LOG_LEVEL_DISABLED:
		default:
			return NIIF_NONE;
	}
}

#if !defined(APP_CONSOLE)
VOID _r_show_aboutmessage (
	_In_opt_ HWND hwnd
)
{
	static BOOLEAN is_opened = FALSE;

	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[2] = {0};
	LPCWSTR str_title;
	WCHAR str_content[512];
	INT command_id;

	if (is_opened)
		return;

	is_opened = TRUE;

#ifdef IDS_ABOUT
	str_title = _r_locale_getstring (IDS_ABOUT);
#else
	str_title = L"About";
#pragma PR_PRINT_WARNING(IDS_ABOUT)
#endif

	_r_str_printf (
		str_content, RTL_NUMBER_OF (str_content),
		L"Version %s %s, %" TEXT (PR_LONG) L"-bit (Unicode)\r\n%s\r\n\r\n" \
		L"<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>",
		_r_app_getversion (),
		_r_app_getversiontype (),
		_r_app_getarch (),
		_r_app_getcopyright (),
		_r_app_getwebsite_url (),
		_r_app_getwebsite_url () + 8,
		_r_app_getsources_url (),
		_r_app_getsources_url () + 8
	);

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pszFooterIcon = TD_INFORMATION_ICON;
	tdc.nDefaultButton = IDCLOSE;
	tdc.pszWindowTitle = str_title;
	tdc.pszMainInstruction = _r_app_getname ();
	tdc.pszContent = str_content;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	tdc.pButtons = td_buttons;
	tdc.cButtons = RTL_NUMBER_OF (td_buttons);

	td_buttons[0].nButtonID = IDOK;
	td_buttons[1].nButtonID = IDCLOSE;

#if defined(IDI_MAIN)
	tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
#pragma PR_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

#if defined(IDS_DONATE)
	td_buttons[0].pszButtonText = _r_locale_getstring (IDS_DONATE);
#else
	td_buttons[0].pszButtonText = L"Give thanks!";
#pragma PR_PRINT_WARNING(IDS_DONATE)
#endif // IDS_DONATE

#if defined(IDS_CLOSE)
	td_buttons[1].pszButtonText = _r_locale_getstring (IDS_CLOSE);
#else
	td_buttons[1].pszButtonText = L"Close";
#pragma PR_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

	tdc.pszFooter = APP_ABOUT_FOOTER;

	if (SUCCEEDED (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL)))
	{
		if (command_id == td_buttons[0].nButtonID)
			_r_shell_opendefault (_r_app_getdonate_url ());
	}

	is_opened = FALSE;
}

VOID _r_show_errormessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR main,
	_In_ ULONG error_code,
	_In_opt_ PR_ERROR_INFO error_info_ptr
)
{
	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[3] = {0};
	WCHAR str_content[1024];
	LPCWSTR str_main;
	LPCWSTR path;
	R_STRINGREF sr;
	PR_STRING string;
	HINSTANCE hinstance;
	INT command_id;
	UINT btn_cnt = 0;
	ULONG status;

	if (error_info_ptr && error_info_ptr->hinst)
	{
		hinstance = error_info_ptr->hinst;
	}
	else
	{
		hinstance = GetModuleHandle (L"kernel32.dll");
	}

	status = _r_sys_formatmessage (error_code, hinstance, 0, &string);

	if (status == ERROR_MR_MID_NOT_FOUND || status == ERROR_RESOURCE_TYPE_NOT_FOUND)
		_r_sys_formatmessage (error_code, GetModuleHandle (L"ntdll.dll"), 0, &string);

	str_main = main ? main : APP_FAILED_MESSAGE_TITLE;

	_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s (0x%08" TEXT (PRIX32) L")", _r_obj_getstringordefault (string, L"n/a"), error_code);

	if (error_info_ptr)
	{
		if (error_info_ptr->description)
		{
			_r_str_append (str_content, RTL_NUMBER_OF (str_content), L"\r\n\r\n");
			_r_str_append (str_content, RTL_NUMBER_OF (str_content), error_info_ptr->description);
		}
	}

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_ENABLE_HYPERLINKS | TDF_NO_SET_FOREGROUND | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pszFooterIcon = TD_WARNING_ICON;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pszMainInstruction = str_main;
	tdc.pszContent = str_content;
	tdc.pszFooter = APP_FAILED_MESSAGE_FOOTER;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	if (error_info_ptr && error_info_ptr->exception_ptr)
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
			path = _r_app_getcrashdirectory (FALSE);

			_r_shell_opendefault (path);
		}
		else if (command_id == IDNO)
		{
			_r_obj_initializestringref (&sr, str_content);

			_r_clipboard_set (NULL, &sr);
		}
	}

	if (string)
		_r_obj_dereference (string);
}

BOOLEAN _r_show_confirmmessage (
	_In_opt_ HWND hwnd,
	_In_opt_ LPCWSTR main,
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
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	if (config_key)
	{
#ifdef IDS_QUESTION_FLAG_CHK
		tdc.pszVerificationText = _r_locale_getstring (IDS_QUESTION_FLAG_CHK);
#else
		tdc.pszVerificationText = L"Do not ask again";
#pragma PR_PRINT_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK
	}

	if (main)
		tdc.pszMainInstruction = main;

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

INT _r_show_message (
	_In_opt_ HWND hwnd,
	_In_ ULONG flags,
	_In_opt_ LPCWSTR main,
	_In_ LPCWSTR content
)
{
	TASKDIALOGCONFIG tdc = {0};
	INT command_id;

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
	tdc.hwndParent = hwnd;
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.pfCallback = &_r_msg_callback;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pszMainInstruction = main;
	tdc.pszContent = content;

	// set icons
	if ((flags & MB_ICONMASK) == MB_USERICON)
	{
#ifdef IDI_MAIN
		tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
		tdc.pszMainIcon = TD_INFORMATION_ICON;
#pragma PR_PRINT_WARNING(IDI_MAIN)
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

	// set options
	if (flags & MB_TOPMOST)
		tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

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
	BOOLEAN is_maximized;

	if (!_r_wnd_getposition (hwnd, &rectangle_current))
		return;

	style = _r_wnd_getstyle (hwnd);

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
		if (_r_str_compare (window_name, L"window") != 0)
		{
			is_maximized = _r_config_getboolean_ex (L"IsMaximized", FALSE, window_name);

			if (is_maximized)
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

	style = _r_wnd_getstyle (hwnd);

	is_maximized = (placement.showCmd == SW_MAXIMIZE) || (placement.showCmd == SW_SHOWMINIMIZED);

	if (style & WS_MAXIMIZEBOX)
		_r_config_setboolean_ex (L"IsMaximized", is_maximized, window_name);

	_r_wnd_recttorectangle (&rectangle, &placement.rcNormalPosition);

	hmonitor = MonitorFromRect (&placement.rcNormalPosition, MONITOR_DEFAULTTOPRIMARY);

	monitor_info.cbSize = sizeof (monitor_info);

	// The rectangle is in workspace coordinates. Convert the values back to screen coordinates.
	if (GetMonitorInfo (hmonitor, &monitor_info))
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
#endif // !APP_CONSOLE

//
// Settings window
//

#if defined(APP_HAVE_SETTINGS)
VOID _r_settings_addpage (
	_In_ INT dlg_id,
	_In_ UINT locale_id
)
{
	R_SETTINGS_PAGE settings_page = {0};

	settings_page.dlg_id = dlg_id;
	settings_page.locale_id = locale_id;

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
	_In_ HWND hwnd,
	_In_opt_ DLGPROC dlg_proc,
	_In_opt_ LONG dlg_id
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SHORT width = 0;
	static SHORT height = 0;

	PVOID buffer;
	PBYTE buffer_ptr;
	R_BYTEREF dlg_buffer;
	LPDLGTEMPLATEEX dlg_template;
	PR_SETTINGS_PAGE ptr_page;
	SIZE_T size;
	WORD controls;

	assert (!_r_obj_isarrayempty (app_global.settings.page_list));

	if (_r_settings_getwindow ())
	{
		_r_wnd_toggle (_r_settings_getwindow (), TRUE);
		return;
	}

	// calculate maximum dialog size
	if (_r_initonce_begin (&init_once))
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
		{
			ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

			if (!ptr_page->dlg_id)
				continue;

			if (!_r_res_loadresource (_r_sys_getimagebase (), MAKEINTRESOURCE (ptr_page->dlg_id), RT_DIALOG, &dlg_buffer))
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
#endif

		_r_initonce_end (&init_once);
	}

#if defined(APP_HAVE_SETTINGS_TABS)
	controls = 4;
#else
	controls = 3;
#endif

	size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) +
			((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;

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
	_r_util_templatewriteulong (
		&buffer_ptr,
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME
	);

	// cdit
	_r_util_templatewriteshort (&buffer_ptr, controls);

	// x
	_r_util_templatewriteshort (&buffer_ptr, 0);

	// y
	_r_util_templatewriteshort (&buffer_ptr, 0);

	// cx
	_r_util_templatewriteshort (&buffer_ptr, width);

	// cy
	_r_util_templatewriteshort (&buffer_ptr, height);

	//
	// set dialog additional data
	//

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
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON,
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

	DialogBoxIndirect (_r_sys_getimagebase (), buffer, hwnd, &_r_settings_wndproc);

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
			LONG dlg_id;
#if !defined(APP_HAVE_SETTINGS_TABS)
			HTREEITEM hitem;
#endif // !APP_HAVE_SETTINGS_TABS
#ifdef IDI_MAIN
			LONG dpi_value;
			LONG icon_small;
			LONG icon_large;
#endif // IDI_MAIN

#if defined(APP_HAVE_SETTINGS_TABS)
			INT index = 0;
#endif

			app_global.settings.hwnd = hwnd;

#ifdef IDI_MAIN
			dpi_value = _r_dc_getwindowdpi (hwnd);

			icon_small = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
			icon_large = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);

			_r_wnd_seticon (
				hwnd,
				_r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (IDI_MAIN), icon_small),
				_r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (IDI_MAIN), icon_large)
			);
#else
#pragma PR_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			dlg_id = _r_config_getlong (L"SettingsLastPage", 0);

			for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
			{
				ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

				if (!ptr_page->dlg_id)
					continue;

				ptr_page->hwnd = _r_wnd_createwindow (_r_sys_getimagebase (), MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, app_global.settings.wnd_proc, 0);

				if (!ptr_page->hwnd)
					continue;

				BringWindowToTop (ptr_page->hwnd); // HACK!!!

				SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);

#if !defined(APP_HAVE_SETTINGS_TABS)
				hitem = _r_treeview_additem (hwnd, IDC_NAV, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, NULL, NULL, (LPARAM)ptr_page);

				if (dlg_id && ptr_page->dlg_id == dlg_id)
					SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hitem);
#else
				EnableThemeDialogTexture (ptr_page->hwnd, ETDT_ENABLETAB);

				_r_tab_additem (hwnd, IDC_NAV, index, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, (LPARAM)ptr_page);

				if (dlg_id && ptr_page->dlg_id == dlg_id)
					_r_tab_selectitem (hwnd, IDC_NAV, index);

				index += 1;
#endif // APP_HAVE_SETTINGS_TABS

				_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page->hwnd);
			}

#if defined(APP_HAVE_SETTINGS_TABS)
			if (_r_tab_getcurrentitem (hwnd, IDC_NAV) <= 0)
				_r_tab_selectitem (hwnd, IDC_NAV, 0);
#endif // APP_HAVE_SETTINGS_TABS

			SendMessage (hwnd, RM_LOCALIZE, 0, 0);

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
#ifdef IDS_SETTINGS
			SetWindowText (hwnd, _r_locale_getstring (IDS_SETTINGS));
#else
			SetWindowText (hwnd, L"Settings");
#pragma PR_PRINT_WARNING(IDS_SETTINGS)
#endif // IDS_SETTINGS

			// localize navigation
#if !defined(APP_HAVE_SETTINGS_TABS)
			dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_treeview_setstyle (
				hwnd,
				IDC_NAV,
				0,
				_r_dc_getdpi (PR_SIZE_ITEMHEIGHT, dpi_value),
				_r_dc_getdpi (PR_SIZE_TREEINDENT, dpi_value)
			);

			hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_ROOT, 0);

			while (hitem)
			{
				ptr_page = (PR_SETTINGS_PAGE)_r_treeview_getlparam (hwnd, IDC_NAV, hitem);

				if (ptr_page)
				{
					_r_treeview_setitem (hwnd, IDC_NAV, hitem, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd)
					{
						if (_r_wnd_isvisible (ptr_page->hwnd))
							PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
					}
				}

				hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hitem);
			}
#else
			for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
			{
				ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

				if (ptr_page)
				{
					_r_tab_setitem (hwnd, IDC_NAV, i, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd && _r_wnd_isvisible (ptr_page->hwnd))
						PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}
			}
#endif // APP_HAVE_SETTINGS_TABS

			// localize buttons
#ifdef IDC_RESET
#ifdef IDS_RESET
			_r_ctrl_setstring (hwnd, IDC_RESET, _r_locale_getstring (IDS_RESET));
#else
			_r_ctrl_setstring (hwnd, IDC_RESET, L"Reset");
#pragma PR_PRINT_WARNING(IDS_RESET)
#endif // IDS_RESET
#else
#pragma PR_PRINT_WARNING(IDC_RESET)
#endif // IDC_RESET

#if defined(APP_HAVE_SETTINGS_TABS)
#ifdef IDC_SAVE
#ifdef IDS_SAVE
			_r_ctrl_setstring (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
#else
			_r_ctrl_setstring (hwnd, IDC_SAVE, L"OK");
#pragma PR_PRINT_WARNING(IDS_SAVE)
#endif // IDS_SAVE
#else
#pragma PR_PRINT_WARNING(IDC_SAVE)
#endif // IDC_SAVE
#endif // APP_HAVE_SETTINGS_TABS

#ifdef IDC_CLOSE
#ifdef IDS_CLOSE
			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));
#else
			_r_ctrl_setstring (hwnd, IDC_CLOSE, L"Close");
#pragma PR_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE
#else
#pragma PR_PRINT_WARNING(IDC_CLOSE)
#endif // IDC_CLOSE

			break;
		}

		case WM_DPICHANGED:
		{
			_r_wnd_message_dpichanged (hwnd, wparam, lparam);

			PostMessage (hwnd, RM_LOCALIZE, 0, 0);

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

			for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
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

#ifdef APP_HAVE_UPDATES
			if (_r_update_isenabled (TRUE))
				_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr;
			INT ctrl_id;

			lphdr = (LPNMHDR)lparam;
			ctrl_id = (INT)(INT_PTR)lphdr->idFrom;

			if (ctrl_id != IDC_NAV)
				break;

			switch (lphdr->code)
			{
#if !defined(APP_HAVE_SETTINGS_TABS)
				case TVN_SELCHANGING:
				{
					LPNMTREEVIEW lpnmtv;
					PR_SETTINGS_PAGE ptr_page_old;
					PR_SETTINGS_PAGE ptr_page_new;

					lpnmtv = (LPNMTREEVIEW)lparam;

					ptr_page_old = (PR_SETTINGS_PAGE)lpnmtv->itemOld.lParam;
					ptr_page_new = (PR_SETTINGS_PAGE)lpnmtv->itemNew.lParam;

					if (ptr_page_old && ptr_page_old->hwnd && _r_wnd_isvisible (ptr_page_old->hwnd))
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (!ptr_page_new || !ptr_page_new->hwnd || _r_wnd_isvisible (ptr_page_new->hwnd))
						break;

					_r_config_setlong (L"SettingsLastPage", ptr_page_new->dlg_id);

					_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page_new->hwnd);

					SendMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

					ShowWindow (ptr_page_new->hwnd, SW_SHOW);

					break;
				}
#else
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

					if (!ptr_page || !ptr_page->hwnd || _r_wnd_isvisible (ptr_page->hwnd))
						break;

					_r_config_setlong (L"SettingsLastPage", ptr_page->dlg_id);

					_r_tab_adjustchild (hwnd, ctrl_id, ptr_page->hwnd);

					SendMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

					ShowWindow (ptr_page->hwnd, SW_SHOW);

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
					LONG_PTR retn;
					BOOLEAN is_saved;

					is_saved = TRUE;

					for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
					{
						ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

						if (ptr_page)
						{
							if (ptr_page->hwnd)
							{
								retn = (LONG_PTR)SendMessage (ptr_page->hwnd, RM_CONFIG_SAVE, (WPARAM)ptr_page->dlg_id, 0);

								if (retn == -1)
								{
									is_saved = FALSE;
									break;
								}
							}
						}
					}

					if (!is_saved)
						break;

					// fall through
				}
#endif // APP_HAVE_SETTINGS_TABS

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}

#ifdef IDC_RESET
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

#ifdef APP_HAVE_AUTORUN
					_r_autorun_enable (NULL, FALSE);
#endif // APP_HAVE_AUTORUN

#ifdef APP_HAVE_SKIPUAC
					_r_skipuac_enable (NULL, FALSE);
#endif // APP_HAVE_SKIPUAC

					// reinitialize application
					hwindow = _r_app_gethwnd ();

					if (hwindow)
					{
						SendMessage (hwindow, RM_INITIALIZE, 0, 0);
						SendMessage (hwindow, RM_LOCALIZE, 0, 0);

						SendMessage (hwindow, WM_EXITSIZEMOVE, 0, 0); // reset size and pos

						SendMessage (hwindow, RM_CONFIG_RESET, 0, 0);
					}

					// reinitialize settings
					for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
					{
						ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

						if (!ptr_page->hwnd)
							continue;

						if (_r_wnd_isvisible (ptr_page->hwnd))
							SendMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

						SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);
					}

					break;
				}
#endif // IDC_RESET
			}

			break;
		}
	}

	return FALSE;
}
#endif // APP_HAVE_SETTINGS

#if defined(APP_HAVE_SKIPUAC)
HRESULT _r_skipuac_checkmodulepath (
	_In_ IRegisteredTaskPtr registered_task
)
{
	ITaskDefinition *task_definition = NULL;
	IActionCollection *action_collection = NULL;
	IAction *action = NULL;
	IExecAction *exec_action = NULL;
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
	PathUnquoteSpaces (task_path);

	if (_r_str_compare (task_path, _r_sys_getimagepath ()) != 0)
	{
		status = SCHED_E_INVALID_TASK;

		goto CleanupExit;
	}

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
	VARIANT empty = {VT_EMPTY};
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	IRegisteredTask *registered_task = NULL;
	BSTR task_root = NULL;
	BSTR task_name = NULL;
	HRESULT status;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	task_root = SysAllocString (L"\\");

	status = ITaskService_GetFolder (task_service, task_root, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	task_name = SysAllocString (APP_SKIPUAC_NAME);

	status = ITaskFolder_GetTask (task_folder, task_name, &registered_task);

	if (FAILED (status))
		goto CleanupExit;

	status = _r_skipuac_checkmodulepath (registered_task);

	if (FAILED (status))
		goto CleanupExit;

CleanupExit:

	if (task_root)
		SysFreeString (task_root);

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
	VARIANT empty = {VT_EMPTY};
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	ITaskDefinition *task_definition = NULL;
	IRegistrationInfo *registration_info = NULL;
	IPrincipal *principal = NULL;
	ITaskSettings *task_settings = NULL;
	ITaskSettings2 *task_settings2 = NULL;
	IActionCollection *action_collection = NULL;
	IAction *action = NULL;
	IExecAction *exec_action = NULL;
	IRegisteredTask *registered_task = NULL;
	BSTR task_root = NULL;
	BSTR task_name = NULL;
	BSTR task_author = NULL;
	BSTR task_url = NULL;
	BSTR task_time_limit = NULL;
	BSTR task_path = NULL;
	BSTR task_directory = NULL;
	BSTR task_args = NULL;
	HRESULT status;

	if (hwnd && is_enable)
	{
		if (!_r_path_issecurelocation (_r_sys_getimagepath ()))
		{
			if (_r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, APP_SECURITY_TITLE, APP_WARNING_UAC_TEXT) != IDYES)
				return E_ABORT;
		}
	}

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	task_root = SysAllocString (L"\\");

	status = ITaskService_GetFolder (task_service, task_root, &task_folder);

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
		//
		// TASK_COMPATIBILITY_V2_4 - win10
		// TASK_COMPATIBILITY_V2_3 - win8.1
		// TASK_COMPATIBILITY_V2_2 - win8
		// TASK_COMPATIBILITY_V2_1 - win7
		// TASK_COMPATIBILITY_V2   - vista

		for (INT i = TASK_COMPATIBILITY_V2_4; i != TASK_COMPATIBILITY_V2; --i)
		{
			status = ITaskSettings_put_Compatibility (task_settings, i);

			if (SUCCEEDED (status))
				break;
		}

		// Set task settings (win7+)
		status = ITaskSettings_QueryInterface (task_settings, &IID_ITaskSettings2, &task_settings2);

		if (SUCCEEDED (status))
		{
			ITaskSettings2_put_UseUnifiedSchedulingEngine (task_settings2, VARIANT_TRUE);
			ITaskSettings2_put_DisallowStartOnRemoteAppSession (task_settings2, VARIANT_TRUE);

			ITaskSettings2_Release (task_settings2);
		}

		task_time_limit = SysAllocString (L"PT0S");

		ITaskSettings_put_AllowDemandStart (task_settings, VARIANT_TRUE);
		ITaskSettings_put_AllowHardTerminate (task_settings, VARIANT_FALSE);
		ITaskSettings_put_ExecutionTimeLimit (task_settings, task_time_limit);
		ITaskSettings_put_DisallowStartIfOnBatteries (task_settings, VARIANT_FALSE);
		ITaskSettings_put_MultipleInstances (task_settings, TASK_INSTANCES_PARALLEL);
		ITaskSettings_put_StartWhenAvailable (task_settings, VARIANT_TRUE);
		ITaskSettings_put_StopIfGoingOnBatteries (task_settings, VARIANT_FALSE);
		//ITaskSettings_put_Priority (task_settings, 4); // NORMAL_PRIORITY_CLASS

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

		if (status == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
			status = S_OK;
	}

CleanupExit:

	if (task_root)
		SysFreeString (task_root);

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
		_r_show_errormessage (hwnd, NULL, status, NULL);

	return status;
}

BOOLEAN _r_skipuac_run ()
{
	VARIANT empty = {VT_EMPTY};
	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	IRegisteredTask *registered_task = NULL;
	IRunningTask *running_task = NULL;
	BSTR task_root = NULL;
	BSTR task_name = NULL;
	BSTR task_args = NULL;
	WCHAR arguments[512] = {0};
	VARIANT params = {0};
	LPWSTR *arga;
	ULONG attempts;
	TASK_STATE state;
	INT numargs;
	HRESULT status;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	task_root = SysAllocString (L"\\");

	status = ITaskService_GetFolder (task_service, task_root, &task_folder);

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
	arga = CommandLineToArgvW (_r_sys_getimagecommandline (), &numargs);

	if (arga)
	{
		if (numargs > 1)
		{
			for (INT i = 1; i < numargs; i++)
			{
				_r_str_appendformat (arguments, RTL_NUMBER_OF (arguments), L"%s ", arga[i]);
			}

			_r_str_trim (arguments, L" ");
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
	attempts = 6;

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

		_r_sys_sleep (150);
	}
	while (--attempts);

CleanupExit:

	if (task_root)
		SysFreeString (task_root);

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
#endif // APP_HAVE_SKIPUAC
