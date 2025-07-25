/**
 * @file main.cpp
 * @brief KULUÇKA MK v5.0 ana uygulama dosyası (PID menü sistemi iyileştirildi)
 * @version 1.7
 */

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "sensors.h"
#include "rtc.h"
#include "joystick.h"
#include "relays.h"
#include "incubation.h"
#include "pid.h"
#include "hysteresis.h"
#include "menu.h"
#include "storage.h"
#include "wifi_manager.h"
#include "alarm.h"
#include "watchdog_manager.h"
#include "pid_auto_tune.h"
#include "relays.h"
#include "i2c_manager.h"
#include "ota_manager.h"

// Modül nesneleri
Display display;
Sensors sensors;
RTCModule rtc;
Joystick joystick;
Relays relays;
Incubation incubation;
PIDController pidController;
Hysteresis hysteresisController;
MenuManager menuManager;
Storage storage;
WiFiManager wifiManager;
AlarmManager alarmManager;
WatchdogManager watchdogManager;
OTAManager otaManager;

// Zaman kontrolü değişkenleri
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastJoystickReadTime = 0;
unsigned long lastMenuTimeout = 0;
unsigned long lastStorageCheckTime = 0;

// Menü zaman aşımı (ms) - 30 saniye
const unsigned long MENU_TIMEOUT_MS = 30000;

// Joystick durum kontrolü için
bool joystickNeedsReset = false;
unsigned long joystickResetTime = 0;
const unsigned long JOYSTICK_RESET_DELAY = 300; // 300ms bekleme

// Motor test için global değişkenler
bool motorTestActive = false;
unsigned long motorTestStartTime = 0;
unsigned long motorTestDuration = 0;
bool motorTestRequested = false;
uint32_t requestedTestDuration = 0;

// Fonksiyon prototipleri
void initializeModules();
void handleJoystick();
void updateSensors();
void updateDisplay();
void updateRelays();
void updateAlarm();
void checkStorageQueue();
void handleMenuActions(JoystickDirection direction);
void loadSettingsFromStorage();
void saveSettingsToStorage();
void handleValueAdjustment(JoystickDirection direction);
void handlePIDAutoTune();
void handleWifiParameterUpdate(String param, String value);
void updateMenuWithCurrentStatus();
void updateWiFiStatus();
void handleMotorTest();

// YENİ EKLENEN FONKSİYON PROTOTİPLERİ
void handleTimeAdjustment(JoystickDirection direction);
void handleDateAdjustment(JoystickDirection direction);
void handleValueAdjustmentScreen(JoystickDirection direction);
bool isTerminalMenu(MenuState state);
void handleTerminalMenu(MenuState state, JoystickDirection direction);
void handleNormalMenuNavigation(JoystickDirection direction, MenuState currentState);
void returnToTimeDateMenu();
void returnToPreviousMenu();
void showCurrentMenu();
void updateTimeDisplay();
void updateDateDisplay();
void updateValueDisplay();
void updateMenuDisplay(MenuState newState);

void updateWiFiStatus() {
    DateTime currentDateTime = rtc.getCurrentDateTime();
    
    // Sensör değerlerini ayrı ayrı oku
    float temp1 = sensors.readTemperature(0);
    float temp2 = sensors.readTemperature(1);
    float humid1 = sensors.readHumidity(0);
    float humid2 = sensors.readHumidity(1);
    bool sensor1Working = sensors.isSensorWorking(0);
    bool sensor2Working = sensors.isSensorWorking(1);
    
    // Tüm durum verilerini güncelle
    wifiManager.updateStatusData(
        sensors.readTemperature(),
        sensors.readHumidity(),
        relays.getHeaterState(),
        relays.getHumidifierState(),
        relays.getMotorState(),
        incubation.getDisplayDay(currentDateTime),
        incubation.getTotalDays(),
        incubation.getIncubationTypeName(),
        pidController.getSetpoint(),        // Gerçek hedef sıcaklık
        hysteresisController.getSetpoint(), // Gerçek hedef nem
        incubation.isIncubationCompleted(),
        incubation.getCurrentDay(currentDateTime),
        temp1, temp2, humid1, humid2,
        sensor1Working, sensor2Working
    );
    
    // PID modunu güncelle
    wifiManager.setPidMode((int)pidController.getPIDMode());
}

void setup() {
    // Seri port başlatma
    Serial.begin(115200);
    Serial.println("KULUCKA MK v5.0 Baslatiliyor...");
    
    // Watchdog timer'ı başlat - YENİ YAKLAŞIM
    watchdogManager.begin();
    
    // Sistem başlatma işlemini başlat
    watchdogManager.beginOperation(OP_SYSTEM_INIT, "Sistem Başlatma");
    
    // Tüm modülleri başlat
    initializeModules();
    
    // Kaydedilmiş ayarları yükle
    watchdogManager.beginOperation(OP_STORAGE_READ, "Ayarları Yükleme");
    loadSettingsFromStorage();
    watchdogManager.endOperation();
    
    // WiFi Manager'a storage referansını ver
    wifiManager.setStorage(&storage);
    
    // Açılış ekranını göster
    watchdogManager.beginOperation(OP_DISPLAY_UPDATE, "Açılış Ekranı");
    display.showSplashScreen();
    watchdogManager.endOperation();
    
    // Ana ekranı ayarla
    display.setupMainScreen();
    
    // Menü durumunu güncelle
    updateMenuWithCurrentStatus();

    otaManager.checkRollback();
    
    Serial.println("KULUCKA MK v5.0 Hazir!");
    watchdogManager.endOperation(); // Sistem başlatma işlemi tamamlandı
}

