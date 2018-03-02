// routine++
// Copyright (c) 2012-2018 Henry++

#include "rapp.hpp"

#ifdef _APP_HAVE_TRAY
CONST UINT WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

rapp::rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// store system information
	is_vistaorlater = _r_sys_validversion (6, 0);
	is_admin = _r_sys_adminstate ();

	// get hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// general information
	StringCchCopy (app_name, _countof (app_name), name);
	StringCchCopy (app_name_short, _countof (app_name_short), short_name);
	StringCchCopy (app_version, _countof (app_version), version);
	StringCchCopy (app_copyright, _countof (app_copyright), copyright);

#if defined(_APP_BETA) || defined(_APP_BETA_RC)
#ifdef _APP_BETA_RC
	StringCchCat (app_version, _countof (app_version), L" RC");
#else
	StringCchCat (app_version, _countof (app_version), L" Beta");
#endif // _APP_BETA_RC
#endif // _APP_BETA

	// get dpi scale
	{
		const HDC hdc = GetDC (nullptr);
		dpi_percent = DOUBLE (GetDeviceCaps (hdc, LOGPIXELSX)) / 96.0f;
		ReleaseDC (nullptr, hdc);
	}

	// get paths
	GetModuleFileName (GetHINSTANCE (), app_binary, _countof (app_binary));
	StringCchCopy (app_directory, _countof (app_directory), app_binary);
	PathRemoveFileSpec (app_directory);

	// useragent
	StringCchCopy (app_useragent, _countof (app_useragent), ConfigGet (L"UserAgent", _r_fmt (L"%s/%s (+%s)", app_name, app_version, _APP_WEBSITE_URL)));

	// parse command line
	INT numargs = 0;
	LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

	if (arga && numargs > 1)
	{
		for (INT i = 1; i < numargs; i++)
		{
			if (_wcsnicmp (arga[i], L"/ini", 3) == 0 && (i + 1) < numargs)
			{
				LPWSTR ptr = arga[i + 1];

				PathUnquoteSpaces (ptr);

				StringCchCopy (app_config_path, _countof (app_config_path), _r_path_expand (ptr));

				if (PathGetDriveNumber (app_config_path) == -1)
					StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s", GetDirectory (), _r_path_expand (ptr).GetString ());

				if (!_r_fs_exists (app_config_path))
				{
					HANDLE hfile = CreateFile (app_config_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

					if (hfile != INVALID_HANDLE_VALUE)
						CloseHandle (hfile);
				}
			}
		}
	}

	// get configuration path
	if (!app_config_path[0] || !_r_fs_exists (app_config_path))
		StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", GetDirectory (), app_name_short);

	if (!_r_fs_exists (app_config_path) && !_r_fs_exists (_r_fmt (L"%s\\portable.dat", GetDirectory ())))
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), _r_path_expand (L"%APPDATA%\\" _APP_AUTHOR L"\\"));
		StringCchCat (app_profile_directory, _countof (app_profile_directory), app_name);
		StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_profile_directory, app_name_short);
	}
	else
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), _r_path_extractdir (app_config_path));
	}

	// get default system locale
	GetLocaleInfo (LOCALE_SYSTEM_DEFAULT, _r_sys_validversion (6, 1) ? LOCALE_SENGLISHLANGUAGENAME : LOCALE_SENGLANGUAGE, locale_default, _countof (locale_default));

	// read config
	ConfigInit ();
}

rapp::~rapp ()
{
	if (app_callback)
		app_callback (GetHWND (), _RM_UNINITIALIZE, nullptr, nullptr);

	UninitializeMutex ();
}

bool rapp::InitializeMutex ()
{
	UninitializeMutex ();

	app_mutex = CreateMutex (nullptr, FALSE, app_name_short);

	return (app_mutex != nullptr);
}

bool rapp::UninitializeMutex ()
{
	if (app_mutex)
	{
		CloseHandle (app_mutex);
		app_mutex = nullptr;

		return true;
	}

	return false;
}

bool rapp::CheckMutex (bool activate_window)
{
	bool result = false;

	HANDLE h = CreateMutex (nullptr, FALSE, app_name_short);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		result = true;

		if (activate_window && !wcsstr (GetCommandLine (), L"/minimized"))
			EnumWindows (&ActivateWindowCallback, (LPARAM)this);
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
		WCHAR fname[MAX_PATH] = {0};

		if (_r_process_getpath (h, fname, _countof (fname)))
		{
			if (ptr && _wcsnicmp (_r_path_extractfile (fname), ptr->app_name_short, wcslen (ptr->app_name_short)) == 0)
			{
				_r_wnd_toggle (hwnd, true);

				CloseHandle (h);

				return FALSE;
			}
		}

		CloseHandle (h);
	}

	return TRUE;
}

#ifdef _APP_HAVE_AUTORUN
bool rapp::AutorunEnable (bool is_enable)
{
	bool result = false;

	const HKEY hroot = HKEY_CURRENT_USER;
	HKEY hkey = nullptr;

	if (RegOpenKeyEx (hroot, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_READ, &hkey) == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			WCHAR buffer[MAX_PATH] = {0};
			StringCchPrintf (buffer, _countof (buffer), L"\"%s\" /minimized", GetBinaryPath ());

			result = (RegSetValueEx (hkey, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) * sizeof (WCHAR)) + sizeof (WCHAR))) == ERROR_SUCCESS);

			if (result)
				ConfigSet (L"AutorunIsEnabled", true);
		}
		else
		{
			RegDeleteValue (hkey, app_name);
			ConfigSet (L"AutorunIsEnabled", false);

			result = true;
		}

		RegCloseKey (hkey);
	}

	return result;
}

