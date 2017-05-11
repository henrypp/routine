// routine++
// Copyright (c) 2012-2017 Henry++

#include "rapp.h"

#ifdef _APP_HAVE_TRAY
CONST UINT WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

rapp::rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// initialize security attributes
	SecureZeroMemory (&sd, sizeof (sd));
	SecureZeroMemory (&sa, sizeof (sa));

	_r_sys_setsecurityattributes (&sa, sizeof (sa), &sd);

	// initialize controls
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	InitCommonControlsEx (&icex);

	// store system information
	is_vistaorlater = _r_sys_validversion (6, 0);
	is_admin = _r_sys_adminstate ();

	// general information
	StringCchCopy (app_name, _countof (app_name), name);
	StringCchCopy (app_name_short, _countof (app_name_short), short_name);
	StringCchCopy (app_version, _countof (app_version), version);
	StringCchCopy (app_copyright, _countof (app_copyright), copyright);

	// get hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// get path
	GetModuleFileName (nullptr, app_binary, _countof (app_binary));
	StringCchCopy (app_directory, _countof (app_directory), app_binary);
	PathRemoveFileSpec (app_directory);

	// get configuration path
	StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", GetDirectory (), app_name_short);

	if (!_r_fs_exists (app_config_path))
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), _r_path_expand (L"%APPDATA%\\" _APP_AUTHOR L"\\"));
		StringCchCat (app_profile_directory, _countof (app_profile_directory), app_name);
		StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_profile_directory, app_name_short);
	}
	else
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), GetDirectory ());
	}

	HDC h = GetDC (nullptr);
	dpi_percent = DOUBLE (GetDeviceCaps (h, LOGPIXELSX)) / 96.0f;
	ReleaseDC (nullptr, h);

	ConfigInit ();
}

rapp::~rapp ()
{
	if (app_callback)
		app_callback (GetHWND (), _RM_UNINITIALIZE, nullptr, nullptr);


#ifndef _APP_NO_SETTINGS
	ClearSettingsPage ();
#endif // _APP_NO_SETTINGS

	UninitializeMutex ();
}

BOOL rapp::InitializeMutex ()
{
	UninitializeMutex ();

	app_mutex = CreateMutex (&sa, FALSE, app_name_short);

	return TRUE;
}

BOOL rapp::UninitializeMutex ()
{
	if (app_mutex)
	{
		CloseHandle (app_mutex);

		app_mutex = nullptr;

		return TRUE;
	}

	return FALSE;
}

BOOL rapp::CheckMutex (BOOL activate_window)
{
	BOOL result = FALSE;

	HANDLE h = CreateMutex (&sa, FALSE, app_name_short);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		result = TRUE;

		if (activate_window)
		{
			EnumWindows (&ActivateWindowCallback, (LPARAM)this);
		}
	}

	if (h)
		CloseHandle (h);

	return result;
}

BOOL CALLBACK rapp::ActivateWindowCallback (HWND hwnd, LPARAM lparam)
{
	if (!lparam)
		return FALSE;

	DWORD pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

	if (GetCurrentProcessId () == pid)
		return TRUE;

	WCHAR title[128] = {0};
	GetWindowText (hwnd, title, _countof (title));

	rapp const* ptr = (rapp*)lparam;

	if (ptr && _wcsnicmp (title, ptr->app_name, wcslen (ptr->app_name)) != 0)
		return TRUE;

	DWORD access_rights = PROCESS_QUERY_LIMITED_INFORMATION; // vista and later

#ifndef _APP_NO_WINXP
	if (ptr && !ptr->IsVistaOrLater ())
		access_rights = PROCESS_QUERY_INFORMATION; // winxp
#endif // _APP_NO_WINXP

	HANDLE h = OpenProcess (access_rights, FALSE, pid);

	if (h)
	{
		WCHAR fname[1024] = {0};

		if (_r_process_getpath (h, fname, _countof (fname)))
		{
			if (ptr && _wcsnicmp (_r_path_extractfile (fname), ptr->app_name_short, wcslen (ptr->app_name_short)) == 0)
			{
				_r_wnd_toggle (hwnd, TRUE);

				CloseHandle (h);

				return FALSE;
			}
		}

		CloseHandle (h);
	}

	return TRUE;
}

#ifdef _APP_HAVE_AUTORUN
VOID rapp::AutorunEnable (BOOL is_enable)
{
	HKEY key = nullptr;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			WCHAR buffer[MAX_PATH] = {0};
			StringCchCopy (buffer, _countof (buffer), GetBinaryPath ());
			PathQuoteSpaces (buffer);
			StringCchCat (buffer, _countof (buffer), L" /minimized");

			RegSetValueEx (key, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) + 1) * sizeof (WCHAR)));
		}
		else
		{
			RegDeleteValue (key, app_name);
		}

		ConfigSet (L"AutorunIsEnabled", is_enable);

		RegCloseKey (key);
	}
}

BOOL rapp::AutorunIsEnabled ()
{
	return ConfigGet (L"AutorunIsEnabled", FALSE).AsBool ();
}
#endif // _APP_HAVE_AUTORUN

#ifndef _APP_NO_UPDATES
VOID rapp::CheckForUpdates (BOOL is_periodical)
{
	if (update_lock)
		return;

	if (is_periodical)
	{
		if (!ConfigGet (L"CheckUpdates", TRUE).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0).AsLonglong ()) <= _APP_UPDATE_PERIOD)
			return;
	}

	is_update_forced = is_periodical;

	_beginthreadex (nullptr, 0, &CheckForUpdatesProc, (LPVOID)this, 0, nullptr);

	update_lock = TRUE;
}
#endif // _APP_NO_UPDATES