void loop() {
    // Mevcut zaman
    unsigned long currentMillis = millis();
    
    // Düzenli watchdog beslemesi - İYİLEŞTİRİLMİŞ
    watchdogManager.feed();

    // Motor test işlemini kontrol et
    handleMotorTest();
    
    // Sensörleri güncelle - İYİLEŞTİRİLMİŞ
    if (currentMillis - lastSensorReadTime >= SENSOR_READ_DELAY) {
        lastSensorReadTime = currentMillis;
        
        watchdogManager.beginOperation(OP_SENSOR_READ, "Sensör Okuma");
        updateSensors();
        watchdogManager.endOperation();
    }
    
    // Joystick kontrolü - İYİLEŞTİRİLMİŞ
    if (currentMillis - lastJoystickReadTime >= JOYSTICK_READ_DELAY) {
        lastJoystickReadTime = currentMillis;
        
        watchdogManager.beginOperation(OP_MENU_NAVIGATION, "Joystick İşleme");
        handleJoystick();
        watchdogManager.endOperation();
    }
    
    // Ekranı güncelle - İYİLEŞTİRİLMİŞ
    if (currentMillis - lastDisplayUpdateTime >= DISPLAY_REFRESH_DELAY) {
        lastDisplayUpdateTime = currentMillis;
        
        if (display.getCurrentMode() == DISPLAY_MAIN) {
            watchdogManager.beginOperation(OP_DISPLAY_UPDATE, "Ana Ekran Güncelleme");
            updateDisplay();
            watchdogManager.endOperation();
        }
    }
    
    // Röleleri güncelle - NORMAL
    updateRelays();
    
    // Alarm durumunu güncelle - NORMAL
    updateAlarm();
    
    // Bekleyen EEPROM değişikliklerini kontrol et - İYİLEŞTİRİLMİŞ
    if (currentMillis - lastStorageCheckTime >= 10000) {
        lastStorageCheckTime = currentMillis;
        
        watchdogManager.beginOperation(OP_STORAGE_WRITE, "Storage İşlemleri");
        checkStorageQueue();
        watchdogManager.endOperation();
    }
    
    // Geliştirilmiş periyodik otomatik kayıt
    static unsigned long lastPeriodicSave = 0;
    static unsigned long lastEmergencySave = 0;
    static unsigned long lastCriticalCheck = 0;
    
    // Her 30 saniyede bir değişiklik kontrolü ve kritik durum analizi
    if (currentMillis - lastPeriodicSave >= 30000) { // 30 saniye
        lastPeriodicSave = currentMillis;
        
        if (storage.getPendingChanges() > 0) {
            watchdogManager.beginOperation(OP_STORAGE_WRITE, "Periyodik Kayıt");
            
            // Kritik sistem durumunu kontrol et
            bool criticalState = false;
            String criticalReason = ""; // String tipinde tanımlandığından emin olun
            
            // Kuluçka çalışıyor mu?
            if (incubation.isIncubationRunning()) {
                criticalState = true;
                criticalReason = "Kuluçka aktif";
            }
            
            // Sıcaklık sapması kontrolü
            float tempDeviation = abs(sensors.readTemperature() - pidController.getSetpoint());
            if (tempDeviation > 2.0) {
                criticalState = true;
                if (criticalReason.length() > 0) {
                    criticalReason += ", ";
                }
                criticalReason += "Yüksek sıcaklık sapması: " + String(tempDeviation, 1) + "°C";
            }
            
            // Nem sapması kontrolü
            float humidDeviation = abs(sensors.readHumidity() - hysteresisController.getSetpoint());
            if (humidDeviation > 10.0) {
                criticalState = true;
                if (criticalReason.length() > 0) {
                    criticalReason += ", ";
                }
                criticalReason += "Yüksek nem sapması: " + String(humidDeviation, 0) + "%";
            }
            
            // Alarm aktif mi?
            if (alarmManager.isAlarmActive()) {
                criticalState = true;
                if (criticalReason.length() > 0) {
                    criticalReason += ", ";
                }
                criticalReason += "Alarm aktif";
            }
            
            if (criticalState) {
                storage.saveStateNow();
                Serial.println("Kritik durum tespit edildi - veriler anında kaydedildi");
                Serial.println("Sebep: " + criticalReason);
            } else {
                storage.processQueue();
                Serial.println("Periyodik kontrol - " + String(storage.getPendingChanges()) + 
                              " bekleyen değişiklik");
            }
            
            watchdogManager.endOperation();
        }
    }
    
    // Her 15 saniyede bir kritik parametre kontrolü
    if (currentMillis - lastCriticalCheck >= 15000) { // 15 saniye
        lastCriticalCheck = currentMillis;
        
        // Sistem durumunu logla
        static float lastLoggedTemp = 0;
        static float lastLoggedHumid = 0;
        
        float currentTemp = sensors.readTemperature();
        float currentHumid = sensors.readHumidity();
        
        // Değerler önemli ölçüde değiştiyse logla
        if (abs(currentTemp - lastLoggedTemp) > 0.5 || abs(currentHumid - lastLoggedHumid) > 2.0) {
            Serial.println("Sistem durumu - Sıcaklık: " + String(currentTemp, 1) + "°C/" + 
                          String(pidController.getSetpoint(), 1) + "°C, Nem: " + 
                          String(currentHumid, 0) + "%/" + String(hysteresisController.getSetpoint(), 0) + "%");
            lastLoggedTemp = currentTemp;
            lastLoggedHumid = currentHumid;
        }
    }
    
    // Acil durum kayıt mekanizması (5 dakikada bir zorunlu)
    if (currentMillis - lastEmergencySave >= 300000) { // 300 saniye = 5 dakika
        lastEmergencySave = currentMillis;
        
        watchdogManager.beginOperation(OP_STORAGE_WRITE, "Zorunlu Kayıt");
        
        Serial.println("=== 5 DAKİKALIK ZORUNLU KAYIT ===");
        Serial.println("Bekleyen değişiklik sayısı: " + String(storage.getPendingChanges()));
        Serial.println("Son kayıttan geçen süre: " + String(storage.getTimeSinceLastSave() / 1000) + " saniye");
        
        storage.saveStateNow();
        
        // Kayıt sonrası durum özeti
        Serial.println("Sistem Özeti:");
        Serial.println("- Kuluçka: " + String(incubation.isIncubationRunning() ? "Aktif" : "Pasif"));
        Serial.println("- PID Modu: " + pidController.getPIDModeString());
        Serial.println("- Sıcaklık: " + String(sensors.readTemperature(), 1) + "°C");
        Serial.println("- Nem: " + String(sensors.readHumidity(), 0) + "%");
        Serial.println("- WiFi: " + wifiManager.getStatusString());
        Serial.println("- Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        Serial.println("=================================");
        
        watchdogManager.endOperation();
    }
    
    // WiFi isteklerini işle - İYİLEŞTİRİLMİŞ
    wifiManager.handleRequests();
    
    // PID Otomatik Ayarlama durumunu kontrol et - İYİLEŞTİRİLMİŞ
    if (pidController.isAutoTuneEnabled()) {
        watchdogManager.beginOperation(OP_PID_AUTOTUNE, "PID Otomatik Ayarlama");
        handlePIDAutoTune();
        watchdogManager.endOperation();
    }
}

void handleMotorTest() {
    // Test başlatma isteği varsa
    if (motorTestRequested && !motorTestActive) {
        motorTestRequested = false;
        motorTestActive = true;
        motorTestStartTime = millis();
        motorTestDuration = requestedTestDuration * 1000UL; // milisaniyeye çevir
        
        relays.setMotor(true);
        updateWiFiStatus();
        
        Serial.println("Motor test başlatıldı - Süre: " + String(requestedTestDuration) + " saniye");
    }
    
    // Aktif test varsa kontrol et
    if (motorTestActive) {
        unsigned long elapsed = millis() - motorTestStartTime;
        
        if (elapsed >= motorTestDuration) {
            // Test tamamlandı
            motorTestActive = false;
            relays.setMotor(false);
            updateWiFiStatus();
            
            Serial.println("Motor test tamamlandı");
            
            // Test tamamlandı mesajı göster
            display.showConfirmationMessage("Motor Testi Tamamlandi");
            
            // 2 saniye sonra ana ekrana dön
            static unsigned long lastDisplayReset = 0;
            if (millis() - lastDisplayReset > 2000) {
                display.setupMainScreen();
                lastDisplayReset = millis();
            }
        } else {
            // Test devam ediyor, progress göster
            static unsigned long lastProgressUpdate = 0;
            if (millis() - lastProgressUpdate > 500) { // Her 500ms'de bir güncelle
                lastProgressUpdate = millis();
                
                unsigned long remaining = (motorTestDuration - elapsed) / 1000;
                Serial.println("Motor test - Kalan süre: " + String(remaining) + " saniye");
                
                // Progress bar göster
                int percentage = 100 - (int)((elapsed * 100) / motorTestDuration);
                display.showProgressBar(
                    20, SCREEN_HEIGHT / 2, 
                    SCREEN_WIDTH - 40, 20, 
                    COLOR_HIGHLIGHT, 
                    percentage
                );
            }
        }
    }
}


void initializeModules() {
    // I2C Manager'ı ilk olarak başlat
    watchdogManager.beginOperation(OP_SYSTEM_INIT, "I2C Bus Başlatma");
    if (!I2C_MANAGER.begin()) {
        Serial.println("I2C Manager başlatma hatası!");
    } else {
        Serial.println("I2C Manager başarıyla başlatıldı");
        
        // I2C bus taraması yap
        I2C_MANAGER.scanBus();
    }
    watchdogManager.endOperation();
    
    // Storage modülünü ikinci sırada başlat (FRAM için I2C gerekli)
    watchdogManager.beginOperation(OP_STORAGE_WRITE, "Storage Başlatma");
    if (!storage.begin()) {
        Serial.println("Saklama yönetimi başlatma hatası!");
#if USE_FRAM
        Serial.println("FRAM ve EEPROM başlatılamadı!");
#endif
    }
    watchdogManager.endOperation();
    
    // Ekran modülü
    watchdogManager.beginOperation(OP_DISPLAY_UPDATE, "Ekran Başlatma");
    if (!display.begin()) {
        Serial.println("Ekran başlatma hatası!");
    }
    watchdogManager.endOperation();
    
    // Sensör modülü
    watchdogManager.beginOperation(OP_SENSOR_READ, "Sensör Başlatma");
    if (!sensors.begin()) {
        Serial.println("Sensör başlatma hatası! En az bir sensör çalışmalı.");
    }
    watchdogManager.endOperation();
    
    // RTC modülü
    if (!rtc.begin()) {
        Serial.println("RTC başlatma hatası!");
    }
    
    // Joystick modülü
    if (!joystick.begin()) {
        Serial.println("Joystick başlatma hatası!");
    }
    
    // Röle modülü
    if (!relays.begin()) {
        Serial.println("Röle başlatma hatası!");
    }

    // YENİ EKLEME: Storage referansını ayarla
    relays.setStorage(&storage);
    
    // Kuluçka modülü
    if (!incubation.begin()) {
        Serial.println("Kuluçka kontrolü başlatma hatası!");
    }
    
    // PID kontrolü
    if (!pidController.begin()) {
        Serial.println("PID kontrolü başlatma hatası!");
    }
    
    // Histerezis kontrolü
    if (!hysteresisController.begin()) {
        Serial.println("Histerezis kontrolü başlatma hatası!");
    }
    
    // Menü yönetimi
    if (!menuManager.begin()) {
        Serial.println("Menü yönetimi başlatma hatası!");
    }
    
    // WiFi yönetimi
    watchdogManager.beginOperation(OP_WIFI_CONNECT, "WiFi Başlatma");
    if (!wifiManager.begin()) {
        Serial.println("WiFi başlatma hatası!");
    } else {
        wifiManager.startServer();
    }
    watchdogManager.endOperation();
    
    // Alarm yönetimi
    if (!alarmManager.begin()) {
        Serial.println("Alarm yönetimi başlatma hatası!");
    }
    
    // initializeModules() fonksiyonuna ekleyin
    if (!otaManager.begin()) {
        Serial.println("OTA Manager başlatma hatası!");
    }
    otaManager.setStorage(&storage);
    otaManager.setWatchdog(&watchdogManager);
}

void updateMenuWithCurrentStatus() {
    // PID durumunu güncelle
    menuManager.updatePIDMenuItems();
    
    // WiFi durumunu güncelle
    menuManager.updateWiFiMenuItems();

    // Alarm durumunu güncelle 
    menuManager.updateAlarmMenuItems();
}

void handleJoystick() {
    joystick.update();
    JoystickDirection direction = joystick.readDirection();
    
    if (direction == JOYSTICK_NONE) {
        return;
    }
    
    Serial.print("Joystick: ");
    Serial.println(direction);
    
    menuManager.updateInteractionTime();
    MenuState currentState = menuManager.getCurrentState();
    
    // Ana ekranda sadece sağ yön menüye giriş yapabilir
    if (currentState == MENU_NONE) {
        if (direction == JOYSTICK_RIGHT) {
            menuManager.setCurrentState(MENU_MAIN);
            updateMenuWithCurrentStatus();
            
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(
                    items.data(),
                    items.size(),
                    menuManager.getSelectedIndex()
                );
            }
        }
        return;
    }
    
    // Saat ayarlama ekranındaysa özel kontrol
    if (menuManager.isInTimeAdjustScreen()) {
        if (direction == JOYSTICK_LEFT) {
            menuManager.setCurrentState(MENU_TIME_DATE);
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
            return;
        } else if (direction == JOYSTICK_RIGHT) {
            menuManager.update(direction);
            display.showTimeAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getTimeString(),
                menuManager.getTimeField()
            );
            return;
        } else if (direction == JOYSTICK_UP || direction == JOYSTICK_DOWN) {
            menuManager.update(direction);
            display.showTimeAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getTimeString(),
                menuManager.getTimeField()
            );
            return;
        } else if (direction == JOYSTICK_PRESS) {
            handleValueAdjustment(JOYSTICK_PRESS);
            return;
        }
        return;
    }

    // Tarih ayarlama ekranındaysa özel kontrol
    if (menuManager.isInDateAdjustScreen()) {
        if (direction == JOYSTICK_LEFT) {
            menuManager.setCurrentState(MENU_TIME_DATE);
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
            return;
        } else if (direction == JOYSTICK_RIGHT) {
            menuManager.update(direction);
            display.showDateAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getDateString(),
                menuManager.getDateField()
            );
            return;
        } else if (direction == JOYSTICK_UP || direction == JOYSTICK_DOWN) {
            menuManager.update(direction);
            display.showDateAdjustScreen(
                menuManager.getAdjustTitle(),
                menuManager.getDateString(),
                menuManager.getDateField()
            );
            return;
        } else if (direction == JOYSTICK_PRESS) {
            handleValueAdjustment(JOYSTICK_PRESS);
            return;
        }
        return;
    }
    
    // Normal değer ayarlama ekranı (saat/tarih dışındaki ayarlar)
    if (menuManager.isInValueAdjustScreen()) {
        if (direction == JOYSTICK_PRESS) {
            handleValueAdjustment(direction);
        } else if (direction == JOYSTICK_LEFT) {
            MenuState prevState = menuManager.getPreviousState();
            menuManager.setCurrentState(prevState);
            
            if (prevState == MENU_NONE) {
                display.setupMainScreen();
            } else {
                std::vector<String> items = menuManager.getMenuItems();
                if (!items.empty()) {
                    display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
                }
            }
            return;
        } else if (direction == JOYSTICK_UP || direction == JOYSTICK_DOWN || direction == JOYSTICK_RIGHT) {
            menuManager.update(direction);
            display.showValueAdjustScreen(
                menuManager.getAdjustTitle(),
                String(menuManager.getAdjustedValue()),
                menuManager.getAdjustUnit()
            );
        }
        return;
    }
    
    // Terminal menülerde özel işlemler
    MenuState terminalMenus[] = {
        MENU_SENSOR_VALUES, MENU_TEMPERATURE, MENU_HUMIDITY,
        MENU_MOTOR_WAIT, MENU_MOTOR_RUN, MENU_MOTOR_TEST,
        MENU_SET_TIME, MENU_SET_DATE,
        MENU_PID_KP, MENU_PID_KI, MENU_PID_KD,
        MENU_PID_AUTO_TUNE, MENU_PID_MANUAL_START, MENU_PID_OFF,
        MENU_CALIBRATION_TEMP_1, MENU_CALIBRATION_TEMP_2,
        MENU_CALIBRATION_HUMID_1, MENU_CALIBRATION_HUMID_2,
        MENU_ALARM_ENABLE_ALL, MENU_ALARM_DISABLE_ALL,
        MENU_ALARM_TEMP_LOW, MENU_ALARM_TEMP_HIGH,
        MENU_ALARM_HUMID_LOW, MENU_ALARM_HUMID_HIGH,
        MENU_ALARM_MOTOR,
        MENU_SENSOR_VALUES,
        MENU_MANUAL_DEV_TEMP, MENU_MANUAL_HATCH_TEMP,
        MENU_MANUAL_DEV_HUMID, MENU_MANUAL_HATCH_HUMID,
        MENU_MANUAL_DEV_DAYS, MENU_MANUAL_HATCH_DAYS,
        MENU_MANUAL_START,
        MENU_WIFI_MODE, MENU_WIFI_SSID, MENU_WIFI_PASSWORD, MENU_WIFI_CONNECT
    };
    
    bool isTerminalMenu = false;
    for (int i = 0; i < sizeof(terminalMenus)/sizeof(terminalMenus[0]); i++) {
        if (currentState == terminalMenus[i]) {
            isTerminalMenu = true;
            break;
        }
    }
    
    if (isTerminalMenu) {
        if (direction == JOYSTICK_LEFT) {
            menuManager.update(direction);
            MenuState newState = menuManager.getCurrentState();
            
            if (newState == MENU_NONE) {
                display.setupMainScreen();
            } else {
                std::vector<String> items = menuManager.getMenuItems();
                if (!items.empty()) {
                    display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
                }
            }
            return;
        } else if (direction == JOYSTICK_PRESS || direction == JOYSTICK_RIGHT) {
            handleMenuActions(direction);
            return;
        }
    }
    
    // Normal menü navigasyonu
    menuManager.update(direction);
    MenuState newState = menuManager.getCurrentState();
    
    // Menü işlemleri
    if (direction == JOYSTICK_PRESS || direction == JOYSTICK_RIGHT) {
        handleMenuActions(direction);
    }
    
    // Ekran güncellemeleri
    if (newState == MENU_NONE) {
        display.setupMainScreen();
    } else if (menuManager.isInTimeAdjustScreen()) {
        display.showTimeAdjustScreen(
            menuManager.getAdjustTitle(),
            menuManager.getTimeString(),
            menuManager.getTimeField()
        );
    } else if (menuManager.isInDateAdjustScreen()) {
        display.showDateAdjustScreen(
            menuManager.getAdjustTitle(),
            menuManager.getDateString(),
            menuManager.getDateField()
        );
    } else if (menuManager.isInValueAdjustScreen()) {
        display.showValueAdjustScreen(
            menuManager.getAdjustTitle(),
            String(menuManager.getAdjustedValue()),
            menuManager.getAdjustUnit()
        );
    } else if (menuManager.isInMenu()) {
        std::vector<String> items = menuManager.getMenuItems();
        if (!items.empty()) {
            display.showMenu(
                items.data(),
                items.size(),
                menuManager.getSelectedIndex()
            );
        }
    }
}

