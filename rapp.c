// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#include "rapp.h"

static R_SPINLOCK app_config_lock;
static PR_HASHTABLE app_config_table = NULL;

#if !defined(APP_CONSOLE)
static WNDPROC app_window_proc = NULL;
static HANDLE app_mutex = NULL;

static R_SPINLOCK app_locale_lock;

static PR_STRING app_locale_current = NULL;
static PR_STRING app_locale_default = NULL;
static PR_STRING app_locale_resource = NULL;

static PR_LIST app_locale_names = NULL;
static PR_HASHTABLE app_locale_table = NULL;

static BOOLEAN app_is_needmaximize = FALSE;
#endif // !APP_CONSOLE

#if defined(APP_HAVE_UPDATES)
static PAPP_UPDATE_INFO app_update_info = NULL;
#endif // APP_HAVE_UPDATES

#if defined(APP_HAVE_SETTINGS)
static PR_ARRAY app_settings_pages = NULL;
static DLGPROC app_settings_proc = NULL;
#endif // APP_HAVE_SETTINGS

#if defined(APP_HAVE_TRAY)
static UINT WM_TASKBARCREATED = 0;
#endif // APP_HAVE_TRAY

#if defined(APP_NO_MUTEX)
static LPCWSTR _r_app_getmutexname ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static WCHAR name[128] = {0};

	if (_r_initonce_begin (&init_once))
	{
		_r_str_printf (name, RTL_NUMBER_OF (name), L"%s_%" PR_ULONG_PTR L"_%" PR_ULONG_PTR, _r_app_getnameshort (), _r_str_hash (_r_sys_getimagepathname ()), _r_str_hash (_r_sys_getimagecommandline ()));

		_r_initonce_end (&init_once);
	}

	return name;
}
#else
#define _r_app_getmutexname _r_app_getnameshort
#endif // APP_NO_MUTEX

ULONG CALLBACK _r_app_sehfilter_callback (_In_ PEXCEPTION_POINTERS exception_ptr)
{
	ULONG error_code;
	HINSTANCE hinst;

	if (NT_NTWIN32 (exception_ptr->ExceptionRecord->ExceptionCode))
	{
		error_code = WIN32_FROM_NTSTATUS (exception_ptr->ExceptionRecord->ExceptionCode);
		hinst = NULL;
	}
	else
	{
		error_code = exception_ptr->ExceptionRecord->ExceptionCode;
		hinst = GetModuleHandle (L"ntdll.dll");
	}

	_r_show_errormessage (_r_app_gethwnd (), L"Exception raised :(", error_code, NULL, hinst);

#if defined(APP_NO_DEPRECATIONS)
	RtlExitUserProcess (exception_ptr->ExceptionRecord->ExceptionCode);
#else
	ExitProcess (exception_ptr->ExceptionRecord->ExceptionCode);
#endif // APP_NO_DEPRECATIONS

	return EXCEPTION_EXECUTE_HANDLER;
}

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
			typedef BOOL (WINAPI *SSPM)(ULONG Flags);
			const SSPM _SetSearchPathMode = (SSPM)GetProcAddress (hkernel32, "SetSearchPathMode");

			if (_SetSearchPathMode)
				_SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

			// Check for SetDefaultDllDirectories since it requires KB2533623.
			typedef BOOL (WINAPI *SDDD)(ULONG DirectoryFlags);
			const SDDD _SetDefaultDllDirectories = (SDDD)GetProcAddress (hkernel32, "SetDefaultDllDirectories");

			if (_SetDefaultDllDirectories)
				_SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

			FreeLibrary (hkernel32);
		}
	}
#endif // APP_NO_DEPRECATIONS

	// set unhandled exception filter callback
#if !defined(_DEBUG)
#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
#endif // !APP_NO_DEPRECATIONS
	{
		ULONG error_mode;

		if (NT_SUCCESS (NtQueryInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG), NULL)))
		{
			error_mode &= ~(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

			NtSetInformationProcess (NtCurrentProcess (), ProcessDefaultHardErrorMode, &error_mode, sizeof (ULONG));
		}

		RtlSetUnhandledExceptionFilter (&_r_app_sehfilter_callback);
	}
#endif // !_DEBUG

	// initialize COM library
	{
		HRESULT hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (FAILED (hr))
		{
#if defined(APP_CONSOLE)
			wprintf (L"Error! COM library initialization failed 0x%08" PRIX32 L"!\r\n", hr);
#else
			_r_show_errormessage (NULL, L"COM library initialization failed!", hr, NULL, NULL);
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

	// set locale information
	app_locale_resource = _r_obj_createstring (APP_LANGUAGE_DEFAULT);
	app_locale_default = _r_obj_createstringex (NULL, LOCALE_NAME_MAX_LENGTH * sizeof (WCHAR));

	if (GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SENGLISHLANGUAGENAME, app_locale_default->buffer, LOCALE_NAME_MAX_LENGTH))
	{
		_r_obj_trimstringtonullterminator (app_locale_default);
	}
	else
	{
		_r_obj_clearreference (&app_locale_default);
	}

	// prevent app duplicates
	if (_r_mutex_isexists (_r_app_getmutexname ()))
	{
		EnumWindows (&_r_util_activate_window_callback, (LPARAM)_r_app_getname ());
		return FALSE;
	}

#if defined(APP_HAVE_TRAY)
	if (!WM_TASKBARCREATED)
		WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // APP_HAVE_TRAY

#if defined(APP_NO_GUEST)
	// use "only admin"-mode
	if (!_r_sys_iselevated ())
	{
		if (!_r_app_runasadmin ())
			_r_show_errormessage (NULL, L"Administrative privileges are required!", ERROR_DS_INSUFF_ACCESS_RIGHTS, NULL, NULL);

		return FALSE;
	}

#elif defined(APP_HAVE_SKIPUAC)
	// use "skipuac" feature
	if (!_r_sys_iselevated () && _r_skipuac_run ())
		return FALSE;
#endif // APP_NO_GUEST

	// set running flag
	_r_mutex_create (_r_app_getmutexname (), &app_mutex);

	// set updates path
#if defined(APP_HAVE_UPDATES)
	app_update_info = _r_mem_allocatezero (sizeof (APP_UPDATE_INFO));

	// initialize objects
	app_update_info->components = _r_obj_createarray (sizeof (APP_UPDATE_COMPONENT), NULL);
#endif // APP_HAVE_UPDATES

#if defined(APP_HAVE_SETTINGS)
	app_settings_pages = _r_obj_createarray (sizeof (APP_SETTINGS_PAGE), NULL);
#endif // APP_HAVE_SETTINGS

	// set classic theme
	if (_r_config_getboolean (L"ClassicUI", APP_CLASSICUI))
		SetThemeAppProperties (STAP_ALLOW_NONCLIENT);

	// check for wow64 working and show warning if it is TRUE!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 ())
	{
		WCHAR warning_message[512];
		_r_str_printf (warning_message, RTL_NUMBER_OF (warning_message), L"You are attempting to run the 32-bit version of %s on 64-bit Windows.\r\nPlease run the 64-bit version of %s instead.", _r_app_getname (), _r_app_getname ());

		if (!_r_show_confirmmessage (NULL, L"Warning!", warning_message, L"ConfirmWOW64"))
			return FALSE;
	}
#endif // !_DEBUG && !_WIN64
#endif // !APP_CONSOLE

	return TRUE;
}

#if defined(APP_NO_APPDATA) || defined(APP_CONSOLE)
FORCEINLINE BOOLEAN _r_app_isportable ()
{
	return TRUE;
}
#else
BOOLEAN _r_app_isportable ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static BOOLEAN is_portable = FALSE;

	if (_r_initonce_begin (&init_once))
	{
		LPCWSTR file_names[] = {L"portable", _r_app_getnameshort ()};
		LPCWSTR file_exts[] = {L"dat", L"ini"};
		PR_STRING string;

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (file_names); i++)
		{
			string = _r_format_string (L"%s%c%s.%s", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, file_names[i], file_exts[i]);

			if (string)
			{
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

LPCWSTR _r_app_getdirectory ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING cached_result = NULL;

	if (_r_initonce_begin (&init_once))
	{
		cached_result = _r_path_getbasedirectory (_r_sys_getimagepathname ());

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
			_r_obj_trimstring (buffer, L".\\/\" ");

			_r_obj_movereference (&buffer, _r_str_expandenvironmentstring (buffer->buffer));

			if (buffer)
			{
				if (!_r_obj_isstringempty (buffer))
				{
					if (PathGetDriveNumber (buffer->buffer) == -1)
					{
						_r_obj_movereference (&new_result, _r_format_string (L"%s%c%s", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, buffer->buffer));
					}
					else
					{
						_r_obj_movereference (&new_result, _r_obj_reference (buffer));
					}

					// trying to create file
					if (!_r_obj_isstringempty (new_result) && !_r_fs_exists (new_result->buffer))
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

				_r_obj_dereference (buffer);
			}
		}

		// get configuration path
		if (_r_obj_isstringempty (new_result) || !_r_fs_exists (new_result->buffer))
		{
			if (_r_app_isportable ())
			{
				_r_obj_movereference (&new_result, _r_format_string (L"%s%c%s.ini", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ()));
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
		cached_result = _r_format_string (L"%s%c%s_debug.log", _r_app_getprofiledirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ());

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
		cached_result = _r_format_string (L"%s%c%s.lng", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ());

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

		if (!_r_obj_isstringempty (new_path))
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
		LPCWSTR useragent_config = _r_config_getstring (L"UserAgent", NULL);

		cached_result = !_r_str_isempty (useragent_config) ? _r_obj_createstring (useragent_config) : _r_format_string (L"%s/%s (+%s)", _r_app_getname (), _r_app_getversion (), _r_app_getwebsite_url ());

		_r_initonce_end (&init_once);
	}

	return _r_obj_getstring (cached_result);
}

#if !defined(APP_CONSOLE)
LRESULT CALLBACK _r_app_maindlgproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#if defined(APP_HAVE_TRAY)
	if (msg == WM_TASKBARCREATED)
	{
		if (app_window_proc)
			return CallWindowProc (app_window_proc, hwnd, RM_TASKBARCREATED, 0, 0);

		return FALSE;
	}
#endif // APP_HAVE_TRAY

	switch (msg)
	{
		case RM_LOCALIZE:
		{
			if (app_window_proc)
			{
				LRESULT result = CallWindowProc (app_window_proc, hwnd, msg, wparam, lparam);

				RedrawWindow (hwnd, NULL, NULL, RDW_ERASENOW | RDW_INVALIDATE);
				DrawMenuBar (hwnd); // HACK!!!

				return result;
			}

			break;
		}

		case WM_DESTROY:
		{
			if ((_r_wnd_getstyle (hwnd) & WS_MAXIMIZEBOX))
				_r_config_setbooleanex (L"IsMaximized", !!IsZoomed (hwnd), L"window");

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
					}

					return FALSE;
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
			if (wparam && app_is_needmaximize)
			{
				ShowWindow (hwnd, SW_SHOWMAXIMIZED);
				app_is_needmaximize = FALSE;
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
	if (app_window_proc)
		return CallWindowProc (app_window_proc, hwnd, msg, wparam, lparam);

	return FALSE;
}

BOOLEAN _r_app_createwindow (_In_ INT dlg_id, _In_opt_ INT icon_id, _In_opt_ DLGPROC dlg_proc)
{
#ifdef APP_HAVE_UPDATES
	// configure components
	WCHAR locale_version[64];
	_r_str_fromlong64 (locale_version, RTL_NUMBER_OF (locale_version), _r_locale_getversion ());

	_r_update_addcomponent (_r_app_getname (), _r_app_getnameshort (), _r_app_getversion (), _r_app_getdirectory (), TRUE);
	_r_update_addcomponent (L"Language pack", L"language", locale_version, _r_app_getlocalepath (), FALSE);
#endif // APP_HAVE_UPDATES

	// create main window
	app_hwnd = CreateDialogParam (NULL, MAKEINTRESOURCE (dlg_id), NULL, dlg_proc, 0);

	if (!app_hwnd)
		return FALSE;

	// set window title
	SetWindowText (app_hwnd, _r_app_getname ());

	// set window icon
	if (icon_id)
	{
		_r_wnd_seticon (app_hwnd,
						_r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXSMICON)),
						_r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXICON))
		);
	}

	// set window prop
	SetProp (app_hwnd, _r_app_getname (), (HANDLE)(INT_PTR)42);

	// set window on top
	_r_wnd_top (app_hwnd, _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP));

	// center window position
	_r_wnd_center (app_hwnd, NULL);

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
			WM_TASKBARCREATED,