VOID rapp::ConfigInit ()
{
	app_config_array.clear (); // reset

	ParseINI (app_config_path, &app_config_array);

	LocaleInit ();

	// check for updates
#ifndef _APP_NO_UPDATES
	CheckForUpdates (TRUE);
#endif // _APP_NO_UPDATES
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%d", def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name) const
{
	rstring result;

	if (!name)
	{
		name = app_name_short;
	}

	// check key is exists
	if (app_config_array.find (name) != app_config_array.end () && app_config_array.at (name).find (key) != app_config_array.at (name).end ())
	{
		result = app_config_array.at (name).at (key);
	}

	if (result.IsEmpty ())
	{
		result = def;
	}

	return result;
}

BOOL rapp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_fs_exists (app_profile_directory))
	{
		_r_fs_mkdir (app_profile_directory);
	}

	if (!name)
	{
		name = app_name_short;
	}

	// update hash value
	app_config_array[name][key] = val;

	return WritePrivateProfileString (name, key, val, app_config_path);
}

BOOL rapp::ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%lld", val), name);
}

#ifndef _APP_NO_ABOUT
VOID rapp::CreateAboutWindow ()
{

	if (!is_about_opened)
	{
		is_about_opened = TRUE;

#ifdef _WIN64
		const unsigned architecture = 64;
#else
		const unsigned architecture = 32;
#endif // _WIN64

		if (IsVistaOrLater ())
			_r_msg (GetHWND (), MB_OK | MB_USERICON, I18N (this, IDS_ABOUT, 0), app_name, L"Version %s, %d-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", app_version, architecture, app_copyright, _APP_WEBSITE_URL, _APP_WEBSITE_URL + 7, _APP_GITHUB_URL, _APP_GITHUB_URL + 8);
		else
			_r_msg (GetHWND (), MB_OK | MB_USERICON, I18N (this, IDS_ABOUT, 0), app_name, L"Version %s, %d-bit (Unicode)\r\n%s\r\n\r\n%s | %s", app_version, architecture, app_copyright, _APP_WEBSITE_URL + 7, _APP_GITHUB_URL + 8);

		is_about_opened = FALSE;
	}
}
#endif // _APP_NO_ABOUT

BOOL rapp::IsAdmin () const
{
	return is_admin;
}

BOOL rapp::IsClassicUI () const
{
	return is_classic;
}

BOOL rapp::IsVistaOrLater () const
{
	return is_vistaorlater;
}

VOID rapp::SetIcon (UINT icon_id)
{
	if (app_icon_1)
		DestroyIcon (app_icon_1);

	if (app_icon_2)
		DestroyIcon (app_icon_2);

	app_icon_1 = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXSMICON));
	app_icon_2 = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXICON));

	SendMessage (GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)app_icon_1);
	SendMessage (GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)app_icon_2);
}

LRESULT CALLBACK rapp::MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = (rapp*)GetWindowLongPtr (hwnd, GWLP_USERDATA);

#ifdef _APP_HAVE_TRAY
	if (msg == WM_TASKBARCREATED)
	{
		if (this_ptr->app_callback)
		{
			this_ptr->app_callback (hwnd, _RM_UNINITIALIZE, nullptr, nullptr);
			this_ptr->app_callback (hwnd, _RM_INITIALIZE, nullptr, nullptr);
		}

		return FALSE;
	}
#endif // _APP_HAVE_TRAY

	switch (msg)
	{
		case WM_THEMECHANGED:
		{
			this_ptr->is_classic = !IsThemeActive () || this_ptr->ConfigGet (L"ClassicUI", FALSE).AsBool ();

			if (this_ptr->app_callback)
				this_ptr->app_callback (this_ptr->GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

			break;
		}

		case WM_DESTROY:
		{
#ifdef _APP_HAVE_SIZING
			RECT rc = {0};
			GetWindowRect (hwnd, &rc);

			if (!IsZoomed (hwnd))
			{
				this_ptr->ConfigSet (L"WindowPosX", rc.left);
				this_ptr->ConfigSet (L"WindowPosY", rc.top);
				this_ptr->ConfigSet (L"WindowPosWidth", rc.right - rc.left);
				this_ptr->ConfigSet (L"WindowPosHeight", rc.bottom - rc.top);
			}

			this_ptr->ConfigSet (L"IsWindowZoomed", IsZoomed (hwnd));
#endif // _APP_HAVE_SIZING

			break;
		}

#ifdef _APP_HAVE_SIZING
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lparam;

			lpmmi->ptMinTrackSize.x = this_ptr->max_width;
			lpmmi->ptMinTrackSize.y = this_ptr->max_height;

			break;
		}
#endif // _APP_HAVE_SIZING

		case WM_SIZE:
		{
#ifdef _APP_HAVE_TRAY
			if (wparam == SIZE_MINIMIZED)
			{
				_r_wnd_toggle (hwnd, FALSE);
				return TRUE;
			}
#endif // _APP_HAVE_TRAY

			break;
		}

		case WM_SYSCOMMAND:
		{
#ifdef _APP_HAVE_SIZING
			if (wparam == SC_RESTORE)
			{
				RECT rc = {0};
				GetWindowRect (hwnd, &rc);

				this_ptr->ConfigSet (L"WindowPosX", rc.left);
				this_ptr->ConfigSet (L"WindowPosY", rc.top);
				this_ptr->ConfigSet (L"WindowPosWidth", rc.right - rc.left);
				this_ptr->ConfigSet (L"WindowPosHeight", rc.bottom - rc.top);
			}
#endif // _APP_HAVE_SIZING

#ifdef _APP_HAVE_TRAY
			if (wparam == SC_CLOSE)
			{
				_r_wnd_toggle (hwnd, FALSE);
				return TRUE;
			}
#endif // _APP_HAVE_TRAY

			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
	}

	if (!this_ptr->app_wndproc) // check (required!)
		return FALSE;

	return CallWindowProc (this_ptr->app_wndproc, hwnd, msg, wparam, lparam);
}