void updateSensors() {
    // Sensör verilerini okumadan önce watchdog beslemesi
    bool needWatchdogFeed = false;
    
    // Sıcaklık ve nem değerlerini oku
    float temp = sensors.readTemperature();
    float humid = sensors.readHumidity();

    // Sensör hata kontrolü - basitleştirilmiş versiyon
    if (temp == -999.0 || humid == -999.0) {
        static unsigned long lastErrorLog = 0;
        unsigned long currentTime = millis();
        
        // Her 10 saniyede bir hata logu yaz
        if (currentTime - lastErrorLog > 10000) {
            lastErrorLog = currentTime;
            Serial.println("KRİTİK: Sensör okuma hatası tespit edildi!");
            Serial.println("Güvenlik önlemleri alınıyor...");
        }
        
        // Güvenlik önlemleri
        if (incubation.isIncubationRunning()) {
            relays.setHeater(false);
            relays.setHumidifier(false);
        }
        
        // WiFi üzerinden hata durumunu bildir
        updateWiFiStatus();
        
        return; // Diğer güncellemeleri yapma
    }
    
    // Eğer sensör hata sayısı yüksekse watchdog beslemeyi zorla
    if (sensors.getI2CErrorCount() > 5) {
        needWatchdogFeed = true;
    }
    
    // PID kontrolü için sıcaklık değerini kullan
    pidController.compute(temp);
    
    // Histerezis kontrolü için nem değerini kullan
    hysteresisController.compute(humid);
    
    // Kuluçka durumunu güncelle
    incubation.update(rtc.getCurrentDateTime());

    // Kuluçka aşaması değiştiğinde hedef değerleri kontrol et ve güncelle
    static IncubationStage lastStage = STAGE_DEVELOPMENT;
    IncubationStage currentStage = incubation.getCurrentStage();

    if (currentStage != lastStage) {
        // Aşama değişti, hedef değerleri güncelle
        float newTargetTemp = incubation.getTargetTemperature();
        float newTargetHumid = incubation.getTargetHumidity();
        
        pidController.setSetpoint(newTargetTemp);
        hysteresisController.setSetpoint(newTargetHumid);
        
        lastStage = currentStage;
        
        Serial.println("Kuluçka aşaması değişti. Yeni hedef değerler:");
        Serial.print("Sıcaklık: ");
        Serial.println(newTargetTemp);
        Serial.print("Nem: ");
        Serial.println(newTargetHumid);
    }
    
    // RTC hata sayısı yüksekse watchdog beslemeyi zorla
    if (rtc.getRTCErrorCount() > 2) {
        needWatchdogFeed = true;
    }
    
    // WiFi durum verilerini güncelle - DÜZELTİLMİŞ
    updateWiFiStatus();
    
    // Alarmları kontrol et
    alarmManager.checkAlarms(
        temp, 
        pidController.getSetpoint(),      // DÜZELTME: Gerçek hedef sıcaklık
        humid, 
        hysteresisController.getSetpoint(), // DÜZELTME: Gerçek hedef nem
        relays.getMotorState(), 
        true,
        sensors.areSensorsWorking()
    );
    
    // Gerekirse watchdog besle
    if (needWatchdogFeed) {
        watchdogManager.feed();
    }

    // Alarm durumu değişiklik tespiti
    static bool lastAlarmEnabledState = true;
    bool currentAlarmEnabledState = alarmManager.areAlarmsEnabled();
    
    if (lastAlarmEnabledState != currentAlarmEnabledState) {
        // Alarm durumu değişti, menü durumunu güncelle
        updateMenuWithCurrentStatus();
        updateWiFiStatus(); // WiFi durumunu da güncelle
        lastAlarmEnabledState = currentAlarmEnabledState;
        
        Serial.println("Alarm durumu değişikliği tespit edildi: " + 
                      String(currentAlarmEnabledState ? "AÇIK" : "KAPALI"));
    }
    
    // Kuluçka tamamlanma durumu kontrolü
    static bool lastCompletedState = false;
    bool currentCompletedState = incubation.isIncubationCompleted();
    
    if (!lastCompletedState && currentCompletedState) {
        // Kuluçka yeni tamamlandı
        Serial.println("=== KULUÇKA SÜRESİ TAMAMLANDI ===");
        Serial.println("Sistem çıkım aşamasında çalışmaya devam ediyor...");
        Serial.println("Manuel olarak durdurmak için kuluçka durdur seçeneğini kullanın.");
        updateWiFiStatus(); // WiFi durumunu güncelle
        lastCompletedState = currentCompletedState;
    }
}

void updateDisplay() {
    // Mevcut duruma göre ekranı güncelle
    if (menuManager.isInHomeScreen()) {
        // Ana ekran güncellemesi
        display.updateMainScreen(
            sensors.readTemperature(),
            pidController.getSetpoint(),
            sensors.readHumidity(),
            hysteresisController.getSetpoint(),
            relays.getMotorWaitTimeLeft(),      // Kalan bekleme süresi (dakika)
            relays.getMotorRunTimeLeft(),       // Kalan çalışma süresi (saniye)
            incubation.getDisplayDay(rtc.getCurrentDateTime()), // DÜZELTME: getDisplayDay kullan
            incubation.getTotalDays(),
            incubation.getIncubationTypeName(),
            relays.getHeaterState(),
            relays.getHumidifierState(),
            relays.getMotorState(),
            rtc.getTimeString(),
            rtc.getDateString()
        );
    } else if (menuManager.isInMenu()) {
        // Menü ekranı güncellemesi
        display.showMenu(
            menuManager.getMenuItems().data(),
            menuManager.getMenuItems().size(),
            menuManager.getSelectedIndex()
        );
    }
    
    // PID Otomatik Ayarlama ekranı
    if (pidController.isAutoTuneEnabled()) {
        display.showProgressBar(
            20, SCREEN_HEIGHT / 2, 
            SCREEN_WIDTH - 40, 20, 
            COLOR_HIGHLIGHT, 
            pidController.getAutoTuneProgress()
        );
        
        if (pidController.isAutoTuneFinished()) {
            display.showConfirmationMessage("Otomatik Ayarlama Tamamlandi");
            pidController.setAutoTuneMode(false);
            saveSettingsToStorage();
        }
    }
    
    // KRİTİK DÜZELTME: Alarm gösterimi için çifte kontrol
    if (alarmManager.areAlarmsEnabled() && alarmManager.getCurrentAlarm() != ALARM_NONE && alarmManager.isAlarmActive()) {
        display.showAlarmMessage(
            alarmManager.getAlarmMessage(),
            "Kontrol Et!"
        );
    }
}