#endif // APP_HAVE_TRAY
		};

		_r_wnd_changemessagefilter (app_hwnd, messages, RTL_NUMBER_OF (messages), MSGFLT_ALLOW);
	}

	// subclass window
	app_window_proc = (WNDPROC)SetWindowLongPtr (app_hwnd, DWLP_DLGPROC, (LONG_PTR)_r_app_maindlgproc);

	// update autorun settings
#if defined(APP_HAVE_AUTORUN)
	if (_r_autorun_isenabled ())
		_r_autorun_enable (NULL, TRUE);
#endif // APP_HAVE_AUTORUN

	// update uac settings (vista+)
#if defined(APP_HAVE_SKIPUAC)
	if (_r_skipuac_isenabled ())
		_r_skipuac_enable (NULL, TRUE);
#endif // APP_HAVE_SKIPUAC

	// set window visibility (or not?)
	{
#if !defined(APP_STARTMINIMIZED)
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
			is_windowhidden = TRUE;
#endif // APP_HAVE_TRAY

		if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
		{
			is_windowhidden = TRUE;
		}

		if ((_r_wnd_getstyle (app_hwnd) & WS_MAXIMIZEBOX) != 0)
		{
			if (show_code == SW_SHOWMAXIMIZED || _r_config_getbooleanex (L"IsMaximized", FALSE, L"window"))
			{
				if (is_windowhidden)
				{
					app_is_needmaximize = TRUE;
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

		ShowWindow (app_hwnd, show_code);
#endif // !APP_STARTMINIMIZED
	}

	// restore window position
	_r_window_restoreposition (app_hwnd, L"window");

	// common initialization
	SendMessage (app_hwnd, RM_INITIALIZE, 0, 0);
	SendMessage (app_hwnd, RM_LOCALIZE, 0, 0);

#if defined(APP_HAVE_UPDATES)
	if (_r_config_getboolean (L"CheckUpdates", TRUE))
		_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

	return TRUE;
}

_Ret_maybenull_
HICON _r_app_getsharedimage (_In_opt_ HINSTANCE hinst, _In_ INT icon_id, _In_ INT icon_size)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static R_SPINLOCK spin_lock;
	static PR_ARRAY shared_icons = NULL;
	PAPP_SHARED_IMAGE shared_icon;
	HICON hicon;

	if (_r_initonce_begin (&init_once))
	{
		shared_icons = _r_obj_createarrayex (sizeof (APP_SHARED_IMAGE), 16, NULL);
		_r_spinlock_initialize (&spin_lock);

		_r_initonce_end (&init_once);
	}

	_r_spinlock_acquireshared (&spin_lock);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (shared_icons); i++)
	{
		shared_icon = _r_obj_getarrayitem (shared_icons, i);

		if (shared_icon && (shared_icon->hinst == hinst) && (shared_icon->icon_id == icon_id) && (shared_icon->icon_size == icon_size))
		{
			_r_spinlock_releaseshared (&spin_lock);

			return shared_icon->hicon;
		}
	}

	_r_spinlock_releaseshared (&spin_lock);

	hicon = _r_loadicon (hinst, MAKEINTRESOURCE (icon_id), icon_size);

	if (!hicon)
		return NULL;

	shared_icon = _r_mem_allocatezero (sizeof (APP_SHARED_IMAGE));

	shared_icon->hinst = hinst;
	shared_icon->icon_id = icon_id;
	shared_icon->icon_size = icon_size;
	shared_icon->hicon = hicon;

	_r_spinlock_acquireexclusive (&spin_lock);

	_r_obj_addarrayitem (shared_icons, shared_icon);

	_r_spinlock_releaseexclusive (&spin_lock);

	_r_mem_free (shared_icon);

	return hicon;
}

BOOLEAN _r_app_runasadmin ()
{
	BOOLEAN is_mutexdestroyed = _r_mutex_destroy (&app_mutex);

#if defined(APP_HAVE_SKIPUAC)
	if (_r_skipuac_run ())
		return TRUE;
#endif // APP_HAVE_SKIPUAC

	SHELLEXECUTEINFO shex = {0};

	WCHAR directory[256] = {0};
	GetCurrentDirectory (RTL_NUMBER_OF (directory), directory);

	shex.cbSize = sizeof (shex);
	shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	shex.lpVerb = L"runas";
	shex.nShow = SW_SHOW;
	shex.lpFile = _r_sys_getimagepathname ();
	shex.lpParameters = _r_sys_getimagecommandline ();
	shex.lpDirectory = directory;

	if (ShellExecuteEx (&shex))
		return TRUE;

	if (is_mutexdestroyed)
		_r_mutex_create (_r_app_getmutexname (), &app_mutex); // restore mutex on error

	_r_sleep (250); // HACK!!! prevent loop

	return FALSE;
}

VOID _r_app_restart (_In_opt_ HWND hwnd)
{
	WCHAR str_content[256];

#ifdef IDS_RESTART
	_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_RESTART, NULL));
#else
	_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Restart required to apply changed configuration, do you want restart now?");
#pragma R_PRINT_WARNING(IDS_RESTART)
#endif // IDS_RESTART

	if (!hwnd || _r_show_message (hwnd, MB_YESNO | MB_ICONQUESTION, NULL, NULL, str_content) != IDYES)
		return;

	WCHAR directory[256] = {0};
	GetCurrentDirectory (RTL_NUMBER_OF (directory), directory);

	BOOLEAN is_mutexdestroyed = _r_mutex_destroy (&app_mutex);

	if (!_r_sys_createprocessex (_r_sys_getimagepathname (), _r_sys_getimagecommandline (), directory, SW_SHOW, 0))
	{
		if (is_mutexdestroyed)
			_r_mutex_create (_r_app_getmutexname (), &app_mutex); // restore mutex on error

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

/*
	Configuration
*/

VOID _r_config_initialize ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_HASHTABLE config_table;

	if (_r_initonce_begin (&init_once))
	{
		_r_spinlock_initialize (&app_config_lock);

		_r_initonce_end (&init_once);
	}

	config_table = _r_parseini (_r_app_getconfigpath (), NULL);

	_r_spinlock_acquireexclusive (&app_config_lock);

	_r_obj_movereference (&app_config_table, config_table);

	_r_spinlock_releaseexclusive (&app_config_lock);
}

BOOLEAN _r_config_getbooleanex (_In_ LPCWSTR key_name, _In_ BOOLEAN def_value, _In_opt_ LPCWSTR section_name)
{
	return _r_str_toboolean (_r_config_getstringex (key_name, def_value ? L"true" : L"false", section_name));
}

