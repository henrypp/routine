// application helper library
// Copyright (c) 2013-2015 Henry++

#include "app_helper.h"

CApp::CApp (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// initialize controls
	INITCOMMONCONTROLSEX icex = {0};

	icex.dwSize = sizeof (icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	InitCommonControlsEx (&icex);

	// check mutex
	this->app_mutex = CreateMutex (nullptr, FALSE, short_name);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		HWND h = FindWindowEx (nullptr, nullptr, nullptr, name);

		if (h)
		{
			_r_windowtoggle (h, TRUE);
		}

		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;
	}
	else
	{
		// general information
		StringCchCopy (this->app_name, _countof (this->app_name), name);
		StringCchCopy (this->app_name_short, _countof (this->app_name_short), short_name);
		StringCchCopy (this->app_version, _countof (this->app_version), version);
		StringCchCopy (this->app_copyright, _countof (this->app_copyright), copyright);

#ifdef _APP_NO_UAC

		if (_r_system_uacstate () && this->SkipUacRun ())
		{
			return;
		}

#endif // _APP_NO_UAC

#ifndef _WIN64

		if (_r_system_iswow64 ())
		{
			_r_msg (nullptr, MB_OK | MB_ICONEXCLAMATION, name, L"WARNING! 32-bit executable may incompatible with 64-bit operating system version!");
		}

#endif // _WIN64

		this->is_initialized = TRUE;

		// get hinstance
		this->app_hinstance = GetModuleHandle (nullptr);

		// get current directory
		GetModuleFileName (nullptr, this->app_directory, _countof (this->app_directory));
		PathRemoveFileSpec (this->app_directory);

		// get configuration path
		StringCchPrintf (this->app_config_path, _countof (this->app_config_path), L"%s\\%s.ini", this->app_directory, this->app_name_short);

		if (!_r_file_is_exists (this->app_config_path))
		{
			ExpandEnvironmentStrings (_r_fmt (L"%%APPDATA%%\\%s\\%s", _APP_AUTHOR, this->app_name), this->app_profile_directory, _countof (this->app_profile_directory));
			StringCchPrintf (this->app_config_path, _countof (this->app_config_path), L"%s\\%s.ini", this->app_profile_directory, this->app_name_short);
		}
		else
		{
			StringCchCopy (this->app_profile_directory, _countof (this->app_profile_directory), this->app_directory);
		}

		HDC h = GetDC (nullptr);

		// get dpi
		this->dpi_percent = (DOUBLE)GetDeviceCaps (h, LOGPIXELSX) / 96.0f;

#ifndef _APP_NO_ABOUT

		// create window class
		WNDCLASSEX wcx = {0};

		wcx.cbSize = sizeof (WNDCLASSEX);
		wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wcx.lpfnWndProc = this->AboutWndProc;
		wcx.hInstance = this->GetHINSTANCE ();
		wcx.lpszClassName = _APP_ABOUT_CLASS;
		wcx.hbrBackground = GetSysColorBrush (COLOR_3DFACE);

		if (!GetClassInfoEx (this->GetHINSTANCE (), _APP_ABOUT_CLASS, nullptr))
		{
			RegisterClassEx (&wcx);
		}

		// create dialog font
		LOGFONT lf = {0};

		lf.lfQuality = CLEARTYPE_QUALITY;
		lf.lfCharSet = DEFAULT_CHARSET;
		lf.lfPitchAndFamily = FF_DONTCARE;
		lf.lfWeight = FW_NORMAL;
		lf.lfHeight = -MulDiv (9, GetDeviceCaps (h, LOGPIXELSY), 72);
		StringCchCopy (lf.lfFaceName, _countof (lf.lfFaceName), L"MS Shell Dlg 2");

		this->app_font = CreateFontIndirect (&lf);

#endif // _APP_NO_ABOUT

		ReleaseDC (nullptr, h);

		// get settings
		this->ConfigInit ();
		this->LocaleInit (this->ConfigGet (L"Language", nullptr));

		// get logo
		this->app_logo = _r_loadicon (this->GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), this->GetDPI (64));
	}
}

