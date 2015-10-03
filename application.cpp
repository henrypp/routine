// application support
// © 2013-2015 Henry++
//
// lastmod: Oct 3, 2015

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

		// configuration
		GetModuleFileName (nullptr, this->app_config_path, _countof (this->app_config_path));
		PathRemoveFileSpec (this->app_config_path);

		StringCchPrintf (this->app_config_path, _countof (this->app_config_path), L"%s\\%s.ini", this->app_config_path, short_name);

		if (!_r_file_is_exists (this->app_config_path))
		{
			ExpandEnvironmentStrings (_r_fmt (L"%%APPDATA%%\\%s\\%s\\%s.ini", this->app_author, name, short_name), this->app_config_path, _countof (this->app_config_path));
		}

		// load logo
		HDC hdc = GetDC (nullptr);

		INT size = MulDiv (48, GetDeviceCaps (hdc, LOGPIXELSY), 72);
		this->app_logo_big = (HICON)LoadImage (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), IMAGE_ICON, size, size, 0);

		ReleaseDC (nullptr, hdc);
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
		if (!this->ConfigGet (L"CheckUpdates", 1) || (_r_unixtime () - this->ConfigGet (L"CheckUpdatesLast", 0)) <= (86400 * APPLICATION_UPDATE_PERIOD))
		{
			return;
		}
	}

	this->app_cfu_mode = is_periodical;

	_beginthreadex (nullptr, 0, &this->CheckForUpdatesProc, (LPVOID)this, 0, nullptr);
}

UINT CApplication::ConfigGet (LPCWSTR key, INT def)
{
	return GetPrivateProfileInt (this->app_name_short, key, def, this->app_config_path);
}

CString CApplication::ConfigGet (LPCWSTR key, LPCWSTR def)
{
	CString buffer;
	DWORD length = ROUTINE_BUFFER_LENGTH;

	while (GetPrivateProfileString (this->app_name_short, key, def, buffer.GetBuffer (length), length, this->app_config_path) == (length - 1))
	{
		buffer.ReleaseBuffer ();

		length += ROUTINE_BUFFER_LENGTH;
	}

	buffer.ReleaseBuffer ();

	return buffer;
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

#ifdef __RESOURCE_H__
	DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_ABOUT), this->GetHWND (), this->AboutWindowProc, (LPARAM)this);
#endif // __RESOURCE_H__

}

BOOL CApplication::CreateMainWindow (DLGPROC proc)
{
	// create window

#ifdef __RESOURCE_H__
	if ((this->app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (IDD_MAIN), nullptr, proc)) == nullptr)
	{
		return FALSE;
	}
	else
	{
		// set title
		SetWindowText (this->GetHWND (), this->app_name);

		// set icons
		SendMessage (this->GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)LoadImage (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), IMAGE_ICON, GetSystemMetrics (SM_CXSMICON), GetSystemMetrics (SM_CYSMICON), 0));
		SendMessage (this->GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)LoadImage (this->app_hinstance, MAKEINTRESOURCE (IDI_MAIN), IMAGE_ICON, GetSystemMetrics (SM_CXICON), GetSystemMetrics (SM_CYICON), 0));
	}
#endif // __RESOURCE_H__

	// check for updates
	this->CheckForUpdates (TRUE);

	return TRUE;
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

			// draw icon
			SendDlgItemMessage (hwnd, IDC_LOGO, STM_SETIMAGE, IMAGE_ICON, (LPARAM)this_ptr->app_logo_big);

			// show information
			SetDlgItemText (hwnd, IDC_TITLE, _r_fmt (L"%s v%s (%d-bit)", this_ptr->app_name, this_ptr->app_version, this_ptr->app_architecture));
			SetDlgItemText (hwnd, IDC_COPYRIGHT, this_ptr->app_copyright);

			CString host1 = this_ptr->app_website;
			CString host2 = this_ptr->app_github;

			host1.Delete (0, host1.Find (L"//", 0) + 2);
			host2.Delete (0, host2.Find (L"//", 0) + 2);

			SetDlgItemText (hwnd, IDC_PANEL, _r_fmt (L"<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", this_ptr->app_website, host1, this_ptr->app_github, host2));

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
		{
			LONG_PTR exstyle = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

			if ((exstyle & WS_EX_LAYERED) != WS_EX_LAYERED)
			{
				SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);
			}

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 200 : 255, LWA_ALPHA);
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

	return 0;
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

						if (_r_versioncompare (this_ptr->app_version, bufferw) == -1)
						{
							if (_r_msg (this_ptr->GetHWND (), MB_YESNO | MB_ICONQUESTION, this_ptr->app_name, this_ptr->LocaleString (IDS_UPDATE_YES), bufferw) == IDYES)
							{
								ShellExecute (this_ptr->GetHWND (), nullptr, this_ptr->app_website, nullptr, nullptr, SW_SHOWDEFAULT);
							}

							result = TRUE;
						}
					}
				}

				this_ptr->ConfigSet (L"CheckUpdatesLast", (DWORD)_r_unixtime ());
			}
		}

#ifdef __RESOURCE_H__
		EnableMenuItem (GetMenu (this_ptr->GetHWND ()), IDM_CHECKUPDATES, MF_BYCOMMAND | MF_ENABLED);
#endif // __RESOURCE_H__

		if (!result && !this_ptr->app_cfu_mode)
		{
			_r_msg (this_ptr->GetHWND (), MB_OK | MB_ICONINFORMATION, this_ptr->app_name, this_ptr->LocaleString (IDS_UPDATE_NO));
		}

		InternetCloseHandle (connect);
		InternetCloseHandle (internet);
	}

	return 0;
}

VOID CApplication::LocaleSet (LPCWSTR name)
{
	this->ConfigSet (L"Language", name);
}

CString CApplication::LocaleString (UINT id)
{
	CString buffer;

	buffer.LoadStringW (id);

	return buffer;
}