bool rapp::AutorunIsEnabled ()
{
	return ConfigGet (L"AutorunIsEnabled", false).AsBool ();
}
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_UPDATES
void rapp::CheckForUpdates (bool is_periodical)
{
	if (update_lock)
		return;

	if (is_periodical)
	{
		if (!ConfigGet (L"CheckUpdates", true).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0).AsLonglong ()) <= _APP_UPDATE_PERIOD)
			return;
	}

	is_update_forced = is_periodical;

	_beginthreadex (nullptr, 0, &CheckForUpdatesProc, (LPVOID)this, 0, nullptr);

	update_lock = true;
}
#endif // _APP_HAVE_UPDATES

void rapp::ConfigInit ()
{
	app_config_array.clear (); // reset
	ParseINI (app_config_path, &app_config_array, nullptr);

	LocaleInit ();

	// check for updates
#ifdef _APP_HAVE_UPDATES
	CheckForUpdates (true);
#endif // _APP_HAVE_UPDATES
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name)
{
	const rstring val = ConfigGet (key, _r_fmt (L"%d", def), name);

	return val;
}

rstring rapp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name)
{
	rstring result;

	if (!name)
		name = app_name_short;

	// check key is exists
	if (app_config_array.find (name) != app_config_array.end () && app_config_array.at (name).find (key) != app_config_array.at (name).end ())
		result = app_config_array.at (name).at (key);

	if (result.IsEmpty ())
		result = def;

	return result;
}

bool rapp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_fs_exists (app_profile_directory))
		_r_fs_mkdir (app_profile_directory);

	if (!name)
		name = app_name_short;

	// update hash value
	app_config_array[name][key] = val;

	if (WritePrivateProfileString (name, key, val, app_config_path))
		return true;

	return false;
}

bool rapp::ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%lld", val), name);
}

bool rapp::ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%d", val), name);
}

bool rapp::ConfigSet (LPCWSTR key, bool val, LPCWSTR name)
{
	return ConfigSet (key, val ? L"true" : L"false", name);
}

bool rapp::ConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg)
{
	if (!ConfigGet (config_cfg, true).AsBool ())
		return true;

	BOOL is_flagchecked = FALSE;

	INT result = 0;

#ifndef _APP_NO_WINXP
	if (_r_sys_validversion (6, 0))
	{
#endif // _APP_NO_WINXP

#ifdef IDS_QUESTION_FLAG_CHK
		const rstring flag_text = LocaleString (IDS_QUESTION_FLAG_CHK, nullptr);
#else
		const rstring flag_text = L"Do not ask again";
#endif // IDS_QUESTION_FLAG_CHK

		TASKDIALOGCONFIG tdc = {0};

		WCHAR str_title[64] = {0};
		WCHAR str_main[256] = {0};
		WCHAR str_content[512] = {0};
		WCHAR str_flag[64] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetModuleHandle (nullptr);
		tdc.pfCallback = &_r_msg_callback;
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
		tdc.pszWindowTitle = str_title;
		tdc.pszMainInstruction = str_main;
		tdc.pszContent = str_content;
		tdc.pszVerificationText = str_flag;
		tdc.lpCallbackData = 1; // always on top

		StringCchCopy (str_title, _countof (str_title), app_name);

		if (main)
			StringCchCopy (str_main, _countof (str_main), main);

		if (text)
			StringCchCopy (str_content, _countof (str_content), text);

		StringCchCopy (str_flag, _countof (str_flag), flag_text);

		_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked);
#ifndef _APP_NO_WINXP
	}
	else
	{
		rstring cfg_string;
		cfg_string.Format (L"%s\\%s", app_name_short, config_cfg);

		result = SHMessageBoxCheck (hwnd, text, app_name, MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST, IDOK, cfg_string);

		// get checkbox value fron registry
		{
			HKEY hkey = nullptr;

			if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\DontShowMeThisDialogAgain", 0, KEY_WRITE | KEY_READ, &hkey) == ERROR_SUCCESS)
			{
				if (result == IDOK)
					is_flagchecked = (RegQueryValueEx (hkey, cfg_string, 0, nullptr, nullptr, nullptr) == ERROR_SUCCESS);

				RegDeleteValue (hkey, cfg_string);

				RegCloseKey (hkey);
			}
		}
	}
#endif // _APP_NO_WINXP

	if (result <= 0)
	{
		_r_msg (hwnd, MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST, app_name, main, L"%s", text);
	}
	else if (result == IDOK)
	{
		if (is_flagchecked)
			ConfigSet (config_cfg, false);

		return true;
	}

	return false;
}

