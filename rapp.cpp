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

	// safe dll loading
	SetDllDirectory (L"");

	// win7+
	if (_r_sys_validversion (6, 1))
	{
		const HINSTANCE hlib = GetModuleHandle (L"kernel32.dll");

		if (hlib)
		{
			typedef BOOL (WINAPI *SSPM) (DWORD); // SetSearchPathMode

			const SSPM _SetSearchPathMode = (SSPM)GetProcAddress (hlib, "SetSearchPathMode");

			if (_SetSearchPathMode)
				_SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

		}
	}

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
					const HANDLE hfile = CreateFile (app_config_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

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

bool rapp::MutexCreate ()
{
	MutexDestroy ();

	app_mutex = CreateMutex (nullptr, FALSE, app_name_short);

	return (app_mutex != nullptr);
}

bool rapp::MutexDestroy ()
{
	if (app_mutex)
	{
		ReleaseMutex (app_mutex);
		CloseHandle (app_mutex);

		app_mutex = nullptr;

		return true;
	}

	return false;
}

bool rapp::MutexIsExists (bool activate_window)
{
	bool result = false;

	const HANDLE hmutex = CreateMutex (nullptr, FALSE, app_name_short);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		result = true;

		if (activate_window && !wcsstr (GetCommandLine (), L"/minimized"))
			EnumWindows (&ActivateWindowCallback, (LPARAM)this);
	}
	else
	{
		ReleaseMutex (hmutex);
	}

	if (hmutex)
		CloseHandle (hmutex);

	return result;
}

BOOL CALLBACK rapp::ActivateWindowCallback (HWND hwnd, LPARAM lparam)
{
	rapp const* this_ptr = (rapp*)lparam;

	if (!this_ptr)
		return FALSE;

	DWORD pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

	if (GetCurrentProcessId () == pid)
		return TRUE;

	WCHAR buffer[MAX_PATH] = {0};

	// check window title
	if (GetWindowText (hwnd, buffer, _countof (buffer)))
	{
		if (_wcsnicmp (this_ptr->app_name, buffer, wcslen (this_ptr->app_name)) != 0)
			return TRUE;
	}
	else
	{
		return TRUE;
	}

	// check window props
	if (GetProp (hwnd, L"this_ptr"))
	{
		_r_wnd_toggle (hwnd, true);
		return FALSE;
	}

	// check window filename
	if (GetWindowModuleFileName (hwnd, buffer, _countof (buffer)))
	{
		if (_wcsnicmp (this_ptr->app_name_short, _r_path_extractfile (buffer), wcslen (this_ptr->app_name_short)) == 0)
		{
			_r_wnd_toggle (hwnd, true);
			return FALSE;
		}
	}

	return TRUE;
}

#ifdef _APP_HAVE_AUTORUN
bool rapp::AutorunIsEnabled ()
{
	return ConfigGet (L"AutorunIsEnabled", false).AsBool ();
}

bool rapp::AutorunEnable (bool is_enable)
{
	bool result = false;

	HKEY hroot = HKEY_CURRENT_USER;
	rstring reg_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

	// create autorun entry for current user only
	{
		rstring reg_domain;
		rstring reg_username;

		_r_sys_getusername (&reg_domain, &reg_username);
		rstring reg_sid = _r_sys_getusernamesid (reg_domain, reg_username);

		if (!reg_sid.IsEmpty ())
		{
			hroot = HKEY_USERS;
			reg_path.Format (L"%s\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", reg_sid.GetString ());
		}
	}

	HKEY hkey = nullptr;

	if (RegOpenKeyEx (hroot, reg_path, 0, KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			WCHAR buffer[MAX_PATH] = {0};
			StringCchPrintf (buffer, _countof (buffer), L"\"%s\" /minimized", GetBinaryPath ());

			result = (RegSetValueEx (hkey, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) + 1) * sizeof (WCHAR))) == ERROR_SUCCESS);

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
#endif // _APP_HAVE_AUTORUN

bool rapp::DownloadURL (LPCWSTR url, LPVOID buffer, bool is_filepath, DOWNLOAD_CALLBACK callback, LONG_PTR lpdata)
{
	bool result = false;

	const HINTERNET hsession = _r_inet_createsession (GetUserAgent (), ConfigGet (L"Proxy", nullptr));

	if (hsession)
	{
		HINTERNET hconnect = nullptr;
		HINTERNET hrequest = nullptr;

		DWORD total_length = 0;

		if (_r_inet_openurl (hsession, url, &hconnect, &hrequest, &total_length))
		{
			const DWORD buffer_length = (_R_BUFFER_LENGTH * 4);
			LPSTR content_buffer = new CHAR[buffer_length];

			if (content_buffer)
			{
				rstring* lpbuffer = nullptr;
				HANDLE hfile = nullptr;

				if (is_filepath)
				{
					hfile = CreateFile ((LPCWSTR)buffer, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, nullptr);

					if (hfile != INVALID_HANDLE_VALUE)
						result = true;
				}
				else
				{
					lpbuffer = (rstring*)buffer;

					if (lpbuffer)
						result = true;
				}

				if (result)
				{
					DWORD notneed = 0;
					DWORD readed = 0;
					DWORD total_readed = 0;

					while (true)
					{
						if (!_r_inet_readrequest (hrequest, content_buffer, buffer_length - 1, &readed, &total_readed))
							break;

						if (is_filepath)
							WriteFile (hfile, content_buffer, readed, &notneed, nullptr);

						else
							lpbuffer->Append (content_buffer);

						if (callback)
						{
							if (!callback (total_readed, total_length, lpdata))
							{
								result = false;
								break;
							}
						}
					}

					if (is_filepath)
						CloseHandle (hfile);

					else
						lpbuffer->Trim (L"\r\n ");
				}

				delete[] content_buffer;
			}
		}

		if (hrequest)
			_r_inet_close (hrequest);

		if (hconnect)
			_r_inet_close (hconnect);

		_r_inet_close (hsession);
	}

	return result;
}

#ifdef _APP_HAVE_UPDATES
void rapp::UpdateAddComponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, bool is_installer)
{
	if (!pupdateinfo)
	{
		pupdateinfo = new APP_UPDATE_INFO;

		if (pupdateinfo)
		{
			pupdateinfo->papp = this;
			pupdateinfo->hend = CreateEvent (nullptr, FALSE, FALSE, nullptr);
		}
	}

	if (pupdateinfo)
	{
		PAPP_UPDATE_COMPONENT pcomponent = new APP_UPDATE_COMPONENT;

		if (pcomponent)
		{
			pcomponent->full_name = full_name;
			pcomponent->short_name = short_name;
			pcomponent->version = version;

			pcomponent->target_path = target_path;

			pcomponent->is_installer = is_installer;

			pupdateinfo->components.push_back (pcomponent);
		}
	}
}