void updateRelays() {
    // PID çıkışına göre ısıtıcı rölesini kontrol et
    relays.setHeater(pidController.isOutputActive());
    
    // Histerezis çıkışına göre nem rölesini kontrol et
    relays.setHumidifier(hysteresisController.getOutput());
    
    // Motor rölesini güncelle
    relays.update();
    
    // Motor durumu değişiklik kontrolü
    static bool lastMotorState = false;
    bool currentMotorState = relays.getMotorState();
    
    if (lastMotorState != currentMotorState) {
        updateWiFiStatus(); // WiFi durumunu güncelle
        lastMotorState = currentMotorState;
        Serial.println("Motor durumu değişti: " + String(currentMotorState ? "AÇIK" : "KAPALI"));
    }
}

void updateAlarm() {
    // Alarm yönetimini güncelle
    alarmManager.update();
}

void checkStorageQueue() {
    
    // Bekleyen EEPROM değişikliklerini işle
    storage.processQueue();
}

// Motor test fonksiyonu
void performMotorTest() {
    // Watchdog besleme
    watchdogManager.beginOperation(OP_CUSTOM, "Motor Test");
    
    // Motor test süresi (storage'dan al)
    uint32_t testDuration = storage.getMotorRunTime();
    
    Serial.println("Motor test başladı - Süre: " + String(testDuration) + " saniye");
    
    // Motoru başlat
    relays.setMotor(true);
    
    // WiFi durumunu güncelle
    updateWiFiStatus();
    
    // Test süresi boyunca bekle
    unsigned long startTime = millis();
    unsigned long testDurationMillis = testDuration * 1000UL;
    unsigned long lastWatchdogFeed = millis();
    unsigned long lastProgressUpdate = millis();
    
    while (millis() - startTime < testDurationMillis) {
        unsigned long currentMillis = millis();
        
        // Her 500ms'de bir watchdog besle
        if (currentMillis - lastWatchdogFeed >= 500) {
            lastWatchdogFeed = currentMillis;
            watchdogManager.feed();
            
            // Kalan süreyi hesapla ve göster
            unsigned long remaining = (testDurationMillis - (currentMillis - startTime)) / 1000;
            Serial.println("Motor test - Kalan süre: " + String(remaining) + " saniye");
        }
        
        // Her 100ms'de bir progress bar güncelle
        if (currentMillis - lastProgressUpdate >= 100) {
            lastProgressUpdate = currentMillis;
            
            // Ekranı güncelle - test sırasında kalan süreyi göster
            unsigned long elapsed = currentMillis - startTime;
            int percentage = 100 - (int)((elapsed * 100) / testDurationMillis);
            display.showProgressBar(
                20, SCREEN_HEIGHT / 2, 
                SCREEN_WIDTH - 40, 20, 
                COLOR_HIGHLIGHT, 
                percentage
            );
        }
        
        delay(50); // CPU kullanımını azaltmak için kısa bekleme
    }
    
    // Motoru durdur
    relays.setMotor(false);
    
    // WiFi durumunu güncelle
    updateWiFiStatus();
    
    Serial.println("Motor test tamamlandı");
    
    // Test tamamlandı mesajı göster
    display.showConfirmationMessage("Motor Testi Tamamlandi");
    
    // Ana ekrana dön
    display.setupMainScreen();
    
    // Watchdog işlemini bitir
    watchdogManager.endOperation();
}

void handleMenuActions(JoystickDirection direction) {
    MenuState currentState = menuManager.getCurrentState();
    
    // Terminal menüler için özel işlemler - TÜM DEĞER AYARLAMA EKRANLARI
    
    // Sensör değerleri ekranı
    if (currentState == MENU_SENSOR_VALUES) {
        display.showSensorValuesScreen(
            sensors.readTemperature(0), sensors.readHumidity(0),
            sensors.readTemperature(1), sensors.readHumidity(1),
            sensors.isSensorWorking(0), sensors.isSensorWorking(1)
        );
        return;
    }
    
    // SICAKLIK AYARI
    if (currentState == MENU_TEMPERATURE) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Hedef Sicaklik", 
                pidController.getSetpoint(), 
                "C", 
                TEMP_MIN, 
                TEMP_MAX, 
                0.1
            );
        }
        return;
    }
    
    // NEM AYARI
    if (currentState == MENU_HUMIDITY) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Hedef Nem", 
                hysteresisController.getSetpoint(), 
                "%", 
                HUMID_MIN, 
                HUMID_MAX, 
                1.0
            );
        }
        return;
    }
    
    // MOTOR AYARLARI
    if (currentState == MENU_MOTOR_WAIT) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Bekleme Suresi", 
                storage.getMotorWaitTime(), 
                "dk", 
                MOTOR_WAIT_TIME_MIN, 
                MOTOR_WAIT_TIME_MAX, 
                1.0
            );
        }
        return;
    }
    
    if (currentState == MENU_MOTOR_RUN) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Calisma Suresi", 
                storage.getMotorRunTime(), 
                "sn", 
                MOTOR_RUN_TIME_MIN, 
                MOTOR_RUN_TIME_MAX, 
                1.0
            );
        }
        return;
    }

    // MOTOR TEST
    if (currentState == MENU_MOTOR_TEST) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            // Motor test işlemini başlat
            Serial.println("Motor test başlatılıyor...");
            
            // Motor test için özel bir fonksiyon çağır
            performMotorTest();
            
            // Motor menüsüne geri dön
            menuManager.setCurrentState(MENU_MOTOR);
            return;
        }
        return;
    }
    
    // SAAT VE TARİH AYARLARI
    if (currentState == MENU_SET_TIME) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            DateTime now = rtc.getCurrentDateTime();
            int timeValue = now.hour() * 100 + now.minute();
            menuManager.showTimeAdjustScreen("Saat Ayarla", timeValue);
        }
        return;
    }
    
    if (currentState == MENU_SET_DATE) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            DateTime now = rtc.getCurrentDateTime();
            long dateValue = now.day() * 1000000L + now.month() * 10000L + now.year();
            menuManager.showDateAdjustScreen("Tarih Ayarla", dateValue);
        }
        return;
    }
    
    // PID PARAMETRELERİ
    if (currentState == MENU_PID_KP) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "PID Kp", 
                pidController.getKp(), 
                "", 
                PID_KP_MIN, 
                PID_KP_MAX, 
                0.1
            );
        }
        return;
    }
    
    if (currentState == MENU_PID_KI) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "PID Ki", 
                pidController.getKi(), 
                "", 
                PID_KI_MIN, 
                PID_KI_MAX, 
                0.01
            );
        }
        return;
    }
    
    if (currentState == MENU_PID_KD) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "PID Kd", 
                pidController.getKd(), 
                "", 
                PID_KD_MIN, 
                PID_KD_MAX, 
                0.1
            );
        }
        return;
    }
    
    // KALIBRASYON AYARLARI
if (currentState == MENU_CALIBRATION_TEMP_1) {
    if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
        float currentCalibration = sensors.getTemperatureCalibration(0);
        menuManager.showValueAdjustScreen(
            "Sensor 1 Sicaklik Kal.", 
            currentCalibration, 
            "C", 
            TEMP_CALIBRATION_MIN, 
            TEMP_CALIBRATION_MAX, 
            0.1
        );
        Serial.println("Sensör 1 sıcaklık kalibrasyonu mevcut değer: " + String(currentCalibration));
    }
    return;
}

if (currentState == MENU_CALIBRATION_TEMP_2) {
    if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
        float currentCalibration = sensors.getTemperatureCalibration(1);
        menuManager.showValueAdjustScreen(
            "Sensor 2 Sicaklik Kal.", 
            currentCalibration, 
            "C", 
            TEMP_CALIBRATION_MIN, 
            TEMP_CALIBRATION_MAX, 
            0.1
        );
        Serial.println("Sensör 2 sıcaklık kalibrasyonu mevcut değer: " + String(currentCalibration));
    }
    return;
}

if (currentState == MENU_CALIBRATION_HUMID_1) {
    if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
        float currentCalibration = sensors.getHumidityCalibration(0);
        menuManager.showValueAdjustScreen(
            "Sensor 1 Nem Kal.", 
            currentCalibration, 
            "%", 
            HUMID_CALIBRATION_MIN, 
            HUMID_CALIBRATION_MAX, 
            0.5
        );
        Serial.println("Sensör 1 nem kalibrasyonu mevcut değer: " + String(currentCalibration));
    }
    return;
}