#ifndef _APP_NO_ABOUT
void rapp::CreateAboutWindow (HWND hwnd, LPCWSTR donate_text)
{
	if (!is_about_opened)
	{
		is_about_opened = true;

#ifdef _WIN64
		static const unsigned architecture = 64;
#else
		static const unsigned architecture = 32;
#endif // _WIN64

#ifndef _APP_NO_WINXP
		if (IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP

			INT result = 0;

			WCHAR title[64] = {0};
			WCHAR main[64] = {0};
			WCHAR content[512] = {0};
			WCHAR footer[256] = {0};
			WCHAR btn_text[64] = {0};

			TASKDIALOGCONFIG tdc = {0};
			TASKDIALOG_BUTTON buttons[1] = {0};

			tdc.cbSize = sizeof (tdc);
			tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
			tdc.hwndParent = hwnd;
			tdc.hInstance = GetHINSTANCE ();
			tdc.pfCallback = &_r_msg_callback;
			tdc.pszMainIcon = MAKEINTRESOURCE (100);
			tdc.pszFooterIcon = TD_INFORMATION_ICON;
			tdc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
			tdc.nDefaultButton = IDCLOSE;
			tdc.pszWindowTitle = title;
			tdc.pszMainInstruction = main;
			tdc.pszContent = content;
			tdc.pszFooter = footer;
			tdc.lpCallbackData = 1; // always on top

			if (donate_text)
			{
				tdc.pButtons = buttons;
				tdc.cButtons = _countof (buttons);

				buttons[0].nButtonID = 100;
				buttons[0].pszButtonText = btn_text;

				StringCchCopy (btn_text, _countof (btn_text), donate_text);
			}

			StringCchCopy (title, _countof (title), LocaleString (IDS_ABOUT, nullptr));
			StringCchCopy (main, _countof (main), app_name);
			StringCchPrintf (content, _countof (content), L"Version %s, %u-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", app_version, architecture, app_copyright, _APP_WEBSITE_URL, _APP_WEBSITE_URL + rstring (_APP_WEBSITE_URL).Find (L':') + 3, _APP_GITHUB_URL, _APP_GITHUB_URL + rstring (_APP_GITHUB_URL).Find (L':') + 3);
			StringCchCopy (footer, _countof (footer), L"This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License 3 as published by the Free Software Foundation.");

			if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
			{
				if (result == buttons[0].nButtonID)
					ShellExecute (GetHWND (), nullptr, _r_fmt (_APP_DONATE_URL, app_name_short), nullptr, nullptr, SW_SHOWDEFAULT);
			}
#ifndef _APP_NO_WINXP
		}
		else
		{
			_r_msg (hwnd, MB_OK | MB_USERICON | MB_TOPMOST, LocaleString (IDS_ABOUT, nullptr), app_name, L"Version %s, %u-bit (Unicode)\r\n%s\r\n\r\n%s | %s", app_version, architecture, app_copyright, _APP_WEBSITE_URL + rstring (_APP_WEBSITE_URL).Find (L':') + 3, _APP_GITHUB_URL + rstring (_APP_GITHUB_URL).Find (L':') + 3);
		}
#endif // _APP_NO_WINXP

		is_about_opened = false;
	}
}
#endif // _APP_NO_ABOUT

bool rapp::IsAdmin () const
{
	return is_admin;
}

bool rapp::IsClassicUI () const
{
	return is_classic;
}

bool rapp::IsVistaOrLater () const
{
	return is_vistaorlater;
}