bool rapp::UpdateCheck (bool is_forced)
{
	if (!is_forced && (!ConfigGet (L"CheckUpdates", true).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0).AsLonglong ()) <= _APP_UPDATE_PERIOD))
		return false;

	const HANDLE hthread = (HANDLE)_beginthreadex (nullptr, 0, &UpdateCheckThread, (LPVOID)pupdateinfo, CREATE_SUSPENDED, nullptr);

	if (hthread)
	{
		if (pupdateinfo)
		{
			pupdateinfo->is_forced = is_forced;
			pupdateinfo->hwnd = nullptr;
		}

		if (is_forced)
		{
			WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_INIT
			StringCchCopy (str_content, _countof (str_content), LocaleString (IDS_UPDATE_INIT, nullptr));
#else
			StringCchCopy (str_content, _countof (str_content), L"Checking for new releases...");
#pragma _R_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_STARTED

			pupdateinfo->hthread = hthread;

			UpdateDialogNavigate (nullptr, nullptr, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
		}
		else
		{
			ResumeThread (hthread);
		}
	}
	else
	{
		return false;
	}

	return true;
}
#endif // _APP_HAVE_UPDATES

void rapp::ConfigInit ()
{
	app_config_array.clear (); // reset
	ParseINI (GetConfigPath (), &app_config_array, nullptr);

	// useragent
	StringCchCopy (app_useragent, _countof (app_useragent), ConfigGet (L"UserAgent", _r_fmt (L"%s/%s (+%s)", app_name, app_version, _APP_WEBSITE_URL)));

	// locale path
	StringCchPrintf (app_localepath, _countof (app_localepath), L"%s\\%s.lng", ConfigGet (L"LocalePath", GetDirectory ()).GetString (), app_name_short);

	// update path
#ifdef _APP_HAVE_UPDATES
	StringCchPrintf (app_updatepath, _countof (app_updatepath), L"%s\\%s_update.exe", GetProfileDirectory (), app_name_short);
#endif // _APP_HAVE_UPDATES

	LocaleInit ();
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRId32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, UINT def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIu32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, DWORD def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%lu", def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LONGLONG def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRId64, def), name);
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

bool rapp::ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%lu", val), name);
}

bool rapp::ConfigSet (LPCWSTR key, LONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRId32, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, LONGLONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRId64, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, bool val, LPCWSTR name)
{
	return ConfigSet (key, val ? L"true" : L"false", name);
}

bool rapp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_fs_exists (app_profile_directory))
		_r_fs_mkdir (app_profile_directory);

	if (!name)
		name = app_name_short;

	// update hash value
	app_config_array[name][key] = val;

	if (WritePrivateProfileString (name, key, val, GetConfigPath ()))
		return true;

	return false;
}