INT _r_config_getintegerex (_In_ LPCWSTR key_name, _In_ INT def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_frominteger (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_tointeger (_r_config_getstringex (key_name, value_text, section_name));
}

UINT _r_config_getuintegerex (_In_ LPCWSTR key_name, UINT def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromuinteger (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_touinteger (_r_config_getstringex (key_name, value_text, section_name));
}

LONG _r_config_getlongex (_In_ LPCWSTR key_name, LONG def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_tolong (_r_config_getstringex (key_name, value_text, section_name));
}

LONG64 _r_config_getlong64ex (_In_ LPCWSTR key_name, LONG64 def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_tolong64 (_r_config_getstringex (key_name, value_text, section_name));
}

ULONG _r_config_getulongex (_In_ LPCWSTR key_name, ULONG def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromulong (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_toulong (_r_config_getstringex (key_name, value_text, section_name));
}

ULONG64 _r_config_getulong64ex (_In_ LPCWSTR key_name, _In_ ULONG64 def_value, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[64];
	_r_str_fromulong64 (value_text, RTL_NUMBER_OF (value_text), def_value);

	return _r_str_toulong64 (_r_config_getstringex (key_name, value_text, section_name));
}

VOID _r_config_getfont (_In_ LPCWSTR key_name, _In_ HWND hwnd, _Inout_ PLOGFONT logfont, _In_opt_ LPCWSTR section_name)
{
	LPCWSTR font_config = _r_config_getstringex (key_name, NULL, section_name);

	if (!_r_str_isempty (font_config))
	{
		R_STRINGREF remaining_part;

		_r_obj_initializestringref (&remaining_part, (LPWSTR)font_config);

		PR_STRING name_part;
		PR_STRING height_part;
		PR_STRING weight_part;

		name_part = _r_str_splitatchar (&remaining_part, &remaining_part, L';');
		height_part = _r_str_splitatchar (&remaining_part, &remaining_part, L';');
		weight_part = _r_str_splitatchar (&remaining_part, &remaining_part, L';');

		if (name_part)
		{
			if (!_r_obj_isstringempty (name_part))
				_r_str_copy (logfont->lfFaceName, LF_FACESIZE, name_part->buffer); // face name

			_r_obj_dereference (name_part);
		}

		if (height_part)
		{
			if (!_r_obj_isstringempty (height_part))
				logfont->lfHeight = _r_dc_fontsizetoheight (hwnd, _r_str_tointeger (height_part->buffer)); // size

			_r_obj_dereference (height_part);
		}

		if (weight_part)
		{
			if (!_r_obj_isstringempty (weight_part))
				logfont->lfWeight = _r_str_tointeger (weight_part->buffer); // weight

			_r_obj_dereference (weight_part);
		}
	}

	// fill missed font values
	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof (ncm);

#if !defined(APP_NO_DEPRECATIONS)
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
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

BOOLEAN _r_config_getsize (_In_ LPCWSTR key_name, _Out_ PSIZE size, _In_ PSIZE def_value, _In_opt_ LPCWSTR section_name)
{
	R_STRINGREF remaining_part;
	PR_STRING x_part;
	PR_STRING y_part;
	LPCWSTR pair_config;

	// initialize defaults
	size->cx = def_value ? def_value->cx : 0;
	size->cy = def_value ? def_value->cy : 0;

	pair_config = _r_config_getstringex (key_name, NULL, section_name);

	if (_r_str_isempty (pair_config))
		return FALSE;

	_r_obj_initializestringref (&remaining_part, (LPWSTR)pair_config);

	x_part = _r_str_splitatchar (&remaining_part, &remaining_part, L',');
	y_part = _r_str_splitatchar (&remaining_part, &remaining_part, L',');

	if (x_part)
	{
		size->cx = _r_str_tolong (x_part->buffer);

		_r_obj_dereference (x_part);
	}

	if (y_part)
	{
		size->cy = _r_str_tolong (y_part->buffer);
		_r_obj_dereference (y_part);
	}

	return TRUE;
}

PR_STRING _r_config_getstringexpandex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name)
{
	PR_STRING string;
	LPCWSTR config_value;

	config_value = _r_config_getstringex (key_name, def_value, section_name);

	if (config_value)
	{
		string = _r_str_expandenvironmentstring (config_value);

		if (string)
			return string;

		return _r_obj_createstring (config_value);
	}

	return NULL;
}

LPCWSTR _r_config_getstringex (_In_ LPCWSTR key_name, _In_opt_ LPCWSTR def_value, _In_opt_ LPCWSTR section_name)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	WCHAR section_string[128];
	PR_HASHSTORE hashstore;
	SIZE_T hash_code;

	if (_r_initonce_begin (&init_once))
	{
		_r_config_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_config_table)
		return NULL;

	if (section_name)
	{
		_r_str_printf (section_string, RTL_NUMBER_OF (section_string), L"%s\\%s\\%s", _r_app_getnameshort (), section_name, key_name);
	}
	else
	{
		_r_str_printf (section_string, RTL_NUMBER_OF (section_string), L"%s\\%s", _r_app_getnameshort (), key_name);
	}

	hash_code = _r_str_hash (section_string);

	if (!hash_code)
		return NULL;

	_r_spinlock_acquireshared (&app_config_lock);

	hashstore = _r_obj_findhashtable (app_config_table, hash_code);

	_r_spinlock_releaseshared (&app_config_lock);

	if (!hashstore)
	{
		_r_spinlock_acquireexclusive (&app_config_lock);

		hashstore = _r_obj_addhashtableitem (app_config_table, hash_code, NULL);

		_r_spinlock_releaseexclusive (&app_config_lock);
	}

	if (hashstore)
	{
		if (_r_obj_isstringempty (hashstore->value_string))
		{
			if (def_value)
				_r_obj_movereference (&hashstore->value_string, _r_obj_createstring (def_value));
		}

		return _r_obj_getstring (hashstore->value_string);
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

VOID _r_config_setfont (_In_ LPCWSTR key_name, _In_ HWND hwnd, _In_ PLOGFONT logfont, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[128];
	_r_str_printf (value_text, RTL_NUMBER_OF (value_text), L"%s;%" PR_LONG L";%" PR_LONG, logfont->lfFaceName, _r_dc_fontheighttosize (hwnd, logfont->lfHeight), logfont->lfWeight);

	_r_config_setstringex (key_name, value_text, section_name);
}

VOID _r_config_setsize (_In_ LPCWSTR key_name, _In_ PSIZE size, _In_opt_ LPCWSTR section_name)
{
	WCHAR value_text[128];
	_r_str_printf (value_text, RTL_NUMBER_OF (value_text), L"%" PR_LONG L",%" PR_LONG, size->cx, size->cy);

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
	WCHAR section_string_full[128];
	PR_HASHSTORE hashstore;
	SIZE_T hash_code;

	if (_r_initonce_begin (&init_once))
	{
		if (_r_obj_ishashtableempty (app_config_table))
			_r_config_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_config_table)
		return;

	if (section_name)
	{
		_r_str_printf (section_string, RTL_NUMBER_OF (section_string), L"%s\\%s", _r_app_getnameshort (), section_name);
		_r_str_printf (section_string_full, RTL_NUMBER_OF (section_string_full), L"%s\\%s\\%s", _r_app_getnameshort (), section_name, key_name);
	}
	else
	{
		_r_str_copy (section_string, RTL_NUMBER_OF (section_string), _r_app_getnameshort ());
		_r_str_printf (section_string_full, RTL_NUMBER_OF (section_string_full), L"%s\\%s", _r_app_getnameshort (), key_name);
	}

	hash_code = _r_str_hash (section_string_full);

	if (!hash_code)
		return;

	_r_spinlock_acquireshared (&app_config_lock);

	hashstore = _r_obj_findhashtable (app_config_table, hash_code);

	_r_spinlock_releaseshared (&app_config_lock);

	if (!hashstore)
	{
		_r_spinlock_acquireexclusive (&app_config_lock);

		hashstore = _r_obj_addhashtableitem (app_config_table, hash_code, NULL);

		_r_spinlock_releaseexclusive (&app_config_lock);
	}

	if (hashstore)
	{
		if (value)
		{
			_r_obj_movereference (&hashstore->value_string, _r_obj_createstring (value));
		}
		else
		{
			_r_obj_clearreference (&hashstore->value_string);
		}

		// write to configuration file
#if !defined(APP_NO_CONFIG)
		WritePrivateProfileString (section_string, key_name, _r_obj_getstring (hashstore->value_string), _r_app_getconfigpath ());
#endif // APP_NO_CONFIG
	}
}

/*
	Localization
*/

#if !defined(APP_CONSOLE)
VOID _r_locale_initialize ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	PR_HASHTABLE locale_table;
	PR_LIST locale_names;
	LPCWSTR language_config;

	language_config = _r_config_getstring (L"Language", NULL);

	if (_r_initonce_begin (&init_once))
	{
		_r_spinlock_initialize (&app_locale_lock);

		_r_initonce_end (&init_once);
	}

	if (_r_str_isempty (language_config))
	{
		if (app_locale_default)
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstringfromstring (app_locale_default));
		}
		else
		{
			_r_obj_clearreference (&app_locale_current);
		}
	}
	else
	{
		if (_r_str_compare (language_config, APP_LANGUAGE_DEFAULT) == 0)
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstringfromstring (app_locale_resource));
		}
		else
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstring (language_config));
		}
	}

	locale_names = _r_obj_createlist (&_r_obj_dereference);
	locale_table = _r_parseini (_r_app_getlocalepath (), locale_names);

	_r_spinlock_acquireexclusive (&app_locale_lock);

	_r_obj_movereference (&app_locale_table, locale_table);
	_r_obj_movereference (&app_locale_names, locale_names);

	_r_spinlock_releaseexclusive (&app_locale_lock);
}

#if defined(APP_HAVE_SETTINGS)
VOID _r_locale_applyfromcontrol (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	PR_STRING string = _r_ctrl_gettext (hwnd, ctrl_id);

	if (!string)
		return;

	_r_obj_movereference (&app_locale_current, string);

	_r_config_setstring (L"Language", _r_obj_getstring (app_locale_current));

	// refresh main window
	HWND hwindow = _r_app_gethwnd ();

	if (hwindow)
		SendMessage (hwindow, RM_LOCALIZE, 0, 0);

	// refresh settings window
	hwindow = _r_settings_getwindow ();

	if (hwindow)
		PostMessage (hwindow, RM_LOCALIZE, 0, 0);
}
#endif // APP_HAVE_SETTINGS

