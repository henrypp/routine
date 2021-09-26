// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#include "rapp.h"

#if defined(APP_NO_MUTEX)
static LPCWSTR _r_app_getmutexname ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR name[128] = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_str_printf (name, RTL_NUMBER_OF (name), L"%s_%" TEXT (PR_ULONG) L"_%" TEXT (PR_ULONG), _r_app_getnameshort (), _r_str_gethash (_r_sys_getimagepath ()), _r_str_gethash (_r_sys_getimagecommandline ()));

		_r_initonce_end (&init_once);
	}

	return name;
}
#else
#define _r_app_getmutexname _r_app_getnameshort
#endif // APP_NO_MUTEX

#if defined(APP_NO_APPDATA) || defined(APP_CONSOLE)
FORCEINLINE BOOLEAN _r_app_isportable ()
{
	return TRUE;
}
#else
static BOOLEAN _r_app_isportable ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_portable = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		LPCWSTR file_names[] = {L"portable", _r_app_getnameshort ()};
		LPCWSTR file_exts[] = {L"dat", L"ini"};

		C_ASSERT (sizeof (file_names) == sizeof (file_exts));

		if (_r_sys_getopt (_r_sys_getimagecommandline (), L"portable", NULL))
		{
			is_portable = TRUE;
		}
		else
		{
			PR_STRING string;

			for (SIZE_T i = 0; i < RTL_NUMBER_OF (file_names); i++)
			{
				string = _r_obj_concatstrings (5, _r_app_getdirectory (), L"\\", file_names[i], L".", file_exts[i]);

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
}
#endif

#if defined(APP_NO_CONFIG) || defined(APP_CONSOLE)
FORCEINLINE BOOLEAN _r_app_isreadonly ()
{
	return TRUE;
}
#else
static BOOLEAN _r_app_isreadonly ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_readonly = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		is_readonly = _r_sys_getopt (_r_sys_getimagecommandline (), L"readonly", NULL);

		_r_initonce_end (&init_once);
	}

	return is_readonly;
}
#endif // APP_NO_CONFIG || APP_CONSOLE

static BOOLEAN _r_app_issecurelocation ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_writeable = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		PSECURITY_DESCRIPTOR security_descriptor;
		PACL dacl;
		ULONG status;

		status = GetNamedSecurityInfo (_r_sys_getimagepath (), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &security_descriptor);

		if (status == ERROR_SUCCESS)
		{
			if (!dacl)
			{
				is_writeable = TRUE;
			}
			else
			{
				PSID current_user_sid;
				PACCESS_ALLOWED_ACE ace;

				current_user_sid = _r_sys_getcurrenttoken ().token_sid;

				for (WORD ace_index = 0; ace_index < dacl->AceCount; ace_index++)
				{
					if (!GetAce (dacl, ace_index, &ace))
						continue;

					if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
						continue;

					if (RtlEqualSid (&ace->SidStart, &SeAuthenticatedUserSid) || RtlEqualSid (&ace->SidStart, current_user_sid))
					{
						if ((ace->Mask & (DELETE | ACTRL_FILE_WRITE_ATTRIB | SYNCHRONIZE | READ_CONTROL)) != 0)
						{
							is_writeable = TRUE;
							break;
						}
					}
				}
			}

			LocalFree (security_descriptor);
		}

		_r_initonce_end (&init_once);
	}

	return !is_writeable;
}

#if !defined(_DEBUG)
static VOID _r_app_exceptionfilter_savedump (_In_ PEXCEPTION_POINTERS exception_ptr)
{
	WCHAR dump_directory[512];
	WCHAR dump_path[512];
	LONG64 current_time;
	HANDLE hfile;

	current_time = _r_unixtime_now ();

	_r_str_printf (dump_directory, RTL_NUMBER_OF (dump_directory), L"%s\\crashdump", _r_app_getprofiledirectory ());
	_r_str_printf (dump_path, RTL_NUMBER_OF (dump_path), L"%s\\%s-%" TEXT (PR_LONG64) L".dmp", dump_directory, _r_app_getnameshort (), current_time);

	if (!_r_fs_exists (dump_directory))
		_r_fs_mkdir (dump_directory);

	hfile = CreateFile (dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		MINIDUMP_EXCEPTION_INFORMATION minidump_info = {0};

		minidump_info.ThreadId = HandleToUlong (NtCurrentThreadId ());
		minidump_info.ExceptionPointers = exception_ptr;
		minidump_info.ClientPointers = FALSE;

		MiniDumpWriteDump (NtCurrentProcess (), HandleToUlong (NtCurrentProcessId ()), hfile, MiniDumpNormal, &minidump_info, NULL, NULL);

		CloseHandle (hfile);
	}
}

ULONG CALLBACK _r_app_exceptionfilter_callback (_In_ PEXCEPTION_POINTERS exception_ptr)
{
	R_ERROR_INFO error_info = {0};
	ULONG error_code;

	if (NT_NTWIN32 (exception_ptr->ExceptionRecord->ExceptionCode))
	{
		error_code = WIN32_FROM_NTSTATUS (exception_ptr->ExceptionRecord->ExceptionCode);
		//error_info.hmodule = NULL;
	}
	else
	{
		error_code = exception_ptr->ExceptionRecord->ExceptionCode;
		error_info.hmodule = GetModuleHandle (L"ntdll.dll");
	}

	_r_app_exceptionfilter_savedump (exception_ptr);

#if defined(APP_CONSOLE)
	wprintf (L"Exception raised :( 0x%08X\r\n", error_code);
#else
	_r_show_errormessage (NULL, L"Exception raised :(", error_code, &error_info);
#endif // !APP_CONSOLE

#if defined(APP_NO_DEPRECATIONS)
	RtlExitUserProcess (exception_ptr->ExceptionRecord->ExceptionCode);
#else
	ExitProcess (exception_ptr->ExceptionRecord->ExceptionCode);
#endif // APP_NO_DEPRECATIONS

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif // !_DEBUG

BOOLEAN _r_app_initialize ()
{
	// Safe DLL loading
	// This prevents DLL planting in the application directory.
	// Check for SetDefaultDllDirectories since it requires KB2533623.

	SetDllDirectory (L"");

#ifdef APP_NO_DEPRECATIONS
	// win7+
	SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

	SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
	{
		HMODULE hkernel32 = LoadLibraryEx (L"kernel32.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (hkernel32)
		{
			const SSPM _SetSearchPathMode = (SSPM)GetProcAddress (hkernel32, "SetSearchPathMode");

			if (_SetSearchPathMode)
				_SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

			// Check for SetDefaultDllDirectories since it requires KB2533623.
			const SDDD _SetDefaultDllDirectories = (SDDD)GetProcAddress (hkernel32, "SetDefaultDllDirectories");

			if (_SetDefaultDllDirectories)
				_SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			FreeLibrary (hkernel32);
		}
	}
#endif // APP_NO_DEPRECATIONS

	// set unhandled exception filter
#if !defined(_DEBUG)
	{
		ULONG error_mode = 0;

		if (NT_SUCCESS (NtQueryInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG), NULL)))
		{
			error_mode &= ~(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

			NtSetInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG));
		}

#if defined(APP_NO_DEPRECATIONS)
		RtlSetUnhandledExceptionFilter (&_r_app_exceptionfilter_callback);
#else
		SetUnhandledExceptionFilter (&_r_app_exceptionfilter_callback);
#endif // APP_NO_DEPRECATIONS
	}
#endif // !_DEBUG

	// initialize COM library
	{
		HRESULT hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (FAILED (hr))
		{
#if defined(APP_CONSOLE)
			wprintf (L"Error! COM library initialization failed 0x%08" TEXT (PRIX32) L"!\r\n", hr);
#else
			_r_show_errormessage (NULL, L"COM library initialization failed!", hr, NULL);
#endif // APP_CONSOLE

			return FALSE;
		}
	}

#if !defined(APP_CONSOLE)
	// initialize controls
	{
		INITCOMMONCONTROLSEX icex = {0};

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;

		InitCommonControlsEx (&icex);
	}

	// prevent app duplicates
	if (_r_mutex_isexists (_r_app_getmutexname ()))
	{
		EnumWindows (&_r_util_activate_window_callback, (LPARAM)_r_app_getname ());
		return FALSE;
	}

	// set locale information
	{
		ULONG length;

		app_global.locale.resource_name = _r_obj_createstring (APP_LANGUAGE_DEFAULT);
		app_global.locale.default_name = _r_obj_createstringex (NULL, LOCALE_NAME_MAX_LENGTH * sizeof (WCHAR));

		length = GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SENGLISHLANGUAGENAME, app_global.locale.default_name->buffer, LOCALE_NAME_MAX_LENGTH);

		if (length > 1)
		{
			_r_obj_trimstringtonullterminator (app_global.locale.default_name); // terminate
		}
		else
		{
			_r_obj_clearreference (&app_global.locale.default_name);
		}
	}

#if defined(APP_HAVE_TRAY)
	if (!app_global.main.taskbar_msg)
		app_global.main.taskbar_msg = RegisterWindowMessage (L"TaskbarCreated");
#endif // APP_HAVE_TRAY

#if defined(APP_NO_GUEST)
	// use "only admin"-mode
	if (!_r_sys_iselevated ())
	{
		if (!_r_app_runasadmin ())
		{
			_r_show_errormessage (NULL, L"Administrative privileges are required!", ERROR_DS_INSUFF_ACCESS_RIGHTS, NULL);
		}

		return FALSE;
	}

#elif defined(APP_HAVE_SKIPUAC)
	// use "skipuac" feature
	if (!_r_sys_iselevated () && _r_skipuac_run ())
		return FALSE;
#endif // APP_NO_GUEST

	// set running flag
	_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex);

	// set updates path
#if defined(APP_HAVE_UPDATES)
	RtlSecureZeroMemory (&app_global.update.info, sizeof (R_UPDATE_INFO));

	// initialize objects
	app_global.update.info.components = _r_obj_createarray (sizeof (R_UPDATE_COMPONENT), NULL);
#endif // APP_HAVE_UPDATES

#if defined(APP_HAVE_SETTINGS)
	app_global.settings.page_list = _r_obj_createarray (sizeof (R_SETTINGS_PAGE), NULL);
#endif // APP_HAVE_SETTINGS

	// check for wow64 working and show warning if it is TRUE!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 () && !_r_sys_getopt (_r_sys_getimagecommandline (), L"nowow64", NULL))
	{
		_r_show_message (NULL, MB_OK | MB_ICONWARNING | MB_TOPMOST, _r_app_getname (), L"WoW64 warning!", L"You are attempting to run the 32-bit executable on 64-bit system.\r\nNote: add \"-nowow64\" argument to avoid this warning.");
		return FALSE;
	}
#endif // !_DEBUG && !_WIN64
#endif // !APP_CONSOLE

	// if profile directory does not exist, we cannot save configuration
	if (!_r_app_isreadonly ())
	{
		LPCWSTR directory = _r_app_getprofiledirectory ();

		if (directory)
		{
			if (!_r_fs_exists (directory))
			{
				_r_fs_mkdir (directory);
			}
		}
	}

	return TRUE;
}

LPCWSTR _r_app_getdirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		R_STRINGREF path;

		_r_obj_initializestringrefconst (&path, _r_sys_getimagepath ());

		cached_result = _r_path_getbasedirectory (&path);

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