void rapp::SetIcon (HWND hwnd, UINT icon_id, bool is_forced)
{
	if (is_forced || (!app_icon_small || !app_icon_big))
	{
		if (app_icon_small)
		{
			DestroyIcon (app_icon_small);
			app_icon_small = nullptr;
		}

		if (app_icon_big)
		{
			DestroyIcon (app_icon_big);
			app_icon_big = nullptr;
		}

		app_icon_small = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXSMICON));
		app_icon_big = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXICON));
	}

	PostMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app_icon_small);
	PostMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app_icon_big);
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
			this_ptr->is_classic = !IsThemeActive () || this_ptr->ConfigGet (L"ClassicUI", false).AsBool ();

			if (this_ptr->app_callback)
				this_ptr->app_callback (this_ptr->GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

			break;
		}

		case WM_SHOWWINDOW:
		{
			if (this_ptr->is_needmaximize)
			{
				ShowWindow (hwnd, SW_MAXIMIZE);
				this_ptr->is_needmaximize = false;
			}

			break;
		}

		case WM_DESTROY:
		{
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
				this_ptr->ConfigSet (L"IsWindowZoomed", IsZoomed (hwnd) ? true : false);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lparam;

			lpmmi->ptMinTrackSize.x = this_ptr->max_width;
			lpmmi->ptMinTrackSize.y = this_ptr->max_height;

			break;
		}

		case WM_EXITSIZEMOVE:
		{
			RECT rc = {0};
			GetWindowRect (hwnd, &rc);

			this_ptr->ConfigSet (L"WindowPosX", (DWORD)rc.left);
			this_ptr->ConfigSet (L"WindowPosY", (DWORD)rc.top);

			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				this_ptr->ConfigSet (L"WindowPosWidth", (DWORD)_R_RECT_WIDTH (&rc));
				this_ptr->ConfigSet (L"WindowPosHeight", (DWORD)_R_RECT_HEIGHT (&rc));
			}

			break;
		}

		case WM_SIZE:
		{
#ifdef _APP_HAVE_TRAY
			if (wparam == SIZE_MINIMIZED)
			{
				_r_wnd_toggle (hwnd, false);
				return TRUE;
			}
#endif // _APP_HAVE_TRAY

			break;
		}

		case WM_SYSCOMMAND:
		{
#ifdef _APP_HAVE_TRAY
			if (wparam == SC_CLOSE)
			{
				_r_wnd_toggle (hwnd, false);
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

bool rapp::CreateMainWindow (UINT dlg_id, UINT icon_id, DLGPROC proc, APPLICATION_CALLBACK callback)
{
	bool result = false;

	// check checksum
	{
		const HINSTANCE hlib = LoadLibrary (L"imagehlp.dll");

		if (hlib)
		{
			const MFACS _MapFileAndCheckSumW = (MFACS)GetProcAddress (hlib, "MapFileAndCheckSumW");

			if (_MapFileAndCheckSumW)
			{
				DWORD dwFileChecksum = 0, dwRealChecksum = 0;
				_MapFileAndCheckSumW (GetBinaryPath (), &dwFileChecksum, &dwRealChecksum);

				if (dwRealChecksum != dwFileChecksum)
					return false;
			}

			FreeLibrary (hlib);
		}
	}

	// initialize controls
	{
		INITCOMMONCONTROLSEX icex = {0};

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

		InitCommonControlsEx (&icex);
	}

	// check arguments
	{
		INT numargs = 0;
		LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

		if (arga)
		{
			if (callback && numargs > 1)
			{
				if (callback (nullptr, _RM_ARGUMENTS, nullptr, nullptr))
					return false;
			}

			LocalFree (arga);
		}
	}

	if (CheckMutex (true))
		return false;

#if defined(_APP_HAVE_SKIPUAC) || defined(_APP_NO_GUEST)
	if (RunAsAdmin ())
		return false;

#ifdef _APP_NO_GUEST
	if (!IsAdmin ())
	{
		_r_msg (nullptr, MB_OK | MB_ICONWARNING, app_name, L"Warning!", L"%s required administrative privileges!", app_name);
		return false;
	}
#endif // _APP_NO_GUEST

#endif // _APP_HAVE_SKIPUAC || _APP_NO_GUEST

#ifndef _WIN64
	if (_r_sys_iswow64 ())
	{
		if (!ConfirmMessage (nullptr, L"Warning!", _r_fmt (L"You are attempting to run the 32-bit version of %s on 64-bit Windows.\r\nPlease run the 64-bit version of %s instead.", app_name, app_name), L"ConfirmWOW64"))
			return false;
	}
#endif // _WIN64

	InitializeMutex ();

	if (ConfigGet (L"ClassicUI", false).AsBool () || !IsThemeActive ())
	{
		is_classic = true;

		if (ConfigGet (L"ClassicUI", false).AsBool ())
			SetThemeAppProperties (1);
	}

	// create window
	if (dlg_id && proc)
	{
		app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (dlg_id), nullptr, proc);

		if (app_hwnd)
		{
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

			// update autorun settings
#ifdef _APP_HAVE_AUTORUN
			if (AutorunIsEnabled ())
				AutorunEnable (true);
#endif // _APP_HAVE_AUTORUN

			// update uac settings
#ifdef _APP_HAVE_SKIPUAC
			if (SkipUacIsEnabled ())
				SkipUacEnable (true);
#endif // _APP_HAVE_SKIPUAC

			// set window on top
			_r_wnd_top (GetHWND (), ConfigGet (L"AlwaysOnTop", false).AsBool ());

			// restore window position
			{
				RECT rect_original = {0};

				// set minmax info
				if (GetWindowRect (GetHWND (), &rect_original))
				{
					max_width = _R_RECT_WIDTH (&rect_original);
					max_height = _R_RECT_HEIGHT (&rect_original);
				}

				// send resize message
				if ((GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_SIZEBOX) != 0)
				{
					RECT rc_client = {0};

					if (GetClientRect (GetHWND (), &rc_client))
						SendMessage (GetHWND (), WM_SIZE, 0, MAKELPARAM (_R_RECT_WIDTH (&rc_client), _R_RECT_HEIGHT (&rc_client)));
				}

				// restore window position
				RECT rect_new = {0};

				rect_new.left = ConfigGet (L"WindowPosX", rect_original.left).AsInt ();
				rect_new.top = ConfigGet (L"WindowPosY", rect_original.top).AsInt ();

				if ((GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_SIZEBOX) != 0)
				{
					rect_new.right = max (ConfigGet (L"WindowPosWidth", 0).AsInt (), max_width) + rect_new.left;
					rect_new.bottom = max (ConfigGet (L"WindowPosHeight", 0).AsInt (), max_height) + rect_new.top;
				}
				else
				{
					rect_new.right = _R_RECT_WIDTH (&rect_original) + rect_new.left;
					rect_new.bottom = _R_RECT_HEIGHT (&rect_original) + rect_new.top;
				}

				_r_wnd_adjustwindowrect (nullptr, &rect_new);

				DWORD flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_FRAMECHANGED;

				SetWindowPos (GetHWND (), nullptr, rect_new.left, rect_new.top, _R_RECT_WIDTH (&rect_new), _R_RECT_HEIGHT (&rect_new), flags);
			}

			// show window (or not?)
			{
				INT show_code = SW_SHOW;

				// show window minimized
#ifdef _APP_HAVE_TRAY
#ifdef _APP_STARTMINIMIZED
				show_code = SW_HIDE;
#else
				// if window have tray - check arguments
				if (wcsstr (GetCommandLine (), L"/minimized") || ConfigGet (L"IsStartMinimized", false).AsBool ())
					show_code = SW_HIDE;
#endif // _APP_STARTMINIMIZED
#endif // _APP_HAVE_TRAY

				if (ConfigGet (L"IsWindowZoomed", false).AsBool () && (GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
				{
					if (show_code == SW_HIDE)
						is_needmaximize = true;

					else
						show_code = SW_SHOWMAXIMIZED;
				}

				ShowWindow (GetHWND (), show_code);
			}

			// set icons
			if (icon_id)
				SetIcon (GetHWND (), icon_id, true);

			// initialization callback
			if (callback)
			{
				app_callback = callback;

				app_callback (GetHWND (), _RM_INITIALIZE, nullptr, nullptr);
				app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

				DrawMenuBar (GetHWND ()); // redraw menu
			}

			result = true;
		}
	}

	return result;
}

#ifdef _APP_HAVE_TRAY
bool rapp::TrayCreate (HWND hwnd, UINT uid, UINT code, HICON hicon, bool is_hidden)
{
	bool result = false;

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uVersion = IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_SHOWTIP | NIF_TIP;
	nid.uCallbackMessage = code;
	nid.hIcon = hicon;
	StringCchCopy (nid.szTip, _countof (nid.szTip), app_name);

	if (is_hidden)
	{
		nid.uFlags |= NIF_STATE;
		nid.dwState = NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
	}

	if (Shell_NotifyIcon (NIM_ADD, &nid))
	{
		Shell_NotifyIcon (NIM_SETVERSION, &nid);
		result = true;
	}

	return result;
}

bool rapp::TrayDestroy (UINT uid)
{
	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uID = uid;

	if (nid.hIcon)
	{
		DestroyIcon (nid.hIcon);
		nid.hIcon = nullptr;
	}

	if (Shell_NotifyIcon (NIM_DELETE, &nid))
		return true;

	return false;
}

bool rapp::TrayPopup (UINT uid, DWORD icon_id, LPCWSTR title, LPCWSTR text)
{
	bool result = false;

	nid.uFlags = NIF_INFO | NIF_REALTIME;
	nid.dwInfoFlags = NIIF_LARGE_ICON | icon_id;
	nid.uID = uid;

	if (icon_id == NIIF_USER && IsVistaOrLater ())
	{
		nid.hBalloonIcon = GetHICON (true);
	}
#ifndef _APP_NO_WINXP
	else
	{
		nid.dwInfoFlags = NIIF_INFO;
	}
#endif // _APP_NO_WINXP

	// tooltip-visibility fix
	if (nid.szTip[0])
		nid.uFlags |= (NIF_SHOWTIP | NIF_TIP);

	if (title)
		StringCchCopy (nid.szInfoTitle, _countof (nid.szInfoTitle), title);

	if (text)
		StringCchCopy (nid.szInfo, _countof (nid.szInfo), text);

	if (Shell_NotifyIcon (NIM_MODIFY, &nid))
		result = true;

	nid.szInfo[0] = nid.szInfoTitle[0] = 0; // clear

	return result;
}

bool rapp::TraySetInfo (UINT uid, HICON hicon, LPCWSTR tooltip)
{
	nid.uFlags = 0;
	nid.uID = uid;

	if (hicon)
	{
		if (nid.hIcon)
		{
			DestroyIcon (nid.hIcon);
			nid.hIcon = nullptr;
		}

		nid.uFlags |= NIF_ICON;
		nid.hIcon = hicon;
	}

	if (tooltip)
	{
		nid.uFlags |= (NIF_SHOWTIP | NIF_TIP);
		StringCchCopy (nid.szTip, _countof (nid.szTip), tooltip);
	}

	if (Shell_NotifyIcon (NIM_MODIFY, &nid))
		return true;

	return false;
}

bool rapp::TrayToggle (UINT uid, bool is_show)
{
	nid.uID = uid;
	nid.uFlags = NIF_STATE;

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	if (Shell_NotifyIcon (NIM_MODIFY, &nid))
		return true;

	return false;
}
#endif // _APP_HAVE_TRAY

#ifdef _APP_HAVE_SETTINGS
void rapp::CreateSettingsWindow (size_t dlg_id)
{
	static bool is_opened = false;

	if (!is_opened)
	{
		is_opened = true;

		if (dlg_id != LAST_VALUE)
			ConfigSet (L"SettingsLastPage", (DWORD)dlg_id);

#ifdef IDD_SETTINGS
		DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), GetHWND (), &SettingsWndProc, (LPARAM)this);
#endif // IDD_SETTINGS
	}

	is_opened = false;
}

size_t rapp::AddSettingsPage (UINT dlg_id, UINT locale_id, APPLICATION_CALLBACK callback, size_t group_id)
{
	PAPP_SETTINGS_PAGE ptr_page = new APP_SETTINGS_PAGE;

	if (ptr_page)
	{
		ptr_page->hwnd = nullptr;

		ptr_page->dlg_id = dlg_id;
		ptr_page->group_id = group_id;
		ptr_page->locale_id = locale_id;

		app_settings_pages.push_back (ptr_page);

		if (callback)
			app_settings_callback = callback;

		return app_settings_pages.size () - 1;
	}

	return LAST_VALUE;
}

HWND rapp::SettingsGetWindow ()
{
	return settings_hwnd;
}

void rapp::SettingsInitialize ()
{
	const HWND hwnd = SettingsGetWindow ();

	if (!hwnd)
		return;

	// localize window
	SetWindowText (hwnd, LocaleString (IDS_SETTINGS, nullptr));

	SetDlgItemText (hwnd, IDC_CLOSE, LocaleString (IDS_CLOSE, nullptr));

	// apply classic ui for buttons
	_r_wnd_addstyle (hwnd, IDC_CLOSE, IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

	// initialize treeview
	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		PAPP_SETTINGS_PAGE const ptr_page = app_settings_pages.at (i);

		TVITEMEX tvi = {0};

		tvi.mask = TVIF_PARAM;
		tvi.hItem = ptr_page->item;

		SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETITEM, 0, (LPARAM)&tvi);

		WCHAR buffer[128] = {0};
		StringCchCopy (buffer, _countof (buffer), LocaleString (ptr_page->locale_id, nullptr));

		tvi.mask = TVIF_TEXT;
		tvi.pszText = buffer;

		SendDlgItemMessage (hwnd, IDC_NAV, TVM_SETITEM, 0, (LPARAM)&tvi);
	}
}

