// routine.c
// project sdk library
//
// Copyright (c) 2012-2024 Henry++

#pragma once

//
// Warning message macro
//

#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)

#define PR_PRINT_WARNING(desc) \
	message("[WARNING!] Compile-time warning: " #desc " (" STRINGIZE(__FILE__) ":" STRINGIZE(__LINE__) ")")

#define PR_PRINT_WARNING_DEFINE(desc) \
	message("[WARNING!] Config already defined: " #desc " (" STRINGIZE(__FILE__) ":" STRINGIZE(__LINE__) ")")

//
// Available project configurations
//

//#define APP_BETA // app has pre-release status

//#define APP_HAVE_AUTORUN // enable autorun feature
//#define APP_HAVE_SETTINGS_TABS // enable settings dialog feature based on treeview
//#define APP_HAVE_TRAY // enable tray icon feature

//#define APP_NO_APPDATA // use portable builds
//#define APP_NO_CONFIG // do not save configuration
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
#define RM_UNINITIALIZE (WM_APP + 2) // application uninitialization

#define RM_INITIALIZE_POST (WM_APP + 3) // application one-time initialization
#define RM_UNINITIALIZE_POST (WM_APP + 4) // application uninitialization

#define RM_LOCALIZE (WM_APP + 5) // localization applied/changed
#define RM_ARGUMENTS (WM_APP + 6) // application arguments
#define RM_TASKBARCREATED (WM_APP + 7) // explorer restarted

#define RM_CONFIG_SAVE (WM_APP + 8) // save configuration
#define RM_CONFIG_UPDATE (WM_APP + 9) // update has finished
#define RM_CONFIG_RESET (WM_APP + 10) // reset configuration has finished

#define RM_TRAYICON (WM_APP + 11)

//
// Project configuration
//

#define APP_LANGUAGE_DEFAULT L"English"
#define APP_SKIPUAC_NAME APP_NAME_SHORT L"Task"

#if defined(APP_COMMENT)
#pragma PR_PRINT_WARNING_DEFINE(APP_COMMENT)
#else
#define APP_COMMENT \
	"What a tragedy it is for these words to fall upon deaf ears doomed to never reach their subject..."
#endif /// APP_COMMENT

// update checking period (in days)
#if defined(APP_UPDATE_PERIOD)
#pragma PR_PRINT_WARNING_DEFINE(APP_UPDATE_PERIOD)
#else
#define APP_UPDATE_PERIOD 4
#endif // APP_UPDATE_PERIOD

// error tray balloon period (in seconds)
#if defined(APP_ERROR_PERIOD)
#pragma PR_PRINT_WARNING_DEFINE(APP_ERROR_PERIOD)
#else
#define APP_ERROR_PERIOD 4
#endif // APP_UPDATE_PERIOD

//
// Messages
//

#define APP_ABOUT_FOOTER L"This program is free software; you can redistribute it and/or modify it under the terms of " \
	L"the <a href=\"https://github.com/henrypp/routine\">GNU General Public License 3</a> " \
	L"as published by the Free Software Foundation."

#define APP_ABOUT_DONATE L"<a href=\"https://www.blockchain.com/btc/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c\">Bitcoin</a> | " \
	L"<a href=\"https://www.blockchain.com/explorer/addresses/eth/0xe2C84A62eb2a4EF154b19bec0c1c106734B95960\">Ethereum</a> | " \
	L"<a href=\"https://paypal.me/henrypp\">PayPal</a> | <a href=\"https://yoomoney.ru/to/4100115776040583\">Yandex Money</a>"

#define APP_EXCEPTION_TITLE L"Exception raised :("

#define APP_SECURITY_TITLE L"Security warning!"

#define APP_FAILED_MESSAGE_TITLE L"It happens ;("
#define APP_FAILED_MESSAGE_FOOTER L"This information may provide clues as to what went wrong and how to fix it."

#define APP_FAILED_ADMIN_RIGHTS L"Administrative privileges are required!"
#define APP_FAILED_COM_INITIALIZE L"COM library initialization failed!"

#define APP_WARNING_WOW64_TITLE L"WoW64 warning!"
#define APP_WARNING_WOW64_TEXT L"This application was not designed to run under WoW64. Do not run " \
	L"32-bit executables\r\non 64-bit system because of performance loss and increased memory consumption.\r\n\r\n" \
	L"Note: Add \"-nowow64\" argument to avoid this warning and take responsibility for the consequences."

#define APP_WARNING_UAC_TEXT L"It is not recommended to enable this option\r\n" \
	L"when running from outside a secure location (e.g. Program Files).\r\n\r\nAre you sure you want to continue?"

#define APP_WARNING_UPDATE_TEXT \
	L"This operating system are obsolete and does not meet security\r\nrequirements to establish secure network connection."

#define APP_WARNING_LOG_TEXT L"Something went wrong. Open debug log file in profile directory."

#define APP_QUESTION_RESTART L"Restart is required to apply configuration, restart now?"
#define APP_QUESTION_RESET L"Are you really sure you want to reset all application settings?"

//
// Debug header
//

#define PR_DEBUG_HEADER L"Level,Date,Function,Code,Description,App version,OS version\r\n"
#define PR_DEBUG_BODY L"\"%s\",\"%s\",\"%s\",\"0x%08" TEXT (PRIX32) L"\",\"%s\"" \
	L",\"%s\",\"%" TEXT (PR_ULONG) L".%" TEXT (PR_ULONG) L" build %" TEXT (PRIu32) L"\"\r\n"
