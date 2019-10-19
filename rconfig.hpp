// routine++
// Copyright (c) 2012-2019 Henry++

#pragma once

#include <windows.h>

// Warning message macro
#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define _R_WARNING(desc) message(__FILE__ "(" STRINGIZE(__LINE__) ") : warning: resource id is not defined " #desc)

//#define _APP_BETA // app has pre-release status

//#define _APP_NO_ABOUT // disable about dialog feature
//#define _APP_NO_GUEST // disable "guest" mode, running without admin rights; see _APP_HAVE_SKIPUAC
//#define _APP_NO_WINXP // disable winxp support
//#define _APP_NO_MUTEX // disable mutex
//#define _APP_NO_DARKTHEME // disable dark theme feature (win10rs5+)

//#define _APP_HAVE_SETTINGS // enable settings dialog feature
//#define _APP_HAVE_UPDATES // enable update checking feature

//#define _APP_HAVE_AUTORUN // enable autorun feature
//#define _APP_HAVE_SKIPUAC // enable skip uac feature
//#define _APP_HAVE_TRAY // enable tray icon feature
//#define _APP_HAVE_SRWLOCK // srw locks (vista+)
//#define _APP_HAVE_MINSIZE // change minimum size of main window
//#define _APP_HAVE_DARKTHEME_SUBCLASS // enable dark theme own draw (win10rs5+)

//#define _APP_STARTMINIMIZED // start with no window

// set defaults
#ifndef _APP_CLASSICUI
#define _APP_CLASSICUI false
#endif // _APP_CLASSICUI

#ifndef _APP_ALWAYSONTOP
#define _APP_ALWAYSONTOP false
#endif // _APP_ALWAYSONTOP

// printf specifiers
#if defined(_WIN64)
#if defined(_WINDOWS_)
#define PR_SIZE_T "Iu"
#define PR_PTRDIFF "Id"
#else
#define PR_SIZE_T "zu"
#define PR_PTRDIFF "zd"
#endif
#elif defined(_WIN32)
#define PR_SIZE_T "u"
#define PR_PTRDIFF "d"
#endif

// Callback message codes
#define RM_INITIALIZE (WM_USER + 1)
#define RM_UNINITIALIZE (WM_USER + 2)
#define RM_CLOSE (WM_USER + 3)
#define RM_MESSAGE (WM_USER + 4)
#define RM_LOCALIZE (WM_USER + 5)
#define RM_ARGUMENTS (WM_USER + 6)
#define RM_UPDATE_DONE (WM_USER + 7) // update done
#define RM_RESET_DONE (WM_USER + 8) // reset done
#define RM_DPICHANGED (WM_USER + 9) // dpi changed message (win81+)

// Invalid types definitions
#define INVALID_INT (INT(-1))
#define INVALID_UINT (UINT(-1))
#define INVALID_SIZE_T (size_t(-1))

// Project configuration
#define _R_BUFFER_LENGTH 8192
#define _R_BUFFER_NET_LENGTH (_R_BUFFER_LENGTH * 4)

#define _R_SIZE_ICON16 16
#define _R_SIZE_ICON32 32
#define _R_SIZE_ITEMHEIGHT 20
#define _R_SIZE_FOOTERHEIGHT 48

// Project information
#define _APP_AUTHOR L"Henry++"
#define _APP_GITHUB_URL L"https://github.com/henrypp"
#define _APP_WEBSITE_URL L"https://www.henrypp.org"
#define _APP_DONATE_URL _APP_WEBSITE_URL L"/donate?from=%s"

#ifdef _APP_BETA
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (12) // update checking period for pre-release is 12 hours
#else
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (48) // update checking period for stable release is 48 hours
#endif // _APP_BETA || _APP_BETA_RC

#define _APP_LANGUAGE_DEFAULT L"English (default)"
#define _APP_TASKSCHD_NAME L"%sSkipUac"