void rapp::SettingsPageInitialize (UINT dlg_id, bool is_initialize, bool is_localize)
{
	if (!app_settings_callback || (!is_initialize && !is_localize))
		return;

	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		APP_SETTINGS_PAGE *ppage = app_settings_pages.at (i);

		if (ppage && ppage->dlg_id == dlg_id)
		{
			if (is_initialize)
				app_settings_callback (ppage->hwnd, _RM_INITIALIZE, nullptr, ppage);

			if (is_localize)
				app_settings_callback (ppage->hwnd, _RM_LOCALIZE, nullptr, ppage);
		}
	}
}
#endif // _APP_HAVE_SETTINGS

LPCWSTR rapp::GetBinaryPath () const
{
	return app_binary;
}

LPCWSTR rapp::GetDirectory () const
{
	return app_directory;
}

LPCWSTR rapp::GetProfileDirectory () const
{
	return app_profile_directory;
}

rstring rapp::GetUserAgent () const
{
	return app_useragent;
}

INT rapp::GetDPI (INT v) const
{
	return (INT)ceil ((DOUBLE)(v)* dpi_percent);
}

HICON rapp::GetHICON (bool is_big) const
{
	if (is_big)
		return app_icon_big;

	return app_icon_small;
}

HINSTANCE rapp::GetHINSTANCE () const
{
	return app_hinstance;
}

