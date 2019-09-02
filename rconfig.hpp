// routine++
// Copyright (c) 2012-2019 Henry++

#pragma once

#include <windows.h>
#include <math.h>
#include <strsafe.h>

/*
	Configuration
*/

#define INVALID_INT (INT(-1))
#define INVALID_UINT (UINT(-1))
#define INVALID_SIZE_T (size_t(-1))

#define _R_BUFFER_LENGTH 8192
#define _R_BUFFER_INET_LENGTH (_R_BUFFER_LENGTH * 4)

//#define _APP_BETA // app has beta status
//#define _APP_BETA_RC // app has release candidate status

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

#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define _R_WARNING(desc) message(__FILE__ "(" STRINGIZE(__LINE__) ") : warning: resource id is not defined " #desc)

/*
	Definitions
*/

#ifndef _APP_AUTHOR
#	define _APP_AUTHOR L"Henry++"
#endif // _APP_AUTHOR

#ifndef _APP_GITHUB_URL
#	define _APP_GITHUB_URL L"https://github.com/henrypp"
#endif // _APP_GITHUB_URL

#ifndef _APP_WEBSITE_URL
#	define _APP_WEBSITE_URL L"https://www.henrypp.org"
#endif // _APP_WEBSITE_URL

#ifndef _APP_DONATE_URL
#	define _APP_DONATE_URL _APP_WEBSITE_URL L"/donate?from=%s"
#endif // _APP_DONATE_URL

#ifndef _APP_DONATE_URL_BTC
#	define _APP_DONATE_TXT_BTC L"blockchain.info (BTC)"
#	define _APP_DONATE_URL_BTC L"https://blockchain.info/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c"
#endif // _APP_DONATE_URL_BTC

#ifndef _APP_DONATE_URL_PAYPAL
#	define _APP_DONATE_TXT_PAYPAL L"paypal.me (USD)"
#	define _APP_DONATE_URL_PAYPAL L"https://www.paypal.me/henrypp/15"
#endif // _APP_DONATE_URL_PAYPAL

#ifndef _APP_UPDATE_URL
#	define _APP_UPDATE_URL _APP_GITHUB_URL L"/%s/releases/latest"
#endif // _APP_UPDATE_URL

#ifndef _APP_UPDATE_PERIOD
#if defined(_APP_BETA) || defined(_APP_BETA_RC)
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (2) // update checking period for beta is 2 hours
#else
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (24) // update checking period for stable is 24 hours
#endif // _APP_BETA || _APP_BETA_RC
#endif // _APP_UPDATE_PERIOD

#define _APP_TASKSCHD_NAME L"%sSkipUac"
#define _APP_LANGUAGE_DEFAULT L"English (default)"
#define _APP_ERRLOG_TITLE L"Date,Function,Code,Description,Version\r\n"

/*
	Callback message codes
*/

#define RM_INITIALIZE (WM_USER + 1)
#define RM_UNINITIALIZE (WM_USER + 2)
#define RM_CLOSE (WM_USER + 3)
#define RM_MESSAGE (WM_USER + 4)
#define RM_LOCALIZE (WM_USER + 5)
#define RM_ARGUMENTS (WM_USER + 6)
#define RM_UPDATE_DONE (WM_USER + 7) // update done
#define RM_RESET_DONE (WM_USER + 8) // reset done
