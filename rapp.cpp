// routine++
// Copyright (c) 2012-2019 Henry++

#include "rapp.hpp"

#ifdef _APP_HAVE_TRAY
CONST UINT WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

rapp::rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// store system information
#ifndef _APP_NO_WINXP
	is_vistaorlater = _r_sys_validversion (6, 0);
#endif // _APP_NO_WINXP

#ifdef _APP_HAVE_UPDATES
	pupdateinfo = nullptr;
#endif // _APP_HAVE_UPDATES

	// safe dll loading
	SetDllDirectory (L"");

	// win7+
	if (_r_sys_validversion (6, 1))
	{
		const HINSTANCE hlib = GetModuleHandle (L"kernel32.dll");

		if (hlib)
		{
			typedef BOOL (WINAPI * SSPM) (DWORD); // SetSearchPathMode
			const SSPM _SetSearchPathMode = (SSPM)GetProcAddress (hlib, "SetSearchPathMode");

			if (_SetSearchPathMode)
				_SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);
		}
	}

	// initialize com library
	CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// get hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// general information
	_r_str_copy (app_name, _countof (app_name), name);
	_r_str_copy (app_name_short, _countof (app_name_short), short_name);
	_r_str_copy (app_version, _countof (app_version), version);
	_r_str_copy (app_copyright, _countof (app_copyright), copyright);

#ifdef _APP_BETA
	_r_str_cat (app_version, _countof (app_version), L" Pre-release");
#else
	_r_str_cat (app_version, _countof (app_version), L" Release");
#endif // _APP_BETA

	// get paths
	GetModuleFileName (GetHINSTANCE (), app_binary, _countof (app_binary));
	_r_str_copy (app_directory, _countof (app_directory), _r_path_getdirectory (app_binary));

	// parse command line
	INT numargs = 0;
	LPWSTR *arga = CommandLineToArgvW (GetCommandLine (), &numargs);

	if (arga)
	{
		if (numargs > 1)
		{
			for (INT i = 1; i < numargs; i++)
			{
				if (_r_str_compare (arga[i], L"/ini", 4) == 0 && (i + 1) <= numargs)
				{
					LPWSTR ptr = arga[i + 1];

					if (*ptr == L'/' || *ptr == L'-')
						continue;

					PathUnquoteSpaces (ptr);

					_r_str_copy (app_config_path, _countof (app_config_path), _r_path_expand (ptr));

					if (PathGetDriveNumber (app_config_path) == INVALID_INT)
						_r_str_printf (app_config_path, _countof (app_config_path), L"%s\\%s", GetDirectory (), _r_path_expand (ptr).GetString ());

					if (!_r_fs_exists (app_config_path))
					{
						HANDLE hfile = CreateFile (app_config_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

						SAFE_DELETE_HANDLE (hfile);
					}
				}
			}
		}

		SAFE_LOCAL_FREE (arga);
	}

	// get configuration path
	if (_r_str_isempty (app_config_path) || !_r_fs_exists (app_config_path))
		_r_str_printf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", GetDirectory (), app_name_short);

	if (!_r_fs_exists (app_config_path) && !_r_fs_exists (_r_fmt (L"%s\\portable.dat", GetDirectory ())))
	{
		_r_str_copy (app_profile_directory, _countof (app_profile_directory), _r_path_expand (L"%APPDATA%\\" _APP_AUTHOR L"\\"));
		_r_str_cat (app_profile_directory, _countof (app_profile_directory), app_name);
		_r_str_printf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_profile_directory, app_name_short);
	}
	else
	{
		_r_str_copy (app_profile_directory, _countof (app_profile_directory), _r_path_getdirectory (app_config_path));
	}

	// get default system locale
	GetLocaleInfo (LOCALE_SYSTEM_DEFAULT, _r_sys_validversion (6, 1) ? LOCALE_SENGLISHLANGUAGENAME : LOCALE_SENGLANGUAGE, locale_default, _countof (locale_default));

	// read config
	ConfigInit ();
}

bool rapp::MutexCreate ()
{
	MutexDestroy ();

#ifndef _APP_NO_MUTEX
	app_mutex = CreateMutex (nullptr, FALSE, app_name_short);
#else
	app_mutex = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%" PR_SIZE_T L"_%" PR_SIZE_T, app_name_short, _r_str_hash (GetBinaryPath ()), _r_str_hash (GetCommandLine ())));
#endif // _APP_NO_MUTEX

	return (app_mutex != nullptr);
}

bool rapp::MutexDestroy ()
{
	if (app_mutex)
	{
		ReleaseMutex (app_mutex);

		SAFE_DELETE_HANDLE (app_mutex);

		return true;
	}

	return false;
}

bool rapp::MutexIsExists (bool activate_window)
{
	bool result = false;

#ifndef _APP_NO_MUTEX
	HANDLE hmutex = CreateMutex (nullptr, FALSE, app_name_short);
#else
	HANDLE hmutex = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%" PR_SIZE_T L"_%" PR_SIZE_T, app_name_short, _r_str_hash (GetBinaryPath ()), _r_str_hash (GetCommandLine ())));
#endif // _APP_NO_MUTEX

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		result = true;

		if (activate_window && !wcsstr (GetCommandLine (), L"/minimized"))
			EnumWindows (&ActivateWindowCallback, (LPARAM)this->app_name);
	}
	else
	{
		if (hmutex)
			ReleaseMutex (hmutex);
	}

	SAFE_DELETE_HANDLE (hmutex);

	return result;
}

BOOL CALLBACK rapp::ActivateWindowCallback (HWND hwnd, LPARAM lparam)
{
	LPCWSTR app_name = (LPCWSTR)lparam;

	if (_r_str_isempty (app_name))
		return FALSE;

	DWORD pid = 0;
	GetWindowThreadProcessId (hwnd, &pid);

	if (GetCurrentProcessId () == pid)
		return TRUE;

	if (!IsWindow (hwnd))
		return TRUE;

	WCHAR classname[64] = {0};
	WCHAR window_title[128] = {0};

	// check window class
	if (!GetClassName (hwnd, classname, _countof (classname)))
		return TRUE;

	if (_r_str_compare (classname, L"#32770", 6) != 0)
		return TRUE;

	// check window title
	if (!GetWindowText (hwnd, window_title, _countof (window_title)))
		return TRUE;

	if (_r_str_compare (app_name, window_title, _r_str_length (app_name)) == 0)
	{
		// check window props
		if (
			GetProp (hwnd, app_name) ||
			GetProp (hwnd, L"this_ptr") // deprecated
			)
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

	HKEY hkey = nullptr;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			WCHAR buffer[MAX_PATH] = {0};
			_r_str_printf (buffer, _countof (buffer), L"\"%s\" /minimized", GetBinaryPath ());

			result = (RegSetValueEx (hkey, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((_r_str_length (buffer) + 1) * sizeof (WCHAR))) == ERROR_SUCCESS);

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

#ifdef _APP_HAVE_UPDATES
void rapp::UpdateAddComponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, bool is_installer)
{
	if (!pupdateinfo)
	{
		pupdateinfo = new APP_UPDATE_INFO;

		pupdateinfo->papp = this;
		pupdateinfo->hend = CreateEvent (nullptr, FALSE, FALSE, nullptr);
	}

	if (pupdateinfo)
	{
		PAPP_UPDATE_COMPONENT pcomponent = new APP_UPDATE_COMPONENT;
		RtlSecureZeroMemory (pcomponent, sizeof (APP_UPDATE_COMPONENT));

		_r_str_alloc (&pcomponent->full_name, _r_str_length (full_name), full_name);
		_r_str_alloc (&pcomponent->short_name, _r_str_length (short_name), short_name);
		_r_str_alloc (&pcomponent->version, _r_str_length (version), version);
		_r_str_alloc (&pcomponent->target_path, _r_str_length (target_path), target_path);

		pcomponent->is_installer = is_installer;

		pupdateinfo->components.push_back (pcomponent);
	}
}

void rapp::UpdateCheck (HWND hparent)
{
	if (!pupdateinfo)
		return;

	if (!hparent && (!ConfigGet (L"CheckUpdates", true).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0LL).AsLonglong ()) <= _APP_UPDATE_PERIOD))
		return;

	const HANDLE hthread = _r_createthread (&UpdateCheckThread, (LPVOID)pupdateinfo, true);

	if (!hthread)
		return;

	pupdateinfo->htaskdlg = nullptr;
	pupdateinfo->hparent = hparent;

	if (hparent)
	{
#ifndef _APP_NO_WINXP
		if (IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP
			WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_INIT
			_r_str_copy (str_content, _countof (str_content), LocaleString (IDS_UPDATE_INIT, nullptr));
#else // IDS_UPDATE_INIT
			_r_str_copy (str_content, _countof (str_content), L"Checking for new releases...");
#pragma _R_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

			pupdateinfo->hthread = hthread;

			UpdateDialogNavigate (nullptr, nullptr, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);

			return;

#ifndef _APP_NO_WINXP
		}
#endif // _APP_NO_WINXP
	}

	ResumeThread (hthread);
}
#endif // _APP_HAVE_UPDATES