LPCWSTR _r_app_getconfigpath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		PR_STRING buffer;
		PR_STRING new_result = NULL;

		// parse config path from arguments
		if (_r_sys_getopt (_r_sys_getimagecommandline (), L"ini", &buffer))
		{
			_r_str_trimstring2 (buffer, L".\\/\" ");

			_r_obj_movereference (&buffer, _r_str_expandenvironmentstring (&buffer->sr));

			if (!_r_obj_isstringempty (buffer))
			{
				if (PathGetDriveNumber (buffer->buffer) == -1)
				{
					_r_obj_movereference (&new_result, _r_obj_concatstrings (3, _r_app_getdirectory (), L"\\", buffer->buffer));
				}
				else
				{
					_r_obj_movereference (&new_result, _r_obj_reference (buffer));
				}

				// trying to create file
				if (!_r_fs_exists (new_result->buffer))
				{
					HANDLE hfile = CreateFile (new_result->buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

					if (!_r_fs_isvalidhandle (hfile))
					{
						_r_obj_clearreference (&new_result);
					}
					else
					{
						CloseHandle (hfile);
					}
				}
			}

			if (buffer)
				_r_obj_dereference (buffer);
		}

		// get configuration path
		if (_r_obj_isstringempty (new_result) || !_r_fs_exists (new_result->buffer))
		{
			if (_r_app_isportable ())
			{
				_r_obj_movereference (&new_result, _r_obj_concatstrings (4, _r_app_getdirectory (), L"\\", _r_app_getnameshort (), L".ini"));
			}
			else
			{
				_r_obj_movereference (&new_result, _r_path_getknownfolder (CSIDL_APPDATA, L"\\" APP_AUTHOR L"\\" APP_NAME L"\\" APP_NAME_SHORT L".ini"));
			}
		}

		cached_result = new_result;

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

LPCWSTR _r_app_getlogpath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_result = _r_obj_concatstrings (4, _r_app_getprofiledirectory (), L"\\", _r_app_getnameshort (), L"_debug.log");

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

#if !defined(APP_CONSOLE)
LPCWSTR _r_app_getlocalepath ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_result = _r_obj_concatstrings (4, _r_app_getdirectory (), L"\\", _r_app_getnameshort (), L".lng");

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}
#endif // !APP_CONSOLE

LPCWSTR _r_app_getprofiledirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		PR_STRING new_path = NULL;

		if (_r_app_isportable ())
		{
			new_path = _r_obj_createstring (_r_app_getdirectory ());
		}
		else
		{
			new_path = _r_path_getknownfolder (CSIDL_APPDATA, L"\\" APP_AUTHOR L"\\" APP_NAME);
		}

		if (new_path)
		{
			if (!_r_fs_exists (new_path->buffer))
				_r_fs_mkdir (new_path->buffer);
		}

		cached_result = new_path;

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

LPCWSTR _r_app_getuseragent ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		PR_STRING useragent_config = _r_config_getstring (L"UserAgent", NULL);

		if (useragent_config)
		{
			cached_result = _r_obj_createstring2 (useragent_config);

			_r_obj_dereference (useragent_config);
		}
		else
		{
			cached_result = _r_obj_concatstrings (6, _r_app_getname (), L"/", _r_app_getversion (), L" (+", _r_app_getwebsite_url (), L")");
		}

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

#if !defined(APP_CONSOLE)
LRESULT CALLBACK _r_app_maindlgproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#if defined(APP_HAVE_TRAY)
	if (app_global.main.taskbar_msg && msg == app_global.main.taskbar_msg)
	{
		if (app_global.main.wnd_proc)
			return CallWindowProc (app_global.main.wnd_proc, hwnd, RM_TASKBARCREATED, 0, 0);

		return FALSE;
	}
#endif // APP_HAVE_TRAY

	switch (msg)
	{
		case RM_LOCALIZE:
		{
			LRESULT result;

			if (app_global.main.wnd_proc)
			{
				result = CallWindowProc (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);

				RedrawWindow (hwnd, NULL, NULL, RDW_ERASENOW | RDW_INVALIDATE);
				DrawMenuBar (hwnd); // HACK!!!

				return result;
			}

			break;
		}

		case WM_DESTROY:
		{
			if ((_r_wnd_getstyle (hwnd) & WS_MAXIMIZEBOX))
			{
				_r_config_setbooleanex (L"IsMaximized", _r_wnd_ismaximized (hwnd), L"window");
			}

			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_SIZE:
		{
			if (wparam == SIZE_MAXIMIZED)
			{
				// prevent windows without maximize button to be maximized (dirty hack!!!)
				if (!(_r_wnd_getstyle (hwnd) & WS_MAXIMIZEBOX))
				{
					WINDOWPLACEMENT wpl = {0};

					wpl.length = sizeof (wpl);

					if (GetWindowPlacement (hwnd, &wpl))
					{
						wpl.showCmd = SW_RESTORE;

						SetWindowPlacement (hwnd, &wpl);

						return FALSE;
					}
				}
			}
#if defined(APP_HAVE_TRAY)
			else if (wparam == SIZE_MINIMIZED)
			{
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
				ShowWindow (hwnd, SW_HIDE);
				return TRUE;
			}

			break;
		}
#endif // APP_HAVE_TRAY

		case WM_SHOWWINDOW:
		{
			if (wparam && app_global.main.is_needmaximize)
			{
				ShowWindow (hwnd, SW_SHOWMAXIMIZED);
				app_global.main.is_needmaximize = FALSE;
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_DPICHANGED:
		{
			R_RECTANGLE rectangle;
			LPRECT rect;

			rect = (LPRECT)lparam;

			_r_wnd_recttorectangle (&rectangle, rect);

			_r_wnd_setposition (hwnd, &rectangle.position, (_r_wnd_getstyle (hwnd) & WS_SIZEBOX) ? &rectangle.size : NULL);

			break;
		}
	}

	// call main callback
	if (app_global.main.wnd_proc)
		return CallWindowProc (app_global.main.wnd_proc, hwnd, msg, wparam, lparam);

	return FALSE;
}

INT _r_app_getshowcode (_In_ HWND hwnd)
{
	STARTUPINFO startup_info = {0};
	INT show_code = SW_SHOWNORMAL;
	BOOLEAN is_windowhidden = FALSE;

	startup_info.cb = sizeof (startup_info);

	GetStartupInfo (&startup_info);

	if ((startup_info.dwFlags & STARTF_USESHOWWINDOW) != 0)
		show_code = startup_info.wShowWindow;

	// if window have tray - check arguments
#if defined(APP_HAVE_TRAY)
	if (_r_config_getboolean (L"IsStartMinimized", FALSE) || _r_sys_getopt (_r_sys_getimagecommandline (), L"minimized", NULL))
	{
		is_windowhidden = TRUE;
	}
#endif // APP_HAVE_TRAY

	if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
	{
		is_windowhidden = TRUE;
	}

	if ((_r_wnd_getstyle (hwnd) & WS_MAXIMIZEBOX) != 0)
	{
		if (show_code == SW_SHOWMAXIMIZED || _r_config_getbooleanex (L"IsMaximized", FALSE, L"window"))
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
	{
		show_code = SW_HIDE;
	}
#endif // APP_HAVE_TRAY

	return show_code;
}

HWND _r_app_createwindow (_In_ INT dlg_id, _In_opt_ LONG icon_id, _In_opt_ DLGPROC dlg_proc)
{
#ifdef APP_HAVE_UPDATES
	// configure components
	WCHAR locale_version[64];
	_r_str_fromlong64 (locale_version, RTL_NUMBER_OF (locale_version), _r_locale_getversion ());

	_r_update_addcomponent (_r_app_getname (), _r_app_getnameshort (), _r_app_getversion (), _r_app_getdirectory (), TRUE);
	_r_update_addcomponent (L"Language pack", L"language", locale_version, _r_app_getlocalepath (), FALSE);
#endif // APP_HAVE_UPDATES

	HWND hwnd;
	LONG dpi_value;

	// create main window
	hwnd = CreateDialogParam (NULL, MAKEINTRESOURCE (dlg_id), NULL, dlg_proc, 0);
	app_global.main.hwnd = hwnd;

	if (!hwnd)
		return NULL;

	// set window title
	SetWindowText (hwnd, _r_app_getname ());

	// set window icon
	if (icon_id)
	{
		dpi_value = _r_dc_getwindowdpi (hwnd);

		_r_wnd_seticon (hwnd,
						_r_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value), TRUE),
						_r_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), _r_dc_getsystemmetrics (SM_CXICON, dpi_value), TRUE)
		);
	}

	// set window prop
	SetProp (hwnd, _r_app_getname (), IntToPtr (42));

	// set window on top
	_r_wnd_top (hwnd, _r_config_getboolean (L"AlwaysOnTop", FALSE));

	// center window position
	_r_wnd_center (hwnd, NULL);

	// enable messages bypass uipi (win7+)
#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
#endif // !APP_NO_DEPRECATIONS
	{
		UINT messages[] = {
			WM_COPYDATA,
			WM_COPYGLOBALDATA,
			WM_DROPFILES,
#if defined(APP_HAVE_TRAY)
			app_global.main.taskbar_msg,
#endif // APP_HAVE_TRAY
		};

		_r_wnd_changemessagefilter (hwnd, messages, RTL_NUMBER_OF (messages), MSGFLT_ALLOW);
	}

	// subclass window
	app_global.main.wnd_proc = (WNDPROC)SetWindowLongPtr (hwnd, DWLP_DLGPROC, (LONG_PTR)_r_app_maindlgproc);

	// restore window position
	_r_window_restoreposition (hwnd, L"window");

	// restore window visibility (or not?)
#if !defined(APP_STARTMINIMIZED)
	ShowWindow (hwnd, _r_app_getshowcode (hwnd));
#endif // !APP_STARTMINIMIZED

	// common initialization
	SendMessage (hwnd, RM_INITIALIZE, 0, 0);
	SendMessage (hwnd, RM_LOCALIZE, 0, 0);

	PostMessage (hwnd, RM_INITIALIZE_POST, 0, 0);

#if defined(APP_HAVE_UPDATES)
	if (_r_config_getboolean (L"CheckUpdates", TRUE))
		_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

	return hwnd;
}

BOOLEAN _r_app_runasadmin ()
{
	BOOLEAN is_mutexdestroyed;

	is_mutexdestroyed = _r_mutex_destroy (&app_global.main.hmutex);

#if defined(APP_HAVE_SKIPUAC)
	if (_r_skipuac_run ())
		return TRUE;
#endif // APP_HAVE_SKIPUAC

	if (_r_sys_runasadmin (_r_sys_getimagepath (), _r_sys_getimagecommandline ()))
		return TRUE;

	if (is_mutexdestroyed)
		_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex); // restore mutex on error

	_r_sleep (500); // HACK!!! prevent loop

	return FALSE;
}

