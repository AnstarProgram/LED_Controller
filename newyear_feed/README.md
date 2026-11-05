# 🎄 Новогодняя школа-лента на ESP32

Проект: цветной дисплей + гирлянда NeoPixel показывают новогодний «телераннер»
(клипы/поздравления), которым вы управляете с ПК по **Wi-Fi** и **Bluetooth**.

## Что в папке

```
firmware_esp32/   — прошивка для Arduino IDE (файл newyear_feed.ino + модули)
pc_sender/        — программа для ПК (sender.py): GUI, конвертер видео, Wi-Fi/BT пульт
data/             — пример index.json
```

## Прошивка (`firmware_esp32`)

| Файл | Зачем |
|---|---|
| `newyear_feed.ino` | главный скетч: Wi-Fi, HTTP-API, Bluetooth, запуск всего |
| `config.h` | **все настройки**: Wi-Fi логин, тип дисплея, пины, тайминги |
| `display.h` | вывод кадров RGB565 на TFT, русские шрифты, прогресс-бары |
| `storage.h` | LittleFS/SD + парсер `index.json` (плейлист клипов) |
| `downloader.h` | фоновая докачка следующего клипа по Wi-Fi + проверка размера/хэша |
| `player.h` | показ клипов с **предзагрузкой** (см. ниже) |
| `bt.h` | Bluetooth LE UART: команды PLAY / PAUSE / NEXT / PREV / RELOAD / STATUS / BRIGHT:n / MODE:SNOW |
| `leds.h` | гирлянда NeoPixel (снежинки/радуга/салют) + задел под музыку |
| `audio.h` | **задел под колонки** (I2S). Раскомментируйте `AUDIO_ENABLE` в config.h когда купите усилитель (MAX98357A/PAM8403+динамики) |

### Как работает «умная буферизация» (то, о чём вы просили)
Никакого «скачать всё до 24:00» нет — не нужно!
1. Идёт показ клипа №N, а лента **молча качает по Wi-Fi клип №N+1**.
2. За **5 секунд** до конца N проверяется готов ли N+1 (размер файла + контрольная сумма).
3. Готов → плавный переход к N+1, стартует докачка N+2.
4. Не готов → на экран пишется **«NEED NEXT FILE»** и закачка запускается заново,
   при этом эфир не гаснет — текущий клип зациклен.
5. На докачку одного файла есть **5 минут** (`RELOAD_TIMEOUT_MS`).

### Установка прошивки
1. Arduino IDE → Settings → «Дополнительные URL»: добавить
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`,
   затем установить плату **esp32 by Espressif Systems** (выберите вашу плату;
   если есть PSRAM — включите *Board Options → PSRAM: OPI/QSPI* — тогда влезут большие кадры).
2. Библиотеки (Sketch → Include Library → Manage Libraries):
   - **TFT_eSPI** (Bodmer)
   - **Adafruit NeoPixel**
   - **ESP32 BLE Arduino** (обычно уже в комплекте esp32 core)
3. Настройка дисплея: откройте файл `User_Setup.h` библиотеки TFT_eSPI
   (Documents/Arduino/libraries/TFT_eSPI/User_Setup.h) и вставьте один из блоков:

   **ESP32-модуль с 2.4" ILI9341 (самый частый):**
   ```cpp
   #define ILI9341_DRIVER
   #define TFT_MOSI 23
   #define TFT_MISO 27
   #define TFT_SCLK 25
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST  -1
   #define TFT_BL   14
   #define BACKLIGHT_PWM_CHANNEL 1
   #define SPI_FREQUENCY  40000000
   ```
   Если у вас плата Lilygo T-Display (ST7789) или другая — см. комментарии в `config.h`.
4. В `config.h` впишите `WIFI_SSID` / `WIFI_PASS` вашей школьной сети.
5. Загрузите скетч. В мониторе порта появится IP ленты, например `192.168.1.42`
   — он понадобится программе на ПК.

## Программа для ПК (`pc_sender/sender.py`)

Зависимости: Python 3.9+, `pip install pillow` (для видео нужен ещё `ffmpeg` в PATH),
`pip install bleak` — только если хотите Bluetooth.

```bash
python sender.py gui                 # графическое приложение (рекомендую)
```

В окне GUI:
1. Впишите **IP ESP32** (из монитора порта).
2. «Добавить видео/фото» — выберите mp4/gif/png: они автоматически нарежутся
   в формат ленты (RGB565, низкий FPS — экономия памяти).
3. Нажмите **«Подключить ПК→ленту»** — запустится файловый сервер и ESP32 узнает адрес ПК.
4. Кнопки Play / Pause / Next / Reload / Статус — управление по Wi-Fi.
5. Кнопки BT — то же самое по Bluetooth (на телефоне аналог — приложение
   «Serial Bluetooth Terminal», там же можно слать текстовые команды).
6. «Русский шрифт» — генерирует `fonts/Cyrillic_Arial14.vlw`; залейте его в
   `/data/fonts` на ESP32 (через LittleFS Upload в Arduino IDE или кнопкой отправки).

CLI без GUI:
```bash
python sender.py convert clip.mp4 --fps 5 --width 160
python sender.py setup --ip 192.168.1.42
python sender.py send next --ip 192.168.1.42
python sender.py status --ip 192.168.1.42
python sender.py bt play
```

## Память: честный ответ про «6 МБ»
На борту обычно **4 МБ flash + 8 МБ PSRAM** (PSRAM — это как оперативка, туда
прошивка кладёт кадры). Flash под файлы ~1.5 МБ — поэтому всё и хранится на ПК,
а ESP32 держит в памяти только текущий кадр и докачивает следующий фоном.
Гирлянду и музыку тащить по Wi-Fi тоже не обязательно — код `audio.h` готов к
стримингу той же схемой предзагрузки.

## План «до Нового года» 🙂
- [x] Прошивка: дисплей + предзагрузка + Wi-Fi API + Bluetooth + гирлянда
- [x] ПК-приложение: конвертер видео, сервер файлов, пульт
- [ ] Вписать свой Wi-Fi и тип дисплея в `config.h`
- [ ] Проверить IP ленты и кнопку «Подключить ПК→ленту»
- [ ] (по желанию) докупить MAX98357A + 2 динамика → раскомментировать `AUDIO_ENABLE`