VOID _r_locale_applyfrommenu (_In_ HMENU hmenu, _In_ UINT selected_id)
{
	MENUITEMINFO mii = {0};
	WCHAR name[LOCALE_NAME_MAX_LENGTH] = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = name;
	mii.cch = RTL_NUMBER_OF (name);

	if (!GetMenuItemInfo (hmenu, selected_id, FALSE, &mii))
		return;

	if (_r_str_compare (name, APP_LANGUAGE_DEFAULT) == 0)
	{
		_r_obj_movereference (&app_locale_current, _r_obj_createstringfromstring (app_locale_resource));
	}
	else
	{
		_r_obj_movereference (&app_locale_current, _r_obj_createstring (name));
	}

	_r_config_setstring (L"Language", _r_obj_getstring (app_locale_current));

	// refresh main window
	HWND hwindow = _r_app_gethwnd ();

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
	SIZE_T count;
	UINT index;
	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		hsubmenu = GetSubMenu (hwnd, ctrl_id);

		// clear menu
		_r_menu_clearitems (hsubmenu);

		AppendMenu (hsubmenu, MF_STRING, (UINT_PTR)menu_id, _r_obj_getstringorempty (app_locale_resource));
		_r_menu_checkitem (hsubmenu, menu_id, menu_id, MF_BYCOMMAND, menu_id);

		_r_menu_enableitem (hwnd, ctrl_id, MF_BYPOSITION, FALSE);
	}
	else
	{
		hsubmenu = NULL; // fix warning!

		_r_combobox_clear (hwnd, ctrl_id);
		_r_combobox_insertitem (hwnd, ctrl_id, 0, _r_obj_getstringorempty (app_locale_resource));
		_r_combobox_setcurrentitem (hwnd, ctrl_id, 0);

		_r_ctrl_enable (hwnd, ctrl_id, FALSE);
	}

	count = _r_locale_getcount ();

	if (count <= 1)
		return;

	index = 1;

	if (is_menu)
	{
		_r_menu_enableitem (hwnd, ctrl_id, MF_BYPOSITION, TRUE);
		AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
	}
	else
	{
		_r_ctrl_enable (hwnd, ctrl_id, TRUE);
	}

	PR_STRING string;
	BOOLEAN is_current;

	_r_spinlock_acquireshared (&app_locale_lock);

	for (SIZE_T i = 0; i < count; i++)
	{
		string = _r_obj_getlistitem (app_locale_names, i);

		if (_r_obj_isstringempty (string))
			continue;

		is_current = !_r_obj_isstringempty (app_locale_current) && (_r_str_compare (app_locale_current->buffer, string->buffer) == 0);

		if (is_menu)
		{
			AppendMenu (hsubmenu, MF_STRING, (UINT_PTR)index + (UINT_PTR)menu_id, string->buffer);

			if (is_current)
				_r_menu_checkitem (hsubmenu, menu_id, menu_id + index, MF_BYCOMMAND, menu_id + index);
		}
		else
		{
			_r_combobox_insertitem (hwnd, ctrl_id, index, string->buffer);

			if (is_current)
				_r_combobox_setcurrentitem (hwnd, ctrl_id, index);
		}

		index += 1;
	}

	_r_spinlock_releaseshared (&app_locale_lock);
}

SIZE_T _r_locale_getcount ()
{
	SIZE_T count;

	_r_spinlock_acquireshared (&app_locale_lock);

	count = _r_obj_getlistsize (app_locale_names);

	_r_spinlock_releaseshared (&app_locale_lock);

	return count;
}

LPCWSTR _r_locale_getstring (_In_ UINT uid)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	WCHAR current_section_string[128] = {0};
	WCHAR resource_section_string[128] = {0};
	PR_HASHSTORE hashstore = NULL;
	PR_STRING value_string = NULL;
	SIZE_T hash_code;
	INT length;

	if (_r_initonce_begin (&init_once))
	{
		_r_locale_initialize ();

		_r_initonce_end (&init_once);
	}

	if (!app_locale_table)
		return NULL;

	if (!_r_obj_isstringempty (app_locale_current))
	{
		_r_str_printf (current_section_string, RTL_NUMBER_OF (current_section_string), L"%s\\%03" PRIu32, app_locale_current->buffer, uid);
	}

	if (!_r_obj_isstringempty (app_locale_resource))
	{
		_r_str_printf (resource_section_string, RTL_NUMBER_OF (resource_section_string), L"%s\\%03" PRIu32, app_locale_resource->buffer, uid);
	}

	if (!_r_obj_isstringempty (app_locale_current))
	{
		_r_spinlock_acquireshared (&app_locale_lock);

		hashstore = _r_obj_findhashtable (app_locale_table, _r_str_hash (current_section_string));

		_r_spinlock_releaseshared (&app_locale_lock);

		if (hashstore)
			value_string = hashstore->value_string;
	}

	if (_r_obj_isstringempty (value_string) && !_r_obj_isstringempty (app_locale_resource))
	{
		hash_code = _r_str_hash (resource_section_string);

		_r_spinlock_acquireshared (&app_locale_lock);

		hashstore = _r_obj_findhashtable (app_locale_table, hash_code);

		_r_spinlock_releaseshared (&app_locale_lock);

		if (!hashstore)
		{
			LPWSTR buffer = NULL;

			length = LoadString (_r_sys_getimagebase (), uid, (LPWSTR)&buffer, 0);

			if (length)
			{
				R_HASHSTORE new_hashstore;

				value_string = _r_obj_createstringex (buffer, length * sizeof (WCHAR));

				_r_obj_initializehashstore (&new_hashstore, value_string, 0);

				_r_spinlock_acquireexclusive (&app_locale_lock);

				_r_obj_addhashtableitem (app_config_table, hash_code, &new_hashstore);

				_r_spinlock_releaseexclusive (&app_locale_lock);
			}
		}
		else
		{
			value_string = hashstore->value_string;
		}
	}

	return _r_obj_getstring (value_string);
}

LONG64 _r_locale_getversion ()
{
	WCHAR timestamp_string[32];

	// HACK!!! Use "Russian" section and default timestamp key (000) for compatibility with old releases...
	LPCWSTR locale_info_section = L"Russian";
	LPCWSTR locale_info_key = L"000";

	if (GetPrivateProfileString (locale_info_section, locale_info_key, NULL, timestamp_string, RTL_NUMBER_OF (timestamp_string), _r_app_getlocalepath ()))
	{
		return _r_str_tolong64 (timestamp_string);
	}

	return 0;
}
#endif // !APP_CONSOLE

#if defined(APP_HAVE_AUTORUN)
BOOLEAN _r_autorun_isenabled ()
{
	if (_r_config_getboolean (L"AutorunIsEnabled", FALSE))
	{
		HKEY hkey;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx (hkey, _r_app_getname (), NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				RegCloseKey (hkey);
				return TRUE;
			}

			RegCloseKey (hkey);
		}
	}

	return FALSE;
}

BOOLEAN _r_autorun_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable)
{
	HKEY hkey;
	LSTATUS status;
	PR_STRING string;

	status = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);

	if (status == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			string = _r_format_string (L"\"%s\" -minimized", _r_sys_getimagepathname ());

			if (string)
			{
				status = RegSetValueEx (hkey, _r_app_getname (), 0, REG_SZ, (PBYTE)string->buffer, (ULONG)(string->length + sizeof (UNICODE_NULL)));

				_r_config_setboolean (L"AutorunIsEnabled", (status == ERROR_SUCCESS));

				_r_obj_dereference (string);
			}
		}
		else
		{
			status = RegDeleteValue (hkey, _r_app_getname ());

			if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND)
			{
				if (status == ERROR_FILE_NOT_FOUND)
					status = ERROR_SUCCESS;

				_r_config_setboolean (L"AutorunIsEnabled", FALSE);
			}
		}

		RegCloseKey (hkey);
	}

	if (hwnd && status != ERROR_SUCCESS)
		_r_show_errormessage (hwnd, NULL, status, NULL, NULL);

	return (status == ERROR_SUCCESS);
}
#endif // APP_HAVE_AUTORUN

#if defined(APP_HAVE_UPDATES)
VOID _r_update_addcomponent (_In_opt_ LPCWSTR full_name, _In_opt_ LPCWSTR short_name, _In_opt_ LPCWSTR version, _In_opt_ LPCWSTR target_path, _In_ BOOLEAN is_installer)
{
	APP_UPDATE_COMPONENT update_component = {0};

	if (full_name)
		update_component.full_name = _r_obj_createstring (full_name);

	if (short_name)
		update_component.short_name = _r_obj_createstring (short_name);

	if (version)
		update_component.version = _r_obj_createstring (version);

	if (target_path)
		update_component.target_path = _r_obj_createstring (target_path);

	update_component.is_installer = is_installer;

	_r_obj_addarrayitem (app_update_info->components, &update_component);
}

VOID _r_update_check (_In_opt_ HWND hparent)
{
	if (app_update_info->is_checking)
		return;

	if (!hparent && (!_r_config_getboolean (L"CheckUpdates", TRUE) || (_r_unixtime_now () - _r_config_getlong64 (L"CheckUpdatesLast", 0)) <= APP_UPDATE_PERIOD))
		return;

	if (!NT_SUCCESS (_r_sys_createthread (&_r_update_checkthread, app_update_info, &app_update_info->hthread)))
		return;

	app_update_info->htaskdlg = NULL;
	app_update_info->hparent = hparent;
	app_update_info->is_checking = TRUE;

	if (hparent)
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
#pragma R_PRINT_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

			_r_update_pagenavigate (NULL, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

			return;
		}
	}

	_r_sys_resumethread (app_update_info->hthread);
}

