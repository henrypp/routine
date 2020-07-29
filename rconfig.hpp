// routine++
// Copyright (c) 2012-2020 Henry++

#pragma once

// Warning message macro

#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define R_PRINT_WARNING(desc) message("[WARNING!] Compile-time warning: " #desc  " (" STRINGIZE(__FILE__) ":" STRINGIZE(__LINE__) ")")

//#define _APP_BETA // app has pre-release status

//#define _APP_NO_DEPRECATIONS // disable deprecated windows version support (support only win7+)
//#define _APP_NO_GUEST // disable "guest" mode, running without admin rights; see _APP_HAVE_SKIPUAC
//#define _APP_NO_MUTEX // disable mutex

//#define _APP_HAVE_SETTINGS // enable settings dialog feature
//#define _APP_HAVE_UPDATES // enable update checking feature

//#define _APP_HAVE_AUTORUN // enable autorun feature
//#define _APP_HAVE_DARKTHEME // enable dark theme feature (win10rs5+)
//#define _APP_HAVE_MINSIZE // change minimum size of main window
//#define _APP_HAVE_NATIVETHREAD // create threads via "RtlCreateUserThread" instead of crt library "_beginthreadex"
//#define _APP_HAVE_SKIPUAC // enable skip uac feature
//#define _APP_HAVE_TRAY // enable tray icon feature

//#define _APP_STARTMINIMIZED // start with no window

// set defaults
#ifndef _APP_CLASSICUI
#define _APP_CLASSICUI FALSE
#endif // _APP_CLASSICUI

#ifndef _APP_ALWAYSONTOP
#define _APP_ALWAYSONTOP FALSE
#endif // _APP_ALWAYSONTOP

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

#endif

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
#define RM_DPICHANGED (WM_APP + 10) // dpi changed message (win81+)

#define WM_TRAYICON (WM_APP + 11)

// Invalid types definitions
#define INVALID_INT ((INT)-1)
#define INVALID_UINT ((UINT)-1)
#define INVALID_SIZE_T ((SIZE_T)-1)

// Project configuration
#define _R_SIZE_TREEINDENT 12
#define _R_SIZE_ITEMHEIGHT 20
#define _R_SIZE_FOOTERHEIGHT 48

// Project information
#define _APP_AUTHOR L"Henry++"
#define _APP_GITHUB_URL L"https://github.com/henrypp"
#define _APP_WEBSITE_URL L"https://www.henrypp.org"
#define _APP_DONATE_NEWURL _APP_WEBSITE_URL L"/donate?from=" APP_NAME_SHORT
#define _APP_LANGUAGE_DEFAULT L"English (default)"
#define _APP_SKIPUAC_NAME APP_NAME_SHORT L"Task"
#define _APP_SKIPUAC_NAME_OLD APP_NAME_SHORT L"SkipUAC"

#if defined(_DEBUG) || defined(_APP_BETA)
#	define _APP_UPDATE_PERIOD _r_calc_days2seconds (time_t, 12) // update checking period for pre-release is 12 hours
#else
#	define _APP_UPDATE_PERIOD _r_calc_days2seconds (time_t, 48) // update checking period for stable release is 48 hours
#endif // _APP_BETA || _APP_BETA_RC