CApp::~CApp ()
{
	if (this->app_mutex)
	{
		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;
	}

#ifndef _APP_NO_ABOUT

	if (this->app_logo)
	{
		DestroyIcon (this->app_logo);
		this->app_logo = nullptr;
	}

	if (this->app_font)
	{
		DeleteObject(this->app_font);
		this->app_font = nullptr;
	}

	UnregisterClass (_APP_ABOUT_CLASS, this->GetHINSTANCE ());

#endif // _APP_NO_ABOUT

#ifndef _APP_NO_SETTINGS

	for (size_t i = 0; i < this->app_settings_pages.size (); i++)
	{
		PAPPLICATION_PAGE ptr = this->app_settings_pages.at (i);

		delete ptr;
	}

#endif // _APP_NO_SETTINGS
}

VOID CApp::AutorunCreate (BOOL is_remove)
{
	HKEY key = nullptr;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE | KEY_READ, &key) == ERROR_SUCCESS)
	{
		if (is_remove)
		{
			RegDeleteValue (key, this->app_name);
		}
		else
		{
			WCHAR buffer[MAX_PATH] = {0};

			GetModuleFileName (nullptr, buffer, _countof (buffer));
			PathQuoteSpaces (buffer);

			StringCchCat (buffer, _countof (buffer), L" ");
			StringCchCat (buffer, _countof (buffer), L"/minimized");

			RegSetValueEx (key, this->app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((_r_str_length (buffer) + 1) * sizeof (WCHAR)));
		}

		RegCloseKey (key);
	}
}

BOOL CApp::AutorunIsPresent ()
{
	HKEY key = nullptr;
	BOOL result = FALSE;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		WCHAR path1[MAX_PATH] = {0};
		WCHAR path2[MAX_PATH] = {0};

		DWORD size = MAX_PATH;

		result = (RegQueryValueEx (key, this->app_name, nullptr, nullptr, (LPBYTE)path1, &size) == ERROR_SUCCESS);

		if (result)
		{
			PathRemoveArgs (path1);
			PathUnquoteSpaces (path1);

			GetModuleFileName (this->GetHINSTANCE (), path2, _countof (path2));

			// check path is to current module
			result = _wcsicmp (path1, path2) == 0;
		}

		RegCloseKey (key);
	}

	return result;
}

#ifndef _APP_NO_UPDATES

VOID CApp::CheckForUpdates (BOOL is_periodical)
{
	if (is_periodical)
	{
		if (!this->ConfigGet (L"CheckUpdates", 1) || (_r_unixtime_now () - this->ConfigGet (L"CheckUpdatesLast", 0)) <= (86400 * _APP_UPDATE_PERIOD))
		{
			return;
		}
	}

	this->is_update_forced = is_periodical;

	_beginthreadex (nullptr, 0, &this->CheckForUpdatesProc, (LPVOID)this, 0, nullptr);
}

#endif // _APP_NO_UPDATES

VOID CApp::ConfigInit ()
{
	this->app_config_array.clear (); // reset

	this->ParseINI (this->app_config_path, &this->app_config_array);
}

DWORD CApp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name)
{
	return wcstoul (this->ConfigGet (key, _r_fmt (L"%i", def), name), nullptr, 10);
}

CString CApp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name)
{
	if (!name)
	{
		name = this->app_name_short;
	}

	// check key is exists
	if (this->app_config_array.find (name) == this->app_config_array.end () || this->app_config_array[name].find (key) == this->app_config_array[name].end ())
	{
		return def;
	}

	return this->app_config_array[name][key];
}

BOOL CApp::ConfigSet (LPCWSTR key, LPCWSTR val, LPCWSTR name)
{
	if (!_r_file_is_exists (this->app_profile_directory))
	{
		SHCreateDirectoryEx (nullptr, this->app_profile_directory, nullptr);
	}

	if (!name)
	{
		name = this->app_name_short;
	}

	// update hash value
	this->app_config_array[name][key] = val;

	return WritePrivateProfileString (name, key, val, this->app_config_path);
}

BOOL CApp::ConfigSet (LPCWSTR key, DWORD val, LPCWSTR name)
{

#ifdef _WIN64
	return this->ConfigSet (key, _r_fmt (L"%lld", val), name);
#else
	return this->ConfigSet (key, _r_fmt (L"%ld", val, name));
#endif // _WIN64

}

#ifndef _APP_NO_ABOUT

