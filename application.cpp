// application support
// Copyright (c) 2013-2015 Henry++
//
// lastmod: Oct 17, 2015

#include "application.h"

CApplication::CApplication (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR author)
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

#ifdef APPLICATION_NEED_PRIVILEGES
		if (_r_system_uacstate () && _r_skipuac_run ())
		{
			return;
		}
#endif // APPLICATION_NEED_PRIVILEGES

		this->is_initialized = TRUE;
		this->app_hinstance = GetModuleHandle (nullptr);

		// general information
		StringCchCopy (this->app_name, _countof (this->app_name), name);
		StringCchCopy (this->app_name_short, _countof (this->app_name_short), short_name);
		StringCchCopy (this->app_version, _countof (this->app_version), version);
		StringCchCopy (this->app_author, _countof (this->app_author), author);

		// set current directory
		GetModuleFileName (nullptr, this->app_directory, _countof (this->app_directory));
		PathRemoveFileSpec (this->app_directory);

		// set configuration path
		StringCchPrintf (this->app_config_path, _countof (this->app_config_path), L"%s\\%s.ini", this->app_directory, this->app_name_short);

		if (!_r_file_is_exists (this->app_config_path))
		{
			ExpandEnvironmentStrings (_r_fmt (L"%%APPDATA%%\\%s\\%s\\%s.ini", this->app_author, this->app_name, this->app_name_short), this->app_config_path, _countof (this->app_config_path));
		}

		// initialize configuration array
		this->ConfigInit ();

		// load locale
		this->LocaleSet (this->ConfigGet (L"Language", nullptr));

		// load logo
#ifdef __RESOURCE_H__
		HDC hdc = GetDC (nullptr);

		INT size = MulDiv (64, GetDeviceCaps (hdc, LOGPIXELSY), 72);
		this->app_logo_big = _r_loadicon (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), size, size);

		ReleaseDC (nullptr, hdc);
#endif // __RESOURCE_H__

	}
}

CApplication::~CApplication ()
{
	if (this->app_mutex)
	{
		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;
	}

	if (this->app_logo_big)
	{
		DestroyIcon (this->app_logo_big);
		this->app_logo_big = nullptr;
	}
}

VOID CApplication::AutorunCreate (BOOL is_remove)
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

			RegSetValueEx (key, this->app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((wcslen (buffer) + 1) * sizeof (WCHAR)));
		}

		RegCloseKey (key);
	}
}

BOOL CApplication::AutorunIsPresent ()
{
	HKEY key = nullptr;
	BOOL result = FALSE;

	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		result = (RegQueryValueEx (key, this->app_name, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);

		RegCloseKey (key);
	}

	return result;
}

VOID CApplication::CheckForUpdates (BOOL is_periodical)
{
	if (is_periodical)
	{
		if (!this->ConfigGet (L"CheckUpdates", 1) || (_r_unixtime_now () - this->ConfigGet (L"CheckUpdatesLast", 0)) <= (86400 * APPLICATION_UPDATE_PERIOD))
		{
			return;
		}
	}

	this->app_cfu_mode = is_periodical;

	_beginthreadex (nullptr, 0, &this->CheckForUpdatesProc, (LPVOID)this, 0, nullptr);
}

VOID CApplication::ConfigInit ()
{
	this->app_config_array.clear (); // reset

	this->ParseINI (this->app_config_path, this->app_name_short, &this->app_config_array);
}

DWORD CApplication::ConfigGet (LPCWSTR key, INT def)
{
	CString val = this->ConfigGet (key, nullptr);

	if (val.IsEmpty ())
	{
		return def;
	}

	return wcstol (val, nullptr, 10);
}

CString CApplication::ConfigGet (LPCWSTR key, LPCWSTR def)
{
	// check key is exists
	if (this->app_config_array.find (key) == this->app_config_array.end ())
	{
		return def;
	}

	return this->app_config_array[key];
}