void rapp::ConfigInit ()
{
	app_config_array.clear (); // reset
	_r_parseini (GetConfigPath (), app_config_array, nullptr);

	// locale path
	_r_str_printf (app_localepath, _countof (app_localepath), L"%s\\%s.lng", ConfigGet (L"LocalePath", GetDirectory ()).GetString (), app_name_short);

	// update path
#ifdef _APP_HAVE_UPDATES
	_r_str_printf (app_updatepath, _countof (app_updatepath), L"%s\\%s_update.exe", GetProfileDirectory (), app_name_short);
#endif // _APP_HAVE_UPDATES

	LocaleInit ();
}

rstring rapp::ConfigGet (LPCWSTR key, bool def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIi32, def ? 1 : 0), name);
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIi32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, UINT def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIu32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LONG def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRId32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, ULONG def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIu32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LONG64 def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRId64, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, ULONG64 def, LPCWSTR name)
{
	return ConfigGet (key, _r_fmt (L"%" PRIu64, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name)
{
	rstring cfg_name;

	if (_r_str_isempty (name))
		cfg_name = app_name_short;
	else
		cfg_name.Format (L"%s\\%s", app_name_short, name);

	// check key is exists
	if (app_config_array.find (cfg_name) != app_config_array.end ())
	{
		rstringmap1& rmap = app_config_array.at (cfg_name);

		if (rmap.find (key) != rmap.end ())
		{
			if (!rmap.at (key).IsEmpty ())
				return rmap.at (key);
		}
	}

	return def;
}

bool rapp::ConfigSet (LPCWSTR key, bool val, LPCWSTR name)
{
	return ConfigSet (key, val ? L"true" : L"false", name);
}

bool rapp::ConfigSet (LPCWSTR key, INT val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRIi32, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, UINT val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRIu32, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, LONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRId32, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, ULONG val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRIu32, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, LONG64 val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRId64, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, ULONG64 val, LPCWSTR name)
{
	return ConfigSet (key, _r_fmt (L"%" PRIu64, val), name);
}

bool rapp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_fs_exists (GetProfileDirectory ()))
		_r_fs_mkdir (GetProfileDirectory ());

	rstring cfg_name;

	if (_r_str_isempty (name))
		cfg_name = app_name_short;
	else
		cfg_name.Format (L"%s\\%s", app_name_short, name);

	// update hash value
	if (_r_str_isempty (val))
		app_config_array[cfg_name][key].Release ();
	else
		app_config_array[cfg_name][key] = val;

	if (WritePrivateProfileString (cfg_name, key, val, GetConfigPath ()))
		return true;

	return false;
}

bool rapp::ConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg)
{
	if (!ConfigGet (config_cfg, true).AsBool ())
		return true;

	BOOL is_flagchecked = FALSE;

	INT result = 0;

#ifdef IDS_QUESTION_FLAG_CHK
	const rstring flag_text = LocaleString (IDS_QUESTION_FLAG_CHK, nullptr);
#else
	const rstring flag_text = L"Do not ask again";
#pragma _R_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK

#ifdef _APP_NO_WINXP
	TASKDIALOGCONFIG tdc = {0};

	WCHAR str_title[64] = {0};
	WCHAR str_main[128] = {0};
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

	_r_str_copy (str_title, _countof (str_title), app_name);

	if (main)
		_r_str_copy (str_main, _countof (str_main), main);

	if (text)
		_r_str_copy (str_content, _countof (str_content), text);

	_r_str_copy (str_flag, _countof (str_flag), flag_text);

	_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked);
#else // _APP_NO_WINXP
	if (IsVistaOrLater ())
	{
		TASKDIALOGCONFIG tdc = {0};

		WCHAR str_title[64] = {0};
		WCHAR str_main[128] = {0};
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

		_r_str_copy (str_title, _countof (str_title), app_name);

		if (main)
			_r_str_copy (str_main, _countof (str_main), main);

		if (text)
			_r_str_copy (str_content, _countof (str_content), text);

		_r_str_copy (str_flag, _countof (str_flag), flag_text);

		_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked);
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
	HANDLE hmutex = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%d_%d", app_name_short, GetCurrentProcessId (), __LINE__));

	if (GetLastError () != ERROR_ALREADY_EXISTS)
	{
#ifdef _WIN64
#define architecture 64
#else
#define architecture 32
#endif // _WIN64

		WCHAR title[64] = {0};

#ifdef IDS_ABOUT
		_r_str_copy (title, _countof (title), LocaleString (IDS_ABOUT, nullptr));
#else
		_r_str_copy (title, _countof (title), L"About");
#pragma _R_WARNING(IDS_ABOUT)
#endif

#ifndef _APP_NO_WINXP
		if (IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP

			INT result = 0;

			WCHAR str_main[128] = {0};
			WCHAR str_content[512] = {0};
			WCHAR str_footer[256] = {0};

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
			tdc.pszMainInstruction = str_main;
			tdc.pszContent = str_content;
			tdc.pszFooter = str_footer;
			tdc.pfCallback = &_r_msg_callback;
			tdc.lpCallbackData = MAKELONG (1, 1); // always on top

			tdc.pButtons = buttons;
			tdc.cButtons = _countof (buttons);

			buttons[0].nButtonID = IDOK;
			buttons[0].pszButtonText = btn_ok;

			buttons[1].nButtonID = IDCLOSE;
			buttons[1].pszButtonText = btn_close;

#ifdef IDS_DONATE
			_r_str_copy (btn_ok, _countof (btn_ok), LocaleString (IDS_DONATE, nullptr));
#else
			_r_str_copy (btn_ok, _countof (btn_ok), L"Give thanks!");
#pragma _R_WARNING(IDS_DONATE)
#endif

#ifdef IDS_CLOSE
			_r_str_copy (btn_close, _countof (btn_close), LocaleString (IDS_CLOSE, nullptr));
#else
			_r_str_copy (btn_close, _countof (btn_close), L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif

			_r_str_copy (str_main, _countof (str_main), app_name);
			_r_str_printf (str_content, _countof (str_content), L"Version %s, %u-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", app_version, architecture, app_copyright, _APP_WEBSITE_URL, _APP_WEBSITE_URL + 8, _APP_GITHUB_URL, _APP_GITHUB_URL + 8);
			_r_str_copy (str_footer, _countof (str_footer), L"This program is free software; you can redistribute it and/or modify it under the terms of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License 3</a> as published by the Free Software Foundation.");

			if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
			{
				if (result == buttons[0].nButtonID)
					ShellExecute (GetHWND (), nullptr, _r_fmt (_APP_DONATE_URL, app_name_short), nullptr, nullptr, SW_SHOWDEFAULT);
			}
#ifndef _APP_NO_WINXP
		}
		else
		{
			_r_msg (hwnd, MB_OK | MB_USERICON | MB_TOPMOST, title, app_name, L"Version %s, %u-bit (Unicode)\r\n%s\r\n\r\n%s | %s", app_version, architecture, app_copyright, _APP_WEBSITE_URL + 8, _APP_GITHUB_URL + 8);
		}
#endif // _APP_NO_WINXP

		if (hmutex)
			ReleaseMutex (hmutex);
	}

	SAFE_DELETE_HANDLE (hmutex);
}
#endif // _APP_NO_ABOUT

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
	static rapp *this_ptr = reinterpret_cast<rapp*>(GetWindowLongPtr (hwnd, GWLP_USERDATA));