THREAD_API _r_update_checkthread (PVOID lparam)
{
	PAPP_UPDATE_INFO app_update_info = lparam;

	// check for beta versions flag
#if defined(_DEBUG) || defined(APP_BETA)
	BOOLEAN is_beta = TRUE;
#else
	BOOLEAN is_beta = _r_config_getboolean (L"CheckUpdatesBeta", FALSE);
#endif // _DEBUG || APP_BETA

	HINTERNET hsession = _r_inet_createsession (_r_app_getuseragent ());

	if (hsession)
	{
		R_DOWNLOAD_INFO download_info;
		PR_STRING update_url;

		update_url = _r_format_string (L"%s/update.php?product=%s&is_beta=%" PRIu16 L"&api=3", _r_app_getwebsite_url (), _r_app_getnameshort (), is_beta);

		_r_inet_initializedownload (&download_info, NULL, NULL, NULL);

		if (_r_inet_begindownload (hsession, _r_obj_getstring (update_url), &download_info) != ERROR_SUCCESS)
		{
			if (app_update_info->hparent)
			{
				WCHAR str_content[256];

#ifdef IDS_UPDATE_ERROR
				_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_ERROR));
#else
				_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update server connection error.");
#pragma R_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

#ifndef APP_NO_DEPRECATIONS
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
				{
					_r_update_pagenavigate (app_update_info->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);
				}
#ifndef APP_NO_DEPRECATIONS
				else
				{
					_r_show_message (app_update_info->hparent, MB_OK | MB_ICONWARNING, NULL, NULL, str_content);
				}
#endif // !APP_NO_DEPRECATIONS
			}
		}
		else
		{
			PAPP_UPDATE_COMPONENT update_component;
			PR_HASHSTORE string_value;
			PR_HASHTABLE string_table;

			WCHAR updates_text[512] = {0};
			PR_STRING version_string = NULL;
			PR_STRING new_version_string = NULL;
			PR_STRING new_url_string = NULL;
			PR_STRING temp_path;
			SIZE_T split_pos;
			BOOLEAN is_updateavailable = FALSE;

			string_table = _r_str_unserialize (download_info.string, L';', L'=');

			if (string_table)
			{
				temp_path = _r_str_expandenvironmentstring (L"%temp%");

				for (SIZE_T i = 0; i < _r_obj_getarraysize (app_update_info->components); i++)
				{
					update_component = _r_obj_getarrayitem (app_update_info->components, i);

					if (!update_component)
						continue;

					string_value = _r_obj_findhashtable (string_table, _r_obj_getstringhash (update_component->short_name));

					if (!string_value || _r_obj_isstringempty (string_value->value_string))
						continue;

					split_pos = _r_str_findchar (string_value->value_string->buffer, L'|');

					if (split_pos == SIZE_MAX)
						continue;

					_r_obj_movereference (&new_version_string, _r_str_extract (string_value->value_string, 0, split_pos));
					_r_obj_movereference (&new_url_string, _r_str_extract (string_value->value_string, split_pos + 1, _r_obj_getstringlength (string_value->value_string) - split_pos - 1));

					if (!_r_obj_isstringempty (new_version_string) && !_r_obj_isstringempty (new_url_string))
					{
						if (_r_str_versioncompare (update_component->version->buffer, new_version_string->buffer) == -1)
						{
							is_updateavailable = TRUE;
							update_component->is_haveupdate = TRUE;

							_r_obj_movereference (&update_component->new_version, _r_obj_reference (new_version_string));
							_r_obj_movereference (&update_component->url, _r_obj_reference (new_url_string));

							_r_obj_movereference (&version_string, _r_util_versionformat (update_component->new_version));

							if (!_r_obj_isstringempty (version_string))
								_r_str_appendformat (updates_text, RTL_NUMBER_OF (updates_text), L"%s %s\r\n", _r_obj_getstring (update_component->full_name), version_string->buffer);

							if (update_component->is_installer)
							{
								_r_obj_movereference (&update_component->temp_path, _r_format_string (L"%s\\%s-%s-%s.exe", _r_obj_getstringorempty (temp_path), _r_app_getnameshort (), _r_obj_getstringorempty (update_component->short_name), _r_obj_getstringorempty (update_component->new_version)));

								// do not check components when new version of application available
								break;
							}
							else
							{
								_r_obj_movereference (&update_component->temp_path, _r_format_string (L"%s\\%s-%s-%s.tmp", _r_obj_getstringorempty (temp_path), _r_app_getnameshort (), _r_obj_getstringorempty (update_component->short_name), _r_obj_getstringorempty (update_component->new_version)));
							}
						}
					}
				}

				if (temp_path)
					_r_obj_dereference (temp_path);

				if (new_version_string)
					_r_obj_dereference (new_version_string);

				if (new_url_string)
					_r_obj_dereference (new_url_string);

				if (version_string)
					_r_obj_dereference (version_string);

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
#pragma R_PRINT_WARNING(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

#ifndef APP_NO_DEPRECATIONS
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
				{
					_r_update_pagenavigate (app_update_info->htaskdlg, NULL, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)app_update_info);
				}
#ifndef APP_NO_DEPRECATIONS
				else
				{
					if (_r_show_message (app_update_info->hparent, MB_YESNO | MB_USERICON, NULL, str_main, updates_text) == IDYES)
						_r_shell_opendefault (_r_app_getwebsite_url ());
				}
#endif // !APP_NO_DEPRECATIONS

			}
			else
			{
				if (app_update_info->htaskdlg)
				{
					WCHAR str_content[256];

#ifdef IDS_UPDATE_NO
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_NO));
#else
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"No updates available.");
#pragma R_PRINT_WARNING(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO

					_r_update_pagenavigate (app_update_info->htaskdlg, NULL, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);
				}
			}

			_r_config_setlong64 (L"CheckUpdatesLast", _r_unixtime_now ());

			_r_inet_destroydownload (&download_info);
		}

		_r_obj_dereference (update_url);

		_r_inet_close (hsession);
	}

	app_update_info->is_checking = FALSE;

	return ERROR_SUCCESS;
}

BOOLEAN NTAPI _r_update_downloadcallback (ULONG total_written, ULONG total_length, PVOID pdata)
{
	PAPP_UPDATE_INFO app_update_info = pdata;

	if (app_update_info->htaskdlg)
	{
		LONG percent = _r_calc_percentof (total_written, total_length);

		WCHAR str_content[256];

#ifdef IDS_UPDATE_DOWNLOAD
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s %" PR_LONG L"%%", _r_locale_getstring (IDS_UPDATE_DOWNLOAD), percent);
#else
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"Downloading update... %" PR_LONG L"%%", percent);
#pragma R_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

		SendMessage (app_update_info->htaskdlg, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
		SendMessage (app_update_info->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
	}

	return TRUE;
}

THREAD_API _r_update_downloadthread (PVOID lparam)
{
	PAPP_UPDATE_INFO app_update_info = lparam;

	HINTERNET hsession = _r_inet_createsession (_r_app_getuseragent ());

	BOOLEAN is_downloaded = FALSE;
	BOOLEAN is_downloaded_installer = FALSE;
	BOOLEAN is_updated = FALSE;

	if (hsession)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (app_update_info->components); i++)
		{
			PAPP_UPDATE_COMPONENT update_component = _r_obj_getarrayitem (app_update_info->components, i);

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

						_r_inet_initializedownload (&download_info, hfile, &_r_update_downloadcallback, app_update_info);

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
#pragma R_PRINT_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL
		}
		else
		{
#ifdef IDS_UPDATE_DONE
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_DONE));
#else
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Downloading update finished.");
#pragma R_PRINT_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
		}
	}
	else
	{
#ifdef IDS_UPDATE_ERROR
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_ERROR));
#else
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update server connection error.");
#pragma R_PRINT_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
	}

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !APP_NO_DEPRECATIONS

		if (app_update_info->htaskdlg)
			_r_update_pagenavigate (app_update_info->htaskdlg, (is_downloaded ? NULL : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

#if !defined(APP_NO_DEPRECATIONS)
	}
	else
	{
		if (app_update_info->hparent)
			_r_show_message (app_update_info->hparent, is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONEXCLAMATION), NULL, NULL, str_content);
	}
#endif // !APP_NO_DEPRECATIONS

	// install update
	if (is_updated)
	{
		_r_config_initialize (); // reload config
		_r_locale_initialize (); // reload locale

		HWND hmain = _r_app_gethwnd ();

		if (hmain)
		{
			SendMessage (hmain, RM_CONFIG_UPDATE, 0, 0);
			SendMessage (hmain, RM_INITIALIZE, 0, 0);

			SendMessage (hmain, RM_LOCALIZE, 0, 0);
		}
	}

	return ERROR_SUCCESS;
}

