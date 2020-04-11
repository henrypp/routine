// routine++
// Copyright (c) 2012-2020 Henry++

#include "rapp.hpp"

#if defined(_APP_HAVE_TRAY)
const UINT WM_TASKBARCREATED = RegisterWindowMessage (L"TaskbarCreated");
#endif // _APP_HAVE_TRAY

namespace rhelper
{
	void template_write (BYTE **pPtr, const void* data, size_t size)
	{
		RtlCopyMemory (*pPtr, data, size);
		*pPtr += size;
	}

	void template_writestring (BYTE **pPtr, LPCWSTR str)
	{
		template_write (pPtr, str, (_r_str_length (str) + 1) * sizeof (WCHAR));
	}

	void template_writevar (BYTE **pPtr, std::any data, size_t size)
	{
		template_write (pPtr, &data, size);
	}

	void template_writecontrol (BYTE **pPtr, DWORD ctrl_id, DWORD style, SHORT x, SHORT y, SHORT cx, SHORT cy, LPCWSTR class_name)
	{
		*pPtr = (LPBYTE)(DWORD_PTR (*pPtr + 3) & ~3); // align as DWORD

		// fill DLGITEMTEMPLATEEX
		template_writevar (pPtr, DWORD (0), sizeof (DWORD)); // helpID
		template_writevar (pPtr, DWORD (0), sizeof (DWORD)); // exStyle
		template_write (pPtr, &style, sizeof (style)); // style

		template_write (pPtr, &x, sizeof (x)); // x
		template_write (pPtr, &y, sizeof (y)); // y
		template_write (pPtr, &cx, sizeof (cx)); // cx
		template_write (pPtr, &cy, sizeof (cy)); // cy

		template_write (pPtr, &ctrl_id, sizeof (ctrl_id)); // id

		template_writestring (pPtr, class_name); // windowClass
		template_writestring (pPtr, L""); // title

		template_writevar (pPtr, WORD (0), sizeof (WORD)); // extraCount
	}

	rstring version_format (rstring text)
	{
		if (_r_str_isnumeric (text))
			return _r_fmt_date (text.AsLonglong (), FDTF_SHORTDATE | FDTF_SHORTTIME);

		return text;
	}