if (currentState == MENU_CALIBRATION_HUMID_2) {
    if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
        float currentCalibration = sensors.getHumidityCalibration(1);
        menuManager.showValueAdjustScreen(
            "Sensor 2 Nem Kal.", 
            currentCalibration, 
            "%", 
            HUMID_CALIBRATION_MIN, 
            HUMID_CALIBRATION_MAX, 
            0.5
        );
        Serial.println("Sensör 2 nem kalibrasyonu mevcut değer: " + String(currentCalibration));
    }
    return;
}
    
    // ALARM AYARLARI - GÜNCELLENMİŞ
    if (currentState == MENU_ALARM_ENABLE_ALL) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            alarmManager.setAlarmsEnabled(true);
            storage.setAlarmsEnabled(true);
            storage.saveStateNow(); // Kritik değişiklik, anında kaydet
            
            updateWiFiStatus();
            updateMenuWithCurrentStatus();
            
            display.showConfirmationMessage("Tum Alarmlar Acildi");
            menuManager.setCurrentState(MENU_ALARM);
            
            Serial.println("Kullanıcı tarafından tüm alarmlar açıldı");
            
            // Menüyü güncelle
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
        }
        return;
    }
    
    if (currentState == MENU_ALARM_DISABLE_ALL) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            alarmManager.setAlarmsEnabled(false);
            storage.setAlarmsEnabled(false);
            storage.saveStateNow(); // Kritik değişiklik, anında kaydet
            
            updateWiFiStatus();
            updateMenuWithCurrentStatus();
            
            display.showConfirmationMessage("Tum Alarmlar Kapatildi");
            menuManager.setCurrentState(MENU_ALARM);
            
            Serial.println("Kullanıcı tarafından tüm alarmlar kapatıldı");
            
            // Menüyü güncelle
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
        }
        return;
    }
    
    if (currentState == MENU_ALARM_HUMID_LOW) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Dusuk Nem Alarmi", 
                alarmManager.getHumidLowThreshold(), 
                "%", 
                ALARM_HUMID_MIN, 
                ALARM_HUMID_MAX, 
                1.0
            );
        }
        return;
    }
    
    if (currentState == MENU_ALARM_HUMID_HIGH) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            menuManager.showValueAdjustScreen(
                "Yuksek Nem Alarmi", 
                alarmManager.getHumidHighThreshold(), 
                "%", 
                ALARM_HUMID_MIN, 
                ALARM_HUMID_MAX, 
                1.0
            );
        }
        return;
    }
    
    // MANUEL KULUÇKA PARAMETRELERİ
    if (currentState == MENU_MANUAL_DEV_TEMP) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Gelisim Sicakligi", 
                params.developmentTemp, 
                "C", 
                TEMP_MIN, 
                TEMP_MAX, 
                0.1
            );
        }
        return;
    }
    
    if (currentState == MENU_MANUAL_HATCH_TEMP) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Cikim Sicakligi", 
                params.hatchingTemp, 
                "C", 
                TEMP_MIN, 
                TEMP_MAX, 
                0.1
            );
        }
        return;
    }
    
    if (currentState == MENU_MANUAL_DEV_HUMID) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Gelisim Nemi", 
                params.developmentHumidity, 
                "%", 
                HUMID_MIN, 
                HUMID_MAX, 
                1.0
            );
        }
        return;
    }
    
    if (currentState == MENU_MANUAL_HATCH_HUMID) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Cikim Nemi", 
                params.hatchingHumidity, 
                "%", 
                HUMID_MIN, 
                HUMID_MAX, 
                1.0
            );
        }
        return;
    }
    
    if (currentState == MENU_MANUAL_DEV_DAYS) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Gelisim Gunleri", 
                params.developmentDays, 
                "gun", 
                1, 
                60, 
                1.0
            );
        }
        return;
    }
    
    if (currentState == MENU_MANUAL_HATCH_DAYS) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            IncubationParameters params = incubation.getParameters();
            menuManager.showValueAdjustScreen(
                "Cikim Gunleri", 
                params.hatchingDays, 
                "gun", 
                1, 
                10, 
                1.0
            );
        }
        return;
    }
    
    // WiFi AYARLARI - YENİ EKLEMELER
    if (currentState == MENU_WIFI_MODE) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            // Burada doğrudan mod değiştirme işlemini yapıyoruz
            Serial.println("WiFi mod değiştirme işlemi başlatılıyor...");
            
            WiFiConnectionMode currentMode = storage.getWifiMode();
            WiFiConnectionMode newMode;
            String modeStr;
            bool success = false;
            
            Serial.println("Mevcut WiFi modu: " + String(currentMode == WIFI_CONN_MODE_AP ? "AP" : "Station"));
            
            if (currentMode == WIFI_CONN_MODE_AP) {
                newMode = WIFI_CONN_MODE_STATION;
                modeStr = "Station Modu";
                
                String stationSSID = storage.getStationSSID();
                String stationPassword = storage.getStationPassword();
                
                if (stationSSID.length() == 0) {
                    display.showConfirmationMessage("SSID Ayarlanmamis!");
                    display.showConfirmationMessage("Web Arayuzunden Ayarlayin");
                    Serial.println("Station SSID boş, işlem iptal edildi");
                    menuManager.setCurrentState(MENU_WIFI_SETTINGS);
                    return;
                }
                
                display.showConfirmationMessage("Station Moduna Geciliyor...");
                display.showConfirmationMessage("SSID: " + stationSSID);
                
                success = wifiManager.switchToStationMode();
                
            } else {
                newMode = WIFI_CONN_MODE_AP;
                modeStr = "AP Modu";
                
                display.showConfirmationMessage("AP Moduna Geciliyor...");
                
                success = wifiManager.switchToAPMode();
            }
            
            if (success) {
                storage.setWifiMode(newMode);
                storage.queueSave();
                
                String ipAddress = wifiManager.getIPAddress();
                display.showConfirmationMessage(modeStr + " Aktif");
                display.showConfirmationMessage("IP: " + ipAddress);
                
                updateWiFiStatus();
                updateMenuWithCurrentStatus();
                
                Serial.println("WiFi modu başarıyla değiştirildi: " + modeStr);
                Serial.println("Yeni IP Adresi: " + ipAddress);
                
            } else {
                display.showConfirmationMessage("Mod Degistirme Basarisiz!");
                display.showConfirmationMessage("Detaylar Serial Monitorde");
                Serial.println("WiFi mod değiştirme hatası: " + modeStr);
            }
            
            menuManager.setCurrentState(MENU_WIFI_SETTINGS);
        }
        return;
    }
    
    if (currentState == MENU_WIFI_SSID) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            display.showConfirmationMessage("SSID Ayari");
            display.showConfirmationMessage("Web Arayuzunden Yapin");
            menuManager.setCurrentState(MENU_WIFI_SETTINGS);
        }
        return;
    }
    
    if (currentState == MENU_WIFI_PASSWORD) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            display.showConfirmationMessage("Sifre Ayari");
            display.showConfirmationMessage("Web Arayuzunden Yapin");
            menuManager.setCurrentState(MENU_WIFI_SETTINGS);
        }
        return;
    }
    
    if (currentState == MENU_WIFI_CONNECT) {
        if (direction == JOYSTICK_RIGHT || direction == JOYSTICK_PRESS) {
            display.showConfirmationMessage("WiFi Baglantisi");
            display.showConfirmationMessage("Web Arayuzunden Yapin");
            menuManager.setCurrentState(MENU_WIFI_SETTINGS);
        }
        return;
    }
    
    // Buton basma ile özel işlemler
    if (direction == JOYSTICK_PRESS) {
        switch (currentState) {
            case MENU_INCUBATION_TYPE:
{
    int selectedType = menuManager.getSelectedIndex();
    
    if (selectedType == 3) {
        menuManager.setCurrentState(MENU_MANUAL_INCUBATION);
    } else if (selectedType < 3) {
        incubation.setIncubationType(selectedType);
        incubation.startIncubation(rtc.getCurrentDateTime());
        
        pidController.setSetpoint(incubation.getTargetTemperature());
        hysteresisController.setSetpoint(incubation.getTargetHumidity());
        
        pidController.setPIDMode(PID_MODE_MANUAL);
        pidController.startManualMode();
        
        storage.setIncubationType(selectedType);
        storage.setIncubationRunning(true);
        storage.setStartTime(rtc.getCurrentDateTime());
        storage.setPidMode(1);
        
        // KRİTİK: ANINDA KAYDET
        storage.saveStateNow();
        
        updateWiFiStatus();
        
        display.showConfirmationMessage("Kulucka ve PID Basladi");
        menuManager.returnToHome();
    }
}
break;

            case MENU_MANUAL_START:
                incubation.setIncubationType(INCUBATION_MANUAL);
                incubation.startIncubation(rtc.getCurrentDateTime());
                
                pidController.setSetpoint(incubation.getTargetTemperature());
                hysteresisController.setSetpoint(incubation.getTargetHumidity());
                
                pidController.setPIDMode(PID_MODE_MANUAL);
                pidController.startManualMode();
                
                storage.setIncubationRunning(true);
                storage.setStartTime(rtc.getCurrentDateTime());
                storage.setPidMode(1);
                storage.queueSave();
                
                updateWiFiStatus();
                
                display.showConfirmationMessage("Manuel Kulucka ve PID Basladi");
                menuManager.returnToHome();
                break;
                
            case MENU_PID_AUTO_TUNE:
                if (!pidController.isAutoTuneEnabled()) {
                    pidController.setPIDMode(PID_MODE_AUTO_TUNE);
                    storage.setPidMode(2);
                    storage.queueSave();
                    updateWiFiStatus();
                    display.showConfirmationMessage("Oto Ayar Basladi");
                    menuManager.setCurrentState(MENU_PID_MODE);
                    updateMenuWithCurrentStatus();
                }
                break;

            case MENU_PID_OFF:
                pidController.setPIDMode(PID_MODE_OFF);
                storage.setPidMode(0);
                storage.queueSave();
                updateWiFiStatus();
                display.showConfirmationMessage("PID Kapatildi");
                menuManager.setCurrentState(MENU_PID_MODE);
                updateMenuWithCurrentStatus();
                break;

            case MENU_PID_MANUAL_START:
                pidController.setPIDMode(PID_MODE_MANUAL);
                storage.setPidMode(1);
                storage.queueSave();
                updateWiFiStatus();
                display.showConfirmationMessage("Manuel PID Basladi");
                menuManager.setCurrentState(MENU_PID_MODE);
                updateMenuWithCurrentStatus();
                break;
                
        }
    }
}