HWND rapp::GetHWND () const
{
	return app_hwnd;
}

#ifdef _APP_HAVE_SETTINGS
void rapp::LocaleApplyFromControl (HWND hwnd, UINT ctrl_id)
{
	const rstring text = _r_ctrl_gettext (hwnd, ctrl_id);

	if (text.CompareNoCase (_APP_LANGUAGE_DEFAULT) == 0)
		locale_current[0] = 0;

	else
		StringCchCopy (locale_current, _countof (locale_current), text);

	ConfigSet (L"Language", text);

	if (app_settings_callback && SettingsGetWindow ())
	{
		SettingsInitialize ();

		if (settings_page_id != LAST_VALUE)
			app_settings_callback (app_settings_pages.at (settings_page_id)->hwnd, _RM_LOCALIZE, nullptr, app_settings_pages.at (settings_page_id));
	}

	if (app_callback)
	{
		app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);
		DrawMenuBar (GetHWND ()); // redraw menu
	}
}
#endif // _APP_HAVE_SETTINGS

void rapp::LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id)
{
	if (selected_id == default_id)
	{
		ConfigSet (L"Language", _APP_LANGUAGE_DEFAULT);
		locale_current[0] = 0;
	}
	else
	{
		WCHAR name[LOCALE_NAME_MAX_LENGTH] = {0};
		GetMenuString (hmenu, selected_id, name, _countof (name), MF_BYCOMMAND);

		ConfigSet (L"Language", name);
		StringCchCopy (locale_current, _countof (locale_current), name);
	}

	if (app_callback)
	{
		app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);

		DrawMenuBar (GetHWND ()); // redraw menu
	}
}

void rapp::LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, const UINT id_start)
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

	if (!app_locale_array.empty ())
	{
		size_t idx = 1;

		if (is_menu)
			AppendMenu (hmenu, MF_SEPARATOR, 0, nullptr);

		for (size_t i = 0; i < app_locale_names.size (); i++)
		{
			LPCWSTR name = app_locale_names.at (i);

			if (is_menu)
			{
				AppendMenu (hmenu, MF_STRING, (idx + id_start), name);

				if (locale_current[0] && _wcsicmp (locale_current, name) == 0)
					CheckMenuRadioItem (hmenu, id_start, id_start + UINT (idx), id_start + UINT (idx), MF_BYCOMMAND);
			}
			else
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, idx, (LPARAM)name);

				if (locale_current[0] && _wcsicmp (locale_current, name) == 0)
					SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, idx, 0);
			}

			idx += 1;
		}
	}
	else
	{
		if (is_menu)
			EnableMenuItem ((HMENU)hwnd, ctrl_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);

		else
			EnableWindow (GetDlgItem (hwnd, ctrl_id), false);
	}
}

size_t rapp::LocaleGetCount ()
{
	return app_locale_array.size ();
}

void rapp::LocaleInit ()
{
	rstring name = ConfigGet (L"Language", nullptr);

	if (name.IsEmpty ())
		name = locale_default;

	else if (name.CompareNoCase (_APP_LANGUAGE_DEFAULT) == 0)
		name = L"";

	// clear
	app_locale_array.clear ();
	app_locale_names.clear ();

	ParseINI (_r_fmt (L"%s\\%s.lng", _r_path_expand (ConfigGet (L"LocalePath", GetDirectory ())).GetString (), app_name_short), &app_locale_array, &app_locale_names);

	if (!app_locale_array.empty ())
	{
		for (auto &p : app_locale_array)
		{
			for (auto &p2 : app_locale_array[p.first])
			{
				p2.second.Replace (L"\\t", L"\t");
				p2.second.Replace (L"\\r", L"\r");
				p2.second.Replace (L"\\n", L"\n");
			}
		}
	}

	if (!name.IsEmpty ())
		StringCchCopy (locale_current, _countof (locale_current), name);

	else
		locale_current[0] = 0;
}

rstring rapp::LocaleString (UINT uid, LPCWSTR append)
{
	rstring result;

	rstring key;
	key.Format (L"%03d", uid);

	if (locale_current[0])
	{
		// check key is exists
		if (app_locale_array.find (locale_current) != app_locale_array.end () && app_locale_array[locale_current].find (key) != app_locale_array[locale_current].end ())
			result = app_locale_array[locale_current][key];
	}

	if (uid && result.IsEmpty ())
	{
		LoadString (GetHINSTANCE (), uid, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH);
		result.ReleaseBuffer ();

		if (result.IsEmpty ())
			result = key;
	}

	if (append)
		result.Append (append);

	return result;
}

void rapp::LocaleMenu (HMENU menu, UINT id, UINT item, bool by_position, LPCWSTR append)
{
	WCHAR buffer[128] = {0};
	StringCchCopy (buffer, _countof (buffer), LocaleString (id, append));

	MENUITEMINFO mi = {0};

	mi.cbSize = sizeof (mi);
	mi.fMask = MIIM_STRING;
	mi.dwTypeData = buffer;

	SetMenuItemInfo (menu, item, by_position, &mi);
}

