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
//#define _APP_NO_SETTINGS // cut settings window methods
//#define _APP_NO_UPDATES // cut update checking methods
//#define _APP_HAVE_AUTORUN // enable autorun feature
//#define _APP_HAVE_SKIPUAC // enable skip uac feature
//#define _APP_HAVE_TRAY // enable tray icon feature
//#define _APP_HAVE_SIMPLE_SETTINGS // simple settints feature
//#define _APP_STARTMINIMIZED // start with no window

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
#	define _APP_WEBSITE_URL L"http://www.henrypp.org"
#endif // _APP_WEBSITE_URL

#ifndef _APP_DONATION_URL
#	define _APP_DONATION_URL _APP_WEBSITE_URL L"/donate?from=" APP_NAME_SHORT
#endif // _APP_WEBSITE_URL

#ifndef _APP_UPDATE_PERIOD
#	define _APP_UPDATE_PERIOD 172800 // update checking period (in seconds. 86400 is 1 day.)
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

/*
	Macroses
*/

#define _R_PERCENT_OF(v, t) INT(ceil((double(v) / double(t)) * 100.0))
#define _R_PERCENT_VAL(v, t) INT(double(t) * double(v) / 100.0)

#define _R_SPINLOCK(x) (InterlockedCompareExchange (&(x), 1, 0) != 0)
#define _R_SPINUNLOCK(x) (InterlockedExchange(&(x), 0))