#ifdef _APP_HAVE_TRAY
	if (msg == WM_TASKBARCREATED)
	{
		SendMessage (hwnd, RM_TASKBARCREATED, 0, 0);
		return FALSE;
	}
#endif // _APP_HAVE_TRAY

	switch (msg)
	{
		case WM_DESTROY:
		{
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
				this_ptr->ConfigSet (L"IsWindowZoomed", !!IsZoomed (hwnd), L"window");

			SendMessage (hwnd, RM_UNINITIALIZE, 0, 0);

			this_ptr->MutexDestroy ();

			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

#ifdef _APP_HAVE_TRAY
		case WM_SYSCOMMAND:
		{
			if (wparam == SC_CLOSE)
			{
				_r_wnd_toggle (hwnd, false);
				return TRUE;
			}

			break;
		}
#endif // _APP_HAVE_TRAY

		case WM_SHOWWINDOW:
		{
			if (this_ptr->is_needmaximize)
			{
				ShowWindow (hwnd, SW_MAXIMIZE);
				this_ptr->is_needmaximize = false;
			}

			break;
		}

		case WM_THEMECHANGED:
		{
#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			this_ptr->is_classic = !IsThemeActive () || this_ptr->ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool ();

			SendMessage (hwnd, RM_LOCALIZE, 0, 0);

			break;
		}

#ifndef _APP_NO_DARKTHEME
		case WM_SETTINGCHANGE:
		{
			if (_r_wnd_isdarkmessage (reinterpret_cast<LPCWSTR>(lparam)))
				SendMessage (hwnd, WM_THEMECHANGED, 0, 0);

			break;
		}
#endif // _APP_NO_DARKTHEME

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLORSTATIC:
		{
			SetBkMode ((HDC)wparam, TRANSPARENT); // HACK!!!
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

			this_ptr->ConfigSet (L"WindowPosX", rc.left, L"window");
			this_ptr->ConfigSet (L"WindowPosY", rc.top, L"window");

			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				this_ptr->ConfigSet (L"WindowPosWidth", _R_RECT_WIDTH (&rc), L"window");
				this_ptr->ConfigSet (L"WindowPosHeight", _R_RECT_HEIGHT (&rc), L"window");

				RedrawWindow (hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);
			}

			break;
		}

		case WM_SIZE:
		{
#ifdef _APP_HAVE_TRAY
			if (wparam == SIZE_MINIMIZED)
			{
				_r_wnd_toggle (hwnd, false);
			}
#endif // _APP_HAVE_TRAY

			break;
		}

		case WM_DPICHANGED:
		{
#ifdef _APP_HAVE_SETTINGS
			if (this_ptr->GetSettingsWindow ())
				SendDlgItemMessage (this_ptr->GetSettingsWindow (), IDC_NAV, TVM_SETITEMHEIGHT, (WPARAM)_r_dc_getdpi (this_ptr->GetSettingsWindow (), _R_SIZE_ITEMHEIGHT), 0);
#endif // _APP_HAVE_SETTINGS

			// call main callback
			SendMessage (hwnd, RM_DPICHANGED, 0, 0);

			// change window size and position
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & (WS_SIZEBOX | WS_MAXIMIZEBOX)) != 0)
			{
				LPRECT lprcnew = reinterpret_cast<LPRECT>(lparam);

				if (lprcnew)
				{
					_r_wnd_resize (0, hwnd, nullptr, lprcnew->left, lprcnew->top, _R_RECT_WIDTH (lprcnew), _R_RECT_HEIGHT (lprcnew), 0);

					RECT rc_client = {0};
					GetClientRect (hwnd, &rc_client);

					SendMessage (hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM (_R_RECT_WIDTH (&rc_client), _R_RECT_HEIGHT (&rc_client)));
					SendMessage (hwnd, WM_EXITSIZEMOVE, 0, 0); // reset size and pos
				}
			}

			break;
		}
	}

	if (this_ptr->app_wndproc)
		return CallWindowProc (this_ptr->app_wndproc, hwnd, msg, wparam, lparam);

	return FALSE;

}