BOOL CApplication::ConfigSet (LPCWSTR key, LPCWSTR val)
{
	WCHAR buffer[MAX_PATH] = {0};

	StringCchCopy (buffer, _countof (buffer), this->app_config_path);
	PathRemoveFileSpec (buffer);

	if (!_r_file_is_exists (buffer))
	{
		SHCreateDirectoryEx (nullptr, buffer, nullptr);
	}

	// update hash value
	this->app_config_array[key] = val;

	return WritePrivateProfileString (this->app_name_short, key, val, this->app_config_path);
}

BOOL CApplication::ConfigSet (LPCWSTR key, DWORD val)
{

#ifdef _WIN64
	return this->ConfigSet (key, _r_fmt (L"%lld", val));
#else
	return this->ConfigSet (key, _r_fmt (L"%ld", val));
#endif // _WIN64

}

VOID CApplication::CreateAboutWindow ()
{

#ifdef IDD_ABOUT
	DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_ABOUT), this->GetHWND (), this->AboutWindowProc, (LPARAM)this);
#endif // __RESOURCE_H__

}

BOOL CApplication::CreateMainWindow (DLGPROC proc)
{
	BOOL result = FALSE;

	if (this->is_initialized)
	{
		// create window

#ifdef IDD_MAIN
		if ((this->app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc)) == nullptr)
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
#ifdef IDI_MAIN
			SendMessage (this->GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)LoadImage (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), IMAGE_ICON, GetSystemMetrics (SM_CXSMICON), GetSystemMetrics (SM_CYSMICON), 0));
			SendMessage (this->GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)LoadImage (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), IMAGE_ICON, GetSystemMetrics (SM_CXICON), GetSystemMetrics (SM_CYICON), 0));
#endif // IDI_MAIN

			result = TRUE;
		}
#endif // IDD_MAIN

		// check for updates
		this->CheckForUpdates (TRUE);
	}

	return result;
}

VOID CApplication::CreateSettingsWindow (DWORD page_count, DLGPROC proc, SETTINGS_SAVE_CALLBACK callback)
{
	this->app_settings_hwnd.clear (); // clear first

	this->app_settings_count = page_count;
	this->app_settings_proc = proc;
	this->app_settings_save = callback;

#ifdef __RESOURCE_H__
	DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS), this->GetHWND (), this->SettingsWindowProc, (LPARAM)this);
#endif // __RESOURCE_H__
}

HWND CApplication::GetHWND ()
{
	return this->app_hwnd;
}

VOID CApplication::Restart ()
{
	WCHAR buffer[MAX_PATH] = {0};
	GetModuleFileName (nullptr, buffer, _countof (buffer));

	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};

	si.cb = sizeof (si);

	ShowWindow (this->GetHWND (), SW_HIDE); // hide main window

	if (this->app_mutex)
	{
		CloseHandle (this->app_mutex);
		this->app_mutex = nullptr;
	}

	if (CreateProcess (buffer, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
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

VOID CApplication::SetCopyright (LPCWSTR copyright)
{
	StringCchCopy (this->app_copyright, _countof (this->app_copyright), copyright);
}

VOID CApplication::SetLinks (LPCWSTR website, LPCWSTR github)
{
	StringCchCopy (this->app_website, _countof (this->app_website), website);
	StringCchCopy (this->app_github, _countof (this->app_github), github);
}

VOID CApplication::LocaleEnum (HWND hwnd, INT ctrl_id)
{
	WIN32_FIND_DATA wfd = {0};
	HANDLE h = FindFirstFile (_r_fmt (L"%s\\" APPLICATION_LOCALE_DIRECTORY L"\\*.ini", this->app_directory), &wfd);

	CString def = this->ConfigGet (L"Language", nullptr);

	if (h != INVALID_HANDLE_VALUE)
	{
		INT count = max (0, (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCOUNT, 0, NULL));

		do
		{
			LPWSTR fname = wfd.cFileName;
			PathRemoveExtension (fname);

			SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, count++, (LPARAM)fname);

			if (def.CompareNoCase (fname) == 0)
			{
				SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, count - 1, NULL);
			}
		}
		while (FindNextFile (h, &wfd));

		FindClose (h);
	}
	else
	{
		EnableWindow (GetDlgItem (hwnd, ctrl_id), FALSE);
	}
}