	bool initialize_mitigation_policy (LPCWSTR path, LPCWSTR dir)
	{
		if (!_r_sys_validversion (10, 0, 15063)) // win10rs2+
			return true;

		DWORD64 default_flags = PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_ON |
			//PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON |
			PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON;

		// PROCESS_CREATION_MITIGATION_POLICY2_MODULE_TAMPERING_PROTECTION_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_SPECULATIVE_STORE_BYPASS_DISABLE_ALWAYS_ON |

		bool is_success = true;

		STARTUPINFOEX startupInfo = {0};

		startupInfo.StartupInfo.cb = sizeof (startupInfo.StartupInfo);
		GetStartupInfo (&startupInfo.StartupInfo);

		startupInfo.StartupInfo.cb = sizeof (startupInfo); // reset structure size

		PROCESS_INFORMATION pi = {0};

		LPWSTR command_line = nullptr;

#if !defined(_APP_NO_WINXP)
		HMODULE hkernel32 = LoadLibraryEx (L"kernel32.dll", nullptr, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (!hkernel32)
			goto CleanupExit;
#endif // _APP_NO_WINXP

		HMODULE hntdll = LoadLibraryEx (L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (!hntdll)
			goto CleanupExit;

		PPS_SYSTEM_DLL_INIT_BLOCK _LdrSystemDllInitBlock = (PPS_SYSTEM_DLL_INIT_BLOCK)GetProcAddress (hntdll, "LdrSystemDllInitBlock");

		if (!_LdrSystemDllInitBlock || !RTL_CONTAINS_FIELD (_LdrSystemDllInitBlock, _LdrSystemDllInitBlock->Size, MitigationOptionsMap))
			goto CleanupExit;

		if ((_LdrSystemDllInitBlock->MitigationOptionsMap.Map[0] & default_flags) == default_flags)
			goto CleanupExit;

		SIZE_T attributeListLength;

#if defined(_APP_NO_WINXP)
		if (!InitializeProcThreadAttributeList (nullptr, 1, 0, &attributeListLength) && GetLastError () != ERROR_INSUFFICIENT_BUFFER)
			goto CleanupExit;
#else
		using IPTAL = decltype (&InitializeProcThreadAttributeList);
		const IPTAL _InitializeProcThreadAttributeList = (IPTAL)GetProcAddress (hkernel32, "InitializeProcThreadAttributeList");

		if (!_InitializeProcThreadAttributeList || (!_InitializeProcThreadAttributeList (nullptr, 1, 0, &attributeListLength) && GetLastError () != ERROR_INSUFFICIENT_BUFFER))
			goto CleanupExit;
#endif // _APP_NO_WINXP

		startupInfo.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)_r_mem_alloc (attributeListLength);

		if (!startupInfo.lpAttributeList)
			goto CleanupExit;

#if defined(_APP_NO_WINXP)
		if (!InitializeProcThreadAttributeList (startupInfo.lpAttributeList, 1, 0, &attributeListLength))
			goto CleanupExit;
#else
		if (!_InitializeProcThreadAttributeList (startupInfo.lpAttributeList, 1, 0, &attributeListLength))
			goto CleanupExit;
#endif // _APP_NO_WINXP

#if defined(_APP_NO_WINXP)
		if (!UpdateProcThreadAttribute (startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &default_flags, sizeof (default_flags), nullptr, nullptr))
			goto CleanupExit;
#else
		using UPTAL = decltype (&UpdateProcThreadAttribute);
		const UPTAL _UpdateProcThreadAttribute = (UPTAL)GetProcAddress (hkernel32, "UpdateProcThreadAttribute");

		if (!_UpdateProcThreadAttribute || !_UpdateProcThreadAttribute (startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &default_flags, sizeof (default_flags), nullptr, nullptr))
			goto CleanupExit;
#endif // _APP_NO_WINXP

		_r_str_alloc (&command_line, INVALID_SIZE_T, GetCommandLine ());

		if (CreateProcess (path, command_line, nullptr, nullptr, FALSE, CREATE_BREAKAWAY_FROM_JOB | EXTENDED_STARTUPINFO_PRESENT, nullptr, dir, &startupInfo.StartupInfo, &pi))
			is_success = false;

		if (pi.hProcess)
			NtClose (pi.hProcess);

		if (pi.hThread)
			NtClose (pi.hThread);

CleanupExit:

		SAFE_DELETE_ARRAY (command_line);

		if (startupInfo.lpAttributeList)
		{
#if defined(_APP_NO_WINXP)
			DeleteProcThreadAttributeList (startupInfo.lpAttributeList);
#else
			using DPTAL = decltype (&DeleteProcThreadAttributeList);
			const DPTAL _DeleteProcThreadAttributeList = (DPTAL)GetProcAddress (hkernel32, "DeleteProcThreadAttributeList");

			if (_DeleteProcThreadAttributeList)
				_DeleteProcThreadAttributeList (startupInfo.lpAttributeList);
#endif // _APP_NO_WINXP

			_r_mem_free (startupInfo.lpAttributeList);
		}

#if !defined(_APP_NO_WINXP)
		if (hkernel32)
			FreeLibrary (hkernel32);
#endif // !_APP_NO_WINXP

		if (hntdll)
			FreeLibrary (hntdll);

		return is_success;
	}

	BOOL CALLBACK activate_window_callback (HWND hwnd, LPARAM lparam)
	{
		LPCWSTR app_name = (LPCWSTR)lparam;

		if (_r_str_isempty (app_name))
			return FALSE;

		DWORD pid = 0;
		GetWindowThreadProcessId (hwnd, &pid);

		if (GetCurrentProcessId () == pid)
			return TRUE;

		// check window class
		WCHAR class_name[64] = {0};

		if (GetClassName (hwnd, class_name, _countof (class_name)) > 0 && _r_str_compare (class_name, L"#32770", 6) == 0)
		{
			// check window title
			WCHAR window_title[128] = {0};

			if (GetWindowText (hwnd, window_title, _countof (window_title)) > 0 && _r_str_compare (window_title, app_name, _r_str_length (app_name)) == 0)
			{
				// check window prop
				if (GetProp (hwnd, app_name))
				{
					_r_wnd_toggle (hwnd, true);
					return FALSE;
				}
			}
		}

		return TRUE;
	}
};

rapp::rapp ()
{
	app_binary[0] = UNICODE_NULL;
	app_profile_directory[0] = UNICODE_NULL;
	app_config_path[0] = UNICODE_NULL;

#if !defined(_APP_CONSOLE)
	locale_default[0] = UNICODE_NULL;
#endif // !_APP_CONSOLE
}

bool rapp::Initialize (LPCWSTR name, LPCWSTR short_name, LPCWSTR version, LPCWSTR copyright)
{
	// initialize process heap
	rinternal::hProcessHeap = RtlCreateHeap (HEAP_GROWABLE | HEAP_CLASS_1, nullptr, 2 * _R_BYTESIZE_MB, 1 * _R_BYTESIZE_MB, nullptr, nullptr);

	if (!rinternal::hProcessHeap)
	{
#if defined(_APP_CONSOLE)
		wprintf (L"Error! Private heap object initialization failed 0x%08" PRIX32 L"!\r\n", GetLastError ());
#else
		ShowErrorMessage (nullptr, L"Private heap object initialization failed!", GetLastError (), nullptr);
#endif // _APP_CONSOLE

		return false;
	}
	else
	{
		ULONG hci = HEAP_COMPATIBILITY_LFH;
		RtlSetHeapInformation (rinternal::hProcessHeap, HeapCompatibilityInformation, &hci, sizeof (hci));
	}

	// safe dll loading
	SetDllDirectory (L"");

	// win7+
	if (_r_sys_validversion (6, 1))
	{
		HMODULE hlib = LoadLibraryEx (L"kernel32.dll", nullptr, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (hlib)
		{
			using SSPM = decltype (&SetSearchPathMode);
			const SSPM _SetSearchPathMode = (SSPM)GetProcAddress (hlib, "SetSearchPathMode");

			if (_SetSearchPathMode)
				_SetSearchPathMode (BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);

			// Check for SetDefaultDllDirectories since it requires KB2533623.
			using SDDD = decltype (&SetDefaultDllDirectories);
			const SDDD _SetDefaultDllDirectories = (SDDD)GetProcAddress (hlib, "SetDefaultDllDirectories");

			if (_SetDefaultDllDirectories)
				_SetDefaultDllDirectories (LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
		}

		FreeLibrary (hlib);
	}

	// store system information
#if !defined(_APP_NO_WINXP)
	is_vistaorlater = _r_sys_validversion (6, 0);
#endif // !_APP_NO_WINXP

	// set hinstance
	app_hinstance = GetModuleHandle (nullptr);

	// set general information
	_r_str_alloc (&app_name, INVALID_SIZE_T, name);
	_r_str_alloc (&app_name_short, INVALID_SIZE_T, short_name);
	_r_str_alloc (&app_copyright, INVALID_SIZE_T, copyright);

#if defined(_DEBUG) || defined(_APP_BETA)
	_r_str_alloc (&app_version, INVALID_SIZE_T, _r_fmt (L"%s Pre-release", version));
#else
	_r_str_alloc (&app_version, INVALID_SIZE_T, _r_fmt (L"%s Release", version));
#endif // _DEBUG || _APP_BETA

	// set app path
	GetModuleFileName (nullptr, app_binary, _countof (app_binary));
	_r_str_alloc (&app_directory, INVALID_SIZE_T, _r_path_getdirectory (app_binary));

	// prevent app duplicates
#if !defined(_APP_CONSOLE)
#if defined(_APP_NO_MUTEX)
	_r_str_alloc (&app_mutex_name, INVALID_SIZE_T, _r_fmt (L"%s_%" PR_SIZE_T L"_%" PR_SIZE_T, app_name_short, _r_str_hash (GetBinaryPath ()), _r_str_hash (GetCommandLine ())));
#else
	_r_str_alloc (&app_mutex_name, INVALID_SIZE_T, app_name_short);
#endif // _APP_NO_MUTEX

	if (MutexIsExists (true))
		return false;

	// initialize controls
	{
		INITCOMMONCONTROLSEX icex = {0};

		icex.dwSize = sizeof (icex);
		icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;

		InitCommonControlsEx (&icex);
	}
#endif // !_APP_CONSOLE

	// initialize COM library
	{
		HRESULT hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		if (FAILED (hr))
		{
#if defined(_APP_CONSOLE)
			wprintf (L"Error! COM library initialization failed 0x%08" PRIX32 L"!\r\n", hr);
#else
			ShowErrorMessage (nullptr, L"COM library initialization failed!", hr, nullptr);
#endif // _APP_CONSOLE

			return false;
		}
	}

	// check for "only admin"-mode
#if !defined(_APP_CONSOLE)
#if defined(_APP_NO_GUEST)
	if (!_r_sys_iselevated ())
	{
		if (!RunAsAdmin ())
			ShowErrorMessage (nullptr, L"Administrative privileges are required!", ERROR_DS_INSUFF_ACCESS_RIGHTS, nullptr);

		return false;
	}

	// use "skipuac" feature
#elif defined(_APP_HAVE_SKIPUAC)
	if (!_r_sys_iselevated () && SkipUacRun ())
		return false;
#endif // _APP_NO_GUEST

	// set running flag
	MutexCreate ();

	// check for wow64 working and show warning if it is true!
#if !defined(_DEBUG) && !defined(_WIN64)
	if (_r_sys_iswow64 ())
	{
		if (!ShowConfirmMessage (nullptr, L"Warning!", _r_fmt (L"You are attempting to run the 32-bit version of %s on 64-bit Windows.\r\nPlease run the 64-bit version of %s instead.", app_name, app_name), L"ConfirmWOW64"))
			return false;
	}
#endif // !_DEBUG && !_WIN64
#endif // !_APP_CONSOLE

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
					LPWSTR value = arga[i + 1];

					if (*value == L'/' || *value == L'-')
						continue;

					PathUnquoteSpaces (value);

					_r_str_copy (app_config_path, _countof (app_config_path), _r_path_expand (value));

					if (PathGetDriveNumber (app_config_path) == INVALID_INT)
						_r_str_printf (app_config_path, _countof (app_config_path), L"%s\\%s", GetDirectory (), _r_path_expand (value).GetString ());

					if (!_r_fs_exists (app_config_path))
					{
						HANDLE hfile = CreateFile (app_config_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

						if (_r_fs_isvalidhandle (hfile))
							CloseHandle (hfile);
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

	// set log path
	_r_str_alloc (&app_logpath, INVALID_SIZE_T, _r_fmt (L"%s\\%s_debug.log", GetProfileDirectory (), app_name_short));

	// set locale path
#if !defined(_APP_CONSOLE)
	_r_str_alloc (&app_localepath, INVALID_SIZE_T, _r_fmt (L"%s\\%s.lng", GetDirectory (), app_name_short));
#endif // !_APP_CONSOLE

	// set updates path
#if defined(_APP_HAVE_UPDATES)
	_r_str_alloc (&app_updatepath, INVALID_SIZE_T, _r_fmt (L"%s\\%s_update.exe", GetProfileDirectory (), app_name_short));
#endif // _APP_HAVE_UPDATES

	// get default system locale
#if !defined(_APP_CONSOLE)
	GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SENGLISHLANGUAGENAME, locale_default, _countof (locale_default));
#endif // !_APP_CONSOLE

	// read config
	ConfigInit ();

	// set classic theme
#if !defined(_APP_CONSOLE)
	if (ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool ())
		SetThemeAppProperties (STAP_ALLOW_NONCLIENT);
#endif // !_APP_CONSOLE

	return true;
}

#if !defined(_APP_CONSOLE)
bool rapp::MutexCreate ()
{
	MutexDestroy ();

	if (!_r_str_isempty (app_mutex_name))
		app_mutex = CreateMutex (nullptr, FALSE, app_mutex_name);

	return _r_fs_isvalidhandle (app_mutex);
}

bool rapp::MutexDestroy ()
{
	if (_r_fs_isvalidhandle (app_mutex))
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
	if (_r_str_isempty (app_mutex_name))
		return false;

	HANDLE hmutex = CreateMutex (nullptr, FALSE, app_mutex_name);

	if (GetLastError () == ERROR_ALREADY_EXISTS)
	{
		if (_r_fs_isvalidhandle (hmutex))
			CloseHandle (hmutex);

		if (activate_window)
			EnumWindows (&rhelper::activate_window_callback, (LPARAM)this->app_name);

		return true;
	}

	if (_r_fs_isvalidhandle (hmutex))
	{
		ReleaseMutex (hmutex);
		CloseHandle (hmutex);
	}

	return false;
}
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_AUTORUN)
bool rapp::AutorunIsEnabled ()
{
	if (ConfigGet (L"AutorunIsEnabled", false).AsBool ())
	{
		HKEY hkey = nullptr;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx (hkey, app_name, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				RegCloseKey (hkey);
				return true;
			}

			RegCloseKey (hkey);
		}
	}

	return false;
}

LSTATUS rapp::AutorunEnable (HWND hwnd, bool is_enable)
{
	HKEY hkey = nullptr;
	LONG result = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);

	if (result == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			WCHAR buffer[MAX_PATH] = {0};
			_r_str_printf (buffer, _countof (buffer), L"\"%s\" /minimized", GetBinaryPath ());

			result = RegSetValueEx (hkey, app_name, 0, REG_SZ, (LPBYTE)buffer, DWORD ((_r_str_length (buffer) + 1) * sizeof (WCHAR)));

			ConfigSet (L"AutorunIsEnabled", !!(result == ERROR_SUCCESS));
		}
		else
		{
			result = RegDeleteValue (hkey, app_name);

			if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
			{
				ConfigSet (L"AutorunIsEnabled", false);

				if (result == ERROR_FILE_NOT_FOUND)
					result = ERROR_SUCCESS;
			}
		}

		RegCloseKey (hkey);
	}

	if (hwnd && result != ERROR_SUCCESS)
		ShowErrorMessage (hwnd, nullptr, result, nullptr);

	return result;
}
#endif // _APP_HAVE_AUTORUN

#if defined(_APP_HAVE_UPDATES)
void rapp::UpdateAddComponent (LPCWSTR full_name, LPCWSTR short_name, LPCWSTR version, LPCWSTR target_path, bool is_installer)
{
	if (!pupdateinfo)
	{
		pupdateinfo = new APP_UPDATE_INFO;
		pupdateinfo->papp = this;
	}

	PAPP_UPDATE_COMPONENT pcomponent = new APP_UPDATE_COMPONENT;
	RtlSecureZeroMemory (pcomponent, sizeof (APP_UPDATE_COMPONENT));

	_r_str_alloc (&pcomponent->full_name, INVALID_SIZE_T, full_name);
	_r_str_alloc (&pcomponent->short_name, INVALID_SIZE_T, short_name);
	_r_str_alloc (&pcomponent->version, INVALID_SIZE_T, version);
	_r_str_alloc (&pcomponent->target_path, INVALID_SIZE_T, target_path);

	pcomponent->is_installer = is_installer;

	pupdateinfo->components.push_back (pcomponent);
}