bool rapp::ConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg)
{
	if (!ConfigGet (config_cfg, true).AsBool ())
		return true;

	BOOL is_flagchecked = FALSE;

	INT result = 0;

#ifndef _APP_NO_WINXP
	if (IsVistaOrLater ())
	{
#endif // _APP_NO_WINXP

#ifdef IDS_QUESTION_FLAG_CHK
		const rstring flag_text = LocaleString (IDS_QUESTION_FLAG_CHK, nullptr);
#else
		const rstring flag_text = L"Do not ask again";
#pragma _R_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK

		TASKDIALOGCONFIG tdc = {0};

		WCHAR str_title[64] = {0};
		WCHAR str_main[256] = {0};
		WCHAR str_content[512] = {0};
		WCHAR str_flag[64] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetHINSTANCE ();
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		tdc.pszWindowTitle = str_title;
		tdc.pszMainInstruction = str_main;
		tdc.pszContent = str_content;
		tdc.pszVerificationText = str_flag;
		tdc.pfCallback = &_r_msg_callback;
		tdc.lpCallbackData = MAKELONG (0, 1);

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
	else if (result == IDOK || result == IDYES)
	{
		if (is_flagchecked)
			ConfigSet (config_cfg, false);

		return true;
	}

	return false;
}

#ifndef _APP_NO_ABOUT
void rapp::CreateAboutWindow (HWND hwnd)
{
	const HANDLE habout = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%s", app_name_short, TEXT (__FUNCTION__)));

	if (GetLastError () != ERROR_ALREADY_EXISTS)
	{
#ifdef _WIN64
#define architecture 64
#else
#define architecture 32
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

			WCHAR btn_ok[64] = {0};
			WCHAR btn_close[64] = {0};

			TASKDIALOGCONFIG tdc = {0};
			TASKDIALOG_BUTTON buttons[2] = {0};

			tdc.cbSize = sizeof (tdc);
			tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
			tdc.hwndParent = hwnd;
			tdc.hInstance = GetHINSTANCE ();
#ifdef IDI_MAIN
			tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
			tdc.pszMainIcon = TD_INFORMATION_ICON;
#pragma _R_WARNING(IDI_MAIN)
#endif
			tdc.pszFooterIcon = TD_INFORMATION_ICON;
			tdc.nDefaultButton = IDCLOSE;
			tdc.pszWindowTitle = title;
			tdc.pszMainInstruction = main;
			tdc.pszContent = content;
			tdc.pszFooter = footer;
			tdc.pfCallback = &_r_msg_callback;
			tdc.lpCallbackData = MAKELONG (1, 1); // always on top

			tdc.pButtons = buttons;
			tdc.cButtons = _countof (buttons);

			buttons[0].nButtonID = IDOK;
			buttons[0].pszButtonText = btn_ok;

			buttons[1].nButtonID = IDCLOSE;
			buttons[1].pszButtonText = btn_close;

#ifdef IDS_DONATE
			StringCchCopy (btn_ok, _countof (btn_ok), LocaleString (IDS_DONATE, nullptr));
#else
			StringCchCopy (btn_ok, _countof (btn_ok), L"Give thanks!");
#pragma _R_WARNING(IDS_DONATE)
#endif

#ifdef IDS_CLOSE
			StringCchCopy (btn_close, _countof (btn_close), LocaleString (IDS_CLOSE, nullptr));
#else
			StringCchCopy (btn_close, _countof (btn_close), L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif

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

		ReleaseMutex (habout);
	}

	if (habout)
		CloseHandle (habout);
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

#ifndef _APP_NO_WINXP
bool rapp::IsVistaOrLater () const
{
	return is_vistaorlater;
}
#endif // _APP_NO_WINXP

LRESULT CALLBACK rapp::MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = (rapp*)GetProp (hwnd, L"this_ptr");

#ifdef _APP_HAVE_TRAY
	if (msg == WM_TASKBARCREATED)
	{
		SendMessage (hwnd, RM_UNINITIALIZE, 0, 0);
		SendMessage (hwnd, RM_INITIALIZE, 0, 0);

		return FALSE;
}
#endif // _APP_HAVE_TRAY

	switch (msg)
	{
		case WM_THEMECHANGED:
		{
			this_ptr->is_classic = !IsThemeActive () || this_ptr->ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool ();

			SendMessage (hwnd, RM_LOCALIZE, 0, 0);

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

			SendMessage (hwnd, RM_UNINITIALIZE, 0, 0);

			this_ptr->MutexDestroy ();

			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
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

			this_ptr->ConfigSet (L"WindowPosX", rc.left);
			this_ptr->ConfigSet (L"WindowPosY", rc.top);

			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				this_ptr->ConfigSet (L"WindowPosWidth", _R_RECT_WIDTH (&rc));
				this_ptr->ConfigSet (L"WindowPosHeight", _R_RECT_HEIGHT (&rc));
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
	}

	if (!this_ptr->app_wndproc) // check (required!)
		return FALSE;

	return CallWindowProc (this_ptr->app_wndproc, hwnd, msg, wparam, lparam);
}

bool rapp::CreateMainWindow (UINT dlg_id, UINT icon_id, DLGPROC proc)
{
	bool result = false;

	// check checksum
	{
		const HINSTANCE hlib = LoadLibrary (L"imagehlp.dll");

		if (hlib)
		{
			typedef BOOL (WINAPI *MFACS) (PCWSTR, PDWORD, PDWORD); // MapFileAndCheckSumW
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

	if (MutexIsExists (true))
		return false;

#if defined(_APP_HAVE_SKIPUAC) || defined(_APP_NO_GUEST)
	if (RunAsAdmin ())
		return false;

#ifdef _APP_NO_GUEST
	if (!IsAdmin ())
	{
		_r_msg (nullptr, MB_OK | MB_ICONEXCLAMATION, app_name, L"Warning!", L"%s administrative privileges is required!", app_name);
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

	MutexCreate ();

	if (ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool () || !IsThemeActive ())
	{
		is_classic = true;

		if (IsThemeActive () && ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool ())
			SetThemeAppProperties (1);
	}

	// create window
	if (dlg_id && proc)
	{
#ifdef _APP_HAVE_UPDATES
		if (_r_fs_exists (GetUpdatePath ()))
		{
			WCHAR str_content[MAX_PATH] = {0};

#ifdef IDS_UPDATE_INSTALL
			StringCchCopy (str_content, _countof (str_content), LocaleString (IDS_UPDATE_INSTALL, nullptr));
#else
			StringCchCopy (str_content, _countof (str_content), L"Update available, do you want to install them?");
#pragma _R_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL

			if (_r_msg (nullptr, MB_YESNO | MB_USERICON | MB_TOPMOST, app_name, nullptr, L"%s", str_content) == IDYES)
			{
				UpdateInstall ();
				return false;
			}
		}
#endif //_APP_HAVE_UPDATES

#ifdef _APP_HAVE_UPDATES
		UpdateAddComponent (app_name, app_name_short, app_version, GetDirectory (), true);
		UpdateAddComponent (L"Language pack", L"language", _r_fmt (L"%" PRId64, LocaleGetVersion ()), GetLocalePath (), false);
#endif _APP_HAVE_UPDATES

		app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (dlg_id), nullptr, proc);

		if (app_hwnd)
		{
			// enable messages bypass uipi
#ifdef _APP_HAVE_TRAY
			_r_wnd_changemessagefilter (GetHWND (), WM_TASKBARCREATED, MSGFLT_ALLOW);
#endif // _APP_HAVE_TRAY

			// uipi fix (vista+)
#ifndef _APP_NO_WINXP
			if (IsVistaOrLater ())
			{
#endif // _APP_NO_WINXP
				_r_wnd_changemessagefilter (GetHWND (), WM_DROPFILES, MSGFLT_ALLOW);
				_r_wnd_changemessagefilter (GetHWND (), WM_COPYDATA, MSGFLT_ALLOW);
				_r_wnd_changemessagefilter (GetHWND (), WM_COPYGLOBALDATA, MSGFLT_ALLOW);
#ifndef _APP_NO_WINXP
			}
#endif // _APP_NO_WINXP

			// subclass window
			SetProp (GetHWND (), L"this_ptr", this);
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
			_r_wnd_top (GetHWND (), ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());

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

				rect_new.left = ConfigGet (L"WindowPosX", rect_original.left).AsLong ();
				rect_new.top = ConfigGet (L"WindowPosY", rect_original.top).AsLong ();

				if ((GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_SIZEBOX) != 0)
				{
					rect_new.right = max (ConfigGet (L"WindowPosWidth", 0).AsLong (), max_width) + rect_new.left;
					rect_new.bottom = max (ConfigGet (L"WindowPosHeight", 0).AsLong (), max_height) + rect_new.top;
				}
				else
				{
					rect_new.right = _R_RECT_WIDTH (&rect_original) + rect_new.left;
					rect_new.bottom = _R_RECT_HEIGHT (&rect_original) + rect_new.top;
				}

				_r_wnd_adjustwindowrect (nullptr, &rect_new);

				SetWindowPos (GetHWND (), nullptr, rect_new.left, rect_new.top, _R_RECT_WIDTH (&rect_new), _R_RECT_HEIGHT (&rect_new), SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_FRAMECHANGED);
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
			{
				SendMessage (GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)GetSharedIcon (GetHINSTANCE (), icon_id, GetSystemMetrics (SM_CXSMICON)));
				SendMessage (GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)GetSharedIcon (GetHINSTANCE (), icon_id, GetSystemMetrics (SM_CXICON)));
			}

			// common initialization
			SendMessage (GetHWND (), RM_INITIALIZE, 0, 0);
			SendMessage (GetHWND (), RM_LOCALIZE, 0, 0);

			DrawMenuBar (GetHWND ()); // redraw menu

#ifdef _APP_HAVE_UPDATES
			if (ConfigGet (L"CheckUpdates", true).AsBool ())
				UpdateCheck (false);
#endif // _APP_HAVE_UPDATES

			result = true;
	}
	}

	return result;
}

#ifdef _APP_HAVE_TRAY
bool rapp::TrayCreate (HWND hwnd, UINT uid, LPGUID guid, UINT code, HICON hicon, bool is_hidden)
{
	bool result = false;

	NOTIFYICONDATA nid = {0};

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = (IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_SHOWTIP | NIF_TIP;
	nid.uID = uid;
	nid.uCallbackMessage = code;
	nid.hIcon = hicon;
	StringCchCopy (nid.szTip, _countof (nid.szTip), app_name);

	if (guid && _r_sys_validversion (6, 1))
	{
		nid.uFlags |= NIF_GUID;
		CopyMemory (&nid.guidItem, guid, sizeof (GUID));
	}

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

bool rapp::TrayDestroy (HWND hwnd, UINT uid, LPGUID guid)
{
	NOTIFYICONDATA nid = {0};

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = (IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (guid && _r_sys_validversion (6, 1))
	{
		nid.uFlags |= NIF_GUID;
		CopyMemory (&nid.guidItem, guid, sizeof (GUID));
	}

	if (Shell_NotifyIcon (NIM_DELETE, &nid))
		return true;

	return false;
}

bool rapp::TrayPopup (HWND hwnd, UINT uid, LPGUID guid, DWORD icon_id, LPCWSTR title, LPCWSTR text)
{
	bool result = false;

	NOTIFYICONDATA nid = {0};

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = (IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.uFlags = NIF_INFO | NIF_REALTIME;
	nid.dwInfoFlags = NIIF_LARGE_ICON | icon_id;
	nid.hWnd = hwnd;
	nid.uID = uid;

	if (guid && _r_sys_validversion (6, 1))
	{
		nid.uFlags |= NIF_GUID;
		CopyMemory (&nid.guidItem, guid, sizeof (GUID));
	}

	if (icon_id == NIIF_USER)
	{
#ifndef _APP_NO_WINXP
		if (IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP

#ifdef IDI_MAIN
			nid.hBalloonIcon = GetSharedIcon (GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXICON));
#else
			nid.hBalloonIcon = GetSharedIcon (nullptr, SIH_INFORMATION, GetSystemMetrics (SM_CXICON));
#pragma _R_WARNING(IDI_MAIN)
#endif

#ifndef _APP_NO_WINXP
		}
		else
		{
			nid.dwInfoFlags = NIIF_INFO;
		}
#endif // _APP_NO_WINXP
	}
	else
	{
		nid.dwInfoFlags = NIIF_INFO;
	}

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

bool rapp::TraySetInfo (HWND hwnd, UINT uid, LPGUID guid, HICON hicon, LPCWSTR tooltip)
{
	NOTIFYICONDATA nid = {0};

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = (IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.hWnd = hwnd;
	nid.uID = uid;

	if (guid && _r_sys_validversion (6, 1))
	{
		nid.uFlags |= NIF_GUID;
		CopyMemory (&nid.guidItem, guid, sizeof (GUID));
	}

	if (hicon)
	{
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

bool rapp::TrayToggle (HWND hwnd, UINT uid, LPGUID guid, bool is_show)
{
	NOTIFYICONDATA nid = {0};

#ifdef _APP_NO_WINXP
	nid.cbSize = sizeof (nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
#else
	nid.cbSize = (IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE);
	nid.uVersion = (IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION);
#endif // _APP_NO_WINXP

	nid.uFlags = NIF_STATE;
	nid.hWnd = hwnd;
	nid.uID = uid;

	if (guid && _r_sys_validversion (6, 1))
	{
		nid.uFlags |= NIF_GUID;
		CopyMemory (&nid.guidItem, guid, sizeof (GUID));
	}

	nid.dwState = is_show ? 0 : NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;

	if (Shell_NotifyIcon (NIM_MODIFY, &nid))
		return true;

	return false;
}
#endif // _APP_HAVE_TRAY

#ifdef _APP_HAVE_SETTINGS
void rapp::CreateSettingsWindow (DLGPROC proc, size_t dlg_id)
{
	const HANDLE hsettings = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%s", app_name_short, TEXT (__FUNCTION__)));

	if (GetLastError () != ERROR_ALREADY_EXISTS)
	{
		if (dlg_id != LAST_VALUE)
			ConfigSet (L"SettingsLastPage", (DWORD)dlg_id);

		if (proc)
			app_settings_proc = proc;

#ifdef IDD_SETTINGS
		DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), nullptr, &SettingsWndProc, (LPARAM)this);
#else
#pragma _R_WARNING(IDD_SETTINGS)
#endif // IDD_SETTINGS

		ReleaseMutex (hsettings);
	}

	if (hsettings)
		CloseHandle (hsettings);
}

size_t rapp::SettingsAddPage (UINT dlg_id, UINT locale_id, size_t group_id)
{
	PAPP_SETTINGS_PAGE ptr_page = new APP_SETTINGS_PAGE;

	if (ptr_page)
	{
		ptr_page->hwnd = nullptr;

		ptr_page->dlg_id = dlg_id;
		ptr_page->group_id = group_id;
		ptr_page->locale_id = locale_id;

		app_settings_pages.push_back (ptr_page);

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
#ifdef IDS_SETTINGS
	SetWindowText (hwnd, LocaleString (IDS_SETTINGS, nullptr));
#else
	SetWindowText (hwnd, L"Settings");
#endif // IDS_SETTINGS

#ifdef IDC_CLOSE
#ifdef IDS_CLOSE
	SetDlgItemText (hwnd, IDC_CLOSE, LocaleString (IDS_CLOSE, nullptr));
#else
	SetDlgItemText (hwnd, IDC_CLOSE, L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE
#else
#pragma _R_WARNING(IDC_CLOSE)
#endif // IDC_CLOSE

#ifdef IDC_RESET
#ifdef IDS_RESET
	SetDlgItemText (hwnd, IDC_RESET, LocaleString (IDS_RESET, nullptr));
#else
	SetDlgItemText (hwnd, IDC_RESET, L"Reset");
#pragma _R_WARNING(IDS_RESET)
#endif // IDS_RESET
#else
#pragma _R_WARNING(IDC_RESET)
#endif // IDC_RESET

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

LPCWSTR rapp::GetConfigPath () const
{
	return app_config_path;
}

LPCWSTR rapp::GetLocalePath () const
{
	return app_localepath;
}

#ifdef _APP_HAVE_UPDATES
LPCWSTR rapp::GetUpdatePath () const
{
	return app_updatepath;
}
#endif // _APP_HAVE_UPDATES

LPCWSTR rapp::GetUserAgent () const
{
	return app_useragent;
}

INT rapp::GetDPI (INT v) const
{
	return (INT)ceil (DOUBLE (v)* dpi_percent);
}

HICON rapp::GetSharedIcon (HINSTANCE hinst, UINT icon_id, INT cx_width)
{
	PAPP_SHARED_ICON presult = nullptr;

	for (size_t i = 0; i < app_shared_icons.size (); i++)
	{
		PAPP_SHARED_ICON const pshared = app_shared_icons.at (i);

		if (pshared && pshared->hinst == hinst && pshared->icon_id == icon_id && pshared->cx_width == cx_width && pshared->hicon)
			presult = pshared;
	}

	if (!presult)
	{
		presult = new APP_SHARED_ICON;

		if (presult)
		{
			const HICON hicon = _r_loadicon (hinst, MAKEINTRESOURCE (icon_id), cx_width);

			if (hicon)
			{
				presult->hinst = hinst;
				presult->icon_id = icon_id;
				presult->cx_width = cx_width;
				presult->hicon = hicon;

				app_shared_icons.push_back (presult);
			}
			else
			{
				delete presult;
				presult = nullptr;
			}
		}
	}

	if (!presult)
		return nullptr;

	return presult->hicon;
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

	if (SettingsGetWindow ())
	{
		SettingsInitialize ();

		if (settings_page_id != LAST_VALUE)
		{
			PAPP_SETTINGS_PAGE ptr_page = app_settings_pages.at (settings_page_id);

			if (ptr_page && ptr_page->hwnd)
				SendMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, (LPARAM)ptr_page);
		}
	}

	if (GetHWND ())
	{
		SendMessage (GetHWND (), RM_LOCALIZE, 0, 0);

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

	if (GetHWND ())
	{
		SendMessage (GetHWND (), RM_LOCALIZE, 0, 0);

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
		{
			AppendMenu (hmenu, MF_SEPARATOR, 0, nullptr);
			EnableMenuItem ((HMENU)hwnd, ctrl_id, MF_BYPOSITION | MF_ENABLED);
		}

		for (size_t i = 0; i < app_locale_names.size (); i++)
		{
			LPCWSTR name = app_locale_names.at (i);

			if (is_menu)
			{
				AppendMenu (hmenu, MF_STRING, UINT (idx) + id_start, name);

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

size_t rapp::LocaleGetCount () const
{
	return app_locale_array.size ();
}

time_t rapp::LocaleGetVersion () const
{
	return app_locale_timetamp;
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

	ParseINI (GetLocalePath (), &app_locale_array, &app_locale_names);

	if (!app_locale_array.empty ())
	{
		for (auto &p : app_locale_array)
		{
			if (app_locale_array[p.first].find (L"000") != app_locale_array[p.first].end ())
			{
				const time_t timestamp = app_locale_array[p.first][L"000"].AsLonglong ();

				if (app_locale_timetamp < timestamp)
					app_locale_timetamp = timestamp;
			}

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

	if (uid)
	{
		rstring key;
		key.Format (L"%03d", uid);

		if (locale_current[0])
		{
			// check key is exists
			if (
				app_locale_array.find (locale_current) != app_locale_array.end () &&
				app_locale_array[locale_current].find (key) != app_locale_array[locale_current].end ()
				)
			{
				result = app_locale_array[locale_current][key];
			}
		}

		if (result.IsEmpty ())
		{
			if (LoadString (GetHINSTANCE (), uid, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH))
			{
				result.ReleaseBuffer ();
			}
			else
			{
				result.ReleaseBuffer ();
				result = key;
			}
		}
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
INT_PTR CALLBACK rapp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = (rapp*)(lparam);
			this_ptr->settings_hwnd = hwnd;

#ifdef IDI_MAIN
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)this_ptr->GetSharedIcon (this_ptr->GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)this_ptr->GetSharedIcon (this_ptr->GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXICON)));
#else
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, this_ptr->GetDPI (20));

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_SETBKCOLOR, TVSIL_STATE, (LPARAM)GetSysColor (COLOR_3DFACE));

			const UINT dlg_id = this_ptr->ConfigGet (L"SettingsLastPage", this_ptr->app_settings_pages.at (0)->dlg_id).AsUint ();

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				PAPP_SETTINGS_PAGE ptr_page = this_ptr->app_settings_pages.at (i);

				if (ptr_page)
				{
					if (ptr_page->dlg_id)
					{
						ptr_page->hwnd = CreateDialogParam (this_ptr->GetHINSTANCE (), MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, this_ptr->app_settings_proc, (LPARAM)ptr_page);

						if (ptr_page->hwnd)
						{
							EnableThemeDialogTexture (ptr_page->hwnd, ETDT_ENABLETAB);

							SetWindowLongPtr (ptr_page->hwnd, GWLP_USERDATA, (LONG_PTR)ptr_page->dlg_id);
							SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, (LPARAM)ptr_page);
						}
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
#ifdef _APP_HAVE_UPDATES
			if (this_ptr->ConfigGet (L"CheckUpdates", true).AsBool ())
				this_ptr->UpdateCheck (false);
#endif // _APP_HAVE_UPDATES

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

					if (ptr_page->item)
						ptr_page->item = nullptr;
				}
			}

			this_ptr->settings_hwnd = nullptr;
			this_ptr->settings_page_id = LAST_VALUE;

			_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());

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

						PAPP_SETTINGS_PAGE const ptr_page_old = this_ptr->app_settings_pages.at (old_id);
						PAPP_SETTINGS_PAGE const ptr_page_new = this_ptr->app_settings_pages.at (new_id);

						if (ptr_page_old && ptr_page_old->hwnd)
							ShowWindow (ptr_page_old->hwnd, SW_HIDE);

						if (ptr_page_new)
						{
							this_ptr->ConfigSet (L"SettingsLastPage", (DWORD)ptr_page_new->dlg_id);

							if (ptr_page_new->hwnd)
							{
								SendMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, (LPARAM)ptr_page_new);
								ShowWindow (ptr_page_new->hwnd, SW_SHOW);
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

#ifdef IDC_RESET
				case IDC_RESET:
				{
					WCHAR str_content[512] = {0};

#ifdef IDS_QUESTION_RESET
					StringCchCopy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_QUESTION_RESET, nullptr).GetString ());
#else
					StringCchCopy (str_content, _countof (str_content), L"Are you really sure you want to reset all application settings?");
#pragma _R_WARNING(IDS_QUESTION_RESET)
#endif // IDS_QUESTION_RESET

					if (_r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2, this_ptr->app_name, nullptr, L"%s", str_content) == IDYES)
					{
						if (!_r_fs_move (this_ptr->GetConfigPath (), _r_fmt (L"%s.bak", this_ptr->GetConfigPath ()), MOVEFILE_REPLACE_EXISTING)) // remove settings
							_r_fs_delete (this_ptr->GetConfigPath (), false);

						this_ptr->ConfigInit ();

#ifdef _APP_HAVE_AUTORUN
						this_ptr->AutorunEnable (false);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_SKIPUAC
						this_ptr->SkipUacEnable (false);
#endif // _APP_HAVE_SKIPUAC

						// reinitialize application
						if (this_ptr->GetHWND ())
						{
							SendMessage (this_ptr->GetHWND (), RM_INITIALIZE, 0, 0);
							SendMessage (this_ptr->GetHWND (), RM_LOCALIZE, 0, 0);

							SendMessage (this_ptr->GetHWND (), WM_EXITSIZEMOVE, 0, 0); // reset size and pos

							SendMessage (this_ptr->GetHWND (), RM_RESET_DONE, 0, 0);

							DrawMenuBar (this_ptr->GetHWND ());
						}

						// reinitialize settings
						for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
						{
							PAPP_SETTINGS_PAGE const ptr_page = this_ptr->app_settings_pages.at (i);

							if (ptr_page && ptr_page->hwnd)
							{
								SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, (LPARAM)ptr_page);

								if (this_ptr->settings_page_id == i)
									SendMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, (LPARAM)ptr_page);
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
#endif // _APP_HAVE_SETTINGS

#ifdef _APP_HAVE_UPDATES
bool rapp::UpdateDownloadCallback (DWORD total_written, DWORD total_length, LONG_PTR lpdata)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lpdata;

	if (pupdateinfo)
	{
		rapp* papp = (rapp*)(pupdateinfo->papp);

		const DWORD percent = _R_PERCENT_OF (total_written, total_length);

#ifndef _APP_NO_WINXP
		if (papp->IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP
			if (pupdateinfo->hwnd)
			{
				WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_DOWNLOAD
				StringCchPrintf (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_DOWNLOAD, L" %d%%"), percent);
#else
				StringCchPrintf (str_content, _countof (str_content), L"Downloading update... %d%%", percent);
#pragma _R_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

				SendMessage (pupdateinfo->hwnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
				SendMessage (pupdateinfo->hwnd, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
			}
#ifndef _APP_NO_WINXP
		}
#endif // _APP_NO_WINXP
	}

	return true;
}

UINT WINAPI rapp::UpdateDownloadThread (LPVOID lparam)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lparam;
	bool is_downloaded = false;
	bool is_downloaded_installer = false;

	if (pupdateinfo)
	{
		rapp* papp = (rapp*)(pupdateinfo->papp);

		for (size_t i = 0; i < pupdateinfo->components.size (); i++)
		{
			PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

			if (pcomponent)
			{
				if (pcomponent->is_haveupdates)
				{
					if (papp->DownloadURL (pcomponent->url, (LPVOID)pcomponent->filepath.GetString (), true, &papp->UpdateDownloadCallback, (LONG_PTR)pupdateinfo))
					{
						pcomponent->is_downloaded = true;
						pcomponent->is_haveupdates = false;

						is_downloaded = true;

						if (pcomponent->is_installer)
						{
							is_downloaded_installer = true;
							break;
						}
					}
				}
			}
		}

		// show result text
		{
			WCHAR str_content[256] = {0};

			if (is_downloaded)
			{
				if (is_downloaded_installer)
				{
#ifdef IDS_UPDATE_INSTALL
					StringCchCopy (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_INSTALL, nullptr));
#else
					StringCchCopy (str_content, _countof (str_content), L"Update available, do you want to install them?");
#pragma _R_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL
				}
				else
				{
#ifdef IDS_UPDATE_DONE
					StringCchCopy (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_DONE, nullptr));
#else
					StringCchCopy (str_content, _countof (str_content), L"Downloading update finished.");
#pragma _R_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
				}
			}
			else
			{
#ifdef IDS_UPDATE_ERROR
				StringCchCopy (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_ERROR, nullptr));
#else
				StringCchCopy (str_content, _countof (str_content), L"Update server connection error");
#pragma _R_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
			}

#ifndef _APP_NO_WINXP
			if (papp->IsVistaOrLater ())
			{
#endif // _APP_NO_WINXP
				if (pupdateinfo->hwnd)
				{
					papp->UpdateDialogNavigate (pupdateinfo->hwnd, (is_downloaded ? nullptr : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
				}
#ifndef _APP_NO_WINXP
			}
			else
			{
				if (pupdateinfo->is_forced)
					_r_msg (papp->GetHWND (), is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONEXCLAMATION), papp->app_name, nullptr, L"%s", str_content);
			}
#endif // _APP_NO_WINXP
		}
	}

	//SetEvent (pupdateinfo->hend);

	_endthreadex (ERROR_SUCCESS);

	return ERROR_SUCCESS;
}

HRESULT CALLBACK rapp::UpdateDialogCallback (HWND hwnd, UINT msg, WPARAM wparam, LPARAM, LONG_PTR lpdata)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lpdata;

	switch (msg)
	{
		case TDN_CREATED:
		{
			pupdateinfo->hwnd = hwnd;

			SendMessage (hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			SendMessage (hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			_r_wnd_center (hwnd, GetParent (hwnd));

			if (pupdateinfo->is_forced)
				_r_wnd_top (hwnd, true);

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			if (pupdateinfo->hthread && pupdateinfo->hthread != (HANDLE)-1L)
				ResumeThread (pupdateinfo->hthread);

			break;
		}

		case TDN_DESTROYED:
		{
			SetEvent (pupdateinfo->hend);

			if (pupdateinfo->hthread && pupdateinfo->hthread != (HANDLE)-1L)
			{
				TerminateThread (pupdateinfo->hthread, 0);
				CloseHandle (pupdateinfo->hthread);

				pupdateinfo->hthread = nullptr;
			}

			break;
		}

		case TDN_BUTTON_CLICKED:
		{
			if (wparam == IDYES)
			{
				rapp* papp = (rapp*)(pupdateinfo->papp);

				WCHAR str_content[MAX_PATH] = {0};

#ifdef IDS_UPDATE_DOWNLOAD
				StringCchPrintf (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_DOWNLOAD, nullptr), 0);
#else
				StringCchCopy (str_content, _countof (str_content), L"Downloading update...");
#pragma _R_WARNING(IDS_UPDATE_DOWNLOAD)
#endif
				pupdateinfo->hthread = (HANDLE)_beginthreadex (nullptr, 0, &UpdateDownloadThread, (LPVOID)pupdateinfo, CREATE_SUSPENDED, nullptr);

				if (pupdateinfo->hthread)
				{
					papp->UpdateDialogNavigate (hwnd, nullptr, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				rapp* papp = (rapp*)(pupdateinfo->papp);

				papp->UpdateInstall ();
				DestroyWindow (papp->GetHWND ());

				return S_FALSE;
			}

			break;
		}
	}

	return S_OK;
}

INT rapp::UpdateDialogNavigate (HWND hwnd, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata)
{
	TASKDIALOGCONFIG tdc = {0};

	WCHAR str_title[128] = {0};
	WCHAR str_main[256] = {0};
	WCHAR str_content[512] = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | flags;
	tdc.hwndParent = GetHWND ();
	tdc.hInstance = GetHINSTANCE ();
	tdc.dwCommonButtons = buttons;
	tdc.pfCallback = &UpdateDialogCallback;
	tdc.lpCallbackData = lpdata;
	tdc.pszWindowTitle = str_title;

	if (main_icon)
	{
		tdc.pszMainIcon = main_icon;
	}
	else
	{
#ifdef IDI_MAIN
		tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
		tdc.pszMainIcon = TD_INFORMATION_ICON;
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN
	}

	StringCchCopy (str_title, _countof (str_title), app_name);

	if (main)
	{
		tdc.pszMainInstruction = str_main;
		StringCchCopy (str_main, _countof (str_main), main);
	}

	if (content)
	{
		tdc.pszContent = str_content;
		StringCchCopy (str_content, _countof (str_content), content);
	}

	INT button = 0;

	if (hwnd)
		SendMessage (hwnd, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);

	else
		_r_msg_taskdialog (&tdc, &button, nullptr, nullptr);

	return button;
}

rstring format_version (rstring vers)
{
	if (vers.IsNumeric ())
		return _r_fmt_date (vers.AsLonglong (), FDTF_SHORTDATE | FDTF_SHORTTIME);

	return vers;
}

void rapp::UpdateInstall ()
{
	_r_run (_r_path_expand (L"%systemroot%\\system32\\cmd.exe"), _r_fmt (L"\"cmd.exe\" /c timeout 4 > nul&&start /wait \"\" \"%s\" /S /D=%s&&timeout 4 > nul&&del /q /f \"%s\"&start \"\" \"%s\"", GetUpdatePath (), GetDirectory (), GetUpdatePath (), GetBinaryPath ()), nullptr, SW_HIDE);
}

UINT WINAPI rapp::UpdateCheckThread (LPVOID lparam)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lparam;

	if (pupdateinfo)
	{
		rapp* papp = (rapp*)(pupdateinfo->papp);

		// check for beta versions flag
#if defined(_APP_BETA) || defined(_APP_BETA_RC)
		const bool is_beta = true;
#else
		const bool is_beta = papp->ConfigGet (L"CheckUpdatesBeta", false).AsBool ();
#endif // _APP_BETA

		rstring buffer;

		if (!papp->DownloadURL (_r_fmt (_APP_WEBSITE_URL L"/update.php?product=%s&is_beta=%d&api=3", papp->app_name_short, is_beta), &buffer, false, nullptr, 0))
		{
			if (pupdateinfo->hwnd)
			{
				WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_ERROR
				StringCchCopy (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_ERROR, nullptr).GetString ());
#else
				StringCchCopy (str_content, _countof (str_content), L"Update server connection error.");
#pragma _R_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

				papp->UpdateDialogNavigate (pupdateinfo->hwnd, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
			}
		}
		else
		{
			rstring::map_one result;

			if (_r_str_unserialize (buffer, L";", L'=', &result))
			{
				bool is_updateavailable = false;
				rstring updates_text;

				for (size_t i = 0; i < pupdateinfo->components.size (); i++)
				{
					PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

					if (pcomponent)
					{
						if (result.find (pcomponent->short_name) != result.end ())
						{
							const rstring::rvector vc = result[pcomponent->short_name].AsVector (L"|");

							if (vc.size () < 2)
								continue;

							const rstring new_version = vc.at (0);
							const rstring new_url = vc.at (1);

							//if (!new_version.IsEmpty () && !new_url.IsEmpty () && (new_version.IsNumeric () ? (new_version.AsLonglong () != pcomponent->version.AsLonglong ()) : (_r_str_versioncompare (pcomponent->version, new_version) == -1)))
							if (!new_version.IsEmpty () && !new_url.IsEmpty () && (new_version.IsNumeric () ? (new_version.AsLonglong () > pcomponent->version.AsLonglong ()) : (_r_str_versioncompare (pcomponent->version, new_version) == -1)))
							{
								is_updateavailable = true;

								pcomponent->new_version = new_version;
								pcomponent->url = new_url;
								pcomponent->is_haveupdates = true;

								if (pcomponent->is_installer)
								{
									pcomponent->filepath = papp->GetUpdatePath ();
								}
								else
								{
									pcomponent->filepath.Format (L"%s\\%s-%s.tmp", _r_path_expand (L"%temp%\\").GetString (), pcomponent->short_name.GetString (), new_version.GetString ());
									pcomponent->version = new_version;
								}

								updates_text.AppendFormat (L"- %s %s\r\n", pcomponent->full_name.GetString (), format_version (new_version).GetString ());

								// do not check components when new version of application available
								if (pcomponent->is_installer)
									break;
							}
						}
					}

				}

				if (is_updateavailable)
				{
					WCHAR str_main[256] = {0};

					updates_text.Trim (L"\r\n ");

#ifdef IDS_UPDATE_YES
					StringCchCopy (str_main, _countof (str_main), papp->LocaleString (IDS_UPDATE_YES, nullptr));
#else
					StringCchCopy (str_main, _countof (str_main), L"Update available, download and install them?");
#pragma _R_WARNING(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

#ifndef _APP_NO_WINXP
					if (papp->IsVistaOrLater ())
					{
#endif
						papp->UpdateDialogNavigate (pupdateinfo->hwnd, nullptr, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)pupdateinfo);

						WaitForSingleObjectEx (pupdateinfo->hend, INFINITE, FALSE);
#ifndef _APP_NO_WINXP
					}
					else
					{
						const INT msg_id = _r_msg (papp->GetHWND (), MB_YESNO | MB_USERICON, papp->app_name, str_main, L"%s", updates_text.GetString ());

						if (msg_id == IDYES)
						{
							pupdateinfo->hthread = (HANDLE)_beginthreadex (nullptr, 0, &papp->UpdateDownloadThread, (LPVOID)pupdateinfo, CREATE_SUSPENDED, nullptr);

							if (pupdateinfo->hthread)
							{
								ResumeThread (pupdateinfo->hthread);
								WaitForSingleObjectEx (pupdateinfo->hend, INFINITE, FALSE);
							}
						}
					}
#endif
					for (size_t i = 0; i < pupdateinfo->components.size (); i++)
					{
						PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

						if (pcomponent)
						{
							if (pcomponent->is_downloaded)
							{
								if (!pcomponent->is_installer)
								{
									_r_fs_copy (pcomponent->filepath, pcomponent->target_path);

									if (papp->GetHWND ())
									{
										papp->ConfigInit (); // reload configuration

										SendMessage (papp->GetHWND (), RM_UPDATE_DONE, 0, 0);
										SendMessage (papp->GetHWND (), RM_INITIALIZE, 0, 0);
										SendMessage (papp->GetHWND (), RM_LOCALIZE, 0, 0);

										DrawMenuBar (papp->GetHWND ());
									}

									_r_fs_delete (pcomponent->filepath, false);
								}
							}
						}
					}
				}
				else
				{
					if (pupdateinfo->hwnd)
					{
						WCHAR str_content[256] = {0};
#ifdef IDS_UPDATE_NO
						StringCchCopy (str_content, _countof (str_content), papp->LocaleString (IDS_UPDATE_NO, nullptr).GetString ());
#else
						StringCchCopy (str_content, _countof (str_content), L"No updates available.");
#pragma _R_WARNING(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO

						papp->UpdateDialogNavigate (pupdateinfo->hwnd, nullptr, 0, TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
					}
				}
			}

			papp->ConfigSet (L"CheckUpdatesLast", _r_unixtime_now ());
		}

		SetEvent (pupdateinfo->hend);
	}

	_endthreadex (ERROR_SUCCESS);

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
bool rapp::SkipUacIsEnabled ()
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

	return ConfigGet (L"SkipUacIsEnabled", false).AsBool ();
}

bool rapp::SkipUacEnable (bool is_enable)
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

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

	MBSTR root (L"\\");
	MBSTR name (_r_fmt (_APP_TASKSCHD_NAME, app_name_short));
	MBSTR author (_APP_AUTHOR);
	MBSTR path (GetBinaryPath ());
	MBSTR directory (GetDirectory ());
	MBSTR args (L"$(Arg0)");
	MBSTR timelimit (L"PT0S");

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
	{
		if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
			{
				if (SUCCEEDED (service->Connect (vtEmpty, vtEmpty, vtEmpty, vtEmpty)))
				{
					if (SUCCEEDED (service->GetFolder (root, &folder)))
					{
						// create task
						if (is_enable)
						{
							if (SUCCEEDED (service->NewTask (0, &task)))
							{
								if (SUCCEEDED (task->get_RegistrationInfo (&reginfo)))
								{
									reginfo->put_Author (author);
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
									settings->put_ExecutionTimeLimit (timelimit);

									settings->Release ();
								}

								if (SUCCEEDED (task->get_Actions (&action_collection)))
								{
									if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
									{
										if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
										{
											if (
												SUCCEEDED (exec_action->put_Path (path)) &&
												SUCCEEDED (exec_action->put_WorkingDirectory (directory)) &&
												SUCCEEDED (exec_action->put_Arguments (args))
												)
											{
												action_result = true;
											}

											exec_action->Release ();
										}

										action->Release ();
									}

									action_collection->Release ();
								}

								if (action_result)
								{
									if (SUCCEEDED (folder->RegisterTaskDefinition (
										name,
										task,
										TASK_CREATE_OR_UPDATE,
										vtEmpty,
										vtEmpty,
										TASK_LOGON_INTERACTIVE_TOKEN,
										vtEmpty,
										&registered_task)
										))
									{
										{
											ConfigSet (L"SkipUacIsEnabled", true);
											result = true;

											registered_task->Release ();
										}
									}

									task->Release ();
								}
							}
						}
						else
						{
							// remove task
							result = SUCCEEDED (folder->DeleteTask (name, 0));

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

	return result;
}

bool rapp::SkipUacRun ()
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

	bool result = false;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	IRegisteredTask* registered_task = nullptr;

	ITaskDefinition* task = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;

	IRunningTask* running_task = nullptr;

	MBSTR root (L"\\");
	MBSTR name (_r_fmt (_APP_TASKSCHD_NAME, app_name_short));

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
	{
		if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
			{
				if (SUCCEEDED (service->Connect (vtEmpty, vtEmpty, vtEmpty, vtEmpty)))
				{
					if (SUCCEEDED (service->GetFolder (root, &folder)))
					{
						if (SUCCEEDED (folder->GetTask (name, &registered_task)))
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
														args.AppendFormat (L"%s ", arga[i]);

													LocalFree (arga);
												}

												variant_t ticker = args.Trim (L" ");

												if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_NO_FLAGS, 0, nullptr, &running_task)))
												{
													UINT8 count = 5; // try count

													do
													{
														_r_sleep (250);

														TASK_STATE state = TASK_STATE_UNKNOWN;

														running_task->Refresh ();
														running_task->get_State (&state);

														if (
															state == TASK_STATE_RUNNING ||
															state == TASK_STATE_READY ||
															state == TASK_STATE_DISABLED
															)
														{
															if (
																state == TASK_STATE_RUNNING ||
																state == TASK_STATE_READY
																)
															{
																result = true;
															}

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

				const bool is_mutexdestroyed = MutexDestroy ();

				if (ShellExecuteEx (&shex))
				{
					result = true;
				}
				else
				{
					if (is_mutexdestroyed)
						MutexCreate ();
				}

				CoUninitialize ();
			}
		}
}

	return result;
}