VOID CApplication::LocaleSet (LPCWSTR name)
{
	this->ConfigSet (L"Language", name);

	// reset
	this->app_locale_array.clear ();
	this->is_localized = FALSE;

	if (name)
	{
		if (this->ParseINI (_r_fmt (L"%s\\" APPLICATION_LOCALE_DIRECTORY L"\\%s.ini", this->app_directory, name), APPLICATION_LOCALE_SECTION, &this->app_locale_array))
		{
			this->is_localized = TRUE;
		}
	}
}

CString CApplication::LocaleString (UINT id, LPCWSTR name)
{
	CString buffer;

	if (this->is_localized)
	{
		// check key is exists
		if (this->app_locale_array.find (name) != this->app_locale_array.end ())
		{
			buffer = this->app_locale_array[name];
		}
	}

	if (buffer.IsEmpty ())
	{
		buffer.LoadStringW (this->app_hinstance, id);
	}

	return buffer;
}

VOID CApplication::LocaleMenu (HMENU menu, LPCWSTR text, UINT item, BOOL by_position)
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

INT_PTR CALLBACK CApplication::AboutWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			CApplication* this_ptr = (CApplication*)lparam;

			// configure
			_r_windowcenter (hwnd);
			_r_windowtotop (hwnd, TRUE);

			SetWindowText (hwnd, I18N (this_ptr, IDS_ABOUT, 0));

			// draw icon
			SendDlgItemMessage (hwnd, IDC_LOGO, STM_SETIMAGE, IMAGE_ICON, (LPARAM)this_ptr->app_logo_big);

			// show information
			CString host1 = this_ptr->app_website;
			CString host2 = this_ptr->app_github;

			host1.Delete (0, host1.Find (L"//", 0) + 2);
			host2.Delete (0, host2.Find (L"//", 0) + 2);

			SetDlgItemText (hwnd, IDC_TITLE, _r_fmt (L"%s v%s (%d-bit)", this_ptr->app_name, this_ptr->app_version, this_ptr->app_architecture));
			SetDlgItemText (hwnd, IDC_COPYRIGHT, this_ptr->app_copyright);
			SetDlgItemText (hwnd, IDC_LINKS, _r_fmt (L"<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", this_ptr->app_website, host1, this_ptr->app_github, host2));
			SetDlgItemText (hwnd, IDC_INFO, I18N (this_ptr, IDS_TRANSLATOR, 0));

			break;
		}

		case WM_CLOSE:
		case WM_LBUTTONDBLCLK:
		{
			EndDialog (hwnd, 0);
			break;
		}

		case WM_LBUTTONDOWN:
		{
			SendMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, NULL);
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
					ShellExecute (nullptr, nullptr, PNMLINK (lparam)->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDOK: // process Enter key
				case IDCANCEL: // process Esc key
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

INT_PTR CALLBACK CApplication::SettingsWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static CApplication* this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = (CApplication*)lparam;

			// localize
#ifdef __RESOURCE_H__
			SetWindowText (hwnd, I18N (this_ptr, IDS_SETTINGS, 0));

			SetDlgItemText (hwnd, IDC_APPLY, I18N (this_ptr, IDS_APPLY, 0));
			SetDlgItemText (hwnd, IDC_CLOSE, I18N (this_ptr, IDS_CLOSE, 0));
#endif //  __RESOURCE_H__

			// configure treeview
			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, GetSystemMetrics (SM_CYSMICON));

			for (DWORD i = 0; i < this_ptr->app_settings_count; i++)
			{
				this_ptr->app_settings_hwnd[i] = CreateDialogParam (nullptr, MAKEINTRESOURCE (IDD_SETTINGS_1 + i), hwnd, this_ptr->app_settings_proc, i);

				_r_treeview_additem (hwnd, IDC_NAV, I18N (this_ptr, IDS_SETTINGS_1 + i, _r_fmt (L"IDS_SETTINGS_%d", i + 1)), -1, (LPARAM)i);
			}

			SendDlgItemMessage (hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_FIRSTVISIBLE, NULL)); // select 1-st item

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

						ShowWindow (this_ptr->app_settings_hwnd[INT (pnmtv->itemOld.lParam)], SW_HIDE);
						ShowWindow (this_ptr->app_settings_hwnd[INT (pnmtv->itemNew.lParam)], SW_SHOW);

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

					for (DWORD i = 0; i < this_ptr->app_settings_count; i++)
					{
						if (this_ptr->app_settings_save (this_ptr->app_settings_hwnd[i], i))
						{
							is_restart = TRUE;
						}
					}

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
					this_ptr->app_settings_save (nullptr, (DWORD)-1);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

