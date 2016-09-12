// routine++
// Copyright (c) 2012-2016 Henry++

#include "rapp.h"

#ifdef _APP_HAVE_TRAY
CONST UINT WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

rapp::rapp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// initialize controls
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	InitCommonControlsEx (&icex);

	// general information
	StringCchCopy (app_name, _countof (app_name), name);
	StringCchCopy (app_name_short, _countof (app_name_short), short_name);
	StringCchCopy (app_version, _countof (app_version), version);
	StringCchCopy (app_copyright, _countof (app_copyright), copyright);

	// get hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// get current directory
	GetModuleFileName (nullptr, app_directory, _countof (app_directory));
	PathRemoveFileSpec (app_directory);

	// get configuration path
	StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_directory, app_name_short);

	if (!_r_fs_exists (app_config_path))
	{
		ExpandEnvironmentStrings (_r_fmt (L"%%APPDATA%%\\%s\\%s", _APP_AUTHOR, app_name), app_profile_directory, _countof (app_profile_directory));
		StringCchPrintf (app_config_path, _countof (app_config_path), L"%s\\%s.ini", app_profile_directory, app_name_short);
	}
	else
	{
		StringCchCopy (app_profile_directory, _countof (app_profile_directory), app_directory);
	}

	HDC h = GetDC (nullptr);

	// get dpi
	dpi_percent = DOUBLE (GetDeviceCaps (h, LOGPIXELSX)) / 96.0f;

	ReleaseDC (nullptr, h);

	// load settings
	ConfigInit ();
}

rapp::~rapp ()
{
	if (app_callback)
		app_callback (app_hwnd, _RM_UNINITIALIZE, nullptr, nullptr);

	if (app_mutex)
	{
		CloseHandle (app_mutex);
		app_mutex = nullptr;
	}

#ifndef _APP_NO_SETTINGS
	ClearSettingsPage ();
#endif // _APP_NO_SETTINGS
}

BOOL rapp::Initialize ()
{
	// check mutex
	if (app_mutex)
		CloseHandle (app_mutex);

	app_mutex = CreateMutex (nullptr, FALSE, app_name_short);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		HWND h = FindWindowEx (nullptr, nullptr, nullptr, app_name);

		if (h)
		{
			_r_wnd_toggle (h, TRUE);
			return FALSE;
		}

		CloseHandle (app_mutex);
		app_mutex = nullptr;
	}

#ifdef _APP_HAVE_SKIPUAC
	if (_r_sys_uacstate () && SkipUacRun ())
	{
		return FALSE;
	}
#endif // _APP_HAVE_SKIPUAC

#ifndef _WIN64
	if (_r_sys_iswow64 ())
	{
		_r_msg (nullptr, MB_OK | MB_ICONEXCLAMATION, app_name, nullptr, L"WARNING! 32-bit executable may incompatible with 64-bit operating system version!");
	}
#endif // _WIN64

	is_vistaorlater = _r_sys_validversion (6, 0);

	return TRUE;
}

#ifdef _APP_HAVE_AUTORUN
VOID rapp::AutorunCreate (BOOL is_remove)
{
	HKEY key = nullptr;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS)
	{
		if (is_remove)
		{
			RegDeleteValue (key, app_name);
		}
		else
		{
			WCHAR buffer[MAX_PATH] = {0};

			GetModuleFileName (nullptr, buffer, _countof (buffer));
			PathQuoteSpaces (buffer);

			StringCchCat (buffer, _countof (buffer), L" ");
			StringCchCat (buffer, _countof (buffer), L"/minimized");

			RegSetValueEx (key, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) + 1) * sizeof (WCHAR)));
		}

		RegCloseKey (key);
	}
}

BOOL rapp::AutorunIsPresent ()
{
	HKEY key = nullptr;
	BOOL result = FALSE;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		WCHAR path1[MAX_PATH] = {0};

		DWORD size = MAX_PATH;

		result = (RegQueryValueEx (key, app_name, nullptr, nullptr, (LPBYTE)path1, &size) == ERROR_SUCCESS);

		if (result)
		{
			WCHAR path2[MAX_PATH] = {0};

			PathRemoveArgs (path1);
			PathUnquoteSpaces (path1);

			GetModuleFileName (GetHINSTANCE (), path2, _countof (path2));

			// check path is to current module
			result = _wcsicmp (path1, path2) == 0;
		}

		RegCloseKey (key);
	}

	return result;
}
#endif // _APP_HAVE_AUTORUN

