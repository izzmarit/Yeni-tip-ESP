/**
 * @file config.h
 * @brief Temel yapılandırma ayarları ve sabitler
 * @version 1.0
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

// EEPROM Ayarları
#define EEPROM_SIZE 512

// PID Varsayılan Değerleri
#define PID_KP 10.0
#define PID_KI 0.1
#define PID_KD 5.0

// Kuluçka Türleri İndeksleri
#define INCUBATION_CHICKEN 0
#define INCUBATION_QUAIL 1
#define INCUBATION_GOOSE 2
#define INCUBATION_MANUAL 3

// Sensör Okuma Gecikmesi (ms)
#define SENSOR_READ_DELAY 2000

// Ekran Yenileme Gecikmesi (ms)
#define DISPLAY_REFRESH_DELAY 1000

// Joystick okuma gecikmesi (ms)
#define JOYSTICK_READ_DELAY 100

// Varsayılan Motor Ayarları
#define DEFAULT_MOTOR_WAIT_TIME 120  // Dakika
#define DEFAULT_MOTOR_RUN_TIME 14    // Saniye

// Alarm Eşik Değerleri
#define DEFAULT_TEMP_LOW_ALARM 1.0   // Hedeften 1°C düşük
#define DEFAULT_TEMP_HIGH_ALARM 1.0  // Hedeften 1°C yüksek
#define DEFAULT_HUMID_LOW_ALARM 10   // Hedeften %10 düşük
#define DEFAULT_HUMID_HIGH_ALARM 10  // Hedeften %10 yüksek

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

#endif // CONFIG_H