#ifdef _APP_HAVE_SETTINGS
INT_PTR CALLBACK rapp::SettingsPageProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = (rapp*)(lparam);

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		case WM_CONTEXTMENU:
		case WM_NOTIFY:
		case WM_PAINT:
		case WM_COMMAND:
		{
			if (this_ptr->settings_page_id != LAST_VALUE)
			{
				MSG wmsg = {0};

				wmsg.message = msg;
				wmsg.wParam = wparam;
				wmsg.lParam = lparam;

				PAPP_SETTINGS_PAGE const ptr_page = this_ptr->app_settings_pages.at (this_ptr->settings_page_id);

				return this_ptr->app_settings_callback (hwnd, _RM_MESSAGE, &wmsg, ptr_page);
			}

			break;
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
			this_ptr = (rapp*)(lparam);

			this_ptr->settings_hwnd = hwnd;
			this_ptr->SetIcon (hwnd, 0, false);

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, this_ptr->GetDPI (20));

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_SETBKCOLOR, TVSIL_STATE, (LPARAM)GetSysColor (COLOR_3DFACE));

			const size_t dlg_id = this_ptr->ConfigGet (L"SettingsLastPage", this_ptr->app_settings_pages.at (0)->dlg_id).AsSizeT ();

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = this_ptr->app_settings_pages.at (i);

				if (ptr_page)
				{
					if (ptr_page->dlg_id)
					{
						ptr_page->hwnd = CreateDialogParam (this_ptr->GetHINSTANCE (), MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, &this_ptr->SettingsPageProc, (LPARAM)this_ptr);
						this_ptr->app_settings_callback (ptr_page->hwnd, _RM_INITIALIZE, nullptr, ptr_page);
					}

					ptr_page->item = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (ptr_page->locale_id, nullptr), ((ptr_page->group_id == LAST_VALUE) ? nullptr : this_ptr->app_settings_pages.at (ptr_page->group_id)->item), LAST_VALUE, (LPARAM)i);

					if (dlg_id == ptr_page->dlg_id)
						SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)ptr_page->item);
				}
			}

			this_ptr->SettingsInitialize ();

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint (hwnd, &ps);

			// calculate "x" position
			RECT rc = {0};
			GetWindowRect (GetDlgItem (hwnd, IDC_NAV), &rc);

			INT pos_x = _R_RECT_WIDTH (&rc);

			// shift "x" position
			MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)&rc, 2);
			pos_x += rc.left + GetSystemMetrics (SM_CYBORDER);

			// calculate "y" position
			GetClientRect (hwnd, &rc);
			const INT pos_y = _R_RECT_HEIGHT (&rc);

			for (INT i = 0; i < pos_y; i++)
				SetPixel (dc, pos_x, i, GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_DESTROY:
		{
			// check for updates
#ifdef _APP_HAVE_UPDATES
			this_ptr->CheckForUpdates (true);
#endif // _APP_HAVE_UPDATES

			BOOL result = false;

			if (this_ptr->app_settings_callback)
				result = this_ptr->app_settings_callback (hwnd, _RM_CLOSE, nullptr, nullptr);

			if (result && this_ptr->app_callback)
				this_ptr->app_callback (this_ptr->GetHWND (), _RM_INITIALIZE, nullptr, nullptr);

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = this_ptr->app_settings_pages.at (i);

				if (ptr_page)
				{
					if (ptr_page->hwnd)
					{
						DestroyWindow (ptr_page->hwnd);
						ptr_page->hwnd = nullptr;
					}
				}
			}

			this_ptr->settings_hwnd = nullptr;
			this_ptr->settings_page_id = LAST_VALUE;

			_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", false).AsBool ());

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

						const size_t old_id = size_t (pnmtv->itemOld.lParam);
						const size_t new_id = size_t (pnmtv->itemNew.lParam);

						this_ptr->settings_page_id = new_id;
						this_ptr->ConfigSet (L"SettingsLastPage", (DWORD)this_ptr->app_settings_pages.at (new_id)->dlg_id);

						if (this_ptr->app_settings_pages.at (old_id)->hwnd)
							ShowWindow (this_ptr->app_settings_pages.at (old_id)->hwnd, SW_HIDE);

						if (this_ptr->app_settings_pages.at (new_id)->hwnd)
						{
							PAPP_SETTINGS_PAGE const ptr_page = this_ptr->app_settings_pages.at (new_id);

							if (ptr_page)
							{
								this_ptr->app_settings_callback (ptr_page->hwnd, _RM_LOCALIZE, nullptr, ptr_page);
								ShowWindow (ptr_page->hwnd, SW_SHOW);
							}
						}

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
				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
#endif // _APP_HAVE_SETTINGS

#ifdef _APP_HAVE_UPDATES
UINT WINAPI rapp::CheckForUpdatesProc (LPVOID lparam)
{
	rapp* this_ptr = (rapp*)(lparam);

	if (this_ptr)
	{
		bool result = false;

		this_ptr->update_lock = true;

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
#endif // IDM_CHECKUPDATES

		HINTERNET hsession = _r_inet_createsession (this_ptr->GetUserAgent ());

		if (hsession)
		{
			HINTERNET hconnect = nullptr;
			HINTERNET hrequest = nullptr;

#if defined(_APP_BETA) || defined(_APP_BETA_RC)
			const bool is_beta = true;
#else
			const bool is_beta = this_ptr->ConfigGet (L"CheckUpdatesForBeta", false).AsBool ();
#endif // _APP_BETA

			if (_r_inet_openurl (hsession, _r_fmt (L"%s/update.php?product=%s&is_beta=%d", _APP_WEBSITE_URL, this_ptr->app_name_short, is_beta), &hconnect, &hrequest, nullptr))
			{
				CHAR buffera[128] = {0};
				rstring bufferw;
				DWORD total_length = 0;

				while (true)
				{
					if (!_r_inet_readrequest (hrequest, buffera, _countof (buffera) - 1, nullptr, &total_length))
						break;

					bufferw.Append (buffera);
				}

				bufferw.Trim (L" \r\n");

				this_ptr->ConfigSet (L"CheckUpdatesLast", _r_unixtime_now ());

				if (!bufferw.IsEmpty () && _r_str_versioncompare (this_ptr->app_version, bufferw) == -1)
				{
					if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, nullptr, this_ptr->LocaleString (IDS_UPDATE_YES, nullptr), bufferw.GetString ()) == IDYES)
						ShellExecute (nullptr, nullptr, _r_fmt (_APP_UPDATE_URL, this_ptr->app_name_short), nullptr, nullptr, SW_SHOWDEFAULT);

					result = true;
				}
			}

			if (hrequest)
				_r_inet_close (hrequest);

			if (hconnect)
				_r_inet_close (hconnect);

			_r_inet_close (hsession);
		}

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // IDM_CHECKUPDATES

		if (!result && !this_ptr->is_update_forced)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, nullptr, this_ptr->LocaleString (IDS_UPDATE_NO, nullptr));
		}

		this_ptr->update_lock = false;
	}

	return ERROR_SUCCESS;
}
#endif // _APP_HAVE_UPDATES

