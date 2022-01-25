## routine.c
routine library for my projects.

### Command line:
List of arguments for compiled binary with `routine` library:

~~~
-ini:"path" - set custom configuration path.
-minimized - start application with minimized/hidden window.
-portable - set force portable mode.
-readonly - set force readonly mode (no settings will be written).
-nowow64 - allow to run 32-bit executable under 64-bit environment (32-bit only)
~~~

### Settings:
List of `ini` options for compiled binary with `routine` library:

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
~~~

Website: [www.henrypp.org](https://www.henrypp.org)<br />
Support: support@henrypp.org<br />
<br />
(c) 2012-2022 Henry++
