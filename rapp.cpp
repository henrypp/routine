// routine++
// Copyright (c) 2012-2020 Henry++

#include "rapp.hpp"

#if defined(_APP_HAVE_TRAY)
UINT WM_TASKBARCREATED;
#endif // _APP_HAVE_TRAY

BOOLEAN _r_app_initialize (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// safe dll loading
	SetDllDirectory (L"");

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

	// set general information
	app_name = _r_obj_createstring (name);
	app_name_short = _r_obj_createstring (short_name);
	app_copyright = _r_obj_createstring (copyright);

#if defined(_DEBUG) || defined(_APP_BETA)
	app_version = _r_format_string (L"%s Pre-release", version);
#else
	app_version = _r_format_string (L"%s Release", version);
#endif // _DEBUG || _APP_BETA

	// set app directory
	app_directory = _r_path_getbasedirectory (_r_sys_getimagepathname ());

	// get app locale information
#if !defined(_APP_CONSOLE)
	app_locale_path = _r_format_string (L"%s\\%s.lng", _r_app_getdirectory (), _r_app_getnameshort ());
	app_locale_resource = _r_obj_createstring (_APP_LANGUAGE_DEFAULT);
	app_locale_default = _r_obj_createstringex (NULL, LOCALE_NAME_MAX_LENGTH * sizeof (WCHAR));

	if (GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SENGLISHLANGUAGENAME, app_locale_default->Buffer, LOCALE_NAME_MAX_LENGTH))
	{
		_r_string_trimtonullterminator (app_locale_default);
	}
	else
	{
		_r_obj_clearreference (&app_locale_default);
	}
#endif // !_APP_CONSOLE

	// prevent app duplicates
#if !defined(_APP_CONSOLE)
#if defined(_APP_NO_MUTEX)
	app_mutex_name = _r_format_string (L"%s_%" TEXT (PR_SIZE_T) L"_%" TEXT (PR_SIZE_T),
									   _r_app_getnameshort (),
									   _r_str_hash (_r_sys_getimagepathname ()),
									   _r_str_hash (_r_sys_getimagecommandline ())
	);
#else
	app_mutex_name = _r_obj_createstring2 (app_name_short);
#endif // _APP_NO_MUTEX

	if (_r_mutex_isexists (app_mutex_name))
	{
		EnumWindows (&_r_util_activate_window_callback, (LPARAM)_r_app_getname ());
		return FALSE;
	}

	// initialize controls
	{
		INITCOMMONCONTROLSEX icex;

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;

		InitCommonControlsEx (&icex);
	}
#endif // !_APP_CONSOLE

	// initialize COM library
	{
		HRESULT hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (hr != S_OK && hr != S_FALSE)
		{
#if defined(_APP_CONSOLE)
			wprintf (L"Error! COM library initialization failed 0x%08" PRIX32 L"!\r\n", hr);
#else
			_r_show_errormessage (NULL, L"COM library initialization failed!", hr, NULL, NULL);
#endif // _APP_CONSOLE

			return FALSE;
		}
	}

#if defined(_APP_HAVE_TRAY)
	WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

	// check for "only admin"-mode
#if !defined(_APP_CONSOLE)
#if defined(_APP_NO_GUEST)
	if (!_r_sys_iselevated ())
	{
		if (!_r_app_runasadmin ())
			_r_show_errormessage (NULL, L"Administrative privileges are required!", ERROR_DS_INSUFF_ACCESS_RIGHTS, NULL, NULL);

		return FALSE;
	}

	// use "skipuac" feature
#elif defined(_APP_HAVE_SKIPUAC)
	if (!_r_sys_iselevated () && _r_skipuac_run ())
		return FALSE;
#endif // _APP_NO_GUEST

	// set running flag
	_r_mutex_create (app_mutex_name, &app_mutex);

	// check for wow64 working and show warning if it is TRUE!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 ())
	{
		WCHAR messageText[512];
		_r_str_printf (messageText, RTL_NUMBER_OF (messageText), L"You are attempting to run the 32-bit version of %s on 64-bit Windows.\r\nPlease run the 64-bit version of %s instead.", _r_app_getname (), _r_app_getname ());

		if (!_r_show_confirmmessage (NULL, L"Warning!", messageText, L"ConfirmWOW64"))
			return FALSE;
	}
#endif // !_DEBUG && !_WIN64
#endif // !_APP_CONSOLE

	// parse command line
	{
		INT numargs;
		LPWSTR *arga = CommandLineToArgvW (_r_sys_getimagecommandline (), &numargs);

		if (arga)
		{
			if (numargs > 1)
			{
				LPWSTR value;
				PR_STRING pathExpanded;

				for (INT i = 1; i < numargs; i++)
				{
					if (_r_str_compare (arga[i], L"/ini") == 0 && (i + 1) <= numargs)
					{
						value = arga[i + 1];

						if (*value == L'/' || *value == L'-')
							continue;

						PathUnquoteSpaces (value);

						pathExpanded = _r_str_expandenvironmentstring (value);

						if (!pathExpanded)
							continue;

						if (PathGetDriveNumber (pathExpanded->Buffer) == INVALID_INT)
						{
							_r_str_trim (pathExpanded, L".\\/ ");
							_r_obj_movereference (&app_config_path, _r_format_string (L"%s%c%s", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, pathExpanded->Buffer));
						}
						else
						{
							_r_obj_movereference (&app_config_path, pathExpanded);
							pathExpanded = NULL;
						}

						if (!_r_str_isempty (app_config_path) && !_r_fs_exists (app_config_path->Buffer))
						{
							HANDLE hfile = CreateFile (app_config_path->Buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

							if (_r_fs_isvalidhandle (hfile))
								CloseHandle (hfile);
						}

						if (pathExpanded)
							_r_obj_dereference (pathExpanded);
					}
				}
			}

			LocalFree (arga);
		}
	}

	// get configuration path
	if (_r_str_isempty (app_config_path) || !_r_fs_exists (_r_config_getpath ()))
		_r_obj_movereference (&app_config_path, _r_format_string (L"%s%c%s.ini", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ()));

	{
		PR_STRING portableFile = _r_format_string (L"%s%cportable.dat", _r_app_getdirectory (), OBJ_NAME_PATH_SEPARATOR);

		if (!_r_fs_exists (_r_config_getpath ()) && !_r_fs_exists (portableFile->Buffer))
		{
			PR_STRING appdataFolder = _r_path_getknownfolder (CSIDL_APPDATA, L"\\" _APP_AUTHOR);

			_r_obj_movereference (&app_profile_directory, _r_format_string (L"%s%c%s", appdataFolder->Buffer, OBJ_NAME_PATH_SEPARATOR, _r_app_getname ()));
			_r_obj_movereference (&app_config_path, _r_format_string (L"%s%c%s.ini", _r_app_getprofiledirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ()));

			_r_obj_dereference (appdataFolder);
		}
		else
		{
			_r_obj_movereference (&app_profile_directory, _r_path_getbasedirectory (_r_config_getpath ()));
		}

		_r_obj_dereference (portableFile);
	}

	// made profile directory available
	if (!_r_fs_exists (_r_app_getprofiledirectory ()))
		_r_fs_mkdir (_r_app_getprofiledirectory ());

	// set debug log path
	app_log_path = _r_format_string (L"%s%c%s_debug.log", _r_app_getprofiledirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ());

	// set updates path
#if defined(_APP_HAVE_UPDATES)
	app_update_path = _r_format_string (L"%s%c%s_update.exe", _r_app_getprofiledirectory (), OBJ_NAME_PATH_SEPARATOR, _r_app_getnameshort ());

	app_update_info = (PAPP_UPDATE_INFO)_r_mem_allocatezero (sizeof (APP_UPDATE_INFO));

	// initialize stl!
	app_update_info->components = new OBJECTS_UPDATE_COMPONENTS_VEC;
#endif // _APP_HAVE_UPDATES

	// initialize config
	_r_config_initialize ();

	// set user-agent value
	{
		LPCWSTR useragentConfig = _r_config_getstring (L"UserAgent", NULL);

		_r_obj_movereference (&app_useragent, _r_str_isempty (useragentConfig) ? _r_format_string (L"%s/%s (+%s)", _r_app_getname (), _r_app_getversion (), _APP_WEBSITE_URL) : _r_obj_createstring (useragentConfig));
	}

	// set classic theme
#if !defined(_APP_CONSOLE)
	if (_r_config_getboolean (L"ClassicUI", _APP_CLASSICUI))
		SetThemeAppProperties (STAP_ALLOW_NONCLIENT);
#endif // !_APP_CONSOLE

	return TRUE;
}

#if !defined(_APP_CONSOLE)
BOOLEAN _r_app_createwindow (INT dlg_id, INT icon_id, DLGPROC dlg_proc)
{
	// show update message (if exist)
#if defined(_APP_HAVE_UPDATES)
	if (_r_fs_exists (_r_update_getpath ()))
	{
		WCHAR str_content[256];

#ifdef IDS_UPDATE_INSTALL
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_INSTALL));
#else
		_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Update available. Do you want to install it now?");
#pragma R_PRINT_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL

		INT command_id = _r_show_message (NULL, MB_YESNOCANCEL | MB_USERICON | MB_TOPMOST, NULL, NULL, str_content);

		if (command_id == IDYES)
		{
			_r_update_install ();

			return FALSE;
		}
		else if (command_id == IDNO)
		{
			_r_fs_remove (_r_update_getpath (), _R_FLAG_REMOVE_FORCE);
		}
		else
		{
			// continue...
		}
	}

	// configure components
	WCHAR localeVersion[64];
	_r_str_fromlong64 (localeVersion, RTL_NUMBER_OF (localeVersion), _r_locale_getversion ());

	_r_update_addcomponent (_r_app_getname (), _r_app_getnameshort (), _r_app_getversion (), _r_app_getdirectory (), TRUE);
	_r_update_addcomponent (L"Language pack", L"language", localeVersion, _r_locale_getpath (), FALSE);
#endif // _APP_HAVE_UPDATES

	// create main window
	app_hwnd = CreateDialog (NULL, MAKEINTRESOURCE (dlg_id), NULL, dlg_proc);

	if (!app_hwnd)
		return FALSE;

	// enable messages bypass uipi (vista+)
