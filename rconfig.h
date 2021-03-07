// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#pragma once

// Warning message macro
#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define R_PRINT_WARNING(desc) message("[WARNING!] Compile-time warning: " #desc  " (" STRINGIZE(__FILE__) ":" STRINGIZE(__LINE__) ")")

//#define APP_BETA // app has pre-release status

//#define APP_NO_APPDATA // use portable builds
//#define APP_NO_CONFIG // do not save configuration
//#define APP_NO_DEPRECATIONS // disable deprecated windows version support (support only win7+)
//#define APP_NO_GUEST // disable "guest" mode, running without admin rights; see APP_HAVE_SKIPUAC
//#define APP_NO_MUTEX // disable mutex

//#define APP_HAVE_AUTORUN // enable autorun feature
//#define APP_HAVE_SETTINGS // enable settings dialog feature
//#define APP_HAVE_SETTINGS_TABS // enable settings dialog feature baased on treeview
//#define APP_HAVE_SKIPUAC // enable skip uac feature
//#define APP_HAVE_TRAY // enable tray icon feature
//#define APP_HAVE_UPDATES // enable update checking feature

#if defined(APP_CONSOLE)
#define APP_NO_APPDATA
#define APP_NO_CONFIG
#undef APP_HAVE_AUTORUN
#undef APP_HAVE_SETTINGS
#undef APP_HAVE_SETTINGS_TABS
#undef APP_HAVE_SKIPUAC
#undef APP_HAVE_TRAY
#undef APP_HAVE_UPDATES
#endif // APP_CONSOLE

#if defined(APP_HAVE_SETTINGS_TABS)
#define APP_HAVE_SETTINGS
#endif // APP_HAVE_SETTINGS_TABS

// set defaults
#if !defined(APP_CLASSICUI)
#define APP_CLASSICUI FALSE
#endif // APP_CLASSICUI

#if !defined(APP_ALWAYSONTOP)
#define APP_ALWAYSONTOP FALSE
#endif // APP_ALWAYSONTOP

// printf specifiers
#define PR_LONG "ld"
#define PR_LONG64 "lld"

#define PR_ULONG "lu"
#define PR_ULONG64 "llu"

#if defined(_WIN64)

#define PR_LONG_PTR PR_LONG64
#define PR_ULONG_PTR PR_ULONG64

#define PR_PTRDIFF PR_LONG64

#elif defined(_WIN32)

#define PR_LONG_PTR PR_LONG
#define PR_ULONG_PTR PR_ULONG

#define PR_PTRDIFF PRIi32

#endif // _WIN64

#define PR_SIZE_T PR_ULONG_PTR

// Callback message codes
#define RM_INITIALIZE (WM_APP + 1)
#define RM_UNINITIALIZE (WM_APP + 2)
#define RM_CLOSE (WM_APP + 3)
#define RM_MESSAGE (WM_APP + 4)
#define RM_LOCALIZE (WM_APP + 5)
#define RM_ARGUMENTS (WM_APP + 6)
#define RM_CONFIG_UPDATE (WM_APP + 7) // update done
#define RM_CONFIG_RESET (WM_APP + 8) // reset done
#define RM_TASKBARCREATED (WM_APP + 9) // explorer restarted

#define WM_TRAYICON (WM_APP + 11)

// Project information
#define APP_LANGUAGE_DEFAULT L"English (default)"
#define APP_SKIPUAC_NAME APP_NAME_SHORT L"Task"
#define APP_SKIPUAC_NAME_OLD APP_NAME_SHORT L"SkipUAC"

#if !defined(APP_COMMENT)
#	define APP_COMMENT "What a tragedy it is for these words to fall upon deaf ears doomed to never reach their subject..."
#endif /// APP_COMMENT

#if defined(_DEBUG) || defined(APP_BETA)
#	define APP_UPDATE_PERIOD _r_calc_hours2seconds (12) // update checking period for pre-release is 12 hours
#else
#	define APP_UPDATE_PERIOD _r_calc_hours2seconds (48) // update checking period for stable release is 48 hours
#endif // _APP_BETA || _APP_BETA_RC

// Project configuration
#define PR_SIZE_TREEINDENT 12
#define PR_SIZE_ITEMHEIGHT 20
#define PR_SIZE_FOOTERHEIGHT 48
#define PR_SIZE_BUFFER_OVERFLOW (256 * 1024 * 1024)