VOID _r_app_restart (_In_ HWND hwnd)
{
	WCHAR directory[256] = {0};
	BOOLEAN is_mutexdestroyed;

	if (_r_show_message (hwnd, MB_YESNO | MB_ICONQUESTION, _r_app_getname (), NULL, L"Restart is required to apply configuration, restart now?") != IDYES)
		return;

	GetCurrentDirectory (RTL_NUMBER_OF (directory), directory);

	is_mutexdestroyed = _r_mutex_destroy (&app_global.main.hmutex);

	if (!_r_sys_createprocessex (_r_sys_getimagepath (), _r_sys_getimagecommandline (), directory, NULL, SW_SHOW, 0))
	{
		if (is_mutexdestroyed)
			_r_mutex_create (_r_app_getmutexname (), &app_global.main.hmutex); // restore mutex on error

		return;
	}

	HWND hmain = _r_app_gethwnd ();

	if (hmain)
	{
		DestroyWindow (hmain);
		WaitForSingleObjectEx (hmain, 3000, FALSE); // wait for exit
	}

	ExitProcess (ERROR_SUCCESS);
}
#endif // !APP_CONSOLE

//
// Configuration
//

VOID _r_config_initialize ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_HASHTABLE config_table;

	if (_r_initonce_begin (&init_once))
	{
		_r_queuedlock_initialize (&app_global.config.lock);

		_r_initonce_end (&init_once);
	}

	config_table = _r_parseini (_r_app_getconfigpath (), NULL);

	_r_queuedlock_acquireexclusive (&app_global.config.lock);

	_r_obj_movereference (&app_global.config.table, config_table);

	_r_queuedlock_releaseexclusive (&app_global.config.lock);
}