#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
#endif // !_APP_NO_DEPRECATIONS
	{
		UINT messages[] = {
			WM_COPYDATA,
			WM_COPYGLOBALDATA,
			WM_DROPFILES,
#if defined(_APP_HAVE_TRAY)
			WM_TASKBARCREATED,
#endif // _APP_HAVE_TRAY
		};

		_r_wnd_changemessagefilter (app_hwnd, messages, RTL_NUMBER_OF (messages), MSGFLT_ALLOW);
	}

	// set window prop
	SetProp (app_hwnd, _r_app_getname (), "sync-2.04; iris fucyo; none of your damn business.");

	// subclass window
	app_window_proc = (WNDPROC)SetWindowLongPtr (app_hwnd, DWLP_DLGPROC, (LONG_PTR)&_r_app_mainwindowproc);

	// update autorun settings
#if defined(_APP_HAVE_AUTORUN)
	if (_r_autorun_isenabled ())
		_r_autorun_enable (NULL, TRUE);
#endif // _APP_HAVE_AUTORUN

	// update uac settings (vista+)
#if defined(_APP_HAVE_SKIPUAC)
	if (_r_skipuac_isenabled ())
		_r_skipuac_enable (NULL, TRUE);
#endif // _APP_HAVE_SKIPUAC

	// set window on top
	_r_wnd_top (app_hwnd, _r_config_getboolean (L"AlwaysOnTop", _APP_ALWAYSONTOP));
	_r_wnd_center (app_hwnd, NULL);

	// set minmax info
	{
		RECT rect_original;

		if (GetWindowRect (app_hwnd, &rect_original))
		{
			app_window_minwidth = _r_calc_rectwidth (LONG, &rect_original);
			app_window_minheight = _r_calc_rectheight (LONG, &rect_original);

#if defined(_APP_HAVE_MINSIZE)
			app_window_minwidth /= 2;
			app_window_minheight /= 2;
#endif // _APP_HAVE_MINSIZE
		}
	}

	_r_window_restoreposition (app_hwnd, L"window");

	// set window icon
	if (icon_id)
	{
		SendMessage (app_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXSMICON)));
		SendMessage (app_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXICON)));
	}

	// set window visibility (or not?)
	{
		BOOLEAN is_windowhidden = FALSE;
		INT show_code = SW_SHOWNORMAL;

		STARTUPINFO startupInfo = {0};
		startupInfo.cb = sizeof (startupInfo);

		GetStartupInfo (&startupInfo);

		if ((startupInfo.dwFlags & STARTF_USESHOWWINDOW) != 0)
			show_code = startupInfo.wShowWindow;

		// show window minimized
#if defined(_APP_HAVE_TRAY)
#if defined(_APP_STARTMINIMIZED)
		is_windowhidden = TRUE;
#else
		// if window have tray - check arguments
		if (wcsstr (_r_sys_getimagecommandline (), L"/minimized") || _r_config_getboolean (L"IsStartMinimized", FALSE))
			is_windowhidden = TRUE;
#endif // _APP_STARTMINIMIZED
#endif // _APP_HAVE_TRAY

		if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
			is_windowhidden = TRUE;

		if ((GetWindowLongPtr (app_hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
		{
			if (show_code == SW_SHOWMAXIMIZED || _r_config_getboolean (L"IsWindowZoomed", FALSE, L"window"))
			{
				if (is_windowhidden)
				{
					is_needmaximize = TRUE;
				}
				else
				{
					show_code = SW_SHOWMAXIMIZED;
				}
			}
		}

		if (is_windowhidden)
			show_code = SW_HIDE;

		ShowWindowAsync (app_hwnd, show_code);
	}

	// common initialization
	SendMessage (app_hwnd, RM_INITIALIZE, 0, 0);
	PostMessage (app_hwnd, RM_LOCALIZE, 0, 0);

#if defined(_APP_HAVE_UPDATES)
	if (_r_config_getboolean (L"CheckUpdates", TRUE))
		_r_update_check (NULL);
#endif // _APP_HAVE_UPDATES

	return TRUE;
}

LRESULT CALLBACK _r_app_mainwindowproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#if defined(_APP_HAVE_TRAY)
	if (msg == WM_TASKBARCREATED)
	{
		PostMessage (hwnd, RM_TASKBARCREATED, 0, 0);
		return FALSE;
	}
#endif // _APP_HAVE_TRAY

	switch (msg)
	{
		case RM_LOCALIZE:
		{
			LRESULT result = FALSE;

			if (app_window_proc)
			{
				result = CallWindowProc (app_window_proc, hwnd, msg, wparam, lparam);
				DrawMenuBar (hwnd); // HACK!!!
			}

			return result;
		}

		case WM_DESTROY:
		{
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
				_r_config_setboolean (L"IsWindowZoomed", !!IsZoomed (hwnd), L"window");

			SendMessage (hwnd, RM_UNINITIALIZE, 0, 0);

			_r_mutex_destroy (&app_mutex);

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
				if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) == 0)
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
#if defined(_APP_HAVE_TRAY)
			else if (wparam == SIZE_MINIMIZED)
			{
				ShowWindowAsync (hwnd, SW_HIDE);
			}
#endif // _APP_HAVE_TRAY

			break;
		}

#if defined(_APP_HAVE_TRAY)
		case WM_SYSCOMMAND:
		{
			if (wparam == SC_CLOSE)
			{
				ShowWindowAsync (hwnd, SW_HIDE);
				return TRUE;
			}

			break;
		}
#endif // _APP_HAVE_TRAY

		case WM_SHOWWINDOW:
		{
			if (wparam && is_needmaximize)
			{
				ShowWindowAsync (hwnd, SW_SHOWMAXIMIZED);
				is_needmaximize = FALSE;
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lparam;

			if (lpmmi)
			{
				lpmmi->ptMinTrackSize.x = app_window_minwidth;
				lpmmi->ptMinTrackSize.y = app_window_minheight;
			}

			break;
		}

		case WM_EXITSIZEMOVE:
		{
			_r_window_saveposition (hwnd, L"window");
			break;
		}

		case WM_DPICHANGED:
		{
			LRESULT result = FALSE;

			// call main callback
			if (app_window_proc)
				result = CallWindowProc (app_window_proc, hwnd, msg, wparam, lparam);

			// change window size and position
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				LPRECT lprcnew = (LPRECT)lparam;

				if (lprcnew)
				{
					SetWindowPos (hwnd, NULL, lprcnew->left, lprcnew->top, _r_calc_rectwidth (INT, lprcnew), _r_calc_rectheight (INT, lprcnew), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

					RECT rc_client;

					if (GetClientRect (hwnd, &rc_client))
					{
						SendMessage (hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM (_r_calc_rectwidth (LONG, &rc_client), _r_calc_rectheight (LONG, &rc_client)));
						SendMessage (hwnd, WM_EXITSIZEMOVE, 0, 0); // reset size and pos
					}
				}
			}

			return result;
		}
	}

	if (app_window_proc)
		return CallWindowProc (app_window_proc, hwnd, msg, wparam, lparam);

	return FALSE;
}
#endif // !_APP_CONSOLE

PR_STRING _r_app_getproxyconfiguration ()
{
	LPCWSTR proxyConfig = _r_config_getstring (L"Proxy", NULL);

	if (!_r_str_isempty (proxyConfig))
	{
		return _r_obj_createstring (proxyConfig);
	}
	else
	{
		PR_STRING string = NULL;
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxy = {0};

		if (WinHttpGetIEProxyConfigForCurrentUser (&proxy))
		{
			if (!_r_str_isempty (proxy.lpszProxy))
				string = _r_obj_createstring (proxy.lpszProxy);

			SAFE_DELETE_GLOBAL (proxy.lpszProxy);
			SAFE_DELETE_GLOBAL (proxy.lpszProxyBypass);
			SAFE_DELETE_GLOBAL (proxy.lpszAutoConfigUrl);

			return string;
		}
	}

	return NULL;
}

#if !defined(_APP_CONSOLE)
HICON _r_app_getsharedimage (HINSTANCE hinst, INT icon_id, INT icon_size)
{
	PAPP_SHARED_IMAGE pimage;
	HICON hicon;

	for (auto it = app_shared_icons.begin (); it != app_shared_icons.end (); ++it)
	{
		pimage = *it;

		if (pimage->hinst == hinst && pimage->icon_id == icon_id && pimage->icon_size == icon_size)
			return pimage->hicon;
	}

	hicon = _r_loadicon (hinst, MAKEINTRESOURCE (icon_id), icon_size);

	if (!hicon)
		return NULL;

	pimage = (PAPP_SHARED_IMAGE)_r_mem_allocatezero (sizeof (APP_SHARED_IMAGE));

	pimage->hinst = hinst;
	pimage->icon_id = icon_id;
	pimage->icon_size = icon_size;
	pimage->hicon = hicon;

	app_shared_icons.push_back (pimage);

	return hicon;
}

BOOLEAN _r_app_runasadmin ()
{
	BOOLEAN is_mutexdestroyed = _r_mutex_destroy (&app_mutex);

#if defined(_APP_HAVE_SKIPUAC)
	if (_r_skipuac_run ())
		return TRUE;
#endif // _APP_HAVE_SKIPUAC

	SHELLEXECUTEINFO shex = {0};

	WCHAR directory[256];
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
		_r_mutex_create (app_mutex_name, &app_mutex); // restore mutex on error

	_r_sleep (250); // HACK!!! prevent loop

	return FALSE;
}

