## routine.c v3.0

> «Искусственный интеллект возникает тогда, когда не хватает естественного.»

`lastmod: 11/09/2026`

<p align="center"><img src="/images/routine.png" /></p>

Это "Project SDK library" для сборки моих проектов, который <ins>больше никогда не будет обновлятся</ins>, исходный код также открыт, но синхронизации больше не будет, достаточно сложно будет дописать/переписать свыше 100к строк кода, но кто-то даже [безуспешно пытается](#попытки), посочувствуем вместе?

<p align="center"><img src="/images/terminator.png" /></p>

Поблагодарите за это **исскуственного идиота** (ИИ), благодаря которому стало **невозможно** [устроится на работу](#работа) - <ins>люди с 20-ти летним стажем, которые хороший код умеют писать, больше никому не нужны</ins> (и это соответствующий ответ такому тупоголовому поведению, принимайте - расписывайтесь), а причина - [пидорас](https://ru.wikipedia.org/wiki/Альтман,_Сэм) (это факт, а не оскорбление) ведь те кто посмотрел в детстве [Терминатора 2](https://www.imdb.com/title/tt0103064/) (только первые 2 части, остальные абсолютно не заслуживают ничьего внимания, ибо сценарий там писал лично [Ваганыч](https://dic.academic.ru/pictures/wiki/files/112/petrosyan_eugene_vaganovich.jpg)), ведь на данный момент этот фильм <ins>является документальным</ins>! Считайте сами - до образования <s>небесной сети</s> AGI осталось 2-3 года в 2029 всё выйдет из под контроля и... ожидайте удар ядерными ракетами по РФ и/или Китаю, звучит как бред? [Ха-ха-ха](https://otr-online.ru/articles/-planeta-evropy-perestanet-sushchestvovat-franciya-i-polsha-provedut-yadernye-ucheniya-protiv-rf-o-chem-ih-predupredili-315306.html), [смищно](https://russiancouncil.ru/analytics-and-comments/analytics/kak-nato-gotovitsya-k-voyne/).

### Интервью века:
[Затроллировали лудщего интервувера](https://www.youtube.com/watch?v=8GGWedu77SA) (это его так народ называет) просто посмеятся нужно, вокруг столько тупоголовых кретинов, а это пример как к ним <ins>нужно</ins> относится.

### Факты:
- [CyberDyne](http://www.cyberdyne.jp/english/) УЖЕ существует, киборгов назвали HAL правда не 9000, а <ins>пока</ins> лишь 5.
- Британцы создали настоящую [небесную сеть](http://en.wikipedia.org/wiki/Skynet_(satellite)) (не влияет на суть, но кто знает как это сложится в будущем?)
- АНБ тоже создали собственный SkyNet - [MonsterMind](http://www.wired.com/2014/08/nsa-monstermind-cyberwarfare/) (в утечке Сноудена всё это описывается)
- МНОГОЧИСЛЕННЫЕ повсеместные встраивания <ins>Искусственного Идиота</ins> во **ВСЕ** участки министерства <s>обороны</s> войны (и не только, считай во всё теперь встраивается этот мусор, <ins>из-за</ins> которого затем неожиданно всё пойдёт не по плану).
- сам [разрабочик (Anthropic/ClosedAI)](https://x.com/hilbertspaess/status/2097476196791709843) предупреждает что этот мусор вполне способен, через 2-3 года, уничтожить человечество.

### Работа:
Вообще не считаю HR за профессию (это рил мусор, посмотрите как продуктивность вырастает если удалить эту раковую опухоль из коллектива, мне лень пруфы приводить, но они есть), примеры:
- [про собеседование с noname](https://t.me/henrypp_dev/125)
- [про собеседование с @yandex](https://t.me/henrypp_dev/127)

### Попытки:
[вот пример PR](https://github.com/henrypp/simplewall/blob/b72c13a0ecc1d42de8c163958393587e261f9886/tools/public-sdk/routine-compat.h)
где автор абсолютно не вывозит:
- новую функцию `_r_wnd_topzoder` объявил как `_r_wnd_top`, но `_r_wnd_topzoder` имеет совершенно иной смысл, и помещает (или убирает) окно в верхнюю часть порядка Z, а не делает его поверх всех остальных.
- `sw_compat_filehash` который сразу помечает возращаемый результат как `NULL` это как бы фикс для `_r_crypt_getfilehash`, но он бессмысленный, ибо у меня асболютно все функции возращают `NULL` при неудаче для `_Out_` параметров.
- вообще к @Tarkiin никаких претензий не имею, просто бессмысленный код генерировать не вижу смысла.

### Пожертвование:
- [Bitcoin](https://www.blockchain.com/btc/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c) (BTC)
- [Ethereum](https://www.blockchain.com/explorer/addresses/eth/0xe2C84A62eb2a4EF154b19bec0c1c106734B95960) (ETH)
- [Yandex Money](https://yoomoney.ru/to/4100115776040583) (RUB)
- [Paypal](https://paypal.me/henrypp) (USD)

### GPG Signature:
Все исполняемые файлы имеют GPG сигнатуру.

- Public key: [pubkey.asc](https://raw.githubusercontent.com/henrypp/builder/master/pubkey.asc) ([pgpkeys.eu](https://pgpkeys.eu/pks/lookup?op=index&fingerprint=on&search=0x5635B5FD))
- Key ID: `0x5635B5FD`
- Fingerprint: `D985 2361 1524 AB29 BE73 30AC 2881 20A7 5635 B5FD`

### Настройки:
Список определений (`#define`) которые изменяют поведение скомпилированного исполняемого файла с библиотекой `routine.c`:

<details>
<summary>Определения (define) проекта:</summary>

---
~~~c
//
// Доступные конфигурации проекта
//

// отключить режим "гостя", никакого запуска не под административными правами; смотри APP_HAVE_SKIPUAC
#define APP_NO_GUEST

// реализовать задание в "планировщике задач" которое и пропускает UAC
#define APP_HAVE_SKIPUAC

// у приложения имеется статус pre-release (это то же самое когда определён _DEBUG)
#define APP_BETA

// включить диалог настроек основанный на вкладках
#define APP_HAVE_SETTINGS_TABS

// включить возможность проверки обновлений
#define APP_HAVE_UPDATES

// включить возможность автозапуска
#define APP_HAVE_AUTORUN

// включить трей иконку
#define APP_HAVE_TRAY

// скомпилировать портативную сборку
#define APP_NO_APPDATA

// отключить мьютекс (mutant)
#define APP_NO_MUTEX

// использовать безопасные функции для работы со строками (3.0+)
#define APP_SAFE_STRING
~~~
</details>

---

Список настроек `.ini` для скомпилированного бинаря с библиотекой `routine.c`:

<details>
<summary>APP_NAME_SHORT.ini:</summary>

---
~~~ini
#
#2.0+
#

# Включить проверку обновлений (BOOLEAN)
# CheckUpdates=TRUE // удалён в 2.3

# Время последней проверки обновления в unixtime (LONG64)
CheckUpdatesLast=0

# Последний открытый диалог в настройках (LONG)
SettingsLastPage=0

# Главное окно поверх всех окон (BOOLEAN)
AlwaysOnTop=FALSE

# Свернуть главное окно при запуске (BOOLEAN)
IsStartMinimized=FALSE

# Имя локализации приложения (STRING)
Language=NULL

# WinHTTP user-agent (STRING)
UserAgent=NULL

#
#2.1+
#

# Минимальный уровень логирования ошмбок (LONG)
ErrorLevel=2 (LOG_LEVEL_INFO)

# Время последнего сообщения об ошибке (LONG64)
#ErrorNotificationsTimestamp=0 // удалён в 2.3

# Период сообщений об ошибках в трее в секундах (LONG64)
#ErrorNotificationsPeriod=4 // удалён в 2.3

# Включить уведомления об ошибках (BOOLEAN)
IsErrorNotificationsEnabled=TRUE

# Включить звук уведомлений (BOOLEAN)
IsNotificationsSound=TRUE

#
#2.3+
#

# Период проверки обновлений в часах, 0 для отключения (LONG)
CheckUpdatesPeriod=6 (APP_UPDATE_PERIOD)

#
#2.3.1+
#

# Свернуть главное окно в системный трей (BOOLEAN)
# NOTE: ттолько если APP_HAVE_TRAY определена.
IsMinimizeToTray=TRUE

# Закрывать главное окно в системный трей (BOOLEAN)
# NOTE: ттолько если APP_HAVE_TRAY определена.
IsCloseToTray=TRUE

#
#2.4+
#

# Автоустановка неисполняемых обновлений (BOOLEAN)
IsAutoinstallUpdates=FALSE (изменено на TRUE в 3.0+)

#
#2.7+
#

# Включить тёмный режим (BOOLEAN)
IsDarkThemeEnabled=<читает значение "HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize" "AppsUseLightTheme">

#
#2.7.2+
#

# Показывать границу у окон Windows 11 и выше (BOOLEAN)
IsWindowBorderEnabled=TRUE

#
#2.7.10+
#

# Закруглять границы окон Windows 11 и выше (BOOLEAN)
IsWindowCornerRound=FALSE

#
#3.0+
#

# Маскировать заголовок главного окна для скрытия его от кого угодно (BOOLEAN)
IsScrambleTitle=FALSE

# Печатать сообщение для отладчика когда приходит сообщение об ошибке (BOOLEAN)
IsErrorDebugEnabled=FALSE
~~~
</details>

### Коммандная строка:
Список аргументов для скомпилированного исполняемого файла с использованием библиотеки `routine.c`:

~~~
-minimized - запуск приложения со свёрнутым/скрытым окном.
-portable - установить портативный режим.
-readonly - установить режим только чтения (настройки не будут сохранятся).
~~~

---
- Сайт: [github.com/henrypp](https://github.com/henrypp)
- Поддержка: sforce5@mail.ru
---
(c) 2012-2026 Henry++