HRESULT CALLBACK _r_update_pagecallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR pdata)
{
	PAPP_UPDATE_INFO app_update_info = (PAPP_UPDATE_INFO)pdata;

	switch (msg)
	{
		case TDN_CREATED:
		{
			app_update_info->htaskdlg = hwnd;

			SendMessage (hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			SendMessage (hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			if (app_update_info->hparent)
			{
				_r_wnd_center (hwnd, app_update_info->hparent);
				_r_wnd_top (hwnd, TRUE);
			}

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
#pragma R_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif
				SAFE_DELETE_HANDLE (app_update_info->hthread);

				if (NT_SUCCESS (_r_sys_createthread (&_r_update_downloadthread, app_update_info, &app_update_info->hthread)))
				{
					_r_update_pagenavigate (hwnd, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				for (SIZE_T i = 0; i < _r_obj_getarraysize (app_update_info->components); i++)
				{
					PAPP_UPDATE_COMPONENT update_component = _r_obj_getarrayitem (app_update_info->components, i);

					if (update_component && update_component->is_installer)
					{
						_r_update_install (update_component->temp_path->buffer);
						break;
					}

				}

				DestroyWindow (_r_app_gethwnd ());

				return S_FALSE;
			}

			break;
		}

		case TDN_DESTROYED:
		{
			SAFE_DELETE_HANDLE (app_update_info->hthread);
			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			if (app_update_info->hthread)
				_r_sys_resumethread (app_update_info->hthread);

			break;
		}
	}

	return S_OK;
}

INT _r_update_pagenavigate (_In_opt_ HWND htaskdlg, _In_opt_ LPCWSTR main_icon, _In_ TASKDIALOG_FLAGS flags, _In_ TASKDIALOG_COMMON_BUTTON_FLAGS buttons, _In_opt_ LPCWSTR main, _In_opt_ LPCWSTR content, _In_opt_ LONG_PTR lpdata)
{
	TASKDIALOGCONFIG tdc = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | flags;
	tdc.hwndParent = _r_app_gethwnd ();
	tdc.hInstance = _r_sys_getimagebase ();
	tdc.dwCommonButtons = buttons;
	tdc.pfCallback = &_r_update_pagecallback;
	tdc.lpCallbackData = lpdata;

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
#pragma R_PRINT_WARNING(IDI_MAIN)
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

VOID _r_update_install (_In_ LPCWSTR install_path)
{
	PR_STRING cmd_path;
	PR_STRING cmd_string;

	cmd_path = _r_str_expandenvironmentstring (L"%systemroot%\\system32\\cmd.exe");
	cmd_string = _r_format_string (L"\"%s\" /c timeout 5 > nul&&start /wait \"\" \"%s\" /S /D=%s&&timeout 5 > nul&&del /q /f \"%s\"&start \"\" \"%s\"",
								   _r_obj_getstring (cmd_path),
								   install_path,
								   _r_app_getdirectory (),
								   install_path,
								   _r_sys_getimagepathname ()
	);

	if (!_r_sys_createprocessex (_r_obj_getstring (cmd_path), _r_obj_getstring (cmd_string), NULL, SW_HIDE, 0))
		_r_show_errormessage (NULL, NULL, GetLastError (), install_path, NULL);

	if (cmd_path)
		_r_obj_dereference (cmd_path);

	if (cmd_string)
		_r_obj_dereference (cmd_string);
}

#endif // APP_HAVE_UPDATES

FORCEINLINE LPCWSTR _r_logleveltostring (_In_ LOG_LEVEL log_level)
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

FORCEINLINE ULONG _r_logleveltrayicon (_In_ LOG_LEVEL log_level)
{
	if (log_level == LOG_LEVEL_INFO)
		return NIIF_INFO;

	if (log_level == LOG_LEVEL_WARNING)
		return NIIF_WARNING;

	if (log_level == LOG_LEVEL_ERROR || log_level == LOG_LEVEL_CRITICAL)
		return NIIF_ERROR;

	return NIIF_NONE;
}

VOID _r_log (_In_ LOG_LEVEL log_level, _In_ UINT tray_id, _In_ LPCWSTR fn, _In_ ULONG code, _In_opt_ LPCWSTR description)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	WCHAR date_string[128];
	PR_STRING error_string;
	LPCWSTR log_path;
	LPCWSTR level_string;
	HANDLE hfile;
	LONG64 current_timestamp;
	static INT log_level_config = 0;

	if (_r_initonce_begin (&init_once))
	{
		log_level_config = _r_config_getinteger (L"ErrorLevel", 0);

		_r_initonce_end (&init_once);
	}

	if (log_level_config >= log_level)
		return;

	log_path = _r_app_getlogpath ();

	if (!log_path)
		return;

	current_timestamp = _r_unixtime_now ();
	_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), current_timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	level_string = _r_logleveltostring (log_level);

	// print log for debuggers
	_r_debug_v (L"[%s], %s(), 0x%08" PRIX32 L", %s\r\n",
				level_string,
				fn,
				code,
				description
	);

	// write log to a file
	hfile = CreateFile (log_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		ULONG written;

		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			BYTE bom[] = {0xFF, 0xFE};

			WriteFile (hfile, bom, sizeof (bom), &written, NULL); // write utf-16 le byte order mask
			WriteFile (hfile, PR_DEBUG_HEADER, (ULONG)(_r_str_length (PR_DEBUG_HEADER) * sizeof (WCHAR)), &written, NULL); // adds csv header
		}
		else
		{
			_r_fs_setpos (hfile, 0, FILE_END);
		}

		error_string = _r_format_string (
			L"\"%s\",\"%s\",\"%s\",\"0x%08" PRIX32 L"\",\"%s\"" L",\"%s\",\"%d.%d build %d\"\r\n",
			level_string,
			date_string,
			fn,
			code,
			description,
			_r_app_getversion (),
			NtCurrentPeb ()->OSMajorVersion,
			NtCurrentPeb ()->OSMinorVersion,
			NtCurrentPeb ()->OSBuildNumber
		);

		if (error_string)
		{
			WriteFile (hfile, error_string->buffer, (ULONG)error_string->length, &written, NULL);

			_r_obj_dereference (error_string);
		}

		CloseHandle (hfile);
	}

	// show tray balloon
#if defined(APP_HAVE_TRAY)
	if (tray_id)
	{
		if (_r_config_getboolean (L"IsErrorNotificationsEnabled", TRUE))
		{
			if ((current_timestamp - _r_config_getlong64 (L"ErrorNotificationsTimestamp", 0)) >= _r_config_getlong64 (L"ErrorNotificationsPeriod", 4)) // check for timeout (sec.)
			{
				_r_tray_popup (_r_app_gethwnd (), tray_id, _r_logleveltrayicon (log_level) | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), _r_app_getname (), L"Something went wrong. Open debug log file in profile directory.");

				_r_config_setlong64 (L"ErrorNotificationsTimestamp", current_timestamp);
			}
		}
	}
#endif // APP_HAVE_TRAY
}

VOID _r_log_v (_In_ LOG_LEVEL log_level, _In_ UINT tray_id, _In_ LPCWSTR fn, _In_ ULONG code, _In_ LPCWSTR format, ...)
{
	WCHAR string[1024];
	va_list arg_ptr;

	if (!format)
		return;

	va_start (arg_ptr, format);
	_r_str_printf_v (string, RTL_NUMBER_OF (string), format, arg_ptr);
	va_end (arg_ptr);

	_r_log (log_level, tray_id, fn, code, string);
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
#pragma R_PRINT_WARNING(IDS_ABOUT)
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
#pragma R_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

#if defined(IDS_DONATE)
		td_buttons[0].pszButtonText = _r_locale_getstring (IDS_DONATE);
#else
		td_buttons[0].pszButtonText = L"Give thanks!";
#pragma R_PRINT_WARNING(IDS_DONATE)
#endif // IDS_DONATE

#if defined(IDS_CLOSE)
		td_buttons[1].pszButtonText = _r_locale_getstring (IDS_CLOSE);
#else
		td_buttons[1].pszButtonText = L"Close";
#pragma R_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

		_r_str_printf (str_content, RTL_NUMBER_OF (str_content),
					   L"Version %s %s, %" PRIi32 L"-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>",
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
					   L"%s\r\n\r\nVersion %s %s, %" PRIi32 L"-bit (Unicode)\r\n%s\r\n\r\n%s | %s\r\n\r\nThis program is free software; you can redistribute it and/\r\nor modify it under the terms of the GNU General Public\r\nLicense 3 as published by the Free Software Foundation.",
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
#pragma R_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

		MessageBoxIndirect (&mbp);
	}
#endif // !APP_NO_DEPRECATIONS

	is_opened = FALSE;
}

VOID _r_show_errormessage (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR main, _In_ ULONG error_code, _In_opt_ LPCWSTR description, _In_opt_ HINSTANCE hmodule)
{
	HLOCAL buffer = NULL;

	if (!hmodule)
		hmodule = GetModuleHandle (L"kernel32.dll"); // FORMAT_MESSAGE_FROM_SYSTEM

	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, hmodule, error_code, 0, (LPWSTR)&buffer, 0, NULL);

	WCHAR str_content[512];
	LPCWSTR str_main;
	LPCWSTR str_footer;

	str_main = main ? main : L"It happens ;(";
	str_footer = L"This information may provide clues as to what went wrong and how to fix it.";

	if (buffer)
		_r_str_trim (buffer, L"\r\n ");

	_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s (0x%08" PRIX32 L")", buffer ? (LPWSTR)buffer : L"n/a", error_code);

	if (description)
		_r_str_appendformat (str_content, RTL_NUMBER_OF (str_content), L"\r\n\r\n\"%s\"", description);

#if !defined(APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !APP_NO_DEPRECATIONS
	{
		TASKDIALOGCONFIG tdc = {0};
		TASKDIALOG_BUTTON td_buttons[2] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_NO_SET_FOREGROUND | TDF_SIZE_TO_CONTENT;
		tdc.hwndParent = hwnd;
		tdc.hInstance = _r_sys_getimagebase ();
		tdc.pszWindowTitle = _r_app_getname ();
		tdc.pszMainInstruction = str_main;
		tdc.pszContent = str_content;
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
#pragma R_PRINT_WARNING(IDS_COPY)
#endif // IDS_COPY

#if defined(IDS_CLOSE)
		td_buttons[1].pszButtonText = _r_locale_getstring (IDS_CLOSE);
#else
		td_buttons[1].pszButtonText = L"Close";
#pragma R_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

		INT command_id;

		if (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL))
		{
			if (command_id == td_buttons[0].nButtonID)
				_r_clipboard_set (NULL, str_content, _r_str_length (str_content));
		}
	}
#if !defined(APP_NO_DEPRECATIONS)
	else
	{
		PR_STRING message_string;
		message_string = _r_format_string (L"%s\r\n\r\n%s\r\n\r\n%s", str_main, str_content, str_footer);

		if (message_string)
		{
			if (MessageBox (hwnd, message_string->buffer, _r_app_getname (), MB_YESNO | MB_DEFBUTTON2) == IDYES)
				_r_clipboard_set (NULL, str_content, _r_str_length (str_content));

			_r_obj_dereference (message_string);
		}
	}
#endif // !APP_NO_DEPRECATIONS

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
#pragma R_PRINT_WARNING(IDS_QUESTION_FLAG_CHK)
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

			command_id = SHMessageBoxCheck (hwnd, text, _r_app_getname (), MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST, IDOK, config_string);

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
			return (MessageBox (hwnd, text, _r_app_getname (), MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST) == IDYES); // fallback!
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
#pragma R_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN
		}
		else if ((flags & MB_ICONMASK) == MB_ICONASTERISK)
		{
			tdc.pszMainIcon = TD_INFORMATION_ICON;
		}
		else if ((flags & MB_ICONMASK) == MB_ICONEXCLAMATION)
		{
			tdc.pszMainIcon = TD_WARNING_ICON;
		}
		else if ((flags & MB_ICONMASK) == MB_ICONQUESTION)
		{
			tdc.pszMainIcon = TD_INFORMATION_ICON;
		}
		else if ((flags & MB_ICONMASK) == MB_ICONHAND)
		{
			tdc.pszMainIcon = TD_ERROR_ICON;
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
	R_RECTANGLE rectangle = {0};
	R_RECTANGLE rectangle_current;
	RECT rect;
	LONG current_scale;
	BOOLEAN is_resizeavailable;

	if (!_r_wnd_getposition (hwnd, &rectangle_current))
		return;

	is_resizeavailable = !!(_r_wnd_getstyle (hwnd) & WS_SIZEBOX);

	_r_config_getsize (L"Position", &rectangle.position, &rectangle_current.position, window_name);

	if (is_resizeavailable)
	{
		_r_config_getsize (L"Size", &rectangle.size, &rectangle_current.size, window_name);

		_r_wnd_rectangletorect (&rect, &rectangle);

		current_scale = _r_dc_getdpivalue (NULL, &rect);

		if (current_scale != USER_DEFAULT_SCREEN_DPI)
		{
			rectangle.width = _r_calc_multipledividesigned (rectangle.width, USER_DEFAULT_SCREEN_DPI, current_scale);
			rectangle.height = _r_calc_multipledividesigned (rectangle.height, USER_DEFAULT_SCREEN_DPI, current_scale);
		}
	}

	_r_wnd_adjustworkingarea (NULL, &rectangle);

	_r_wnd_setposition (hwnd, &rectangle.position, is_resizeavailable ? &rectangle.size : NULL);
}