void rapp::UpdateCheck (HWND hparent)
{
	if (!pupdateinfo)
		return;

	if (!hparent && (!ConfigGet (L"CheckUpdates", true).AsBool () || (_r_unixtime_now () - ConfigGet (L"CheckUpdatesLast", time_t (0)).AsLonglong ()) <= _APP_UPDATE_PERIOD))
		return;

	pupdateinfo->hthread = _r_createthread (&UpdateCheckThread, (LPVOID)pupdateinfo, true);

	if (!pupdateinfo->hthread)
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
#else
			_r_str_copy (str_content, _countof (str_content), L"Checking for new releases...");
#pragma _R_WARNING(IDS_UPDATE_INIT)
#endif // IDS_UPDATE_INIT

			UpdateDialogNavigate (nullptr, nullptr, TDF_SHOW_PROGRESS_BAR, TDCBF_CANCEL_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);

			return;

#ifndef _APP_NO_WINXP
		}
#endif // _APP_NO_WINXP
	}

	ResumeThread (pupdateinfo->hthread);
}
#endif // _APP_HAVE_UPDATES

void rapp::ConfigInit ()
{
	app_config_array.clear (); // reset
	_r_parseini (GetConfigPath (), app_config_array, nullptr);

#if !defined(_APP_CONSOLE)
	LocaleInit (); // initialize locale
#endif // !_APP_CONSOLE
}

rstring rapp::ConfigGet (LPCWSTR key, bool def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRIi32, def ? 1 : 0), name);
}

rstring rapp::ConfigGet (LPCWSTR key, INT def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRIi32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, UINT def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRIu32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LONG def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRId32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, ULONG def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRIu32, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LONG64 def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRId64, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, ULONG64 def, LPCWSTR name) const
{
	return ConfigGet (key, _r_fmt (L"%" PRIu64, def), name);
}