VOID _r_app_restart (HWND hwnd)
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

	WCHAR directory[256];
	GetCurrentDirectory (RTL_NUMBER_OF (directory), directory);

	BOOLEAN is_mutexdestroyed = _r_mutex_destroy (&app_mutex);

	if (!_r_sys_createprocessex (_r_sys_getimagepathname (), _r_sys_getimagecommandline (), directory, SW_SHOW, 0))
	{
		if (is_mutexdestroyed)
			_r_mutex_create (app_mutex_name, &app_mutex); // restore mutex on error

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
#endif // !_APP_CONSOLE


/*
	Config
*/

VOID _r_config_initialize ()
{
	_r_util_clear_objects_strings_map2 (&app_config_array);

	_r_parseini (_r_config_getpath (), &app_config_array, NULL);

#if !defined(_APP_CONSOLE)
	_r_locale_initialize (); // initialize locale
#endif // !_APP_CONSOLE
}

BOOLEAN _r_config_getboolean (LPCWSTR key, BOOLEAN def, LPCWSTR name)
{
	return _r_str_toboolean (_r_config_getstring (key, def ? L"true" : L"false", name));
}

INT _r_config_getinteger (LPCWSTR key, INT def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_frominteger (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_tointeger (_r_config_getstring (key, value_text, name));
}

UINT _r_config_getuinteger (LPCWSTR key, UINT def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromuinteger (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_touinteger (_r_config_getstring (key, value_text, name));
}

LONG _r_config_getlong (LPCWSTR key, LONG def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_tolong (_r_config_getstring (key, value_text, name));
}

LONG64 _r_config_getlong64 (LPCWSTR key, LONG64 def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_tolong64 (_r_config_getstring (key, value_text, name));
}

ULONG _r_config_getulong (LPCWSTR key, ULONG def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromulong (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_toulong (_r_config_getstring (key, value_text, name));
}

ULONG64 _r_config_getulong64 (LPCWSTR key, ULONG64 def, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromulong64 (value_text, RTL_NUMBER_OF (value_text), def);

	return _r_str_toulong64 (_r_config_getstring (key, value_text, name));
}

VOID _r_config_getfont (LPCWSTR key, HWND hwnd, PLOGFONT pfont, LPCWSTR name)
{
	LPCWSTR fontConfig = _r_config_getstring (key, NULL, name);

	if (!_r_str_isempty (fontConfig))
	{
		R_STRINGREF remainingPart;

		_r_stringref_initialize (&remainingPart, (LPWSTR)fontConfig);

		PR_STRING namePart;
		PR_STRING heightPart;
		PR_STRING weightPart;

		namePart = _r_str_splitatchar (&remainingPart, &remainingPart, L';');
		heightPart = _r_str_splitatchar (&remainingPart, &remainingPart, L';');
		weightPart = _r_str_splitatchar (&remainingPart, &remainingPart, L';');

		if (namePart)
		{
			if (!_r_str_isempty (namePart))
				_r_str_copy (pfont->lfFaceName, LF_FACESIZE, namePart->Buffer); // face name

			_r_obj_dereference (namePart);
		}

		if (heightPart)
		{
			if (!_r_str_isempty (heightPart))
				pfont->lfHeight = _r_dc_fontsizetoheight (hwnd, _r_str_tointeger (heightPart->Buffer)); // size

			_r_obj_dereference (heightPart);
		}

		if (weightPart)
		{
			if (!_r_str_isempty (weightPart))
				pfont->lfWeight = _r_str_tointeger (weightPart->Buffer); // weight

			_r_obj_dereference (weightPart);
		}
	}

	// fill missed font values
	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof (ncm);

#ifndef _APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		ncm.cbSize -= sizeof (INT);
#endif // _APP_NO_DEPRECATIONS

	if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
	{
		PLOGFONT systemFont = &ncm.lfMessageFont;

		if (_r_str_isempty (pfont->lfFaceName))
			_r_str_copy (pfont->lfFaceName, LF_FACESIZE, systemFont->lfFaceName);

		if (!pfont->lfHeight)
			pfont->lfHeight = systemFont->lfHeight;

		if (!pfont->lfWeight)
			pfont->lfWeight = systemFont->lfWeight;

		pfont->lfCharSet = systemFont->lfCharSet;
		pfont->lfQuality = systemFont->lfQuality;
	}
}

LPCWSTR _r_config_getstring (LPCWSTR key, LPCWSTR def, LPCWSTR name)
{
	OBJECTS_STRINGS_MAP1* valuesMap;
	PR_STRING sectionString;
	PR_STRING keyString;
	PR_STRING valueString;
	LPCWSTR result;

	result = NULL;
	sectionString = name ? _r_format_string (L"%s\\%s", _r_app_getnameshort (), name) : _r_obj_createstring2 (app_name_short);
	keyString = _r_obj_createstring (key);

	// create section if not exist
	if (app_config_array.find (sectionString) == app_config_array.end ())
	{
		app_config_array.emplace (_r_obj_reference (sectionString), NULL);
	}

	valuesMap = &app_config_array.at (sectionString);

	// create new value key if not exist
	if (valuesMap->find (keyString) == valuesMap->end ())
	{
		valuesMap->insert_or_assign (_r_obj_reference (keyString), (PR_STRING)NULL);
	}

	valueString = valuesMap->at (keyString);

	if (_r_str_isempty (valueString))
	{
		if (def)
		{
			_r_obj_movereference (&valueString, _r_obj_createstring (def));

			valuesMap->insert_or_assign (keyString, valueString);
		}
	}

	if (!_r_str_isempty (valueString))
		result = valueString->Buffer;

	_r_obj_dereference (sectionString);
	_r_obj_dereference (keyString);

	return result;
}

VOID _r_config_setboolean (LPCWSTR key, BOOLEAN val, LPCWSTR name)
{
	_r_config_setstring (key, val ? L"true" : L"false", name);
}

VOID _r_config_setinteger (LPCWSTR key, INT val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_frominteger (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setuinteger (LPCWSTR key, UINT val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromuinteger (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setlong (LPCWSTR key, LONG val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromlong (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setlong64 (LPCWSTR key, LONG64 val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromlong64 (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setulong (LPCWSTR key, ULONG val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromulong (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setulong64 (LPCWSTR key, ULONG64 val, LPCWSTR name)
{
	WCHAR value_text[64];
	_r_str_fromulong64 (value_text, RTL_NUMBER_OF (value_text), val);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setfont (LPCWSTR key, HWND hwnd, PLOGFONT pfont, LPCWSTR name)
{
	WCHAR value_text[128];
	_r_str_printf (value_text, RTL_NUMBER_OF (value_text), L"%s;%" TEXT (PRIi32) L";%" TEXT (PR_LONG), pfont->lfFaceName, _r_dc_fontheighttosize (hwnd, pfont->lfHeight), pfont->lfWeight);

	_r_config_setstring (key, value_text, name);
}

VOID _r_config_setstring (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	OBJECTS_STRINGS_MAP1* valuesMap;
	PR_STRING sectionString;
	PR_STRING keyString;
	PR_STRING valueString;

	sectionString = name ? _r_format_string (L"%s\\%s", _r_app_getnameshort (), name) : _r_obj_createstring2 (app_name_short);
	keyString = _r_obj_createstring (key);
	valueString = _r_obj_createstring (val);

	if (app_config_array.find (sectionString) == app_config_array.end ())
	{
		app_config_array.emplace (_r_obj_reference (sectionString), NULL);
	}

	valuesMap = &app_config_array.at (sectionString);

	// update current hash value
	if (valuesMap->find (keyString) == valuesMap->end ())
	{
		valuesMap->insert_or_assign (_r_obj_reference (keyString), (PR_STRING)NULL);
	}

	_r_obj_movereference (&valuesMap->at (keyString), _r_obj_reference (valueString));

	// write to configuration file
	WritePrivateProfileString (sectionString->Buffer, keyString->Buffer, valueString->Buffer, _r_config_getpath ());

	_r_obj_dereference (sectionString);
	_r_obj_dereference (keyString);
	_r_obj_dereference (valueString);
}

/*
	Locale
*/

#if !defined(_APP_CONSOLE)
VOID _r_locale_initialize ()
{
	LPCWSTR languageConfig = _r_config_getstring (L"Language", NULL);

	if (_r_str_isempty (languageConfig))
	{
		_r_obj_movereference (&app_locale_current, _r_obj_createstring2 (app_locale_default));
	}
	else
	{
		if (_r_str_compare (languageConfig, _APP_LANGUAGE_DEFAULT) == 0)
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstring2 (app_locale_resource));
		}
		else
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstring (languageConfig));
		}
	}

	// clear
	_r_util_clear_objects_strings_map2 (&app_locale_array);
	_r_util_clear_objects_strings_vector (&app_locale_names);

	app_locale_timetamp = 0;

	_r_parseini (_r_locale_getpath (), &app_locale_array, &app_locale_names);

	if (!app_locale_array.empty ())
	{
		PR_STRING timestampKey = _r_obj_createstring (L"000");
		PR_STRING string;
		time_t timestamp;

		for (auto it = app_locale_array.begin (); it != app_locale_array.end (); ++it)
		{
			if (it->second.find (timestampKey) != it->second.end ())
			{
				string = it->second.at (timestampKey);

				if (!_r_str_isempty (string))
				{
					timestamp = _r_str_tolong64 (string->Buffer);

					if (app_locale_timetamp < timestamp)
						app_locale_timetamp = timestamp;
				}
			}
		}

		_r_obj_dereference (timestampKey);
	}
}

#if defined(_APP_HAVE_SETTINGS)
VOID _r_locale_applyfromcontrol (HWND hwnd, INT ctrl_id)
{
	PR_STRING string = _r_ctrl_gettext (hwnd, ctrl_id);

	_r_obj_movereference (&app_locale_current, string);

	_r_config_setstring (L"Language", _r_obj_getstring (app_locale_current));

	// refresh settings window
	HWND hsettings = _r_settings_getwindow ();

	if (hsettings)
		PostMessage (hsettings, RM_LOCALIZE, 0, 0);

	// refresh main window
	HWND hmain = _r_app_gethwnd ();

	if (hmain)
		PostMessage (hmain, RM_LOCALIZE, 0, 0);
}
#endif // _APP_HAVE_SETTINGS

VOID _r_locale_applyfrommenu (HMENU hmenu, UINT selected_id)
{
	WCHAR name[LOCALE_NAME_MAX_LENGTH] = {0};

	MENUITEMINFO mii = {0};

	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = name;
	mii.cch = RTL_NUMBER_OF (name);

	if (GetMenuItemInfo (hmenu, selected_id, FALSE, &mii))
	{
		if (_r_str_compare (name, _APP_LANGUAGE_DEFAULT) == 0)
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstring2 (app_locale_resource));
		}
		else
		{
			_r_obj_movereference (&app_locale_current, _r_obj_createstring (name));
		}
	}

	_r_config_setstring (L"Language", _r_obj_getstring (app_locale_current));

#if defined(_APP_HAVE_SETTINGS)
	// refresh settings window
	HWND hsettings = _r_settings_getwindow ();

	if (hsettings)
		PostMessage (hsettings, RM_LOCALIZE, 0, 0);
#endif // _APP_HAVE_SETTINGS

	// refresh main window
	HWND hmain = _r_app_gethwnd ();

	if (hmain)
		PostMessage (hmain, RM_LOCALIZE, 0, 0);
}

VOID _r_locale_enum (HWND hwnd, INT ctrl_id, UINT menu_id)
{
	HMENU hmenu;
	HMENU hsubmenu;
	BOOLEAN is_menu;

	is_menu = (menu_id != 0);

	if (is_menu)
	{
		hmenu = (HMENU)hwnd;
		hsubmenu = GetSubMenu (hmenu, ctrl_id);

		// clear menu
		for (UINT i = 0;; i++)
		{
			if (!DeleteMenu (hsubmenu, menu_id + i, MF_BYCOMMAND))
			{
				DeleteMenu (hsubmenu, 0, MF_BYPOSITION); // delete separator
				break;
			}
		}

		AppendMenu (hsubmenu, MF_STRING, (UINT_PTR)menu_id, _r_obj_getstringorempty (app_locale_resource));
		_r_menu_checkitem (hsubmenu, menu_id, menu_id, MF_BYCOMMAND, menu_id);

		_r_menu_enableitem (hmenu, ctrl_id, MF_BYPOSITION, FALSE);
	}
	else
	{
		hmenu = hsubmenu = NULL; // fix warning!

		SendDlgItemMessage (hwnd, ctrl_id, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, 0, (LPARAM)_r_obj_getstringorempty (app_locale_resource));
		SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, 0, 0);

		_r_ctrl_enable (hwnd, ctrl_id, FALSE);

	}

	if (app_locale_array.size () > 1)
	{
		UINT index = 1;

		if (is_menu)
		{
			_r_menu_enableitem (hmenu, ctrl_id, MF_BYPOSITION, TRUE);
			AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
		}
		else
		{
			_r_ctrl_enable (hwnd, ctrl_id, TRUE);
		}

		for (auto it = app_locale_names.begin (); it != app_locale_names.end (); ++it)
		{
			if (_r_str_isempty (*it))
				continue;

			LPCWSTR name = (*it)->Buffer;
			BOOLEAN is_current = !_r_str_isempty (app_locale_current) && (_r_str_compare (app_locale_current->Buffer, name) == 0);

			if (is_menu)
			{
				AppendMenu (hsubmenu, MF_STRING, (UINT_PTR)index + (UINT_PTR)menu_id, name);

				if (is_current)
					_r_menu_checkitem (hsubmenu, menu_id, menu_id + index, MF_BYCOMMAND, menu_id + index);
			}
			else
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, (WPARAM)index, (LPARAM)name);

				if (is_current)
					SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, (WPARAM)index, 0);
			}

			index += 1;
		}
	}
}

LPCWSTR _r_locale_getstring (UINT uid)
{
	OBJECTS_STRINGS_MAP1* valuesMap;
	PR_STRING keyString;
	PR_STRING valueString;
	LPCWSTR result;
	LPCWSTR buffer;
	INT length;

	result = NULL;
	keyString = _r_format_string (L"%03" TEXT (PRIu32), uid);
	valueString = NULL;

	if (!_r_str_isempty (app_locale_current))
	{
		if (app_locale_array.find (app_locale_current) != app_locale_array.end ())
		{
			valuesMap = &app_locale_array.at (app_locale_current);

			if (valuesMap->find (keyString) != valuesMap->end ())
			{
				valueString = valuesMap->at (keyString);

				if (!_r_str_isempty (valueString))
					result = valueString->Buffer;
			}
		}
	}

	if (_r_str_isempty (result) && !_r_str_isempty (app_locale_resource))
	{
		if (app_locale_array.find (app_locale_resource) == app_locale_array.end ())
		{
			app_locale_array.emplace (_r_obj_reference (app_locale_resource), NULL);
		}

		valuesMap = &app_locale_array.at (app_locale_resource);

		if (valuesMap->find (keyString) == valuesMap->end ())
		{
			length = LoadString (_r_sys_getimagebase (), uid, (LPWSTR)&buffer, 0);

			if (length)
			{
				valueString = _r_obj_createstringex (buffer, length * sizeof (WCHAR));

				valuesMap->insert_or_assign (_r_obj_reference (keyString), valueString);
			}
		}
		else
		{
			valueString = valuesMap->at (keyString);
		}

		if (!_r_str_isempty (valueString))
			result = valueString->Buffer;
	}

	_r_obj_dereference (keyString);

	return result;
}
#endif // !_APP_CONSOLE

/*
	Mutexes
*/

#if !defined(_APP_CONSOLE)
BOOLEAN _r_mutex_create (PR_STRING name, PHANDLE pmutex)
{
	_r_mutex_destroy (pmutex);

	if (!_r_str_isempty (name))
		*pmutex = CreateMutex (NULL, FALSE, name->Buffer);

	return _r_fs_isvalidhandle (*pmutex);
}

BOOLEAN _r_mutex_destroy (PHANDLE pmutex)
{
	HANDLE hmutex = *pmutex;

	if (_r_fs_isvalidhandle (hmutex))
	{
		ReleaseMutex (hmutex);
		CloseHandle (hmutex);

		*pmutex = NULL;

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _r_mutex_isexists (PR_STRING name)
{
	if (_r_str_isempty (name))
		return FALSE;

	HANDLE hmutex = OpenMutex (MUTANT_QUERY_STATE, FALSE, name->Buffer);

	if (hmutex)
	{
		CloseHandle (hmutex);

		return TRUE;
	}

	return FALSE;
}
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_AUTORUN)
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

BOOLEAN _r_autorun_enable (HWND hwnd, BOOLEAN is_enable)
{
	HKEY hkey;
	LSTATUS status;
	PR_STRING string;

	status = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);

	if (status == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			string = _r_format_string (L"\"%s\" /minimized", _r_sys_getimagepathname ());

			status = RegSetValueEx (hkey, _r_app_getname (), 0, REG_SZ, (LPBYTE)string->Buffer, (ULONG)(string->Length + sizeof (UNICODE_NULL)));

			_r_obj_dereference (string);

			_r_config_setboolean (L"AutorunIsEnabled", (status == ERROR_SUCCESS));
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
#endif // _APP_HAVE_AUTORUN

#if defined(_APP_HAVE_UPDATES)
VOID _r_update_addcomponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, BOOLEAN is_installer)
{
	PAPP_UPDATE_COMPONENT pcomponent = (PAPP_UPDATE_COMPONENT)_r_mem_allocatezero (sizeof (APP_UPDATE_COMPONENT));

	if (full_name)
		pcomponent->full_name = _r_obj_createstring (full_name);

	if (short_name)
		pcomponent->short_name = _r_obj_createstring (short_name);

	if (version)
		pcomponent->version = _r_obj_createstring (version);

	if (target_path)
		pcomponent->target_path = _r_obj_createstring (target_path);

	pcomponent->is_installer = is_installer;

	app_update_info->components->push_back (pcomponent);
}

VOID _r_update_check (HWND hparent)
{
	if (!hparent && (!_r_config_getboolean (L"CheckUpdates", TRUE) || (_r_unixtime_now () - _r_config_getlong64 (L"CheckUpdatesLast", 0)) <= _APP_UPDATE_PERIOD))
		return;

	if (!NT_SUCCESS (_r_sys_createthread (&_r_update_checkthread, (PVOID)app_update_info, &app_update_info->hthread)))
		return;

	app_update_info->htaskdlg = NULL;
	app_update_info->hparent = hparent;

	if (hparent)
	{
#ifndef _APP_NO_DEPRECATIONS
		if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		{
#endif // _APP_NO_DEPRECATIONS
			WCHAR str_content[256];

#ifdef IDS_UPDATE_INIT
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), _r_locale_getstring (IDS_UPDATE_INIT));
#else
			_r_str_copy (str_content, RTL_NUMBER_OF (str_content), L"Checking for new releases...");
#pragma R_PRINT_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

			_r_update_pagenavigate (NULL, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

			return;

#ifndef _APP_NO_DEPRECATIONS
		}
#endif // _APP_NO_DEPRECATIONS
	}

	_r_sys_resumethread (app_update_info->hthread);
}

THREAD_API _r_update_checkthread (PVOID lparam)
{
	PAPP_UPDATE_INFO app_update_info = (PAPP_UPDATE_INFO)lparam;

	// check for beta versions flag
#if defined(_DEBUG) || defined(_APP_BETA)
	BOOLEAN is_beta = TRUE;
#else
	BOOLEAN is_beta = _r_config_getboolean (L"CheckUpdatesBeta", FALSE);
#endif // _DEBUG || _APP_BETA

	PR_STRING proxyString = _r_app_getproxyconfiguration ();
	HINTERNET hsession = _r_inet_createsession (_r_app_getuseragent (), _r_obj_getstring (proxyString));

	if (hsession)
	{
		PR_STRING updateUrl;
		PR_STRING updateBuffer;

		updateUrl = _r_format_string (_APP_WEBSITE_URL L"/update.php?product=%s&is_beta=%" TEXT (PRIu16) L"&api=3", _r_app_getnameshort (), is_beta);

		if (_r_inet_downloadurl (hsession, _r_obj_getstring (proxyString), updateUrl->Buffer, (LPVOID*)&updateBuffer, FALSE, NULL, 0) != ERROR_SUCCESS)
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

#ifdef _APP_NO_DEPRECATIONS
				_r_update_pagenavigate (app_update_info->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);
#else
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
				{
					_r_update_pagenavigate (app_update_info->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);
				}
				else
				{
					_r_show_message (app_update_info->hparent, MB_OK | MB_ICONWARNING, NULL, NULL, str_content);
				}
#endif // _APP_NO_DEPRECATIONS
			}
		}
		else
		{
			OBJECTS_STRINGS_MAP1 valuesMap;

			_r_str_unserialize (updateBuffer, L';', L'=', &valuesMap);

			WCHAR updates_text[512] = {0};
			PR_STRING componentValues;
			PR_STRING newVersionString;
			PR_STRING newUrlString;
			SIZE_T split_pos;
			BOOLEAN is_updateavailable = FALSE;

			for (auto it = app_update_info->components->begin (); it != app_update_info->components->end (); ++it)
			{
				if (!*it)
					continue;

				PAPP_UPDATE_COMPONENT pcomponent = *it;

				if (!_r_str_isempty (pcomponent->short_name) && valuesMap.find (pcomponent->short_name) != valuesMap.end ())
				{
					componentValues = valuesMap.at (pcomponent->short_name);

					if (_r_str_isempty (componentValues))
						continue;

					split_pos = _r_str_findchar (componentValues->Buffer, _r_obj_getstringlength (componentValues), L'|');

					if (split_pos == INVALID_SIZE_T)
						continue;

					newVersionString = _r_str_extract (componentValues, 0, split_pos);
					newUrlString = _r_str_extract (componentValues, split_pos + 1, _r_obj_getstringlength (componentValues) - split_pos - 1);

					if (!_r_str_isempty (newVersionString) && !_r_str_isempty (newUrlString) && (_r_str_isnumeric (newVersionString->Buffer) ? (_r_str_tolong64 (_r_obj_getstring (pcomponent->version)) < _r_str_tolong64 (newVersionString->Buffer)) : (_r_str_versioncompare (_r_obj_getstring (pcomponent->version), newVersionString->Buffer) == -1)))
					{
						is_updateavailable = TRUE;
						pcomponent->is_haveupdate = TRUE;

						_r_obj_movereference (&pcomponent->new_version, _r_obj_reference (newVersionString));
						_r_obj_movereference (&pcomponent->url, _r_obj_reference (newUrlString));

						PR_STRING versionString = _r_util_versionformat (pcomponent->new_version);

						if (versionString)
						{
							_r_str_appendformat (updates_text, RTL_NUMBER_OF (updates_text), L"%s %s\r\n", pcomponent->full_name->Buffer, versionString->Buffer);

							_r_obj_dereference (versionString);
						}

						if (pcomponent->is_installer)
						{
							_r_obj_movereference (&pcomponent->temp_path, _r_obj_createstring (_r_update_getpath ()));

							// do not check components when new version of application available
							break;
						}
						else
						{
							PR_STRING tempPath = _r_str_expandenvironmentstring (L"%temp%");

							_r_obj_movereference (&pcomponent->temp_path, _r_format_string (L"%s\\%s-%s-%s.tmp", _r_obj_getstringorempty (tempPath), _r_app_getnameshort (), _r_obj_getstringorempty (pcomponent->short_name), _r_obj_getstringorempty (pcomponent->new_version)));

							if (tempPath)
								_r_obj_dereference (tempPath);
						}
					}

					if (newVersionString)
						_r_obj_dereference (newVersionString);

					if (newUrlString)
						_r_obj_dereference (newUrlString);
				}
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

#ifdef _APP_NO_DEPRECATIONS
				_r_update_pagenavigate (app_update_info->htaskdlg, NULL, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)app_update_info);
#else
				if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
				{
					_r_update_pagenavigate (app_update_info->htaskdlg, NULL, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)app_update_info);
				}
				else
				{
					INT command_id = _r_show_message (app_update_info->hparent, MB_YESNO | MB_USERICON, NULL, str_main, updates_text);

					if (command_id == IDYES)
						_r_sys_createthread2 (&_r_update_downloadthread, (PVOID)app_update_info);
				}
#endif // _APP_NO_DEPRECATIONS
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

			_r_util_clear_objects_strings_map1 (&valuesMap);

			_r_obj_dereference (updateBuffer);
		}

		_r_obj_dereference (updateUrl);

		_r_inet_close (hsession);
	}

	if (proxyString)
		_r_obj_dereference (proxyString);

	return ERROR_SUCCESS;
}

BOOLEAN NTAPI _r_update_downloadcallback (ULONG total_written, ULONG total_length, LONG_PTR lpdata)
{
	PAPP_UPDATE_INFO app_update_info = (PAPP_UPDATE_INFO)lpdata;

	if (app_update_info->htaskdlg)
	{
		INT percent = _r_calc_percentof (INT, total_written, total_length);

		WCHAR str_content[256];

#ifdef IDS_UPDATE_DOWNLOAD
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s %" TEXT (PRIi32) L"%%", _r_locale_getstring (IDS_UPDATE_DOWNLOAD), percent);
#else
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"Downloading update... %" TEXT (PRIi32) L"%%", percent);
#pragma R_PRINT_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

		SendMessage (app_update_info->htaskdlg, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
		SendMessage (app_update_info->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
	}

	return TRUE;
}

THREAD_API _r_update_downloadthread (PVOID lparam)
{
	PAPP_UPDATE_INFO app_update_info = (PAPP_UPDATE_INFO)lparam;

	PR_STRING proxyString = _r_app_getproxyconfiguration ();
	HINTERNET hsession = _r_inet_createsession (_r_app_getuseragent (), _r_obj_getstring (proxyString));

	BOOLEAN is_downloaded = FALSE;
	BOOLEAN is_downloaded_installer = FALSE;
	BOOLEAN is_updated = FALSE;

	if (hsession)
	{
		for (auto it = app_update_info->components->begin (); it != app_update_info->components->end (); ++it)
		{
			if (!*it)
				continue;

			PAPP_UPDATE_COMPONENT pcomponent = *it;

			if (pcomponent->is_haveupdate)
			{
				if (!_r_str_isempty (pcomponent->temp_path))
				{
					HANDLE hfile = CreateFile (pcomponent->temp_path->Buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

					if (_r_fs_isvalidhandle (hfile))
					{
						_R_CALLBACK_HTTP_DOWNLOAD downloadCallback = NULL;

#if defined(_APP_NO_DEPRECATIONS)
						downloadCallback = _r_update_downloadcallback;
#else
						if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
							downloadCallback = _r_update_downloadcallback;
#endif // _APP_NO_DEPRECATIONS

						if (_r_inet_downloadurl (hsession, _r_obj_getstring (proxyString), _r_obj_getstring (pcomponent->url), &hfile, TRUE, downloadCallback, (LONG_PTR)app_update_info) == ERROR_SUCCESS)
						{
							CloseHandle (hfile); // required!

							pcomponent->is_haveupdate = FALSE;

							is_downloaded = TRUE;

							if (pcomponent->is_installer)
							{
								is_downloaded_installer = TRUE;
								break;
							}
							else
							{
								LPCWSTR path = _r_obj_getstring (pcomponent->target_path);

								// copy required files
								SetFileAttributes (path, FILE_ATTRIBUTE_NORMAL);

								_r_fs_move (pcomponent->temp_path->Buffer, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
								_r_fs_remove (pcomponent->temp_path->Buffer, _R_FLAG_REMOVE_FORCE);

								// set new version
								_r_obj_movereference (&pcomponent->version, pcomponent->new_version);
								pcomponent->new_version = NULL;

								is_updated = TRUE;
							}
						}
						else
						{
							CloseHandle (hfile);
						}
					}
				}
			}
		}

		_r_inet_close (hsession);
	}

	if (proxyString)
		_r_obj_dereference (proxyString);

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

#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !_APP_NO_DEPRECATIONS

		if (app_update_info->htaskdlg)
			_r_update_pagenavigate (app_update_info->htaskdlg, (is_downloaded ? NULL : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

#if !defined(_APP_NO_DEPRECATIONS)
	}
	else
	{
		if (app_update_info->hparent)
			_r_show_message (app_update_info->hparent, is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONEXCLAMATION), NULL, NULL, str_content);
	}
#endif // !_APP_NO_DEPRECATIONS

	// install update
	if (is_updated)
	{
		_r_config_initialize (); // reload configuration

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

HRESULT CALLBACK _r_update_pagecallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LONG_PTR lpdata)
{
	PAPP_UPDATE_INFO app_update_info = (PAPP_UPDATE_INFO)lpdata;

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

#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

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

				if (NT_SUCCESS ( _r_sys_createthread (&_r_update_downloadthread, (PVOID)app_update_info, &app_update_info->hthread)))
				{
					_r_update_pagenavigate (hwnd, NULL, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, NULL, str_content, (LONG_PTR)app_update_info);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				_r_update_install ();

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
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, NULL);
			SendMessage (hwnd, WM_SETICON, ICON_BIG, NULL);

			if (app_update_info->hthread)
				_r_sys_resumethread (app_update_info->hthread);

			break;
		}
	}

	return S_OK;
}

INT _r_update_pagenavigate (HWND htaskdlg, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata)
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
		SendMessage (htaskdlg, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);

	else
		_r_msg_taskdialog (&tdc, &command_id, NULL, NULL);

	return command_id;
}

VOID _r_update_install ()
{
	PR_STRING commandLinePath;
	PR_STRING commandLine;

	commandLinePath = _r_str_expandenvironmentstring (L"%systemroot%\\system32\\cmd.exe");
	commandLine = _r_format_string (L"\"%s\" /c timeout 3 > nul&&start /wait \"\" \"%s\" /S /D=%s&&timeout 3 > nul&&del /q /f \"%s\"&start \"\" \"%s\"",
									_r_obj_getstring (commandLinePath),
									_r_update_getpath (),
									_r_app_getdirectory (),
									_r_update_getpath (),
									_r_sys_getimagepathname ()
	);

	if (!_r_sys_createprocessex (_r_obj_getstring (commandLinePath), commandLine->Buffer, NULL, SW_HIDE, 0))
		_r_show_errormessage (NULL, NULL, GetLastError (), _r_update_getpath (), NULL);

	if (commandLinePath)
		_r_obj_dereference (commandLinePath);

	_r_obj_dereference (commandLine);
}

#endif // _APP_HAVE_UPDATES

LPCWSTR _r_logleveltostring (APP_LOG_LEVEL level)
{
	if (level == Debug)
		return L"Debug";

	if (level == Information)
		return L"Information";

	if (level == Warning)
		return L"Warning";

	if (level == Error)
		return L"Error";

	if (level == Critical)
		return L"Critical";

	return NULL;
}

ULONG _r_logleveltotrayicon (APP_LOG_LEVEL level)
{
	if (level == Debug || level == Information)
		return NIIF_INFO;

	if (level == Warning)
		return NIIF_WARNING;

	if (level == Error || level == Critical)
		return NIIF_ERROR;

	return NIIF_NONE;
}

VOID _r_logerror (APP_LOG_LEVEL level, UINT tray_id, LPCWSTR fn, ULONG code, LPCWSTR description)
{
	WCHAR dateString[128];
	PR_STRING errorString;
	time_t timestamp;
	HANDLE hfile;

	if (_r_config_getinteger (L"ErrorLevel", 0) >= level)
		return;

	SYSTEMTIME systemTime = {0};
	GetSystemTime (&systemTime);
	timestamp = _r_unixtime_now ();
	_r_format_dateex (dateString, RTL_NUMBER_OF (dateString), timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	errorString = _r_format_string (
		L"\"%s\",\"%s\",\"%s()\",\"0x%08" TEXT (PRIX32) L"\",\"%s\"" L",\"%s\",\"%d.%d build %d\"\r\n",
		_r_logleveltostring (level),
		dateString,
		fn,
		code,
		description,
		_r_app_getversion (),
		NtCurrentPeb ()->OSMajorVersion,
		NtCurrentPeb ()->OSMinorVersion,
		NtCurrentPeb ()->OSBuildNumber
	);

	// print log for debuggers
	_r_dbg_print_v (L"[%s], %s(), 0x%08" TEXT (PRIX32) L", %s\r\n",
					_r_logleveltostring (level),
					fn,
					code,
					description
	);

	// write log to a file
	hfile = CreateFile (_r_app_getlogpath (), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		ULONG written;

		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			BYTE bom[] = {0xFF, 0xFE};

			WriteFile (hfile, bom, sizeof (bom), &written, NULL); // write utf-16 le byte order mask
			WriteFile (hfile, _R_DEBUG_HEADER, (ULONG)(_r_str_length (_R_DEBUG_HEADER) * sizeof (WCHAR)), &written, NULL); // adds csv header
		}
		else
		{
			_r_fs_setpos (hfile, 0, FILE_END);
		}

		WriteFile (hfile, errorString->Buffer, (ULONG)errorString->Length, &written, NULL);

		CloseHandle (hfile);
	}

	// show tray balloon
#if defined(_APP_HAVE_TRAY)
	if (tray_id && _r_config_getboolean (L"IsErrorNotificationsEnabled", TRUE))
	{
		if ((timestamp - _r_config_getlong64 (L"ErrorNotificationsTimestamp", 0)) >= _r_config_getlong64 (L"ErrorNotificationsPeriod", 4)) // check for timeout (sec.)
		{
			_r_tray_popup (_r_app_gethwnd (), tray_id, _r_logleveltotrayicon (level) | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), _r_app_getname (), L"Something went wrong. Open debug log file in profile directory.");

			_r_config_setlong64 (L"ErrorNotificationsTimestamp", timestamp);
		}
	}
#endif // _APP_HAVE_TRAY

	_r_obj_dereference (errorString);
}

VOID _r_logerror_v (APP_LOG_LEVEL level, UINT tray_id, LPCWSTR fn, ULONG code, LPCWSTR format, ...)
{
	va_list argPtr;
	PR_STRING string;

	if (!format)
	{
		_r_logerror (level, tray_id, fn, code, NULL);
		return;
	}

	va_start (argPtr, format);
	string = _r_format_string_v (format, argPtr);
	va_end (argPtr);

	_r_logerror (level, tray_id, fn, code, string->Buffer);

	_r_obj_dereference (string);
}

#if !defined(_APP_CONSOLE)
VOID _r_show_aboutmessage (HWND hwnd)
{
	static BOOLEAN is_opened = FALSE;

	if (is_opened)
		return;

	is_opened = TRUE;

#ifdef _WIN64
#define architecture 64
#else
#define architecture 32
#endif // _WIN64

	LPCWSTR str_title;
	WCHAR str_content[512];

#ifdef IDS_ABOUT
	str_title = _r_locale_getstring (IDS_ABOUT);
#else
	str_title = L"About";
#pragma R_PRINT_WARNING(IDS_ABOUT)
#endif

#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !_APP_NO_DEPRECATIONS

		INT command_id = 0;

		TASKDIALOGCONFIG tdc = {0};
		TASKDIALOG_BUTTON td_buttons[2];

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

		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"Version %s, %" TEXT (PRIi32) L"-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", _r_app_getversion (), architecture, _r_app_getcopyright (), _APP_WEBSITE_URL, _APP_WEBSITE_URL + 8, _APP_GITHUB_URL, _APP_GITHUB_URL + 8);
		tdc.pszFooter = L"This program is free software; you can redistribute it and/or modify it under the terms of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License 3</a> as published by the Free Software Foundation.";

		if (_r_msg_taskdialog (&tdc, &command_id, NULL, NULL))
		{
			if (command_id == td_buttons[0].nButtonID)
				ShellExecute (_r_app_gethwnd (), NULL, _APP_DONATE_NEWURL, NULL, NULL, SW_SHOWNORMAL);
		}
#if !defined(_APP_NO_DEPRECATIONS)
	}
	else
	{
		_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s\r\n\r\nVersion %s, %" TEXT (PRIi32) L"-bit (Unicode)\r\n%s\r\n\r\n%s | %s\r\n\r\nThis program is free software; you can redistribute it and/\r\nor modify it under the terms of the GNU General Public\r\nLicense 3 as published by the Free Software Foundation.", _r_app_getname (), _r_app_getversion (), architecture, _r_app_getcopyright (), _APP_WEBSITE_URL + 8, _APP_GITHUB_URL + 8);

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
#endif // !_APP_NO_DEPRECATIONS

	is_opened = FALSE;
}

VOID _r_show_errormessage (HWND hwnd, LPCWSTR main, ULONG errcode, LPCWSTR description, HINSTANCE hmodule)
{
	HLOCAL buffer = NULL;
	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, hmodule, errcode, 0, (LPWSTR)&buffer, 0, NULL);

	WCHAR str_content[512];

	LPCWSTR str_main;
	LPWSTR str_errortext;
	LPCWSTR str_footer;

	str_main = main ? main : L"It happens ;(";
	str_errortext = (LPWSTR)buffer;
	str_footer = L"This information may provide clues as to what went wrong and how to fix it.";

	_r_str_trim (str_errortext, L"\r\n "); // trim trailing whitespaces

	_r_str_printf (str_content, RTL_NUMBER_OF (str_content), L"%s (0x%08" TEXT (PRIX32) L")", str_errortext ? str_errortext : L"n/a", errcode);

	if (description)
		_r_str_appendformat (str_content, RTL_NUMBER_OF (str_content), L"\r\n\r\n\"%s\"", description);

#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !_APP_NO_DEPRECATIONS

		TASKDIALOGCONFIG tdc = {0};
		TASKDIALOG_BUTTON td_buttons[2];

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

#if !defined(_APP_NO_DEPRECATIONS)
	}
	else
	{
		PR_STRING messageText;
		messageText = _r_format_string (L"%s\r\n\r\n%s\r\n\r\n%s", str_main, str_content, str_footer);

		if (MessageBox (hwnd, messageText->Buffer, _r_app_getname (), MB_YESNO | MB_DEFBUTTON2) == IDYES)
			_r_clipboard_set (NULL, str_content, _r_str_length (str_content));

		_r_obj_dereference (messageText);
	}