bool rapp::ParseINI (LPCWSTR path, rstring::map_two* pmap, std::vector<rstring>* psections)
{
	bool result = false;

	if (pmap && _r_fs_exists (path))
	{
		rstring section_ptr;
		rstring value_ptr;

		size_t length = 0, out_length = 0;
		size_t delimeter = 0;

		// get sections
		do
		{
			length += _R_BUFFER_LENGTH;

			out_length = GetPrivateProfileSectionNames (section_ptr.GetBuffer (length), (DWORD)length, path);
		}
		while (out_length == (length - 1));

		section_ptr.SetLength (out_length);

		LPCWSTR section = section_ptr.GetString ();

		while (*section)
		{
			// get values
			length = 0;

			if (psections)
				psections->push_back (section);

			do
			{
				length += _R_BUFFER_LENGTH;

				out_length = GetPrivateProfileSection (section, value_ptr.GetBuffer (length), (DWORD)length, path);
			}
			while (out_length == (length - 1));

			value_ptr.SetLength (out_length);

			LPCWSTR value = value_ptr.GetString ();

			while (*value)
			{
				rstring parser = value;

				delimeter = parser.Find (L'=');

				if (delimeter != rstring::npos)
					(*pmap)[section][parser.Midded (0, delimeter)] = parser.Midded (delimeter + 1); // set

				value += wcslen (value) + 1; // go next item
			}

			section += wcslen (section) + 1; // go next section
		}

		result = true;
	}

	return result;
}

#ifdef _APP_HAVE_SKIPUAC
bool rapp::SkipUacEnable (bool is_enable)
{
	bool result = false;
	bool action_result = false;

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

	if (IsVistaOrLater ())
	{
		rstring name;
		name.Format (_APP_TASKSCHD_NAME, app_name_short);

		if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
		{
			if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
			{
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

												WCHAR directory[MAX_PATH] = {0};
												StringCchCopy (directory, _countof (directory), GetDirectory ());

												if (SUCCEEDED (exec_action->put_Path (path)) && SUCCEEDED (exec_action->put_WorkingDirectory (directory)) && SUCCEEDED (exec_action->put_Arguments (L"$(Arg0)")))
												{
													action_result = true;
												}

												exec_action->Release ();
											}

											action->Release ();
										}

										action_collection->Release ();
									}

									if (action_result && SUCCEEDED (folder->RegisterTaskDefinition (name.GetBuffer (), task, TASK_CREATE_OR_UPDATE, _variant_t (), _variant_t (), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t (), &registered_task)))
									{
										ConfigSet (L"SkipUacIsEnabled", true);
										result = true;

										registered_task->Release ();
									}

									task->Release ();
								}
							}
							else
							{
								result = SUCCEEDED (folder->DeleteTask (name.GetBuffer (), 0));
								ConfigSet (L"SkipUacIsEnabled", false);
							}

							folder->Release ();
						}
					}

					service->Release ();
				}
			}

			CoUninitialize ();
		}
	}

	return result;
}

bool rapp::SkipUacIsEnabled ()
{
	if (IsVistaOrLater ())
		return ConfigGet (L"SkipUacIsEnabled", false).AsBool ();

	return false;
}

bool rapp::SkipUacRun ()
{
	bool result = false;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	IRegisteredTask* registered_task = nullptr;

	ITaskDefinition* task = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;

	IRunningTask* running_task = nullptr;

	if (IsVistaOrLater ())
	{
		rstring name;
		name.Format (_APP_TASKSCHD_NAME, app_name_short);

		if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
		{
			if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
			{
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
													}

													variant_t ticker = args.Trim (L" ");

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
																	result = true;

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
			}

			CoUninitialize ();
		}
	}

	return result;
}
#endif // _APP_HAVE_SKIPUAC

bool rapp::RunAsAdmin ()
{
	bool result = false;

	if (!IsAdmin ())
	{
#ifdef _APP_HAVE_SKIPUAC
		result = SkipUacRun ();
#endif // _APP_HAVE_SKIPUAC

		if (!result)
		{
			const bool is_mutexdestroyed = UninitializeMutex ();

			if (SUCCEEDED (CoInitialize (nullptr)))
			{
				SHELLEXECUTEINFO shex = {0};

				WCHAR path[MAX_PATH] = {0};
				StringCchCopy (path, _countof (path), GetBinaryPath ());

				WCHAR directory[MAX_PATH] = {0};
				StringCchCopy (directory, _countof (directory), GetDirectory ());

				WCHAR args[MAX_PATH] = {0};
				StringCchCopy (args, _countof (args), GetCommandLine ());

				shex.cbSize = sizeof (shex);
				shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_NO_UI;
				shex.lpVerb = L"runas";
				shex.nShow = SW_NORMAL;
				shex.lpFile = path;
				shex.lpDirectory = directory;
				shex.lpParameters = args;

				if (ShellExecuteEx (&shex))
				{
					result = true;
				}
				else
				{
					if (is_mutexdestroyed)
						InitializeMutex ();
				}

				CoUninitialize ();
			}
		}
	}

	return result;
}