bool rapp::CreateMainWindow (INT dlg_id, INT icon_id, DLGPROC proc)
{
	// check checksum
#ifndef _DEBUG
	{
		const HINSTANCE hlib = LoadLibrary (L"imagehlp.dll");

		if (hlib)
		{
			typedef DWORD (WINAPI * MFACS) (PCWSTR, PDWORD, PDWORD); // MapFileAndCheckSumW
			const MFACS _MapFileAndCheckSumW = (MFACS)GetProcAddress (hlib, "MapFileAndCheckSumW");

			if (_MapFileAndCheckSumW)
			{
				DWORD dwFileChecksum, dwRealChecksum;

				if (_MapFileAndCheckSumW (GetBinaryPath (), &dwFileChecksum, &dwRealChecksum) == ERROR_SUCCESS)
				{
					if (dwRealChecksum != dwFileChecksum)
					{
						FreeLibrary (hlib);
						return false;
					}
				}
			}

			FreeLibrary (hlib);
		}
	}
#endif // _DEBUG

	bool result = false;

	// initialize controls
	{
		INITCOMMONCONTROLSEX icex = {0};

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_LISTVIEW_CLASSES |
			ICC_TREEVIEW_CLASSES |
			ICC_BAR_CLASSES |
			ICC_TAB_CLASSES |
			ICC_UPDOWN_CLASS |
			ICC_PROGRESS_CLASS |
			ICC_HOTKEY_CLASS |
			ICC_ANIMATE_CLASS |
			ICC_WIN95_CLASSES |
			ICC_DATE_CLASSES |
			ICC_USEREX_CLASSES |
			ICC_COOL_CLASSES |
			ICC_INTERNET_CLASSES |
			ICC_PAGESCROLLER_CLASS |
			ICC_NATIVEFNTCTL_CLASS |
			ICC_STANDARD_CLASSES |
			ICC_LINK_CLASS;

		InitCommonControlsEx (&icex);
	}

	if (MutexIsExists (true))
		return false;

#if defined(_APP_NO_GUEST)
	if (!_r_sys_iselevated ())
	{
		if (RunAsAdmin ())
			return false;

		_r_msg (nullptr, MB_OK | MB_ICONEXCLAMATION, app_name, L"Warning!", L"%s administrative privileges are required!", app_name);
		return false;
	}
#elif defined(_APP_HAVE_SKIPUAC)
	if (!_r_sys_iselevated () && SkipUacRun ())
		return false;
#endif // _APP_NO_GUEST

#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 ())
	{
		if (!ConfirmMessage (nullptr, L"Warning!", _r_fmt (L"You are attempting to run the 32-bit version of %s on 64-bit Windows.\r\nPlease run the 64-bit version of %s instead.", app_name, app_name), L"ConfirmWOW64"))
			return false;
	}
#endif // _DEBUG && _WIN64

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
			WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_INSTALL
			_r_str_copy (str_content, _countof (str_content), LocaleString (IDS_UPDATE_INSTALL, nullptr));
#else
			_r_str_copy (str_content, _countof (str_content), L"Update available, do you want to install them?");
#pragma _R_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL

			const INT code = _r_msg (nullptr, MB_YESNOCANCEL | MB_USERICON | MB_TOPMOST, app_name, nullptr, L"%s", str_content);

			if (code == IDYES)
			{
				UpdateInstall ();
				return false;
			}
			else if (code == IDNO)
			{
				SetFileAttributes (GetUpdatePath (), FILE_ATTRIBUTE_NORMAL);
				_r_fs_delete (GetUpdatePath (), false);
			}
			else
			{
				// continue...
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
			// enable messages bypass uipi (vista+)
#ifndef _APP_NO_WINXP
			if (IsVistaOrLater ())
			{
#endif // _APP_NO_WINXP

				UINT messages[] = {
					WM_COPYDATA,
					WM_COPYGLOBALDATA,
					WM_DROPFILES,
#ifdef _APP_HAVE_TRAY
					WM_TASKBARCREATED,
#endif // _APP_HAVE_TRAY
				};

				_r_wnd_changemessagefilter (GetHWND (), messages, _countof (messages), MSGFLT_ALLOW);

#ifndef _APP_NO_WINXP
			}
#endif // _APP_NO_WINXP

			// set window prop
			SetProp (GetHWND (), app_name, "sync-1.02; iris fucyo; none of your damn business.");

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
			_r_wnd_top (GetHWND (), ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());

			RestoreWindowPosition (GetHWND (), L"window");

			// send resize message
			if ((GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				RECT rc_client = {0};

				GetClientRect (GetHWND (), &rc_client);
				SendMessage (GetHWND (), WM_SIZE, 0, MAKELPARAM (_R_RECT_WIDTH (&rc_client), _R_RECT_HEIGHT (&rc_client)));
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

				if ((GetWindowLongPtr (GetHWND (), GWL_STYLE) & WS_MAXIMIZEBOX) != 0 && ConfigGet (L"IsWindowZoomed", false, L"window").AsBool ())
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
				SendMessage (GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)GetSharedImage (GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (GetHWND (), SM_CXSMICON)));
				SendMessage (GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)GetSharedImage (GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (GetHWND (), SM_CXICON)));
			}

			// common initialization
			SendMessage (GetHWND (), RM_INITIALIZE, 0, 0);
			SendMessage (GetHWND (), RM_LOCALIZE, 0, 0);

			DrawMenuBar (GetHWND ()); // redraw menu

#ifdef _APP_HAVE_UPDATES
			if (ConfigGet (L"CheckUpdates", true).AsBool ())
				UpdateCheck (nullptr);
#endif // _APP_HAVE_UPDATES

			result = true;
		}
	}

	return result;
}

void rapp::RestoreWindowPosition (HWND hwnd, LPCWSTR window_name)
{
	// restore window position
	RECT rect_original = {0};

	// set minmax info
	if (GetWindowRect (hwnd, &rect_original))
	{
#ifdef _APP_HAVE_MINSIZE
		max_width = _R_RECT_WIDTH (&rect_original) / 2;
		max_height = _R_RECT_HEIGHT (&rect_original) / 2;
#else
		max_width = _R_RECT_WIDTH (&rect_original);
		max_height = _R_RECT_HEIGHT (&rect_original);
#endif // _APP_HAVE_MINSIZE
	}

	// restore window position
	RECT rect_new = {0};

	rect_new.left = ConfigGet (L"WindowPosX", rect_original.left, window_name).AsLong ();
	rect_new.top = ConfigGet (L"WindowPosY", rect_original.top, window_name).AsLong ();

	if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
	{
		rect_new.right = max (ConfigGet (L"WindowPosWidth", _R_RECT_WIDTH (&rect_original), window_name).AsLong (), max_width) + rect_new.left;
		rect_new.bottom = max (ConfigGet (L"WindowPosHeight", _R_RECT_HEIGHT (&rect_original), window_name).AsLong (), max_height) + rect_new.top;
	}
	else
	{
		rect_new.right = _R_RECT_WIDTH (&rect_original) + rect_new.left;
		rect_new.bottom = _R_RECT_HEIGHT (&rect_original) + rect_new.top;
	}

	_r_wnd_adjustwindowrect (nullptr, &rect_new);

	_r_wnd_resize (nullptr, hwnd, nullptr, rect_new.left, rect_new.top, _R_RECT_WIDTH (&rect_new), _R_RECT_HEIGHT (&rect_new), 0);
}

#ifdef _APP_HAVE_SETTINGS
void rapp::CreateSettingsWindow (DLGPROC proc, INT dlg_id)
{
	HANDLE hmutex = CreateMutex (nullptr, FALSE, _r_fmt (L"%s_%d_%d", app_name_short, GetCurrentProcessId (), __LINE__));

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		_r_wnd_toggle (GetSettingsWindow (), true);
	}
	else
	{
		if (dlg_id != INVALID_INT)
			ConfigSet (L"SettingsLastPage", dlg_id);

		if (proc)
			app_settings_proc = proc;

#ifdef IDD_SETTINGS
		DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), GetHWND (), &SettingsWndProc, (LPARAM)this);
#else
#pragma _R_WARNING(IDD_SETTINGS)
#endif // IDD_SETTINGS

		if (hmutex)
			ReleaseMutex (hmutex);
	}

	SAFE_DELETE_HANDLE (hmutex);
}

INT rapp::SettingsAddPage (INT dlg_id, UINT locale_id, INT group_id)
{
	PAPP_SETTINGS_PAGE ptr_page = new APP_SETTINGS_PAGE;

	RtlSecureZeroMemory (ptr_page, sizeof (APP_SETTINGS_PAGE));

	ptr_page->dlg_id = dlg_id;
	ptr_page->group_id = group_id;
	ptr_page->locale_id = locale_id;

	app_settings_pages.push_back (ptr_page);

	return INT (app_settings_pages.size () - 1);
}

HWND rapp::GetSettingsWindow ()
{
	return settings_hwnd;
}