VOID CApp::CreateAboutWindow ()
{
	MSG msg = {0};

#ifdef _WIN64
	const INT architecture = 64;
#else
	const INT architecture = 32;
#endif // _WIN64

	HWND hwnd = CreateWindowEx (WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, _APP_ABOUT_CLASS, L"About", WS_CAPTION | WS_SYSMENU | WS_BORDER, CW_USEDEFAULT, CW_USEDEFAULT, this->GetDPI (388), this->GetDPI (124), this->GetHWND (), nullptr, this->GetHINSTANCE (), nullptr);

	// create controls
	HWND hctrl = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ICON, this->GetDPI (12), this->GetDPI (12), this->GetDPI (64), this->GetDPI (64), hwnd, nullptr, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (LPARAM)this->app_logo);

	hctrl = CreateWindowEx (0, WC_STATIC, _r_fmt (L"%s v%s (%i-bit)", this->app_name, this->app_version, architecture), WS_CHILD | WS_VISIBLE, this->GetDPI (88), this->GetDPI (14), this->GetDPI (270), this->GetDPI (16), hwnd, nullptr, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)this->app_font, TRUE);

	hctrl = CreateWindowEx (0, WC_STATIC, this->app_copyright, WS_CHILD | WS_VISIBLE, this->GetDPI (88), this->GetDPI (36), this->GetDPI (270), this->GetDPI (16), hwnd, nullptr, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)this->app_font, TRUE);

	hctrl = CreateWindowEx (0, WC_LINK, _r_fmt (L"<a href=\"%s\">Website</a> | <a href=\"%s\">GitHub</a> | <a href=\"%s/%s/blob/master/LICENSE\">License agreement</a>", _APP_WEBSITE_URL, _APP_GITHUB_URL, _APP_GITHUB_URL, this->app_name_short), WS_CHILD | WS_VISIBLE, this->GetDPI (88), this->GetDPI (58), this->GetDPI (270), this->GetDPI (16), hwnd, nullptr, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)this->app_font, TRUE);

	ShowWindow (hwnd, SW_SHOW);
	UpdateWindow (hwnd);

	while (GetMessage (&msg, nullptr, 0, 0) > 0)
	{
		if (!IsDialogMessage (hwnd, &msg))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}
}

#endif // _APP_NO_ABOUT

BOOL CApp::CreateMainWindow (DLGPROC proc)
{
	BOOL result = FALSE;

	if (this->is_initialized)
	{
		// create window
		this->app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc);

		if (this->app_hwnd == nullptr)
		{
			result = FALSE;
		}
		else
		{
			// set title
			SetWindowText (this->GetHWND (), this->app_name);

			// set on top
			_r_windowtotop (this->GetHWND (), this->ConfigGet (L"AlwaysOnTop", 0));

			// set icons
			SendMessage (this->GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)_r_loadicon (this->GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), GetSystemMetrics (SM_CXSMICON)));
			SendMessage (this->GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)_r_loadicon (this->GetHINSTANCE (), MAKEINTRESOURCE (IDI_MAIN), GetSystemMetrics (SM_CXICON)));

			result = TRUE;
		}

		// check for updates
#ifndef _APP_NO_UPDATES

		this->CheckForUpdates (TRUE);

#endif // _APP_NO_UPDATES

	}

	return result;
}

#ifndef _APP_NO_SETTINGS

VOID CApp::CreateSettingsWindow ()
{
	DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), this->GetHWND (), this->SettingsWndProc, (LPARAM)this);
}

VOID CApp::AddSettingsPage (HINSTANCE h, UINT dlg_id, LPCWSTR title, APPLICATION_CALLBACK callback)
{
	PAPPLICATION_PAGE ptr = new APPLICATION_PAGE;

	if (ptr)
	{
		ptr->h = h;
		ptr->dlg_id = dlg_id;
		ptr->title = title;
		ptr->callback = callback;

		this->app_settings_pages.push_back (ptr);
	}
}

#endif // _APP_NO_SETTINGS

CString CApp::GetDirectory ()
{
	return this->app_directory;
}

CString CApp::GetProfileDirectory ()
{
	return this->app_profile_directory;
}

CString CApp::GetUserAgent ()
{
	return _r_fmt (L"%s/%s (+%s)", this->app_name, this->app_version, _APP_WEBSITE_URL);
}