BOOL rapp::CreateMainWindow (DLGPROC proc, APPLICATION_CALLBACK callback)
{
	BOOL result = FALSE;

	// check checksum
	{
		HINSTANCE h = LoadLibrary (L"imagehlp.dll");

		if (h)
		{
			MFACS _MapFileAndCheckSumW = (MFACS)GetProcAddress (h, "MapFileAndCheckSumW");

			if (_MapFileAndCheckSumW)
			{
				DWORD dwFileChecksum = 0, dwRealChecksum = 0;

				_MapFileAndCheckSumW (GetBinaryPath (), &dwFileChecksum, &dwRealChecksum);

				if (dwRealChecksum != dwFileChecksum)
					return FALSE;
			}

			FreeLibrary (h);
		}
	}

	// check arguments
	INT numargs = 0;
	LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

	if (arga)
	{
		if (callback && numargs > 1)
		{
			if (callback (nullptr, _RM_ARGUMENTS, nullptr, nullptr))
				return FALSE;
		}

		LocalFree (arga);
	}

	if (CheckMutex (TRUE))
		return FALSE;

#ifdef _APP_HAVE_SKIPUAC
	if (RunAsAdmin ())
		return FALSE;

#ifdef _APP_NO_GUEST
	if (!IsAdmin ())
		return FALSE;
#endif // _APP_NO_GUEST

#endif // _APP_HAVE_SKIPUAC

#ifdef _APP_HAVE_SKIPUAC
	SkipUacEnable (ConfigGet (L"SkipUacIsEnabled", FALSE).AsBool ());
#endif // _APP_HAVE_SKIPUAC

	InitializeMutex ();

	if (ConfigGet (L"ClassicUI", FALSE).AsBool () || !IsThemeActive ())
	{
		is_classic = TRUE;

		if (ConfigGet (L"ClassicUI", FALSE).AsBool ())
			SetThemeAppProperties (1);
	}

	// create window
#ifdef IDD_MAIN
	app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc);
#else
	UNREFERENCED_PARAMETER (proc);
#endif // IDD_MAIN

	if (app_hwnd)
	{
		// remove focus
		SetFocus (nullptr);

		// set autorun state
#ifdef _APP_HAVE_AUTORUN
		AutorunEnable (AutorunIsEnabled ());
#endif // _APP_HAVE_AUTORUN

		// set window on top
		_r_wnd_top (GetHWND (), ConfigGet (L"AlwaysOnTop", FALSE).AsBool ());

		// set minmax info
#ifdef _APP_HAVE_SIZING
		{
			RECT rc = {0};

			if (GetWindowRect (GetHWND (), &rc))
			{
				max_width = (rc.right - rc.left);
				max_height = (rc.bottom - rc.top);
			}

			if (GetClientRect (GetHWND (), &rc))
			{
				SendMessage (GetHWND (), WM_SIZE, 0, MAKELPARAM ((rc.right - rc.left), (rc.bottom - rc.top)));
			}
		}
#endif // _APP_HAVE_SIZING

		// set window pos
#ifdef _APP_HAVE_SIZING
		{
			const INT xpos = ConfigGet (L"WindowPosX", 0).AsInt ();
			const INT ypos = ConfigGet (L"WindowPosY", 0).AsInt ();
			const INT width = ConfigGet (L"WindowPosWidth", 0).AsInt ();
			const INT height = ConfigGet (L"WindowPosHeight", 0).AsInt ();

			DWORD flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE;

			if (xpos <= 0 || ypos <= 0)
				flags |= SWP_NOMOVE;

			if (width <= 0 || height <= 0)
				flags |= SWP_NOSIZE;

			SetWindowPos (GetHWND (), nullptr, xpos, ypos, width, height, flags);
		}
#endif // _APP_HAVE_SIZING

		{
			BOOL is_minimized = FALSE;

			// show window
#ifdef _APP_STARTMINIMIZED
			is_minimized = TRUE;
#else
#ifdef _APP_HAVE_TRAY

			if (wcsstr (GetCommandLine (), L"/minimized") || ConfigGet (L"StartMinimized", FALSE).AsBool ())
				is_minimized = TRUE;
#endif // _APP_HAVE_TRAY
#endif // _APP_STARTMINIMIZED

			if (!is_minimized)
			{
				INT code = SW_SHOW;

#ifdef _APP_HAVE_SIZING
				if (ConfigGet (L"IsWindowZoomed", FALSE).AsBool ())
					code = SW_SHOWMAXIMIZED;
#endif // _APP_HAVE_SIZING

				ShowWindow (GetHWND (), code);
			}
			else
			{
#ifndef _APP_HAVE_TRAY
				ShowWindow (GetHWND (), SW_SHOWMINIMIZED);
#endif // _APP_HAVE_TRAY

#ifdef _APP_HAVE_SIZING
				//if (ConfigGet (L"IsWindowZoomed", FALSE).AsBool ())
				{
					//ShowWindow (GetHWND (), SW_SHOWMAXIMIZED);
				}
#endif // _APP_HAVE_SIZING
			}
		}

		// enable messages bypass uipi
#ifdef _APP_HAVE_TRAY
		_r_wnd_changemessagefilter (GetHWND (), WM_TASKBARCREATED, MSGFLT_ALLOW);
#endif // _APP_HAVE_TRAY

		_r_wnd_changemessagefilter (GetHWND (), WM_DROPFILES, MSGFLT_ALLOW);
		_r_wnd_changemessagefilter (GetHWND (), WM_COPYDATA, MSGFLT_ALLOW);
		_r_wnd_changemessagefilter (GetHWND (), 0x0049, MSGFLT_ALLOW); // WM_COPYGLOBALDATA

		// subclass window
		SetWindowLongPtr (GetHWND (), GWLP_USERDATA, (LONG_PTR)this);
		app_wndproc = (WNDPROC)SetWindowLongPtr (GetHWND (), DWLP_DLGPROC, (LONG_PTR)&MainWindowProc);

		// set icons
#ifdef IDI_MAIN
		SetIcon (IDI_MAIN);
#endif // IDI_MAIN

		// initialization callback
		if (callback)
		{
			app_callback = callback;

			app_callback (GetHWND (), _RM_INITIALIZE, nullptr, nullptr);
			app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

			DrawMenuBar (GetHWND ()); // redraw menu
		}

		result = TRUE;
	}
	else
	{
		result = FALSE;
	}

	return result;
}