void rapp::SettingsInitialize ()
{
	const HWND hwnd = GetSettingsWindow ();

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
#ifdef IDC_RESET
	_r_wnd_addstyle (hwnd, IDC_RESET, IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#endif // IDC_RESET

#ifdef IDC_CLOSE
	_r_wnd_addstyle (hwnd, IDC_CLOSE, IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#endif // IDC_CLOSE

	// initialize treeview
	for (size_t i = 0; i < app_settings_pages.size (); i++)
	{
		PAPP_SETTINGS_PAGE ptr_page = app_settings_pages.at (i);

		if (!ptr_page || !ptr_page->hitem)
			continue;

		_r_treeview_setitem (hwnd, IDC_NAV, ptr_page->hitem, LocaleString (ptr_page->locale_id, nullptr));
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

rstring rapp::GetProxyConfiguration ()
{
	rstring proxy = ConfigGet (L"Proxy", nullptr).GetString ();

	if (!proxy.IsEmpty ())
	{
		return proxy;
	}
	else
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig = {0};

		if (WinHttpGetIEProxyConfigForCurrentUser (&proxyConfig))
		{
			if (!_r_str_isempty (proxyConfig.lpszProxy))
				proxy = proxyConfig.lpszProxy;

			SAFE_GLOBAL_FREE (proxyConfig.lpszProxy);
			SAFE_GLOBAL_FREE (proxyConfig.lpszProxyBypass);
			SAFE_GLOBAL_FREE (proxyConfig.lpszAutoConfigUrl);
		}
	}

	return proxy;
}

rstring rapp::GetUserAgent ()
{
	return ConfigGet (L"UserAgent", _r_fmt (L"%s/%s (+%s)", app_name, app_version, _APP_WEBSITE_URL));
}

HICON rapp::GetSharedImage (HINSTANCE hinst, INT icon_id, INT icon_size)
{
	PAPP_SHARED_IMAGE presult = nullptr;

	for (size_t i = 0; i < app_shared_icons.size (); i++)
	{
		const PAPP_SHARED_IMAGE pshared = app_shared_icons.at (i);

		if (pshared && pshared->hinst == hinst && pshared->icon_id == icon_id && pshared->icon_size == icon_size && pshared->hicon)
			presult = pshared;
	}

	if (!presult)
	{
		const HICON hicon = _r_loadicon (hinst, MAKEINTRESOURCE (icon_id), icon_size);

		if (hicon)
		{
			presult = new APP_SHARED_IMAGE;

			presult->hinst = hinst;
			presult->icon_id = icon_id;
			presult->icon_size = icon_size;
			presult->hicon = hicon;

			app_shared_icons.push_back (presult);
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
void rapp::LocaleApplyFromControl (HWND hwnd, INT ctrl_id)
{
	const rstring text = _r_ctrl_gettext (hwnd, ctrl_id);

	if (_r_str_compare (text, _APP_LANGUAGE_DEFAULT) == 0)
		*locale_current = UNICODE_NULL;

	else
		_r_str_copy (locale_current, _countof (locale_current), text);

	ConfigSet (L"Language", text);

	if (GetSettingsWindow ())
	{
		SettingsInitialize ();

		if (settings_page_id != INVALID_SIZE_T)
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
		*locale_current = UNICODE_NULL;
	}
	else
	{
		WCHAR name[LOCALE_NAME_MAX_LENGTH] = {0};
		GetMenuString (hmenu, selected_id, name, _countof (name), MF_BYCOMMAND);

		ConfigSet (L"Language", name);
		_r_str_copy (locale_current, _countof (locale_current), name);
	}

	if (GetHWND ())
	{
		SendMessage (GetHWND (), RM_LOCALIZE, 0, 0);

		DrawMenuBar (GetHWND ()); // redraw menu
	}
}

void rapp::LocaleEnum (HWND hwnd, INT ctrl_id, bool is_menu, UINT id_start)
{
	HMENU hmenu;
	HMENU hsubmenu;

	if (is_menu)
	{
		hmenu = (HMENU)hwnd;
		hsubmenu = GetSubMenu (hmenu, ctrl_id);

		// clear menu
		for (UINT i = 0;; i++)
		{
			if (!DeleteMenu (hsubmenu, id_start + i, MF_BYCOMMAND))
			{
				DeleteMenu (hsubmenu, 0, MF_BYPOSITION); // delete separator
				break;
			}
		}

		AppendMenu (hsubmenu, MF_STRING, static_cast<UINT_PTR>(id_start), _APP_LANGUAGE_DEFAULT);
		CheckMenuRadioItem (hsubmenu, id_start, id_start, id_start, MF_BYCOMMAND);
	}
	else
	{
		hmenu = hsubmenu = nullptr; // fix warning!

		SendDlgItemMessage (hwnd, ctrl_id, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, 0, (LPARAM)_APP_LANGUAGE_DEFAULT);
		SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, 0, 0);
	}

	if (!app_locale_array.empty ())
	{
		UINT idx = 1;

		if (is_menu)
		{
			EnableMenuItem (hmenu, ctrl_id, MF_BYPOSITION | MF_ENABLED);
			AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);
		}
		else
		{
			_r_ctrl_enable (hwnd, ctrl_id, true);
		}

		for (size_t i = 0; i < app_locale_names.size (); i++)
		{
			rstring& name = app_locale_names.at (i);

			const bool is_current = !_r_str_isempty (locale_current) && (_r_str_compare (locale_current, name) == 0);

			if (is_menu)
			{
				AppendMenu (hsubmenu, MF_STRING, static_cast<UINT_PTR>(idx) + static_cast<UINT_PTR>(id_start), name);

				if (is_current)
					CheckMenuRadioItem (hsubmenu, id_start, id_start + idx, id_start + idx, MF_BYCOMMAND);
			}
			else
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, (WPARAM)idx, (LPARAM)name.GetString ());

				if (is_current)
					SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, (WPARAM)idx, 0);
			}

			idx += 1;
		}
	}
	else
	{
		if (is_menu)
		{
			EnableMenuItem (hmenu, ctrl_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
		}
		else
		{
			_r_ctrl_enable (hwnd, ctrl_id, false);
		}
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

	else if (_r_str_compare (name, _APP_LANGUAGE_DEFAULT) == 0)
		name = nullptr;

	// clear
	app_locale_array.clear ();
	app_locale_names.clear ();

	_r_parseini (GetLocalePath (), app_locale_array, &app_locale_names);

	if (!app_locale_array.empty ())
	{
		for (auto &p : app_locale_array)
		{
			rstringmap1& rmap = app_locale_array[p.first];

			if (rmap.find (L"000") != rmap.end ())
			{
				const time_t timestamp = rmap[L"000"].AsLonglong ();

				if (app_locale_timetamp < timestamp)
					app_locale_timetamp = timestamp;
			}

			for (auto &p2 : rmap)
			{
				p2.second.Replace (L"\\t", L"\t");
				p2.second.Replace (L"\\r", L"\r");
				p2.second.Replace (L"\\n", L"\n");
			}
		}
	}

	if (!name.IsEmpty ())
		_r_str_copy (locale_current, _countof (locale_current), name);

	else
		*locale_current = UNICODE_NULL;
}

rstring rapp::LocaleString (UINT uid, LPCWSTR append)
{
	rstring result;

	if (uid)
	{
		rstring key_name;
		key_name.Format (L"%03" PRIu32, uid);

		if (!_r_str_isempty (locale_current))
		{
			// check key is exists
			if (
				app_locale_array.find (locale_current) != app_locale_array.end () &&
				app_locale_array[locale_current].find (key_name) != app_locale_array[locale_current].end ()
				)
			{
				result = app_locale_array[locale_current][key_name];
			}
		}

		if (result.IsEmpty ())
		{
			if (!LoadString (GetHINSTANCE (), uid, result.GetBuffer (_R_BUFFER_LENGTH), _R_BUFFER_LENGTH))
			{
				result.Release ();
				result = std::move (key_name);
			}
			else
			{
				result.ReleaseBuffer ();
			}
		}
	}

	if (!_r_str_isempty (append))
		result.Append (append);

	return result;
}

void rapp::LocaleMenu (HMENU hmenu, UINT uid, UINT item, bool by_position, LPCWSTR append)
{
	WCHAR buffer[128] = {0};
	_r_str_copy (buffer, _countof (buffer), LocaleString (uid, append));

	MENUITEMINFO mi = {0};

	mi.cbSize = sizeof (mi);
	mi.fMask = MIIM_STRING;
	mi.dwTypeData = buffer;

	SetMenuItemInfo (hmenu, item, by_position, &mi);
}

#ifdef _APP_HAVE_SETTINGS
INT_PTR CALLBACK rapp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp *this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);
			this_ptr->settings_hwnd = hwnd;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

#ifdef IDI_MAIN
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)this_ptr->GetSharedImage (this_ptr->GetHINSTANCE (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)this_ptr->GetSharedImage (this_ptr->GetHINSTANCE (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXICON)));
#else
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, this_ptr->GetHWND ());

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, _r_dc_getdpi (hwnd, _R_SIZE_ITEMHEIGHT));

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_SETBKCOLOR, TVSIL_STATE, (LPARAM)GetSysColor (COLOR_3DFACE));

			const INT dlg_id = this_ptr->ConfigGet (L"SettingsLastPage", this_ptr->app_settings_pages.at (0)->dlg_id).AsInt ();

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

					ptr_page->hitem = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (ptr_page->locale_id, nullptr), ((ptr_page->group_id == INVALID_INT) ? nullptr : this_ptr->app_settings_pages.at (ptr_page->group_id)->hitem), INVALID_INT, (LPARAM)i);

					if (dlg_id == ptr_page->dlg_id)
						SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)ptr_page->hitem);
				}
			}

			this_ptr->SettingsInitialize ();

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