INT CApp::GetDPI (INT val)
{
	return ceil (static_cast<DOUBLE>(val) * this->dpi_percent);
}

HINSTANCE CApp::GetHINSTANCE ()
{
	return this->app_hinstance;
}

HWND CApp::GetHWND ()
{
	return this->app_hwnd;
}

VOID CApp::Restart ()
{
	WCHAR buffer[MAX_PATH] = {0};
	GetModuleFileName (nullptr, buffer, _countof (buffer));

	ShowWindow (this->GetHWND (), SW_HIDE); // hide main window

	if (this->app_mutex)
	{
		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;
	}

	if (_r_run (buffer, nullptr))
	{
		if (this->GetHWND ())
		{
			DestroyWindow (this->GetHWND ());
		}

		ExitProcess (EXIT_SUCCESS);
	}
	else
	{
		ShowWindow (this->GetHWND (), SW_SHOW);

		this->app_mutex = CreateMutex (nullptr, FALSE, this->app_name_short);
	}
}

VOID CApp::SetHWND (HWND hwnd)
{
	this->app_hwnd = hwnd;
}

#ifndef _APP_NO_SETTINGS

VOID CApp::LocaleEnum (HWND hwnd, INT ctrl_id)
{
	WIN32_FIND_DATA wfd = {0};
	HANDLE h = FindFirstFile (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\*.ini", this->app_directory), &wfd);

	CString def = this->ConfigGet (L"Language", nullptr);

	if (h != INVALID_HANDLE_VALUE)
	{
		INT count = max (0, (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCOUNT, 0, 0));

		do
		{
			LPWSTR fname = wfd.cFileName;
			PathRemoveExtension (fname);

			SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, count, (LPARAM)fname);

			if (def.CompareNoCase (fname) == 0)
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, count, 0);
			}

			count += 1;
		}
		while (FindNextFile (h, &wfd));

		FindClose (h);
	}
	else
	{
		EnableWindow (GetDlgItem (hwnd, ctrl_id), FALSE);
	}
}

#endif // _APP_NO_SETTINGS

VOID CApp::LocaleInit (LPCWSTR name)
{
	if (this->ConfigGet (L"Language", nullptr).CompareNoCase (name) != 0)
	{
		this->ConfigSet (L"Language", name);
	}

	// reset
	this->app_locale_array.clear ();
	this->is_localized = FALSE;

	if (name)
	{
		if (this->ParseINI (_r_fmt (L"%s\\" _APP_I18N_DIRECTORY L"\\%s.ini", this->GetDirectory (), name), &this->app_locale_array))
		{
			this->is_localized = TRUE;
		}
	}
}

CString CApp::LocaleString (HINSTANCE h, UINT id, LPCWSTR name)
{
	CString buffer;

	if (this->is_localized)
	{
		// check key is exists
		if (this->app_locale_array[_APP_I18N_SECTION].find (name) != this->app_locale_array[_APP_I18N_SECTION].end ())
		{
			buffer = this->app_locale_array[_APP_I18N_SECTION][name];

			buffer.Replace (L"\\t", L"\t");
			buffer.Replace (L"\\r", L"\r");
			buffer.Replace (L"\\n", L"\n");
		}
	}

	if (buffer.IsEmpty ())
	{
		if (!h)
		{
			h = this->GetHINSTANCE ();
		}

		buffer.LoadStringW (h, id);
	}

	return buffer;
}

VOID CApp::LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position)
{
	if (this->is_localized && text)
	{
		MENUITEMINFO mif = {0};

		mif.cbSize = sizeof (mif);
		mif.fMask = MIIM_STRING;
		mif.dwTypeData = (LPWSTR)text;

		SetMenuItemInfo (menu, item, by_position, &mif);
	}
}

#ifndef _APP_NO_ABOUT