BOOLEAN _r_config_getbooleanex (_In_ LPCWSTR key_name, _In_opt_ BOOLEAN def_value, _In_opt_ LPCWSTR section_name)
{
	PR_STRING string;
	BOOLEAN result;

	string = _r_config_getstringex (key_name, def_value ? L"true" : L"false", section_name);

	if (string)
	{
		result = _r_str_toboolean (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return FALSE;
}

INT _r_config_getintegerex (_In_ LPCWSTR key_name, _In_opt_ INT def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	INT result;

	_r_str_frominteger (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_tointeger (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

UINT _r_config_getuintegerex (_In_ LPCWSTR key_name, _In_opt_ UINT def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	UINT result;

	_r_str_fromuinteger (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_touinteger (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

LONG _r_config_getlongex (_In_ LPCWSTR key_name, _In_opt_ LONG def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	LONG result;

	_r_str_fromlong (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_tolong (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

LONG64 _r_config_getlong64ex (_In_ LPCWSTR key_name, _In_opt_ LONG64 def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	LONG64 result;

	_r_str_fromlong64 (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_tolong64 (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

ULONG _r_config_getulongex (_In_ LPCWSTR key_name, _In_opt_ ULONG def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	ULONG result;

	_r_str_fromulong (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_toulong (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

ULONG64 _r_config_getulong64ex (_In_ LPCWSTR key_name, _In_opt_ ULONG64 def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR number_string[64];
	PR_STRING string;
	ULONG64 result;

	_r_str_fromulong64 (number_string, RTL_NUMBER_OF (number_string), def_value);

	string = _r_config_getstringex (key_name, number_string, section_name);

	if (string)
	{
		result = _r_str_toulong64 (&string->sr);

		_r_obj_dereference (string);

		return result;
	}

	return 0;
}

VOID _r_config_getfontex (_In_ LPCWSTR key_name, _Inout_ PLOGFONT logfont, _In_ LONG dpi_value, _In_opt_ LPCWSTR section_name)
{
	PR_STRING font_config = _r_config_getstringex (key_name, NULL, section_name);

	if (font_config)
	{
		R_STRINGREF remaining_part;
		R_STRINGREF first_part;
		INT font_size;

		_r_obj_initializestringref2 (&remaining_part, font_config);

		// get font face name
		if (_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part))
		{
			_r_str_copystring (logfont->lfFaceName, RTL_NUMBER_OF (logfont->lfFaceName), &first_part);
		}

		// get font size
		_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part);

		font_size = _r_str_tointeger (&first_part);

		if (font_size)
			logfont->lfHeight = _r_dc_fontsizetoheight (font_size, dpi_value);

		// get font weight
		_r_str_splitatchar (&remaining_part, L';', &first_part, &remaining_part);
		logfont->lfWeight = _r_str_tolong (&first_part); // weight

		_r_obj_dereference (font_config);
	}

	// fill missed font values
	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof (ncm);

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		ncm.cbSize -= sizeof (INT); // xp support
#endif // !APP_NO_DEPRECATIONS

	if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
	{
		PLOGFONT system_font = &ncm.lfMessageFont;

		if (_r_str_isempty (logfont->lfFaceName))
			_r_str_copy (logfont->lfFaceName, LF_FACESIZE, system_font->lfFaceName);

		if (!logfont->lfHeight)
			logfont->lfHeight = system_font->lfHeight;

		if (!logfont->lfWeight)
			logfont->lfWeight = system_font->lfWeight;

		logfont->lfCharSet = system_font->lfCharSet;
		logfont->lfQuality = system_font->lfQuality;
	}
}

VOID _r_config_getsize (_In_ LPCWSTR key_name, _Out_ PR_SIZE size, _In_opt_ PR_SIZE def_value, _In_opt_ LPCWSTR section_name)
{
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING pair_config;

	size->cx = size->cy = 0; // initialize size values

	pair_config = _r_config_getstringex (key_name, NULL, section_name);

	if (pair_config)
	{
		_r_obj_initializestringref2 (&remaining_part, pair_config);

		// get x value
		_r_str_splitatchar (&remaining_part, L',', &first_part, &remaining_part);
		size->cx = _r_str_tolong (&first_part);

		// get y value
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
PR_STRING _r_config_getstringexpandex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name)
{
	PR_STRING string;
	PR_STRING config_value;

	config_value = _r_config_getstringex (key_name, def_value, section_name);

	if (config_value)
	{
		string = _r_str_expandenvironmentstring (&config_value->sr);

		if (string)
		{
			_r_obj_dereference (config_value);

			return string;
		}

		string = _r_obj_createstring2 (config_value);

		_r_obj_dereference (config_value);

		return string;
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _r_config_getstringex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_OBJECT_POINTER object_ptr;
	PR_STRING section_string;
	ULONG_PTR hash_code;

	if (_r_initonce_begin (&init_once))
	{
		_r_config_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.config.table)
		return NULL;

	if (section_name)
	{
		section_string = _r_obj_concatstrings (5, _r_app_getnameshort (), L"\\", section_name, L"\\", key_name);
	}
	else
	{
		section_string = _r_obj_concatstrings (3, _r_app_getnameshort (), L"\\", key_name);
	}

	hash_code = _r_obj_getstringhash (section_string);

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

	if (object_ptr)
	{
		if (!object_ptr->object_body)
		{
			if (!_r_str_isempty (def_value))
			{
				_r_obj_movereference (&object_ptr->object_body, _r_obj_createstring (def_value));
			}
		}

		return _r_obj_referencesafe (object_ptr->object_body);
	}

	return NULL;
}

VOID _r_config_setbooleanex (_In_ LPCWSTR key_name, _In_ BOOLEAN value, _In_opt_ LPCWSTR section_name)
{
	_r_config_setstringex (key_name, value ? L"true" : L"false", section_name);
}

VOID _r_config_setintegerex (_In_ LPCWSTR key_name, _In_ INT value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_frominteger (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setuintegerex (_In_ LPCWSTR key_name, _In_ UINT value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromuinteger (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setlongex (_In_ LPCWSTR key_name, _In_ LONG value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setlong64ex (_In_ LPCWSTR key_name, _In_ LONG64 value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setulongex (_In_ LPCWSTR key_name, _In_ ULONG value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromulong (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setulong64ex (_In_ LPCWSTR key_name, _In_ ULONG64 value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromulong64 (value_text, RTL_NUMBER_OF (value_text), value);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setfontex (_In_ LPCWSTR key_name, _In_ PLOGFONT logfont, _In_ LONG dpi_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[128];
	_r_str_printf (value_text, RTL_NUMBER_OF (value_text), L"%s;%" TEXT (PR_LONG) L";%" TEXT (PR_LONG), logfont->lfFaceName, logfont->lfHeight ? _r_dc_fontheighttosize (logfont->lfHeight, dpi_value) : 0, logfont->lfWeight);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setsize (_In_ LPCWSTR key_name, _In_ PR_SIZE size, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[128];
	_r_str_printf (value_text, RTL_NUMBER_OF (value_text), L"%" TEXT (PR_LONG) L",%" TEXT (PR_LONG), size->cx, size->cy);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setstringexpandex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value, _In_opt_ LPCWSTR section_name)
{
	PR_STRING string = NULL;

	if (value)
		string = _r_str_unexpandenvironmentstring (value);

	_r_config_setstringex (key_name, _r_obj_getstring (string), section_name);

	if (string)
		_r_obj_dereference (string);
}

VOID _r_config_setstringex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR value, _In_opt_ LPCWSTR section_name)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	WCHAR section_string[128];
	PR_STRING section_string_full;
	PR_OBJECT_POINTER object_ptr;
	ULONG_PTR hash_code;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_obj_ishashtableempty (app_global.config.table))
		{
			_r_config_initialize ();
		}

		_r_initonce_end (&init_once);
	}

	if (!app_global.config.table)
		return;

	if (section_name)
	{
		_r_str_printf (section_string, RTL_NUMBER_OF (section_string), L"%s\\%s", _r_app_getnameshort (), section_name);
		section_string_full = _r_obj_concatstrings (5, _r_app_getnameshort (), L"\\", section_name, L"\\", key_name);
	}
	else
	{
		_r_str_copy (section_string, RTL_NUMBER_OF (section_string), _r_app_getnameshort ());
		section_string_full = _r_obj_concatstrings (3, _r_app_getnameshort (), L"\\", key_name);
	}

	hash_code = _r_obj_getstringhash (section_string_full);

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

	if (object_ptr)
	{
		if (value)
		{
			_r_obj_movereference (&object_ptr->object_body, _r_obj_createstring (value));
		}
		else
		{
			_r_obj_clearreference (&object_ptr->object_body);
		}

		// write to configuration file
		if (!_r_app_isreadonly ())
		{
			WritePrivateProfileString (section_string, key_name, _r_obj_getstring (object_ptr->object_body), _r_app_getconfigpath ());
		}
	}
}

//
// Localization
//

#if !defined(APP_CONSOLE)
VOID _r_locale_initialize ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_HASHTABLE locale_table;
	PR_LIST locale_names;
	PR_STRING language_config;

	if (_r_initonce_begin (&init_once))
	{
		_r_queuedlock_initialize (&app_global.locale.lock);

		_r_initonce_end (&init_once);
	}

	language_config = _r_config_getstring (L"Language", NULL);

	if (!language_config)
	{
		if (app_global.locale.default_name)
		{
			_r_obj_movereference (&app_global.locale.current_name, _r_obj_createstring2 (app_global.locale.default_name));
		}
		else
		{
			_r_obj_clearreference (&app_global.locale.current_name);
		}
	}
	else
	{
		if (_r_str_isstartswith (&language_config->sr, &app_global.locale.resource_name->sr, TRUE))
		{
			_r_obj_movereference (&app_global.locale.current_name, _r_obj_createstring2 (app_global.locale.resource_name));
		}
		else
		{
			_r_obj_movereference (&app_global.locale.current_name, _r_obj_createstring2 (language_config));
		}

		_r_obj_dereference (language_config);
	}

	locale_names = _r_obj_createlist (&_r_obj_dereference);
	locale_table = _r_parseini (_r_app_getlocalepath (), locale_names);

	_r_queuedlock_acquireexclusive (&app_global.locale.lock);

	_r_obj_movereference (&app_global.locale.table, locale_table);
	_r_obj_movereference (&app_global.locale.available_list, locale_names);

	_r_queuedlock_releaseexclusive (&app_global.locale.lock);
}

VOID _r_locale_apply (_In_ PVOID hwnd, _In_ INT ctrl_id, _In_opt_ UINT menu_id)
{
	PR_STRING locale_name;
	SIZE_T locale_index;
	HWND hwindow;
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
		INT item_id;

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
		_r_obj_movereference (&app_global.locale.current_name, _r_obj_createstring2 (app_global.locale.resource_name));
	}
	else
	{
		locale_name = _r_obj_getlistitem (app_global.locale.available_list, locale_index);

		if (locale_name)
		{
			_r_obj_movereference (&app_global.locale.current_name, _r_obj_createstring2 (locale_name));
		}
		else
		{
			_r_obj_clearreference (&app_global.locale.current_name);
		}
	}

	_r_config_setstring (L"Language", _r_obj_getstring (app_global.locale.current_name));

	// refresh main window
	hwindow = _r_app_gethwnd ();

	if (hwindow)
		SendMessage (hwindow, RM_LOCALIZE, 0, 0);

#if defined(APP_HAVE_SETTINGS)
	// refresh settings window
	hwindow = _r_settings_getwindow ();

	if (hwindow)
		PostMessage (hwindow, RM_LOCALIZE, 0, 0);
#endif // APP_HAVE_SETTINGS
}

VOID _r_locale_enum (_In_ PVOID hwnd, _In_ INT ctrl_id, _In_opt_ UINT menu_id)
{
	HMENU hsubmenu;
	SIZE_T locale_count;
	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		hsubmenu = GetSubMenu (hwnd, ctrl_id);

		// clear menu
		_r_menu_clearitems (hsubmenu);

		AppendMenu (hsubmenu, MF_STRING, (UINT_PTR)menu_id, _r_obj_getstringorempty (app_global.locale.resource_name));
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
		AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
	}
	else
	{
		_r_ctrl_enable (hwnd, ctrl_id, TRUE);
	}

	PR_STRING locale_name;
	UINT index;
	BOOLEAN is_current;

	_r_queuedlock_acquireshared (&app_global.locale.lock);

	index = 1;

	for (SIZE_T i = 0; i < locale_count; i++)
	{
		locale_name = _r_obj_getlistitem (app_global.locale.available_list, i);

		if (!locale_name)
			continue;

		is_current = !_r_obj_isstringempty (app_global.locale.current_name) && (_r_str_isequal (&app_global.locale.current_name->sr, &locale_name->sr, TRUE));

		if (is_menu)
		{
			UINT menu_index;

			menu_index = menu_id + (INT)(INT_PTR)i + 2;

			AppendMenu (hsubmenu, MF_STRING, menu_index, locale_name->buffer);

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
PR_STRING _r_locale_getstringex (_In_ UINT uid)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_STRING hash_string;
	PR_STRING value_string;
	ULONG_PTR hash_code;

	if (_r_initonce_begin (&init_once))
	{
		_r_locale_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_global.locale.table)
		return NULL;

	if (app_global.locale.current_name)
	{
		hash_string = _r_format_string (L"%s\\%03" TEXT (PRIu32), app_global.locale.current_name->buffer, uid);
		hash_code = _r_obj_getstringhash (hash_string);

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
			hash_code = _r_obj_getstringhash (hash_string);

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
LPCWSTR _r_locale_getstring (_In_ UINT uid)
{
	PR_STRING string;
	LPCWSTR result;

	string = _r_locale_getstringex (uid);

	if (string)
	{
		result = string->buffer;
		_r_obj_dereference (string);

		return result;
	}

	return NULL;
}

LONG64 _r_locale_getversion ()
{
	WCHAR timestamp_string[32];
	R_STRINGREF string;
	ULONG length;

	// HACK!!! Use "Russian" section and default timestamp key (000) for compatibility with old releases...
	length = GetPrivateProfileString (L"Russian", L"000", NULL, timestamp_string, RTL_NUMBER_OF (timestamp_string), _r_app_getlocalepath ());

	if (length)
	{
		_r_obj_initializestringrefex (&string, timestamp_string, length * sizeof (WCHAR));

		return _r_str_tolong64 (&string);
	}

	return 0;
}
#endif // !APP_CONSOLE

#if defined(APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ()
{
	HKEY hkey;
	PR_STRING path;
	BOOLEAN is_enabled = FALSE;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
	{
		path = _r_reg_querystring (hkey, NULL, _r_app_getname ());

		if (path)
		{
			PathRemoveArgs (path->buffer);
			PathUnquoteSpaces (path->buffer);

			_r_obj_trimstringtonullterminator (path);

			if (_r_str_isequal2 (&path->sr, _r_sys_getimagepath (), TRUE))
			{
				is_enabled = TRUE;
			}

			_r_obj_dereference (path);
		}

		RegCloseKey (hkey);
	}

	return is_enabled;
}

BOOLEAN _r_autorun_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable)
{
	HKEY hkey;
	PR_STRING string;
	LSTATUS status;

	status = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);

	if (status == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			string = _r_obj_concatstrings (3, L"\"", _r_sys_getimagepath (), L"\" -minimized");

			status = RegSetValueEx (hkey, _r_app_getname (), 0, REG_SZ, (PBYTE)string->buffer, (ULONG)(string->length + sizeof (UNICODE_NULL)));

			_r_obj_dereference (string);
		}
		else
		{
			status = RegDeleteValue (hkey, _r_app_getname ());

			if (status == ERROR_FILE_NOT_FOUND)
			{
				status = ERROR_SUCCESS;
			}
		}

		RegCloseKey (hkey);
	}

	if (hwnd && status != ERROR_SUCCESS)
	{
		_r_show_errormessage (hwnd, NULL, status, NULL);
	}

	return (status == ERROR_SUCCESS);
}
#endif // APP_HAVE_AUTORUN

#if defined(APP_HAVE_UPDATES)
static NTSTATUS NTAPI _r_update_checkthread (_In_ PVOID arglist)
{
	PR_UPDATE_INFO update_info;
	HINTERNET hsession;

	update_info = arglist;

	hsession = _r_inet_createsession (_r_app_getuseragent ());

	if (hsession)
	{
		R_DOWNLOAD_INFO download_info;
		PR_STRING update_url;

		update_url = _r_obj_concatstrings (3, _r_app_getwebsite_url (), L"/update.php?api=3&product=", _r_app_getnameshort ());

		_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

		if (_r_inet_begindownload (hsession, update_url->buffer, &download_info) != ERROR_SUCCESS)
		{
			if (update_info->hparent)
			{
				WCHAR str_content[256];

#ifdef IDS_UPDATE_ERROR
				_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_ERROR));
#else
				_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update server connection error.");
#pragma PR_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

#ifndef APP_NO_DEPRECATIONS
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
				{
					_r_update_pagenavigate (update_info->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)&app_global.update.info);
				}
#ifndef APP_NO_DEPRECATIONS
				else
				{
					_r_show_message (update_info->hparent, MB_OK | MB_ICONWARNING, NULL, NULL, str_content);
				}
#endif // !APP_NO_DEPRECATIONS
			}
		}
		else
		{
			PR_UPDATE_COMPONENT update_component;
			PR_HASHTABLE string_table;
			PR_STRING string_value;

			WCHAR updates_text[512] = {0};
			PR_STRING version_string = NULL;
			R_STRINGREF new_version_string;
			R_STRINGREF new_url_string;
			BOOLEAN is_updateavailable = FALSE;

			string_table = _r_str_unserialize (&download_info.string->sr, L';', L'=');

			if (string_table)
			{
				for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
				{
					update_component = _r_obj_getarrayitem (update_info->components, i);

					if (!update_component)
						continue;

					string_value = _r_obj_findhashtablepointer (string_table, _r_obj_getstringhash (update_component->short_name));

					if (string_value)
					{
						if (_r_str_splitatchar (&string_value->sr, L'|', &new_version_string, &new_url_string))
						{
							if (_r_str_versioncompare (&update_component->version->sr, &new_version_string) == -1)
							{
								is_updateavailable = TRUE;
								update_component->is_haveupdate = TRUE;

								_r_obj_movereference (&version_string, _r_util_versionformat (&new_version_string));

								_r_obj_movereference (&update_component->new_version, _r_obj_createstring3 (&new_version_string));
								_r_obj_movereference (&update_component->url, _r_obj_createstring3 (&new_url_string));

								_r_str_appendformat (updates_text, RTL_NUMBER_OF (updates_text), L"%s %s\r\n", _r_obj_getstring (update_component->full_name), version_string->buffer);

								if (update_component->is_installer)
								{
									PR_STRING string = _r_obj_concatstrings (6, _r_sys_gettempdirectory (), L"\\", _r_app_getnameshort (), L"-", _r_obj_getstring (update_component->new_version), L".exe");

									_r_obj_movereference (&update_component->temp_path, string);

									_r_obj_dereference (string_value);

									// do not check components when new version of application available
									break;
								}
								else
								{
									PR_STRING string = _r_obj_concatstrings (8, _r_sys_gettempdirectory (), L"\\", _r_app_getnameshort (), L"-", _r_obj_getstring (update_component->short_name), L"-", _r_obj_getstring (update_component->new_version), L".tmp");

									_r_obj_movereference (&update_component->temp_path, string);
								}
							}
						}

						_r_obj_dereference (string_value);
					}
				}

				if (version_string)
				{
					_r_obj_dereference (version_string);
				}

				_r_obj_dereference (string_table);
			}

			if (is_updateavailable)
			{
				_r_str_trim (updates_text, L"\r\n ");

				WCHAR str_main[256];

#ifdef IDS_UPDATE_YES
				_r_str_copy (str_main, RTL_NUMBER_OF (str_main), _r_locale_getstring (IDS_UPDATE_YES));
#else
				_r_str_copy (str_main, RTL_NUMBER_OF (str_main), L"Update available. Download and install now?");
#pragma PR_PRINT_WARNING(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

#ifndef APP_NO_DEPRECATIONS
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
				{
					_r_update_pagenavigate (update_info->htaskdlg, NULL, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)&app_global.update.info);
				}
#ifndef APP_NO_DEPRECATIONS
				else
				{
					if (_r_show_message (update_info->hparent, MB_YESNO | MB_USERICON, NULL, str_main, updates_text) == IDYES)
					{
						_r_shell_opendefault (_r_app_getwebsite_url ());
					}
				}
#endif // !APP_NO_DEPRECATIONS

			}
			else
			{
				if (update_info->htaskdlg)
				{
					WCHAR str_content[256];

#ifdef IDS_UPDATE_NO
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_NO));
#else
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"No updates available.");
#pragma PR_PRINT_WARNING(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO

					_r_update_pagenavigate (update_info->htaskdlg, NULL, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)&app_global.update.info);
				}
			}

			_r_config_setlong64 (L"CheckUpdatesLast", _r_unixtime_now ());

			_r_inet_destroydownload (&download_info);
		}

		_r_obj_dereference (update_url);

		_r_inet_close (hsession);
	}

	update_info->is_checking = FALSE;

	return STATUS_SUCCESS;
}

VOID _r_update_check (_In_opt_ HWND hparent)
{
	PR_UPDATE_INFO update_info;

	update_info = &app_global.update.info;

	if (update_info->is_checking)
		return;

	if (!hparent && (!_r_config_getboolean (L"CheckUpdates", TRUE) || (_r_unixtime_now () - _r_config_getlong64 (L"CheckUpdatesLast", 0)) <= APP_UPDATE_PERIOD))
		return;

	update_info->is_checking = TRUE;
	update_info->htaskdlg = NULL;
	update_info->hparent = hparent;

	if (update_info->hparent)
	{
#ifndef APP_NO_DEPRECATIONS
		if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // APP_NO_DEPRECATIONS
		{
			WCHAR str_content[256];

#ifdef IDS_UPDATE_INIT
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_INIT));
#else
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Checking for new releases...");
#pragma PR_PRINT_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

			_r_update_pagenavigate (NULL, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)update_info);
		}
	}
	else
	{
		_r_sys_createthread2 (&_r_update_checkthread, update_info);
	}
}

BOOLEAN NTAPI _r_update_downloadcallback (_In_ ULONG total_written, _In_ ULONG total_length, _In_ PVOID param)
{
	PR_UPDATE_INFO update_info;

	update_info = param;

	if (update_info->htaskdlg)
	{
		LONG percent = _r_calc_percentof (total_written, total_length);

		WCHAR str_content[256];

#ifdef IDS_UPDATE_DOWNLOAD
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s %" TEXT (PR_LONG) L"%%", _r_locale_getstring (IDS_UPDATE_DOWNLOAD), percent);
#else
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"Downloading update... %" PR_LONG L"%%", percent);
#pragma PR_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

		SendMessage (update_info->htaskdlg, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
		SendMessage (update_info->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
	}

	return TRUE;
}

NTSTATUS NTAPI _r_update_downloadthread (_In_ PVOID arglist)
{
	PR_UPDATE_INFO update_info;

	HINTERNET hsession;
	HWND hwindow;

	BOOLEAN is_downloaded = FALSE;
	BOOLEAN is_downloaded_installer = FALSE;
	BOOLEAN is_updated = FALSE;

	update_info = arglist;

	hsession = _r_inet_createsession (_r_app_getuseragent ());

	if (hsession)
	{
		PR_UPDATE_COMPONENT update_component;

		for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
		{
			update_component = _r_obj_getarrayitem (update_info->components, i);

			if (!update_component)
				continue;

			if (update_component->is_haveupdate)
			{
				if (!_r_obj_isstringempty (update_component->temp_path))
				{
					HANDLE hfile = CreateFile (update_component->temp_path->buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

					if (_r_fs_isvalidhandle (hfile))
					{
						R_DOWNLOAD_INFO download_info;

						_r_inet_initializedownload (&download_info, hfile, &_r_update_downloadcallback, update_info);

						if (_r_inet_begindownload (hsession, update_component->url->buffer, &download_info) == ERROR_SUCCESS)
						{
							is_downloaded = TRUE;
							update_component->is_haveupdate = FALSE;

							_r_inet_destroydownload (&download_info); // required!

							if (update_component->is_installer)
							{
								is_downloaded_installer = TRUE;
								break;
							}
							else
							{
								LPCWSTR path = _r_obj_getstring (update_component->target_path);

								if (path)
								{
									// copy required files
									SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL);

									_r_fs_movefile (update_component->temp_path->buffer, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
									_r_fs_deletefile (update_component->temp_path->buffer, TRUE);

									// set new version
									_r_obj_movereference (&update_component->version, update_component->new_version);
									update_component->new_version = NULL;

									is_updated = TRUE;
								}
							}
						}
						else
						{
							_r_inet_destroydownload (&download_info); // required!
						}
					}
				}
			}
		}

		_r_inet_close (hsession);
	}

	// show result text
	WCHAR str_content[256];

	if (is_downloaded)
	{
		if (is_downloaded_installer)
		{
#ifdef IDS_UPDATE_INSTALL
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_INSTALL));
#else
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update available. Do you want to install it now?");
#pragma PR_PRINT_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL
		}
		else
		{
#ifdef IDS_UPDATE_DONE
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_DONE));
#else
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Downloading update finished.");
#pragma PR_PRINT_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
		}
	}
	else
	{
#ifdef IDS_UPDATE_ERROR
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_ERROR));
#else
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update server connection error.");
#pragma PR_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
	}

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !APP_NO_DEPRECATIONS
		if (update_info->htaskdlg)
		{
			_r_update_pagenavigate (update_info->htaskdlg, (is_downloaded ? NULL : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)&app_global.update.info);
		}
#if !defined(APP_NO_DEPRECATIONS)
	}
	else
	{
		if (update_info->hparent)
		{
			_r_show_message (update_info->hparent, is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONWARNING), NULL, NULL, str_content);
		}
	}
#endif // !APP_NO_DEPRECATIONS

	// install update
	if (is_updated)
	{
		_r_config_initialize (); // reload config
		_r_locale_initialize (); // reload locale

		hwindow = _r_app_gethwnd ();

		if (hwindow)
		{
			SendMessage (hwindow, RM_CONFIG_UPDATE, 0, 0);
			SendMessage (hwindow, RM_INITIALIZE, 0, 0);

			SendMessage (hwindow, RM_LOCALIZE, 0, 0);
		}
	}

	return STATUS_SUCCESS;
}

HRESULT CALLBACK _r_update_pagecallback (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam, _In_ LONG_PTR param)
{
	PR_UPDATE_INFO update_info = (PR_UPDATE_INFO)param;

	switch (msg)
	{
		case TDN_CREATED:
		{
			update_info->htaskdlg = hwnd;

			SendMessage (hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			SendMessage (hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			if (update_info->hparent)
			{
				_r_wnd_center (hwnd, update_info->hparent);
				_r_wnd_top (hwnd, TRUE);
			}

			_r_sys_createthread2 (&_r_update_checkthread, update_info);

			break;
		}

		case TDN_BUTTON_CLICKED:
		{
			if (wparam == IDYES)
			{
				WCHAR str_content[256];

#ifdef IDS_UPDATE_DOWNLOAD
				_r_str_printf (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_DOWNLOAD), 0);
#else
				_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Downloading update...");
#pragma PR_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif

				if (NT_SUCCESS (_r_sys_createthread2 (&_r_update_downloadthread, update_info)))
				{
					_r_update_pagenavigate (hwnd, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)update_info);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				PR_UPDATE_COMPONENT update_component;

				for (SIZE_T i = 0; i < _r_obj_getarraysize (update_info->components); i++)
				{
					update_component = _r_obj_getarrayitem (update_info->components, i);

					if (update_component && update_component->is_haveupdate && update_component->is_installer)
					{
						_r_update_install (update_component->temp_path->buffer);
						break;
					}

				}

				return S_FALSE;
			}

			break;
		}
	}

	return S_OK;
}

INT _r_update_pagenavigate (_In_opt_ HWND htaskdlg, _In_opt_ LPCWSTR main_icon, _In_ TASKDIALOG_FLAGS flags, _In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons, _In_opt_ LPCWSTR main, _In_opt_ LPCWSTR content, _In_ LONG_PTR param)
{
	TASKDIALOGCONFIG tdc = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | flags;
	tdc.hwndParent = _r_app_gethwnd ();
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.dwCommonButtons = buttons;
	tdc.pfCallback = &_r_update_pagecallback;
	tdc.lpCallbackData = param;

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

	INT command_id = 0;

	if (htaskdlg)
	{
		SendMessage (htaskdlg, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);
	}
	else
	{
		_r_msg_taskdialog (&tdc, &command_id, NULL, NULL);
	}

	return command_id;
}

VOID _r_update_addcomponent (_In_opt_ LPCWSTR full_name, _In_opt_ LPCWSTR short_name, _In_opt_ LPCWSTR version, _In_opt_ LPCWSTR target_path, _In_ BOOLEAN is_installer)
{
	R_UPDATE_COMPONENT update_component = {0};

	if (full_name)
		update_component.full_name = _r_obj_createstring (full_name);

	if (short_name)
		update_component.short_name = _r_obj_createstring (short_name);

	if (version)
		update_component.version = _r_obj_createstring (version);

	if (target_path)
		update_component.target_path = _r_obj_createstring (target_path);

	update_component.is_installer = is_installer;

	_r_obj_addarrayitem (app_global.update.info.components, &update_component);
}

BOOLEAN _r_update_install (_In_ LPCWSTR install_path)
{
	PR_STRING cmd_string;
	BOOLEAN is_success;

	cmd_string = _r_format_string (L"\"%s\" /S /D=\"%s\"",
								   install_path,
								   _r_app_getdirectory ()
	);

	;

	if (!_r_sys_runasadmin (install_path, _r_obj_getstring (cmd_string)))
	{
		R_ERROR_INFO error_info = {0};
		error_info.description = install_path;

		_r_show_errormessage (NULL, NULL, GetLastError (), &error_info);

		is_success = FALSE;
	}
	else
	{
		is_success = TRUE;
	}

	if (cmd_string)
		_r_obj_dereference (cmd_string);

	return is_success;
}

#endif // APP_HAVE_UPDATES

FORCEINLINE LPCWSTR _r_logleveltostring (_In_ R_LOG_LEVEL log_level)
{
	if (log_level == LOG_LEVEL_DEBUG)
		return L"Debug";

	if (log_level == LOG_LEVEL_INFO)
		return L"LOG_LEVEL_INFO";

	if (log_level == LOG_LEVEL_WARNING)
		return L"Warning";

	if (log_level == LOG_LEVEL_ERROR)
		return L"Error";

	if (log_level == LOG_LEVEL_CRITICAL)
		return L"Critical";

	return NULL;
}

FORCEINLINE ULONG _r_logleveltrayicon (_In_ R_LOG_LEVEL log_level)
{
	if (log_level == LOG_LEVEL_INFO)
		return NIIF_INFO;

	if (log_level == LOG_LEVEL_WARNING)
		return NIIF_WARNING;

	if (log_level == LOG_LEVEL_ERROR || log_level == LOG_LEVEL_CRITICAL)
		return NIIF_ERROR;

	return NIIF_NONE;
}

VOID _r_log (_In_ R_LOG_LEVEL log_level, _In_opt_ LPCGUID tray_guid, _In_ LPCWSTR title, _In_ ULONG code, _In_opt_ LPCWSTR description)
{
	PR_STRING error_string;
	PR_STRING date_string;
	LPCWSTR level_string;
	LONG64 current_timestamp;
	ULONG unused;

	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE hfile = NULL;
	static INT log_level_config = 0;

	if (_r_initonce_begin (&init_once))
	{
		log_level_config = _r_config_getinteger (L"ErrorLevel", 0);

		// write to file only when readonly mode is not specified
		if (!_r_app_isreadonly ())
		{
			LPCWSTR path = _r_app_getlogpath ();

			if (path)
			{
				hfile = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

				if (_r_fs_isvalidhandle (hfile))
				{
					if (GetLastError () != ERROR_ALREADY_EXISTS)
					{
						BYTE bom[] = {0xFF, 0xFE};

						WriteFile (hfile, bom, sizeof (bom), &unused, NULL); // write utf-16 le byte order mask
						WriteFile (hfile, PR_DEBUG_HEADER, (ULONG)(_r_str_getlength (PR_DEBUG_HEADER) * sizeof (WCHAR)), &unused, NULL); // adds csv header
					}
					else
					{
						_r_fs_setpos (hfile, 0, FILE_END);
					}
				}
			}
		}

		_r_initonce_end (&init_once);
	}

	if (log_level_config >= log_level)
		return;

	current_timestamp = _r_unixtime_now ();
	date_string = _r_format_unixtimeex (current_timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	level_string = _r_logleveltostring (log_level);

	// print log for debuggers
	_r_debug_v (L"[%s], %s, 0x%08" TEXT (PRIX32) L", %s\r\n",
				level_string,
				title,
				code,
				description
	);

	if (_r_fs_isvalidhandle (hfile))
	{
		error_string = _r_format_string (
			L"\"%s\",\"%s\",\"%s\",\"0x%08" TEXT (PRIX32) L"\",\"%s\"" L",\"%s\",\"%d.%d build %d\"\r\n",
			level_string,
			_r_obj_getstring (date_string),
			title,
			code,
			description,
			_r_app_getversion (),
			NtCurrentPeb ()->OSMajorVersion,
			NtCurrentPeb ()->OSMinorVersion,
			NtCurrentPeb ()->OSBuildNumber
		);

		if (error_string)
		{
			WriteFile (hfile, error_string->buffer, (ULONG)error_string->length, &unused, NULL);

			_r_obj_dereference (error_string);
		}
	}

	if (date_string)
		_r_obj_dereference (date_string);

	// show tray balloon
#if defined(APP_HAVE_TRAY)
	if (tray_guid)
	{
		if (_r_config_getboolean (L"IsErrorNotificationsEnabled", TRUE))
		{
			if ((current_timestamp - _r_config_getlong64 (L"ErrorNotificationsTimestamp", 0)) >= _r_config_getlong64 (L"ErrorNotificationsPeriod", 4)) // check for timeout (sec.)
			{
				_r_tray_popup (_r_app_gethwnd (), tray_guid, _r_logleveltrayicon (log_level) | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), _r_app_getname (), L"Something went wrong. Open debug log file in profile directory.");

				_r_config_setlong64 (L"ErrorNotificationsTimestamp", current_timestamp);
			}
		}
	}
#endif // APP_HAVE_TRAY
}

VOID _r_log_v (_In_ R_LOG_LEVEL log_level, _In_opt_ LPCGUID tray_guid, _In_ LPCWSTR title, _In_ ULONG code, _In_ _Printf_format_string_ LPCWSTR format, ...)
{
	WCHAR string[1024];
	va_list arg_ptr;

	if (!format)
		return;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	_r_log (log_level, tray_guid, title, code, string);
}

#if !defined(APP_CONSOLE)
VOID _r_show_aboutmessage (_In_opt_ HWND hwnd)
{
	static BOOLEAN is_opened = FALSE;

	if (is_opened)
		return;

	is_opened = TRUE;

	LPCWSTR str_title;
	WCHAR str_content[512];

#ifdef IDS_ABOUT
	str_title = _r_locale_getstring (IDS_ABOUT);
#else
	str_title = L"About";
#pragma PR_PRINT_WARNING(IDS_ABOUT)
#endif

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
	{
		INT command_id = 0;

		TASKDIALOGCONFIG tdc = {0};
		TASKDIALOG_BUTTON td_buttons[2] = {0};

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
		tdc.pszMainIcon = MAKEINTRESOURCE (100);
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

		_r_str_printf (str_content, RTL_NUMBER_OF (str_content),
					   L"Version %s %s, %" TEXT (PRIi32) L"-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>",
					   _r_app_getversion (),
					   _r_app_getversiontype (),
					   _r_app_getarch (),
					   _r_app_getcopyright (),
					   _r_app_getwebsite_url (),
					   _r_app_getwebsite_url () + 8,
					   _r_app_getsources_url (),
					   _r_app_getsources_url () + 8
		);

		tdc.pszFooter = L"This program is free software; you can redistribute it and/or modify it under the terms of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License 3</a> as published by the Free Software Foundation.";

		if (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL))
		{
			if (command_id == td_buttons[0].nButtonID)
				_r_shell_opendefault (_r_app_getdonate_url ());
		}
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content),
					   L"%s\r\n\r\nVersion %s %s, %" TEXT (PRIi32) L"-bit (Unicode)\r\n%s\r\n\r\n%s | %s\r\n\r\nThis program is free software; you can redistribute it and/\r\nor modify it under the terms of the GNU General Public\r\nLicense 3 as published by the Free Software Foundation.",
					   _r_app_getname (),
					   _r_app_getversion (),
					   _r_app_getversiontype (),
					   _r_app_getarch (),
					   _r_app_getcopyright (),
					   _r_app_getwebsite_url () + 8,
					   _r_app_getsources_url () + 8
		);

		MSGBOXPARAMS mbp = {0};

		mbp.cbSize = sizeof (mbp);
		mbp.dwStyle = MB_OK | MB_TOPMOST | MB_USERICON;
		mbp.hwndOwner = hwnd;
		mbp.hInstance = _r_sys_getimagebase ();
		mbp.lpszCaption = str_title;
		mbp.lpszText = str_content;

#if defined(IDI_MAIN)
		mbp.lpszIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
		mbp.lpszIcon = MAKEINTRESOURCE (100);
#pragma PR_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

		MessageBoxIndirect (&mbp);
	}
#endif // !APP_NO_DEPRECATIONS

	is_opened = FALSE;
}

VOID _r_show_errormessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ ULONG error_code, _In_opt_ PR_ERROR_INFO error_info_ptr)
{
	HLOCAL buffer = NULL;
	HINSTANCE hmodule;

	if (error_info_ptr && error_info_ptr->hmodule)
	{
		hmodule = error_info_ptr->hmodule;
	}
	else
	{
		hmodule = GetModuleHandle (L"kernel32.dll"); // FORMAT_MESSAGE_FROM_SYSTEM
	}

	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, hmodule, error_code, 0, (LPWSTR)&buffer, 0, NULL);

	R_STRINGBUILDER string_builder;
	LPCWSTR str_main;
	LPCWSTR str_footer;
	PR_STRING string;

	str_main = main ? main : L"It happens ;(";
	str_footer = L"This information may provide clues as to what went wrong and how to fix it.";

	if (buffer)
		_r_str_trim (buffer, L"\r\n ");

	_r_obj_initializestringbuilder (&string_builder);

	_r_obj_appendstringbuilderformat (&string_builder, L"%s (0x%08" TEXT (PRIX32) L")", buffer ? (LPWSTR)buffer : L"n/a", error_code);

	if (error_info_ptr && error_info_ptr->description)
	{
		_r_obj_appendstringbuilderformat (&string_builder, L"\r\n\r\n\"%s\"", error_info_ptr->description);
	}

	string = _r_obj_finalstringbuilder (&string_builder);

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
	{
		TASKDIALOGCONFIG tdc = {0};
		TASKDIALOG_BUTTON td_buttons[2] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_NO_SET_FOREGROUND | TDF_SIZE_TO_CONTENT;
		tdc.hwndParent = hwnd;
		tdc.hInstance = _r_sys_getimagebase ();
		tdc.pszFooterIcon = TD_WARNING_ICON;
		tdc.pszWindowTitle = _r_app_getname ();
		tdc.pszMainInstruction = str_main;
		tdc.pszContent = string->buffer;
		tdc.pszFooter = str_footer;
		tdc.pfCallback = &_r_msg_callback;
		tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

		tdc.pButtons = td_buttons;
		tdc.cButtons = RTL_NUMBER_OF (td_buttons);

		td_buttons[0].nButtonID = IDYES;
		td_buttons[1].nButtonID = IDCLOSE;

		tdc.nDefaultButton = td_buttons[1].nButtonID;

#if defined(IDS_COPY)
		td_buttons[0].pszButtonText = _r_locale_getstring (IDS_COPY);
#else
		td_buttons[0].pszButtonText = L"Copy";
#pragma PR_PRINT_WARNING(IDS_COPY)
#endif // IDS_COPY

#if defined(IDS_CLOSE)
		td_buttons[1].pszButtonText = _r_locale_getstring (IDS_CLOSE);
#else
		td_buttons[1].pszButtonText = L"Close";
#pragma PR_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

		INT command_id;

		if (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL))
		{
			if (command_id == td_buttons[0].nButtonID)
			{
				_r_clipboard_set (NULL, &string->sr);
			}
		}
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		PR_STRING message_string;

		message_string = _r_obj_concatstrings (5, str_main, L"\r\n\r\n", string->buffer, L"\r\n\r\n", str_footer);

		MessageBox (hwnd, message_string->buffer, _r_app_getname (), MB_OK | MB_ICONERROR);

		_r_obj_dereference (message_string);
	}