#ifdef _APP_HAVE_TRAY
BOOL rapp::TrayCreate (HWND hwnd, UINT uid, UINT code, HICON h, BOOL is_hidden)
{
	BOOL result = FALSE;

	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uVersion = IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
	nid.hWnd = hwnd;
	nid.uID = uid;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_SHOWTIP | NIF_TIP;
	nid.uCallbackMessage = code;
	nid.hIcon = h;
	StringCchCopy (nid.szTip, _countof (nid.szTip), app_name);

	if (is_hidden)
	{
		nid.uFlags |= NIF_STATE;
		nid.dwState = NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
	}

	result = Shell_NotifyIcon (NIM_ADD, &nid);
	Shell_NotifyIcon (NIM_SETVERSION, &nid);

	return result;
}

BOOL rapp::TrayDestroy (UINT uid)
{
	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uID = uid;

	if (nid.hIcon)
	{
		DestroyIcon (nid.hIcon);
		nid.hIcon = nullptr;
	}

	return Shell_NotifyIcon (NIM_DELETE, &nid);
}

BOOL rapp::TrayPopup (DWORD icon, LPCWSTR title, LPCWSTR text)
{
	BOOL result = FALSE;

	nid.uFlags = NIF_INFO | NIF_REALTIME;
	nid.dwInfoFlags = NIIF_LARGE_ICON | icon;

	// tooltip-visibility fix
	if (nid.szTip[0])
		nid.uFlags |= (NIF_SHOWTIP | NIF_TIP);

	if (title)
		StringCchCopy (nid.szInfoTitle, _countof (nid.szInfoTitle), title);

	if (text)
		StringCchCopy (nid.szInfo, _countof (nid.szInfo), text);

	result = Shell_NotifyIcon (NIM_MODIFY, &nid);

	nid.szInfo[0] = nid.szInfoTitle[0] = 0; // clear

	return result;
}

BOOL rapp::TraySetInfo (HICON h, LPCWSTR tooltip)
{
	nid.uFlags = 0;

	if (tooltip)
	{
		nid.uFlags |= (NIF_SHOWTIP | NIF_TIP);
		StringCchCopy (nid.szTip, _countof (nid.szTip), tooltip);
	}

	if (h)
	{
		if (nid.hIcon)
		{
			DestroyIcon (nid.hIcon);
			nid.hIcon = nullptr;
		}

		nid.uFlags |= NIF_ICON;
		nid.hIcon = h;
	}

	return Shell_NotifyIcon (NIM_MODIFY, &nid);
}

BOOL rapp::TrayToggle (DWORD uid, BOOL is_show)
{
	nid.uID = uid;
	nid.uFlags = NIF_STATE;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	return Shell_NotifyIcon (NIM_MODIFY, &nid);
}
#endif // _APP_HAVE_TRAY

#ifndef _APP_NO_SETTINGS
VOID rapp::CreateSettingsWindow ()
{
	static bool is_opened = false;

	if (!is_opened)
	{
		is_opened = true;

#ifdef IDD_SETTINGS
		DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), GetHWND (), &SettingsWndProc, (LPARAM)this);
#endif // IDD_SETTINGS
	}

	is_opened = false;
}

size_t rapp::AddSettingsPage (HINSTANCE h, UINT dlg_id, UINT locale_id, LPCWSTR locale_sid, APPLICATION_CALLBACK callback, size_t group_id, LPARAM lparam)
{
	PAPP_SETTINGS_PAGE ptr = new APP_SETTINGS_PAGE;

	if (ptr)
	{
		ptr->hwnd = nullptr;

		ptr->h = h;
		ptr->group_id = group_id;
		ptr->dlg_id = dlg_id;
		ptr->callback = callback;
		ptr->lparam = lparam;

		ptr->locale_id = locale_id;
		StringCchCopy (ptr->locale_sid, _countof (ptr->locale_sid), locale_sid);

		app_settings_pages.push_back (ptr);

		return app_settings_pages.size () - 1;
	}

	return LAST_VALUE;
}

VOID rapp::ClearSettingsPage ()
{
	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		PAPP_SETTINGS_PAGE const ptr = app_settings_pages.at (i);

		delete ptr;
	}

	app_settings_pages.clear ();

#ifdef _APP_HAVE_SIMPLE_SETTINGS
	for (auto const& p : app_configs)
	{
		PAPP_SETTINGS_CONFIG const ptr = p.second;

		delete ptr;
	}

	app_configs.clear ();