void handleValueAdjustment(JoystickDirection direction) {
    if (direction == JOYSTICK_PRESS) {
        MenuState currentState = menuManager.getCurrentState();
        
        // Saat ayarlama işlemi
        if (currentState == MENU_SET_TIME) {
            int timeValue = menuManager.getAdjustedTimeValue();
            int hour = timeValue / 100;
            int minute = timeValue % 100;
            
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
                DateTime now = rtc.getCurrentDateTime();
                rtc.setDateTime(hour, minute, now.day(), now.month(), now.year());
                display.showConfirmationMessage("Saat Kaydedildi");
                updateWiFiStatus();
                Serial.println("Saat kaydedildi: " + String(hour) + ":" + String(minute));
            } else {
                display.showConfirmationMessage("Gecersiz Saat!");
                Serial.println("Geçersiz saat değeri: " + String(hour) + ":" + String(minute));
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
            return;
        }
        
        // Tarih ayarlama işlemi
        if (currentState == MENU_SET_DATE) {
            long dateValue = menuManager.getAdjustedDateValue();
            int day = (int)(dateValue / 1000000);
            int month = (int)((dateValue / 10000) % 100);
            int year = (int)(dateValue % 10000);
            
            if (day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 2025 && year <= 2050) {
                DateTime now = rtc.getCurrentDateTime();
                rtc.setDateTime(now.hour(), now.minute(), day, month, year);
                display.showConfirmationMessage("Tarih Kaydedildi");
                updateWiFiStatus();
                Serial.println("Tarih kaydedildi: " + String(day) + "/" + String(month) + "/" + String(year));
            } else {
                display.showConfirmationMessage("Gecersiz Tarih!");
                Serial.println("Geçersiz tarih değeri: " + String(day) + "/" + String(month) + "/" + String(year));
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
            std::vector<String> items = menuManager.getMenuItems();
            if (!items.empty()) {
                display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            }
            return;
        }
        
        // Normal değer ayarlama işlemleri
        MenuState prevState = menuManager.getPreviousState();
        float value = menuManager.getAdjustedValue();
        
        Serial.println("Değer ayarlama - Önceki menü: " + String(prevState) + " Değer: " + String(value));
        
        switch (prevState) {
            case MENU_TEMPERATURE:
                pidController.setSetpoint(value);
                storage.setTargetTemperature(value);
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetTemperature(value);
                }
                Serial.println("Hedef sıcaklık güncellendi: " + String(value));
                break;
                
            case MENU_HUMIDITY:
                hysteresisController.setSetpoint(value);
                storage.setTargetHumidity((uint8_t)value);
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetHumidity((uint8_t)value);
                }
                Serial.println("Hedef nem güncellendi: " + String(value));
                break;
                
            case MENU_PID_KP:
                pidController.setTunings(value, pidController.getKi(), pidController.getKd());
                storage.setPidKp(value);
                Serial.println("PID Kp güncellendi: " + String(value));
                break;
                
            case MENU_PID_KI:
                pidController.setTunings(pidController.getKp(), value, pidController.getKd());
                storage.setPidKi(value);
                Serial.println("PID Ki güncellendi: " + String(value));
                break;
                
            case MENU_PID_KD:
                pidController.setTunings(pidController.getKp(), pidController.getKi(), value);
                storage.setPidKd(value);
                Serial.println("PID Kd güncellendi: " + String(value));
                break;
                
            case MENU_MOTOR_WAIT:
                relays.updateMotorTiming(millis(), (uint32_t)value, storage.getMotorRunTime());
                storage.setMotorWaitTime((uint32_t)value);
                Serial.println("Motor bekleme süresi güncellendi: " + String(value));
                break;
                
            case MENU_MOTOR_RUN:
                relays.updateMotorTiming(millis(), storage.getMotorWaitTime(), (uint32_t)value);
                storage.setMotorRunTime((uint32_t)value);
                Serial.println("Motor çalışma süresi güncellendi: " + String(value));
                break;
                
            case MENU_CALIBRATION_TEMP_1:
                Serial.println("KALIBRASYON: Sensör 1 sıcaklık - Eski değer: " + String(sensors.getTemperatureCalibration(0)) + " Yeni değer: " + String(value));
                sensors.setTemperatureCalibrationSingle(0, value);
                storage.setTempCalibration(0, value);
                Serial.println("KALIBRASYON SONRASI: Sensör 1: " + String(sensors.getTemperatureCalibration(0)) + " Sensör 2: " + String(sensors.getTemperatureCalibration(1)));
                break;
                
            case MENU_CALIBRATION_TEMP_2:
                Serial.println("KALIBRASYON: Sensör 2 sıcaklık - Eski değer: " + String(sensors.getTemperatureCalibration(1)) + " Yeni değer: " + String(value));
                sensors.setTemperatureCalibrationSingle(1, value);
                storage.setTempCalibration(1, value);
                Serial.println("KALIBRASYON SONRASI: Sensör 1: " + String(sensors.getTemperatureCalibration(0)) + " Sensör 2: " + String(sensors.getTemperatureCalibration(1)));
                break;
                
            case MENU_CALIBRATION_HUMID_1:
                Serial.println("KALIBRASYON: Sensör 1 nem - Eski değer: " + String(sensors.getHumidityCalibration(0)) + " Yeni değer: " + String(value));
                sensors.setHumidityCalibrationSingle(0, value);
                storage.setHumidCalibration(0, value);
                Serial.println("KALIBRASYON SONRASI: Sensör 1: " + String(sensors.getHumidityCalibration(0)) + " Sensör 2: " + String(sensors.getHumidityCalibration(1)));
                break;
                
            case MENU_CALIBRATION_HUMID_2:
                Serial.println("KALIBRASYON: Sensör 2 nem - Eski değer: " + String(sensors.getHumidityCalibration(1)) + " Yeni değer: " + String(value));
                sensors.setHumidityCalibrationSingle(1, value);
                storage.setHumidCalibration(1, value);
                Serial.println("KALIBRASYON SONRASI: Sensör 1: " + String(sensors.getHumidityCalibration(0)) + " Sensör 2: " + String(sensors.getHumidityCalibration(1)));
                break;
                
            case MENU_ALARM_TEMP_LOW:
                alarmManager.setTempLowThreshold(value);
                storage.setTempLowAlarm(value);
                break;
                
            case MENU_ALARM_TEMP_HIGH:
                alarmManager.setTempHighThreshold(value);
                storage.setTempHighAlarm(value);
                break;
                
            case MENU_ALARM_HUMID_LOW:
                alarmManager.setHumidLowThreshold(value);
                storage.setHumidLowAlarm(value);
                break;
                
            case MENU_ALARM_HUMID_HIGH:
                alarmManager.setHumidHighThreshold(value);
                storage.setHumidHighAlarm(value);
                break;
                
            case MENU_MANUAL_DEV_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(value, params.hatchingTemp, params.developmentHumidity, 
                                                 params.hatchingHumidity, params.developmentDays, params.hatchingDays);
                    storage.setManualDevTemp(value);
                }
                break;
                
            case MENU_MANUAL_HATCH_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(params.developmentTemp, value, params.developmentHumidity, 
                                                 params.hatchingHumidity, params.developmentDays, params.hatchingDays);
                    storage.setManualHatchTemp(value);
                }
                break;
                
            case MENU_MANUAL_DEV_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, (uint8_t)value, 
                                                 params.hatchingHumidity, params.developmentDays, params.hatchingDays);
                    storage.setManualDevHumid((uint8_t)value);
                }
                break;
                
            case MENU_MANUAL_HATCH_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                                 (uint8_t)value, params.developmentDays, params.hatchingDays);
                    storage.setManualHatchHumid((uint8_t)value);
                }
                break;
                
            case MENU_MANUAL_DEV_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                                 params.hatchingHumidity, (uint8_t)value, params.hatchingDays);
                    storage.setManualDevDays((uint8_t)value);
                }
                break;
                
            case MENU_MANUAL_HATCH_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                                 params.hatchingHumidity, params.developmentDays, (uint8_t)value);
                    storage.setManualHatchDays((uint8_t)value);
                }
                break;
                
            default:
                Serial.println("Bilinmeyen önceki menü durumu: " + String(prevState));
                break;
        }
        
        // KRİTİK: TÜM DEĞİŞİKLİKLER ANINDA KAYDEDİLECEK
        storage.saveStateNow();
        Serial.println("!!! DEĞER DEĞİŞİKLİĞİ ANINDA KAYDEDİLDİ !!!");
        
        updateWiFiStatus();
        display.showConfirmationMessage("Kaydedildi");
        
        // Bir üst menüye dönüş
        MenuState targetState = menuManager.getBackState(prevState);
        menuManager.setCurrentState(targetState);
        
        // Menü ekranını göster
        std::vector<String> items = menuManager.getMenuItems();
        if (!items.empty()) {
            display.showMenu(items.data(), items.size(), menuManager.getSelectedIndex());
            Serial.println("Menü geçişi yapıldı - Hedef: " + String(targetState));
        } else {
            Serial.println("HATA: Menü öğeleri boş - Hedef: " + String(targetState));
        }
    }
}

void handlePIDAutoTune() {
    // PID Otomatik Ayarlama durumunu kontrol et ve güvenlik önlemlerini uygula
    if (pidController.isAutoTuneEnabled()) {
        
        // Otomatik ayarlama tamamlandı mı?
        if (pidController.isAutoTuneFinished()) {
            // Otomatik ayarlama tamamlandı, değerleri kaydet
            storage.setPidMode(1);  // Otomatik ayarlama sonrası manuel moda geç
            storage.queueSave();
            updateMenuWithCurrentStatus();
            updateWiFiStatus(); // WiFi durumunu güncelle
        }
        
        // PID otomatik ayarlama sırasında güvenlik önlemleri
        float currentTemp = sensors.readTemperature();
        
        // Sensörler çalışmıyorsa otomatik ayarlamayı iptal et
        if (!sensors.areSensorsWorking()) {
            Serial.println("Otomatik Ayarlama: Sensör hatası nedeniyle iptal edildi!");
            pidController.setAutoTuneMode(false);
            display.showConfirmationMessage("Oto Ayar Iptal: Sensor Hatasi");
            updateMenuWithCurrentStatus();
            updateWiFiStatus(); // WiFi durumunu güncelle
        }
    }
}