#endif // !_APP_NO_DEPRECATIONS

	if (buffer)
		LocalFree (buffer);
}

BOOLEAN _r_show_confirmmessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR configKey)
{
	if (configKey && !_r_config_getboolean (configKey, TRUE))
		return TRUE;

	INT command_id = 0;
	BOOL is_flagchecked = FALSE;

#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif
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

		if (configKey)
		{
			tdc.pszVerificationText = str_flag;

#ifdef IDS_QUESTION_FLAG_CHK
			_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), _r_locale_getstring (IDS_QUESTION_FLAG_CHK));
#else
			_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), L"Do not ask again");
#pragma R_PRINT_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK
		}

		if (main)
			tdc.pszMainInstruction = main;

		if (text)
			tdc.pszContent = text;

		_r_msg_taskdialog (&tdc, &command_id, NULL, &is_flagchecked);

#if !defined(_APP_NO_DEPRECATIONS)
	}
	else
	{
		if (configKey)
		{
			HKEY hkey;
			WCHAR configString[128];

			_r_str_printf (configString, RTL_NUMBER_OF (configString), L"%s\\%s", _r_app_getnameshort (), configKey);

			command_id = SHMessageBoxCheck (hwnd, text, _r_app_getname (), MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST, IDOK, configString);

			// get checkbox value fron registry
			if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\DontShowMeThisDialogAgain", 0, KEY_WRITE | KEY_READ, &hkey) == ERROR_SUCCESS)
			{
				if (command_id == IDOK || command_id == IDYES)
					is_flagchecked = (RegQueryValueEx (hkey, configString, 0, NULL, NULL, NULL) == ERROR_SUCCESS);

				RegDeleteValue (hkey, configString);
				RegCloseKey (hkey);
			}
		}
	}

	if (command_id == INVALID_INT && MessageBox (hwnd, text, _r_app_getname (), MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST) == IDYES) // fallback!
		return TRUE;