#ifndef _APP_NO_DARKTHEME
		case WM_THEMECHANGED:
		{
			_r_wnd_setdarktheme (hwnd);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			if (_r_wnd_isdarkmessage (reinterpret_cast<LPCWSTR>(lparam)))
				SendMessage (hwnd, WM_THEMECHANGED, 0, 0);

			break;
		}
#endif // _APP_NO_DARKTHEME

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint (hwnd, &ps);

			// calculate "x" position
			RECT rc = {0};
			GetWindowRect (GetDlgItem (hwnd, IDC_NAV), &rc);

			INT pos_x = _R_RECT_WIDTH (&rc);

			// shift "x" position
			MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)&rc, 2);
			pos_x += rc.left + _r_dc_getsystemmetrics (hwnd, SM_CXBORDER);

			// calculate "y" position
			GetClientRect (hwnd, &rc);
			const INT pos_y = _R_RECT_HEIGHT (&rc);

			for (INT i = 0; i < pos_y; i++)
				SetPixel (hdc, pos_x, i, GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_DESTROY:
		{
#ifdef _APP_HAVE_UPDATES
			if (this_ptr->ConfigGet (L"CheckUpdates", true).AsBool ())
				this_ptr->UpdateCheck (nullptr);
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

					if (ptr_page->hitem)
						ptr_page->hitem = nullptr;
				}
			}

			this_ptr->settings_hwnd = nullptr;
			this_ptr->settings_page_id = INVALID_SIZE_T;

			_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			const INT ctrl_id = static_cast<INT>(lphdr->idFrom);

			if (ctrl_id != IDC_NAV)
				break;

			switch (lphdr->code)
			{
				case TVN_SELCHANGED:
				{
					LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

					const size_t old_id = size_t (pnmtv->itemOld.lParam);
					const size_t new_id = size_t (pnmtv->itemNew.lParam);

					this_ptr->settings_page_id = new_id;

					const PAPP_SETTINGS_PAGE ptr_page_old = this_ptr->app_settings_pages.at (old_id);
					const PAPP_SETTINGS_PAGE ptr_page_new = this_ptr->app_settings_pages.at (new_id);

					if (ptr_page_old && ptr_page_old->hwnd)
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (ptr_page_new)
					{
						this_ptr->ConfigSet (L"SettingsLastPage", ptr_page_new->dlg_id);

						if (ptr_page_new->hwnd)
						{
							SendMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, (LPARAM)ptr_page_new);

							ShowWindow (ptr_page_new->hwnd, SW_SHOW);
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
					WCHAR str_content[512] = {0};

#ifdef IDS_QUESTION_RESET
					_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_QUESTION_RESET, nullptr).GetString ());
#else
					_r_str_copy (str_content, _countof (str_content), L"Are you really sure you want to reset all application settings?");
#pragma _R_WARNING(IDS_QUESTION_RESET)
#endif // IDS_QUESTION_RESET

					if (_r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2, this_ptr->app_name, nullptr, L"%s", str_content) == IDYES)
					{
						const time_t current_timestamp = _r_unixtime_now ();

						_r_fs_makebackup (this_ptr->GetConfigPath (), current_timestamp);

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

							SendMessage (this_ptr->GetHWND (), RM_CONFIG_RESET, 0, (LPARAM)current_timestamp);

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
	PAPP_UPDATE_INFO pupdateinfo = reinterpret_cast<PAPP_UPDATE_INFO>(lpdata);

	if (pupdateinfo)
	{
		rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

		const INT percent = (INT)_R_PERCENT_OF (total_written, total_length);

#ifndef _APP_NO_WINXP
		if (this_ptr->IsVistaOrLater ())
		{
#endif // _APP_NO_WINXP
			if (pupdateinfo->htaskdlg)
			{
				WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_DOWNLOAD
				_r_str_printf (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_DOWNLOAD, L" %d%%"), percent);
#else
				_r_str_printf (str_content, _countof (str_content), L"Downloading update... %d%%", percent);
#pragma _R_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

				SendMessage (pupdateinfo->htaskdlg, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
				SendMessage (pupdateinfo->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
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
		rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

		rstring proxy_addr = this_ptr->GetProxyConfiguration ();
		HINTERNET hsession = _r_inet_createsession (this_ptr->GetUserAgent (), proxy_addr);

		if (hsession)
		{
			for (size_t i = 0; i < pupdateinfo->components.size (); i++)
			{
				PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

				if (pcomponent && pcomponent->is_haveupdates && !pcomponent->is_downloaded)
				{
					HANDLE hfile = CreateFile (pcomponent->filepath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

					if (hfile != INVALID_HANDLE_VALUE)
					{
						if (_r_inet_downloadurl (hsession, proxy_addr, pcomponent->url, (LONG_PTR)hfile, true, &this_ptr->UpdateDownloadCallback, (LONG_PTR)pupdateinfo) == ERROR_SUCCESS)
						{
							pcomponent->is_downloaded = true;
							pcomponent->is_haveupdates = false;

							is_downloaded = true;

							if (pcomponent->is_installer)
							{
								SAFE_DELETE_HANDLE (hfile);

								is_downloaded_installer = true;

								break;
							}
						}

						SAFE_DELETE_HANDLE (hfile);
					}
				}
			}

			_r_inet_close (hsession);
		}

		// show result text
		{
			WCHAR str_content[256] = {0};

			if (is_downloaded)
			{
				if (is_downloaded_installer)
				{
#ifdef IDS_UPDATE_INSTALL
					_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_INSTALL, nullptr));
#else
					_r_str_copy (str_content, _countof (str_content), L"Update available, do you want to install them?");
#pragma _R_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL
				}
				else
				{
#ifdef IDS_UPDATE_DONE
					_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_DONE, nullptr));
#else
					_r_str_copy (str_content, _countof (str_content), L"Downloading update finished.");
#pragma _R_WARNING(IDS_UPDATE_DONE)
#endif // IDS_UPDATE_DONE
				}
			}
			else
			{
#ifdef IDS_UPDATE_ERROR
				_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_ERROR, nullptr));
#else
				_r_str_copy (str_content, _countof (str_content), L"Update server connection error");
#pragma _R_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
			}

			if (pupdateinfo->hparent)
			{
#ifdef _APP_NO_WINXP
				if (pupdateinfo->htaskdlg)
					this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, (is_downloaded ? nullptr : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
#else
				if (this_ptr->IsVistaOrLater ())
				{
					if (pupdateinfo->htaskdlg)
						this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, (is_downloaded ? nullptr : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
				}
				else
				{
					_r_msg (pupdateinfo->hparent, is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONEXCLAMATION), this_ptr->app_name, nullptr, L"%s", str_content);
				}
#endif // _APP_NO_WINXP
			}
		}
	}

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
			pupdateinfo->htaskdlg = hwnd;

			SendMessage (hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
			SendMessage (hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 10);

			if (pupdateinfo->hparent)
			{
				_r_wnd_center (hwnd, pupdateinfo->hparent);
				_r_wnd_top (hwnd, true);
			}

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			if (pupdateinfo->hthread)
				ResumeThread (pupdateinfo->hthread);

			break;
		}

		case TDN_DESTROYED:
		{
			SetEvent (pupdateinfo->hend);

			if (pupdateinfo->hthread)
			{
				TerminateThread (pupdateinfo->hthread, 0);
				SAFE_DELETE_HANDLE (pupdateinfo->hthread);
			}

			break;
		}

		case TDN_BUTTON_CLICKED:
		{
			if (wparam == IDYES)
			{
				rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

				WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_DOWNLOAD
				_r_str_printf (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_DOWNLOAD, nullptr), 0);
#else
				_r_str_copy (str_content, _countof (str_content), L"Downloading update...");
#pragma _R_WARNING(IDS_UPDATE_DOWNLOAD)
#endif
				pupdateinfo->hthread = (HANDLE)_r_createthread (&UpdateDownloadThread, (LPVOID)pupdateinfo, true);

				if (pupdateinfo->hthread)
				{
					this_ptr->UpdateDialogNavigate (hwnd, nullptr, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);

					return S_FALSE;
				}
			}
			else if (wparam == IDOK)
			{
				rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

				this_ptr->UpdateInstall ();

				DestroyWindow (this_ptr->GetHWND ());

				return S_FALSE;
			}

			break;
		}
	}

	return S_OK;
}