void loadSettingsFromStorage() {
    // Uzun işlem başlat
    watchdogManager.beginOperation(OP_STORAGE_READ, "Ayarları Yükleme");
    
    Serial.println("=== AYARLAR YÜKLENİYOR ===");
    
    // Kuluçka ayarlarını yükle
    incubation.setIncubationType(storage.getIncubationType());
    Serial.println("Kuluçka tipi: " + String(storage.getIncubationType()));
    
    // Manüel kuluçka parametrelerini ayarla
    incubation.setManualParameters(
        storage.getManualDevTemp(),
        storage.getManualHatchTemp(),
        storage.getManualDevHumid(),
        storage.getManualHatchHumid(),
        storage.getManualDevDays(),
        storage.getManualHatchDays()
    );
    
    watchdogManager.feed(); // Ara besleme
    
    // PID parametrelerini ayarla
    pidController.setTunings(
        storage.getPidKp(),
        storage.getPidKi(),
        storage.getPidKd()
    );
    Serial.println("PID parametreleri yüklendi - Kp:" + String(storage.getPidKp()) + 
                   " Ki:" + String(storage.getPidKi()) + " Kd:" + String(storage.getPidKd()));
    
    // DÜZELTME: Storage'dan PID modunu oku ve uygula
    uint8_t savedPidMode = storage.getPidMode();
    Serial.println("Kaydedilmiş PID modu: " + String(savedPidMode));
    
    // Motor ayarlarını yükle ve relays modülünü güncelle
    relays.updateMotorTiming(
        millis(),
        storage.getMotorWaitTime(),
        storage.getMotorRunTime()
    );
    Serial.println("Motor ayarları - Bekleme: " + String(storage.getMotorWaitTime()) + 
                   " dk, Çalışma: " + String(storage.getMotorRunTime()) + " sn");
    
    // KRITIK: Motor zamanlama durumunu storage'dan yükle
    relays.loadMotorTimingFromStorage(&storage);
    Serial.println("Motor zamanlama durumu yüklendi");
    
    watchdogManager.feed(); // Ara besleme
    
    // Kuluçka devam ediyor mu kontrolü
    if (storage.isIncubationRunning()) {
        DateTime startTime = storage.getStartTime();
        incubation.startIncubation(startTime);
        
        Serial.println("Kuluçka devam ediyor - Başlangıç: " + 
                      String(startTime.day()) + "/" + String(startTime.month()) + "/" + 
                      String(startTime.year()) + " " + String(startTime.hour()) + ":" + 
                      String(startTime.minute()));
        
        // DÜZELTME: Kuluçka devam ediyorsa ve kaydedilmiş PID modu varsa onu kullan
        if (savedPidMode != 0) {
            pidController.setPIDMode((PIDMode)savedPidMode);
            Serial.println("PID modu storage'dan yüklendi: " + String(savedPidMode));
        } else {
            // Eğer PID modu kaydedilmemişse manuel modda başlat
            pidController.setPIDMode(PID_MODE_MANUAL);
            pidController.startManualMode();
            storage.setPidMode(1);  // Manuel modu kaydet
            Serial.println("Kuluçka devam ediyor, PID manuel modda başlatıldı");
        }
        
        // Hedef değerleri kuluçka parametrelerinden al
        float targetTemp = incubation.getTargetTemperature();
        float targetHumid = incubation.getTargetHumidity();
        pidController.setSetpoint(targetTemp);
        hysteresisController.setSetpoint(targetHumid);
        
        Serial.println("Hedef değerler ayarlandı - Sıcaklık: " + String(targetTemp) + 
                      "°C, Nem: " + String(targetHumid) + "%");
    } else {
        // Kuluçka çalışmıyorsa storage'daki PID modunu kullan
        pidController.setPIDMode((PIDMode)savedPidMode);
        Serial.println("PID modu: " + String(savedPidMode == 0 ? "Kapalı" : 
                                            savedPidMode == 1 ? "Manuel" : "Otomatik"));
        
        // DÜZELTME: Kuluçka çalışmıyorsa da hedef değerleri storage'dan yükle
        if (storage.getTargetTemperature() > 0 && storage.getTargetHumidity() > 0) {
            // Storage'da kaydedilmiş hedef değerler varsa onları kullan
            float savedTargetTemp = storage.getTargetTemperature();
            float savedTargetHumid = storage.getTargetHumidity();
            
            pidController.setSetpoint(savedTargetTemp);
            hysteresisController.setSetpoint(savedTargetHumid);
            
            Serial.println("Kaydedilmiş hedef değerler yüklendi - Sıcaklık: " + 
                          String(savedTargetTemp) + "°C, Nem: " + String(savedTargetHumid) + "%");
        } else {
            // Hedef değerler yoksa varsayılan değerleri kullan
            pidController.setSetpoint(37.5);
            hysteresisController.setSetpoint(60);
            
            // Ve bu değerleri storage'a kaydet
            storage.setTargetTemperature(37.5);
            storage.setTargetHumidity(60);
            
            Serial.println("Varsayılan hedef değerler ayarlandı - Sıcaklık: 37.5°C, Nem: 60%");
        }
    }
    
    // Sensör kalibrasyonlarını ayarla - DÜZELTİLMİŞ
    sensors.setTemperatureCalibrationSingle(0, storage.getTempCalibration(0));
    sensors.setTemperatureCalibrationSingle(1, storage.getTempCalibration(1));
    sensors.setHumidityCalibrationSingle(0, storage.getHumidCalibration(0));
    sensors.setHumidityCalibrationSingle(1, storage.getHumidCalibration(1));
    Serial.println("Sensör kalibrasyonları yüklendi");
    
    // Alarm eşiklerini ayarla
    alarmManager.setTempLowThreshold(storage.getTempLowAlarm());
    alarmManager.setTempHighThreshold(storage.getTempHighAlarm());
    alarmManager.setHumidLowThreshold(storage.getHumidLowAlarm());
    alarmManager.setHumidHighThreshold(storage.getHumidHighAlarm());
    
    // Alarm durumunu ayarla
    alarmManager.setAlarmsEnabled(storage.areAlarmsEnabled());
    Serial.println("Alarm durumu: " + String(storage.areAlarmsEnabled() ? "AÇIK" : "KAPALI"));
    
    // WiFi ayarlarını kontrol et ve uygula - DÜZELTME BURADA
    WiFiConnectionMode wifiMode = storage.getWifiMode();
    String stationSSID = storage.getStationSSID();
    String stationPassword = storage.getStationPassword();
    
    Serial.println("=== WiFi AYARLARI YÜKLENİYOR ===");
    Serial.println("Kaydedilmiş WiFi modu: " + String(wifiMode == WIFI_CONN_MODE_AP ? "AP" : "Station"));
    Serial.println("Station SSID: " + stationSSID);
    Serial.println("Station şifre uzunluğu: " + String(stationPassword.length()));
    
    // WiFi Manager'a credential'ları ayarla
    if (stationSSID.length() > 0) {
        wifiManager.setStationCredentials(stationSSID, stationPassword);
        Serial.println("WiFi Manager'a station credential'ları ayarlandı");
    }
    
    // WiFi modunu WiFi Manager'a bildir - ÖNEMLİ: begin() çağrılmadan önce yapılmalı
    if (wifiMode == WIFI_CONN_MODE_STATION && stationSSID.length() == 0) {
        Serial.println("Station modu seçili ancak SSID kaydedilmemiş, AP moduna geçiliyor");
        storage.setWifiMode(WIFI_CONN_MODE_AP);
        storage.saveStateNow();
    }
    
    Serial.println("=== AYARLAR YÜKLEMESİ TAMAMLANDI ===");
    
    watchdogManager.endOperation(); // İşlem tamamlandı
}

void saveSettingsToStorage() {
    // Uzun işlem başlat
    watchdogManager.beginOperation(OP_STORAGE_WRITE, "Ayarları Kaydetme");
    
    // Ayarları saklama modülüne kaydet
    
    // Kuluçka türü
    storage.setIncubationType(incubation.getIncubationType());
    
    // Manuel kuluçka parametreleri
    IncubationParameters params = incubation.getParameters();
    if (incubation.getIncubationType() == INCUBATION_MANUAL) {
        storage.setManualDevTemp(params.developmentTemp);
        storage.setManualHatchTemp(params.hatchingTemp);
        storage.setManualDevHumid(params.developmentHumidity);
        storage.setManualHatchHumid(params.hatchingHumidity);
        storage.setManualDevDays(params.developmentDays);
        storage.setManualHatchDays(params.hatchingDays);
    }
    
    watchdogManager.feed(); // Ara besleme
    
    // PID parametreleri
    storage.setPidKp(pidController.getKp());
    storage.setPidKi(pidController.getKi());
    storage.setPidKd(pidController.getKd());
    storage.setPidMode((uint8_t)pidController.getPIDMode());
    
    // Kuluçka durumu
    storage.setIncubationRunning(incubation.isIncubationRunning());
    if (incubation.isIncubationRunning()) {
        storage.setStartTime(incubation.getStartTime());
    }
    
    // Motor zamanlama durumunu da kaydet
    relays.saveMotorTimingToStorage(&storage);
    
    // Alarm durumunu kaydet
    storage.setAlarmsEnabled(alarmManager.areAlarmsEnabled());
    
    // WiFi ayarlarını kaydet
    storage.setWifiMode(wifiManager.getCurrentMode() == WIFI_AP ? WIFI_CONN_MODE_AP : WIFI_CONN_MODE_STATION);
    
    // Ayarları anında EEPROM'a kaydet (kritik durumlarda)
    storage.saveStateNow();
    
    watchdogManager.endOperation(); // İşlem tamamlandı
}

void handleWifiParameterUpdate(String param, String value) {
    // KRİTİK: Tüm parametre güncellemeleri kritik olarak değerlendirilecek
    bool criticalUpdate = true; // VARSAYILAN OLARAK KRİTİK
    
    // Sıcaklık ve nem hedef değerleri
    if (param == "targetTemp") {
        float temp = value.toFloat();
        if (temp >= 20.0 && temp <= 40.0) {
            pidController.setSetpoint(temp);
            storage.setTargetTemperature(temp); // YENİ EKLENEN
            if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                incubation.setTargetTemperature(temp);
            }
            updateWiFiStatus();
            Serial.println("Hedef sıcaklık güncellendi ve kaydedilecek: " + String(temp));
        }
    } else if (param == "targetHumid") {
        float humid = value.toFloat();
        if (humid >= 30.0 && humid <= 90.0) {
            hysteresisController.setSetpoint(humid);
            storage.setTargetHumidity(humid); // YENİ EKLENEN
            if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                incubation.setTargetHumidity((uint8_t)humid);
            }
            updateWiFiStatus();
            Serial.println("Hedef nem güncellendi ve kaydedilecek: " + String(humid));
        }
    }
    // PID parametreleri
    else if (param == "pidKp") {
        float kp = value.toFloat();
        if (kp >= 0.1 && kp <= 100.0) {
            pidController.setTunings(kp, pidController.getKi(), pidController.getKd());
            storage.setPidKp(kp);
            updateWiFiStatus();
            Serial.println("PID Kp güncellendi ve kaydedilecek: " + String(kp));
        }
    } else if (param == "pidKi") {
        float ki = value.toFloat();
        if (ki >= 0.01 && ki <= 10.0) {
            pidController.setTunings(pidController.getKp(), ki, pidController.getKd());
            storage.setPidKi(ki);
            updateWiFiStatus();
            Serial.println("PID Ki güncellendi ve kaydedilecek: " + String(ki));
        }
    } else if (param == "pidKd") {
        float kd = value.toFloat();
        if (kd >= 0.1 && kd <= 100.0) {
            pidController.setTunings(pidController.getKp(), pidController.getKi(), kd);
            storage.setPidKd(kd);
            updateWiFiStatus();
            Serial.println("PID Kd güncellendi ve kaydedilecek: " + String(kd));
        }
    }
    // PID modu değişikliği
    else if (param == "pidMode") {
        int mode = value.toInt();
        if (mode >= 0 && mode <= 2) {
            PIDMode newMode = (PIDMode)mode;
            pidController.setPIDMode(newMode);
            storage.setPidMode(mode);
            updateWiFiStatus();
            updateMenuWithCurrentStatus();
            Serial.println("PID modu güncellendi ve kaydedilecek: " + String(mode));
        }
    }
    // Motor ayarları
    else if (param == "motorWaitTime") {
    uint32_t waitTime = value.toInt();
    if (waitTime >= 1 && waitTime <= 1440) { // 240'dan 1440'a değiştirildi
        relays.updateMotorTiming(millis(), waitTime, storage.getMotorRunTime());
        storage.setMotorWaitTime(waitTime);
        updateWiFiStatus();
        Serial.println("Motor bekleme süresi güncellendi ve kaydedilecek: " + String(waitTime));
    } else {
        Serial.println("Geçersiz motor bekleme süresi: " + String(waitTime) + " (1-1440 aralığında olmalı)");
    }
} else if (param == "motorRunTime") {
    uint32_t runTime = value.toInt();
    if (runTime >= 1 && runTime <= 300) { // 60'dan 300'e değiştirildi
        relays.updateMotorTiming(millis(), storage.getMotorWaitTime(), runTime);
        storage.setMotorRunTime(runTime);
        updateWiFiStatus();
        Serial.println("Motor çalışma süresi güncellendi ve kaydedilecek: " + String(runTime));
    } else {
        Serial.println("Geçersiz motor çalışma süresi: " + String(runTime) + " (1-300 aralığında olmalı)");
    }
}
    // Motor test komutu