#endif // !_APP_NO_DEPRECATIONS

	if (command_id == IDOK || command_id == IDYES)
	{
		if (configKey && is_flagchecked)
			_r_config_setboolean (configKey, FALSE);

		return TRUE;
	}

	return FALSE;
}

INT _r_show_message (HWND hwnd, ULONG flags, LPCWSTR title, LPCWSTR main, LPCWSTR content)
{
#if !defined(_APP_NO_DEPRECATIONS)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
	{
#endif // !_APP_NO_DEPRECATIONS

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

#if !defined(_APP_NO_DEPRECATIONS)
	}
	else
	{
		return MessageBox (hwnd, content, title ? title : _r_app_getname (), flags);
	}
#endif // !_APP_NO_DEPRECATIONS

	return 0;
}

VOID _r_window_restoreposition (HWND hwnd, LPCWSTR window_name)
{
	RECT rect;
	RECT rect_new;
	UINT swp_flags;
	INT left_pos;
	INT top_pos;

	if (!GetWindowRect (hwnd, &rect))
		return;

	left_pos = _r_config_getinteger (L"WindowPosX", rect.left, window_name);
	top_pos = _r_config_getinteger (L"WindowPosY", rect.top, window_name);

	SetRect (&rect_new,
			 left_pos,
			 top_pos,
			 max (_r_config_getinteger (L"WindowPosWidth", _r_calc_rectwidth (INT, &rect), window_name), _r_calc_rectwidth (INT, &rect)) + left_pos,
			 max (_r_config_getinteger (L"WindowPosHeight", _r_calc_rectheight (INT, &rect), window_name), _r_calc_rectheight (INT, &rect)) + top_pos
	);

	_r_wnd_adjustwindowrect (hwnd, &rect_new);

	swp_flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) == 0)
		swp_flags |= SWP_NOSIZE;

	SetWindowPos (hwnd, NULL, rect_new.left, rect_new.top, _r_calc_rectwidth (INT, &rect_new), _r_calc_rectheight (INT, &rect_new), swp_flags);

	// send resize message
	if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
	{
		if (GetClientRect (hwnd, &rect))
			SendMessage (app_hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM (_r_calc_rectwidth (LONG, &rect), _r_calc_rectheight (LONG, &rect)));
	}
}