#ifndef _APP_NO_UPDATES
VOID rapp::CheckForUpdates (BOOL is_periodical)
{
	if (is_periodical)
	{
		if (!ConfigGet (L"CheckUpdates", 1).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", 0).AsLonglong ()) <= _APP_UPDATE_PERIOD)
		{
			return;
		}
	}

	is_update_forced = is_periodical;

	_beginthreadex (nullptr, 0, &CheckForUpdatesProc, (LPVOID)this, 0, nullptr);
}
#endif // _APP_NO_UPDATES

VOID rapp::ConfigInit ()
{
	app_config_array.clear (); // reset

	ParseINI (app_config_path, &app_config_array);

	LocaleInit ();

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

BOOL rapp::IsVistaOrLater ()
{
	return is_vistaorlater;
}

BOOL rapp::IsClassicUI ()
{
	return is_classic;
}

VOID rapp::SetIcon (UINT icon_id)
{
	if (app_icon_1)
		DestroyIcon (app_icon_1);

	if (app_icon_2)
		DestroyIcon (app_icon_2);

	app_icon_1 = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXSMICON));
	app_icon_2 = _r_loadicon (GetHINSTANCE (), MAKEINTRESOURCE (icon_id), GetSystemMetrics (SM_CXICON));

	SendMessage (app_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app_icon_1);
	SendMessage (app_hwnd, WM_SETICON, ICON_BIG, (LPARAM)app_icon_2);
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
			this_ptr->is_classic = !IsThemeActive () || this_ptr->ConfigGet (L"ClassicUI", 0).AsBool ();

			return FALSE;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
	}

	return CallWindowProc (this_ptr->app_wndproc, hwnd, msg, wparam, lparam);
}

BOOL rapp::CreateMainWindow (DLGPROC proc, APPLICATION_CALLBACK callback)
{
	BOOL result = FALSE;

	if (Initialize ())
	{
		if (ConfigGet (L"ClassicUI", 0).AsBool () || !IsThemeActive ())
		{
			is_classic = TRUE;

			if (ConfigGet (L"ClassicUI", 0).AsBool ())
				SetThemeAppProperties (1);
		}

		// create window
#ifdef IDD_MAIN
		app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc);

		// enable messages bypass uipi
#ifdef _APP_HAVE_TRAY
		_r_wnd_changemessagefilter (app_hwnd, WM_TASKBARCREATED, MSGFLT_ALLOW);
#endif // _APP_HAVE_TRAY

		_r_wnd_changemessagefilter (app_hwnd, WM_DROPFILES, MSGFLT_ALLOW);
		_r_wnd_changemessagefilter (app_hwnd, WM_COPYDATA, MSGFLT_ALLOW);
		_r_wnd_changemessagefilter (app_hwnd, 0x0049, MSGFLT_ALLOW); // WM_COPYGLOBALDATA

		SetWindowLongPtr (app_hwnd, GWLP_USERDATA, (LONG_PTR)this);
		app_wndproc = (WNDPROC)SetWindowLongPtr (app_hwnd, GWLP_WNDPROC, (LONG_PTR)this->MainWindowProc);
#else
		UNREFERENCED_PARAMETER (proc);
#endif // IDD_MAIN

		if (app_hwnd)
		{
			// set title
			SetWindowText (app_hwnd, app_name);

			// set on top
			_r_wnd_top (app_hwnd, ConfigGet (L"AlwaysOnTop", 0).AsBool ());

			// set icons
#ifdef IDI_MAIN
			SetIcon (IDI_MAIN);
#endif // IDI_MAIN

			// check for updates
#ifndef _APP_NO_UPDATES
			CheckForUpdates (TRUE);
#endif // _APP_NO_UPDATES

			// initialization callback
			if (callback)
			{
				app_callback = callback;
				app_callback (app_hwnd, _RM_INITIALIZE, nullptr, nullptr);
				app_callback (app_hwnd, _RM_LOCALIZE, nullptr, nullptr);
			}

			result = TRUE;
		}
		else
		{
			result = FALSE;
		}
	}

	return result;
}

#ifdef _APP_HAVE_TRAY
BOOL rapp::TrayCreate (UINT id, UINT code, HICON h)
{
	BOOL result = FALSE;

	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uVersion = IsVistaOrLater () ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
	nid.hWnd = app_hwnd;
	nid.uID = id;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_SHOWTIP | NIF_TIP;
	nid.uCallbackMessage = code;
	nid.hIcon = h;
	StringCchCopy (nid.szTip, _countof (nid.szTip), app_name);

	result = Shell_NotifyIcon (NIM_ADD, &nid);
	Shell_NotifyIcon (NIM_SETVERSION, &nid);

	return result;
}