rstring rapp::ConfigGet (LPCWSTR key, LPCWSTR def, LPCWSTR name) const
{
	rstring cfg_name;

	if (_r_str_isempty (name))
		cfg_name = app_name_short;
	else
		cfg_name.Format (L"%s\\%s", app_name_short, name);

	// check key is exists
	if (app_config_array.find (cfg_name) != app_config_array.end ())
	{
		const rstringmap1& rmap = app_config_array.at (cfg_name);

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

void rapp::LogError (LPCWSTR fn, DWORD errcode, LPCWSTR desc, UINT tray_id)
{
	const time_t current_time = _r_unixtime_now ();

	rstring error_text;
	error_text.Format (L"\"%s\"," _R_DEBUG_BODY L",\"%s\"\r\n", _r_fmt_date (current_time, FDTF_SHORTDATE | FDTF_LONGTIME).GetString (), fn, errcode, desc, app_version);

	// print log for debuggers
	OutputDebugString (error_text);

	// write log to a file
	HANDLE hfile = CreateFile (GetLogPath (), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (_r_fs_isvalidhandle (hfile))
	{
		DWORD written;

		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (hfile, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask
			WriteFile (hfile, _R_DEBUG_HEADER, DWORD (_r_str_length (_R_DEBUG_HEADER) * sizeof (WCHAR)), &written, nullptr); // adds csv header
		}
		else
		{
			_r_fs_setpos (hfile, 0, FILE_END);
		}

		WriteFile (hfile, error_text.GetString (), DWORD (error_text.GetLength () * sizeof (WCHAR)), &written, nullptr);

		CloseHandle (hfile);
	}

	// show tray balloon
#if defined(_APP_HAVE_TRAY)
	if (tray_id && ConfigGet (L"IsErrorNotificationsEnabled", true).AsBool ())
	{
		if ((current_time - ConfigGet (L"ErrorNotificationsTimestamp", time_t (0)).AsLonglong ()) >= ConfigGet (L"ErrorNotificationsPeriod", time_t (4)).AsLonglong ()) // check for timeout (sec.)
		{
			_r_tray_popup (GetHWND (), tray_id, NIIF_WARNING, app_name, L"Something went wrong. Open debug log file in profile directory.");

			ConfigSet (L"ErrorNotificationsTimestamp", current_time);
		}
	}
#endif // _APP_HAVE_TRAY
}

#if !defined(_APP_CONSOLE)
bool rapp::ShowConfirmMessage (HWND hwnd, LPCWSTR main, LPCWSTR text, LPCWSTR config_cfg)
{
	if (config_cfg && !ConfigGet (config_cfg, true).AsBool ())
		return true;

	BOOL is_flagchecked = FALSE;

	INT result = 0;

#if !defined(_APP_NO_WINXP)
	if (IsVistaOrLater ())
	{
#endif
		TASKDIALOGCONFIG tdc = {0};
		RtlSecureZeroMemory (&tdc, sizeof (tdc));

		WCHAR str_main[128] = {0};
		WCHAR str_content[512] = {0};
		WCHAR str_flag[64] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetHINSTANCE ();
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
		tdc.pszWindowTitle = app_name;
		tdc.pfCallback = &_r_msg_callback;
		tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

		if (config_cfg)
		{
			tdc.pszVerificationText = str_flag;

#ifdef IDS_QUESTION_FLAG_CHK
			_r_str_copy (str_flag, _countof (str_flag), LocaleString (IDS_QUESTION_FLAG_CHK, nullptr));
#else
			_r_str_copy (str_flag, _countof (str_flag), L"Do not ask again");
#pragma _R_WARNING(IDS_QUESTION_FLAG_CHK)
#endif // IDS_QUESTION_FLAG_CHK
		}

		if (!_r_str_isempty (main))
		{
			tdc.pszMainInstruction = str_main;
			_r_str_copy (str_main, _countof (str_main), main);
		}

		if (!_r_str_isempty (text))
		{
			tdc.pszContent = str_content;
			_r_str_copy (str_content, _countof (str_content), text);
		}

		_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked);
#if !defined(_APP_NO_WINXP)
	}
	else
	{
		if (config_cfg)
		{
			rstring cfg_string;
			cfg_string.Format (L"%s\\%s", app_name_short, config_cfg);

			result = SHMessageBoxCheck (hwnd, text, app_name, MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TOPMOST, IDOK, cfg_string);

			// get checkbox value fron registry
			{
				HKEY hkey = nullptr;

				if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\DontShowMeThisDialogAgain", 0, KEY_WRITE | KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					if (result == IDOK || result == IDYES)
						is_flagchecked = (RegQueryValueEx (hkey, cfg_string, 0, nullptr, nullptr, nullptr) == ERROR_SUCCESS);

					RegDeleteValue (hkey, cfg_string);
					RegCloseKey (hkey);
				}
			}
		}
	}

	if (result <= 0 && MessageBox (hwnd, text, app_name, MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST) == IDYES) // fallback!
		return true;
#endif // !_APP_NO_WINXP

	if (result == IDOK || result == IDYES)
	{
		if (is_flagchecked)
			ConfigSet (config_cfg, false);

		return true;
	}

	return false;
}

void rapp::ShowErrorMessage (HWND hwnd, LPCWSTR main, DWORD errcode, HINSTANCE hmodule)
{
	HLOCAL buffer = nullptr;
	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, hmodule, errcode, 0, (LPWSTR)&buffer, 0, nullptr);

	WCHAR str_main[128] = {0};
	WCHAR str_content[512] = {0};
	WCHAR str_footer[128] = {0};

	LPWSTR text = (LPWSTR)buffer;
	_r_str_trim (text, L"\r\n "); // trim trailing whitespaces

	_r_str_copy (str_main, _countof (str_main), _r_str_isempty (main) ? L"It happens ;(" : main);
	_r_str_printf (str_content, _countof (str_content), L"%s (0x%08" PRIX32 L")", _r_str_isempty (text) ? L"n/a" : text, errcode);
	_r_str_copy (str_footer, _countof (str_footer), L"This information may provide clues as to what went wrong and how to fix it.");

#if !defined(_APP_NO_WINXP)
	if (IsVistaOrLater ())
	{
#endif // !_APP_NO_WINXP
		TASKDIALOGCONFIG tdc = {0};
		RtlSecureZeroMemory (&tdc, sizeof (tdc));

		TASKDIALOG_BUTTON td_buttons[2] = {0};

		WCHAR btn_1[64] = {0};
		WCHAR btn_2[64] = {0};

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_NO_SET_FOREGROUND | TDF_SIZE_TO_CONTENT;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetHINSTANCE ();
		tdc.pszWindowTitle = app_name;
		tdc.pszMainInstruction = str_main;
		tdc.pszContent = str_content;
		tdc.pszFooter = str_footer;
		tdc.pfCallback = &_r_msg_callback;
		tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

		tdc.pButtons = td_buttons;
		tdc.cButtons = _countof (td_buttons);

		td_buttons[0].nButtonID = IDYES;
		td_buttons[0].pszButtonText = btn_1;

		td_buttons[1].nButtonID = IDCLOSE;
		td_buttons[1].pszButtonText = btn_2;

		tdc.nDefaultButton = td_buttons[1].nButtonID;

#if defined(IDS_COPY)
		_r_str_copy (btn_1, _countof (btn_1), LocaleString (IDS_COPY, nullptr));
#else
		_r_str_copy (btn_1, _countof (btn_1), L"Copy");
#pragma _R_WARNING(IDS_COPY)
#endif // IDS_COPY

#if defined(IDS_CLOSE)
		_r_str_copy (btn_2, _countof (btn_2), LocaleString (IDS_CLOSE, nullptr));
#else
		_r_str_copy (btn_2, _countof (btn_2), L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

		INT result = 0;

		if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
		{
			if (result == td_buttons[0].nButtonID)
				_r_clipboard_set (nullptr, str_content, _r_str_length (str_content));
		}

#if !defined(_APP_NO_WINXP)
	}
	else
	{
		if (MessageBox (hwnd, _r_fmt (L"%s\r\n\r\n%s\r\n\r\n%s", str_main, str_content, str_footer), app_name, MB_YESNO | MB_DEFBUTTON2) == IDYES)
			_r_clipboard_set (nullptr, str_content, _r_str_length (str_content));
	}
#endif // !_APP_NO_WINXP

	SAFE_LOCAL_FREE (buffer);
}

INT rapp::ShowMessage (HWND hwnd, DWORD flags, LPCWSTR title, LPCWSTR main, LPCWSTR content) const
{
#if !defined(_APP_NO_WINXP)
	if (IsVistaOrLater ())
	{
#endif // !_APP_NO_WINXP

		TASKDIALOGCONFIG tdc = {0};
		RtlSecureZeroMemory (&tdc, sizeof (tdc));

		tdc.cbSize = sizeof (tdc);
		tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_NO_SET_FOREGROUND;
		tdc.hwndParent = hwnd;
		tdc.hInstance = GetHINSTANCE ();
		tdc.pfCallback = &_r_msg_callback;
		tdc.pszWindowTitle = !_r_str_isempty (title) ? title : app_name;
		tdc.pszMainInstruction = main;
		tdc.pszContent = content;

		// set icons
		if ((flags & MB_ICONMASK) == MB_USERICON)
		{
#ifdef IDI_MAIN
			tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
			tdc.pszMainIcon = MAKEINTRESOURCE (100);
#pragma _R_WARNING(IDI_MAIN)
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

		INT result = 0;

		if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
			return result;

#if !defined(_APP_NO_WINXP)
	}
	else
	{
		return MessageBox (hwnd, content, !_r_str_isempty (title) ? title : app_name, flags);
	}
#endif // !_APP_NO_WINXP

	return 0;
}

void rapp::CreateAboutWindow (HWND hwnd)
{
	static INT guard = 0;

	if (!guard)
	{
		guard += 1;

#ifdef _WIN64
#define architecture 64
#else
#define architecture 32
#endif // _WIN64

		WCHAR str_title[64] = {0};
		WCHAR str_content[512] = {0};

#ifdef IDS_ABOUT
		_r_str_copy (str_title, _countof (str_title), LocaleString (IDS_ABOUT, nullptr));
#else
		_r_str_copy (str_title, _countof (str_title), L"About");
#pragma _R_WARNING(IDS_ABOUT)
#endif

#if !defined(_APP_NO_WINXP)
		if (IsVistaOrLater ())
		{
#endif // !_APP_NO_WINXP

			INT result = 0;

			WCHAR str_main[128] = {0};
			WCHAR str_footer[256] = {0};

			WCHAR btn_1[64] = {0};
			WCHAR btn_2[64] = {0};

			TASKDIALOGCONFIG tdc = {0};
			RtlSecureZeroMemory (&tdc, sizeof (tdc));

			TASKDIALOG_BUTTON td_buttons[2] = {0};

			tdc.cbSize = sizeof (tdc);
			tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
			tdc.hwndParent = hwnd;
			tdc.hInstance = GetHINSTANCE ();
			tdc.pszFooterIcon = TD_INFORMATION_ICON;
			tdc.nDefaultButton = IDCLOSE;
			tdc.pszWindowTitle = str_title;
			tdc.pszMainInstruction = str_main;
			tdc.pszContent = str_content;
			tdc.pszFooter = str_footer;
			tdc.pfCallback = &_r_msg_callback;
			tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

			tdc.pButtons = td_buttons;
			tdc.cButtons = _countof (td_buttons);

			td_buttons[0].nButtonID = IDOK;
			td_buttons[0].pszButtonText = btn_1;

			td_buttons[1].nButtonID = IDCLOSE;
			td_buttons[1].pszButtonText = btn_2;

#if defined(IDI_MAIN)
			tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
			tdc.pszMainIcon = MAKEINTRESOURCE (100);
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN

#if defined(IDS_DONATE)
			_r_str_copy (btn_1, _countof (btn_1), LocaleString (IDS_DONATE, nullptr));
#else
			_r_str_copy (btn_1, _countof (btn_1), L"Give thanks!");
#pragma _R_WARNING(IDS_DONATE)
#endif // IDS_DONATE

#if defined(IDS_CLOSE)
			_r_str_copy (btn_2, _countof (btn_2), LocaleString (IDS_CLOSE, nullptr));
#else
			_r_str_copy (btn_2, _countof (btn_2), L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE

			_r_str_copy (str_main, _countof (str_main), app_name);
			_r_str_printf (str_content, _countof (str_content), L"Version %s, %" PRIu32 L"-bit (Unicode)\r\n%s\r\n\r\n<a href=\"%s\">%s</a> | <a href=\"%s\">%s</a>", app_version, architecture, app_copyright, _APP_WEBSITE_URL, _APP_WEBSITE_URL + 8, _APP_GITHUB_URL, _APP_GITHUB_URL + 8);
			_r_str_copy (str_footer, _countof (str_footer), L"This program is free software; you can redistribute it and/or modify it under the terms of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License 3</a> as published by the Free Software Foundation.");

			if (_r_msg_taskdialog (&tdc, &result, nullptr, nullptr))
			{
				if (result == td_buttons[0].nButtonID)
					ShellExecute (GetHWND (), nullptr, _r_fmt (_APP_DONATE_URL, app_name_short), nullptr, nullptr, SW_SHOWNORMAL);
			}
#if !defined(_APP_NO_WINXP)
		}
		else
		{
			_r_str_printf (str_content, _countof (str_content), L"%s\r\n\r\nVersion %s, %" PRIu32 L"-bit (Unicode)\r\n%s\r\n\r\n%s | %s\r\n\r\nThis program is free software; you can redistribute it and/\r\nor modify it under the terms of the GNU General Public\r\nLicense 3 as published by the Free Software Foundation.", app_name, app_version, architecture, app_copyright, _APP_WEBSITE_URL + 8, _APP_GITHUB_URL + 8);

			MSGBOXPARAMS mbp = {0};
			RtlSecureZeroMemory (&mbp, sizeof (mbp));

			mbp.cbSize = sizeof (mbp);
			mbp.dwStyle = MB_OK | MB_TOPMOST | MB_USERICON;
			mbp.hwndOwner = hwnd;
			mbp.hInstance = GetHINSTANCE ();
			mbp.lpszCaption = str_title;
			mbp.lpszText = str_content;

#if defined(IDI_MAIN)
			mbp.lpszIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
			mbp.lpszIcon = MAKEINTRESOURCE (100);
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			MessageBoxIndirect (&mbp);
		}
#endif // !_APP_NO_WINXP

		guard -= 1;
	}
}

bool rapp::IsClassicUI () const
{
	return ConfigGet (L"ClassicUI", _APP_CLASSICUI).AsBool () || !IsAppThemed ();
}
#endif // !_APP_CONSOLE

#if !defined(_APP_NO_WINXP)
bool rapp::IsVistaOrLater () const
{
	return is_vistaorlater;
}
#endif // !_APP_NO_WINXP

#if !defined(_APP_CONSOLE)
LRESULT CALLBACK rapp::MainWindowProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp *this_ptr = reinterpret_cast<rapp*>(GetWindowLongPtr (hwnd, GWLP_USERDATA));

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

			if (this_ptr->app_wndproc)
			{
				result = CallWindowProc (this_ptr->app_wndproc, hwnd, msg, wparam, lparam);
				DrawMenuBar (hwnd); // HACK!!!
			}

			return result;
		}

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

		case WM_SIZE:
		{
			if (wparam == SIZE_MAXIMIZED)
			{
				// prevent windows without maximize button to be maximized (dirty hack!!!)
				if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) == 0)
				{
					WINDOWPLACEMENT wpl = {0};

					wpl.length = sizeof (wpl);
					GetWindowPlacement (hwnd, &wpl);

					wpl.showCmd = SW_RESTORE;
					SetWindowPlacement (hwnd, &wpl);

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
			if (wparam && this_ptr->is_needmaximize)
			{
				ShowWindowAsync (hwnd, SW_SHOWMAXIMIZED);
				this_ptr->is_needmaximize = false;
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
				lpmmi->ptMinTrackSize.x = this_ptr->max_width;
				lpmmi->ptMinTrackSize.y = this_ptr->max_height;
			}

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
			}

			InvalidateRect (hwnd, nullptr, TRUE); // redraw window content when resizing ends (HACK!!!)

			break;
		}

		case WM_DPICHANGED:
		{
			// call main callback
			SendMessage (hwnd, RM_DPICHANGED, 0, 0);

			// change window size and position
			if ((GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
			{
				LPRECT lprcnew = reinterpret_cast<LPRECT>(lparam);

				if (lprcnew)
				{
					SetWindowPos (hwnd, nullptr, lprcnew->left, lprcnew->top, _R_RECT_WIDTH (lprcnew), _R_RECT_HEIGHT (lprcnew), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

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

bool rapp::CreateMainWindow (INT dlg_id, INT icon_id, DLGPROC dlg_proc)
{
	if (!dlg_id || !dlg_proc)
		return false;

	// show update message (if exist)
#if defined(_APP_HAVE_UPDATES)
	if (_r_fs_exists (GetUpdatePath ()))
	{
		WCHAR str_content[256] = {0};

#ifdef IDS_UPDATE_INSTALL
		_r_str_copy (str_content, _countof (str_content), LocaleString (IDS_UPDATE_INSTALL, nullptr));
#else
		_r_str_copy (str_content, _countof (str_content), L"Update available, do you want to install them?");
#pragma _R_WARNING(IDS_UPDATE_INSTALL)
#endif // IDS_UPDATE_INSTALL

		const INT code = ShowMessage (nullptr, MB_YESNOCANCEL | MB_USERICON | MB_TOPMOST, nullptr, nullptr, str_content);

		if (code == IDYES)
		{
			UpdateInstall ();
			return false;
		}
		else if (code == IDNO)
		{
			_r_fs_remove (GetUpdatePath (), RFS_FORCEREMOVE);
		}
		else
		{
			// continue...
		}
	}
#endif //_APP_HAVE_UPDATES

#if defined(_APP_HAVE_UPDATES)
	UpdateAddComponent (app_name, app_name_short, app_version, GetDirectory (), true);
	UpdateAddComponent (L"Language pack", L"language", _r_fmt (L"%" PRId64, LocaleGetVersion ()), GetLocalePath (), false);
#endif // _APP_HAVE_UPDATES

	// create main window
	app_hwnd = CreateDialog (nullptr, MAKEINTRESOURCE (dlg_id), nullptr, dlg_proc);

	if (!app_hwnd)
		return false;

	// enable messages bypass uipi (vista+)
#if !defined(_APP_NO_WINXP)
	if (IsVistaOrLater ())
#endif // !_APP_NO_WINXP
	{
		UINT messages[] = {
			WM_COPYDATA,
			WM_COPYGLOBALDATA,
			WM_DROPFILES,
#if defined(_APP_HAVE_TRAY)
			WM_TASKBARCREATED,
#endif // _APP_HAVE_TRAY
		};

		_r_wnd_changemessagefilter (app_hwnd, messages, _countof (messages), MSGFLT_ALLOW);
	}

	// set window prop
	SetProp (app_hwnd, app_name, "sync-1.02; iris fucyo; none of your damn business.");

	// subclass window
	SetWindowLongPtr (app_hwnd, GWLP_USERDATA, (LONG_PTR)this);
	app_wndproc = (WNDPROC)SetWindowLongPtr (app_hwnd, DWLP_DLGPROC, (LONG_PTR)&MainWindowProc);

	// update autorun settings
#if defined(_APP_HAVE_AUTORUN)
	if (AutorunIsEnabled ())
		AutorunEnable (nullptr, true);
#endif // _APP_HAVE_AUTORUN

	// update uac settings (vista+)
#if defined(_APP_HAVE_SKIPUAC)
	if (SkipUacIsEnabled ())
		SkipUacEnable (nullptr, true);
#endif // _APP_HAVE_SKIPUAC

	// set window on top
	_r_wnd_top (app_hwnd, ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());
	_r_wnd_center (app_hwnd, nullptr);

	// set minmax info
	{
		RECT rect_original = {0};

		if (GetWindowRect (app_hwnd, &rect_original))
		{
			max_width = _R_RECT_WIDTH (&rect_original);
			max_height = _R_RECT_HEIGHT (&rect_original);

#if defined(_APP_HAVE_MINSIZE)
			max_width /= 2;
			max_height /= 2;
#endif // _APP_HAVE_MINSIZE
		}
	}

	RestoreWindowPosition (app_hwnd, L"window");

	// send resize message
	if ((GetWindowLongPtr (app_hwnd, GWL_STYLE) & WS_SIZEBOX) != 0)
	{
		RECT rc_client = {0};

		GetClientRect (app_hwnd, &rc_client);
		SendMessage (app_hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM (_R_RECT_WIDTH (&rc_client), _R_RECT_HEIGHT (&rc_client)));
	}

	// set window icon
	if (icon_id)
	{
		SendMessage (app_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)GetSharedImage (GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXSMICON)));
		SendMessage (app_hwnd, WM_SETICON, ICON_BIG, (LPARAM)GetSharedImage (GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (app_hwnd, SM_CXICON)));
	}

	// set window visibility (or not?)
	{
		bool is_windowhidden = false;
		INT show_code = SW_SHOWNORMAL;

		STARTUPINFO si = {0};
		RtlSecureZeroMemory (&si, sizeof (si));

		si.cb = sizeof (si);

		GetStartupInfo (&si);

		if ((si.dwFlags & STARTF_USESHOWWINDOW) != 0)
			show_code = si.wShowWindow;

		// show window minimized
#if defined(_APP_HAVE_TRAY)
#if defined(_APP_STARTMINIMIZED)
		is_windowhidden = true;
#else
		// if window have tray - check arguments
		if (wcsstr (GetCommandLine (), L"/minimized") || ConfigGet (L"IsStartMinimized", false).AsBool ())
			is_windowhidden = true;
#endif // _APP_STARTMINIMIZED
#endif // _APP_HAVE_TRAY

		if (show_code == SW_HIDE || show_code == SW_MINIMIZE || show_code == SW_SHOWMINNOACTIVE || show_code == SW_FORCEMINIMIZE)
			is_windowhidden = true;

		if ((GetWindowLongPtr (app_hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) != 0)
		{
			if (show_code == SW_SHOWMAXIMIZED || ConfigGet (L"IsWindowZoomed", false, L"window").AsBool ())
			{
				if (is_windowhidden)
					is_needmaximize = true;

				else
					show_code = SW_SHOWMAXIMIZED;
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
	if (ConfigGet (L"CheckUpdates", true).AsBool ())
		UpdateCheck (nullptr);
#endif // _APP_HAVE_UPDATES

	return true;
}

void rapp::RestoreWindowPosition (HWND hwnd, LPCWSTR window_name)
{
	// restore window position
	RECT rect_original = {0};
	GetWindowRect (hwnd, &rect_original);

	// restore window position
	RECT rect_new = {0};

	rect_new.left = ConfigGet (L"WindowPosX", rect_original.left, window_name).AsLong ();
	rect_new.top = ConfigGet (L"WindowPosY", rect_original.top, window_name).AsLong ();

	const bool is_resizeable = (GetWindowLongPtr (hwnd, GWL_STYLE) & WS_SIZEBOX) != 0;

	if (is_resizeable)
	{
		rect_new.right = (std::max) (ConfigGet (L"WindowPosWidth", _R_RECT_WIDTH (&rect_original), window_name).AsLong (), _R_RECT_WIDTH (&rect_original)) + rect_new.left;
		rect_new.bottom = (std::max) (ConfigGet (L"WindowPosHeight", _R_RECT_HEIGHT (&rect_original), window_name).AsLong (), _R_RECT_HEIGHT (&rect_original)) + rect_new.top;
	}
	else
	{
		rect_new.right = _R_RECT_WIDTH (&rect_original) + rect_new.left;
		rect_new.bottom = _R_RECT_HEIGHT (&rect_original) + rect_new.top;
	}

	_r_wnd_adjustwindowrect (nullptr, &rect_new);

	if (is_resizeable)
		SetWindowPos (hwnd, nullptr, rect_new.left, rect_new.top, _R_RECT_WIDTH (&rect_new), _R_RECT_HEIGHT (&rect_new), SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	else
		SetWindowPos (hwnd, nullptr, rect_new.left, rect_new.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}
#endif // !(_APP_CONSOLE

#if defined(_APP_HAVE_SETTINGS)
void rapp::CreateSettingsWindow (HWND hwnd, DLGPROC dlg_proc, INT dlg_id)
{
	if (GetSettingsWindow ())
	{
		_r_wnd_toggle (GetSettingsWindow (), true);
		return;
	}

	if (dlg_id)
		ConfigSet (L"SettingsLastPage", dlg_id);

	if (dlg_proc)
		app_settings_proc = dlg_proc;

	SHORT width = 0;
	SHORT height = 0;
	const WORD controls = 3;

	// calculate dialog size
	{
		for (auto &ptr_page : app_settings_pages)
		{
			if (ptr_page && ptr_page->dlg_id)
			{
				LPDLGTEMPLATEEX pdlg = (LPDLGTEMPLATEEX)_r_loadresource (GetHINSTANCE (), MAKEINTRESOURCE (ptr_page->dlg_id), RT_DIALOG, nullptr);

				if (pdlg)
				{
					if (width < pdlg->cx)
						width = pdlg->cx;

					if (height < pdlg->cy)
						height = pdlg->cy;
				}
			}
		}

		height += 38;
		width += 112;
	}

	constexpr size_t size = ((sizeof (DLGTEMPLATEEX) + (sizeof (WORD) * 8)) + ((sizeof (DLGITEMTEMPLATEEX) + (sizeof (WORD) * 3)) * controls)) + 128;

	LPVOID lpbuffer = _r_mem_allocex (size, HEAP_ZERO_MEMORY);

	if (!lpbuffer)
		return;

	LPBYTE pPtr = (LPBYTE)lpbuffer;

	// fill DLGTEMPLATEEX
	rhelper::template_writevar (&pPtr, WORD (1), sizeof (WORD)); // dlgVer
	rhelper::template_writevar (&pPtr, WORD (0xFFFF), sizeof (WORD)); // signature

	rhelper::template_writevar (&pPtr, DWORD (0), sizeof (DWORD)); // helpID
	rhelper::template_writevar (&pPtr, DWORD (WS_EX_APPWINDOW | WS_EX_CONTROLPARENT), sizeof (DWORD)); // exStyle
	rhelper::template_writevar (&pPtr, DWORD (WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_SHELLFONT | DS_MODALFRAME), sizeof (DWORD)); // style

	rhelper::template_write (&pPtr, &controls, sizeof (controls)); // cdit

	rhelper::template_writevar (&pPtr, SHORT (0), sizeof (SHORT)); // x
	rhelper::template_writevar (&pPtr, SHORT (0), sizeof (SHORT)); // y
	rhelper::template_write (&pPtr, &width, sizeof (width)); // cx
	rhelper::template_write (&pPtr, &height, sizeof (height)); // cy

	// additional data
	rhelper::template_writestring (&pPtr, L""); // menu
	rhelper::template_writestring (&pPtr, L""); // windowClass
	rhelper::template_writestring (&pPtr, L""); // title

	// set font
	rhelper::template_writevar (&pPtr, SHORT (8), sizeof (SHORT)); // pointsize
	rhelper::template_writevar (&pPtr, SHORT (FW_NORMAL), sizeof (SHORT)); // weight
	rhelper::template_writevar (&pPtr, SHORT (FALSE), sizeof (SHORT)); // bItalic
	rhelper::template_writestring (&pPtr, L"MS Shell Dlg"); // font

	rhelper::template_writecontrol (&pPtr, IDC_NAV, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_TRACKSELECT | TVS_INFOTIP | TVS_NOHSCROLL, 8, 6, 88, (height - 14), WC_TREEVIEW);
	rhelper::template_writecontrol (&pPtr, IDC_RESET, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, 88 + 14, (height - 22), 50, 14, WC_BUTTON);
	rhelper::template_writecontrol (&pPtr, IDC_CLOSE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | BS_PUSHBUTTON, (width - 58), (height - 22), 50, 14, WC_BUTTON);

	DialogBoxIndirectParam (GetHINSTANCE (), (LPCDLGTEMPLATE)lpbuffer, hwnd, &SettingsWndProc, (LPARAM)this);

	_r_mem_free (lpbuffer);
}

void rapp::SettingsAddPage (INT dlg_id, UINT locale_id)
{
	PAPP_SETTINGS_PAGE ptr_page = new APP_SETTINGS_PAGE;
	RtlSecureZeroMemory (ptr_page, sizeof (APP_SETTINGS_PAGE));

	ptr_page->dlg_id = dlg_id;
	ptr_page->locale_id = locale_id;

	app_settings_pages.push_back (ptr_page);
}

HWND rapp::GetSettingsWindow ()
{
	return settings_hwnd;
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

LPCWSTR rapp::GetLogPath () const
{
	return app_logpath;
}

#if !defined(_APP_CONSOLE)
LPCWSTR rapp::GetLocalePath () const
{
	return app_localepath;
}
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_UPDATES)
LPCWSTR rapp::GetUpdatePath () const
{
	return app_updatepath;
}
#endif // _APP_HAVE_UPDATES

rstring rapp::GetProxyConfiguration () const
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

#if !defined (_APP_CONSOLE)
HINSTANCE rapp::GetHINSTANCE () const
{
	return app_hinstance;
}

HWND rapp::GetHWND () const
{
	return app_hwnd;
}

HICON rapp::GetSharedImage (HINSTANCE hinst, INT icon_id, INT icon_size)
{
	for (auto &pimage : app_shared_icons)
	{
		if (pimage && (pimage->hinst == hinst) && (pimage->icon_id == icon_id) && (pimage->icon_size == icon_size))
			return pimage->hicon;
	}

	const HICON hicon = _r_loadicon (hinst, MAKEINTRESOURCE (icon_id), icon_size);

	if (!hicon)
		return nullptr;

	PAPP_SHARED_IMAGE pimage = new APP_SHARED_IMAGE;
	RtlSecureZeroMemory (pimage, sizeof (APP_SHARED_IMAGE));

	pimage->hinst = hinst;
	pimage->icon_id = icon_id;
	pimage->icon_size = icon_size;
	pimage->hicon = hicon;

	app_shared_icons.push_back (pimage);

	return hicon;
}
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_SETTINGS)
void rapp::LocaleApplyFromControl (HWND hwnd, INT ctrl_id)
{
	const rstring text = _r_ctrl_gettext (hwnd, ctrl_id);

	if (_r_str_compare (text, _APP_LANGUAGE_DEFAULT) == 0)
	{
		SAFE_DELETE_ARRAY (app_locale_current);
	}
	else
	{
		_r_str_alloc (&app_locale_current, INVALID_SIZE_T, text);
	}

	ConfigSet (L"Language", text);

	// refresh settings window
	const HWND hsettings = GetSettingsWindow ();

	if (hsettings)
		PostMessage (hsettings, RM_LOCALIZE, 0, 0);

	// refresh main window
	const HWND hmain = GetHWND ();

	if (hmain)
		PostMessage (hmain, RM_LOCALIZE, 0, 0);
}
#endif // _APP_HAVE_SETTINGS

#if !defined(_APP_CONSOLE)
void rapp::LocaleApplyFromMenu (HMENU hmenu, UINT selected_id, UINT default_id)
{
	if (selected_id == default_id)
	{
		ConfigSet (L"Language", _APP_LANGUAGE_DEFAULT);
		SAFE_DELETE_ARRAY (app_locale_current);
	}
	else
	{
		WCHAR name[LOCALE_NAME_MAX_LENGTH] = {0};
		GetMenuString (hmenu, selected_id, name, _countof (name), MF_BYCOMMAND);

		ConfigSet (L"Language", name);
		_r_str_alloc (&app_locale_current, INVALID_SIZE_T, name);
	}

	// refresh main window
	const HWND hmain = GetHWND ();

	if (hmain)
		PostMessage (hmain, RM_LOCALIZE, 0, 0);
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
			_r_menu_enable (hmenu, ctrl_id, MF_BYPOSITION, true);
			AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);
		}
		else
		{
			_r_ctrl_enable (hwnd, ctrl_id, true);
		}

		for (auto &name : app_locale_names)
		{
			const bool is_current = app_locale_current && (_r_str_compare (app_locale_current, name) == 0);

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
			_r_menu_enable (hmenu, ctrl_id, MF_BYPOSITION, false);
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

	app_locale_timetamp = 0;

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
	{
		_r_str_alloc (&app_locale_current, INVALID_SIZE_T, name);
	}
	else
	{
		SAFE_DELETE_ARRAY (app_locale_current);
	}
}

rstring rapp::LocaleString (UINT uid, LPCWSTR append)
{
	rstring result;

	if (uid)
	{
		rstring key_name;
		key_name.Format (L"%03" PRIu32, uid);

		if (!_r_str_isempty (app_locale_current))
		{
			// check key is exists
			if (app_locale_array.find (app_locale_current) != app_locale_array.end ())
			{
				rstringmap1& rmap = app_locale_array[app_locale_current];

				if (rmap.find (key_name) != rmap.end ())
				{
					result = rmap[key_name];
				}
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

void rapp::LocaleMenu (HMENU hmenu, UINT uid, UINT item, BOOL by_position, LPCWSTR append)
{
	WCHAR buffer[128] = {0};
	_r_str_copy (buffer, _countof (buffer), LocaleString (uid, append));

	MENUITEMINFO mi = {0};

	mi.cbSize = sizeof (mi);
	mi.fMask = MIIM_STRING;
	mi.dwTypeData = buffer;

	SetMenuItemInfo (hmenu, item, by_position, &mi);
}
#endif // !_APP_CONSOLE

#if defined(_APP_HAVE_SETTINGS)
INT_PTR CALLBACK rapp::SettingsWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static rapp *this_ptr = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			this_ptr = reinterpret_cast<rapp*>(lparam);
			this_ptr->settings_hwnd = hwnd;

#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

#ifdef IDI_MAIN
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)this_ptr->GetSharedImage (this_ptr->GetHINSTANCE (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)this_ptr->GetSharedImage (this_ptr->GetHINSTANCE (), IDI_MAIN, _r_dc_getsystemmetrics (hwnd, SM_CXICON)));
#else
#pragma _R_WARNING(IDI_MAIN)
#endif // IDI_MAIN

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure navigation control
			const INT dlg_id = this_ptr->ConfigGet (L"SettingsLastPage", this_ptr->app_settings_pages.at (0)->dlg_id).AsInt ();

			_r_treeview_setstyle (hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, _r_dc_getdpi (hwnd, _R_SIZE_ITEMHEIGHT), _r_dc_getdpi (hwnd, _R_SIZE_TREEINDENT));

			for (auto &ptr_page : this_ptr->app_settings_pages)
			{
				if (!ptr_page)
					continue;

				if (ptr_page->dlg_id)
				{
					ptr_page->hwnd = CreateDialog (this_ptr->GetHINSTANCE (), MAKEINTRESOURCE (ptr_page->dlg_id), hwnd, this_ptr->app_settings_proc);

					if (ptr_page->hwnd)
						SendMessage (ptr_page->hwnd, RM_INITIALIZE, (WPARAM)ptr_page->dlg_id, 0);

					HTREEITEM hitem = _r_treeview_additem (hwnd, IDC_NAV, this_ptr->LocaleString (ptr_page->locale_id, nullptr), nullptr, I_IMAGENONE, (LPARAM)ptr_page);

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
			SetWindowText (hwnd, this_ptr->LocaleString (IDS_SETTINGS, nullptr));
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
					_r_treeview_setitem (hwnd, IDC_NAV, hitem, this_ptr->LocaleString (ptr_page->locale_id, nullptr));

					if (ptr_page->hwnd && IsWindowVisible (ptr_page->hwnd))
						PostMessage (ptr_page->hwnd, RM_LOCALIZE, (WPARAM)ptr_page->dlg_id, 0);
				}

				hitem = (HTREEITEM)SendDlgItemMessage (hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hitem);
			}

			// localize buttons
#ifdef IDC_RESET
#ifdef IDS_RESET
			SetDlgItemText (hwnd, IDC_RESET, this_ptr->LocaleString (IDS_RESET, nullptr));
#else
			SetDlgItemText (hwnd, IDC_RESET, L"Reset");
#pragma _R_WARNING(IDS_RESET)
#endif // IDS_RESET
			_r_wnd_addstyle (hwnd, IDC_RESET, this_ptr->IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#else
#pragma _R_WARNING(IDC_RESET)
#endif // IDC_RESET

#ifdef IDC_CLOSE
#ifdef IDS_CLOSE
			SetDlgItemText (hwnd, IDC_CLOSE, this_ptr->LocaleString (IDS_CLOSE, nullptr));
#else
			SetDlgItemText (hwnd, IDC_CLOSE, L"Close");
#pragma _R_WARNING(IDS_CLOSE)
#endif // IDS_CLOSE
			_r_wnd_addstyle (hwnd, IDC_CLOSE, this_ptr->IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
#else
#pragma _R_WARNING(IDC_CLOSE)
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
			if (!this_ptr) // fix npe!
				break;

			for (auto &ptr_page : this_ptr->app_settings_pages)
			{
				if (ptr_page && ptr_page->hwnd)
				{
					DestroyWindow (ptr_page->hwnd);
					ptr_page->hwnd = nullptr;
				}
			}

			this_ptr->settings_hwnd = nullptr;

			_r_wnd_top (this_ptr->GetHWND (), this_ptr->ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());

#ifdef _APP_HAVE_UPDATES
			if (this_ptr->ConfigGet (L"CheckUpdates", true).AsBool ())
				this_ptr->UpdateCheck (nullptr);
#endif // _APP_HAVE_UPDATES

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
				case TVN_SELCHANGING:
				{
					LPNMTREEVIEW lptv = (LPNMTREEVIEW)lparam;

					const PAPP_SETTINGS_PAGE ptr_page_old = (PAPP_SETTINGS_PAGE)lptv->itemOld.lParam;
					const PAPP_SETTINGS_PAGE ptr_page_new = (PAPP_SETTINGS_PAGE)lptv->itemNew.lParam;

					if (ptr_page_old && ptr_page_old->hwnd && IsWindowVisible (ptr_page_old->hwnd))
						ShowWindow (ptr_page_old->hwnd, SW_HIDE);

					if (!ptr_page_new || IsWindowVisible (ptr_page_new->hwnd))
						break;

					this_ptr->ConfigSet (L"SettingsLastPage", ptr_page_new->dlg_id);

					if (ptr_page_new->hwnd)
					{
						RECT rc_tree = {0};
						RECT rc_child = {0};

						GetWindowRect (lphdr->hwndFrom, &rc_tree);
						MapWindowPoints (nullptr, hwnd, (LPPOINT)&rc_tree, 2);

						GetClientRect (ptr_page_new->hwnd, &rc_child);

						PostMessage (ptr_page_new->hwnd, RM_LOCALIZE, (WPARAM)ptr_page_new->dlg_id, 0);

						SetWindowPos (ptr_page_new->hwnd, nullptr, (rc_tree.left * 2) + _R_RECT_WIDTH (&rc_tree), rc_tree.top, _R_RECT_WIDTH (&rc_child), _R_RECT_HEIGHT (&rc_child), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
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
					WCHAR str_content[256] = {0};

#ifdef IDS_QUESTION_RESET
					_r_str_copy (str_content, _countof (str_content), this_ptr->LocaleString (IDS_QUESTION_RESET, nullptr).GetString ());
#else
					_r_str_copy (str_content, _countof (str_content), L"Are you really sure you want to reset all application settings?");
#pragma _R_WARNING(IDS_QUESTION_RESET)
#endif // IDS_QUESTION_RESET

					if (this_ptr->ShowConfirmMessage (hwnd, nullptr, str_content, nullptr))
					{
						const time_t current_timestamp = _r_unixtime_now ();

#ifdef _APP_HAVE_AUTORUN
						if (this_ptr->AutorunIsEnabled ())
							this_ptr->AutorunEnable (nullptr, false);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_SKIPUAC
						if (this_ptr->SkipUacIsEnabled ())
							this_ptr->SkipUacEnable (nullptr, false);
#endif // _APP_HAVE_SKIPUAC

						_r_fs_makebackup (this_ptr->GetConfigPath (), current_timestamp);
						_r_fs_remove (this_ptr->GetConfigPath (), RFS_FORCEREMOVE);

						this_ptr->ConfigInit ();

						// reinitialize application
						const HWND hmain = this_ptr->GetHWND ();

						if (hmain)
						{
							SendMessage (hmain, RM_INITIALIZE, 0, 0);
							SendMessage (hmain, RM_LOCALIZE, 0, 0);

							SendMessage (hmain, WM_EXITSIZEMOVE, 0, 0); // reset size and pos

							SendMessage (hmain, RM_CONFIG_RESET, 0, (LPARAM)current_timestamp);
						}

						// reinitialize settings
						SendMessage (hwnd, RM_LOCALIZE, 0, 0);

						for (auto &ptr_page : this_ptr->app_settings_pages)
						{
							if (ptr_page && ptr_page->hwnd)
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

#if defined(_APP_HAVE_UPDATES)
bool rapp::UpdateDownloadCallback (DWORD total_written, DWORD total_length, LONG_PTR lpdata)
{
	PAPP_UPDATE_INFO pupdateinfo = reinterpret_cast<PAPP_UPDATE_INFO>(lpdata);

	if (!pupdateinfo)
		return true;

	rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

#if !defined(_APP_NO_WINXP)
	if (this_ptr->IsVistaOrLater ())
	{
#endif // !_APP_NO_WINXP

		if (pupdateinfo->htaskdlg)
		{
			WCHAR str_content[256] = {0};

			const INT percent = (INT)_R_PERCENT_OF (total_written, total_length);

#ifdef IDS_UPDATE_DOWNLOAD
			_r_str_printf (str_content, _countof (str_content), this_ptr->LocaleString (IDS_UPDATE_DOWNLOAD, L" %d%%"), percent);
#else
			_r_str_printf (str_content, _countof (str_content), L"Downloading update... %d%%", percent);
#pragma _R_WARNING(IDS_UPDATE_DOWNLOAD)
#endif // IDS_UPDATE_DOWNLOAD

			SendMessage (pupdateinfo->htaskdlg, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)str_content);
			SendMessage (pupdateinfo->htaskdlg, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
		}

#if !defined(_APP_NO_WINXP)
	}
#endif // !_APP_NO_WINXP

	return true;
}

UINT WINAPI rapp::UpdateDownloadThread (LPVOID lparam)
{
	PAPP_UPDATE_INFO pupdateinfo = (PAPP_UPDATE_INFO)lparam;

	if (pupdateinfo)
	{
		rapp *this_ptr = static_cast<rapp*>(pupdateinfo->papp);

		rstring proxy_addr = this_ptr->GetProxyConfiguration ();
		HINTERNET hsession = _r_inet_createsession (this_ptr->GetUserAgent (), proxy_addr);

		bool is_downloaded = false;
		bool is_downloaded_installer = false;
		bool is_updated = false;

		if (hsession)
		{
			for (auto &pcomponent : pupdateinfo->components)
			{
				if (pcomponent && pcomponent->is_haveupdate)
				{
					HANDLE hfile = CreateFile (pcomponent->temp_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

					if (_r_fs_isvalidhandle (hfile))
					{
						if (_r_inet_downloadurl (hsession, proxy_addr, pcomponent->url, (LONG_PTR)hfile, true, &this_ptr->UpdateDownloadCallback, (LONG_PTR)pupdateinfo) == ERROR_SUCCESS)
						{
							SAFE_DELETE_HANDLE (hfile); // required!

							pcomponent->is_haveupdate = false;

							is_downloaded = true;

							if (pcomponent->is_installer)
							{
								is_downloaded_installer = true;
								break;
							}
							else
							{
								// copy required files
								SetFileAttributes (pcomponent->target_path, FILE_ATTRIBUTE_NORMAL);

								_r_fs_move (pcomponent->temp_path, pcomponent->target_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
								_r_fs_remove (pcomponent->temp_path, RFS_FORCEREMOVE);

								// set new version
								SAFE_DELETE_ARRAY (pcomponent->version);

								pcomponent->version = pcomponent->new_version;
								pcomponent->new_version = nullptr;

								is_updated = true;
							}
						}

						if (_r_fs_isvalidhandle (hfile))
							CloseHandle (hfile);
					}
				}
			}

			_r_inet_close (hsession);
		}

		// show result text
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
			_r_str_copy (str_content, _countof (str_content), L"Update server connection error.");
#pragma _R_WARNING(IDS_UPDATE_ERROR)
#endif // IDS_UPDATE_ERROR
		}

#if !defined(_APP_NO_WINXP)
		if (this_ptr->IsVistaOrLater ())
		{
#endif // !_APP_NO_WINXP

			if (pupdateinfo->htaskdlg)
				this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, (is_downloaded ? nullptr : TD_WARNING_ICON), 0, is_downloaded_installer ? TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON : TDCBF_CLOSE_BUTTON, nullptr, str_content, (LONG_PTR)pupdateinfo);

#if !defined(_APP_NO_WINXP)
		}
		else
		{
			if (pupdateinfo->hparent)
				this_ptr->ShowMessage (pupdateinfo->hparent, is_downloaded_installer ? MB_OKCANCEL : MB_OK | (is_downloaded ? MB_USERICON : MB_ICONEXCLAMATION), nullptr, nullptr, str_content);
		}
#endif // !_APP_NO_WINXP

		// install update
		if (is_updated)
		{
			this_ptr->ConfigInit (); // reload configuration

			const HWND hmain = this_ptr->GetHWND ();

			if (hmain)
			{
				SendMessage (hmain, RM_CONFIG_UPDATE, 0, 0);
				SendMessage (hmain, RM_INITIALIZE, 0, 0);

				SendMessage (hmain, RM_LOCALIZE, 0, 0);
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

#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, 0);
			SendMessage (hwnd, WM_SETICON, ICON_BIG, 0);

			if (_r_fs_isvalidhandle (pupdateinfo->hthread))
				ResumeThread (pupdateinfo->hthread);

			break;
		}

		case TDN_DESTROYED:
		{
			SAFE_DELETE_HANDLE (pupdateinfo->hthread);
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
#if defined(_DEBUG) || defined(_APP_BETA)
		const bool is_beta = true;
#else
		const bool is_beta = this_ptr->ConfigGet (L"CheckUpdatesBeta", false).AsBool ();
#endif // _DEBUG || _APP_BETA

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
						this_ptr->ShowMessage (pupdateinfo->hparent, MB_OK | MB_ICONWARNING, nullptr, nullptr, str_content);
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

					for (auto &pcomponent : pupdateinfo->components)
					{
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

								pcomponent->is_haveupdate = true;

								_r_str_alloc (&pcomponent->new_version, new_version.GetLength (), new_version);
								_r_str_alloc (&pcomponent->url, new_url.GetLength (), new_url);

								updates_text.AppendFormat (L"%s %s\r\n", pcomponent->full_name, rhelper::version_format (new_version).GetString ());

								if (pcomponent->is_installer)
								{
									_r_str_alloc (&pcomponent->temp_path, INVALID_SIZE_T, this_ptr->GetUpdatePath ());

									// do not check components when new version of application available
									break;
								}
								else
								{
									_r_str_alloc (&pcomponent->temp_path, INVALID_SIZE_T, _r_fmt (L"%s\\%s-%s-%s.tmp", _r_path_expand (L"%temp%\\").GetString (), this_ptr->app_name_short, pcomponent->short_name, new_version.GetString ()));
								}
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
#else
						if (this_ptr->IsVistaOrLater ())
						{
							this_ptr->UpdateDialogNavigate (pupdateinfo->htaskdlg, nullptr, 0, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, str_main, updates_text, (LONG_PTR)pupdateinfo);
						}
						else
						{
							const INT msg_id = this_ptr->ShowMessage (pupdateinfo->hparent, MB_YESNO | MB_USERICON, nullptr, str_main, updates_text.GetString ());

							if (msg_id == IDYES)
								pupdateinfo->hthread = _r_createthread (&this_ptr->UpdateDownloadThread, (LPVOID)pupdateinfo, false);
						}
#endif // _APP_NO_WINXP
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

INT rapp::UpdateDialogNavigate (HWND htaskdlg, LPCWSTR main_icon, TASKDIALOG_FLAGS flags, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, LPCWSTR main, LPCWSTR content, LONG_PTR lpdata)
{
	TASKDIALOGCONFIG tdc = {0};
	RtlSecureZeroMemory (&tdc, sizeof (tdc));

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
#if defined(IDI_MAIN)
		tdc.pszMainIcon = MAKEINTRESOURCE (IDI_MAIN);
#else
		tdc.pszMainIcon = MAKEINTRESOURCE (100);
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
#endif // _APP_HAVE_UPDATES

#if defined(_APP_HAVE_SKIPUAC)
bool rapp::SkipUacIsEnabled () const
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

	return ConfigGet (L"SkipUacIsEnabled", false).AsBool ();
}

HRESULT rapp::SkipUacEnable (HWND hwnd, bool is_enable)
{
#ifndef _APP_NO_WINXP
	if (!IsVistaOrLater ())
		return false;
#endif // _APP_NO_WINXP

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

	HRESULT result = CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service);

	if (SUCCEEDED (result))
	{
		result = service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ());

		if (SUCCEEDED (result))
		{
			MBSTR root (L"\\");

			result = service->GetFolder (root, &folder);

			if (SUCCEEDED (result))
			{
				MBSTR name (_r_fmt (_APP_TASKSCHD_NAME, app_name_short));

				if (is_enable)
				{
					MBSTR author (_APP_AUTHOR);
					MBSTR path (GetBinaryPath ());
					MBSTR directory (GetDirectory ());
					MBSTR args (L"$(Arg0)");
					MBSTR timelimit (L"PT0S");

					// create task
					result = service->NewTask (0, &taskdef);

					if (SUCCEEDED (result))
					{
						if (SUCCEEDED (taskdef->get_RegistrationInfo (&reginfo)))
						{
							reginfo->put_Author (author);
							reginfo->Release ();
						}

						if (SUCCEEDED (taskdef->get_Principal (&principal)))
						{
							principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
							principal->put_LogonType (TASK_LOGON_INTERACTIVE_TOKEN);

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
								// TASK_COMPATIBILITY_V2_4 - win10
								// TASK_COMPATIBILITY_V2_3 - win8.1
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

						result = folder->RegisterTaskDefinition (name, taskdef, TASK_CREATE_OR_UPDATE, _variant_t (), _variant_t (), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t (), &registered_task);

						if (SUCCEEDED (result))
						{
							ConfigSet (L"SkipUacIsEnabled", true);

							registered_task->Release ();
						}

						taskdef->Release ();
					}
				}
				else
				{
					// remove task
					result = folder->DeleteTask (name, 0);

					if (SUCCEEDED (result) || result == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
					{
						ConfigSet (L"SkipUacIsEnabled", false);

						if (result == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
							result = S_OK;
					}
				}

				folder->Release ();
			}
		}

		service->Release ();
	}

	if (hwnd && FAILED (result))
		ShowErrorMessage (hwnd, nullptr, result, nullptr);

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

	if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
	{
		if (SUCCEEDED (service->Connect (_variant_t (), _variant_t (), _variant_t (), _variant_t ())))
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

#if !defined(_APP_CONSOLE)
void rapp::Restart (HWND hwnd)
{
	WCHAR str_content[512] = {0};

#ifdef IDS_RESTART
	_r_str_copy (str_content, _countof (str_content), LocaleString (IDS_RESTART, nullptr));
#else
	_r_str_copy (str_content, _countof (str_content), L"Restart required to apply changed configuration, do you want restart now?");
#pragma _R_WARNING(IDS_RESTART)
#endif // IDS_RESTART

	if (!hwnd || ShowMessage (hwnd, MB_YESNO | MB_ICONQUESTION, nullptr, nullptr, str_content) != IDYES)
		return;

	WCHAR path[MAX_PATH] = {0};
	_r_str_copy (path, _countof (path), GetBinaryPath ());

	WCHAR directory[MAX_PATH] = {0};
	GetCurrentDirectory (_countof (directory), directory);

	WCHAR args[MAX_PATH] = {0};
	_r_str_copy (args, _countof (args), GetCommandLine ());

	const bool is_mutexdestroyed = MutexDestroy ();

	if (!_r_run (path, args, directory, SW_SHOW))
	{
		if (is_mutexdestroyed)
			MutexCreate (); // restore mutex on error

		return;
	}

	const HWND hmain = GetHWND ();

	if (hmain)
	{
		DestroyWindow (hmain);
		WaitForSingleObjectEx (hmain, 3000, FALSE); // wait for exit
	}

	ExitProcess (ERROR_SUCCESS);
}

bool rapp::RunAsAdmin ()
{
#if defined(_APP_HAVE_SKIPUAC)
	if (SkipUacRun ())
		return true;
#endif // _APP_HAVE_SKIPUAC

	SHELLEXECUTEINFO shex = {0};
	RtlSecureZeroMemory (&shex, sizeof (shex));

	WCHAR path[MAX_PATH] = {0};
	_r_str_copy (path, _countof (path), GetBinaryPath ());

	WCHAR directory[MAX_PATH] = {0};
	GetCurrentDirectory (_countof (directory), directory);

	WCHAR args[MAX_PATH] = {0};
	_r_str_copy (args, _countof (args), GetCommandLine ());

	shex.cbSize = sizeof (shex);
	shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	shex.lpVerb = L"runas";
	shex.nShow = SW_SHOW;
	shex.lpFile = path;
	shex.lpDirectory = directory;
	shex.lpParameters = args;

	const bool is_mutexdestroyed = MutexDestroy ();

	if (ShellExecuteEx (&shex))
		return true;

	if (is_mutexdestroyed)
		MutexCreate (); // restore mutex on error

	_r_sleep (250); // HACK!!! prevent loop

	return false;
}
#endif // !_APP_CONSOLE