VOID _r_window_saveposition (HWND hwnd, LPCWSTR window_name)
{
	RECT rect;

	if (!GetWindowRect (hwnd, &rect))
		return;

	_r_config_setlong (L"WindowPosX", rect.left, window_name);
	_r_config_setlong (L"WindowPosY", rect.top, window_name);

	if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
	{
		_r_config_setlong (L"WindowPosWidth", _r_calc_rectwidth (LONG, &rect), window_name);
		_r_config_setlong (L"WindowPosHeight", _r_calc_rectheight (LONG, &rect), window_name);
	}

	InvalidateRect (hwnd, NULL, TRUE); // redraw window content when resizing ends (HACK!!!)
}
#endif // !(_APP_CONSOLE

#if defined(_APP_HAVE_SETTINGS)
VOID _r_settings_addpage (INT dlg_id, UINT locale_id)
{
	PAPP_SETTINGS_PAGE ptr_page = (PAPP_SETTINGS_PAGE)_r_mem_allocatezero (sizeof (APP_SETTINGS_PAGE));

	ptr_page->dlg_id = dlg_id;
	ptr_page->locale_id = locale_id;

	app_settings_pages.push_back (ptr_page);
}

VOID _r_settings_createwindow (HWND hwnd, DLGPROC dlg_proc, INT dlg_id)
{
	if (_r_settings_getwindow ())
	{
		_r_wnd_toggle (_r_settings_getwindow (), TRUE);
		return;
	}

	if (dlg_id)
		_r_config_setinteger (L"SettingsLastPage", dlg_id);

	if (dlg_proc)
		app_settings_proc = dlg_proc;

	SHORT width = 0;
	SHORT height = 0;
	const WORD controls = 3;

	// calculate dialog size
	{
		UINT param_id;
		LPDLGTEMPLATEEX pdlg;

		for (auto it = app_settings_pages.begin (); it != app_settings_pages.end (); ++it)
		{
			if (!*it)
				continue;

			param_id = (*it)->dlg_id;

			if (!param_id)
				continue;

			pdlg = (LPDLGTEMPLATEEX)_r_loadresource (_r_sys_getimagebase (), MAKEINTRESOURCE (param_id), RT_DIALOG, NULL);

			if (pdlg)
			{
				if (width < pdlg->cx)
					width = pdlg->cx;

				if (height < pdlg->cy)
					height = pdlg->cy;
			}
		}

		height += 38;
		width += 112;
	}

	const SIZE_T size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) + ((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;

	PVOID lpbuffer = _r_mem_allocatezero (size);
	LPBYTE pPtr = (LPBYTE)lpbuffer;

	// set dialog information by filling DLGTEMPLATEEX structure
	_r_util_templatewriteshort (&pPtr, 1); // dlgVer
	_r_util_templatewriteshort (&pPtr, 0xFFFF); // signature
	_r_util_templatewriteulong (&pPtr, 0); // helpID
	_r_util_templatewriteulong (&pPtr, WS_EX_APPWINDOW | WS_EX_CONTROLPARENT); // exStyle
	_r_util_templatewriteulong (&pPtr, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME); // style
	_r_util_templatewriteshort (&pPtr, controls); // cdit
	_r_util_templatewriteshort (&pPtr, 0); // x
	_r_util_templatewriteshort (&pPtr, 0); // y
	_r_util_templatewriteshort (&pPtr, width); // cx
	_r_util_templatewriteshort (&pPtr, height); // cy

	// set dialog additional data
	_r_util_templatewritestring (&pPtr, L""); // menu
	_r_util_templatewritestring (&pPtr, L""); // windowClass
	_r_util_templatewritestring (&pPtr, L""); // title

	// set dialog font
	_r_util_templatewriteshort (&pPtr, 8); // pointsize
	_r_util_templatewriteshort (&pPtr, FW_NORMAL); // weight
	_r_util_templatewriteshort (&pPtr, FALSE); // bItalic
	_r_util_templatewritestring (&pPtr, L"MS Shell Dlg"); // font

	// insert dialog controls
	_r_util_templatewritecontrol (&pPtr, IDC_NAV, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_TRACKSELECT | TVS_INFOTIP | TVS_NOHSCROLL, 8, 6, 88, (height - 14), WC_TREEVIEW);
	_r_util_templatewritecontrol (&pPtr, IDC_RESET, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, 88 + 14, (height - 22), 50, 14, WC_BUTTON);
	_r_util_templatewritecontrol (&pPtr, IDC_CLOSE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, (width - 58), (height - 22), 50, 14, WC_BUTTON);

	DialogBoxIndirect (NULL, (LPCDLGTEMPLATE)lpbuffer, hwnd, &_r_settings_wndproc);

	_r_mem_free (lpbuffer);
}

INT_PTR CALLBACK _r_settings_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			app_settings_hwnd = hwnd;

#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

#ifdef IDI_MAIN
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_r_app_getsharedimage (_r_sys_getimagebase (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)_r_app_getsharedimage (_r_sys_getimagebase (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXICON)));
#else
#pragma R_PRINT_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			INT dlg_id = _r_config_getinteger (L"SettingsLastPage", app_settings_pages.at (0)->dlg_id);

			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, _r_dc_getdpi (hwnd, _R_SIZE_ITEMHEIGHT), _r_dc_getdpi (hwnd, _R_SIZE_TREEINDENT));

			for (auto it = app_settings_pages.begin (); it != app_settings_pages.end (); ++it)
			{
				if (!*it)
					continue;

				PAPP_SETTINGS_PAGE ptr_page = *it;

				if (ptr_page->dlg_id)
				{
					ptr_page->hwnd = CreateDialog (NULL, MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, app_settings_proc);

					if (ptr_page->hwnd)
						SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);

					HTREEITEM hitem = _r_treeview_additem (hwnd, IDC_NAV, _r_locale_getstring (ptr_page->locale_id), NULL, I_IMAGENONE, (LPARAM)ptr_page);

					if (ptr_page->dlg_id == dlg_id)
						SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)hitem);
				}
			}

			PostMessage (hwnd, RM_LOCALIZE, 0, 0);

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
#endif // IDS_SETTINGS

			// localize navigation
			_r_treeview_setstyle (hwnd, IDC_NAV, 0, _r_dc_getdpi (hwnd, _R_SIZE_ITEMHEIGHT), _r_dc_getdpi (hwnd, _R_SIZE_TREEINDENT));

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

			// localize buttons
