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

void updateWiFiStatus() {
    DateTime currentDateTime = rtc.getCurrentDateTime();
    
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
        incubation.getCurrentDay(currentDateTime)
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
    
    Serial.println("KULUCKA MK v5.0 Hazir!");
    watchdogManager.endOperation(); // Sistem başlatma işlemi tamamlandı
}

void loop() {
    // Mevcut zaman
    unsigned long currentMillis = millis();
    
    // Düzenli watchdog beslemesi - İYİLEŞTİRİLMİŞ
    watchdogManager.feed();
    
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
    
    // WiFi isteklerini işle - İYİLEŞTİRİLMİŞ
    wifiManager.handleRequests();
    
    // PID Otomatik Ayarlama durumunu kontrol et - İYİLEŞTİRİLMİŞ
    if (pidController.isAutoTuneEnabled()) {
        watchdogManager.beginOperation(OP_PID_AUTOTUNE, "PID Otomatik Ayarlama");
        handlePIDAutoTune();
        watchdogManager.endOperation();
    }
}


void initializeModules() {
    // Her modülü başlat ve hata kontrolünü yap
    
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
    
    // Saklama yönetimi
    watchdogManager.beginOperation(OP_STORAGE_WRITE, "Storage Başlatma");
    if (!storage.begin()) {
        Serial.println("Saklama yönetimi başlatma hatası!");
    }
    watchdogManager.endOperation();
    
    // WiFi yönetimi - uzun süren işlem
    watchdogManager.beginOperation(OP_WIFI_CONNECT, "WiFi Başlatma");

    // Sabit IP kullanmak isterseniz (opsiyonel):
    // wifiManager.setStaticIP(true, 
    //     IPAddress(192, 168, 1, 100),  // IP
    //     IPAddress(192, 168, 1, 1),    // Gateway
    //     IPAddress(255, 255, 255, 0),  // Subnet
    //     IPAddress(8, 8, 8, 8)         // DNS
    // );

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
    
    if (direction != JOYSTICK_NONE) {
        Serial.print("Joystick: ");
        Serial.println(direction);
        
        menuManager.updateInteractionTime();
        MenuState currentState = menuManager.getCurrentState();
        
        // Ana ekranda sadece sağ yön menüye giriş yapabilir
        if (currentState == MENU_NONE) {
            if (direction == JOYSTICK_RIGHT) {
                menuManager.setCurrentState(MENU_MAIN);
                
                // Menü durumunu güncelle
                updateMenuWithCurrentStatus();
                
                // Ana menü ekranını göster
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
        
        // Menü navigasyonu
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
}

void updateSensors() {
    // Sensör verilerini okumadan önce watchdog beslemesi
    bool needWatchdogFeed = false;
    
    // Sıcaklık ve nem değerlerini oku
    float temp = sensors.readTemperature();
    float humid = sensors.readHumidity();
    
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

void handleMenuActions(JoystickDirection direction) {
    MenuState currentState = menuManager.getCurrentState();
    
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
                    storage.queueSave();
                    
                    updateWiFiStatus(); // WiFi durumunu güncelle
                    
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
                
                updateWiFiStatus(); // WiFi durumunu güncelle
                
                display.showConfirmationMessage("Manuel Kulucka ve PID Basladi");
                menuManager.returnToHome();
                break;
                
            case MENU_PID_AUTO_TUNE:
                if (!pidController.isAutoTuneEnabled()) {
                    pidController.setPIDMode(PID_MODE_AUTO_TUNE);
                    storage.setPidMode(2);
                    storage.queueSave();
                    updateWiFiStatus(); // WiFi durumunu güncelle
                    display.showConfirmationMessage("Oto Ayar Basladi");
                    menuManager.setCurrentState(MENU_PID_MODE);
                    updateMenuWithCurrentStatus();
                }
                break;

            case MENU_PID_OFF:
                pidController.setPIDMode(PID_MODE_OFF);
                storage.setPidMode(0);
                storage.queueSave();
                updateWiFiStatus(); // WiFi durumunu güncelle
                display.showConfirmationMessage("PID Kapatildi");
                menuManager.setCurrentState(MENU_PID_MODE);
                updateMenuWithCurrentStatus();
                break;

            case MENU_PID_MANUAL_START:
                pidController.setPIDMode(PID_MODE_MANUAL);
                storage.setPidMode(1);
                storage.queueSave();
                updateWiFiStatus(); // WiFi durumunu güncelle
                display.showConfirmationMessage("Manuel PID Basladi");
                menuManager.setCurrentState(MENU_PID_MODE);
                updateMenuWithCurrentStatus();
                break;
                
            case MENU_ALARM_ENABLE_ALL:
                {
                    alarmManager.setAlarmsEnabled(true);
                    storage.setAlarmsEnabled(true);
                    storage.queueSave();
                    
                    updateWiFiStatus(); // WiFi durumunu güncelle
                    updateMenuWithCurrentStatus();
                    
                    display.showConfirmationMessage("Tum Alarmlar Acildi");
                    menuManager.setCurrentState(MENU_ALARM);
                    
                    Serial.println("Kullanıcı tarafından tüm alarmlar açıldı");
                }
                break;
            
            case MENU_ALARM_DISABLE_ALL:
                {
                    alarmManager.setAlarmsEnabled(false);
                    storage.setAlarmsEnabled(false);
                    storage.queueSave();
                    
                    updateWiFiStatus(); // WiFi durumunu güncelle
                    updateMenuWithCurrentStatus();
                    
                    display.showConfirmationMessage("Tum Alarmlar Kapatildi");
                    menuManager.setCurrentState(MENU_ALARM);
                    
                    Serial.println("Kullanıcı tarafından tüm alarmlar kapatıldı");
                }
                break;
                
            case MENU_WIFI_MODE:
            {
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
                    
                    updateWiFiStatus(); // WiFi durumunu güncelle
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
                updateWiFiStatus(); // WiFi durumunu güncelle
            } else {
                display.showConfirmationMessage("Gecersiz Saat!");
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
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
                updateWiFiStatus(); // WiFi durumunu güncelle
            } else {
                display.showConfirmationMessage("Gecersiz Tarih!");
                return;
            }
            
            menuManager.setCurrentState(MENU_TIME_DATE);
            return;
        }
        
        // Normal değer ayarlama işlemleri
        MenuState prevState = menuManager.getPreviousState();
        float value = menuManager.getAdjustedValue();
        
        switch (prevState) {
            case MENU_TEMPERATURE:
                pidController.setSetpoint(value);
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetTemperature(value);
                }
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_HUMIDITY:
                hysteresisController.setSetpoint(value);
                if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                    incubation.setTargetHumidity((uint8_t)value);
                }
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_PID_KP:
                pidController.setTunings(value, pidController.getKi(), pidController.getKd());
                storage.setPidKp(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_PID_KI:
                pidController.setTunings(pidController.getKp(), value, pidController.getKd());
                storage.setPidKi(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_PID_KD:
                pidController.setTunings(pidController.getKp(), pidController.getKi(), value);
                storage.setPidKd(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_MOTOR_WAIT:
                relays.updateMotorTiming(millis(), (uint32_t)value, storage.getMotorRunTime());
                storage.setMotorWaitTime((uint32_t)value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_MOTOR_RUN:
                relays.updateMotorTiming(millis(), storage.getMotorWaitTime(), (uint32_t)value);
                storage.setMotorRunTime((uint32_t)value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_CALIBRATION_TEMP_1:
                sensors.setTemperatureCalibration(value, sensors.getTemperatureCalibration(1));
                storage.setTempCalibration(0, value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_CALIBRATION_TEMP_2:
                sensors.setTemperatureCalibration(sensors.getTemperatureCalibration(0), value);
                storage.setTempCalibration(1, value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_CALIBRATION_HUMID_1:
                sensors.setHumidityCalibration(value, sensors.getHumidityCalibration(1));
                storage.setHumidCalibration(0, value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_CALIBRATION_HUMID_2:
                sensors.setHumidityCalibration(sensors.getHumidityCalibration(0), value);
                storage.setHumidCalibration(1, value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_ALARM_TEMP_LOW:
                alarmManager.setTempLowThreshold(value);
                storage.setTempLowAlarm(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_ALARM_TEMP_HIGH:
                alarmManager.setTempHighThreshold(value);
                storage.setTempHighAlarm(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_ALARM_HUMID_LOW:
                alarmManager.setHumidLowThreshold(value);
                storage.setHumidLowAlarm(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_ALARM_HUMID_HIGH:
                alarmManager.setHumidHighThreshold(value);
                storage.setHumidHighAlarm(value);
                storage.queueSave();
                updateWiFiStatus(); // Anında güncelle
                break;
                
            case MENU_MANUAL_DEV_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        value,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualDevTemp(value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            case MENU_MANUAL_HATCH_TEMP:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        value,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualHatchTemp(value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            case MENU_MANUAL_DEV_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        (uint8_t)value,
                        params.hatchingHumidity,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualDevHumid((uint8_t)value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            case MENU_MANUAL_HATCH_HUMID:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        (uint8_t)value,
                        params.developmentDays,
                        params.hatchingDays
                    );
                    storage.setManualHatchHumid((uint8_t)value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            case MENU_MANUAL_DEV_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        (uint8_t)value,
                        params.hatchingDays
                    );
                    storage.setManualDevDays((uint8_t)value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            case MENU_MANUAL_HATCH_DAYS:
                {
                    IncubationParameters params = incubation.getParameters();
                    incubation.setManualParameters(
                        params.developmentTemp,
                        params.hatchingTemp,
                        params.developmentHumidity,
                        params.hatchingHumidity,
                        params.developmentDays,
                        (uint8_t)value
                    );
                    storage.setManualHatchDays((uint8_t)value);
                    storage.queueSave();
                    updateWiFiStatus(); // Anında güncelle
                }
                break;
                
            default:
                break;
        }
        
        display.showConfirmationMessage("Kaydedildi");
        
        // Menü durumunu güncelle
        if (prevState >= MENU_PID_KP && prevState <= MENU_PID_KD) {
            menuManager.setCurrentState(MENU_PID);
            updateMenuWithCurrentStatus();
        }
        else if (prevState >= MENU_CALIBRATION_TEMP_1 && prevState <= MENU_CALIBRATION_TEMP_2) {
            menuManager.setCurrentState(MENU_CALIBRATION_TEMP);
        }
        else if (prevState >= MENU_CALIBRATION_HUMID_1 && prevState <= MENU_CALIBRATION_HUMID_2) {
            menuManager.setCurrentState(MENU_CALIBRATION_HUMID);
        }
        else if (prevState >= MENU_ALARM_TEMP_LOW && prevState <= MENU_ALARM_TEMP_HIGH) {
            menuManager.setCurrentState(MENU_ALARM_TEMP);
        }
        else if (prevState >= MENU_ALARM_HUMID_LOW && prevState <= MENU_ALARM_HUMID_HIGH) {
            menuManager.setCurrentState(MENU_ALARM_HUMID);
        }
        else if (prevState >= MENU_MANUAL_DEV_TEMP && prevState <= MENU_MANUAL_HATCH_DAYS) {
            menuManager.setCurrentState(MENU_MANUAL_INCUBATION);
        }
        else if (prevState == MENU_MOTOR_WAIT || prevState == MENU_MOTOR_RUN) {
            menuManager.setCurrentState(MENU_MOTOR);
        }
        else {
            menuManager.setCurrentState(MENU_MAIN);
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
    
    // Ayarları saklama modülünden yükle
    incubation.setIncubationType(storage.getIncubationType());
    
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
    
    // DÜZELTME: Storage'dan PID modunu oku ve uygula
    uint8_t savedPidMode = storage.getPidMode();
    
    // Motor ayarlarını yükle ve relays modülünü güncelle
    relays.updateMotorTiming(
        millis(),
        storage.getMotorWaitTime(),
        storage.getMotorRunTime()
    );
    
    // Motor zamanlama durumunu storage'dan yükle
    relays.loadMotorTimingFromStorage(&storage);
    
    watchdogManager.feed(); // Ara besleme
    
    // Kuluçka devam ediyor mu kontrolü
    if (storage.isIncubationRunning()) {
        incubation.startIncubation(storage.getStartTime());
        
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
    } else {
        // Kuluçka çalışmıyorsa storage'daki PID modunu kullan
        pidController.setPIDMode((PIDMode)savedPidMode);
        Serial.println("PID modu: " + String(savedPidMode == 0 ? "Kapalı" : 
                                            savedPidMode == 1 ? "Manuel" : "Otomatik"));
    }
    
    // Hedef değerleri ayarla
    pidController.setSetpoint(incubation.getTargetTemperature());
    hysteresisController.setSetpoint(incubation.getTargetHumidity());
    
    // Sensör kalibrasyonlarını ayarla
    sensors.setTemperatureCalibration(
        storage.getTempCalibration(0),
        storage.getTempCalibration(1)
    );
    
    sensors.setHumidityCalibration(
        storage.getHumidCalibration(0),
        storage.getHumidCalibration(1)
    );
    
    // Alarm eşiklerini ayarla
    alarmManager.setTempLowThreshold(storage.getTempLowAlarm());
    alarmManager.setTempHighThreshold(storage.getTempHighAlarm());
    alarmManager.setHumidLowThreshold(storage.getHumidLowAlarm());
    alarmManager.setHumidHighThreshold(storage.getHumidHighAlarm());
    
    // Alarm durumunu ayarla
    alarmManager.setAlarmsEnabled(storage.areAlarmsEnabled());
    
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
    storage.setPidMode((uint8_t)pidController.getPIDMode());  // YENİ: PID modunu kaydet
    
    // Kuluçka durumu
    storage.setIncubationRunning(incubation.isIncubationRunning());
    if (incubation.isIncubationRunning()) {
        storage.setStartTime(incubation.getStartTime());
    }
    
    // Motor zamanlama durumunu da kaydet
    relays.saveMotorTimingToStorage(&storage);
    
    // Alarm durumunu kaydet
    storage.setAlarmsEnabled(alarmManager.areAlarmsEnabled());
    
    // Ayarları anında EEPROM'a kaydet (kritik durumlarda)
    storage.saveStateNow();
    
    watchdogManager.endOperation(); // İşlem tamamlandı
}

void handleWifiParameterUpdate(String param, String value) {
    // Sıcaklık ve nem hedef değerleri
    if (param == "targetTemp") {
        float temp = value.toFloat();
        if (temp >= 20.0 && temp <= 40.0) {
            pidController.setSetpoint(temp);
            if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                incubation.setTargetTemperature(temp);
            }
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Hedef sıcaklık güncellendi ve kaydedildi: " + String(temp));
        }
    } else if (param == "targetHumid") {
        float humid = value.toFloat();
        if (humid >= 30.0 && humid <= 90.0) {
            hysteresisController.setSetpoint(humid);
            if (incubation.getIncubationType() == INCUBATION_MANUAL) {
                incubation.setTargetHumidity((uint8_t)humid);
            }
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Hedef nem güncellendi ve kaydedildi: " + String(humid));
        }
    }
    // PID parametreleri
    else if (param == "pidKp") {
        float kp = value.toFloat();
        if (kp >= 0.1 && kp <= 100.0) {
            pidController.setTunings(kp, pidController.getKi(), pidController.getKd());
            storage.setPidKp(kp);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("PID Kp güncellendi ve kaydedildi: " + String(kp));
        }
    } else if (param == "pidKi") {
        float ki = value.toFloat();
        if (ki >= 0.01 && ki <= 10.0) {
            pidController.setTunings(pidController.getKp(), ki, pidController.getKd());
            storage.setPidKi(ki);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("PID Ki güncellendi ve kaydedildi: " + String(ki));
        }
    } else if (param == "pidKd") {
        float kd = value.toFloat();
        if (kd >= 0.1 && kd <= 100.0) {
            pidController.setTunings(pidController.getKp(), pidController.getKi(), kd);
            storage.setPidKd(kd);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("PID Kd güncellendi ve kaydedildi: " + String(kd));
        }
    }
    // PID modu değişikliği
    else if (param == "pidMode") {
        int mode = value.toInt();
        if (mode >= 0 && mode <= 2) {
            PIDMode newMode = (PIDMode)mode;
            pidController.setPIDMode(newMode);
            storage.setPidMode(mode);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            updateMenuWithCurrentStatus();
            Serial.println("PID modu güncellendi ve kaydedildi: " + String(mode));
        }
    }
    // Motor ayarları
    else if (param == "motorWaitTime") {
        uint32_t waitTime = value.toInt();
        if (waitTime >= 1 && waitTime <= 240) {
            relays.updateMotorTiming(millis(), waitTime, storage.getMotorRunTime());
            storage.setMotorWaitTime(waitTime);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Motor bekleme süresi güncellendi ve kaydedildi: " + String(waitTime));
        }
    } else if (param == "motorRunTime") {
        uint32_t runTime = value.toInt();
        if (runTime >= 1 && runTime <= 60) {
            relays.updateMotorTiming(millis(), storage.getMotorWaitTime(), runTime);
            storage.setMotorRunTime(runTime);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Motor çalışma süresi güncellendi ve kaydedildi: " + String(runTime));
        }
    }
    // Alarm ayarları
    else if (param == "tempLowAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 0.1 && alarm <= 5.0) {
            alarmManager.setTempLowThreshold(alarm);
            storage.setTempLowAlarm(alarm);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Düşük sıcaklık alarmı güncellendi ve kaydedildi: " + String(alarm));
        }
    } else if (param == "tempHighAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 0.1 && alarm <= 5.0) {
            alarmManager.setTempHighThreshold(alarm);
            storage.setTempHighAlarm(alarm);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Yüksek sıcaklık alarmı güncellendi ve kaydedildi: " + String(alarm));
        }
    } else if (param == "humidLowAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 1 && alarm <= 20) {
            alarmManager.setHumidLowThreshold(alarm);
            storage.setHumidLowAlarm(alarm);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Düşük nem alarmı güncellendi ve kaydedildi: " + String(alarm));
        }
    } else if (param == "humidHighAlarm") {
        float alarm = value.toFloat();
        if (alarm >= 1 && alarm <= 20) {
            alarmManager.setHumidHighThreshold(alarm);
            storage.setHumidHighAlarm(alarm);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Yüksek nem alarmı güncellendi ve kaydedildi: " + String(alarm));
        }
    } else if (param == "alarmEnabled") {
        bool enabled = (value == "1" || value == "true");
        alarmManager.setAlarmsEnabled(enabled);
        storage.setAlarmsEnabled(enabled);
        storage.saveStateNow();
        updateWiFiStatus(); // Anında güncelle
        updateMenuWithCurrentStatus();
        Serial.println("Alarm durumu güncellendi ve kaydedildi: " + String(enabled ? "AÇIK" : "KAPALI"));
    }
    // Kalibrasyon ayarları
    else if (param == "tempCalibration1") {
        float cal = value.toFloat();
        if (cal >= -10.0 && cal <= 10.0) {
            sensors.setTemperatureCalibration(cal, sensors.getTemperatureCalibration(1));
            storage.setTempCalibration(0, cal);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Sensör 1 sıcaklık kalibrasyonu güncellendi ve kaydedildi: " + String(cal));
        }
    } else if (param == "tempCalibration2") {
        float cal = value.toFloat();
        if (cal >= -10.0 && cal <= 10.0) {
            sensors.setTemperatureCalibration(sensors.getTemperatureCalibration(0), cal);
            storage.setTempCalibration(1, cal);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Sensör 2 sıcaklık kalibrasyonu güncellendi ve kaydedildi: " + String(cal));
        }
    } else if (param == "humidCalibration1") {
        float cal = value.toFloat();
        if (cal >= -20.0 && cal <= 20.0) {
            sensors.setHumidityCalibration(cal, sensors.getHumidityCalibration(1));
            storage.setHumidCalibration(0, cal);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Sensör 1 nem kalibrasyonu güncellendi ve kaydedildi: " + String(cal));
        }
    } else if (param == "humidCalibration2") {
        float cal = value.toFloat();
        if (cal >= -20.0 && cal <= 20.0) {
            sensors.setHumidityCalibration(sensors.getHumidityCalibration(0), cal);
            storage.setHumidCalibration(1, cal);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Sensör 2 nem kalibrasyonu güncellendi ve kaydedildi: " + String(cal));
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Kuluçka tipi güncellendi ve kaydedildi: " + String(type));
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            updateMenuWithCurrentStatus();
            Serial.println("Kuluçka başlatıldı ve kaydedildi");
        } else if (!running && incubation.isIncubationRunning()) {
            incubation.stopIncubation();
            storage.setIncubationRunning(false);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Kuluçka durduruldu ve kaydedildi");
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel gelişim sıcaklığı güncellendi ve kaydedildi: " + String(temp));
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel çıkım sıcaklığı güncellendi ve kaydedildi: " + String(temp));
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel gelişim nemi güncellendi ve kaydedildi: " + String(humid));
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
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel çıkım nemi güncellendi ve kaydedildi: " + String(humid));
        }
    } else if (param == "manualDevDays") {
        uint8_t days = value.toInt();
        if (days >= 1 && days <= 60) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                         params.hatchingHumidity, days, params.hatchingDays);
            storage.setManualDevDays(days);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel gelişim günleri güncellendi ve kaydedildi: " + String(days));
        }
    } else if (param == "manualHatchDays") {
        uint8_t days = value.toInt();
        if (days >= 1 && days <= 10) {
            IncubationParameters params = incubation.getParameters();
            incubation.setManualParameters(params.developmentTemp, params.hatchingTemp, params.developmentHumidity, 
                                         params.hatchingHumidity, params.developmentDays, days);
            storage.setManualHatchDays(days);
            storage.saveStateNow();
            updateWiFiStatus(); // Anında güncelle
            Serial.println("Manuel çıkım günleri güncellendi ve kaydedildi: " + String(days));
        }
    }
    // WiFi ayarları
    else if (param == "wifiStationSSID") {
        storage.setStationSSID(value);
        storage.saveStateNow();
        updateWiFiStatus(); // Anında güncelle
        Serial.println("Station SSID güncellendi ve kaydedildi: " + value);
    } else if (param == "wifiStationPassword") {
        storage.setStationPassword(value);
        storage.saveStateNow();
        updateWiFiStatus(); // Anında güncelle
        Serial.println("Station şifresi güncellendi ve kaydedildi");
    } else if (param == "wifiMode") {
        WiFiConnectionMode mode = (value == "1") ? WIFI_CONN_MODE_STATION : WIFI_CONN_MODE_AP;
        storage.setWifiMode(mode);
        storage.saveStateNow();
        updateWiFiStatus(); // Anında güncelle
        Serial.println("WiFi modu güncellendi ve kaydedildi: " + String(mode));
    }
}
