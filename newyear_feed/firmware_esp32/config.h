// ============================================================================
//  config.h — настройки «Новогодней ленты» на ESP32
//  Правьте только этот файл, если меняете пины/сеть.
// ============================================================================
#pragma once

// --------------------------- Wi-Fi ------------------------------------------
#define WIFI_SSID       "ШКОЛА-WIFI"        // <<< вставьте имя вашей сети
#define WIFI_PASS       "пароль"            // <<< вставьте пароль

// ------------------------- TFT-дисплей --------------------------------------
// Проверенные пресеты (выберите ОДИН, раскомментировав):
//
// Вариант A: самый популярный модуль «ESP32 2.4\" TFT» (ILI9341 + XPT2046 тач)
#define TFT_DRIVER   ILI9341_BLACKPILL      // SPI: MOSI=23 MISO=27 SCLK=25 CS=15 DC=2 RST=-1 BL=14 TOUCH: IRQ=32 CS=33
//
// Вариант B: «Lilygo T-Display» (ESP32 + 1.9\" ST7789)
// #define TFT_DRIVER  LILYGO_TDISPLAY       // SPI: MOSI=8 SCLK=7 DC=16 RST=23 CS=5 BL=4 BL_PWM=17
//
// Вариант C: «Waveshare / generic ST7789 1.3\"-2\"»
// #define TFT_DRIVER  ST7789_GENERIC        // SPI: MOSI=23 SCLK=18 DC=16 RST=-1 CS=5 BL=-1
//
// Вариант D: параллельный RGB-дисплей 480x320 (дорогие платы с 8MB PSRAM)
// #define TFT_DRIVER  RGB_PARALLEL_480x320  // см. comments внутри setupDisplay()

#define ILI9341_BLACKPILL     1
#define LILYGO_TDISPLAY       2
#define ST7789_GENERIC        3
#define RGB_PARALLEL_480x320  4

#ifndef TFT_DRIVER
#define TFT_DRIVER ILI9341_BLACKPILL
#endif

// ---------------------------- NeoPixel --------------------------------------
#define NEOPIXEL_PIN      4      // DIN первой LED-гирлянды
#define NEOPIXEL_COUNT    60     // сколько светодиодов в ленте (поправьте под свою)
#define NEOPIXEL_TYPE     NEO_GRB + NEO_KHZ800

// ---------------------------- Серво для «снежинки» --------------------------
#define SERVO_PIN         26     // если серво нет — закомментируйте USE_SERVO
//#define USE_SERVO                // раскомментируйте, чтобы включить мотор

// ---------------------------- Bluetooth LE UART ------------------------------
// Имя устройства в списке Bluetooth на телефоне/ПК:
#define BT_DEVICE_NAME    "NY-Feed-ESP32"

// ---------------------------- HTTP-порт и пути --------------------------------
#define HTTP_PORT         80
#define DATA_DIR          "/data"          // каталог на SD или LittleFS
#define INDEX_FILE        "/data/index.json"

// ---------------------------- Буферизация (главная фишка) --------------------
// Пока идёт показ клипа N, лента молча докачивает клип N+1.
// Если за 5 секунд до конца клипа следующий ещё не готов — показываем ошибку
// и перезагружаем тот же файл заново.
#define PREBUFFER_LEAD_MS   5000    // начинать проверку готовности следующего файла
#define RELOAD_TIMEOUT_MS   300000  // максимум 5 минут на докачку одного файла

// ---------------------------- Хранение файлов ---------------------------------
// true — файлы на microSD (SD_CS = 5). false — LittleFS во flash (до ~1.5 МБ)
#define USE_SD_CARD     false
#define SD_CS_PIN       5