#endif // _APP_HAVE_SIMPLE_SETTINGS
}

VOID rapp::InitSettingsPage (HWND hwnd, BOOL is_newlocale)
{
	if (is_newlocale)
	{
		// localize window
		SetWindowText (hwnd, I18N (this, IDS_SETTINGS, 0));

		SetDlgItemText (hwnd, IDC_APPLY, I18N (this, IDS_APPLY, 0));
		SetDlgItemText (hwnd, IDC_CLOSE, I18N (this, IDS_CLOSE, 0));
	}

	// apply classic ui for buttons
	_r_wnd_addstyle (hwnd, IDC_APPLY, IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_CLOSE, IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

	// initialize treeview
	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		PAPP_SETTINGS_PAGE const ptr = app_settings_pages.at (i);

		if (is_newlocale)
		{
			TVITEMEX tvi = {0};

			tvi.mask = TVIF_PARAM;
			tvi.hItem = ptr->item;

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETITEM, 0, (LPARAM)&tvi);

			rstring text = LocaleString (ptr->h, ptr->locale_id, ptr->locale_sid);

			tvi.mask = TVIF_TEXT;
			tvi.pszText = text.GetBuffer ();

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_SETITEM, 0, (LPARAM)&tvi);

			text.Clear ();
		}

		if (ptr->dlg_id && ptr->callback)
			ptr->callback (ptr->hwnd, _RM_INITIALIZE, nullptr, ptr);
	}

	_r_ctrl_enable (hwnd, IDC_APPLY, FALSE);
}
#endif // _APP_NO_SETTINGS

#ifdef _APP_HAVE_SIMPLE_SETTINGS
VOID rapp::AddSettingsItem (LPCWSTR name, LPCWSTR def_value, CfgType type, UINT locale_id, LPCWSTR locale_sid)
{
	if (app_configs.find (name) != app_configs.end ())
		return;

	PAPP_SETTINGS_CONFIG ptr = new APP_SETTINGS_CONFIG;

	if (ptr)
	{
		ptr->type = type;
		StringCchCopy (ptr->def_value, _countof (ptr->def_value), def_value);

		ptr->locale_id = locale_id;
		StringCchCopy (ptr->locale_sid, _countof (ptr->locale_sid), locale_sid);

		app_configs[name] = ptr;
	}
}
#endif // _APP_HAVE_SIMPLE_SETTINGS

rstring rapp::GetBinaryPath () const
{
	return app_binary;
}

rstring rapp::GetDirectory () const
{
	return app_directory;
}

rstring rapp::GetProfileDirectory () const
{
	return app_profile_directory;
}

rstring rapp::GetUserAgent () const
{
	return ConfigGet (L"UserAgent", _r_fmt (L"%s/%s (+%s)", app_name, app_version, _APP_WEBSITE_URL));
}

INT rapp::GetDPI (INT v) const
{
	return (INT)ceil (static_cast<DOUBLE>(v) * dpi_percent);
}

HINSTANCE rapp::GetHINSTANCE () const
{
	return app_hinstance;
}

HWND rapp::GetHWND () const
{
	return app_hwnd;
}

VOID rapp::LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id)
{
	if (selected_id == default_id)
	{
		ConfigSet (L"Language", nullptr);
	}
	else
	{
		WCHAR buffer[MAX_PATH] = {0};

		GetMenuString (hmenu, selected_id, buffer, _countof (buffer), MF_BYCOMMAND);

		ConfigSet (L"Language", buffer);
	}

	LocaleInit ();

	if (app_callback)
	{
		app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

		DrawMenuBar (GetHWND ()); // redraw menu
	}
}

VOID rapp::LocaleEnum (HWND hwnd, INT ctrl_id, BOOL is_menu, const UINT id_start)
{
	HMENU hmenu = nullptr;

	if (is_menu)
	{
		hmenu = GetSubMenu ((HMENU)hwnd, ctrl_id);

		// clear menu
		for (UINT i = 0;; i++)
		{
			if (!DeleteMenu (hmenu, id_start + i, MF_BYCOMMAND))
			{
				DeleteMenu (hmenu, 0, MF_BYPOSITION); // delete separator
				break;
			}
		}

		AppendMenu (hmenu, MF_STRING, id_start, _APP_LANGUAGE_DEFAULT);
		CheckMenuRadioItem (hmenu, id_start, id_start, id_start, MF_BYCOMMAND);
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, 0, (LPARAM)_APP_LANGUAGE_DEFAULT);
		SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, 0, 0);
	}

	WIN32_FIND_DATA wfd = {0};
	HANDLE h = FindFirstFile (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\*.ini", GetDirectory ()), &wfd);

	if (h != INVALID_HANDLE_VALUE)
	{
		app_locale_count = 0;
		rstring def = ConfigGet (L"Language", nullptr);

		if (is_menu)
			AppendMenu (hmenu, MF_SEPARATOR, 0, nullptr);

		do
		{
			LPWSTR fname = wfd.cFileName;
			PathRemoveExtension (fname);

			if (is_menu)
			{
				AppendMenu (hmenu, MF_STRING, (++app_locale_count + id_start), fname);

				if (!def.IsEmpty () && def.CompareNoCase (fname) == 0)
					CheckMenuRadioItem (hmenu, id_start, id_start + app_locale_count, id_start + app_locale_count, MF_BYCOMMAND);
			}
			else
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, ++app_locale_count, (LPARAM)fname);

				if (!def.IsEmpty () && def.CompareNoCase (fname) == 0)
					SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, app_locale_count, 0);
			}
		}
		while (FindNextFile (h, &wfd));

		FindClose (h);
	}
	else
	{
		if (is_menu)
		{
			EnableMenuItem ((HMENU)hwnd, ctrl_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
		}
		else
		{
			EnableWindow (GetDlgItem (hwnd, ctrl_id), FALSE);
		}
	}
}