INT rapp::UpdateDialogNavigate (HWND htaskdlg, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata)
{
	TASKDIALOGCONFIG tdc = {0};

	WCHAR str_title[64] = {0};
	WCHAR str_main[128] = {0};
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

	_r_str_copy (str_title, _countof (str_title), app_name);

	if (main)
	{
		tdc.pszMainInstruction = str_main;
		_r_str_copy (str_main, _countof (str_main), main);
	}

	if (content)
	{
		tdc.pszContent = str_content;
		_r_str_copy (str_content, _countof (str_content), content);
	}

	INT button = 0;

	if (htaskdlg)
		SendMessage (htaskdlg, TDM_NAVIGATE_PAGE, 0, (LPARAM)&tdc);

	else
		_r_msg_taskdialog (&tdc, &button, nullptr, nullptr);

	return button;
}

rstring format_version (rstring vers)
{
	if (_r_str_isnumeric (vers))
		return _r_fmt_date (vers.AsLonglong (), FDTF_SHORTDATE | FDTF_SHORTTIME);

	return vers;
}

void rapp::UpdateInstall () const
{
	_r_run (_r_path_expand (L"%systemroot%\\system32\\cmd.exe"), _r_fmt (L"\"cmd.exe\" /c timeout 3 > nul&&start /wait \"\" \"%s\" /S /D=%s&&timeout 3 > nul&&del /q /f \"%s\"&start \"\" \"%s\"", GetUpdatePath (), GetDirectory (), GetUpdatePath (), GetBinaryPath ()), nullptr, SW_HIDE);
}

UINT WINAPI rapp::UpdateCheckThread (LPVOID lparam)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lparam;

	if (pupdateinfo)
	{
		rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

		// check for beta versions flag
#ifdef _APP_BETA
		const bool is_beta = true;
#else
		const bool is_beta = this_ptr->ConfigGet (L"CheckUpdatesBeta", false).AsBool ();
#endif // _APP_BETA

		rstring proxy_addr = this_ptr->GetProxyConfiguration ();
		HINTERNET hsession = _r_inet_createsession (this_ptr->GetUserAgent (), proxy_addr);

		if (hsession)
		{
			rstring buffer;

			if (_r_inet_downloadurl (hsession, proxy_addr, _r_fmt (_APP_WEBSITE_URL L"/update.php?product=%s&is_beta=%d&api=3", this_ptr->app_name_short, is_beta), (LONG_PTR)&buffer, false, nullptr, 0) != ERROR_SUCCESS)
			{
				if (pupdateinfo->hparent)
				{
					WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_ERROR
					_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_ERROR, nullptr).GetString ());
#else
					_r_str_copy (str_content, _countof (str_content), L"Update server connection error.");
#pragma _R_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR

#ifdef _APP_NO_WINXP
					this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
#else
					if (this_ptr->IsVistaOrLater ())
					{
						this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, TD_WARNING_ICON, 0, TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
					}
					else
					{
						_r_msg (pupdateinfo->hparent, MB_OK | MB_ICONWARNING, this_ptr->app_name, nullptr, L"%s", str_content);
					}
#endif // _APP_NO_WINXP
				}
			}
			else
			{
				rstringmap1 result;

				if (_r_str_unserialize (buffer, L';', L'=', &result))
				{
					bool is_updateavailable = false;
					rstring updates_text;

					for (size_t i = 0; i < pupdateinfo->components.size (); i++)
					{
						PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

						if (pcomponent && !_r_str_isempty (pcomponent->short_name) && result.find (pcomponent->short_name) != result.end ())
						{
							const rstring& rlink = result[pcomponent->short_name];
							const size_t split_pos = _r_str_find (rlink, rlink.GetLength (), L'|');

							if (split_pos == INVALID_SIZE_T)
								continue;

							const rstring new_version = _r_str_extract (rlink, rlink.GetLength (), 0, split_pos);
							const rstring new_url = _r_str_extract (rlink, rlink.GetLength (), split_pos + 1);

							if (!new_version.IsEmpty () && !new_url.IsEmpty () && (_r_str_isnumeric (new_version) ? (wcstoll (pcomponent->version, nullptr, 10) < new_version.AsLonglong ()) : (_r_str_versioncompare (pcomponent->version, new_version) == -1)))
							{
								is_updateavailable = true;

								_r_str_alloc (&pcomponent->new_version, new_version.GetLength (), new_version);
								_r_str_alloc (&pcomponent->url, new_url.GetLength (), new_url);

								pcomponent->is_haveupdates = true;

								if (pcomponent->is_installer)
								{
									_r_str_alloc (&pcomponent->filepath, _r_str_length (this_ptr->GetUpdatePath ()), this_ptr->GetUpdatePath ());
								}
								else
								{
									rstring path;
									path.Format (L"%s\\%s-%s-%s.tmp", _r_path_expand (L"%temp%\\").GetString (), this_ptr->app_name_short, pcomponent->short_name, new_version.GetString ());

									_r_str_alloc (&pcomponent->filepath, path.GetLength (), path);
									_r_str_alloc (&pcomponent->version, new_version.GetLength (), new_version);
								}

								updates_text.AppendFormat (L"%s %s\r\n", pcomponent->full_name, format_version (new_version).GetString ());

								// do not check components when new version of application available
								if (pcomponent->is_installer)
									break;
							}
						}
					}

					if (is_updateavailable)
					{
						WCHAR str_main[256] = {0};

						_r_str_trim (updates_text, L"\r\n ");

#ifdef IDS_UPDATE_YES
						_r_str_copy (str_main, _countof (str_main), this_ptr->LocaleString (IDS_UPDATE_YES, nullptr));
#else
						_r_str_copy (str_main, _countof (str_main), L"Update available, download and install them?");
#pragma _R_WARNING(IDS_UPDATE_YES)
#endif // IDS_UPDATE_YES

#ifdef _APP_NO_WINXP
						this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, nullptr, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)pupdateinfo);

						WaitForSingleObjectEx (pupdateinfo->hend, INFINITE, FALSE);
#else
						if (this_ptr->IsVistaOrLater ())
						{
							this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, nullptr, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)pupdateinfo);

							WaitForSingleObjectEx (pupdateinfo->hend, INFINITE, FALSE);
						}
						else
						{
							const INT msg_id = _r_msg (pupdateinfo->hparent, MB_YESNO | MB_USERICON, this_ptr->app_name, str_main, L"%s", updates_text.GetString ());

							if (msg_id == IDYES)
							{
								pupdateinfo->hthread = _r_createthread (&this_ptr->UpdateDownloadThread, (LPVOID)pupdateinfo, true);

								if (pupdateinfo->hthread)
								{
									ResumeThread (pupdateinfo->hthread);
									WaitForSingleObjectEx (pupdateinfo->hend, INFINITE, FALSE);
								}
							}
						}