#endif // !APP_NO_DEPRECATIONS

	_r_obj_deletestringbuilder (&string_builder);

	if (buffer)
		LocalFree (buffer);
}

BOOLEAN _r_show_confirmmessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ LPCWSTR text, _In_opt_ LPCWSTR config_key)
{
	if (config_key && !_r_config_getboolean (config_key, TRUE))
		return TRUE;

	INT command_id = 0;
	BOOL is_flagchecked = FALSE;

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
	{
		TASKDIALOGCONFIG tdc = {0};

		WCHAR str_flag[128];

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
			_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), _r_locale_getstring (IDS_QUESTION_FLAG_CHK));
#else
			_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), L"Do not ask again");
#pragma PR_PRINT_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK

			tdc.pszVerificationText = str_flag;
		}

		if (main)
			tdc.pszMainInstruction = main;

		if (text)
			tdc.pszContent = text;

		_r_msg_taskdialog (&tdc, &command_id, NULL, &is_flagchecked);
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		if (config_key)
		{
			HKEY hkey;
			WCHAR config_string[128];

			_r_str_printf (config_string, RTL_NUMBER_OF (config_string), L"%s\\%s", _r_app_getnameshort (), config_key);

			command_id = SHMessageBoxCheck (hwnd, text, _r_app_getname (), MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST, IDOK, config_string);

			// get checkbox value from registry
			if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\DontShowMeThisDialogAgain", 0, KEY_WRITE | KEY_READ, &hkey) == ERROR_SUCCESS)
			{
				if (command_id == IDOK || command_id == IDYES)
					is_flagchecked = (RegQueryValueEx (hkey, config_string, 0, NULL, NULL, NULL) == ERROR_SUCCESS);

				RegDeleteValue (hkey, config_string);

				RegCloseKey (hkey);
			}
		}
		else
		{
			return (MessageBox (hwnd, text, _r_app_getname (), MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES); // fallback!
		}
	}