UINT rapp::LocaleGetCount ()
{
	return app_locale_count;
}

VOID rapp::LocaleInit ()
{
	rstring name = ConfigGet (L"Language", nullptr);

	app_locale_array.clear (); // clear
	is_localized = FALSE;

	if (!name.IsEmpty ())
		is_localized = ParseINI (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\%s.ini", GetDirectory (), name), &app_locale_array);
}

rstring rapp::LocaleString (HINSTANCE h, UINT uid, LPCWSTR name)
{
	rstring result;

	if (!uid)
	{
		result = name;
	}
	else
	{
		if (is_localized)
		{
			// check key is exists
			if (app_locale_array[_APP_I18N_SECTION].find (name) != app_locale_array[_APP_I18N_SECTION].end ())
			{
				result = app_locale_array[_APP_I18N_SECTION][name];

				result.Replace (L"\\t", L"\t");
				result.Replace (L"\\r", L"\r");
				result.Replace (L"\\n", L"\n");
			}
		}

		if (result.IsEmpty ())
		{
			if (!h)
			{
				h = GetHINSTANCE ();
			}

			LoadString (h, uid, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH);
			result.ReleaseBuffer ();
		}
	}

	return result;
}

VOID rapp::LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position) const
{
	if (text)
	{
		rstring ptr = text;

		MENUITEMINFO mif = {0};

		mif.cbSize = sizeof (MENUITEMINFO);
		mif.fMask = MIIM_STRING;
		mif.dwTypeData = ptr.GetBuffer ();

		SetMenuItemInfo (menu, item, by_position, &mif);
	}
}

#ifndef _APP_NO_SETTINGS
INT_PTR CALLBACK rapp::SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);
			break;
		}

		case WM_COMMAND:
		case WM_CONTEXTMENU:
		case WM_NOTIFY:
		case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
		{
			BOOL result = FALSE;

			MSG wmsg = {0};

			wmsg.message = msg;
			wmsg.wParam = wparam;
			wmsg.lParam = lparam;

			PAPP_SETTINGS_PAGE const ptr = this_ptr->app_settings_pages.at (this_ptr->ConfigGet (L"SettingsLastPage", 0).AsSizeT ());

			if (!ptr->callback)
				break;

			result = ptr->callback (hwnd, _RM_MESSAGE, &wmsg, ptr);

			if (msg == WM_COMMAND)
			{
				BOOL is_button = (GetWindowLongPtr (GetDlgItem (hwnd, LOWORD (wparam)), GWL_STYLE) & (BS_CHECKBOX | BS_RADIOBUTTON)) != 0;

				if (lparam && ((HIWORD (wparam) == BN_CLICKED && is_button) || (HIWORD (wparam) == EN_CHANGE || HIWORD (wparam) == CBN_SELENDOK)))
					_r_ctrl_enable (GetParent (hwnd), IDC_APPLY, TRUE);
			}
			else if (msg == WM_NOTIFY)
			{
				if (LPNMHDR (lparam)->code == UDN_DELTAPOS || (LPNMHDR (lparam)->code == LVN_ITEMCHANGED && (LPNMLISTVIEW (lparam)->uNewState == 8192 || LPNMLISTVIEW (lparam)->uNewState == 4096)) || (LPNMHDR (lparam)->code == LVN_DELETEITEM) || (LPNMHDR (lparam)->code == LVN_ENDLABELEDIT && result))
					_r_ctrl_enable (GetParent (hwnd), IDC_APPLY, TRUE);
			}

			return result;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK rapp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);

			// configure window
			_r_wnd_center (hwnd);

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, GetSystemMetrics (SM_CYSMICON));

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				PAPP_SETTINGS_PAGE ptr = this_ptr->app_settings_pages.at (i);

				if (ptr->dlg_id)
					ptr->hwnd = CreateDialogParam (ptr->h, MAKEINTRESOURCE (ptr->dlg_id), hwnd, &this_ptr->SettingsPagesProc, (LPARAM)this_ptr);

				ptr->item = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (ptr->h, ptr->locale_id, ptr->locale_sid), ptr->group_id == LAST_VALUE ? nullptr : this_ptr->app_settings_pages.at (ptr->group_id)->item, LAST_VALUE, (LPARAM)i);

				if (this_ptr->ConfigGet (L"SettingsLastPage", 0).AsSizeT () == i)
					SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)ptr->item);
			}

#ifdef _APP_HAVE_SIMPLE_SETTINGS
			for (auto const &p : this_ptr->app_configs)
			{
				_r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (nullptr, p.second->locale_id, p.second->locale_sid), this_ptr->app_settings_pages.at (0)->item, LAST_VALUE, 0);
			}
