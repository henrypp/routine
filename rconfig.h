// routine.c
// project sdk library
//
// Copyright (c) 2012-2021 Henry++

#pragma once

//
// Available project configurations
//

//#define APP_BETA // app has pre-release status

//#define APP_HAVE_AUTORUN // enable autorun feature
//#define APP_HAVE_SETTINGS // enable settings dialog feature
//#define APP_HAVE_SETTINGS_TABS // enable settings dialog feature based on treeview
//#define APP_HAVE_SKIPUAC // enable skip uac feature
//#define APP_HAVE_TRAY // enable tray icon feature
//#define APP_HAVE_UPDATES // enable update checking feature

//#define APP_NO_APPDATA // use portable builds
//#define APP_NO_CONFIG // do not save configuration
//#define APP_NO_DEPRECATIONS // disable deprecated windows version support (support only win7+)
//#define APP_NO_GUEST // disable "guest" mode, running without admin rights; see APP_HAVE_SKIPUAC
//#define APP_NO_MUTEX // disable mutex

#if defined(APP_CONSOLE)
#undef APP_HAVE_AUTORUN
#undef APP_HAVE_SETTINGS
#undef APP_HAVE_SETTINGS_TABS
#undef APP_HAVE_SKIPUAC
#undef APP_HAVE_TRAY
#undef APP_HAVE_UPDATES

#define APP_NO_APPDATA
#define APP_NO_CONFIG
#endif // APP_CONSOLE

#if defined(APP_HAVE_SETTINGS_TABS)
#define APP_HAVE_SETTINGS
#endif // APP_HAVE_SETTINGS_TABS

//
// Callback message codes
//

#define RM_INITIALIZE (WM_APP + 1) // application initialization
#define RM_INITIALIZE_POST (WM_APP + 2) // application one-time initialization
#define RM_UNINITIALIZE (WM_APP + 3) // application uninitialization
#define RM_LOCALIZE (WM_APP + 4) // localization applied/changed
#define RM_TASKBARCREATED (WM_APP + 5) // explorer restarted
#define RM_CONFIG_SAVE (WM_APP + 6) // save configuration
#define RM_CONFIG_UPDATE (WM_APP + 7) // update has finished
#define RM_CONFIG_RESET (WM_APP + 8) // reset configuration has finished

#define WM_TRAYICON (WM_APP + 10)

//
// Project information
//

#define APP_LANGUAGE_DEFAULT L"English"
#define APP_SKIPUAC_NAME APP_NAME_SHORT L"Task"

#if !defined(APP_COMMENT)
#	define APP_COMMENT "What a tragedy it is for these words to fall upon deaf ears doomed to never reach their subject..."
#endif /// !APP_COMMENT

#if defined(_DEBUG) || defined(APP_BETA)
#	define APP_UPDATE_PERIOD _r_calc_days2seconds (1) // update checking period for pre-release is 1 day
#else
#	define APP_UPDATE_PERIOD _r_calc_days2seconds (7) // update checking period for stable release is 7 days
#endif // _APP_BETA || _APP_BETA_RC

//
// Debug header
//

#define PR_DEBUG_HEADER L"Level,Date,Function,Code,Description,Version,OS Version\r\n"

//
// Warning message macro
//

#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define PR_PRINT_WARNING(desc) message("[WARNING!] Compile-time warning: " #desc " (" STRINGIZE(__FILE__) ":" STRINGIZE(__LINE__) ")")

