## routine.c
routine library for my projects.

### System requirements:
- Windows 7 SP1 and above operating system.
- [Visual C++ 2022 Redistributable package](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

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

### Command line:
List of arguments for compiled binary with `routine` library:

~~~
-minimized - start application with minimized/hidden window.
-portable - set force portable mode.
-readonly - set force readonly mode (no settings will be written).
-nowow64 - allow to run 32-bit executable under 64-bit environment (32-bit only)
~~~

### Settings:
List of `ini` options for compiled binary with `routine` library:

<details>
<summary>APP_NAME_SHORT.ini:</summary>

---
~~~ini
#
#2.0+
#

# Enable update checking (boolean)
# CheckUpdates=TRUE // removed in 2.3

# Last update checking timestamp (long64)
CheckUpdatesLast=0

# Last opened settings dialog id (long)
SettingsLastPage=0

# Main window always on top (boolean)
AlwaysOnTop=FALSE

# Minimized main window on startup (boolean)
IsStartMinimized=FALSE

# Application locale name (string)
Language=NULL

# WinHTTP connections user-agent (string)
UserAgent=NULL

#
#2.1+
#

# Minimum error logging level (long)
ErrorLevel=LOG_LEVEL_INFO

# Last error notification timestamp (long64)
#ErrorNotificationsTimestamp=0 // removed in 2.3

# Error notification period in seconds (long64)
#ErrorNotificationsPeriod=4 // removed in 2.3

# Enable error notifications
IsErrorNotificationsEnabled=TRUE

# Enable notifications sound
IsNotificationsSound=TRUE

#
#2.3+
#

# Update checking period in days, 0 for disable (long)
CheckUpdatesPeriod=APP_UPDATE_PERIOD

#
#2.3.1+
#

# Minimized main window into system tray (boolean)
# Note: only when APP_HAVE_TRAY defined.
IsMinimizeToTray=TRUE

# Close main window into system tray (boolean)
# Note: only when APP_HAVE_TRAY defined.
IsCloseToTray=TRUE

#
#2.4+
#

# Auto install non-executable updates if found (boolean)
IsAutoinstallUpdates=FALSE

#
#2.7.2+
#

# Show window border in Windows 11 and above (boolean)
IsWindowBorderEnabled=TRUE
~~~
---
</details>

- Website: [github.com/henrypp](https://github.com/henrypp)
- Support: sforce5@mail.ru

(c) 2012-2024 Henry++