LRESULT CALLBACK CApp::AboutWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static HWND hparent = nullptr;

	switch (msg)
	{
		case WM_CREATE:
		{
			hparent = GetWindow (hwnd, GW_OWNER);

			_r_windowcenter (hwnd);

			EnableWindow (hparent, FALSE);

			return TRUE;
		}

		case WM_DESTROY:
		{
			if (!GetWindow (hparent, GW_ENABLEDPOPUP))
			{
				EnableWindow (hparent, TRUE);
				SetActiveWindow (hparent);
			}

			PostQuitMessage (0);

			return TRUE;
		}

		case WM_CLOSE:
		case WM_LBUTTONDBLCLK:
		{
			DestroyWindow (hwnd);
			return TRUE;
		}

		case WM_LBUTTONDOWN:
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
			break;
		}

		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
		case WM_CAPTURECHANGED:
		{
			LONG_PTR exstyle = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

			if ((exstyle & WS_EX_LAYERED) != WS_EX_LAYERED)
			{
				SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
			}

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 100 : 255, LWA_ALPHA);
			SetCursor (LoadCursor (nullptr, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW));

			break;
		}

		case WM_NOTIFY:
		{
			switch (LPNMHDR (lparam)->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					ShellExecute (hwnd, nullptr, PNMLINK (lparam)->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
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
				{
					DestroyWindow (hwnd);
					break;
				}
			}

			break;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}

#endif // _APP_NO_ABOUT

#ifndef _APP_NO_SETTINGS

INT_PTR CALLBACK CApp::SettingsPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static CApp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = (CApp*)lparam;
			break;
		}

		case WM_COMMAND:
		case WM_CONTEXTMENU:
		case WM_NOTIFY:
		{
			MSG wmsg = {0};

			wmsg.message = msg;
			wmsg.wParam = wparam;
			wmsg.lParam = lparam;

			return this_ptr->app_settings_pages.at (this_ptr->app_settings_page)->callback (hwnd, APP_CALLBACK_MESSAGE, &wmsg, this_ptr->app_settings_pages.at (this_ptr->app_settings_page));
		}
	}

	return FALSE;
}

INT_PTR CALLBACK CApp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static CApp* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = (CApp*)lparam;

			// localize
			SetWindowText (hwnd, I18N (this_ptr, IDS_SETTINGS, 0));

			SetDlgItemText (hwnd, IDC_APPLY, I18N (this_ptr, IDS_APPLY, 0));
			SetDlgItemText (hwnd, IDC_CLOSE, I18N (this_ptr, IDS_CLOSE, 0));

			// configure window
			_r_windowcenter (hwnd);

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, GetSystemMetrics (SM_CYSMICON));

			for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
			{
				this_ptr->app_settings_pages.at (i)->hwnd = CreateDialogParam (this_ptr->app_settings_pages.at (i)->h, MAKEINTRESOURCE (this_ptr->app_settings_pages.at (i)->dlg_id), hwnd, this_ptr->SettingsPagesProc, (LPARAM)this_ptr);

				HTREEITEM item = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->app_settings_pages.at (i)->title, -1, (LPARAM)i);

				if (this_ptr->ConfigGet (L"SettingsLastPage", 0) == i)
				{
					SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, (LPARAM)item);
				}

				this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, APP_CALLBACK_INITIALIZE, nullptr, this_ptr->app_settings_pages.at (i));
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

						ShowWindow (this_ptr->app_settings_pages.at (size_t (pnmtv->itemOld.lParam))->hwnd, SW_HIDE);
						ShowWindow (this_ptr->app_settings_pages.at (size_t (pnmtv->itemNew.lParam))->hwnd, SW_SHOW);

						this_ptr->ConfigSet (L"SettingsLastPage", INT (pnmtv->itemNew.lParam));

						this_ptr->app_settings_page = size_t (pnmtv->itemNew.lParam);

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
					BOOL is_restart = FALSE;

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						if (this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, APP_CALLBACK_SETTINGS_SAVE, nullptr, this_ptr->app_settings_pages.at (i)))
						{
							is_restart = TRUE;
						}
					}

					// call saved state
					this_ptr->app_settings_pages.at (0)->callback (this_ptr->app_settings_pages.at (0)->hwnd, APP_CALLBACK_SETTINGS_INIT, nullptr, nullptr);

					if (is_restart)
					{
						this_ptr->Restart ();
					}

					this_ptr->ConfigInit (); // reload settings

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);

					for (size_t i = 0; i < this_ptr->app_settings_pages.size (); i++)
					{
						this_ptr->app_settings_pages.at (i)->callback (this_ptr->app_settings_pages.at (i)->hwnd, APP_CALLBACK_UNINITIALIZE, nullptr, this_ptr->app_settings_pages.at (i)); // call closed state
					}

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