VOID _r_window_saveposition (_In_ HWND hwnd, _In_ LPCWSTR window_name)
{
	WINDOWPLACEMENT wpl = {0};
	MONITORINFO monitor_info = {0};
	R_RECTANGLE rectangle;
	LONG current_scale;
	BOOLEAN is_resizeavailable;

	wpl.length = sizeof (wpl);
	monitor_info.cbSize = sizeof (monitor_info);

	if (!GetWindowPlacement (hwnd, &wpl))
		return;

	_r_wnd_recttorectangle (&rectangle, &wpl.rcNormalPosition);

	if (GetMonitorInfo (MonitorFromRect (&wpl.rcNormalPosition, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
	{
		rectangle.left += monitor_info.rcWork.left - monitor_info.rcMonitor.left;
		rectangle.top += monitor_info.rcWork.top - monitor_info.rcMonitor.top;
	}

	is_resizeavailable = !!(_r_wnd_getstyle (hwnd) & WS_SIZEBOX);

	current_scale = _r_dc_getdpivalue (hwnd, NULL);

	if (current_scale != USER_DEFAULT_SCREEN_DPI)
	{
		rectangle.width = _r_calc_multipledividesigned (rectangle.width, current_scale, USER_DEFAULT_SCREEN_DPI);
		rectangle.height = _r_calc_multipledividesigned (rectangle.height, current_scale, USER_DEFAULT_SCREEN_DPI);
	}

	_r_config_setsize (L"Position", &rectangle.position, window_name);

	if (is_resizeavailable)
	{
		_r_config_setsize (L"Size", &rectangle.size, window_name);
	}
}
#endif // !APP_CONSOLE

/*
	Settings window
*/

#if defined(APP_HAVE_SETTINGS)
VOID _r_settings_addpage (_In_ INT dlg_id, _In_ UINT locale_id)
{
	APP_SETTINGS_PAGE settings_page = {0};

	settings_page.dlg_id = dlg_id;
	settings_page.locale_id = locale_id;

	_r_obj_addarrayitem (app_settings_pages, &settings_page);
}

VOID _r_settings_adjustchild (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ HWND hchild)
{
#if !defined(APP_HAVE_SETTINGS_TABS)
	RECT rc_tree;
	RECT rc_child;

	if (!GetWindowRect (GetDlgItem (hwnd, ctrl_id), &rc_tree) || !GetClientRect (hchild, &rc_child))
		return;

	MapWindowPoints (NULL, hwnd, (PPOINT)&rc_tree, 2);
	SetWindowPos (hchild, NULL, (rc_tree.left * 2) + _r_calc_rectwidth (&rc_tree), rc_tree.top, _r_calc_rectwidth (&rc_child), _r_calc_rectheight (&rc_child), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

#else
	_r_tab_adjustchild (hwnd, IDC_NAV, hchild);
#endif // !APP_HAVE_SETTINGS_TABS
}

VOID _r_settings_createwindow (_In_ HWND hwnd, _In_opt_ DLGPROC dlg_proc, _In_opt_ INT dlg_id)
{
	assert (!_r_obj_isarrayempty (app_settings_pages));

	if (_r_settings_getwindow ())
	{
		_r_wnd_toggle (_r_settings_getwindow (), TRUE);
		return;
	}

	if (dlg_id)
		_r_config_setinteger (L"SettingsLastPage", dlg_id);

	if (dlg_proc)
		app_settings_proc = dlg_proc;

	static R_INITONCE init_once = PR_INITONCE_INIT;
	static SHORT width = 0;
	static SHORT height = 0;

	// calculate dialog size
	if (_r_initonce_begin (&init_once))
	{
		LPDLGTEMPLATEEX pdlg;

		for (SIZE_T i = 0; i < _r_obj_getarraysize (app_settings_pages); i++)
		{
			PAPP_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_settings_pages, i);

			if (!ptr_page || !ptr_page->dlg_id)
				continue;

			pdlg = _r_res_loadresource (_r_sys_getimagebase (), MAKEINTRESOURCE (ptr_page->dlg_id), RT_DIALOG, NULL);

			if (pdlg && (pdlg->style & WS_CHILD))
			{
				if (width < pdlg->cx)
					width = pdlg->cx;

				if (height < pdlg->cy)
					height = pdlg->cy;
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

	const WORD controls = 3;
	const SIZE_T size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) + ((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;
	PVOID buffer = _r_mem_allocatezero (size);
	PBYTE ptr = buffer;

	// set dialog information by filling DLGTEMPLATEEX structure
	_r_util_templatewriteshort (&ptr, 1); // dlgVer
	_r_util_templatewriteshort (&ptr, USHRT_MAX); // signature
	_r_util_templatewriteulong (&ptr, 0); // helpID
	_r_util_templatewriteulong (&ptr, WS_EX_APPWINDOW | WS_EX_CONTROLPARENT); // exStyle
	_r_util_templatewriteulong (&ptr, WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME); // style
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
#else
	_r_util_templatewritecontrol (&ptr, IDC_NAV, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_TRACKSELECT | TVS_INFOTIP | TVS_NOHSCROLL, 8, 6, 88, height - 14, WC_TREEVIEW);
	_r_util_templatewritecontrol (&ptr, IDC_RESET, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, 88 + 14, (height - 22), 50, 14, WC_BUTTON);

#endif // APP_HAVE_SETTINGS_TABS

	_r_util_templatewritecontrol (&ptr, IDC_CLOSE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, (width - 58), (height - 22), 50, 14, WC_BUTTON);

	DialogBoxIndirect (NULL, buffer, hwnd, &_r_settings_wndproc);

	_r_mem_free (buffer);
}

INT_PTR CALLBACK _r_settings_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static R_LAYOUT_MANAGER layout_manager = {0};

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			app_settings_hwnd = hwnd;

#ifdef IDI_MAIN
			_r_wnd_seticon (hwnd,
							_r_app_getsharedimage (_r_sys_getimagebase (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)),
							_r_app_getsharedimage (_r_sys_getimagebase (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXICON))
			);
#else
#pragma R_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			INT dlg_id = _r_config_getinteger (L"SettingsLastPage", 0);

#if defined(APP_HAVE_SETTINGS_TABS)
			INT index = 0;
#endif

			for (SIZE_T i = 0; i < _r_obj_getarraysize (app_settings_pages); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_settings_pages, i);

				if (!ptr_page || !ptr_page->dlg_id || !(ptr_page->hwnd = CreateDialog (NULL, MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, app_settings_proc)))
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

			// initialize layout manager
			_r_layout_initializemanager (&layout_manager, hwnd);

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case RM_LOCALIZE:
		{
			// localize window
#ifdef IDS_SETTINGS
			SetWindowText (hwnd, _r_locale_getstring (IDS_SETTINGS));
#else
			SetWindowText (hwnd, L"Settings");
#pragma R_PRINT_WARNING(IDS_SETTINGS)
#endif // IDS_SETTINGS

			// localize navigation
#if !defined(APP_HAVE_SETTINGS_TABS)
			_r_treeview_setstyle (hwnd, IDC_NAV, 0, _r_dc_getdpi (hwnd, PR_SIZE_ITEMHEIGHT), _r_dc_getdpi (hwnd, PR_SIZE_TREEINDENT));

			HTREEITEM hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_ROOT, 0);

			while (hitem)
			{
				PAPP_SETTINGS_PAGE ptr_page = (PAPP_SETTINGS_PAGE)_r_treeview_getlparam (hwnd, IDC_NAV, hitem);

				if (ptr_page)
				{
					_r_treeview_setitem (hwnd, IDC_NAV, hitem, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd && IsWindowVisible (ptr_page->hwnd))
						PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}

				hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hitem);
			}
#else
			for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_NAV); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = (PAPP_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, IDC_NAV, i);

				if (ptr_page)
				{
					_r_tab_setitem (hwnd, IDC_NAV, i, _r_locale_getstring (ptr_page->locale_id), I_IMAGENONE, 0);

					if (ptr_page->hwnd && IsWindowVisible (ptr_page->hwnd))
						PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}
			}
#endif // APP_HAVE_SETTINGS_TABS

			BOOLEAN is_classic = _r_app_isclassicui ();

			// localize buttons
#ifdef IDC_RESET
#ifdef IDS_RESET
			_r_ctrl_settext (hwnd, IDC_RESET, _r_locale_getstring (IDS_RESET));
#else
			_r_ctrl_settext (hwnd, IDC_RESET, L"Reset");
#pragma R_PRINT_WARNING(IDS_RESET)
#endif // IDS_RESET
			_r_wnd_addstyle (hwnd, IDC_RESET, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#else
#pragma R_PRINT_WARNING(IDC_RESET)
#endif // IDC_RESET

#ifdef IDC_CLOSE
#ifdef IDS_CLOSE
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));
#else
			_r_ctrl_settext (hwnd, IDC_CLOSE, L"Close");
#pragma R_PRINT_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE
			_r_wnd_addstyle (hwnd, IDC_CLOSE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#else
#pragma R_PRINT_WARNING(IDC_CLOSE)
#endif // IDC_CLOSE

			break;
		}

		case WM_SIZE:
		{
			_r_layout_resize (&layout_manager, wparam);
			break;
		}

		case WM_GETMINMAXINFO:
		{
			_r_layout_resizeminimumsize (&layout_manager, lparam);
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
			for (SIZE_T i = 0; i < _r_obj_getarraysize (app_settings_pages); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_settings_pages, i);

				if (ptr_page && ptr_page->hwnd)
				{
					DestroyWindow (ptr_page->hwnd);
					ptr_page->hwnd = NULL;
				}
			}

			app_settings_hwnd = NULL;

			_r_wnd_top (_r_app_gethwnd (), _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP));

#ifdef APP_HAVE_UPDATES
			if (_r_config_getboolean (L"CheckUpdates", TRUE))
				_r_update_check (NULL);
#endif // APP_HAVE_UPDATES

			_r_layout_destroymanager (&layout_manager);

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

					PAPP_SETTINGS_PAGE ptr_page_old = (PAPP_SETTINGS_PAGE)lptv->itemOld.lParam;
					PAPP_SETTINGS_PAGE ptr_page_new = (PAPP_SETTINGS_PAGE)lptv->itemNew.lParam;

					if (ptr_page_old && ptr_page_old->hwnd && IsWindowVisible (ptr_page_old->hwnd))
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (!ptr_page_new || IsWindowVisible (ptr_page_new->hwnd))
						break;

					_r_config_setinteger (L"SettingsLastPage", ptr_page_new->dlg_id);

					if (ptr_page_new->hwnd)
					{
						_r_settings_adjustchild (hwnd, IDC_NAV, ptr_page_new->hwnd);

						PostMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

						ShowWindow (ptr_page_new->hwnd, SW_SHOWNA);
					}

					break;
				}
#else
				case TCN_SELCHANGING:
				{
					PAPP_SETTINGS_PAGE ptr_page = (PAPP_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, ctrl_id, -1);

					if (!ptr_page)
						break;

					ShowWindow (ptr_page->hwnd, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					PAPP_SETTINGS_PAGE ptr_page = (PAPP_SETTINGS_PAGE)_r_tab_getitemlparam (hwnd, ctrl_id, -1);

					if (!ptr_page || IsWindowVisible (ptr_page->hwnd))
						break;

					_r_config_setinteger (L"SettingsLastPage", ptr_page->dlg_id);

					_r_tab_adjustchild (hwnd, ctrl_id, ptr_page->hwnd);

					PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);

					ShowWindow (ptr_page->hwnd, SW_SHOWNA);

					if (IsWindowVisible (hwnd) && !IsIconic (hwnd)) // HACK!!!
						SetFocus (ptr_page->hwnd);

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
				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}

#ifdef IDC_RESET
				case IDC_RESET:
				{
					WCHAR str_content[256];

#ifdef IDS_QUESTION_RESET
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_QUESTION_RESET));
#else
					_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Are you really sure you want to reset all application settings?");
