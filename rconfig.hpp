// routine++
// Copyright (c) 2012-2018 Henry++

#pragma once

#include <windows.h>
#include <math.h>
#include <strsafe.h>

/*
	Configuration
*/

#define LAST_VALUE (size_t(-1))

#define _R_BUFFER_LENGTH 8192

//#define _APP_NO_ABOUT // disable about dialog feature
//#define _APP_NO_GUEST // disable "guest" mode, running without admin rights; see _APP_HAVE_SKIPUAC
//#define _APP_NO_WINXP // disable winxp support

//#define _APP_HAVE_SETTINGS // enable settings dialog feature
//#define _APP_HAVE_UPDATES // enable update checking feature

//#define _APP_HAVE_AUTORUN // enable autorun feature
//#define _APP_HAVE_NTDLL // sdk have ntdll definitions and functions
//#define _APP_HAVE_SKIPUAC // enable skip uac feature
//#define _APP_HAVE_TRAY // enable tray icon feature
//#define _APP_HAVE_SRWLOCK // srw locks (vista+)
//#define _APP_STARTMINIMIZED // start with no window


//#define _APP_BETA // app has beta status
//#define _APP_BETA_RC // app has release candidate status

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
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (12) // update checking period for beta is 12 hours
#else
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_HOUR (48) // update checking period for stable is 48 hours
#endif // _APP_BETA || _APP_BETA_RC
#endif // _APP_UPDATE_PERIOD

#define _APP_TASKSCHD_NAME L"%sSkipUac"
#define _APP_LANGUAGE_DEFAULT L"English (default)"

/*
	Callback message codes
*/

#define RM_INITIALIZE (WM_USER + 1)
#define RM_UNINITIALIZE (WM_USER + 2)
#define RM_CLOSE (WM_USER + 3)
#define RM_MESSAGE (WM_USER + 4)
#define RM_LOCALIZE (WM_USER + 5)
#define RM_ARGUMENTS (WM_USER + 6)