BOOL rapp::TrayDestroy (UINT id)
{
	nid.cbSize = IsVistaOrLater () ? sizeof (nid) : NOTIFYICONDATA_V3_SIZE;
	nid.uID = id;

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

	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_RESPECT_QUIET_TIME | NIIF_LARGE_ICON | icon;

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
	PAPPLICATION_PAGE ptr = new APPLICATION_PAGE;

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
		PAPPLICATION_PAGE ptr = app_settings_pages.at (i);

		delete ptr;
	}

	app_settings_pages.clear ();
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
		PAPPLICATION_PAGE ptr = app_settings_pages.at (i);

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

		if (ptr->dlg_id)
			ptr->callback (ptr->hwnd, _RM_INITIALIZE, nullptr, ptr);
	}

	_r_ctrl_enable (hwnd, IDC_APPLY, FALSE);
}
#endif // _APP_NO_SETTINGS

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

VOID rapp::SetHWND (HWND hwnd)
{
	app_hwnd = hwnd;
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
		app_callback (GetHWND (), _RM_LOCALIZE, nullptr, nullptr);
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
	HANDLE h = FindFirstFile (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\*.ini", app_directory), &wfd);

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
				{
					CheckMenuRadioItem (hmenu, id_start, id_start + app_locale_count, id_start + app_locale_count, MF_BYCOMMAND);
				}
			}
			else
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, ++app_locale_count, (LPARAM)fname);

				if (!def.IsEmpty () && def.CompareNoCase (fname) == 0)
				{
					SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, app_locale_count, 0);
				}
			}
		}
		while (FindNextFile (h, &wfd));

		FindClose (h);
	}
	else
	{
		if (is_menu)
		{
			EnableMenuItem ((HMENU)hwnd, ctrl_id, MF_BYPOSITION | MF_DISABLED);
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
	{
		is_localized = ParseINI (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\%s.ini", GetDirectory (), name), &app_locale_array);
	}
}

rstring rapp::LocaleString (HINSTANCE h, UINT uid, LPCWSTR name)
{
	rstring result;

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

			PAPPLICATION_PAGE ptr = this_ptr->app_settings_pages.at (this_ptr->ConfigGet (L"SettingsLastPage", 0).AsSizeT ());

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
				PAPPLICATION_PAGE ptr = this_ptr->app_settings_pages.at (i);

				if (ptr->dlg_id)
					ptr->hwnd = CreateDialogParam (ptr->h, MAKEINTRESOURCE (ptr->dlg_id), hwnd, &this_ptr->SettingsPagesProc, (LPARAM)this_ptr);

				ptr->item = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (ptr->h, ptr->locale_id, ptr->locale_sid), ptr->group_id == LAST_VALUE ? nullptr : this_ptr->app_settings_pages.at (ptr->group_id)->item, LAST_VALUE, (LPARAM)i);

				if (this_ptr->ConfigGet (L"SettingsLastPage", 0).AsSizeT () == i)
					SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)ptr->item);
			}

			this_ptr->InitSettingsPage (hwnd, TRUE);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			if (lphdr->idFrom == IDC_NAV)
			{
				switch (lphdr->code)
				{
					case TVN_SELCHANGING:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						PAPPLICATION_PAGE ptr1 = this_ptr->app_settings_pages.at (size_t (pnmtv->itemNew.lParam));

						if (!ptr1->dlg_id)
						{
							SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)this_ptr->app_settings_pages.at (size_t (pnmtv->itemNew.lParam) + 1)->item);
							return TRUE;
						}

						break;
					}

					case TVN_SELCHANGED:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						size_t old_id = size_t (pnmtv->itemOld.lParam);
						size_t new_id = size_t (pnmtv->itemNew.lParam);

						if (this_ptr->app_settings_pages.at (old_id)->hwnd)
							ShowWindow (this_ptr->app_settings_pages.at (old_id)->hwnd, SW_HIDE);

						if (this_ptr->app_settings_pages.at (new_id)->hwnd)
							ShowWindow (this_ptr->app_settings_pages.at (new_id)->hwnd, SW_SHOW);

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
					BOOL is_newlocale = FALSE;

					_r_ctrl_enable (hwnd, IDC_APPLY, FALSE);

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						PAPPLICATION_PAGE ptr = this_ptr->app_settings_pages.at (i);

						if (ptr->dlg_id)
						{
							if (ptr->callback (ptr->hwnd, _RM_SAVE, nullptr, ptr))
								is_newlocale = TRUE;
						}
					}

					this_ptr->ConfigInit (); // re-read settings

					// reinitialization
					if (this_ptr->app_callback)
					{
						this_ptr->app_callback (this_ptr->app_hwnd, _RM_UNINITIALIZE, nullptr, nullptr);
						this_ptr->app_callback (this_ptr->app_hwnd, _RM_INITIALIZE, nullptr, nullptr);

						if (is_newlocale)
							this_ptr->app_callback (this_ptr->app_hwnd, _RM_LOCALIZE, nullptr, nullptr);
					}

					this_ptr->InitSettingsPage (hwnd, is_newlocale);

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
						this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, _RM_UNINITIALIZE, nullptr, this_ptr->app_settings_pages.at (i)); // call closed state

					_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", 0).AsBool ());

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

	if (this_ptr)
	{
		BOOL result = FALSE;
		HINTERNET internet = nullptr, connect = nullptr;

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED);
#endif // IDM_CHECKUPDATES

		internet = InternetOpen (this_ptr->GetUserAgent (), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);

		if (internet)
		{
			connect = InternetOpenUrl (internet, _r_fmt (L"%s/update.php?product=%s", _APP_WEBSITE_URL, this_ptr->app_name_short), nullptr, 0, INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_NO_COOKIES, 0);

			if (connect)
			{
				DWORD dwStatus = 0, dwStatusSize = sizeof (dwStatus);
				HttpQueryInfo (connect, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, nullptr);

				if (dwStatus == HTTP_STATUS_OK)
				{
					DWORD out = 0;

					CHAR buffer[_R_BUFFER_LENGTH] = {0};

					rstring bufferw;

					while (TRUE)
					{
						if (!InternetReadFile (connect, buffer, _R_BUFFER_LENGTH - 1, &out) || !out)
						{
							break;
						}

						buffer[out] = 0;

						bufferw.Append (buffer);
					}

					bufferw.Trim (L" \r\n");

					if (_r_str_versioncompare (this_ptr->app_version, bufferw) == -1)
					{
						if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, nullptr, I18N (this_ptr, IDS_UPDATE_YES, 0), bufferw) == IDYES)
						{
							ShellExecute (nullptr, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
						}

						result = TRUE;
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", _r_unixtime_now ());
			}
		}

#ifdef IDM_CHECKUPDATES
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // IDM_CHECKUPDATES

		if (!result && !this_ptr->is_update_forced)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, nullptr, I18N (this_ptr, IDS_UPDATE_NO, 0));
		}

		InternetCloseHandle (connect);
		InternetCloseHandle (internet);
	}

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
BOOL rapp::SkipUacCreate (BOOL is_remove)
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
					if (is_remove)
					{
						result = (folder->DeleteTask (name.GetBuffer (), 0) == S_OK);
					}
					else
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

										GetModuleFileName (nullptr, path, _countof (path));
										PathQuoteSpaces (path);

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

					folder->Release ();
				}
			}

			service->Release ();
		}

		CoUninitialize ();
	}

	return result;
}