#endif // !APP_NO_DEPRECATIONS

	if (command_id == IDOK || command_id == IDYES)
	{
		if (config_key && is_flagchecked)
			_r_config_setboolean (config_key, FALSE);

		return TRUE;
	}

	return FALSE;
}

INT _r_show_message (_In_opt_ HWND hwnd, _In_ ULONG flags, _In_opt_ LPCWSTR title, _In_opt_ LPCWSTR main, _In_ LPCWSTR content)
{
#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
	{
		TASKDIALOGCONFIG tdc = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
		tdc.hwndParent = hwnd;
		tdc.hInstance = _r_sys_getimagebase ();
		tdc.pfCallback = &_r_msg_callback;
		tdc.pszWindowTitle = title ? title : _r_app_getname ();
		tdc.pszMainInstruction = main;
		tdc.pszContent = content;

		// set icons
		if ((flags & MB_ICONMASK) == MB_USERICON)
		{
#ifdef IDI_MAIN
			tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
			tdc.pszMainIcon = MAKEINTRESOURCE (100);
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
		if ((flags & MB_DEFMASK) == MB_DEFBUTTON2)
			tdc.nDefaultButton = IDNO;

		// set options
		if ((flags & MB_TOPMOST) != 0)
			tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

		INT command_id;

		if (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL))
			return command_id;
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		return MessageBox (hwnd, content, title ? title : _r_app_getname (), flags);
	}
#endif // !APP_NO_DEPRECATIONS

	return 0;
}