#endif // _APP_NO_WINXP
						for (size_t i = 0; i < pupdateinfo->components.size (); i++)
						{
							PAPP_UPDATE_COMPONENT pcomponent = pupdateinfo->components.at (i);

							if (pcomponent)
							{
								if (pcomponent->is_downloaded)
								{
									if (!pcomponent->is_installer)
									{
										SetFileAttributes (pcomponent->target_path, FILE_ATTRIBUTE_NORMAL);
										_r_fs_move (pcomponent->filepath, pcomponent->target_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
										_r_fs_delete (pcomponent->filepath, false);

										this_ptr->ConfigInit (); // reload configuration

										if (this_ptr->GetHWND ())
										{
											SendMessage (this_ptr->GetHWND (), RM_CONFIG_UPDATE, 0, 0);
											SendMessage (this_ptr->GetHWND (), RM_INITIALIZE, 0, 0);
											SendMessage (this_ptr->GetHWND (), RM_LOCALIZE, 0, 0);

											DrawMenuBar (this_ptr->GetHWND ());
										}
									}
								}
							}
						}
					}
					else
					{
						if (pupdateinfo->htaskdlg)
						{
							WCHAR str_content[256] = {0};
#ifdef IDS_UPDATE_NO
							_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_NO, nullptr).GetString ());
#else
							_r_str_copy (str_content, _countof (str_content), L"No updates available.");
#pragma _R_WARNING(IDS_UPDATE_NO)
#endif // IDS_UPDATE_NO

							this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, nullptr, 0, TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);
						}
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", _r_unixtime_now ());
			}

			_r_inet_close (hsession);
		}
	}

	_endthreadex (ERROR_SUCCESS);

	return ERROR_SUCCESS;
}
#endif // _APP_HAVE_UPDATES

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

	ITaskService *service = nullptr;
	ITaskFolder *folder = nullptr;
	ITaskDefinition *taskdef = nullptr;
	IRegistrationInfo *reginfo = nullptr;
	IPrincipal *principal = nullptr;
	ITaskSettings *settings = nullptr;
	IActionCollection *action_collection = nullptr;
	IAction *action = nullptr;
	IExecAction *exec_action = nullptr;
	IRegisteredTask *registered_task = nullptr;

	MBSTR root (L"\\");
	MBSTR name (_r_fmt (_APP_TASKSCHD_NAME, app_name_short));
	MBSTR author (_APP_AUTHOR);
	MBSTR path (GetBinaryPath ());
	MBSTR directory (GetDirectory ());
	MBSTR args (L"$(Arg0)");
	MBSTR timelimit (L"PT0S");

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID *)&service)))
	{
		if (SUCCEEDED (service->Connect (vtEmpty, vtEmpty, vtEmpty, vtEmpty)))
		{
			if (SUCCEEDED (service->GetFolder (root, &folder)))
			{
				if (is_enable)
				{
					// create task
					if (SUCCEEDED (service->NewTask (0, &taskdef)))
					{
						if (SUCCEEDED (taskdef->get_RegistrationInfo (&reginfo)))
						{
							reginfo->put_Author (author);
							reginfo->Release ();
						}

						if (SUCCEEDED (taskdef->get_Principal (&principal)))
						{
							principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
							principal->Release ();
						}

						if (SUCCEEDED (taskdef->get_Settings (&settings)))
						{
							settings->put_AllowHardTerminate (VARIANT_TRUE);
							settings->put_DisallowStartIfOnBatteries (VARIANT_FALSE);
							settings->put_StartWhenAvailable (VARIANT_FALSE);
							settings->put_StopIfGoingOnBatteries (VARIANT_FALSE);
							settings->put_ExecutionTimeLimit (timelimit);
							settings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
							settings->put_Priority (4); // NORMAL_PRIORITY_CLASS

							// set compatibility (win7+)
							if (_r_sys_validversion (6, 1))
							{
								// TASK_COMPATIBILITY_V2_2 - win8
								// TASK_COMPATIBILITY_V2_1 - win7

								for (INT i = TASK_COMPATIBILITY_V2_4; i != TASK_COMPATIBILITY_V1; i--)
								{
									if (SUCCEEDED (settings->put_Compatibility ((TASK_COMPATIBILITY)i)))
										break;
								}
							}

							settings->Release ();
						}

						if (SUCCEEDED (taskdef->get_Actions (&action_collection)))
						{
							if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
							{
								if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID *)&exec_action)))
								{
									exec_action->put_Path (path);
									exec_action->put_WorkingDirectory (directory);
									exec_action->put_Arguments (args);

									exec_action->Release ();
								}

								action->Release ();
							}

							action_collection->Release ();
						}

						if (SUCCEEDED (folder->RegisterTaskDefinition (name, taskdef, TASK_CREATE_OR_UPDATE, vtEmpty, vtEmpty, TASK_LOGON_INTERACTIVE_TOKEN, vtEmpty, &registered_task)))
						{
							ConfigSet (L"SkipUacIsEnabled", true);
							result = true;

							registered_task->Release ();
						}

						taskdef->Release ();
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

	return result;
}

bool rapp::SkipUacRun ()
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

	bool result = false;

	ITaskService *service = nullptr;
	ITaskFolder *folder = nullptr;
	IRegisteredTask *registered_task = nullptr;

	ITaskDefinition *task = nullptr;
	IActionCollection *action_collection = nullptr;
	IAction *action = nullptr;
	IExecAction *exec_action = nullptr;

	IRunningTask *running_task = nullptr;

	MBSTR root (L"\\");
	MBSTR name (_r_fmt (_APP_TASKSCHD_NAME, app_name_short));

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID *)&service)))
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
								if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID *)&exec_action)))
								{
									BSTR path = nullptr;
									exec_action->get_Path (&path);

									PathUnquoteSpaces (path);

									// check path is to current module
									if (_r_str_compare (path, GetBinaryPath ()) == 0)
									{
										rstring args;

										// get arguments
										{
											INT numargs = 0;
											LPWSTR *arga = CommandLineToArgvW (GetCommandLine (), &numargs);

											for (INT i = 1; i < numargs; i++)
												args.AppendFormat (L"%s ", arga[i]);

											SAFE_LOCAL_FREE (arga);
										}

										_r_str_trim (args, L" ");

										variant_t ticker = args;

										if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_AS_SELF | TASK_RUN_IGNORE_CONSTRAINTS, 0, nullptr, &running_task)))
										{
											DWORD attempts = 6;

											do
											{
												_r_sleep (250);

												TASK_STATE state;

												running_task->Refresh ();

												if (SUCCEEDED (running_task->get_State (&state)))
												{
													if (state == TASK_STATE_RUNNING || state == TASK_STATE_DISABLED)
													{
														if (state == TASK_STATE_RUNNING)
															result = true;

														break;
													}
												}
											}
											while (attempts--);

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

	return result;
}
#endif // _APP_HAVE_SKIPUAC

bool rapp::RunAsAdmin ()
{
#ifdef _APP_HAVE_SKIPUAC
	if (SkipUacRun ())
		return true;
#endif // _APP_HAVE_SKIPUAC

	SHELLEXECUTEINFO shex = {0};

	WCHAR path[MAX_PATH] = {0};
	_r_str_copy (path, _countof (path), GetBinaryPath ());

	WCHAR directory[MAX_PATH] = {0};
	_r_str_copy (directory, _countof (directory), GetDirectory ());

	WCHAR args[MAX_PATH] = {0};
	_r_str_copy (args, _countof (args), GetCommandLine ());

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
		return true;
	}
	else
	{
		if (is_mutexdestroyed)
			MutexCreate ();

		_r_sleep (250);
	}

	return false;
}