#pragma R_PRINT_WARNING(IDS_QUESTION_RESET)
#endif // IDS_QUESTION_RESET

					if (!_r_show_confirmmessage (hwnd, NULL, str_content, NULL))
						break;

					// made backup of existing configuration
					LONG64 current_timestamp = _r_unixtime_now ();

					_r_fs_makebackup (_r_app_getconfigpath (), current_timestamp, TRUE);

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

						SendMessage (hmain, RM_CONFIG_RESET, 0, (LPARAM)current_timestamp);
					}

					// reinitialize settings
					SendMessage (hwnd, RM_LOCALIZE, 0, 0);

					for (SIZE_T i = 0; i < _r_obj_getarraysize (app_settings_pages); i++)
					{
						PAPP_SETTINGS_PAGE ptr_page = _r_obj_getarrayitem (app_settings_pages, i);

						if (ptr_page)
						{
							if (ptr_page->hwnd)
								SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);
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
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return FALSE;
#endif // APP_NO_DEPRECATIONS

	// old task compatibility
	if (_r_config_getboolean (L"SkipUacIsEnabled", FALSE))
	{
		_r_config_setboolean (L"IsAdminTaskEnabled", TRUE);
		_r_config_setboolean (L"SkipUacIsEnabled", FALSE);

		return TRUE;
	}

	return _r_config_getboolean (L"IsAdminTaskEnabled", FALSE);
}

HRESULT _r_skipuac_enable (_In_opt_ HWND hwnd, _In_ BOOLEAN is_enable)
{
#ifndef APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return E_NOTIMPL;
#endif // APP_NO_DEPRECATIONS

	HRESULT status;

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

	BSTR root = NULL;
	BSTR name = NULL;
	BSTR name_old = NULL;
	BSTR author = NULL;
	BSTR url = NULL;
	BSTR time_limit = NULL;
	BSTR path = NULL;
	BSTR directory = NULL;
	BSTR args = NULL;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	root = SysAllocString (L"\\");

	status = ITaskService_GetFolder (task_service, root, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	// remove old task
	if (_r_config_getboolean (L"SkipUacIsEnabled", FALSE))
	{
		name_old = SysAllocString (APP_SKIPUAC_NAME_OLD);

		ITaskFolder_DeleteTask (task_folder, name_old, 0);

		_r_config_setboolean (L"SkipUacIsEnabled", FALSE);
	}

	name = SysAllocString (APP_SKIPUAC_NAME);

	if (is_enable)
	{
		status = ITaskService_NewTask (task_service, 0, &task_definition);

		if (FAILED (status))
			goto CleanupExit;

		status = ITaskDefinition_get_RegistrationInfo (task_definition, &registration_info);

		if (FAILED (status))
			goto CleanupExit;

		author = SysAllocString (_r_app_getauthor ());
		url = SysAllocString (_r_app_getwebsite_url ());

		IRegistrationInfo_put_Author (registration_info, author);
		IRegistrationInfo_put_URI (registration_info, url);

		status = ITaskDefinition_get_Settings (task_definition, &task_settings);

		if (FAILED (status))
			goto CleanupExit;

		/*
			Set task compatibility (win7+)

			TASK_COMPATIBILITY_V2_4 - win10
			TASK_COMPATIBILITY_V2_3 - win8.1
			TASK_COMPATIBILITY_V2_2 - win8
			TASK_COMPATIBILITY_V2_1 - win7
			TASK_COMPATIBILITY_V2   - vista
		*/

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

		time_limit = SysAllocString (L"PT0S");

		ITaskSettings_put_AllowDemandStart (task_settings, VARIANT_TRUE);
		ITaskSettings_put_AllowHardTerminate (task_settings, VARIANT_FALSE);
		ITaskSettings_put_ExecutionTimeLimit (task_settings, time_limit);
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

		path = SysAllocString (_r_sys_getimagepathname ());
		directory = SysAllocString (_r_app_getdirectory ());
		args = SysAllocString (L"$(Arg0)");

		IExecAction_put_Path (exec_action, path);
		IExecAction_put_WorkingDirectory (exec_action, directory);
		IExecAction_put_Arguments (exec_action, args);

		// remove task
		ITaskFolder_DeleteTask (task_folder, name, 0);

		status = ITaskFolder_RegisterTaskDefinition (task_folder, name, task_definition, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty, &registered_task);

		if (SUCCEEDED (status))
		{
			_r_config_setboolean (L"IsAdminTaskEnabled", TRUE);
		}
	}
	else
	{
		status = ITaskFolder_DeleteTask (task_folder, name, 0);

		if (status == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
		{
			status = S_OK;
		}

		if (SUCCEEDED (status))
		{
			_r_config_setboolean (L"IsAdminTaskEnabled", FALSE);
		}
	}

CleanupExit:

	if (root)
		SysFreeString (root);

	if (name)
		SysFreeString (name);

	if (name_old)
		SysFreeString (name_old);

	if (author)
		SysFreeString (author);

	if (url)
		SysFreeString (url);

	if (time_limit)
		SysFreeString (time_limit);

	if (path)
		SysFreeString (path);

	if (directory)
		SysFreeString (directory);

	if (args)
		SysFreeString (args);

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
		_r_show_errormessage (hwnd, NULL, status, NULL, NULL);

	return status;
}

BOOLEAN _r_skipuac_run ()
{
#ifndef APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return FALSE;
#endif // APP_NO_DEPRECATIONS

	HRESULT status;

	VARIANT empty = {VT_EMPTY};

	ITaskService *task_service = NULL;
	ITaskFolder *task_folder = NULL;
	ITaskDefinition *task_definition = NULL;
	IRegisteredTask *registered_task = NULL;
	IActionCollection *action_collection = NULL;
	IAction *action = NULL;
	IExecAction *exec_action = NULL;
	IRunningTask* running_task = NULL;

	BSTR root = NULL;
	BSTR name = NULL;
	BSTR path = NULL;
	BSTR args = NULL;

	WCHAR arguments[512] = {0};
	VARIANT params = {0};
	LPWSTR *arga;
	ULONG attempts;
	INT numargs;

	status = CoCreateInstance (&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, &task_service);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskService_Connect (task_service, empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	root = SysAllocString (L"\\");

	status = ITaskService_GetFolder (task_service, root, &task_folder);

	if (FAILED (status))
		goto CleanupExit;

	name = SysAllocString (APP_SKIPUAC_NAME);

	status = ITaskFolder_GetTask (task_folder, name, &registered_task);

	if (FAILED (status))
		goto CleanupExit;

	status = IRegisteredTask_get_Definition (registered_task, &task_definition);

	if (FAILED (status))
		goto CleanupExit;

	status = ITaskDefinition_get_Actions (task_definition, &action_collection);

	if (FAILED (status))
		goto CleanupExit;

	// check path is to current module
	if (SUCCEEDED (IActionCollection_get_Item (action_collection, 1, &action)))
	{
		if (SUCCEEDED (IAction_QueryInterface (action, &IID_IExecAction, &exec_action)))
		{
			if (SUCCEEDED (IExecAction_get_Path (exec_action, &path)))
			{
				PathUnquoteSpaces (path);

				if (_r_str_compare (path, _r_sys_getimagepathname ()) != 0)
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
		args = SysAllocString (arguments);

		params.vt = VT_BSTR;
		params.bstrVal = args;
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
	attempts = 6;
	TASK_STATE state;

	do
	{
		IRunningTask_Refresh (running_task);

		if (SUCCEEDED (IRunningTask_get_State (running_task, &state)))
		{
			if (state == TASK_STATE_DISABLED)
			{
				break;
			}
			else if (state == TASK_STATE_RUNNING)
			{
				status = S_OK;
				break;
			}
		}

		_r_sleep (150);
	}
	while (--attempts);

CleanupExit:

	if (root)
		SysFreeString (root);

	if (name)
		SysFreeString (name);

	if (path)
		SysFreeString (path);

	if (args)
		SysFreeString (args);

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