#endif // _APP_HAVE_SIMPLE_SETTINGS

			this_ptr->InitSettingsPage (hwnd, TRUE);

			break;
		}

		case WM_DESTROY:
		{
			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				PAPP_SETTINGS_PAGE const ptr = this_ptr->app_settings_pages.at (i);

				// send close message to settings callback
				if (!ptr->h && ptr->callback)
				{
					ptr->callback (ptr->hwnd, _RM_UNINITIALIZE, nullptr, ptr);
					break;
				}
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			if (lphdr->idFrom == IDC_NAV)
			{
				switch (lphdr->code)
				{
					case TVN_SELCHANGED:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						size_t old_id = size_t (pnmtv->itemOld.lParam);
						size_t new_id = size_t (pnmtv->itemNew.lParam);

						if (this_ptr->app_settings_pages.at (old_id)->hwnd)
							ShowWindow (this_ptr->app_settings_pages.at (old_id)->hwnd, SW_HIDE);

						if (this_ptr->app_settings_pages.at (new_id)->hwnd)
						{
							ShowWindow (this_ptr->app_settings_pages.at (new_id)->hwnd, SW_SHOW);
							SetFocus (this_ptr->app_settings_pages.at (new_id)->hwnd);
						}

						this_ptr->ConfigSet (L"SettingsLastPage", new_id);

						break;
					}
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDOK: // process Enter key
				case IDC_APPLY:
				{
					// if not enabled - nothing is changed!
					if (!IsWindowEnabled (GetDlgItem (hwnd, IDC_APPLY)))
						return FALSE;

					BOOL is_newlocale = FALSE;

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						PAPP_SETTINGS_PAGE const ptr = this_ptr->app_settings_pages.at (i);

						if (ptr->dlg_id && ptr->callback)
						{
							if (ptr->callback (ptr->hwnd, _RM_SAVE, nullptr, ptr))
								is_newlocale = TRUE;
						}
					}

					this_ptr->ConfigInit (); // re-read settings

					// reinitialization
					if (this_ptr->app_callback)
					{
						this_ptr->app_callback (this_ptr->GetHWND (), _RM_UNINITIALIZE, nullptr, nullptr);
						this_ptr->app_callback (this_ptr->GetHWND (), _RM_INITIALIZE, nullptr, nullptr);

						if (is_newlocale)
						{
							this_ptr->app_callback (this_ptr->GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

							DrawMenuBar (this_ptr->GetHWND ()); // redraw menu
						}
					}

					this_ptr->InitSettingsPage (hwnd, is_newlocale);

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);

					_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", FALSE).AsBool ());

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
#endif // _APP_NO_SETTINGS

#ifndef _APP_NO_UPDATES
UINT WINAPI rapp::CheckForUpdatesProc (LPVOID lparam)
{
	rapp* this_ptr = static_cast<rapp*>(lparam);

	this_ptr->update_lock = TRUE;

	if (this_ptr)
	{
		BOOL result = FALSE;

		HINTERNET hconnect = nullptr;
		HINTERNET hrequest = nullptr;

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
#endif // IDM_CHECKUPDATES

		HINTERNET hsession = _r_inet_createsession (this_ptr->GetUserAgent ());

		if (hsession)
		{
			if (_r_inet_openurl (hsession, _r_fmt (L"%s/update.php?product=%s", _APP_WEBSITE_URL, this_ptr->app_name_short), &hconnect, &hrequest))
			{
				LPSTR buffera = new CHAR[1024];
				rstring bufferw;
				DWORD total_length = 0;

				while (TRUE)
				{
					if (!_r_inet_readrequest (hrequest, buffera, 1024 - 1, &total_length))
						break;

					bufferw.Append (buffera);
				}

				delete[] buffera;

				bufferw.Trim (L" \r\n");

				if (_r_str_versioncompare (this_ptr->app_version, bufferw) == -1)
				{
					if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, nullptr, I18N (this_ptr, IDS_UPDATE_YES, 0), bufferw) == IDYES)
					{
						ShellExecute (nullptr, nullptr, _r_fmt (_APP_UPDATE_URL, this_ptr->app_name_short), nullptr, nullptr, SW_SHOWDEFAULT);
					}

					result = TRUE;
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", _r_unixtime_now ());
			}

			_r_inet_close (hsession);
		}

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // IDM_CHECKUPDATES

		if (!result && !this_ptr->is_update_forced)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, nullptr, I18N (this_ptr, IDS_UPDATE_NO, 0));
		}

		if (hconnect)
			_r_inet_close (hconnect);

		if (hrequest)
			_r_inet_close (hrequest);
	}

	this_ptr->update_lock = FALSE;

	return ERROR_SUCCESS;
}
#endif // _APP_NO_UPDATES

BOOL rapp::ParseINI (LPCWSTR path, rstring::map_two* map)
{
	BOOL result = FALSE;

	if (map && _r_fs_exists (path))
	{
		rstring section_ptr;
		rstring value_ptr;

		size_t length = 0, out_length = 0;
		size_t delimeter = 0;

		map->clear (); // clear first

		// get sections
		do
		{
			length += _R_BUFFER_LENGTH;

			out_length = GetPrivateProfileSectionNames (section_ptr.GetBuffer (length), static_cast<DWORD>(length), path);
		}
		while (out_length == (length - 1));

		section_ptr.SetLength (out_length);

		LPCWSTR section = section_ptr.GetString ();

		while (*section)
		{
			// get values
			length = 0;

			do
			{
				length += _R_BUFFER_LENGTH;

				out_length = GetPrivateProfileSection (section, value_ptr.GetBuffer (length), static_cast<DWORD>(length), path);
			}
			while (out_length == (length - 1));

			value_ptr.SetLength (out_length);

			LPCWSTR value = value_ptr.GetString ();

			while (*value)
			{
				rstring parser = value;

				delimeter = parser.Find (L'=');

				if (delimeter != rstring::npos)
				{
					(*map)[section][parser.Midded (0, delimeter)] = parser.Midded (delimeter + 1); // set
				}

				value += wcslen (value) + 1; // go next item
			}

			section += wcslen (section) + 1; // go next section
		}

		result = TRUE;
	}

	return result;
}