UINT WINAPI CApplication::CheckForUpdatesProc (LPVOID lparam)
{
	CApplication* this_ptr = (CApplication*)lparam;

	if (this_ptr)
	{
		BOOL result = FALSE;
		HINTERNET internet = nullptr, connect = nullptr;

#ifdef __RESOURCE_H__
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_DISABLED);
#endif  // __RESOURCE_H__

		internet = InternetOpen (_r_fmt (L"%s/%s (+%s)", this_ptr->app_name, this_ptr->app_version, this_ptr->app_website), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);

		if (internet)
		{
			connect = InternetOpenUrl (internet, _r_fmt (L"%s/update.php?product=%s", this_ptr->app_website, this_ptr->app_name_short), nullptr, 0, INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_NO_COOKIES, 0);

			if (connect)
			{
				DWORD dwStatus = 0, dwStatusSize = sizeof (dwStatus);
				HttpQueryInfo (connect, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, nullptr);

				if (dwStatus == HTTP_STATUS_OK)
				{
					DWORD count = 0;

					CHAR buffera[MAX_PATH] = {0};
					CString bufferw;

					if (InternetReadFile (connect, buffera, _countof (buffera), &count) && count)
					{
						bufferw = CA2W (buffera, CP_UTF8);
						bufferw = bufferw.Trim (L" \r\n"); // trim whitespaces

						if (_r_string_versioncompare (this_ptr->app_version, bufferw) == -1)
						{
							if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_YES, 0), bufferw) == IDYES)
							{
								ShellExecute (nullptr, nullptr, this_ptr->app_website, nullptr, nullptr, SW_SHOWDEFAULT);
							}

							result = TRUE;
						}
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", DWORD (_r_unixtime_now ()));
			}
		}

#ifdef __RESOURCE_H__
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // __RESOURCE_H__

		if (!result && !this_ptr->app_cfu_mode)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, I18N (this_ptr, IDS_UPDATE_NO, 0));
		}

		InternetCloseHandle (connect);
		InternetCloseHandle (internet);
	}

	return 0;
}

BOOL CApplication::ParseINI (LPCWSTR path, LPCWSTR section, CStringMap* map)
{
	BOOL result = FALSE;

	if (map && _r_file_is_exists (path))
	{
		CString buffer;
		CString parser;

		DWORD length = ROUTINE_BUFFER_LENGTH;
		INT delimeter = 0;

		// read whole file
		while (GetPrivateProfileSection (section, buffer.GetBuffer (length), length, path) == (length - 1))
		{
			buffer.ReleaseBuffer ();

			length += ROUTINE_BUFFER_LENGTH;
		}

		buffer.ReleaseBuffer ();

		LPWSTR ptr = buffer.GetBuffer ();

		map->clear (); // clear first

		while (*ptr)
		{
			parser = ptr;
			delimeter = parser.Find (L"=");

			(*map)[parser.Mid (0, delimeter)] = parser.Mid (delimeter + 1); // set

			ptr += lstrlen (ptr) + 1; // go next
		}

		result = TRUE;
	}

	return result;
}