BOOL rapp::SkipUacIsPresent (BOOL is_run)
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
										BSTR path1 = nullptr;
										WCHAR path2[MAX_PATH] = {0};

										exec_action->get_Path (&path1);
										PathUnquoteSpaces (path1);

										GetModuleFileName (GetHINSTANCE (), path2, _countof (path2));

										// check path is to current module
										if (_wcsicmp (path1, path2) == 0)
										{
											if (is_run)
											{
												INT numargs = 0;
												LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);
												rstring args;

												for (INT i = 1; i < numargs; i++)
												{
													args.Append (arga[i]);
													args.Append (L" ");
												}

												LocalFree (arga);

												variant_t ticker = args.Trim (L" ");

												if (SUCCEEDED (registered_task->RunEx (ticker, TASK_RUN_AS_SELF, 0, nullptr, &running_task)) && running_task)
												{
													TASK_STATE state;
													INT count = 5; // try count

													do
													{
														Sleep (500);

														running_task->Refresh ();
														running_task->get_State (&state);

														if (state == TASK_STATE_RUNNING || state == TASK_STATE_DISABLED)
														{
															if (state == TASK_STATE_RUNNING)
															{
																result = TRUE;
															}
															break;
														}
													}
													while (count--);

													running_task->Release ();
												}
											}
											else
											{
												result = TRUE;
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

BOOL rapp::SkipUacRun ()
{
	BOOL result = FALSE;

	if (_r_sys_uacstate ())
	{
		CloseHandle (app_mutex);
		app_mutex = nullptr;

#ifdef _APP_HAVE_SKIPUAC
		result = SkipUacIsPresent (TRUE);
#endif // _APP_HAVE_SKIPUAC

		if (!result)
		{
			WCHAR buffer[MAX_PATH] = {0};

			SHELLEXECUTEINFO shex = {0};

			shex.cbSize = sizeof (shex);
			shex.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI;
			shex.lpVerb = L"runas";
			shex.nShow = SW_NORMAL;

			GetModuleFileName (nullptr, buffer, _countof (buffer));
			shex.lpFile = buffer;

			if (ShellExecuteEx (&shex))
				result = TRUE;
		}

		if (!result)
			app_mutex = CreateMutex (nullptr, FALSE, app_name_short);
	}
	else
	{
		result = TRUE;
	}

	return result;
}