#ifdef _APP_HAVE_SKIPUAC
BOOL rapp::SkipUacEnable (BOOL is_enable)
{
	BOOL result = FALSE;
	BOOL action_result = FALSE;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	ITaskDefinition* task = nullptr;
	IRegistrationInfo* reginfo = nullptr;
	IPrincipal* principal = nullptr;
	ITaskSettings* settings = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;
	IRegisteredTask* registered_task = nullptr;

	rstring name;
	name.Format (_APP_TASKSCHD_NAME, app_name_short);

	if (IsVistaOrLater ())
	{
		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (is_enable)
					{
						if (SUCCEEDED (service->NewTask (0, &task)))
						{
							if (SUCCEEDED (task->get_RegistrationInfo (&reginfo)))
							{
								reginfo->put_Author (_APP_AUTHOR);
								reginfo->Release ();
							}

							if (SUCCEEDED (task->get_Principal (&principal)))
							{
								principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
								principal->Release ();
							}

							if (SUCCEEDED (task->get_Settings (&settings)))
							{
								settings->put_AllowHardTerminate (VARIANT_BOOL (FALSE));
								settings->put_StartWhenAvailable (VARIANT_BOOL (FALSE));
								settings->put_DisallowStartIfOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_StopIfGoingOnBatteries (VARIANT_BOOL (FALSE));
								settings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
								settings->put_ExecutionTimeLimit (L"PT0S");

								settings->Release ();
							}

							if (SUCCEEDED (task->get_Actions (&action_collection)))
							{
								if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
								{
									if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
									{
										WCHAR path[MAX_PATH] = {0};
										StringCchCopy (path, _countof (path), GetBinaryPath ());

										if (SUCCEEDED (exec_action->put_Path (path)) && SUCCEEDED (exec_action->put_Arguments (L"$(Arg0)")))
										{
											action_result = TRUE;
										}

										exec_action->Release ();
									}

									action->Release ();
								}

								action_collection->Release ();
							}

							if (action_result && SUCCEEDED (folder->RegisterTaskDefinition (name.GetBuffer (), task, TASK_CREATE_OR_UPDATE, _variant_t (), _variant_t (), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t (), &registered_task)))
							{
								result = TRUE;
								registered_task->Release ();
							}

							task->Release ();
						}
					}
					else
					{
						result = (folder->DeleteTask (name.GetBuffer (), 0) == S_OK);
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();

		ConfigSet (L"SkipUacIsEnabled", is_enable);
	}

	return result;
}

BOOL rapp::SkipUacIsEnabled ()
{
	if (IsVistaOrLater ())
	{
		return ConfigGet (L"SkipUacIsEnabled", FALSE).AsBool ();
	}

	return FALSE;
}

BOOL rapp::SkipUacRun ()
{
	BOOL result = FALSE;

	rstring name;
	name.Format (_APP_TASKSCHD_NAME, app_name_short);

	if (IsVistaOrLater ())
	{
		ITaskService* service = nullptr;
		ITaskFolder* folder = nullptr;
		IRegisteredTask* registered_task = nullptr;

		ITaskDefinition* task = nullptr;
		IActionCollection* action_collection = nullptr;
		IAction* action = nullptr;
		IExecAction* exec_action = nullptr;

		IRunningTask* running_task = nullptr;

		CoInitializeEx (nullptr, COINIT_MULTITHREADED);
		CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

		if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
		{
			if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
			{
				if (SUCCEEDED (service->GetFolder (L"\\", &folder)))
				{
					if (SUCCEEDED (folder->GetTask (name.GetBuffer (), &registered_task)))
					{
						if (SUCCEEDED (registered_task->get_Definition (&task)))
						{
							if (SUCCEEDED (task->get_Actions (&action_collection)))
							{
								if (SUCCEEDED (action_collection->get_Item (1, &action)))
								{
									if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
									{
										BSTR path = nullptr;

										exec_action->get_Path (&path);

										PathUnquoteSpaces (path);

										// check path is to current module
										if (_wcsicmp (path, GetBinaryPath ()) == 0)
										{
											rstring args;

											// get arguments
											{
												INT numargs = 0;
												LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

												for (INT i = 1; i < numargs; i++)
												{
													args.Append (arga[i]);
													args.Append (L" ");
												}

												LocalFree (arga);

												args.Trim (L" ");
											}

											variant_t ticker = args;

											if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_AS_SELF, 0, nullptr, &running_task)) && running_task)
											{
												TASK_STATE state;
												INT count = 5; // try count

												do
												{
													_r_sleep (500);

													running_task->Refresh ();
													running_task->get_State (&state);

													if (state == TASK_STATE_RUNNING || state == TASK_STATE_DISABLED)
													{
														if (state == TASK_STATE_RUNNING)
															result = TRUE;

														break;
													}
												}
												while (count--);

												running_task->Release ();
											}
										}

										exec_action->Release ();
									}

									action->Release ();
								}

								action_collection->Release ();
							}

							task->Release ();
						}

						registered_task->Release ();
					}

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}
#endif // _APP_HAVE_SKIPUAC

BOOL rapp::RunAsAdmin ()
{
	BOOL result = FALSE;

	if (_r_sys_uacstate ())
	{
#ifdef _APP_HAVE_SKIPUAC
		result = SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC

		if (!result)
		{
			BOOL is_mutexdestroyed = UninitializeMutex ();

			SHELLEXECUTEINFO shex = {0};

			shex.cbSize = sizeof (shex);
			shex.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI;
			shex.lpVerb = L"runas";
			shex.nShow = SW_NORMAL;
			shex.lpFile = GetBinaryPath ();

			if (ShellExecuteEx (&shex))
			{
				result = TRUE;
			}
			else
			{
				if (is_mutexdestroyed)
					InitializeMutex ();
			}
		}
	}

	return result;
}