#ifdef IDC_RESET
#ifdef IDS_RESET
			_r_ctrl_settext (hwnd, IDC_RESET, _r_locale_getstring (IDS_RESET));
#else
			_r_ctrl_settext (hwnd, IDC_RESET, L"Reset");
#pragma R_PRINT_WARNING(IDS_RESET)
#endif // IDS_RESET
			_r_wnd_addstyle (hwnd, IDC_RESET, _r_app_isclassicui () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
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
			_r_wnd_addstyle (hwnd, IDC_CLOSE, _r_app_isclassicui () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#else
#pragma R_PRINT_WARNING(IDC_CLOSE)
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
			for (auto it = app_settings_pages.begin (); it != app_settings_pages.end (); ++it)
			{
				if (!*it)
					continue;

				PAPP_SETTINGS_PAGE ptr_page = *it;

				if (ptr_page->hwnd)
				{
					DestroyWindow (ptr_page->hwnd);
					ptr_page->hwnd = NULL;
				}
			}

			app_settings_hwnd = NULL;

			_r_wnd_top (_r_app_gethwnd (), _r_config_getboolean (L"AlwaysOnTop", _APP_ALWAYSONTOP));

#ifdef _APP_HAVE_UPDATES
			if (_r_config_getboolean (L"CheckUpdates", TRUE))
				_r_update_check (NULL);
#endif // _APP_HAVE_UPDATES

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;
			INT ctrl_id = PtrToInt ((PVOID)lphdr->idFrom);

			if (ctrl_id != IDC_NAV)
				break;

			switch (lphdr->code)
			{
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
						RECT rc_tree;

						if (GetWindowRect (lphdr->hwndFrom, &rc_tree))
						{
							MapWindowPoints (NULL, hwnd, (LPPOINT)&rc_tree, 2);

							RECT rc_child;

							if (GetClientRect (ptr_page_new->hwnd, &rc_child))
							{
								PostMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

								SetWindowPos (ptr_page_new->hwnd, NULL, (rc_tree.left * 2) + _r_calc_rectwidth (INT, &rc_tree), rc_tree.top, _r_calc_rectwidth (INT, &rc_child), _r_calc_rectheight (INT, &rc_child), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
							}
						}
					}

					break;
				}
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

					if (_r_show_confirmmessage (hwnd, NULL, str_content, NULL))
					{
						time_t current_timestamp = _r_unixtime_now ();

						_r_fs_makebackup (_r_config_getpath (), current_timestamp, TRUE);

						_r_config_initialize ();

#ifdef _APP_HAVE_AUTORUN
						_r_autorun_enable (NULL, FALSE);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_SKIPUAC
						_r_skipuac_enable (NULL, FALSE);
#endif // _APP_HAVE_SKIPUAC

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

						for (auto it = app_settings_pages.begin (); it != app_settings_pages.end (); ++it)
						{
							if (!*it)
								continue;

							PAPP_SETTINGS_PAGE ptr_page = *it;

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
#endif // _APP_HAVE_SETTINGS

#if defined(_APP_HAVE_SKIPUAC)
BOOLEAN _r_skipuac_isenabled ()
{
#ifndef _APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return FALSE;
#endif // _APP_NO_DEPRECATIONS

	// old task compatibility
	if (_r_config_getboolean (L"SkipUacIsEnabled", FALSE))
	{
		_r_config_setboolean (L"IsAdminTaskEnabled", TRUE);
		return TRUE;
	}

	return _r_config_getboolean (L"IsAdminTaskEnabled", FALSE);
}

HRESULT _r_skipuac_enable (HWND hwnd, BOOLEAN is_enable)
{
#ifndef _APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return E_NOTIMPL;
#endif // _APP_NO_DEPRECATIONS

	HRESULT status;

	VARIANT empty = {VT_EMPTY};

	ITaskService *taskService = NULL;
	ITaskFolder *taskFolder = NULL;
	ITaskDefinition *taskDefinition = NULL;
	IRegistrationInfo *taskRegistration = NULL;
	IPrincipal *taskPrincipal = NULL;
	ITaskSettings *taskSettings = NULL;
	ITaskSettings2 *taskSettings2 = NULL;
	IActionCollection *taskActionCollection = NULL;
	IAction *taskAction = NULL;
	IExecAction *taskExecAction = NULL;
	IRegisteredTask *taskRegisteredTask = NULL;

	BSTR root = NULL;
	BSTR name = NULL;
	BSTR name_old = NULL;
	BSTR author = NULL;
	BSTR url = NULL;
	BSTR timeLimit = NULL;
	BSTR path = NULL;
	BSTR directory = NULL;
	BSTR args = NULL;

	status = CoCreateInstance (CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (PVOID*)&taskService);

	if (FAILED (status))
		goto CleanupExit;

	status = taskService->Connect (empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	root = SysAllocString (L"\\");

	status = taskService->GetFolder (root, &taskFolder);

	if (FAILED (status))
		goto CleanupExit;

	// remove old task
	if (_r_config_getboolean (L"SkipUacIsEnabled", FALSE))
	{
		name_old = SysAllocString (_APP_SKIPUAC_NAME_OLD);

		taskFolder->DeleteTask (name_old, 0);

		_r_config_setboolean (L"SkipUacIsEnabled", FALSE);
	}

	name = SysAllocString (_APP_SKIPUAC_NAME);

	if (is_enable)
	{
		// create task
		status = taskService->NewTask (0, &taskDefinition);

		if (FAILED (status))
			goto CleanupExit;

		status = taskDefinition->get_RegistrationInfo (&taskRegistration);

		if (FAILED (status))
			goto CleanupExit;

		author = SysAllocString (_APP_AUTHOR);
		url = SysAllocString (_APP_WEBSITE_URL);

		taskRegistration->put_Author (author);
		taskRegistration->put_URI (url);

		status = taskDefinition->get_Settings (&taskSettings);

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
			if (SUCCEEDED (taskSettings->put_Compatibility ((TASK_COMPATIBILITY)i)))
				break;
		}

		// Set task settings (win7+)
		if (SUCCEEDED (taskSettings->QueryInterface (IID_ITaskSettings2, (PVOID*)&taskSettings2)))
		{
			taskSettings2->put_UseUnifiedSchedulingEngine (VARIANT_TRUE);
			taskSettings2->put_DisallowStartOnRemoteAppSession (VARIANT_TRUE);

			taskSettings2->Release ();
		}

		timeLimit = SysAllocString (L"PT0S");

		taskSettings->put_AllowDemandStart (VARIANT_TRUE);
		taskSettings->put_AllowHardTerminate (VARIANT_FALSE);
		taskSettings->put_ExecutionTimeLimit (timeLimit);
		taskSettings->put_DisallowStartIfOnBatteries (VARIANT_FALSE);
		taskSettings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
		taskSettings->put_StartWhenAvailable (VARIANT_TRUE);
		taskSettings->put_StopIfGoingOnBatteries (VARIANT_FALSE);
		//taskSettings->put_Priority (4); // NORMAL_PRIORITY_CLASS

		status = taskDefinition->get_Principal (&taskPrincipal);

		if (FAILED (status))
			goto CleanupExit;

		taskPrincipal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
		taskPrincipal->put_LogonType (TASK_LOGON_INTERACTIVE_TOKEN);

		status = taskDefinition->get_Actions (&taskActionCollection);

		if (FAILED (status))
			goto CleanupExit;

		status = taskActionCollection->Create (TASK_ACTION_EXEC, &taskAction);

		if (FAILED (status))
			goto CleanupExit;

		status = taskAction->QueryInterface (IID_IExecAction, (PVOID*)&taskExecAction);

		if (FAILED (status))
			goto CleanupExit;

		path = SysAllocString (_r_sys_getimagepathname ());
		directory = SysAllocString (_r_app_getdirectory ());
		args = SysAllocString (L"$(Arg0)");

		taskExecAction->put_Path (path);
		taskExecAction->put_WorkingDirectory (directory);
		taskExecAction->put_Arguments (args);

		// remove task
		taskFolder->DeleteTask (name, 0);

		status = taskFolder->RegisterTaskDefinition (name, taskDefinition, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty, &taskRegisteredTask);

		if (SUCCEEDED (status))
		{
			_r_config_setboolean (L"IsAdminTaskEnabled", TRUE);
		}
	}
	else
	{
		status = taskFolder->DeleteTask (name, 0);

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

	if (timeLimit)
		SysFreeString (timeLimit);

	if (path)
		SysFreeString (path);

	if (directory)
		SysFreeString (directory);

	if (args)
		SysFreeString (args);

	if (taskRegisteredTask)
		taskRegisteredTask->Release ();

	if (taskExecAction)
		taskExecAction->Release ();

	if (taskAction)
		taskAction->Release ();

	if (taskActionCollection)
		taskActionCollection->Release ();

	if (taskPrincipal)
		taskPrincipal->Release ();

	if (taskSettings)
		taskSettings->Release ();

	if (taskRegistration)
		taskRegistration->Release ();

	if (taskDefinition)
		taskDefinition->Release ();

	if (taskFolder)
		taskFolder->Release ();

	if (taskService)
		taskService->Release ();

	if (hwnd && FAILED (status))
		_r_show_errormessage (hwnd, NULL, status, NULL, NULL);

	return status;
}

BOOLEAN _r_skipuac_run ()
{
#ifndef _APP_NO_DEPRECATIONS
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
		return FALSE;
#endif // _APP_NO_DEPRECATIONS

	HRESULT status;

	VARIANT empty = {VT_EMPTY};

	ITaskService *taskService = NULL;
	ITaskFolder *taskFolder = NULL;
	ITaskDefinition *taskDefinition = NULL;
	IRegisteredTask *taskRegisteredTask = NULL;
	IActionCollection *taskActionCollection = NULL;
	IAction *taskAction = NULL;
	IExecAction *taskExecAction = NULL;
	IRunningTask* taskRunningTask = NULL;

	BSTR root = NULL;
	BSTR name = NULL;
	BSTR path = NULL;
	BSTR args = NULL;

	WCHAR arguments[512] = {0};
	VARIANT params;
	LPWSTR *arga;
	INT numargs;

	status = CoCreateInstance (CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (PVOID*)&taskService);

	if (FAILED (status))
		goto CleanupExit;

	status = taskService->Connect (empty, empty, empty, empty);

	if (FAILED (status))
		goto CleanupExit;

	root = SysAllocString (L"\\");

	status = taskService->GetFolder (root, &taskFolder);

	if (FAILED (status))
		goto CleanupExit;

	name = SysAllocString (_APP_SKIPUAC_NAME);

	status = taskFolder->GetTask (name, &taskRegisteredTask);

	if (FAILED (status))
		goto CleanupExit;

	status = taskRegisteredTask->get_Definition (&taskDefinition);

	if (FAILED (status))
		goto CleanupExit;

	status = taskDefinition->get_Actions (&taskActionCollection);

	if (FAILED (status))
		goto CleanupExit;

	// check path is to current module
	if (SUCCEEDED (taskActionCollection->get_Item (1, &taskAction)))
	{
		if (SUCCEEDED (taskAction->QueryInterface (IID_IExecAction, (PVOID*)&taskExecAction)))
		{
			if (SUCCEEDED (taskExecAction->get_Path (&path)))
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

	status = taskRegisteredTask->RunEx (params, TASK_RUN_AS_SELF, 0, NULL, &taskRunningTask);

	if (FAILED (status))
		goto CleanupExit;

	status = E_ABORT;

	// check if run succesfull
	ULONG attempts = 6;
	TASK_STATE state;

	do
	{
		taskRunningTask->Refresh ();

		if (SUCCEEDED (taskRunningTask->get_State (&state)))
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
	while (attempts--);

CleanupExit:

	if (root)
		SysFreeString (root);

	if (name)
		SysFreeString (name);

	if (path)
		SysFreeString (path);

	if (args)
		SysFreeString (args);

	if (taskRunningTask)
		taskRunningTask->Release ();

	if (taskExecAction)
		taskExecAction->Release ();

	if (taskAction)
		taskAction->Release ();

	if (taskActionCollection)
		taskActionCollection->Release ();

	if (taskDefinition)
		taskDefinition->Release ();

	if (taskRegisteredTask)
		taskRegisteredTask->Release ();

	if (taskFolder)
		taskFolder->Release ();

	if (taskService)
		taskService->Release ();

	return SUCCEEDED (status);
}
#endif // _APP_HAVE_SKIPUAC
