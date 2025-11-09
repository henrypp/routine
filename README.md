## routine.c
routine library for my projects.

### System requirements:
- Windows 7, 8, 8.1, 10, 11 32-bit/64-bit/ARM64
- An SSE2-capable CPU
- <s>KB2533623</s> KB3063858 update for Windows 7 was required [[x64](https://www.microsoft.com/en-us/download/details.aspx?id=47442) / [x32](https://www.microsoft.com/en-us/download/details.aspx?id=47409)]

### Donate:
- [Bitcoin](https://www.blockchain.com/btc/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c) (BTC)
- [Ethereum](https://www.blockchain.com/explorer/addresses/eth/0xe2C84A62eb2a4EF154b19bec0c1c106734B95960) (ETC)
- [Paypal](https://paypal.me/henrypp) (USD)
- [Yandex Money](https://yoomoney.ru/to/4100115776040583) (RUB)

### GPG Signature:
Binaries have this GPG signature.

- Public key: [pubkey.asc](https://raw.githubusercontent.com/henrypp/builder/master/pubkey.asc) ([pgpkeys.eu](https://pgpkeys.eu/pks/lookup?op=index&fingerprint=on&search=0x5635B5FD))
- Key ID: 0x5635B5FD
- Fingerprint: D985 2361 1524 AB29 BE73 30AC 2881 20A7 5635 B5FD

### Settings:
List of definitions which can change compiled binary with `routine` library:

<details>
<summary>Project macroses:</summary>

---
~~~c
//
// Available project configurations
//

// app has pre-release status (same as when _DEBUG bulded)
#define APP_BETA

// enable autorun feature
#define APP_HAVE_AUTORUN

// enable settings dialog feature based on tabs
#define APP_HAVE_SETTINGS_TABS

// implement admin task feature based on task schedulerwhich can skip UAC
#define APP_HAVE_SKIPUAC

// enable tray icon feature
#define APP_HAVE_TRAY

// enable update checkng feature
#define APP_HAVE_UPDATES

// compile portable builds
#define APP_NO_APPDATA

// do not save configuration
#define APP_NO_CONFIG

// disable "guest" mode, running without admin rights; see APP_HAVE_SKIPUAC
#define APP_NO_GUEST

// disable mutex
#define APP_NO_MUTEX
~~~
</details>

List of `.ini` options for compiled binary with `routine` library:

<details>
<summary>APP_NAME_SHORT.ini:</summary>

---
~~~ini
#
#2.0+
#

# Enable update checking (BOOLEAN)
# CheckUpdates=TRUE // removed in 2.3

# Last update checking timestamp (LONG64)
CheckUpdatesLast=0

# Last opened settings dialog id (LONG)
SettingsLastPage=0

# Main window always on top (BOOLEAN)
AlwaysOnTop=FALSE

# Minimized main window on startup (BOOLEAN)
IsStartMinimized=FALSE

# Application locale name (STRING)
Language=NULL

# WinHTTP connections user-agent (STRING)
UserAgent=NULL

#
#2.1+
#

# Minimum error logging level (LONG)
ErrorLevel=LOG_LEVEL_INFO

# Last error notification timestamp (LONG64)
#ErrorNotificationsTimestamp=0 // removed in 2.3

# Error notification period in seconds (LONG64)
#ErrorNotificationsPeriod=4 // removed in 2.3

# Enable error notifications (BOOLEAN)
IsErrorNotificationsEnabled=TRUE

# Enable notifications sound (BOOLEAN)
IsNotificationsSound=TRUE

#
#2.3+
#

# Update checking period in hours, 0 for disable (LONG)
CheckUpdatesPeriod=APP_UPDATE_PERIOD

#
#2.3.1+
#

# Minimized main window into system tray (BOOLEAN)
# Note: only when APP_HAVE_TRAY defined.
IsMinimizeToTray=TRUE

# Close main window into system tray (BOOLEAN)
# Note: only when APP_HAVE_TRAY defined.
IsCloseToTray=TRUE

#
#2.4+
#

# Auto install non-executable updates if found (BOOLEAN)
IsAutoinstallUpdates=FALSE (changed to TRUE in 2.7.12+)

#
#2.7+
#

# Enable dark theme support (BOOLEAN)
IsDarkThemeEnabled=<reads "HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize" "AppsUseLightTheme" value>

#
#2.7.2+
#

# Show window border in Windows 11 and above (BOOLEAN)
IsWindowBorderEnabled=TRUE

#
#2.7.10+
#

# Show window round corners in Windows 11 and above (BOOLEAN)
IsWindowCornerRound=FALSE
~~~
</details>

### Command line:
List of arguments for compiled binary with `routine` library:

~~~
-minimized - start application with minimized/hidden window.
-portable - set force portable mode.
-readonly - set force readonly mode (no settings will be written).
-nowow64 - allow to run 32-bit executable under 64-bit environment (32-bit only)
~~~

---
- Website: [github.com/henrypp](https://github.com/henrypp)
- Support: sforce5@mail.ru
---
(c) 2012-2026 Henry++