else if (param == "motorTest") {
    uint32_t testDuration = value.toInt();
    if (testDuration > 0 && testDuration <= 60) {
        Serial.println("WiFi API: Motor test isteği alındı - Süre: " + String(testDuration) + " saniye");
        
        // Motor test global değişkenlerini ayarla
        motorTestRequested = true;
        requestedTestDuration = testDuration;
        
        Serial.println("Motor test kuyruğa alındı");
        return; // Kayıt işlemini atla
    }
}
    // Alarm ayarları
    else if (param == "tempLowAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 0.1 && alarm <= 5.0) {
            alarmManager.setTempLowThreshold(alarm);
            storage.setTempLowAlarm(alarm);
            updateWiFiStatus();
            Serial.println("Düşük sıcaklık alarmı güncellendi ve kaydedilecek: " + String(alarm));
        }
    } else if (param == "tempHighAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 0.1 && alarm <= 5.0) {
            alarmManager.setTempHighThreshold(alarm);
            storage.setTempHighAlarm(alarm);
            updateWiFiStatus();
            Serial.println("Yüksek sıcaklık alarmı güncellendi ve kaydedilecek: " + String(alarm));
        }
    } else if (param == "humidLowAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 1 && alarm <= 20) {
            alarmManager.setHumidLowThreshold(alarm);
            storage.setHumidLowAlarm(alarm);
            updateWiFiStatus();
            Serial.println("Düşük nem alarmı güncellendi ve kaydedilecek: " + String(alarm));
        }
    } else if (param == "humidHighAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 1 && alarm <= 20) {
            alarmManager.setHumidHighThreshold(alarm);
            storage.setHumidHighAlarm(alarm);
            updateWiFiStatus();
            Serial.println("Yüksek nem alarmı güncellendi ve kaydedilecek: " + String(alarm));
        }
    } else if (param == "alarmEnabled") {
        bool enabled = (value == "1" || value == "true");
        alarmManager.setAlarmsEnabled(enabled);
        storage.setAlarmsEnabled(enabled);
        updateWiFiStatus();
        updateMenuWithCurrentStatus();
        Serial.println("Alarm durumu güncellendi ve kaydedilecek: " + String(enabled ? "AÇIK" : "KAPALI"));
    }
    // Kalibrasyon ayarları
    else if (param == "tempCalibration1") {
        float cal = value.toFloat();
        if (cal >= -10.0 && cal <= 10.0) {
            sensors.setTemperatureCalibrationSingle(0, cal);
            storage.setTempCalibration(0, cal);
            updateWiFiStatus();
            Serial.println("Sensör 1 sıcaklık kalibrasyonu güncellendi ve kaydedilecek: " + String(cal));
        }
    } else if (param == "tempCalibration2") {
        float cal = value.toFloat();
        if (cal >= -10.0 && cal <= 10.0) {
            sensors.setTemperatureCalibrationSingle(1, cal);
            storage.setTempCalibration(1, cal);
            updateWiFiStatus();
            Serial.println("Sensör 2 sıcaklık kalibrasyonu güncellendi ve kaydedilecek: " + String(cal));
        }
    } else if (param == "humidCalibration1") {
        float cal = value.toFloat();
        if (cal >= -20.0 && cal <= 20.0) {
            sensors.setHumidityCalibrationSingle(0, cal);
            storage.setHumidCalibration(0, cal);
            updateWiFiStatus();
            Serial.println("Sensör 1 nem kalibrasyonu güncellendi ve kaydedilecek: " + String(cal));
        }
    } else if (param == "humidCalibration2") {
        float cal = value.toFloat();
        if (cal >= -20.0 && cal <= 20.0) {
            sensors.setHumidityCalibrationSingle(1, cal);
            storage.setHumidCalibration(1, cal);
            updateWiFiStatus();
            Serial.println("Sensör 2 nem kalibrasyonu güncellendi ve kaydedilecek: " + String(cal));
        }
    }
    // Kuluçka ayarları
    else if (param == "incubationType") {
        uint8_t type = value.toInt();
        if (type <= INCUBATION_MANUAL) {
            incubation.setIncubationType(type);
            storage.setIncubationType(type);
            
            float newTargetTemp = incubation.getTargetTemperature();
            float newTargetHumid = incubation.getTargetHumidity();
            
            pidController.setSetpoint(newTargetTemp);
            hysteresisController.setSetpoint(newTargetHumid);
            updateWiFiStatus();
            Serial.println("Kuluçka tipi güncellendi ve kaydedilecek: " + String(type));
        }
    } else if (param == "isIncubationRunning") {
        bool running = (value == "1");
        if (running && !incubation.isIncubationRunning()) {
            incubation.startIncubation(rtc.getCurrentDateTime());
            storage.setIncubationRunning(true);
            storage.setStartTime(rtc.getCurrentDateTime());
            
            pidController.setPIDMode(PID_MODE_MANUAL);
            pidController.startManualMode();
            storage.setPidMode(1);
            updateWiFiStatus();
            updateMenuWithCurrentStatus();
            Serial.println("Kuluçka başlatıldı ve kaydedilecek");
        } else if (!running && incubation.isIncubationRunning()) {
            incubation.stopIncubation();
            storage.setIncubationRunning(false);
            updateWiFiStatus();
            Serial.println("Kuluçka durduruldu ve kaydedilecek");
        }
    }
    // Manuel kuluçka parametreleri
    else if (param == "manualDevTemp") {
        float temp = value.toFloat();
        if (temp >= 20.0 && temp <= 40.0) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(temp, params.hatchingTemp, params.developmentHumidity, 
                                         params.hatchingHumidity, params.developmentDays, params.hatchingDays);
            storage.setManualDevTemp(temp);
            
            if (incubation.getIncubationType() == INCUBATION_MANUAL && incubation.getCurrentStage() == STAGE_DEVELOPMENT) {
                pidController.setSetpoint(temp);
            }
            updateWiFiStatus();
            Serial.println("Manuel gelişim sıcaklığı güncellendi ve kaydedilecek: " + String(temp));
        }
    } else if (param == "manualHatchTemp") {
        float temp = value.toFloat();
        if (temp >= 20.0 && temp <= 40.0) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, temp, params.developmentHumidity, 
                                         params.hatchingHumidity, params.developmentDays, params.hatchingDays);
            storage.setManualHatchTemp(temp);
            
            if (incubation.getIncubationType() == INCUBATION_MANUAL && incubation.getCurrentStage() == STAGE_HATCHING) {
                pidController.setSetpoint(temp);
            }
            updateWiFiStatus();
            Serial.println("Manuel çıkım sıcaklığı güncellendi ve kaydedilecek: " + String(temp));
        }
    } else if (param == "manualDevHumid") {
        uint8_t humid = value.toInt();
        if (humid >= 30 && humid <= 90) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, humid, 
                                         params.hatchingHumidity, params.developmentDays, params.hatchingDays);
            storage.setManualDevHumid(humid);
            
            if (incubation.getIncubationType() == INCUBATION_MANUAL && incubation.getCurrentStage() == STAGE_DEVELOPMENT) {
                hysteresisController.setSetpoint(humid);
            }
            updateWiFiStatus();
            Serial.println("Manuel gelişim nemi güncellendi ve kaydedilecek: " + String(humid));
        }
    } else if (param == "manualHatchHumid") {
        uint8_t humid = value.toInt();
        if (humid >= 30 && humid <= 90) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                         humid, params.developmentDays, params.hatchingDays);
            storage.setManualHatchHumid(humid);
            
            if (incubation.getIncubationType() == INCUBATION_MANUAL && incubation.getCurrentStage() == STAGE_HATCHING) {
                hysteresisController.setSetpoint(humid);
            }
            updateWiFiStatus();
            Serial.println("Manuel çıkım nemi güncellendi ve kaydedilecek: " + String(humid));
        }
    } else if (param == "manualDevDays") {
        uint8_t days = value.toInt();
        if (days >= 1 && days <= 60) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                         params.hatchingHumidity, days, params.hatchingDays);
            storage.setManualDevDays(days);
            updateWiFiStatus();
            Serial.println("Manuel gelişim günleri güncellendi ve kaydedilecek: " + String(days));
        }
    } else if (param == "manualHatchDays") {
        uint8_t days = value.toInt();
        if (days >= 1 && days <= 10) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                         params.hatchingHumidity, params.developmentDays, days);
            storage.setManualHatchDays(days);
            updateWiFiStatus();
            Serial.println("Manuel çıkım günleri güncellendi ve kaydedilecek: " + String(days));
        }
    }
    // WiFi ayarları
    else if (param == "wifiStationSSID") {
        storage.setStationSSID(value);
        updateWiFiStatus();
        Serial.println("Station SSID güncellendi ve kaydedilecek: " + value);
    } else if (param == "wifiStationPassword") {
        storage.setStationPassword(value);
        updateWiFiStatus();
        Serial.println("Station şifresi güncellendi ve kaydedilecek");
    } else if (param == "wifiMode") {
        WiFiConnectionMode mode = (value == "1") ? WIFI_CONN_MODE_STATION : WIFI_CONN_MODE_AP;
        storage.setWifiMode(mode);
        updateWiFiStatus();
        Serial.println("WiFi modu güncellendi ve kaydedilecek: " + String(mode));
    }
    
    // KRİTİK: TÜM PARAMETRE DEĞİŞİKLİKLERİ ANINDA KAYDEDİLECEK
    if (criticalUpdate) {
        storage.saveStateNow();
        Serial.println("!!! PARAMETRE DEĞİŞİKLİĞİ ANINDA KAYDEDİLDİ !!!");
    }
}
