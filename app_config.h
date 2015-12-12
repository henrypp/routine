// application helper library
// Copyright (c) 2013-2015 Henry++

#pragma once

/*
	Configuration
*/

//#define _APP_NO_ABOUT // cut about window management at compile time
//#define _APP_NO_SETTINGS // cut settings window management at compile time
//#define _APP_NO_UPDATES // cut update checking at compile time
//#define _APP_NO_UAC // enable skip uac feature

/*
	Application definitions
*/

#define _APP_AUTHOR L"Henry++"

#define _APP_GITHUB_URL L"https://github.com/henrypp"
#define _APP_WEBSITE_URL L"http://www.henrypp.org"

#define _APP_I18N_DIRECTORY L"i18n"
#define _APP_I18N_SECTION L"i18n"

#define _APP_ABOUT_CLASS L"AboutWND"
#define _APP_TASKSCHD_NAME L"%sSkipUac"
#define _APP_UPDATE_PERIOD 2 // update checking period (in days)

/*
	Callback message codes
*/

#define APP_CALLBACK_INITIALIZE 0x1
#define APP_CALLBACK_UNINITIALIZE 0x2

#define APP_CALLBACK_SETTINGS_SAVE 0x4
#define APP_CALLBACK_SETTINGS_INIT 0x8

#define APP_CALLBACK_MESSAGE 0x16
