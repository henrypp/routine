// routine++
// Copyright (c) 2012-2016 Henry++

#pragma once

#define STRSAFE_NO_CB_FUNCTIONS

#include <windows.h>
#include <math.h>
#include <strsafe.h>

/*
	Configuration
*/

#define _R_BUFFER_LENGTH 8192

//#define _APP_NO_ABOUT // cut about window methods
//#define _APP_NO_SETTINGS // cut settings window methods
//#define _APP_NO_UPDATES // cut update checking methods
//#define _APP_NO_UAC // enable skip uac methods

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

#ifndef _APP_UPDATE_PERIOD
#	define _APP_UPDATE_PERIOD 86400 // update checking period (in seconds. 86400 is 1 day.)
#endif // _APP_UPDATE_PERIOD

#define _APP_I18N_DIRECTORY L"i18n"
#define _APP_I18N_SECTION L"i18n"

#define _APP_ABOUT_CLASS L"AboutWND"
#define _APP_TASKSCHD_NAME L"%sSkipUac"

/*
	Callback message codes
*/

#define _RM_INITIALIZE 0x1
#define _RM_UNINITIALIZE 0x2

#define _RM_SETTINGS_SAVE 0x4
#define _RM_SETTINGS_INIT 0x8

#define _RM_MESSAGE 0x16

/*
	Macroses
*/

#define _R_PERCENT_OF(v, t) INT(ceil((double(v) / double(t)) * 100.0))
#define _R_PERCENT_VAL(v, t) INT(double(t) * double(v) / 100.0)

#define _R_SPINLOCK(x) while (InterlockedCompareExchange (&(x), 1, 1) == 1)
#define _R_SPINUNLOCK(x) (x) = 0