UINT WINAPI CApp::CheckForUpdatesProc (LPVOID lparam)
{
	CApp* this_ptr = (CApp*)lparam;

	if (this_ptr)
	{
		BOOL result = FALSE;
		HINTERNET internet = nullptr, connect = nullptr;

		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED);

		internet = InternetOpen (this_ptr->GetUserAgent (), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);

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

					CHAR buffera[MAX_PATH] = {0};
					CString bufferw;

					if (InternetReadFile (connect, buffera, _countof (buffera), &out) && out)
					{
						bufferw = CA2W (buffera, CP_UTF8);
						bufferw = bufferw.Trim (L" \r\n"); // trim whitespaces

						if (_r_str_versioncompare (this_ptr->app_version, bufferw) == -1)
						{
							if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_YES, 0), bufferw) == IDYES)
							{
								ShellExecute (nullptr, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
							}

							result = TRUE;
						}
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", DWORD (_r_unixtime_now ()));
			}
		}

		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);

		if (!result && !this_ptr->is_update_forced)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_NO, 0));
		}

		InternetCloseHandle (connect);
		InternetCloseHandle (internet);
	}

	return ERROR_SUCCESS;
}

#endif // _APP_NO_UPDATES

BOOL CApp::ParseINI (LPCWSTR path, IniMap* map)
{
	BOOL result = FALSE;

	if (map && _r_file_is_exists (path))
	{
		CString sc, vs;
		CString parser;

		DWORD length = _R_BUFFER_LENGTH;
		INT delimeter = 0;

		map->clear (); // clear first

		// get sections
		GetPrivateProfileSectionNames (sc.GetBuffer (1024), 1024, path);
		sc.ReleaseBuffer ();

		LPWSTR section = sc.GetBuffer ();
		LPWSTR values = nullptr;

		while (*section)
		{
			// get section data
			while (GetPrivateProfileSection (section, vs.GetBuffer (length), length, path) == (length - 1))
			{
				vs.ReleaseBuffer ();

				length += _R_BUFFER_LENGTH;
			}

			vs.ReleaseBuffer ();

			values = vs.GetBuffer ();

			while (*values)
			{
				parser = values;
				delimeter = parser.Find (L"=");

				(*map)[section][parser.Mid (0, delimeter)] = parser.Mid (delimeter + 1); // set

				values += _r_str_length (values) + 1; // go next data
			}

			section += _r_str_length (section) + 1; // go next section
		}

		result = TRUE;
	}

	return result;
}

#ifdef _APP_NO_UAC

BOOL CApp::SkipUacCreate (BOOL is_remove)
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

	CString name;
	name.Format (_APP_TASKSCHD_NAME, this->app_name_short);

	if (_r_system_validversion (6, 0))
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

BOOL CApp::SkipUacIsPresent (BOOL is_run)
{
	BOOL result = FALSE;

	CString name;
	name.Format (_APP_TASKSCHD_NAME, this->app_name_short);

	if (_r_system_validversion (6, 0))
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
										WCHAR path2[MAX_PATH];;

										exec_action->get_Path (&path1);

										GetModuleFileName (this->GetHINSTANCE (), path2, _countof (path2));

										// check path is to current module
										if (_wcsicmp (path1, path2) == 0)
										{
											if (is_run)
											{
												INT numargs = 0;
												LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);
												CString args;

												for (INT i = 1; i < numargs; i++)
												{
													args.Append (arga[i]);
													args.Append (L" ");
												}

												LocalFree (arga);

												variant_t ticker = args.Trim ().GetString ();

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
															if (state == TASK_STATE_RUNNING) { result = TRUE; }
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

BOOL CApp::SkipUacRun ()
{
	if (_r_system_uacstate ())
	{
		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;

		if (this->SkipUacIsPresent (TRUE))
		{
			return TRUE;
		}
		else
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
			{
				return TRUE;
			}
		}

		this->app_mutex = CreateMutex (nullptr, FALSE, this->app_name_short);
	}
	else
	{
		return TRUE;
	}

	return FALSE;
}

#endif // _APP_NO_UAC