VOID _r_window_restoreposition (_In_ HWND hwnd, _In_ LPCWSTR window_name)
{
	R_RECTANGLE rectangle_new = {0};
	R_RECTANGLE rectangle_current;
	LONG dpi_value;
	BOOLEAN is_resizeavailable;

	if (!_r_wnd_getposition (hwnd, &rectangle_current))
		return;

	_r_config_getsize (L"Position", &rectangle_new.position, &rectangle_current.position, window_name);

	is_resizeavailable = !!(_r_wnd_getstyle (hwnd) & WS_SIZEBOX);

	if (is_resizeavailable)
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

	_r_wnd_adjustworkingarea (NULL, &rectangle_new);

	_r_wnd_setposition (hwnd, &rectangle_new.position, is_resizeavailable ? &rectangle_new.size : NULL);
}

VOID _r_window_saveposition (_In_ HWND hwnd, _In_ LPCWSTR window_name)
{
	MONITORINFO monitor_info = {0};
	R_RECTANGLE rectangle;

	monitor_info.cbSize = sizeof (monitor_info);

	if (!_r_wnd_getposition (hwnd, &rectangle))
		return;

	if (GetMonitorInfo (MonitorFromWindow (hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
	{
		rectangle.left += monitor_info.rcWork.left - monitor_info.rcMonitor.left;
		rectangle.top += monitor_info.rcWork.top - monitor_info.rcMonitor.top;
	}

	_r_config_setsize (L"Position", &rectangle.position, window_name);

	if ((_r_wnd_getstyle (hwnd) & WS_SIZEBOX))
	{
		_r_dc_getsizedpivalue (&rectangle.size, _r_dc_getwindowdpi (hwnd), FALSE);

		_r_config_setsize (L"Size", &rectangle.size, window_name);
	}
}
#endif // !APP_CONSOLE

//
// Settings window
//

#if defined(APP_HAVE_SETTINGS)
VOID _r_settings_addpage (_In_ INT dlg_id, _In_ UINT locale_id)
{
	R_SETTINGS_PAGE settings_page = {0};

	settings_page.dlg_id = dlg_id;
	settings_page.locale_id = locale_id;

	_r_obj_addarrayitem (app_global.settings.page_list, &settings_page);
}

VOID _r_settings_adjustchild (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HWND hchild)
{
#if !defined(APP_HAVE_SETTINGS_TABS)
	RECT rc_tree;
	RECT rc_child;

	if (!GetWindowRect (GetDlgItem (hwnd, ctrl_id), &rc_tree) || !GetClientRect (hchild, &rc_child))
		return;

	MapWindowPoints (NULL, hwnd, (PPOINT)&rc_tree, 2);
	SetWindowPos (hchild, NULL, (rc_tree.left * 2) + _r_calc_rectwidth (&rc_tree), rc_tree.top, rc_child.right, rc_child.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

#else
	_r_tab_adjustchild (hwnd, ctrl_id, hchild);
#endif // !APP_HAVE_SETTINGS_TABS
}

VOID _r_settings_createwindow (_In_ HWND hwnd, _In_opt_ DLGPROC dlg_proc, _In_opt_ INT dlg_id)
{
	assert (!_r_obj_isarrayempty (app_global.settings.page_list));

	if (_r_settings_getwindow ())
	{
		_r_wnd_toggle (_r_settings_getwindow (), TRUE);
		return;
	}

	if (dlg_id)
		_r_config_setinteger (L"SettingsLastPage", dlg_id);

	if (dlg_proc)
		app_global.settings.wnd_proc = dlg_proc;

	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SHORT width = 0;
	static SHORT height = 0;

	// calculate dialog size
	if (_r_initonce_begin (&init_once))
	{
		LPDLGTEMPLATEEX dlg_template;
		PR_SETTINGS_PAGE ptr_page;

		for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
		{
			ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

			if (!ptr_page || !ptr_page->dlg_id)
				continue;

			dlg_template = _r_res_loadresource (_r_sys_getimagebase (), MAKEINTRESOURCE (ptr_page->dlg_id), RT_DIALOG, NULL);

			if (dlg_template && (dlg_template->style & WS_CHILD))
			{
				if (width < dlg_template->cx)
					width = dlg_template->cx;

				if (height < dlg_template->cy)
					height = dlg_template->cy;
			}
		}

#if defined(APP_HAVE_SETTINGS_TABS)
		height += 38;
		width += 18;
#else
		height += 38;
		width += 112;
#endif

		_r_initonce_end (&init_once);
	}

#if defined(APP_HAVE_SETTINGS_TABS)
	const WORD controls = 4;
#else
	const WORD controls = 3;
#endif

	const SIZE_T size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) + ((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;
	PVOID buffer = _r_mem_allocatezero (size);
	PBYTE ptr = buffer;

	// set dialog information by filling DLGTEMPLATEEX structure
	_r_util_templatewriteshort (&ptr, 1); // dlgVer
	_r_util_templatewriteshort (&ptr, USHRT_MAX); // signature
	_r_util_templatewriteulong (&ptr, 0); // helpID
	_r_util_templatewriteulong (&ptr, WS_EX_APPWINDOW | WS_EX_CONTROLPARENT); // exStyle
	_r_util_templatewriteulong (&ptr, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME); // style
	_r_util_templatewriteshort (&ptr, controls); // cdit
	_r_util_templatewriteshort (&ptr, 0); // x
	_r_util_templatewriteshort (&ptr, 0); // y
	_r_util_templatewriteshort (&ptr, width); // cx
	_r_util_templatewriteshort (&ptr, height); // cy

	// set dialog additional data
	_r_util_templatewritestring (&ptr, L""); // menu
	_r_util_templatewritestring (&ptr, L""); // windowClass
	_r_util_templatewritestring (&ptr, L""); // title

	// set dialog font
	_r_util_templatewriteshort (&ptr, 8); // pointsize
	_r_util_templatewriteshort (&ptr, FW_NORMAL); // weight
	_r_util_templatewriteshort (&ptr, FALSE); // bItalic
	_r_util_templatewritestring (&ptr, L"MS Shell Dlg"); // font

	// insert dialog controls
#if defined(APP_HAVE_SETTINGS_TABS)
	_r_util_templatewritecontrol (&ptr, IDC_NAV, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TCS_HOTTRACK | TCS_TOOLTIPS, 8, 6, width - 16, height - 34, WC_TABCONTROL);
	_r_util_templatewritecontrol (&ptr, IDC_RESET, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, 8, (height - 22), 50, 14, WC_BUTTON);
	_r_util_templatewritecontrol (&ptr, IDC_SAVE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, (width - 114), (height - 22), 50, 14, WC_BUTTON);
#else
	_r_util_templatewritecontrol (&ptr, IDC_NAV, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_TRACKSELECT | TVS_INFOTIP | TVS_NOHSCROLL, 8, 6, 88, height - 14, WC_TREEVIEW);
	_r_util_templatewritecontrol (&ptr, IDC_RESET, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, 88 + 14, (height - 22), 50, 14, WC_BUTTON);

#endif // APP_HAVE_SETTINGS_TABS

	_r_util_templatewritecontrol (&ptr, IDC_CLOSE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, (width - 58), (height - 22), 50, 14, WC_BUTTON);

	DialogBoxIndirect (NULL, buffer, hwnd, &_r_settings_wndproc);

	_r_mem_free (buffer);
}

INT_PTR CALLBACK _r_settings_wndproc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			app_global.settings.hwnd = hwnd;

#ifdef IDI_MAIN
			LONG dpi_value = _r_dc_getwindowdpi (_r_app_gethwnd ());

			_r_wnd_seticon (hwnd,
							_r_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCE (IDI_MAIN), _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value), TRUE),
							_r_loadicon (_r_sys_getimagebase (), MAKEINTRESOURCE (IDI_MAIN), _r_dc_getsystemmetrics (SM_CXICON, dpi_value), TRUE)
			);
#else
#pragma PR_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			INT dlg_id = _r_config_getinteger (L"SettingsLastPage", 0);

#if defined(APP_HAVE_SETTINGS_TABS)
			INT index = 0;
#endif

			for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
			{
				PR_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

				if (!ptr_page || !ptr_page->dlg_id || !(ptr_page->hwnd = CreateDialog (NULL, MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, app_global.settings.wnd_proc)))
					continue;

				BringWindowToTop (ptr_page->hwnd); // HACK!!!

				SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);

#if !defined(APP_HAVE_SETTINGS_TABS)
				HTREEITEM hitem = _r_treeview_additem (hwnd, IDC_NAV, _r_locale_getstring (ptr_page->locale_id), NULL, I_IMAGENONE, (LPARAM)ptr_page);

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
			// localize window
#ifdef IDS_SETTINGS
			SetWindowText (hwnd, _r_locale_getstring (IDS_SETTINGS));
#else
			SetWindowText (hwnd, L"Settings");
#pragma PR_PRINT_WARNING(IDS_SETTINGS)
#endif // IDS_SETTINGS

			// localize navigation
#if !defined(APP_HAVE_SETTINGS_TABS)
			LONG dpi_value = _r_dc_getwindowdpi (hwnd);

			_r_treeview_setstyle (hwnd, IDC_NAV, 0, _r_dc_getdpi (PR_SIZE_ITEMHEIGHT, dpi_value), _r_dc_getdpi (PR_SIZE_TREEINDENT, dpi_value));

			HTREEITEM hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_ROOT, 0);

			while (hitem)
			{
				PR_SETTINGS_PAGE ptr_page = (PR_SETTINGS_PAGE)_r_treeview_getlparam (hwnd, IDC_NAV, hitem);

				if (ptr_page)
				{
					_r_treeview_setitem (hwnd, IDC_NAV, hitem, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd && _r_wnd_isvisible (ptr_page->hwnd))
						PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}

				hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hitem);
			}
#else
			for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
			{
				PR_SETTINGS_PAGE ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

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
			_r_ctrl_settext (hwnd, IDC_RESET, _r_locale_getstring (IDS_RESET));
#else
			_r_ctrl_settext (hwnd, IDC_RESET, L"Reset");
#pragma PR_PRINT_WARNING(IDS_RESET)
#endif // IDS_RESET
#else
#pragma PR_PRINT_WARNING(IDC_RESET)
#endif // IDC_RESET

#if defined(APP_HAVE_SETTINGS_TABS)
#ifdef IDC_SAVE
#ifdef IDS_SAVE
			_r_ctrl_settext (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
#else
			_r_ctrl_settext (hwnd, IDC_SAVE, L"OK");
#pragma PR_PRINT_WARNING(IDS_SAVE)
#endif // IDS_SAVE
#else
#pragma PR_PRINT_WARNING(IDC_SAVE)
#endif // IDC_SAVE
#endif // APP_HAVE_SETTINGS_TABS

#ifdef IDC_CLOSE
#ifdef IDS_CLOSE
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));
#else
			_r_ctrl_settext (hwnd, IDC_CLOSE, L"Close");
#pragma PR_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE
#else
#pragma PR_PRINT_WARNING(IDC_CLOSE)
#endif // IDC_CLOSE

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_DPICHANGED:
		{
			PostMessage (hwnd, RM_LOCALIZE, 0, 0);
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

				if (ptr_page && ptr_page->hwnd)
				{
					DestroyWindow (ptr_page->hwnd);
					ptr_page->hwnd = NULL;
				}
			}

			app_global.settings.hwnd = NULL;

			_r_wnd_top (_r_app_gethwnd (), _r_config_getboolean (L"AlwaysOnTop", FALSE));

#ifdef APP_HAVE_UPDATES
			if (_r_config_getboolean (L"CheckUpdates", TRUE))
				_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;
			INT ctrl_id = (INT)(INT_PTR)lphdr->idFrom;

			if (ctrl_id != IDC_NAV)
				break;

			switch (lphdr->code)
			{
#if !defined(APP_HAVE_SETTINGS_TABS)
				case TVN_SELCHANGING:
				{
					LPNMTREEVIEW lptv = (LPNMTREEVIEW)lparam;

					PR_SETTINGS_PAGE ptr_page_old = (PR_SETTINGS_PAGE)lptv->itemOld.lParam;
					PR_SETTINGS_PAGE ptr_page_new = (PR_SETTINGS_PAGE)lptv->itemNew.lParam;

					if (ptr_page_old && ptr_page_old->hwnd && _r_wnd_isvisible (ptr_page_old->hwnd))
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (!ptr_page_new || !ptr_page_new->hwnd || _r_wnd_isvisible (ptr_page_new->hwnd))
						break;

					_r_config_setinteger (L"SettingsLastPage", ptr_page_new->dlg_id);

					_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page_new->hwnd);

					PostMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

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

					_r_config_setinteger (L"SettingsLastPage", ptr_page->dlg_id);

					_r_tab_adjustchild (hwnd, ctrl_id, ptr_page->hwnd);

					PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

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
					INT retn;
					BOOLEAN is_saved;

					is_saved = TRUE;

					for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
					{
						ptr_page = (PR_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

						if (ptr_page)
						{
							if (ptr_page->hwnd)
							{
								retn = (INT)SendMessage (ptr_page->hwnd, RM_CONFIG_SAVE, (WPARAM)ptr_page->dlg_id, 0);

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
					if (_r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, _r_app_getname (), NULL, L"Are you really sure you want to reset all application settings?") != IDYES)
						break;

					// made backup of existing configuration
					_r_fs_makebackup (_r_app_getconfigpath (), TRUE);

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
					HWND hmain = _r_app_gethwnd ();

					if (hmain)
					{
						SendMessage (hmain, RM_INITIALIZE, 0, 0);
						SendMessage (hmain, RM_LOCALIZE, 0, 0);

						SendMessage (hmain, WM_EXITSIZEMOVE, 0, 0); // reset size and pos

						SendMessage (hmain, RM_CONFIG_RESET, 0, 0);
					}

					// reinitialize settings
					SendMessage (hwnd, RM_LOCALIZE, 0, 0);

					for (SIZE_T i = 0; i < _r_obj_getarraysize (app_global.settings.page_list); i++)
					{
						PR_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_global.settings.page_list, i);

						if (ptr_page)
						{
							if (ptr_page->hwnd)
							{
								SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);
							}
						}
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
BOOLEAN _r_skipuac_isenabled ()
{
#ifndef APP_NO_DEPRECATIONS
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return FALSE;
#endif // APP_NO_DEPRECATIONS

	VARIANT empty = {VT_EMPTY};

	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	ITaskDefinition *task_definition = NULL;
	IActionCollection *action_collection = NULL;
	IAction *action = NULL;
	IExecAction *exec_action = NULL;
	IRegisteredTask *registered_task = NULL;

	BSTR task_root = NULL;
	BSTR task_name = NULL;
	BSTR task_path = NULL;

	BOOLEAN is_enabled = FALSE;

	if (FAILED (CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service)))
		goto CleanupExit;

	if (FAILED (ITaskService_Connect (task_service, empty, empty, empty, empty)))
		goto CleanupExit;

	task_root = SysAllocString (L"\\");

	if (FAILED (ITaskService_GetFolder (task_service, task_root, &task_folder)))
		goto CleanupExit;

	task_name = SysAllocString (APP_SKIPUAC_NAME);

	if (FAILED (ITaskFolder_GetTask (task_folder, task_name, &registered_task)))
		goto CleanupExit;

	if (FAILED (IRegisteredTask_get_Definition (registered_task, &task_definition)))
		goto CleanupExit;

	if (FAILED (ITaskDefinition_get_Actions (task_definition, &action_collection)))
		goto CleanupExit;

	if (FAILED (IActionCollection_get_Item (action_collection, 1, &action)))
		goto CleanupExit;

	if (FAILED (IAction_QueryInterface (action, &IID_IExecAction, &exec_action)))
		goto CleanupExit;

	if (FAILED (IExecAction_get_Path (exec_action, &task_path)))
		goto CleanupExit;

	// check path is to current module
	PathUnquoteSpaces (task_path);

	if (_r_str_compare (task_path, _r_sys_getimagepath ()) == 0)
	{
		is_enabled = TRUE;
	}

CleanupExit:

	if (task_root)
		SysFreeString (task_root);

	if (task_name)
		SysFreeString (task_name);

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

	if (registered_task)
		IRegisteredTask_Release (registered_task);

	if (task_folder)
		ITaskFolder_Release (task_folder);

	if (task_service)
		ITaskService_Release (task_service);

	return is_enabled;
}

HRESULT _r_skipuac_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable)
{
#ifndef APP_NO_DEPRECATIONS
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return E_NOTIMPL;
#endif // APP_NO_DEPRECATIONS

	if (hwnd && is_enable)
	{
		if (!_r_app_issecurelocation () && _r_show_message (hwnd, MB_YESNO | MB_ICONWARNING, _r_app_getname (), L"Security warning!", L"It is not recommended to enable this option\r\nwhen running from outside a secure location (e.g. Program Files).\r\n\r\nAre you sure you want to continue?") != IDYES)
			return E_ABORT;
	}

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
			if (SUCCEEDED (ITaskSettings_put_Compatibility (task_settings, i)))
				break;
		}

		// Set task settings (win7+)
		if (SUCCEEDED (ITaskSettings_QueryInterface (task_settings, &IID_ITaskSettings2, &task_settings2)))
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
		task_directory = SysAllocString (_r_app_getdirectory ());
		task_args = SysAllocString (L"$(Arg0)");

		IExecAction_put_Path (exec_action, task_path);
		IExecAction_put_WorkingDirectory (exec_action, task_directory);
		IExecAction_put_Arguments (exec_action, task_args);

		ITaskFolder_DeleteTask (task_folder, task_name, 0);

		status = ITaskFolder_RegisterTaskDefinition (task_folder, task_name, task_definition, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty, &registered_task);
	}
	else
	{
		status = ITaskFolder_DeleteTask (task_folder, task_name, 0);

		if (status == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
		{
			status = S_OK;
		}
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
	{
		_r_show_errormessage (hwnd, NULL, status, NULL);
	}

	return status;
}

BOOLEAN _r_skipuac_run ()
{
#ifndef APP_NO_DEPRECATIONS
	if (_r_sys_isosversionlower (WINDOWS_VISTA))
		return FALSE;
#endif // APP_NO_DEPRECATIONS

	VARIANT empty = {VT_EMPTY};

	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	ITaskDefinition *task_definition = NULL;
	IRegisteredTask *registered_task = NULL;
	IActionCollection *action_collection = NULL;
	IAction *action = NULL;
	IExecAction *exec_action = NULL;
	IRunningTask *running_task = NULL;

	BSTR task_root = NULL;
	BSTR task_name = NULL;
	BSTR task_path = NULL;
	BSTR task_args = NULL;

	WCHAR arguments[512] = {0};
	VARIANT params = {0};
	LPWSTR *arga;
	ULONG attempts;
	LONG action_count;
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

	status = IRegisteredTask_get_Definition (registered_task, &task_definition);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskDefinition_get_Actions (task_definition, &action_collection);

	if (FAILED (status))
		goto CleanupExit;

	// check actions count is equal to 1
	status = IActionCollection_get_Count (action_collection, &action_count);

	if (FAILED (status))
		goto CleanupExit;

	if (action_count != 1)
	{
		status = E_ABORT;

		goto CleanupExit;
	}

	// check path is to current image path
	if (SUCCEEDED (IActionCollection_get_Item (action_collection, 1, &action)))
	{
		if (SUCCEEDED (IAction_QueryInterface (action, &IID_IExecAction, &exec_action)))
		{
			if (SUCCEEDED (IExecAction_get_Path (exec_action, &task_path)))
			{
				PathUnquoteSpaces (task_path);

				if (_r_str_compare (task_path, _r_sys_getimagepath ()) != 0)
				{
					status = E_ABORT;

					goto CleanupExit;
				}
			}
		}
	}

	// set correct arguments for running task
	arga = CommandLineToArgvW (_r_sys_getimagecommandline (), &numargs);

	if (arga)
	{
		if (numargs > 1)
		{
			for (INT i = 1; i < numargs; i++)
				_r_str_appendformat (arguments, RTL_NUMBER_OF (arguments), L"%s ", arga[i]);

			_r_str_trim (arguments, L" ");
		}

		LocalFree (arga);
	}

	if (!_r_str_isempty (arguments))
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

	// check if run succesfull
	TASK_STATE state;

	attempts = 6;

	do
	{
		IRunningTask_Refresh (running_task);

		if (SUCCEEDED (IRunningTask_get_State (running_task, &state)))
		{
			if (state == TASK_STATE_DISABLED)
			{
				break;
			}
			else if (state == TASK_STATE_RUNNING || state == TASK_STATE_UNKNOWN)
			{
				status = S_OK;
				break;
			}
		}

		_r_sleep (150);
	}
	while (--attempts);

CleanupExit:

	if (task_root)
		SysFreeString (task_root);

	if (task_name)
		SysFreeString (task_name);

	if (task_path)
		SysFreeString (task_path);

	if (task_args)
		SysFreeString (task_args);

	if (running_task)
		IRunningTask_Release (running_task);

	if (exec_action)
		IExecAction_Release (exec_action);

	if (action)
		IAction_Release (action);

	if (action_collection)
		IActionCollection_Release (action_collection);

	if (task_definition)
		ITaskDefinition_Release (task_definition);

	if (registered_task)
		IRegisteredTask_Release (registered_task);

	if (task_folder)
		ITaskFolder_Release (task_folder);

	if (task_service)
		ITaskService_Release (task_service);

	return SUCCEEDED (status);
}
#endif // APP_HAVE_SKIPUAC
