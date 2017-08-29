// routine++
// Copyright (c) 2012-2017 Henry++

#pragma once

#include <windows.h>
#include <math.h>
#include <strsafe.h>

/*
	Configuration
*/

#define LAST_VALUE (size_t(-1))

#define _R_BUFFER_LENGTH 8192

//#define _APP_NO_ABOUT // cut about window methods
//#define _APP_NO_GUEST // cut "guest" mode, running without admin rights; see _APP_HAVE_SKIPUAC
//#define _APP_NO_SETTINGS // cut settings window methods
//#define _APP_NO_UPDATES // cut update checking methods
//#define _APP_NO_WINXP // cut windows xp support
//#define _APP_HAVE_AUTORUN // enable autorun feature
//#define _APP_HAVE_SKIPUAC // enable skip uac feature
//#define _APP_HAVE_TRAY // enable tray icon feature
//#define _APP_HAVE_SIMPLE_SETTINGS // simple settints feature
//#define _APP_HAVE_SIZING // window is resizable
//#define _APP_STARTMINIMIZED // start with no window
//#define _APP_BETA // app has beta status

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

#ifndef _APP_DONATE_BTC_URL
#	define _APP_DONATE_BTC L"blockchain.info (BTC)"
#	define _APP_DONATE_BTC_URL L"https://blockchain.info/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c"
#endif // _APP_DONATE_BTC_URL

#ifndef _APP_DONATE_PAYPAL_URL
#	define _APP_DONATE_PAYPAL L"paypal.me (USD)"
#	define _APP_DONATE_PAYPAL_URL L"https://www.paypal.me/henrypp/15"
#endif // _APP_DONATE_PAYPAL_URL

#ifndef _APP_UPDATE_URL
#	define _APP_UPDATE_URL _APP_GITHUB_URL L"/%s/releases/latest"
#endif // _APP_UPDATE_URL

#ifndef _APP_UPDATE_PERIOD
#	define _APP_UPDATE_PERIOD _R_SECONDSCLOCK_DAY (2) // update checking period (sec.)
#endif // _APP_UPDATE_PERIOD

#define _APP_I18N_DIRECTORY L"i18n"
#define _APP_I18N_SECTION L"i18n"

#define _APP_TASKSCHD_NAME L"%sSkipUac"
#define _APP_LANGUAGE_DEFAULT L"English (default)"

/*
	Callback message codes
*/

#define _RM_INITIALIZE 0x01
#define _RM_UNINITIALIZE 0x02
#define _RM_SAVE 0x04
#define _RM_MESSAGE 0x08
#define _RM_LOCALIZE 0x10
#define _RM_ARGUMENTS 0x20
