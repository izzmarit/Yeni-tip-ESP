/**
 * @file config.h
 * @brief Temel yapılandırma ayarları ve sabitler
 * @version 1.1
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <esp_task_wdt.h>

// Watchdog ayarları
#define WDT_TIMEOUT 10 // Saniye cinsinden normal zaman aşımı süresi
#define WDT_LONG_TIMEOUT 30 // Uzun işlemler için zaman aşımı süresi
#define WDT_PANIC_MODE true // true: panik modu (reset)

// EEPROM yazma gecikmesi (ms) - 5 dakika
#define EEPROM_WRITE_DELAY 300000
#define EEPROM_MAX_CHANGES 10 // Maksimum değişiklik sayısı aşıldığında anında yazma

// Ekran Pinleri
#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     2
#define TFT_MOSI   23 // SDA
#define TFT_SCLK   18 // SCL
#define TFT_LED    16  // Ekran arka ışık pini

// SHT31 Sensör Adresleri
#define SHT31_ADDR_1 0x44 // Alt sensör I2C adresi
#define SHT31_ADDR_2 0x45 // Üst sensör I2C adresi (ADDR pini HIGH olarak ayarlanmalıdır)

// I2C Pinleri
#define I2C_SDA    21
#define I2C_SCL    22

// Röle Pinleri
#define RELAY_HEAT 25  // Isıtıcı rölesi
#define RELAY_HUMID 26 // Nem rölesi
#define RELAY_MOTOR 27 // Motor rölesi

// Joystick Pinleri
#define JOY_X      34
#define JOY_Y      35
#define JOY_BTN    32

// Alarm Pini
#define ALARM_PIN  33

// WiFi Ayarları
#define AP_SSID "KULUCKA_MK_v5"
#define AP_PASS "12345678"
#define WIFI_PORT 80

// WiFi Bağlantı Zaman Aşımları
#define WIFI_CONNECTION_TIMEOUT 15000  // 15 saniye bağlantı zaman aşımı
#define WIFI_RETRY_INTERVAL 30000     // 30 saniye tekrar deneme aralığı
#define WIFI_MAX_RETRY_COUNT 3        // Maksimum tekrar deneme sayısı

// EEPROM Ayarları
#define EEPROM_SIZE 512

// PID Varsayılan Değerleri
#define PID_KP 10.0
#define PID_KI 0.1
#define PID_KD 5.0

// PID Sınır Değerleri
#define PID_KP_MIN 0.1
#define PID_KP_MAX 100.0
#define PID_KI_MIN 0.01
#define PID_KI_MAX 10.0
#define PID_KD_MIN 0.1
#define PID_KD_MAX 100.0

// PID Otomatik Ayarlama Ayarları
#define PID_AUTOTUNE_TIMEOUT 1800000  // 30 dakika maksimum süre
#define PID_AUTOTUNE_TEMP_TOLERANCE 2.0  // ±2°C güvenlik sınırı
#define PID_AUTOTUNE_MIN_CYCLES 2     // Minimum salınım sayısı

// Kuluçka Türleri İndeksleri
#define INCUBATION_CHICKEN 0
#define INCUBATION_QUAIL 1
#define INCUBATION_GOOSE 2
#define INCUBATION_MANUAL 3

// Sensör Okuma Gecikmesi (ms)
#define SENSOR_READ_DELAY 2000

// Ekran Yenileme Gecikmesi (ms) - 3000'den 1000'e düşürüldü
#define DISPLAY_REFRESH_DELAY 1000

// Joystick okuma gecikmesi (ms)
#define JOYSTICK_READ_DELAY 100

// Varsayılan Motor Ayarları
#define DEFAULT_MOTOR_WAIT_TIME 120  // Dakika
#define DEFAULT_MOTOR_RUN_TIME 14    // Saniye

// Motor Sınır Değerleri
#define MOTOR_WAIT_TIME_MIN 1        // Minimum bekleme süresi (dakika)
#define MOTOR_WAIT_TIME_MAX 240      // Maksimum bekleme süresi (dakika)
#define MOTOR_RUN_TIME_MIN 1         // Minimum çalışma süresi (saniye)
#define MOTOR_RUN_TIME_MAX 60        // Maksimum çalışma süresi (saniye)

// Alarm Eşik Değerleri
#define DEFAULT_TEMP_LOW_ALARM 1.0   // Hedeften 1°C düşük
#define DEFAULT_TEMP_HIGH_ALARM 1.0  // Hedeften 1°C yüksek
#define DEFAULT_HUMID_LOW_ALARM 10   // Hedeften %10 düşük
#define DEFAULT_HUMID_HIGH_ALARM 10  // Hedeften %10 yüksek

// Alarm Sınır Değerleri
#define ALARM_TEMP_MIN 0.1           // Minimum sıcaklık alarm eşiği
#define ALARM_TEMP_MAX 5.0           // Maksimum sıcaklık alarm eşiği
#define ALARM_HUMID_MIN 1            // Minimum nem alarm eşiği
#define ALARM_HUMID_MAX 20           // Maksimum nem alarm eşiği

// Sıcaklık ve Nem Sınır Değerleri
#define TEMP_MIN 20.0                // Minimum hedef sıcaklık
#define TEMP_MAX 40.0                // Maksimum hedef sıcaklık
#define HUMID_MIN 30                 // Minimum hedef nem
#define HUMID_MAX 90                 // Maksimum hedef nem

// Kalibrasyon Sınır Değerleri
#define TEMP_CALIBRATION_MIN -10.0   // Minimum sıcaklık kalibrasyonu
#define TEMP_CALIBRATION_MAX 10.0    // Maksimum sıcaklık kalibrasyonu
#define HUMID_CALIBRATION_MIN -20    // Minimum nem kalibrasyonu
#define HUMID_CALIBRATION_MAX 20     // Maksimum nem kalibrasyonu

// Ekran Ayarları
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

// Renk Tanımları
#define COLOR_BACKGROUND 0x0000  // Siyah
#define COLOR_TEXT 0xFFFF        // Beyaz
#define COLOR_TEMP 0xF800        // Kırmızı
#define COLOR_HUMID 0x001F       // Mavi
#define COLOR_HIGHLIGHT 0x07E0   // Yeşil
#define COLOR_DIVISION 0x7BEF    // Gri
#define COLOR_ALARM 0xF81F       // Mor
#define COLOR_TARGET 0xFFE0      // Sarı
#define COLOR_PID_ACTIVE 0x07FF  // Cyan (PID aktif)
#define COLOR_PID_INACTIVE 0x8410 // Koyu gri (PID inaktif)

// Menü Zaman Aşımı Ayarları
#define MENU_TIMEOUT 30000       // 30 saniye menü zaman aşımı
#define MENU_INACTIVE_TIMEOUT 60000 // 1 dakika inaktivite zaman aşımı

// WiFi Durum Kontrol Ayarları
#define WIFI_STATUS_CHECK_INTERVAL 5000  // 5 saniyede bir durum kontrolü
#define WIFI_RECONNECT_INTERVAL 60000    // 1 dakikada bir yeniden bağlanma denemesi

// Web Sunucu Ayarları
#define WEB_REQUEST_TIMEOUT 5000         // 5 saniye istek zaman aşımı
#define WEB_MAX_CLIENTS 4                // Maksimum eş zamanlı istemci sayısı

// JSON Buffer Boyutları
#define JSON_BUFFER_SIZE_SMALL 256       // Küçük JSON buffer
#define JSON_BUFFER_SIZE_MEDIUM 512      // Orta JSON buffer
#define JSON_BUFFER_SIZE_LARGE 1024      // Büyük JSON buffer

// Sistem Performans Ayarları
#define WATCHDOG_FEED_INTERVAL 5000      // 5 saniyede bir watchdog besleme
#define STORAGE_CHECK_INTERVAL 10000     // 10 saniyede bir storage kontrolü
#define SENSOR_ERROR_THRESHOLD 5         // Maksimum sensör hata sayısı
#define I2C_ERROR_THRESHOLD 10           // Maksimum I2C hata sayısı

// Histerezis Varsayılan Değerleri
#define HYSTERESIS_LOW_THRESHOLD 5.0     // Varsayılan düşük eşik (%5)
#define HYSTERESIS_HIGH_THRESHOLD 2.0    // Varsayılan yüksek eşik (%2)

// Sistem Durum LED'i (varsa)
#define STATUS_LED_PIN -1                // Durum LED pini (-1 = kullanılmıyor)

// Debug Ayarları
#define DEBUG_SERIAL_SPEED 115200        // Debug seri port hızı
#define DEBUG_ENABLED true               // Debug mesajları etkin mi?

// Güvenlik Ayarları
#define SAFETY_TEMP_MAX 45.0             // Güvenlik maksimum sıcaklığı
#define SAFETY_TEMP_MIN 15.0             // Güvenlik minimum sıcaklığı
#define EMERGENCY_SHUTDOWN_TEMP 50.0     // Acil kapatma sıcaklığı

#endif // CONFIG_H